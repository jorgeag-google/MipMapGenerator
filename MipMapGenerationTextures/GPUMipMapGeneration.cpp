#include <exception>
#include "GPUMipMapGeneration.h"
#include <DirectXTex.h>

#ifndef SAFE_RELEASE
#define SAFE_RELEASE(p)      { if (p) { (p)->Release(); (p)=nullptr; } }
#endif

GPUMipMapGenerator::GPUMipMapGenerator() {
    // Enable run-time memory check for debug builds.
#ifdef _DEBUG
    _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif 
    
    if (FAILED(createComputeDevice(&mDevice, &mContext, false))) {
        throw std::exception("Failed to create device");
    }
    
    if (FAILED(createComputeShader(mShaderSrcFile, "CSMain", mDevice, &mComputeShader))) {
        throw std::exception("Failed to create shader object");
    }

    if (FAILED(CoInitialize(nullptr))) {
        throw std::exception("Fail to init the WIC image factory, needed to save using wic codecs");
    }
}

GPUMipMapGenerator::~GPUMipMapGenerator() {
    // In case of abnormal termination
    SAFE_RELEASE(mSamplerLinear);
    SAFE_RELEASE(mConstantBuffer);
    SAFE_RELEASE(mTextInputSRV);
    SAFE_RELEASE(mTextResultUAV);
    SAFE_RELEASE(mTextInput);
    SAFE_RELEASE(mTextResult);
    // Normal cleaning
    SAFE_RELEASE(mComputeShader);
    SAFE_RELEASE(mContext);
    SAFE_RELEASE(mDevice);
}

bool GPUMipMapGenerator::generateMip(const ImageData& src_image, ImageData& dst_image) {
    // Create textures in GPU mem
    if (FAILED(createSrcTexture(mDevice, src_image, &mTextInput))) {
        throw std::exception("Unable to create src image texture");
    }
    if (FAILED(createDstTexture(mDevice, dst_image, &mTextResult))) {
        throw std::exception("Unable to create dst image texture");
    }

    // Ask for debug info from the buffers
#if defined(_DEBUG) || defined(PROFILE)
    if (mTextInput) {
        mTextInput->SetPrivateData(WKPDID_D3DDebugObjectName, sizeof("srcTex") - 1, "srcTex");
    }
    if (mTextResult) {
        mTextResult->SetPrivateData(WKPDID_D3DDebugObjectName, sizeof("dstTex") - 1, "dstTex");
    }
#endif
    // Creating texture views
    if (FAILED(createTextureSRV(mDevice, mTextInput, &mTextInputSRV))) {
        throw std::exception("Unable to create SRV for src image");
    }
    if (FAILED(createTextureUAV(mDevice, mTextResult, &mTextResultUAV))) {
        throw std::exception("Unable to create UAV for dst image");
    }
    if (FAILED(createSampler(mDevice, &mSamplerLinear))) {
        throw std::exception("Unable to linear sampler state");
    }
    // Prepare constant data for shader
    ShaderConstantData csConstants;
    csConstants.texel_size[0] = 1.0f / src_image.width;
    csConstants.texel_size[1] = 1.0f / src_image.height;
    csConstants.src_mip_level = 0;
    // If width is even
    if ((src_image.width % 2) == 0) {
        // Test the height
        csConstants.dimension_case = (src_image.height % 2) == 0 ? 0 : 1;
    } else { // width is odd
        // Test the height
        csConstants.dimension_case = (src_image.height % 2) == 0 ? 2 : 3;
    }
    if (FAILED(createConstantBuffer(mDevice, sizeof(csConstants), &csConstants, &mConstantBuffer))) {
        throw std::exception("Unable to create constant buffer");
    }

    // Run the compute shader
    ID3D11ShaderResourceView* aRViews[1] = { mTextInputSRV };
    ID3D11SamplerState* samplerStates[1] = { mSamplerLinear };
    runComputeShader(mContext, mComputeShader, 1, aRViews, 1, samplerStates,
        mConstantBuffer, &csConstants, sizeof(csConstants),
        mTextResultUAV, dst_image.width, dst_image.height, 1);
    
    // Read back the results from GPU and save it to an image
    saveResult();

    SAFE_RELEASE(mSamplerLinear);
    SAFE_RELEASE(mConstantBuffer);
    SAFE_RELEASE(mTextInputSRV);
    SAFE_RELEASE(mTextResultUAV);
    SAFE_RELEASE(mTextInput);
    SAFE_RELEASE(mTextResult);

    return true;
}

HRESULT GPUMipMapGenerator::saveResult(const wchar_t* resultImageFile) {
    DirectX::ScratchImage image;

    HRESULT hr = S_OK;

    hr = DirectX::CaptureTexture(mDevice, mContext, mTextResult, image);
    if (FAILED(hr)) {
        return hr;
    }
    // Since the DX11 resource could contains several planes or mipmap levels, we extract the first image's mipmap 0
    const DirectX::Image* img = image.GetImage(0, 0, 0);
    assert(img);

    hr = DirectX::SaveToWICFile(*img, DirectX::WIC_FLAGS_NONE, DirectX::GetWICCodec(DirectX::WIC_CODEC_JPEG), resultImageFile);
    if (FAILED(hr)) {
        return hr;
    }

    return hr;
}

_Use_decl_annotations_
HRESULT GPUMipMapGenerator::createComputeDevice(ID3D11Device** ppDeviceOut, ID3D11DeviceContext** ppContextOut, bool bForceRef)
{
    *ppDeviceOut = nullptr;
    *ppContextOut = nullptr;

    HRESULT hr = S_OK;

    UINT uCreationFlags = D3D11_CREATE_DEVICE_SINGLETHREADED;
#ifdef _DEBUG
    uCreationFlags |= D3D11_CREATE_DEVICE_DEBUG;
#endif
    D3D_FEATURE_LEVEL flOut;
    static const D3D_FEATURE_LEVEL flvl[] = { D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_1, D3D_FEATURE_LEVEL_10_0 };

    bool bNeedRefDevice = false;
    if (!bForceRef)
    {
        hr = D3D11CreateDevice(nullptr,                        // Use default graphics card
            D3D_DRIVER_TYPE_HARDWARE,    // Try to create a hardware accelerated device
            nullptr,                        // Do not use external software rasterizer module
            uCreationFlags,              // Device creation flags
            flvl,
            sizeof(flvl) / sizeof(D3D_FEATURE_LEVEL),
            D3D11_SDK_VERSION,           // SDK version
            ppDeviceOut,                 // Device out
            &flOut,                      // Actual feature level created
            ppContextOut);              // Context out

        if (SUCCEEDED(hr))
        {
            // A hardware accelerated device has been created, so check for Compute Shader support

            // If we have a device >= D3D_FEATURE_LEVEL_11_0 created, full CS5.0 support is guaranteed, no need for further checks
            if (flOut < D3D_FEATURE_LEVEL_11_0)
            {
                // Otherwise, we need further check whether this device support CS4.x (Compute on 10)
                D3D11_FEATURE_DATA_D3D10_X_HARDWARE_OPTIONS hwopts;
                (*ppDeviceOut)->CheckFeatureSupport(D3D11_FEATURE_D3D10_X_HARDWARE_OPTIONS, &hwopts, sizeof(hwopts));
                if (!hwopts.ComputeShaders_Plus_RawAndStructuredBuffers_Via_Shader_4_x)
                {
                    bNeedRefDevice = true;
                    printf("No hardware Compute Shader capable device found, trying to create ref device.\n");
                }
            }
        }
    }

    if (bForceRef || FAILED(hr) || bNeedRefDevice)
    {
        // Either because of failure on creating a hardware device or hardware lacking CS capability, we create a ref device here

        SAFE_RELEASE(*ppDeviceOut);
        SAFE_RELEASE(*ppContextOut);

        hr = D3D11CreateDevice(nullptr,                        // Use default graphics card
            D3D_DRIVER_TYPE_REFERENCE,   // Try to create a hardware accelerated device
            nullptr,                        // Do not use external software rasterizer module
            uCreationFlags,              // Device creation flags
            flvl,
            sizeof(flvl) / sizeof(D3D_FEATURE_LEVEL),
            D3D11_SDK_VERSION,           // SDK version
            ppDeviceOut,                 // Device out
            &flOut,                      // Actual feature level created
            ppContextOut);              // Context out
        if (FAILED(hr))
        {
            printf("Reference rasterizer device create failure\n");
            return hr;
        }
    }

    return hr;
}

_Use_decl_annotations_
HRESULT GPUMipMapGenerator::createComputeShader(LPCWSTR pSrcFile, LPCSTR pFunctionName,
    ID3D11Device* pDevice, ID3D11ComputeShader** ppShaderOut)
{
    if (!pDevice || !ppShaderOut)
        return E_INVALIDARG;

    // Finds the correct path for the shader file.
    // This is only required for this sample to be run correctly from within the Sample Browser,
    // in your own projects, these lines could be removed safely
    WCHAR str[MAX_PATH];
    HRESULT hr = findDXSDKShaderFileCch(str, MAX_PATH, pSrcFile);
    if (FAILED(hr))
        return hr;

    DWORD dwShaderFlags = D3DCOMPILE_ENABLE_STRICTNESS;
#ifdef _DEBUG
    // Set the D3DCOMPILE_DEBUG flag to embed debug information in the shaders.
    // Setting this flag improves the shader debugging experience, but still allows 
    // the shaders to be optimized and to run exactly the way they will run in 
    // the release configuration of this program.
    dwShaderFlags |= D3DCOMPILE_DEBUG;

    // Disable optimizations to further improve shader debugging
    dwShaderFlags |= D3DCOMPILE_SKIP_OPTIMIZATION;
#endif

    const D3D_SHADER_MACRO defines[] =
    {
        "USE_STRUCTURED_BUFFERS", "1",
        nullptr, nullptr
    };

    // We generally prefer to use the higher CS shader profile when possible as CS 5.0 is better performance on 11-class hardware
    LPCSTR pProfile = (pDevice->GetFeatureLevel() >= D3D_FEATURE_LEVEL_11_0) ? "cs_5_0" : "cs_4_0";

    ID3DBlob* pErrorBlob = nullptr;
    ID3DBlob* pBlob = nullptr;
    hr = D3DCompileFromFile(pSrcFile, defines, D3D_COMPILE_STANDARD_FILE_INCLUDE, pFunctionName, pProfile,
        dwShaderFlags, 0, &pBlob, &pErrorBlob);
    if (FAILED(hr))
    {
        if (pErrorBlob)
            OutputDebugStringA((char*)pErrorBlob->GetBufferPointer());

        SAFE_RELEASE(pErrorBlob);
        SAFE_RELEASE(pBlob);

        return hr;
    }

    hr = pDevice->CreateComputeShader(pBlob->GetBufferPointer(), pBlob->GetBufferSize(), nullptr, ppShaderOut);

    SAFE_RELEASE(pErrorBlob);
    SAFE_RELEASE(pBlob);

#if defined(_DEBUG) || defined(PROFILE)
    if (SUCCEEDED(hr))
    {
        (*ppShaderOut)->SetPrivateData(WKPDID_D3DDebugObjectName, lstrlenA(pFunctionName), pFunctionName);
    }
#endif

    return hr;
}

_Use_decl_annotations_
HRESULT GPUMipMapGenerator::findDXSDKShaderFileCch(WCHAR* strDestPath,
    int cchDest,
    LPCWSTR strFilename)
{
    if (!strFilename || strFilename[0] == 0 || !strDestPath || cchDest < 10)
        return E_INVALIDARG;

    // Get the exe name, and exe path
    WCHAR strExePath[MAX_PATH] =
    {
        0
    };
    WCHAR strExeName[MAX_PATH] =
    {
        0
    };
    WCHAR* strLastSlash = nullptr;
    GetModuleFileName(nullptr, strExePath, MAX_PATH);
    strExePath[MAX_PATH - 1] = 0;
    strLastSlash = wcsrchr(strExePath, TEXT('\\'));
    if (strLastSlash)
    {
        wcscpy_s(strExeName, MAX_PATH, &strLastSlash[1]);

        // Chop the exe name from the exe path
        *strLastSlash = 0;

        // Chop the .exe from the exe name
        strLastSlash = wcsrchr(strExeName, TEXT('.'));
        if (strLastSlash)
            *strLastSlash = 0;
    }

    // Search in directories:
    //      .\
    //      %EXE_DIR%\..\..\%EXE_NAME%

    wcscpy_s(strDestPath, cchDest, strFilename);
    if (GetFileAttributes(strDestPath) != 0xFFFFFFFF)
        return S_OK;

    swprintf_s(strDestPath, cchDest, L"%s\\..\\..\\%s\\%s", strExePath, strExeName, strFilename);
    if (GetFileAttributes(strDestPath) != 0xFFFFFFFF)
        return S_OK;

    // On failure, return the file as the path but also return an error code
    wcscpy_s(strDestPath, cchDest, strFilename);

    return E_FAIL;
}

_Use_decl_annotations_
HRESULT GPUMipMapGenerator::createStructuredBuffer(ID3D11Device* pDevice, UINT uElementSize, UINT uCount, void* pInitData, ID3D11Buffer** ppBufOut)
{
    *ppBufOut = nullptr;

    D3D11_BUFFER_DESC desc = {};
    desc.BindFlags = D3D11_BIND_UNORDERED_ACCESS | D3D11_BIND_SHADER_RESOURCE;
    desc.ByteWidth = uElementSize * uCount;
    desc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
    desc.StructureByteStride = uElementSize;

    if (pInitData)
    {
        D3D11_SUBRESOURCE_DATA InitData;
        InitData.pSysMem = pInitData;
        return pDevice->CreateBuffer(&desc, &InitData, ppBufOut);
    }
    else
        return pDevice->CreateBuffer(&desc, nullptr, ppBufOut);
}

_Use_decl_annotations_
HRESULT GPUMipMapGenerator::createSrcTexture(_In_ ID3D11Device* pDevice, _In_ const ImageData& image, _Outptr_ ID3D11Texture2D** pTextureOut) {
    D3D11_TEXTURE2D_DESC desc;
    desc.Width = image.width;
    desc.Height = image.height;
    desc.MipLevels = desc.ArraySize = 1;
    desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.SampleDesc.Count = 1;
    desc.SampleDesc.Quality = 0;
    desc.Usage = D3D11_USAGE_DYNAMIC;
    desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    desc.MiscFlags = 0;
         
    if (image.pixels != nullptr) {
        // Fill in the subresource data.
        D3D11_SUBRESOURCE_DATA initData;
        initData.pSysMem = image.pixels;
        initData.SysMemPitch = static_cast<UINT>(4 * image.width);
        initData.SysMemSlicePitch = static_cast<UINT>(0);
        return pDevice->CreateTexture2D(&desc, &initData, pTextureOut);
    }
    else {
        return pDevice->CreateTexture2D(&desc, nullptr, pTextureOut);
    }

}

_Use_decl_annotations_
HRESULT GPUMipMapGenerator::createDstTexture(_In_ ID3D11Device* pDevice, _In_ const ImageData& image, _Outptr_ ID3D11Texture2D** pTextureOut) {
    D3D11_TEXTURE2D_DESC desc;
    desc.Width = image.width;
    desc.Height = image.height;
    desc.MipLevels = desc.ArraySize = 1;
    desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.SampleDesc.Count = 1;
    desc.SampleDesc.Quality = 0;
    desc.Usage = D3D11_USAGE_DEFAULT;
    desc.BindFlags = D3D11_BIND_UNORDERED_ACCESS | D3D11_BIND_SHADER_RESOURCE;
    desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    desc.MiscFlags = 0;

    if (image.pixels != nullptr) {
        // Fill in the subresource data.
        D3D11_SUBRESOURCE_DATA initData;
        initData.pSysMem = image.pixels;
        initData.SysMemPitch = static_cast<UINT>(4 * image.width);
        initData.SysMemSlicePitch = static_cast<UINT>(0);
        return pDevice->CreateTexture2D(&desc, &initData, pTextureOut);
    }
    else {
        return pDevice->CreateTexture2D(&desc, nullptr, pTextureOut);
    }

}

_Use_decl_annotations_
HRESULT GPUMipMapGenerator::createConstantBuffer(ID3D11Device* pDevice, UINT uElementSize, void* pInitData, ID3D11Buffer** ppBufOut)
{
    // Fill in the buffer description.
    D3D11_BUFFER_DESC cbDesc = {};
    cbDesc.ByteWidth = uElementSize;
    cbDesc.Usage = D3D11_USAGE_DYNAMIC;
    cbDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    cbDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    cbDesc.MiscFlags = 0;
    cbDesc.StructureByteStride = 0;

    if (pInitData) {
        // Fill in the subresource data.
        D3D11_SUBRESOURCE_DATA InitData;
        InitData.pSysMem = pInitData;
        InitData.SysMemPitch = 0;
        InitData.SysMemSlicePitch = 0;
        return pDevice->CreateBuffer(&cbDesc, &InitData, ppBufOut);
    }
    else {
        return pDevice->CreateBuffer(&cbDesc, nullptr, ppBufOut);
    }
}

_Use_decl_annotations_
HRESULT GPUMipMapGenerator::createTextureSRV(_In_ ID3D11Device* pDevice, _In_ ID3D11Texture2D* pTexture, _Outptr_ ID3D11ShaderResourceView** ppSRVOut) {
    D3D11_TEXTURE2D_DESC descText = {};
    pTexture->GetDesc(&descText);

    D3D11_SHADER_RESOURCE_VIEW_DESC desc = {};
    desc.Format = descText.Format;
    desc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
    desc.Texture2D.MostDetailedMip = 0;    
    desc.Texture2D.MipLevels = -1;

    return pDevice->CreateShaderResourceView(pTexture, &desc, ppSRVOut);
}

_Use_decl_annotations_
HRESULT GPUMipMapGenerator::createTextureUAV(_In_ ID3D11Device* pDevice, _In_ ID3D11Texture2D* pTexture, _Outptr_ ID3D11UnorderedAccessView** pUAVOut) {
    D3D11_TEXTURE2D_DESC descText = {};
    pTexture->GetDesc(&descText);

    D3D11_UNORDERED_ACCESS_VIEW_DESC desc = {};
    desc.ViewDimension = D3D11_UAV_DIMENSION_TEXTURE2D;
    desc.Format = descText.Format;
    desc.Texture2D.MipSlice = 0;
   
    return pDevice->CreateUnorderedAccessView(pTexture, &desc, pUAVOut);
}

_Use_decl_annotations_
HRESULT GPUMipMapGenerator::createSampler(_In_ ID3D11Device* pDevice, _Outptr_ ID3D11SamplerState** ppSamplerOut) {
    // Description of the sampler
    D3D11_SAMPLER_DESC sampDesc = {};
    sampDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    sampDesc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
    sampDesc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
    sampDesc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
    sampDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;
    sampDesc.MinLOD = 0;
    sampDesc.MaxLOD = D3D11_FLOAT32_MAX;
    // Create the sample state
    return pDevice->CreateSamplerState(&sampDesc, ppSamplerOut);
}

_Use_decl_annotations_
void GPUMipMapGenerator::runComputeShader(ID3D11DeviceContext* pd3dImmediateContext,
    ID3D11ComputeShader* pComputeShader,
    UINT nNumViews, ID3D11ShaderResourceView** pShaderResourceViews,
    _In_ UINT nNumSamplerStates, _In_reads_(nNumSamplerStates) ID3D11SamplerState** pShaderSamplerStates,
    ID3D11Buffer* pCBCS, void* pCSData, DWORD dwNumDataBytes,
    ID3D11UnorderedAccessView* pUnorderedAccessView,
    UINT X, UINT Y, UINT Z)
{
    pd3dImmediateContext->CSSetShader(pComputeShader, nullptr, 0);
    pd3dImmediateContext->CSSetSamplers(0, nNumSamplerStates, pShaderSamplerStates);
    pd3dImmediateContext->CSSetShaderResources(0, nNumViews, pShaderResourceViews);
    pd3dImmediateContext->CSSetUnorderedAccessViews(0, 1, &pUnorderedAccessView, nullptr);
    if (pCBCS && pCSData)
    {
        D3D11_MAPPED_SUBRESOURCE MappedResource;
        pd3dImmediateContext->Map(pCBCS, 0, D3D11_MAP_WRITE_DISCARD, 0, &MappedResource);
        memcpy(MappedResource.pData, pCSData, dwNumDataBytes);
        pd3dImmediateContext->Unmap(pCBCS, 0);
        ID3D11Buffer* ppCB[1] = { pCBCS };
        pd3dImmediateContext->CSSetConstantBuffers(0, 1, ppCB);
    }

    pd3dImmediateContext->Dispatch(X, Y, Z);

    pd3dImmediateContext->CSSetShader(nullptr, nullptr, 0);

    ID3D11UnorderedAccessView* ppUAViewnullptr[1] = { nullptr };
    pd3dImmediateContext->CSSetUnorderedAccessViews(0, 1, ppUAViewnullptr, nullptr);

    ID3D11ShaderResourceView* ppSRVnullptr[2] = { nullptr, nullptr };
    pd3dImmediateContext->CSSetShaderResources(0, 2, ppSRVnullptr);

    ID3D11Buffer* ppCBnullptr[1] = { nullptr };
    pd3dImmediateContext->CSSetConstantBuffers(0, 1, ppCBnullptr);
}

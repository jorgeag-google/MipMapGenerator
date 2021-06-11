//--------------------------------------------------------------------------------------
// File: ComputeShaderSample.cpp
//
// Does simple image processing using image read and write to disk (no graphics involved)
// this is a pure compute sample.
// It is based on MS Sample from here: 
// https://github.com/walbourn/directx-sdk-samples/blob/master/BasicCompute11/BasicCompute11.hlsl
// And the shader is based on another sample form here
// http://www.codinglabs.net/tutorial_compute_shaders_filters.aspx
//
//--------------------------------------------------------------------------------------

#include <string>

#include <stdio.h>
#include <crtdbg.h>
#include <d3dcommon.h>
#include <d3d11.h>
#include <d3dcompiler.h>

#include "ImageData.h"

#ifndef SAFE_RELEASE
#define SAFE_RELEASE(p)      { if (p) { (p)->Release(); (p)=nullptr; } }
#endif

//--------------------------------------------------------------------------------------
// Forward declarations 
//--------------------------------------------------------------------------------------
HRESULT CreateComputeDevice(_Outptr_ ID3D11Device** ppDeviceOut, _Outptr_ ID3D11DeviceContext** ppContextOut, _In_ bool bForceRef);
HRESULT CreateComputeShader(_In_z_ LPCWSTR pSrcFile, _In_z_ LPCSTR pFunctionName,
    _In_ ID3D11Device* pDevice, _Outptr_ ID3D11ComputeShader** ppShaderOut);
HRESULT CreateStructuredBuffer(_In_ ID3D11Device* pDevice, _In_ UINT uElementSize, _In_ UINT uCount,
    _In_reads_(uElementSize* uCount) void* pInitData,
    _Outptr_ ID3D11Buffer** ppBufOut);
HRESULT CreateConstantBuffer(_In_ ID3D11Device* pDevice, _In_ UINT uElementSize, 
    _In_ void* pInitData,
    _Outptr_ ID3D11Buffer** ppBufOut);
HRESULT CreateBufferSRV(_In_ ID3D11Device* pDevice, _In_ ID3D11Buffer* pBuffer, _Outptr_ ID3D11ShaderResourceView** ppSRVOut);
HRESULT CreateBufferUAV(_In_ ID3D11Device* pDevice, _In_ ID3D11Buffer* pBuffer, _Outptr_ ID3D11UnorderedAccessView** pUAVOut);
ID3D11Buffer* CreateAndCopyToDebugBuf(_In_ ID3D11Device* pDevice, _In_ ID3D11DeviceContext* pd3dImmediateContext, _In_ ID3D11Buffer* pBuffer);
void RunComputeShader(_In_ ID3D11DeviceContext* pd3dImmediateContext,
    _In_ ID3D11ComputeShader* pComputeShader,
    _In_ UINT nNumViews, _In_reads_(nNumViews) ID3D11ShaderResourceView** pShaderResourceViews,
    _In_opt_ ID3D11Buffer* pCBCS, _In_reads_opt_(dwNumDataBytes) void* pCSData, _In_ DWORD dwNumDataBytes,
    _In_ ID3D11UnorderedAccessView* pUnorderedAccessView,
    _In_ UINT X, _In_ UINT Y, _In_ UINT Z);
HRESULT FindDXSDKShaderFileCch(_Out_writes_(cchDest) WCHAR* strDestPath,
    _In_ int cchDest,
    _In_z_ LPCWSTR strFilename);

//--------------------------------------------------------------------------------------
// Global variables
//--------------------------------------------------------------------------------------
ID3D11Device* g_pDevice = nullptr;
ID3D11DeviceContext* g_pContext = nullptr;
ID3D11ComputeShader* g_pCS = nullptr;

ID3D11Buffer* g_pBuf0 = nullptr;
// ID3D11Buffer* g_pBuf1 = nullptr;
ID3D11Buffer* g_pBufResult = nullptr;
ID3D11Buffer* g_pConstantBuffer = nullptr;

ID3D11ShaderResourceView* g_pBuf0SRV = nullptr;
// ID3D11ShaderResourceView* g_pBuf1SRV = nullptr;
ID3D11UnorderedAccessView* g_pBufResultUAV = nullptr;

// These stcruct need to match the definitions in the CS shader
struct Pixel {
    int color;
};

// For the constant data
// This struct needs to ba aligned by 16 bytes
// i. e. sizeof(ShaderConstantData) % 16 == 0
struct ShaderConstantData
{
    int src_width;
    int src_height;
    int dst_width;
    int dst_height;
};

//--------------------------------------------------------------------------------------
// Entry point to the program
//--------------------------------------------------------------------------------------
int __cdecl main()
{
    // Enable run-time memory check for debug builds.
#ifdef _DEBUG
    _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif    

    printf("Creating device...");
    if (FAILED(CreateComputeDevice(&g_pDevice, &g_pContext, false)))
        return 1;
    printf("done\n");

    const wchar_t* shader_file = L"GenerateMip.hlsl";
    //const wchar_t* shader_file = L"Desaturate.hlsl";
    printf("Creating Compute Shader from file: %S... ", shader_file);
   
    if (FAILED(CreateComputeShader(shader_file, "CSMain", g_pDevice, &g_pCS)))
        return 1;
    printf("done\n");

    const char* file_name = "textures/input.jpg";
    printf("Loading image from file: %s... ", file_name);
    ImageData input_image{std::string(file_name)};
    printf("%s\n", input_image.print().c_str());

    printf("Creating dst image placeholder... ");
    ImageData output_image;
    output_image.width = input_image.width / 2;
    output_image.height = input_image.height / 2;
    output_image.original_channels = input_image.original_channels;
    output_image.desired_channels = input_image.desired_channels;
    output_image.level = input_image.level - 1;
    output_image.size = output_image.width * output_image.height * output_image.desired_channels;
    printf("%s\n", output_image.print().c_str());

    CreateStructuredBuffer(g_pDevice, sizeof(Pixel), input_image.width * input_image.height, input_image.pixels, &g_pBuf0);
    CreateStructuredBuffer(g_pDevice, sizeof(Pixel), output_image.width * output_image.height, nullptr, &g_pBufResult);
   
    
    //CreateStructuredBuffer(g_pDevice, sizeof(BufType), NUM_ELEMENTS, &g_vBuf0[0], &g_pBuf0);
    //CreateStructuredBuffer(g_pDevice, sizeof(BufType), NUM_ELEMENTS, &g_vBuf1[0], &g_pBuf1);
    //CreateStructuredBuffer(g_pDevice, sizeof(BufType), NUM_ELEMENTS, nullptr, &g_pBufResult);


#if defined(_DEBUG) || defined(PROFILE)
    if (g_pBuf0)
        g_pBuf0->SetPrivateData(WKPDID_D3DDebugObjectName, sizeof("Buffer0") - 1, "Buffer0");
    //if (g_pBuf1)
    //    g_pBuf1->SetPrivateData(WKPDID_D3DDebugObjectName, sizeof("Buffer1") - 1, "Buffer1");
    if (g_pBufResult)
        g_pBufResult->SetPrivateData(WKPDID_D3DDebugObjectName, sizeof("Result") - 1, "Result");
#endif

    printf("done\n");

    printf("Creating buffer views...");
    CreateBufferSRV(g_pDevice, g_pBuf0, &g_pBuf0SRV);
    // CreateBufferSRV(g_pDevice, g_pBuf1, &g_pBuf1SRV);
    CreateBufferUAV(g_pDevice, g_pBufResult, &g_pBufResultUAV);

#if defined(_DEBUG) || defined(PROFILE)
    if (g_pBuf0SRV)
        g_pBuf0SRV->SetPrivateData(WKPDID_D3DDebugObjectName, sizeof("Buffer0 SRV") - 1, "Buffer0 SRV");
    //if (g_pBuf1SRV)
    //    g_pBuf1SRV->SetPrivateData(WKPDID_D3DDebugObjectName, sizeof("Buffer1 SRV") - 1, "Buffer1 SRV");
    if (g_pBufResultUAV)
        g_pBufResultUAV->SetPrivateData(WKPDID_D3DDebugObjectName, sizeof("Result UAV") - 1, "Result UAV");
#endif

    printf("done\n");
    
    printf("Prepairing data for shader...");
    ShaderConstantData csConstants;
    csConstants.src_width = input_image.width;
    csConstants.src_height = input_image.height;
    csConstants.dst_width = output_image.width;
    csConstants.dst_height = output_image.height;
    if (FAILED(CreateConstantBuffer(g_pDevice, sizeof(csConstants), &csConstants, &g_pConstantBuffer))) {
        printf("Unable to create constant buffer...\n");
    }
    printf("done\n");

    printf("Running Compute Shader...");
    // ID3D11ShaderResourceView* aRViews[2] = { g_pBuf0SRV, g_pBuf1SRV };
    ID3D11ShaderResourceView* aRViews[1] = { g_pBuf0SRV };
    //RunComputeShader(g_pContext, g_pCS, 1, aRViews, nullptr, nullptr, 0, g_pBufResultUAV, input_image.width, input_image.height, 1);
    RunComputeShader(g_pContext, g_pCS, 1, aRViews, 
            g_pConstantBuffer, &csConstants, sizeof(csConstants), 
            g_pBufResultUAV, output_image.width, output_image.height, 1);
    printf("done\n");

    // Read back the result from GPU, and saving as an image into disk
    {
        ID3D11Buffer* resultbuf = CreateAndCopyToDebugBuf(g_pDevice, g_pContext, g_pBufResult);
        D3D11_MAPPED_SUBRESOURCE MappedResource;
        Pixel* p;
        g_pContext->Map(resultbuf, 0, D3D11_MAP_READ, 0, &MappedResource);

        // Set a break point here and put down the expression "p, [NUM_PIXELS]" in your watch window to see what has been written out by our CS
        // This is also a common trick to debug CS programs.
        p = (Pixel*)MappedResource.pData;

        // Write the image to disk to see if the results were correct
        printf("Write results to disk...\n");
        {
            const char* out_file_name = "textures/output.jpg";
            output_image.pixels = reinterpret_cast<unsigned char*>(p); // Borrow the data from p for a momment
            printf("Saving %s : %s\n", out_file_name, output_image.save(std::string(out_file_name)) ? "success" : "fail");
            output_image.pixels = nullptr; // Since we do not own this data, we cannot free it. We just borrowed from p to writing it
        }
        g_pContext->Unmap(resultbuf, 0);

        SAFE_RELEASE(resultbuf);
    }

    printf("Cleaning up...\n");
    SAFE_RELEASE(g_pBuf0SRV);
    // SAFE_RELEASE(g_pBuf1SRV);
    SAFE_RELEASE(g_pBufResultUAV);
    SAFE_RELEASE(g_pBuf0);
    // SAFE_RELEASE(g_pBuf1);
    SAFE_RELEASE(g_pBufResult);
    SAFE_RELEASE(g_pCS);
    SAFE_RELEASE(g_pContext);
    SAFE_RELEASE(g_pDevice);

    return 0;
}


//--------------------------------------------------------------------------------------
// Create the D3D device and device context suitable for running Compute Shaders(CS)
//--------------------------------------------------------------------------------------
_Use_decl_annotations_
HRESULT CreateComputeDevice(ID3D11Device** ppDeviceOut, ID3D11DeviceContext** ppContextOut, bool bForceRef)
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

//--------------------------------------------------------------------------------------
// Compile and create the CS
//--------------------------------------------------------------------------------------
_Use_decl_annotations_
HRESULT CreateComputeShader(LPCWSTR pSrcFile, LPCSTR pFunctionName,
    ID3D11Device* pDevice, ID3D11ComputeShader** ppShaderOut)
{
    if (!pDevice || !ppShaderOut)
        return E_INVALIDARG;

    // Finds the correct path for the shader file.
    // This is only required for this sample to be run correctly from within the Sample Browser,
    // in your own projects, these lines could be removed safely
    WCHAR str[MAX_PATH];
    HRESULT hr = FindDXSDKShaderFileCch(str, MAX_PATH, pSrcFile);
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
    hr = D3DCompileFromFile(str, defines, D3D_COMPILE_STANDARD_FILE_INCLUDE, pFunctionName, pProfile,
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

//--------------------------------------------------------------------------------------
// Create Constant Buffer
//--------------------------------------------------------------------------------------
_Use_decl_annotations_
HRESULT CreateConstantBuffer(ID3D11Device* pDevice, UINT uElementSize, void* pInitData, ID3D11Buffer** ppBufOut)
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
    } else{
        return pDevice->CreateBuffer(&cbDesc, nullptr, ppBufOut);
    }
   
}

//--------------------------------------------------------------------------------------
// Create Structured Buffer
//--------------------------------------------------------------------------------------
_Use_decl_annotations_
HRESULT CreateStructuredBuffer(ID3D11Device* pDevice, UINT uElementSize, UINT uCount, void* pInitData, ID3D11Buffer** ppBufOut)
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

//--------------------------------------------------------------------------------------
// Create Shader Resource View for Structured or Raw Buffers
//--------------------------------------------------------------------------------------
_Use_decl_annotations_
HRESULT CreateBufferSRV(ID3D11Device* pDevice, ID3D11Buffer* pBuffer, ID3D11ShaderResourceView** ppSRVOut)
{
    D3D11_BUFFER_DESC descBuf = {};
    pBuffer->GetDesc(&descBuf);

    D3D11_SHADER_RESOURCE_VIEW_DESC desc = {};
    desc.ViewDimension = D3D11_SRV_DIMENSION_BUFFEREX;
    desc.BufferEx.FirstElement = 0;

    if (descBuf.MiscFlags & D3D11_RESOURCE_MISC_BUFFER_ALLOW_RAW_VIEWS)
    {
        // This is a Raw Buffer

        desc.Format = DXGI_FORMAT_R32_TYPELESS;
        desc.BufferEx.Flags = D3D11_BUFFEREX_SRV_FLAG_RAW;
        desc.BufferEx.NumElements = descBuf.ByteWidth / 4;
    }
    else
        if (descBuf.MiscFlags & D3D11_RESOURCE_MISC_BUFFER_STRUCTURED)
        {
            // This is a Structured Buffer

            desc.Format = DXGI_FORMAT_UNKNOWN;
            desc.BufferEx.NumElements = descBuf.ByteWidth / descBuf.StructureByteStride;
        }
        else
        {
            return E_INVALIDARG;
        }

    return pDevice->CreateShaderResourceView(pBuffer, &desc, ppSRVOut);
}

//--------------------------------------------------------------------------------------
// Create Unordered Access View for Structured or Raw Buffers
//-------------------------------------------------------------------------------------- 
_Use_decl_annotations_
HRESULT CreateBufferUAV(ID3D11Device* pDevice, ID3D11Buffer* pBuffer, ID3D11UnorderedAccessView** ppUAVOut)
{
    D3D11_BUFFER_DESC descBuf = {};
    pBuffer->GetDesc(&descBuf);

    D3D11_UNORDERED_ACCESS_VIEW_DESC desc = {};
    desc.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
    desc.Buffer.FirstElement = 0;

    if (descBuf.MiscFlags & D3D11_RESOURCE_MISC_BUFFER_ALLOW_RAW_VIEWS)
    {
        // This is a Raw Buffer

        desc.Format = DXGI_FORMAT_R32_TYPELESS; // Format must be DXGI_FORMAT_R32_TYPELESS, when creating Raw Unordered Access View
        desc.Buffer.Flags = D3D11_BUFFER_UAV_FLAG_RAW;
        desc.Buffer.NumElements = descBuf.ByteWidth / 4;
    }
    else
        if (descBuf.MiscFlags & D3D11_RESOURCE_MISC_BUFFER_STRUCTURED)
        {
            // This is a Structured Buffer

            desc.Format = DXGI_FORMAT_UNKNOWN;      // Format must be must be DXGI_FORMAT_UNKNOWN, when creating a View of a Structured Buffer
            desc.Buffer.NumElements = descBuf.ByteWidth / descBuf.StructureByteStride;
        }
        else
        {
            return E_INVALIDARG;
        }

    return pDevice->CreateUnorderedAccessView(pBuffer, &desc, ppUAVOut);
}

//--------------------------------------------------------------------------------------
// Create a CPU accessible buffer and download the content of a GPU buffer into it
// This function is very useful for debugging CS programs
//-------------------------------------------------------------------------------------- 
_Use_decl_annotations_
ID3D11Buffer* CreateAndCopyToDebugBuf(ID3D11Device* pDevice, ID3D11DeviceContext* pd3dImmediateContext, ID3D11Buffer* pBuffer)
{
    ID3D11Buffer* debugbuf = nullptr;

    D3D11_BUFFER_DESC desc = {};
    pBuffer->GetDesc(&desc);
    desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    desc.Usage = D3D11_USAGE_STAGING;
    desc.BindFlags = 0;
    desc.MiscFlags = 0;
    if (SUCCEEDED(pDevice->CreateBuffer(&desc, nullptr, &debugbuf)))
    {
#if defined(_DEBUG) || defined(PROFILE)
        debugbuf->SetPrivateData(WKPDID_D3DDebugObjectName, sizeof("Debug") - 1, "Debug");
#endif

        pd3dImmediateContext->CopyResource(debugbuf, pBuffer);
    }

    return debugbuf;
}

//--------------------------------------------------------------------------------------
// Run CS
//-------------------------------------------------------------------------------------- 
_Use_decl_annotations_
void RunComputeShader(ID3D11DeviceContext* pd3dImmediateContext,
    ID3D11ComputeShader* pComputeShader,
    UINT nNumViews, ID3D11ShaderResourceView** pShaderResourceViews,
    ID3D11Buffer* pCBCS, void* pCSData, DWORD dwNumDataBytes,
    ID3D11UnorderedAccessView* pUnorderedAccessView,
    UINT X, UINT Y, UINT Z)
{
    pd3dImmediateContext->CSSetShader(pComputeShader, nullptr, 0);
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

//--------------------------------------------------------------------------------------
// Tries to find the location of the shader file
// This is a trimmed down version of DXUTFindDXSDKMediaFileCch.
//--------------------------------------------------------------------------------------
_Use_decl_annotations_
HRESULT FindDXSDKShaderFileCch(WCHAR* strDestPath,
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

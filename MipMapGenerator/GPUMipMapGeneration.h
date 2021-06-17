#pragma once

#include <crtdbg.h>
#include <d3dcommon.h>
#include <d3d11.h>
#include <d3dcompiler.h>

#include "ImageData.h"

struct Pixel {
	int color; // It's is a 32 bits RGBA pixel. Where each channel has 8bits
};

// For the constant data
// This struct needs to ba aligned by 16 bytes
// i. e. sizeof(ShaderConstantData) % 16 == 0
struct alignas(16) ShaderConstantData
{
    // width and height of the source texture
    int src_width;
    int src_height;
    // width and height of the destination texture. 
    // we will calculate them by halving the source texture ones.
    int dst_width;
    int dst_height;
    // Filter dimensions depends on the dimensions of the src texture
    // 0 - both are even
    // 1 - width is even and height is odd
    // 2 - width is odd and height is even
    // 3 - both are odd
    int dimension_case;
    // TODO: will choose which filter use to interpolate. By default is bi-linear
    int filter_option;
};

class GPUMipMapGenerator {
private:
	// General resources to use a CS
	ID3D11Device* mDevice{ nullptr };
	ID3D11DeviceContext* mContext{ nullptr };
	ID3D11ComputeShader* mComputeShader{ nullptr };
	//Input/output data related
	ID3D11Buffer* mBufInput{ nullptr };
	ID3D11Buffer* mBufResult{ nullptr };
	ID3D11Buffer* mConstantBuffer{ nullptr };
	// View to map the resources
	ID3D11ShaderResourceView* mBufInputSRV{ nullptr };
	ID3D11UnorderedAccessView* mBufResultUAV{ nullptr };
    // Compute shader source code file location
    const wchar_t* mShaderSrcFile = L"GenerateMip.hlsl";
    // Helper private methods
    HRESULT createComputeDevice(_Outptr_ ID3D11Device** ppDeviceOut, _Outptr_ ID3D11DeviceContext** ppContextOut, _In_ bool bForceRef);
    HRESULT createComputeShader(_In_z_ LPCWSTR pSrcFile, _In_z_ LPCSTR pFunctionName,
        _In_ ID3D11Device* pDevice, _Outptr_ ID3D11ComputeShader** ppShaderOut);
    HRESULT findDXSDKShaderFileCch(_Out_writes_(cchDest) WCHAR* strDestPath,
        _In_ int cchDest,
        _In_z_ LPCWSTR strFilename);
    HRESULT createStructuredBuffer(_In_ ID3D11Device* pDevice, _In_ UINT uElementSize, _In_ UINT uCount,
        _In_reads_(uElementSize* uCount) void* pInitData,
        _Outptr_ ID3D11Buffer** ppBufOut);
    HRESULT createConstantBuffer(_In_ ID3D11Device* pDevice, _In_ UINT uElementSize,
        _In_ void* pInitData,
        _Outptr_ ID3D11Buffer** ppBufOut);
    HRESULT createBufferSRV(_In_ ID3D11Device* pDevice, _In_ ID3D11Buffer* pBuffer, _Outptr_ ID3D11ShaderResourceView** ppSRVOut);
    HRESULT createBufferUAV(_In_ ID3D11Device* pDevice, _In_ ID3D11Buffer* pBuffer, _Outptr_ ID3D11UnorderedAccessView** pUAVOut);
    ID3D11Buffer* createAndCopyToDebugBuf(_In_ ID3D11Device* pDevice, _In_ ID3D11DeviceContext* pd3dImmediateContext, _In_ ID3D11Buffer* pBuffer);
    void runComputeShader(_In_ ID3D11DeviceContext* pd3dImmediateContext,
        _In_ ID3D11ComputeShader* pComputeShader,
        _In_ UINT nNumViews, _In_reads_(nNumViews) ID3D11ShaderResourceView** pShaderResourceViews,
        _In_opt_ ID3D11Buffer* pCBCS, _In_reads_opt_(dwNumDataBytes) void* pCSData, _In_ DWORD dwNumDataBytes,
        _In_ ID3D11UnorderedAccessView* pUnorderedAccessView,
        _In_ UINT X, _In_ UINT Y, _In_ UINT Z);

public:
	GPUMipMapGenerator();
	bool generateMip(const ImageData& src_image, ImageData& dst_image);
    ~GPUMipMapGenerator();
};

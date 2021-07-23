#pragma once

#include <crtdbg.h>
#include <d3dcommon.h>
#include <d3d11.h>
#include <d3dcompiler.h>

#include "ImageData.h"

// For the constant data
// This struct needs to ba aligned by 16 bytes
// i. e. sizeof(ShaderConstantData) % 16 == 0
struct alignas(16) ShaderConstantData
{
	float texel_size[2];	// 1.0 / srcTex.Dimensions
	int src_mip_level;
	// Case to filter according the parity of the dimensions in the src texture. 
	// Must be one of 0, 1, 2 or 3
	// See CSMain function bellow
	int dimension_case;
	// Ignored for now, if we want to use a different filter strategy. Current one is bi-linear filter
	int filter_option;
};

class GPUMipMapGenerator {
private:
	// General resources to use a CS
	ID3D11Device* mDevice{ nullptr };
	ID3D11DeviceContext* mContext{ nullptr };
	ID3D11ComputeShader* mComputeShader{ nullptr };
	//Input/output data related
    ID3D11Texture2D* mTextInput{ nullptr };
	ID3D11Texture2D* mTextResult{ nullptr };
    ID3D11SamplerState* mSamplerLinear{ nullptr };
	ID3D11Buffer* mConstantBuffer{ nullptr };
	// View to map the resources
	ID3D11ShaderResourceView* mTextInputSRV{ nullptr };
	ID3D11UnorderedAccessView* mTextResultUAV{ nullptr };
    // Compute shader source code file location
    const wchar_t* mShaderSrcFile = L"GenerateMip.hlsl";
	std::wstring mTmpImageStoreFile{ L"result.jpg" };
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
    HRESULT createSrcTexture(_In_ ID3D11Device* pDevice, _In_ const ImageData& image, _Outptr_ ID3D11Texture2D** pTextureOut);
	HRESULT createDstTexture(_In_ ID3D11Device* pDevice, _In_ const ImageData& image, _Outptr_ ID3D11Texture2D** pTextureOut);
	HRESULT createTextureSRV(_In_ ID3D11Device* pDevice, _In_ ID3D11Texture2D* pTexture, _Outptr_ ID3D11ShaderResourceView** ppSRVOut);
	HRESULT createTextureUAV(_In_ ID3D11Device* pDevice, _In_ ID3D11Texture2D* pTexture, _Outptr_ ID3D11UnorderedAccessView** pUAVOut);
    HRESULT createConstantBuffer(_In_ ID3D11Device* pDevice, _In_ UINT uElementSize,
        _In_ void* pInitData,
        _Outptr_ ID3D11Buffer** ppBufOut);
	HRESULT createSampler(_In_ ID3D11Device* pDevice, _Outptr_ ID3D11SamplerState** ppSamplerOut);
	void runComputeShader(_In_ ID3D11DeviceContext* pd3dImmediateContext,
		_In_ ID3D11ComputeShader* pComputeShader,
		_In_ UINT nNumViews, _In_reads_(nNumViews) ID3D11ShaderResourceView** pShaderResourceViews,
		_In_ UINT nNumSamplerStates, _In_reads_(nNumSamplerStates) ID3D11SamplerState** pShaderSamplerStates,
		_In_opt_ ID3D11Buffer* pCBCS, _In_reads_opt_(dwNumDataBytes) void* pCSData, _In_ DWORD dwNumDataBytes,
		_In_ ID3D11UnorderedAccessView* pUnorderedAccessView,
		_In_ UINT X, _In_ UINT Y, _In_ UINT Z);
	HRESULT saveResult(const wchar_t* resultImageFile);

public:
	GPUMipMapGenerator();
	bool generateMip(const ImageData& src_image, ImageData& dst_image);
	
    ~GPUMipMapGenerator();
};

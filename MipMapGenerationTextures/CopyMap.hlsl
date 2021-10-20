RWTexture2D<float4> dstTex : register(u0); // Destination texture

cbuffer ShaderConstantData : register(b1) {

	float2 texel_size;	// 1.0 / srcTex.Dimensions
	int src_mip_level;
	// Case to filter according the parity of the dimensions in the src texture. 
	// Must be one of 0, 1, 2 or 3
	// See CSMain function bellow
	int dimension_case;
	// Ignored for now, if we want to use a different filter strategy. Current one is bi-linear filter
	int filter_option;
};

Texture2D<float4> srcTex : register(t2); // Source texture

SamplerState LinearClampSampler : register(s3); // Sampler state

[numthreads(1, 1, 1)]
void CSMain(uint3 dispatchThreadID : SV_DispatchThreadID)
{
	// Calculate the coordinates of the top left corner of the neighbourhood
	float2 coordInSrc = ((2 * dispatchThreadID.xy) * texel_size) + 0.5 * texel_size;
	float3 color = srcTex.SampleLevel(LinearClampSampler, coordInSrc, src_mip_level).xyz;

	dstTex[dispatchThreadID.xy] = float4(color, 1.0f);
}

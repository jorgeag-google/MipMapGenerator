cbuffer ShaderConstantData : register(b0) {
	// Dimensions in pixels of the source texture
	int src_width;
	int src_height;
	// Dimensions in pixels of the destination texture
	int dst_width;
	int dst_height;
	float2 texel_size;	// 1.0 / srcTex.Dimensions
	int src_mip_level;
	// Case to filter according the parity of the dimensions in the src texture. 
	// Must be one of 0, 1, 2 or 3
	// See CSMain function bellow
	int dimension_case;
	// Ignored for now, if we want to use a different filter strategy. Current one is bi-linear filter
	int filter_option;
};

Texture2D<float4> srcTex : register(t0); // Source texture
RWTexture2D<float4> dstTex : register(u0); // Destination texture

// Linear clamp sampler.
SamplerState LinearClampSampler
{
	Filter = MIN_MAG_MIP_LINEAR;
	AddressU = TEXTURE_ADDRESS_CLAMP;
	AddressV = TEXTURE_ADDRESS_CLAMP;
};

[numthreads(1, 1, 1)]
void CSMain(uint3 dispatchThreadID : SV_DispatchThreadID)
{
	// flip coordinate at reading
	float2 sampleCoords = 1.0 - (texel_size * dispatchThreadID.xy + 0.5 * texel_size);
	float3 pixel = srcTex.SampleLevel(LinearClampSampler, sampleCoords, src_mip_level).rgb;
	// Desaturate this pixel (i.e make it grayscale)
	pixel.rgb = pixel.r * 0.3 + pixel.g * 0.59 + pixel.b * 0.11;
	// Put a red bar along  100 <= x < 200
	if (dispatchThreadID.y > 100 && dispatchThreadID.y < 200) {
		dstTex[dispatchThreadID.xy] = float4(1.0, 0.0f, 0.0f, 1.0f);
	} else {
		dstTex[dispatchThreadID.xy] = float4(pixel, 1.0);
	}
}

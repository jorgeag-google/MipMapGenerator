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

float3 blur(float2 srcCoords) {
	float3 resultPixel = float3(0.0, 0.0, 0.0);

	float2 samplingDeltas[3][3] = {
		{ {-texel_size.x, -texel_size.y}, {0.0, -texel_size.y}, {texel_size.x, -texel_size.y}},
		{ {-texel_size.x,           0.0}, {0.0,           0.0}, {texel_size.x,           0.0}},
		{ {-texel_size.x,  texel_size.y}, {0.0,  texel_size.y}, {texel_size.x,  texel_size.y}}
	};

	float kernell[3][3] = {
		{ 0.0625, 0.125, 0.0625},
		{ 0.125,  0.25 , 0.125},
		{ 0.0625, 0.125, 0.0625}
	};

	for (int j = 0; j < 3; j++) {
		for (int i = 0; i < 3; i++) {
			float2 sampleCoords = srcCoords + samplingDeltas[j][i];
			resultPixel += kernell[j][i] * srcTex.SampleLevel(LinearClampSampler, sampleCoords, src_mip_level).rgb;
		}
	}

	return resultPixel;
}

float3 sharpen(float2 srcCoords, float k) {
	float3 resultPixel = float3(0.0, 0.0, 0.0);

	float2 samplingDeltas[3][3] = {
		{ {-texel_size.x, -texel_size.y}, {0.0, -texel_size.y}, {texel_size.x, -texel_size.y}},
		{ {-texel_size.x,           0.0}, {0.0,           0.0}, {texel_size.x,           0.0}},
		{ {-texel_size.x,  texel_size.y}, {0.0,  texel_size.y}, {texel_size.x,  texel_size.y}}
	};

	float kernell[3][3] = {
		{ -0.0625 * k, -0.0625 * k, -0.0625 * k},
		{ -0.0625 * k,     1.0 * k, -0.0625 * k},
		{ -0.0625 * k, -0.0625 * k, -0.0625 * k}
	};

	for (int j = 0; j < 3; j++) {
		for (int i = 0; i < 3; i++) {
			float2 sampleCoords = srcCoords + samplingDeltas[j][i];
			resultPixel += kernell[j][i] * srcTex.SampleLevel(LinearClampSampler, sampleCoords, src_mip_level).rgb;
		}
	}

	return resultPixel;
}

[numthreads(1, 1, 1)]
void CSMain(uint3 dispatchThreadID : SV_DispatchThreadID)
{
	// Calculate the sampling coordinates of the center of this pixel
	float2 sampleCoords = texel_size * dispatchThreadID.xy + 0.5 * texel_size;
	// Blur the pixel
	//float3 result = blur(sampleCoords);
	float3 result = sharpen(sampleCoords, 2.0);
	// Write reult back to dst texture
	dstTex[dispatchThreadID.xy] = float4(result, 1.0);
}

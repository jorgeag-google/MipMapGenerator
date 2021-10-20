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

[numthreads(1, 1, 1)]
void CSMain(uint3 dispatchThreadID : SV_DispatchThreadID)
{
	float3 color = float3(0.5f, 0.5f, 0.5f);
	switch (filter_option % 4) {
		case 0:
			color = float3(1.0f, 0.0f, 0.0f);
		break;
		case 1:
			color = float3(0.0f, 1.0f, 0.0f);
		break;
		case 2:
			color = float3(0.0f, 0.0f, 1.0f);
		break;
		case 3:
			color = float3(1.0f, 1.0f, 0.0f);
		break;
	}

	dstTex[dispatchThreadID.xy] = float4(color, 1.0f);
}

RWTexture2D<float4> dstTex : register(u0); // Destination texture

cbuffer ShaderConstantData : register(b0) {

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

SamplerState LinearClampSampler : register(s0); // Sampler state

// According to the dimensions of the src texture we can be in one of four cases
float3 computePixelEvenEven(float2 scrCoords);
float3 computePixelEvenOdd(float2 srcCoords);
float3 computePixelOddEven(float2 srcCoords);
float3 computePixelOddOdd(float2 srcCoords);

[numthreads(1, 1, 1)]
void CSMain(uint3 dispatchThreadID : SV_DispatchThreadID)
{
	// Calculate the coordinates of the top left corner of the neighbourhood
	float2 coordInSrc = ((2 * dispatchThreadID.xy) * texel_size) + 0.5 * texel_size;
	
	float3 resultingPixel = float3(0.0f, 0.0f, 0.0f);
	// Get the filtered value from the src texture's neighbourhood
	// Choose the correct case according to src texture dimensions
	switch (dimension_case) {
		case 0: // Both dimension are even
			resultingPixel = computePixelEvenEven(coordInSrc);
		break;
		case 1: // width is even and height is odd
			resultingPixel = computePixelEvenOdd(coordInSrc);
		break;
		case 2: // width is odd an height is even
			resultingPixel = computePixelOddEven(coordInSrc);
		break;
		case 3: // both dimensions are odd
			resultingPixel = computePixelOddOdd(coordInSrc);
		break;
	}
	// Write the resulting color into dst texture
	dstTex[dispatchThreadID.xy] = float4(resultingPixel, 1.0f);
}


// In this case both dimensions (width and height) are even
// srcCoor are the coordinates of the top left corner of the neighbourhood in the src texture
float3 computePixelEvenEven(float2 srcCoords) {	
	float3 resultPixel = float3(0.0f, 0.0f, 0.0f);
	//We will need a 2x2 neighbourhood sampling
	const float2 neighboursCoords[2][2] = {
		{ {0.0, 0.0},          {texel_size.x, 0.0} },
		{ {0.0, texel_size.y}, {texel_size.x, texel_size.y} }
	};
	// Filter or kernell: These are the coeficients for the weighted average 1/4 = 0.25 
	const float coeficients[2][2] = { 
									  { 0.25f, 0.25f }, 
									  { 0.25f, 0.25f } 
									};
	// Perform the filtering by convolution
	for (int j = 0; j < 2; j++) {
		for (int i = 0; i < 2; i++) {
			float2 sampleCoords = srcCoords + neighboursCoords[j][i];
			resultPixel += coeficients[j][i] * srcTex.SampleLevel(LinearClampSampler, sampleCoords, src_mip_level).xyz;
		}
	}
	return resultPixel;
}

// In this case width is even and height is odd
// srcCoor are the coordinates of the top left corner of the neighbourhood in the src texture
// This neighbourhood has size 2x3 (in math matices notation)
float3 computePixelEvenOdd(float2 srcCoords) {
	float3 resultPixel = float3(0.0f, 0.0f, 0.0f);
	//We will need a 2x3 neighbourhood sampling
	const float2 neighboursCoords[2][3] = {
		{ {0.0,          0.0}, {texel_size.x,          0.0}, {2.0 * texel_size.x, 0.0} },
		{ {0.0, texel_size.y}, {texel_size.x, texel_size.y}, {2.0 * texel_size.x, texel_size.y} }
	};
	// Filter or kernell: These are the coeficients for the weighted average. 1/4 = 0.25, 1/8 = 0.125
	const float coeficients[2][3] = {
									  { 0.125f, 0.25f, 0.125f},
									  { 0.125f, 0.25f, 0.125f}
	};
	// Perform the filtering by convolution
	for (int j = 0; j < 2; j++) {
		for (int i = 0; i < 3; i++) {
			float2 sampleCoords = srcCoords + neighboursCoords[j][i];
			resultPixel += coeficients[j][i] * srcTex.SampleLevel(LinearClampSampler, sampleCoords, src_mip_level).xyz;
		}
	}
	return resultPixel;
}

// In this case width is odd and height is even
// srcCoor are the coordinates of the top left corner of the neighbourhood in the src texture
// This neighbourhood has size 3x2 (in math matices notation)
float3 computePixelOddEven(float2 srcCoords) {
	float3 resultPixel = float3(0.0f, 0.0f, 0.0f);
	//We will need a 3x2 neighbourhood sampling
	const float2 neighboursCoords[3][2] = {
		{ {0.0,                0.0}, {texel_size.x,               0.0f} },
		{ {0.0,       texel_size.y}, {texel_size.x,       texel_size.y} },
		{ {0.0, 2.0 * texel_size.y}, {texel_size.x, 2.0 * texel_size.y} }
	};
	// Filter or kernell: These are the coeficients for the weighted average. 1/4 = 0.25, 1/8 = 0.125
	const float coeficients[3][2] = {
									  { 0.125f, 0.125f },
									  { 0.25f,  0.25f },
									  { 0.125f, 0.125f }
	};
	// Perform the filtering by convolution
	for (int j = 0; j < 3; j++) {
		for (int i = 0; i < 2; i++) {
			float2 sampleCoords = srcCoords + neighboursCoords[j][i];
			resultPixel += coeficients[j][i] * srcTex.SampleLevel(LinearClampSampler, sampleCoords, src_mip_level).xyz;
		}
	}
	return resultPixel;
}

// In this case both width and height are odd
// srcCoor are the coordinates of the higher left corner of the neighbourhood in the src texture
// This neighbourhood has size 3x3 (in math matices notation)
float3 computePixelOddOdd(float2 srcCoords) {
	float3 resultPixel = float3(0.0f, 0.0f, 0.0f);
	//We will need a 3x3 neighbourhood sampling	
	const float2 neighboursCoords[3][3] = {
		{ {0.0,                0.0}, {texel_size.x,                0.0}, {2.0 * texel_size.x,                0.0} },
		{ {0.0,       texel_size.y}, {texel_size.x,       texel_size.y}, {2.0 * texel_size.x,       texel_size.y} },
		{ {0.0, 2.0 * texel_size.y}, {texel_size.x, 2.0 * texel_size.y}, {2.0 * texel_size.x, 2.0 * texel_size.y} }
	};
	// Filter or kernell: These are the coeficients for the weighted average. 1/4 = 0.25, 1/8 = 0.125, 1/16 = 0.0625
	const float coeficients[3][3] = {
									  { 0.0625f, 0.125f, 0.0625f},
									  { 0.125f,  0.25f,  0.125f},
									  { 0.0625f,  0.125f, 0.0625f}
	};
	// Perform the filtering by convolution
	for (int j = 0; j < 3; j++) {
		for (int i = 0; i < 3; i++) {
			float2 sampleCoords = srcCoords + neighboursCoords[j][i];
			resultPixel += coeficients[j][i] * srcTex.SampleLevel(LinearClampSampler, sampleCoords, src_mip_level).xyz;
		}
	}
	return resultPixel;
}

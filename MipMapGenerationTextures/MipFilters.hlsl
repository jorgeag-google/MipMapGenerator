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
// Filter funtions
float filter(float x);
float FilterBox(in float x);
float FilterTriangle(in float x);
float FilterGaussian(in float x);
float FilterCubic(in float x, in float B, in float C);
float FilterSinc(in float x, in float filterRadius);
float FilterBlackmanHarris(in float x);
float FilterSmoothstep(in float x);

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
	// Distance of the sample pos to the center of the filter of radious 1
	const float sampleDist[2][2] = {
									  { 0.707107f, 0.707107f },
									  { 0.707107f, 0.707107f }
	};
	float3 sum = float3(0.0, 0.0, 0.0);
	float totalWeight = 0.0;
	// Perform the filtering by convolution
	for (int j = 0; j < 2; j++) {
		for (int i = 0; i < 2; i++) {
			float x = sampleDist[j][i];
			float filterWeight = filter(x);
			totalWeight += filterWeight;
			float2 sampleCoords = srcCoords + neighboursCoords[j][i];
			float3 pixel = max(float3(0.0f, 0.0f, 0.0f), srcTex.SampleLevel(LinearClampSampler, sampleCoords, src_mip_level).xyz);
			sum += filterWeight * pixel;
		}
	}
	return max(sum / totalWeight, 0.0f);
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
	// Distance of the sample pos to the center of the filter of radious 1
	const float sampleDist[2][3] = {
									  { 0.745356f, 0.333333f, 0.745356f},
									  { 0.745356f, 0.333333f, 0.745356f}
	};
	float3 sum = float3(0.0, 0.0, 0.0);
	float totalWeight = 0.0;
	// Perform the filtering by convolution
	for (int j = 0; j < 2; j++) {
		for (int i = 0; i < 3; i++) {
			float x = sampleDist[j][i];
			float filterWeight = filter(x);
			totalWeight += filterWeight;
			float2 sampleCoords = srcCoords + neighboursCoords[j][i];
			float3 pixel = max(float3(0.0f, 0.0f, 0.0f), srcTex.SampleLevel(LinearClampSampler, sampleCoords, src_mip_level).xyz);
			sum += filterWeight * pixel;
		}
	}
	return max(sum / totalWeight, 0.0f);
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
	// Distance of the sample pos to the center of the filter of radious 1
	const float sampleDist[3][2] = {
									  { 0.745356f, 0.745356f },
									  { 0.333333f, 0.333333f },
									  { 0.745356f, 0.745356f }
	};
	float3 sum = float3(0.0, 0.0, 0.0);
	float totalWeight = 0.0;
	// Perform the filtering by convolution
	for (int j = 0; j < 3; j++) {
		for (int i = 0; i < 2; i++) {
			float x = sampleDist[j][i];
			float filterWeight = filter(x);
			totalWeight += filterWeight;
			float2 sampleCoords = srcCoords + neighboursCoords[j][i];
			float3 pixel = max(float3(0.0f, 0.0f, 0.0f), srcTex.SampleLevel(LinearClampSampler, sampleCoords, src_mip_level).xyz);
			sum += filterWeight * pixel;
		}
	}
	return max(sum / totalWeight, 0.0f);
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
	// Distance of the sample pos to the center of the filter of radious 1
	const float sampleDist[3][3] = {
									  { 0.942804f, 0.666667f, 0.942804f},
									  { 0.666667f,      0.0f, 0.666667f},
									  { 0.942804f, 0.666667f, 0.942804f}
	};
	float3 sum = float3(0.0, 0.0, 0.0);
	float totalWeight = 0.0;
	// Perform the filtering by convolution
	for (int j = 0; j < 3; j++) {
		for (int i = 0; i < 3; i++) {
			float x = sampleDist[j][i];
			float filterWeight = filter(x);
			totalWeight += filterWeight;
			float2 sampleCoords = srcCoords + neighboursCoords[j][i];
			float3 pixel = max(float3(0.0f, 0.0f, 0.0f), srcTex.SampleLevel(LinearClampSampler, sampleCoords, src_mip_level).xyz);
			sum += filterWeight * pixel;
		}
	}
	return max(sum / totalWeight, 0.0f);
}

float filter(in float x)
{
	// Cubic filters naturually work in a [-2, 2] domain. For the resolve case we
	// want to rescale the filter so that it works in [-1, 1] instead
	float cubicX = x * 2.0f;
	float result = 0.0f;
	switch (filter_option) {
	case 0: // FilterTypes_Box
		result = FilterBox(x);
		break;
	case 1: // FilterTypes_Triangle
		result = FilterTriangle(x);
		break;
	case 2: // FilterTypes_Gaussian
		result = FilterGaussian(x);
		break;
	case 3: // FilterTypes_BlackmanHarris
		result = FilterBlackmanHarris(x);
		break;
	case 4: // FilterTypes_Smoothstep
		result = FilterSmoothstep(x);
		break;
	case 5: // FilterTypes_BSpline
		result = FilterCubic(cubicX, 1.0, 0.0f);
		break;
	case 6: // FilterTypes_CatmullRom
		result = FilterCubic(cubicX, 0, 0.5f);
		break;
	case 7: // FilterTypes_Mitchell
		result = FilterCubic(cubicX, 1 / 3.0f, 1 / 3.0f);
		break;
	case 8: // FilterTypes_GeneralizedCubic
		const float CubicB = 1.0;
		const float CubicC = 1.0;
		result = FilterCubic(cubicX, CubicB, CubicC);
		break;
	case 9: // FilterTypes_Sinc
		const float filterRadius = 1.0f;
		result = FilterSinc(x, filterRadius);
		break;
	}

	return result;
}


// All filtering functions assume that 'x' is normalized to [0, 1], where 1 == FilteRadius
float FilterBox(in float x)
{
	return x <= 1.0f;
}

float FilterTriangle(in float x)
{
	return saturate(1.0f - x);
}

float FilterGaussian(in float x)
{
	const float sigma = 1.0f;
	const float g = 1.0f / sqrt(2.0f * 3.14159f * sigma * sigma);
	return (g * exp(-(x * x) / (2 * sigma * sigma)));
}

float FilterCubic(in float x, in float B, in float C)
{
	float y = 0.0f;
	float x2 = x * x;
	float x3 = x * x * x;
	if (x < 1)
		y = (12 - 9 * B - 6 * C) * x3 + (-18 + 12 * B + 6 * C) * x2 + (6 - 2 * B);
	else if (x <= 2)
		y = (-B - 6 * C) * x3 + (6 * B + 30 * C) * x2 + (-12 * B - 48 * C) * x + (8 * B + 24 * C);

	return y / 6.0f;
}

float FilterSinc(in float x, in float filterRadius)
{
	float s;
	const float Pi = 3.14159265f;

	x *= filterRadius * 2.0f;

	if (x < 0.001f)
		s = 1.0f;
	else
		s = sin(x * Pi) / (x * Pi);

	return s;
}

float FilterBlackmanHarris(in float x)
{
	x = 1.0f - x;
	const float Pi = 3.14159265f;
	const float a0 = 0.35875f;
	const float a1 = 0.48829f;
	const float a2 = 0.14128f;
	const float a3 = 0.01168f;
	return saturate(a0 - a1 * cos(Pi * x) + a2 * cos(2 * Pi * x) - a3 * cos(3 * Pi * x));
}

float FilterSmoothstep(in float x)
{
	return 1.0f - smoothstep(0.0f, 1.0f, x);
}
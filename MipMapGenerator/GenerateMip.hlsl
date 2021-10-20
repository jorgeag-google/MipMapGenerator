cbuffer ShaderConstantData : register(b0) {
	// Dimensions in pixels of the source texture
	int src_width;
	int src_height; 
	// Dimensions in pixels of the destination texture
	int dst_width;
	int dst_height;
	// Case to filter according the parity of the dimensions in the src texture. 
	// Must be one of 0, 1, 2 or 3
	// See CSMain function bellow
	int dimension_case;
	// Ignored for now, if we want to use a different filter startegy. Current one is bi-linear filter
	int filter_option;
};

struct Pixel
{
	int colour; // It's is a 32 bits RGBA pixel. Where each channel has 8bits
};

StructuredBuffer<Pixel> Buffer0 : register(t0); // Source texture
RWStructuredBuffer<Pixel> BufferOut : register(u0); // Destination texture

// Helper functions to fetch/write values into the textures
void writeToPixel(int x, int y, float3 colour);
float3 readPixel(int x, int y);

// According to the dimensions of the src texture we can be in one of four cases
float3 computePixelEvenEven(int2 scrCoords);
float3 computePixelEvenOdd(int2 srcCoords);
float3 computePixelOddEven(int2 srcCoords);
float3 computePixelOddOdd(int2 srcCoords);

[numthreads(1, 1, 1)]
void CSMain(uint3 dispatchThreadID : SV_DispatchThreadID)
{
	// Calculate the coordinates of the top left corner of the neighbourhood
	int2 coordInSrc = 2 * dispatchThreadID.xy;
	
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
	writeToPixel(dispatchThreadID.x, dispatchThreadID.y, resultingPixel);
}


// In this case both dimensions (width and height) are even
// srcCoor are the coordinates of the top left corner of the neighbourhood in the src texture
float3 computePixelEvenEven(int2 srcCoords) {	
	float3 resultPixel = float3(0.0f, 0.0f, 0.0f);
	//We will need a 2x2 neighbourhood sampling
	const int2 neighbours[2][2] = {
		{ {srcCoords.x, srcCoords.y    }, {srcCoords.x + 1, srcCoords.y    } },
		{ {srcCoords.x, srcCoords.y + 1}, {srcCoords.x + 1, srcCoords.y + 1} }
	};
	// Filter or kernell: These are the coeficients for the weighted average 1/4 = 0.25 
	const float coeficients[2][2] = { 
									  { 0.25f, 0.25f }, 
									  { 0.25f, 0.25f } 
									};
	// Perform the filtering by convolution
	for (int j = 0; j < 2; j++) {
		for (int i = 0; i < 2; i++) {
			resultPixel += coeficients[j][i] * readPixel(neighbours[j][i].x, neighbours[j][i].y);
		}
	}
	return resultPixel;
}

// In this case width is even and height is odd
// srcCoor are the coordinates of the top left corner of the neighbourhood in the src texture
// This neighbourhood has size 2x3 (in math matices notation)
float3 computePixelEvenOdd(int2 srcCoords) {
	float3 resultPixel = float3(0.0f, 0.0f, 0.0f);
	//We will need a 2x3 neighbourhood sampling
	const int2 neighbours[2][3] = {
		{ {srcCoords.x, srcCoords.y    }, {srcCoords.x + 1, srcCoords.y    }, {srcCoords.x + 2, srcCoords.y    } },
		{ {srcCoords.x, srcCoords.y + 1}, {srcCoords.x + 1, srcCoords.y + 1}, {srcCoords.x + 2, srcCoords.y + 1} }
	};
	// Filter or kernell: These are the coeficients for the weighted average. 1/4 = 0.25, 1/8 = 0.125
	const float coeficients[2][3] = {
									  { 0.125f, 0.25f, 0.125f},
									  { 0.125f, 0.25f, 0.125f}
	};
	// Perform the filtering by convolution
	for (int j = 0; j < 2; j++) {
		for (int i = 0; i < 3; i++) {
			resultPixel += coeficients[j][i] * readPixel(neighbours[j][i].x, neighbours[j][i].y);
		}
	}
	return resultPixel;
}

// In this case width is odd and height is even
// srcCoor are the coordinates of the top left corner of the neighbourhood in the src texture
// This neighbourhood has size 3x2 (in math matices notation)
float3 computePixelOddEven(int2 srcCoords) {
	float3 resultPixel = float3(0.0f, 0.0f, 0.0f);
	//We will need a 3x2 neighbourhood sampling
	const int2 neighbours[3][2] = {
		{ {srcCoords.x, srcCoords.y    }, {srcCoords.x + 1, srcCoords.y    } },
		{ {srcCoords.x, srcCoords.y + 1}, {srcCoords.x + 1, srcCoords.y + 1} },
		{ {srcCoords.x, srcCoords.y + 2}, {srcCoords.x + 1, srcCoords.y + 2} }
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
			resultPixel += coeficients[j][i] * readPixel(neighbours[j][i].x, neighbours[j][i].y);
		}
	}
	return resultPixel;
}

// In this case both width and height are odd
// srcCoor are the coordinates of the higher left corner of the neighbourhood in the src texture
// This neighbourhood has size 3x3 (in math matices notation)
float3 computePixelOddOdd(int2 srcCoords) {
	float3 resultPixel = float3(0.0f, 0.0f, 0.0f);
	//We will need a 3x3 neighbourhood sampling
	const int2 neighbours[3][3] = {
		{ {srcCoords.x, srcCoords.y    }, {srcCoords.x + 1, srcCoords.y    }, {srcCoords.x + 2, srcCoords.y    } },
		{ {srcCoords.x, srcCoords.y + 1}, {srcCoords.x + 1, srcCoords.y + 1}, {srcCoords.x + 2, srcCoords.y + 1} },
		{ {srcCoords.x, srcCoords.y + 2}, {srcCoords.x + 1, srcCoords.y + 2}, {srcCoords.x + 2, srcCoords.y + 2} }
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
			resultPixel += coeficients[j][i] * readPixel(neighbours[j][i].x, neighbours[j][i].y);
		}
	}
	return resultPixel;
}
// Write the colour to the destitantion texture at pixle coordinates (x,y)
// in order to work propartlly x must be in [0, dst_width) and y in [0, dst_height]
// otherwise a different pixel might be written
void writeToPixel(int x, int y, float3 colour) {
	// Since image is flattened, we need to recover the corresponding index
	uint index = (x + y * dst_width);
	// The pixels are encoded a an unsigned 32 bit integer
	// We need to clamp then in [0.0f, 1.0f] before encoding them as 
	// 8 bits per channel in the order RGBA
	int ired =   (int)(clamp(colour.r, 0.0f, 1.0f) * 255);
	int igreen = (int)(clamp(colour.g, 0.0f, 1.0f) * 255) << 8;
	int iblue =  (int)(clamp(colour.b, 0.0f, 1.0f) * 255) << 16;
	// Write to destination texture
	BufferOut[index].colour = ired + igreen + iblue;
}

// Read a colour from the source texture at pixle coordinates (x,y)
// in order to work propartlly x must be in [0, src_width) and y in [0, src_height]
// otherwise a different pixel might be returned
float3 readPixel(int x, int y) {
	float3 output;
	// Since image is flattened, we need to recover the corresponding index
	uint index = (x + y * src_width);
	// The pixels are encoded a an unsigned 32 bit integer
	// with 8 bits per channel in the order RGBA
	// We need to recover them as float in [0.0f, 1.0f]
	output.x = (float)(((Buffer0[index].colour) & 0x000000ff)      ) / 255.0f;
	output.y = (float)(((Buffer0[index].colour) & 0x0000ff00) >>  8) / 255.0f;
	output.z = (float)(((Buffer0[index].colour) & 0x00ff0000) >> 16) / 255.0f;

	return output;
}

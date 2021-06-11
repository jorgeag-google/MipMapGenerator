cbuffer ShaderConstantData : register(b0) {
	int src_width;
	int src_height;
	int dst_width;
	int dst_height;
};

struct Pixel
{
	int colour;
};

StructuredBuffer<Pixel> Buffer0 : register(t0);
RWStructuredBuffer<Pixel> BufferOut : register(u0);

float3 readPixel(int x, int y)
{
	float3 output;
	uint index = (x + y * src_width);

	output.x = (float)(((Buffer0[index].colour) & 0x000000ff)) / 255.0f;
	output.y = (float)(((Buffer0[index].colour) & 0x0000ff00) >> 8) / 255.0f;
	output.z = (float)(((Buffer0[index].colour) & 0x00ff0000) >> 16) / 255.0f;

	return output;
}

void writeToPixel(int x, int y, float3 colour)
{
	uint index = (x + y * dst_width);

	int ired = (int)(clamp(colour.r, 0, 1) * 255);
	int igreen = (int)(clamp(colour.g, 0, 1) * 255) << 8;
	int iblue = (int)(clamp(colour.b, 0, 1) * 255) << 16;

	BufferOut[index].colour = ired + igreen + iblue;
}

[numthreads(1, 1, 1)]
void CSMain(uint3 dispatchThreadID : SV_DispatchThreadID)
{
	// We are assumimg that both dimesnions on src and destination are even
	// Moreover, we assume that dst is exactlly half the size of src on each dimension
	float3 resultPixel = float3(1.0f, 0.0f, 0.0f);
	// Query the four corresponding pixels from src texture
	float3 pixelInSrc[4];
	int2 coordInSrc = 2 * dispatchThreadID.xy;
	pixelInSrc[0] = readPixel(coordInSrc.x, coordInSrc.y);
	pixelInSrc[1] = readPixel(coordInSrc.x + 1, coordInSrc.y);
	pixelInSrc[2] = readPixel(coordInSrc.x, coordInSrc.y + 1);
	pixelInSrc[3] = readPixel(coordInSrc.x + 1, coordInSrc.y + 1);
	// Average the color from the four samples
	resultPixel = 0.25f * pixelInSrc[0] + 0.25f * pixelInSrc[1] + 0.25f * pixelInSrc[2] + 0.25f * pixelInSrc[3];
	// Write the resulting color into dst texture
	writeToPixel(dispatchThreadID.x, dispatchThreadID.y, resultPixel);
}

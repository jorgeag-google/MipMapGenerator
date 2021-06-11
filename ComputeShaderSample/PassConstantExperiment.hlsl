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
	// flip coordinate at reading
	float3 pixel = readPixel(src_width - dispatchThreadID.x - 1, src_height - dispatchThreadID.y - 1); 
	// Desaturate this pixel (i.e make it grayscale)
	pixel.rgb = pixel.r * 0.3 + pixel.g * 0.59 + pixel.b * 0.11;
	// Put a red bar along  100 <= x < 200
	if (dispatchThreadID.y > 100 && dispatchThreadID.y < 200) {
		writeToPixel(dispatchThreadID.x, dispatchThreadID.y, float3(1.0f, 0.0, 0.0));
	} else {
		writeToPixel(dispatchThreadID.x, dispatchThreadID.y, pixel);
	}
}

// Inputs
Texture2D<float4> InputTexture : register(t0);

// Outputs
RWTexture2D<float2> OutputTexture : register(u0);

// Samplers
SamplerState LinearSampler : register(s0);

// Entry point
[numthreads(TGSize_, TGSize_, 1)]
void Rescale(uint3 GroupID : SV_GroupID, uint3 DispatchThreadID : SV_DispatchThreadID,
	uint3 GroupThreadID : SV_GroupThreadID, uint GroupIndex : SV_GroupIndex)
{
	uint2 samplePos = GroupID.xy * uint2(TGSize_, TGSize_) + GroupThreadID.xy;

	uint2 textureSize;
	OutputTexture.GetDimensions(textureSize.x, textureSize.y);

	[branch]
	if (samplePos.x < textureSize.x && samplePos.y < textureSize.y)
	{
		float2 uv = (samplePos + 0.5f) / textureSize;
		OutputTexture[samplePos] = InputTexture(LinearSampler, uv);
	}
}
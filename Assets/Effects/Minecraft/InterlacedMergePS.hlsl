Texture2D baseTexture;
SamplerState baseSampler;

Texture2D normalTexture;
SamplerState normalSampler;

Texture2D flagTexture;
SamplerState flagSampler;

#define leftImage baseTexture
#define rightImage normalTexture
#define maskImage flagTexture

struct PS_INPUT
{
	float2 vertexTCoord : TEXCOORD0;
};

struct PS_OUTPUT
{
	float4 pixelColor0 : SV_TARGET0;
};

PS_OUTPUT PSMain(PS_INPUT input)
{
	float4 left = leftImage.Sample(baseSampler, input.vertexTCoord);
	float4 right = rightImage.Sample(normalSampler, input.vertexTCoord);
	float4 mask = maskImage.Sample(flagSampler, input.vertexTCoord);

	PS_OUTPUT output;
	if (mask.r > 0.5)
		output.pixelColor0 = right;
	else
		output.pixelColor0 = left;
	return output;
}
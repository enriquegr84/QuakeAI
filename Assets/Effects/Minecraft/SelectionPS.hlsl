Texture2D baseTexture;
SamplerState baseSampler;

struct PS_INPUT
{
	float4 vertexColor : COLOR0;
	float2 vertexTCoord : TEXCOORD0;
};

struct PS_OUTPUT
{
	float4 pixelColor0 : SV_TARGET0;
};

PS_OUTPUT PSMain(PS_INPUT input)
{
	PS_OUTPUT output;

	float2 uv = input.vertexTCoord;
	float4 color = baseTexture.Sample(baseSampler, uv).rgba;
	color.rgb *= input.vertexColor.rgb;
	output.pixelColor0 = color;
	return output;
}
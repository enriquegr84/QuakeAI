Texture2D baseTexture;
SamplerState baseSampler;

cbuffer EmissiveColor
{
	float4 emissiveColor;
};
cbuffer SkyBgColor
{
	float4 skyBgColor;
};
cbuffer FogDistance
{
	float fogDistance;
};

struct PS_INPUT
{
	float3 vertexPosition : TEXCOORD0;
	float3 vertexNormal : TEXCOORD1;
	// World position in the visible world (i.e. relative to the cameraOffset.)
	// This can be used for many shader effects without loss of precision.
	// If the absolute position is required it can be calculated with
	// cameraOffset + worldPosition (for large coordinates the limits of float
	// precision must be considered).
	float3 worldPosition : TEXCOORD2;
	float4 vertexColor : COLOR0;
	float2 vertexTCoord : TEXCOORD3;
	float3 eyeVec : TEXCOORD4;
	float vIDiff : TEXCOORD5;
};

struct PS_OUTPUT
{
	float4 pixelColor0 : SV_TARGET0;
};

static const float e = 2.718281828459;
static const float BS = 10.0;
static const float fogStart = FOG_START;
static const float fogShadingParameter = 1.0 / (1.0 - fogStart);

#if ENABLE_TONE_MAPPING

/* Hable's UC2 Tone mapping parameters
	A = 0.22;
	B = 0.30;
	C = 0.10;
	D = 0.20;
	E = 0.01;
	F = 0.30;
	W = 11.2;
	equation used:  ((x * (A * x + C * B) + D * E) / (x * (A * x + B) + D * F)) - E / F
*/

float3 uncharted2Tonemap(float3 x)
{
	float3 result;
	result = ((x * (0.22 * x + 0.03) + 0.002) / (x * (0.22 * x + 0.3) + 0.06)) - 0.03333;
	return result;
}

float4 applyToneMapping(float4 color)
{
	float4 result;
	color = float4(pow(color.rgb, float3(2.2)), color.a);
	const float gamma = 1.6;
	const float exposureBias = 5.5;
	color.rgb = uncharted2Tonemap(exposureBias * color.rgb);
	// Precalculated white_scale from
	//float3 whiteScale = 1.0 / uncharted2Tonemap(float3(W));
	float3 whiteScale = float3(1.036015346);
	color.rgb *= whiteScale;
	result = float4(pow(color.rgb, float3(1.0 / gamma)), color.a);
	return result;
}
#endif

PS_OUTPUT PSMain(PS_INPUT input)
{
	PS_OUTPUT output;

	float3 color;
	float2 uv = input.vertexTCoord;

	float4 base = baseTexture.Sample(baseSampler, uv).rgba;
	color = base.rgb;

	float4 col = float4(color.rgb, base.a);
	col.rgb *= input.vertexColor.rgb;
	col.rgb *= emissiveColor.rgb * input.vIDiff;

#if ENABLE_TONE_MAPPING
	col = applyToneMapping(col);
#endif

	// Due to a bug in some (older ?) graphics stacks (possibly in the glsl compiler ?),
	// the fog will only be rendered correctly if the last operation before the
	// clamp() is an addition. Else, the clamp() seems to be ignored.
	// E.g. the following won't work:
	//      float clarity = clamp(fogShadingParameter
	//		* (fogDistance - length(input.eyeVec)) / fogDistance), 0.0, 1.0);
	// As additions usually come for free following a multiplication, the new formula
	// should be more efficient as well.
	// Note: clarity = (1 - fogginess)
	float clarity = clamp(fogShadingParameter
		- fogShadingParameter * length(input.eyeVec) / fogDistance, 0.0, 1.0);
	col = lerp(skyBgColor, col, clarity);

	output.pixelColor0 = float4(col.rgb, base.a);
	return output;
}

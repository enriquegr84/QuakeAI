cbuffer PVWMatrix
{
	float4x4 pvwMatrix;
};

struct VS_INPUT
{
	float3 modelPosition : POSITION;
	float2 modelTCoord : TEXCOORD0;
	float4 modelColor : COLOR0;
};

struct VS_OUTPUT
{
	float4 vertexColor : COLOR0;
	float2 vertexTCoord : TEXCOORD0;
	float4 clipPosition : SV_POSITION;
};

VS_OUTPUT VSMain(VS_INPUT input)
{
	VS_OUTPUT output;
	output.vertexTCoord = input.modelTCoord;
#if GE_USE_MAT_VEC
	output.clipPosition = mul(pvwMatrix, float4(input.modelPosition, 1.0f));
#else
	output.clipPosition = mul(float4(input.modelPosition, 1.0f), pvwMatrix);
#endif

	output.vertexColor = input.modelColor;
	return output;
}
cbuffer PVWMatrix
{
    float4x4 pvwMatrix;
};

cbuffer StarColor
{
	float4 starColor;
};

struct VS_INPUT
{
	float3 modelPosition : POSITION;
};

struct VS_OUTPUT
{
	float4 vertexColor : COLOR0;
	float4 clipPosition : SV_POSITION;
};

VS_OUTPUT VSMain(VS_INPUT input)
{
	VS_OUTPUT output;
#if GE_USE_MAT_VEC
	output.clipPosition = mul(pvwMatrix, float4(input.modelPosition, 1.0f));
#else
	output.clipPosition = mul(float4(input.modelPosition, 1.0f), pvwMatrix);
#endif
	output.vertexColor = starColor;
	return output;
}
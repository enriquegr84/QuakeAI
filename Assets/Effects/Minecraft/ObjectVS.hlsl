cbuffer WMatrix
{
	float4x4 wMatrix;
};
cbuffer VWMatrix
{
	float4x4 vwMatrix;
};
cbuffer PVMatrix
{
	float4x4 pvMatrix;
};
cbuffer PVWMatrix
{
	float4x4 pvwMatrix;
};

struct VS_INPUT
{
	float3 modelPosition : POSITION;
	float2 modelTCoord : TEXCOORD0;
	float4 modelColor : COLOR0;
	float3 modelNormal : NORMAL;
};

struct VS_OUTPUT
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
	float4 clipPosition : SV_POSITION;
};

static const float e = 2.718281828459;
static const float BS = 10.0;

float directionalAmbient(float3 normal)
{
	float result;
	float3 v = normal * normal;

	if (normal.y < 0.0)
		result = dot(v, float3(0.670820, 0.447213, 0.836660));
	else
		result = dot(v, float3(0.670820, 1.000000, 0.836660));
	return result;
}

VS_OUTPUT VSMain(VS_INPUT input)
{
	VS_OUTPUT output;
	output.vertexTCoord = input.modelTCoord;
#if GE_USE_MAT_VEC
	output.worldPosition = mul(wMatrix, float4(input.modelPosition, 1.0f)).xyz;
#else
	output.worldPosition = mul(float4(input.modelPosition, 1.0f), wMatrix).xyz;
#endif

#if GE_USE_MAT_VEC
	output.clipPosition = mul(pvMatrix, float4(output.worldPosition, 1.0f));
#else
	output.clipPosition = mul(float4(output.worldPosition, 1.0f), pvMatrix);
#endif

	output.vertexPosition = output.clipPosition.xyz;
	output.vertexNormal = input.modelNormal;

#if GE_USE_MAT_VEC
	output.eyeVec = -mul(vwMatrix, float4(input.modelPosition, 1.0f)).xyz;
#else
	output.eyeVec = -mul(float4(input.modelPosition, 1.0f), vwMatrix).xyz;
#endif

#if (MATERIAL_TYPE == TILE_MATERIAL_PLAIN) || (MATERIAL_TYPE == TILE_MATERIAL_PLAIN_ALPHA)
	output.vIDiff = 1.0;
#else
	// This is intentional comparison with zero without any margin.
	// If normal is not equal to zero exactly, then we assume it's a valid, just not normalized vector
	output.vIDiff = length(input.modelNormal) == 0.0 ?
		1.0 : directionalAmbient(normalize(input.modelNormal));
#endif

	output.vertexColor = input.modelColor;
	return output;
}
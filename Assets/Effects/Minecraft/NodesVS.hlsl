cbuffer WMatrix
{
	float4x4 wMatrix;
};
cbuffer VWMatrix
{
	float4x4 vwMatrix;
};
cbuffer PVWMatrix
{
	float4x4 pvwMatrix;
};
cbuffer DayLight
{
	// Color of the light emitted by the sun.
	float3 dayLight;
};
cbuffer CameraOffset
{
	// The cameraOffset is the current center of the visible world.
	float3 cameraOffset;
};
cbuffer AnimationTimer
{
	float animationTimer;
};

struct VS_INPUT
{
	float3 modelPosition : POSITION;
	float3 modelBlockPos : TEXCOORD0;
	float3 modelTCoord : TEXCOORD1;
	float4 modelColor : COLOR0;
};

struct VS_OUTPUT
{
	float3 vertexPosition : TEXCOORD0;
	// World position in the visible world (i.e. relative to the cameraOffset.)
	// This can be used for many shader effects without loss of precision.
	// If the absolute position is required it can be calculated with
	// cameraOffset + worldPosition (for large coordinates the limits of float
	// precision must be considered).
	float3 worldPosition : TEXCOORD1;
	float4 vertexColor : COLOR0;
	float3 vertexTCoord : TEXCOORD2;
	float3 eyeVec : TEXCOORD3;
	float4 clipPosition : SV_POSITION;
};

// Color of the light emitted by the light sources.
static const float3 artificialLight = float3(1.04, 1.04, 1.04);
static const float e = 2.718281828459;
static const float BS = 10.0;

float smoothCurve(float x)
{
	float result;
	result = x * x * (3.0 - 2.0 * x);
	return result;
}

float triangleWave(float x)
{
	float result;
	result = abs(frac(x + 0.5) * 2.0 - 1.0);
	return result;
}

float smoothTriangleWave(float x)
{
	float result;
	result = smoothCurve(triangleWave(x)) * 2.0 - 1.0;
	return result;
}

// OpenGL < 4.3 does not support continued preprocessor lines
#if (MATERIAL_TYPE == TILE_MATERIAL_WAVING_LIQUID_TRANSPARENT || MATERIAL_TYPE == TILE_MATERIAL_WAVING_LIQUID_OPAQUE || MATERIAL_TYPE == TILE_MATERIAL_WAVING_LIQUID_BASIC) && ENABLE_WAVING_WATER

//
// Simple, fast noise function.
// See: https://gist.github.com/patriciogonzalezvivo/670c22f3966e662d2f83
//
float4 perm(float4 x)
{
	float4 result;
	result = mod(((x * 34.0) + 1.0) * x, 289.0);
	return result;
}

float snoise(float3 p)
{
	float result;
	float3 a = floor(p);
	float3 d = p - a;
	d = d * d * (3.0 - 2.0 * d);

	float4 b = a.xxyy + float4(0.0, 1.0, 0.0, 1.0);
	float4 k1 = perm(b.xyxy);
	float4 k2 = perm(k1.xyxy + b.zzww);

	float4 c = k2 + a.zzzz;
	float4 k3 = perm(c);
	float4 k4 = perm(c + 1.0);

	float4 o1 = frac(k3 * (1.0 / 41.0));
	float4 o2 = frac(k4 * (1.0 / 41.0));

	float4 o3 = o2 * d.z + o1 * (1.0 - d.z);
	float2 o4 = o3.yw * d.x + o3.xz * (1.0 - d.x);

	result = o4.y * d.y + o4.x * (1.0 - d.y);
	return result;
}

#endif

VS_OUTPUT VSMain(VS_INPUT input)
{
	VS_OUTPUT output;
	output.vertexTCoord = input.modelTCoord;

	float dispX;
	float dispZ;
	float3 modelWorldPosition = (input.modelBlockPos - cameraOffset) + input.modelPosition;

	// OpenGL < 4.3 does not support continued preprocessor lines
#if (MATERIAL_TYPE == TILE_MATERIAL_WAVING_LEAVES && ENABLE_WAVING_LEAVES) || (MATERIAL_TYPE == TILE_MATERIAL_WAVING_PLANTS && ENABLE_WAVING_PLANTS)

#if GE_USE_MAT_VEC
	float4 pos2 = mul(wMatrix, float4(modelWorldPosition, 1.0f));
#else
	float4 pos2 = mul(float4(modelWorldPositionn, 1.0f), wMatrix);
#endif

	float tOffset = (pos2.x + pos2.y) * 0.001 + pos2.z * 0.002;
	dispX = (smoothTriangleWave(animationTimer * 23.0 + tOffset) +
		smoothTriangleWave(animationTimer * 11.0 + tOffset)) * 0.4;
	dispZ = (smoothTriangleWave(animationTimer * 31.0 + tOffset) +
		smoothTriangleWave(animationTimer * 29.0 + tOffset) +
		smoothTriangleWave(animationTimer * 13.0 + tOffset)) * 0.5;
#endif

#if GE_USE_MAT_VEC
	output.worldPosition = mul(wMatrix, float4(modelWorldPosition, 1.0f)).xyz;
#else
	output.worldPosition = mul(float4(modelWorldPosition, 1.0f), wMatrix).xyz;
#endif

	// OpenGL < 4.3 does not support continued preprocessor lines
#if (MATERIAL_TYPE == TILE_MATERIAL_WAVING_LIQUID_TRANSPARENT || MATERIAL_TYPE == TILE_MATERIAL_WAVING_LIQUID_OPAQUE || MATERIAL_TYPE == TILE_MATERIAL_WAVING_LIQUID_BASIC) && ENABLE_WAVING_WATER
	// Generate waves with Perlin-type noise.
	// The constants are calibrated such that they roughly
	// correspond to the old sine waves.
	float4 pos = float4(modelWorldPosition, 1.0f);
	float3 wavePos = output.worldPosition + cameraOffset;
	// The waves are slightly compressed along the z-axis to get
	// wave-fronts along the x-axis.
	wavePos.x /= WATER_WAVE_LENGTH * 3.0;
	wavePos.z /= WATER_WAVE_LENGTH * 2.0;
	wavePos.z += animationTimer * WATER_WAVE_SPEED * 10.0;
	pos.y += (snoise(wavePos) - 1.0) * WATER_WAVE_HEIGHT * 5.0;

#if GE_USE_MAT_VEC
	output.clipPosition = mul(pvwMatrix, pos);
#else
	output.clipPosition = mul(pos, pvwMatrix);
#endif

#elif MATERIAL_TYPE == TILE_MATERIAL_WAVING_LEAVES && ENABLE_WAVING_LEAVES
	float4 pos = float4(modelWorldPosition, 1.0f);
	pos.x += dispX;
	pos.y += dispZ * 0.1;
	pos.z += dispZ;

#if GE_USE_MAT_VEC
	output.clipPosition = mul(pvwMatrix, pos);
#else
	output.clipPosition = mul(pos, pvwMatrix);
#endif

#elif MATERIAL_TYPE == TILE_MATERIAL_WAVING_PLANTS && ENABLE_WAVING_PLANTS
	float4 pos = float4(modelWorldPosition, 1.0f);
	if (output.vertexTCoord.y < 0.05) 
	{
		pos.x += dispX;
		pos.z += dispZ;
	}

#if GE_USE_MAT_VEC
	output.clipPosition = mul(pvwMatrix, pos);
#else
	output.clipPosition = mul(pos, pvwMatrix);
#endif

#else

#if GE_USE_MAT_VEC
	output.clipPosition = mul(pvwMatrix, float4(modelWorldPosition, 1.0f));
#else
	output.clipPosition = mul(float4(modelWorldPosition, 1.0f), pvwMatrix);
#endif

#endif

	output.vertexPosition = output.clipPosition.xyz;

#if GE_USE_MAT_VEC
	output.eyeVec = -mul(vwMatrix, float4(modelWorldPosition, 1.0f)).xyz;
#else
	output.eyeVec = -mul(float4(modelWorldPosition, 1.0f), vwMatrix).xyz;
#endif

	// Calculate color.
	// Red, green and blue components are pre-multiplied with
	// the brightness, so now we have to multiply these
	// colors with the color of the incoming light.
	// The pre-baked colors are halved to prevent overflow.
	float4 color;
	// The alpha gives the ratio of sunlight in the incoming light.
	float nightRatio = 1.0 - input.modelColor.a;
	color.rgb = input.modelColor.rgb * (input.modelColor.a * dayLight.rgb +
		nightRatio * artificialLight.rgb) * 2.0;
	color.a = 1.0;

	// Emphase blue a bit in darker places
	// See C++ implementation in mapblock_mesh.cpp final_color_blend()
	float brightness = (color.r + color.g + color.b) / 3.0;
	color.b += max(0.0, 0.021 - abs(0.2 * brightness - 0.021) + 0.07 * brightness);

	output.vertexColor = clamp(color, 0.0, 1.0);
	return output;
}

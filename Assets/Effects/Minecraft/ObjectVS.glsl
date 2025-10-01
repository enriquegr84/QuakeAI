uniform WMatrix
{
	mat4 wMatrix;
};
uniform VWMatrix
{
    mat4 vwMatrix;
};
uniform PVMatrix
{
    mat4 pvMatrix;
};
uniform PVWMatrix
{
    mat4 pvwMatrix;
};

layout(location = 0) in vec3 modelPosition;
layout(location = 1) in vec2 modelTCoord;
layout(location = 2) in vec4 modelColor;
layout(location = 3) in vec3 modelNormal;

layout(location = 0) out vec3 vertexPosition;
layout(location = 1) out vec3 vertexNormal;
layout(location = 2) out vec3 worldPosition;
layout(location = 3) out vec4 vertexColor;
layout(location = 4) out vec2 vertexTCoord;
layout(location = 5) out vec3 eyeVec;
layout(location = 6) out float vIDiff;

const float e = 2.718281828459;
const float BS = 10.0;

float directionalAmbient(vec3 normal)
{
	vec3 v = normal * normal;

	if (normal.y < 0.0)
		return dot(v, vec3(0.670820, 0.447213, 0.836660));

	return dot(v, vec3(0.670820, 1.000000, 0.836660));
}

void main()
{
	vertexTCoord = modelTCoord.st;
#if GE_USE_MAT_VEC
	worldPosition = (wMatrix * vec4(modelPosition, 1.0f)).xyz;
#else
	worldPosition = (vec4(modelPosition, 1.0f) * wMatrix).xyz;
#endif

#if GE_USE_MAT_VEC
    gl_Position = pvMatrix * vec4(worldPosition, 1.0f);
#else
    gl_Position = vec4(worldPosition, 1.0f) * pvMatrix;
#endif

	vertexPosition = gl_Position.xyz;
	vertexNormal = modelNormal;

#if GE_USE_MAT_VEC
	eyeVec = -(vwMatrix * vec4(modelPosition, 1.0f)).xyz;
#else
	eyeVec = -(vec4(modelPosition, 1.0f), vwMatrix).xyz;
#endif


#if (MATERIAL_TYPE == TILE_MATERIAL_PLAIN) || (MATERIAL_TYPE == TILE_MATERIAL_PLAIN_ALPHA)
	vIDiff = 1.0;
#else
	// This is intentional comparison with zero without any margin.
	// If normal is not equal to zero exactly, then we assume it's a valid, just not normalized vector
	vIDiff = length(modelNormal) == 0.0 ? 
		1.0 : directionalAmbient(normalize(modelNormal));
#endif

	vertexColor = modelColor;
}

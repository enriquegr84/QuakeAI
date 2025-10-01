uniform PVWMatrix
{
    mat4 pvwMatrix;
};

layout(location = 0) in vec3 modelPosition;
layout(location = 1) in vec2 modelTCoord;
layout(location = 2) in vec4 modelColor;

layout(location = 0) out vec4 vertexColor;
layout(location = 1) out vec2 vertexTCoord;

void main()
{
	vertexTCoord = modelTCoord.st;
#if GE_USE_MAT_VEC
    gl_Position = pvwMatrix * vec4(modelPosition, 1.0f);
#else
    gl_Position = vec4(modelPosition, 1.0f) * pvwMatrix;
#endif

	vertexColor = modelColor;
}

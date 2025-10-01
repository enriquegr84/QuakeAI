uniform PVWMatrix
{
    mat4 pvwMatrix;
};

uniform StarColor
{
    vec4 starColor;
};

layout(location = 0) in vec3 modelPosition;

layout(location = 0) out vec4 vertexColor;

void main()
{
#if GE_USE_MAT_VEC
    gl_Position = pvwMatrix * vec4(modelPosition, 1.0f);
#else
    gl_Position = vec4(modelPosition, 1.0f) * pvwMatrix;
#endif
    vertexColor = starColor;
}

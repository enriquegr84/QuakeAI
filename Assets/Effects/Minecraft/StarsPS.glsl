layout(location = 0) in vec4 vertexColor;

layout(location = 0) out vec4 pixelColor;

void main()
{
	pixelColor = vertexColor;
}

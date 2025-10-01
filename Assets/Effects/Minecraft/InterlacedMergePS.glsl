uniform sampler2D baseSampler;
uniform sampler2D normalSampler;
uniform sampler2D flagSampler;

#define leftImage baseSampler
#define rightImage normalSampler
#define maskImage flagSampler

layout(location = 0) in vec2 vertexTCoord;

layout(location = 0) out vec4 pixelColor;

void main(void)
{
	vec4 left = texture(leftImage, vertexTCoord);
	vec4 right = texture(rightImage, vertexTCoord);
	vec4 mask = texture(maskImage, vertexTCoord);
	vec4 color;
	if (mask.r > 0.5)
		color = right;
	else
		color = left;
	pixelColor = color;
}

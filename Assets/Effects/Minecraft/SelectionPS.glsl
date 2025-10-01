uniform sampler2D baseSampler;

layout(location = 0) in vec4 vertexColor;
layout(location = 1) in vec2 vertexTCoord;

layout(location = 0) out vec4 pixelColor;

void main(void)
{
	vec2 uv = vertexTCoord.st;
	vec4 color = texture(baseSampler, uv);
	color.rgb *= vertexColor.rgb;
	pixelColor = color;
}

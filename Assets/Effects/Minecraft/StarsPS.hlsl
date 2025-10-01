struct PS_INPUT
{
	float4 vertexColor : COLOR0;
};

struct PS_OUTPUT
{
	float4 pixelColor0 : SV_TARGET0;
};

PS_OUTPUT PSMain(PS_INPUT input)
{
	PS_OUTPUT output;
	output.pixelColor0 = input.vertexColor;
	return output;
}

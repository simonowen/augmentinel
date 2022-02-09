struct VS_OUTPUT
{
	float4 pos : SV_POSITION;
	float4 colour : COLOR;
};

float4 main(VS_OUTPUT input) : SV_TARGET
{
	return float4(input.colour.xyz, 1.0f);
}

cbuffer cbPerObject : register(b0)
{
	float dissolved;	// 0.0f = fully visible, 1.0f = fully dissolved
	float noise;		// 0.0f <= noise < 1.0f
};

struct VS_OUTPUT
{
	float4 pos : SV_POSITION;	// unused
	float4 colour : COLOR0;
	float2 uv : TEXCOORD;
};

float rnd(float2 uv)
{
	// https://stackoverflow.com/questions/5149544/can-i-generate-a-random-number-inside-a-pixel-shader#answer-10625698
	return frac(cos(fmod(123456780.0f, 1024.0f * dot(uv, float2(23.14069263277926f, 2.6651441426902251f)))));
}

float4 main(VS_OUTPUT input) : SV_TARGET
{
	if (dissolved == 0.0f)
		return input.colour;

	float2 uv = frac(input.uv + float2(noise, noise));
	clip(rnd(uv) - dissolved);

	return input.colour;
}

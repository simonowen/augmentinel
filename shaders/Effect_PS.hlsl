Texture2D tex : register(t0);
sampler samp : register(s0);

cbuffer cbPerObject : register(b0)
{
	float dissolved;		// 0.0f = fully visible, 1.0f = fully dissolved
	float noise;			// 0.0f <= noise < 1.0f
	float view_dissolved;	// 0.0f = normal, 1.0f = dissolved
	float view_desaturate;	// 0.0f = colour, 1.0f = greysale
	float view_fade;		// 0.0f = normal, 1.0f = black
};

struct VS_OUTPUT
{
	float4 pos : SV_POSITION;	// unused
	float2 uv : TEXCOORD;
};

struct PS_OUTPUT
{
	float4 colour : SV_TARGET;
};

float rnd(float2 uv)
{
	// https://stackoverflow.com/questions/5149544/can-i-generate-a-random-number-inside-a-pixel-shader#answer-10625698
	return frac(cos(fmod(123456780.0f, 1024.0f * dot(uv, float2(23.14069263277926f, 2.6651441426902251f)))));
}

PS_OUTPUT main(VS_OUTPUT input)
{
	PS_OUTPUT output;

	if (view_dissolved)
	{
		// Discard pixel if random value determines it shouldn't be visible.
		float2 uv = frac(input.uv + float2(noise, noise));
		clip(rnd(uv) - view_dissolved);
	}

	// Sample pixel and calculate luminance.
	output.colour = tex.Sample(samp, input.uv);

	if (view_desaturate)
	{
		// Calculate luminance and apply saturation.
		float lum = dot(output.colour.rgb, float3(0.299, 0.587, 0.114));
		output.colour.rgb = lerp(output.colour.rgb, float3(lum, lum, lum), view_desaturate);
	}

	if (view_fade)
	{
		output.colour.rgb *= (1.0f - view_fade);
	}

	return output;
}

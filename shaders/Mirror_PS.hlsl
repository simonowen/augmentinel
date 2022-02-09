Texture2D tex : register(t0);
sampler samp : register(s0);

struct VS_OUTPUT
{
	float4 pos : SV_POSITION;	// unused
	float2 uv : TEXCOORD;
};

struct PS_OUTPUT
{
	float4 colour : SV_TARGET;
};

PS_OUTPUT main(VS_OUTPUT input)
{
	PS_OUTPUT output;
	output.colour = tex.Sample(samp, input.uv);
	return output;
}

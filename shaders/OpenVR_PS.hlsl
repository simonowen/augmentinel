SamplerState g_Sampler : register(s0);
Texture2D g_Texture : register(t0);

struct VS_OUTPUT
{
	float4 pos : SV_POSITION;
	float2 uv : TEXCOORD0;
};

float4 main(VS_OUTPUT input) : SV_TARGET
{
	return g_Texture.Sample(g_Sampler, input.uv);
}

cbuffer cbPerObject : register(b0)
{
	float4x4 WVP;
};

struct VS_INPUT
{
	float3 pos : POSITION;
	float3 normal: TEXCOORD0;
	float2 uv: TEXCOORD1;
};

struct VS_OUTPUT
{
	float4 pos : SV_POSITION;
	float2 uv : TEXCOORD0;
};

VS_OUTPUT main(VS_INPUT i)
{
	VS_OUTPUT output;
	output.pos = mul(WVP, float4(i.pos, 1.0));
	output.uv = i.uv;
	return output;
}

#include "SharedConstants.h"

#pragma pack_matrix(row_major)

cbuffer cbPerObject : register(b0)
{
	float4x4 WVP;
	float4x4 W;
	float4 Palette[PALETTE_SIZE];
	float3 EyePos;
	float z_fade;
	float fog_density;
	uint fog_colour_idx;
	uint lighting;
};

struct VS_INPUT
{
	float4 pos : POSITION;
	float3 normal : NORMAL;	// unused
	uint colour : COLOR;
	float2 uv : TEXCOORD;	// unused
};

struct VS_OUTPUT
{
	float4 pos : SV_POSITION;
	float4 colour : COLOR;
};

VS_OUTPUT main(VS_INPUT input)
{
	VS_OUTPUT output;
	output.pos = mul(float4(input.pos.xyz, 1.0f), WVP);
	output.colour = Palette[input.colour];
	return output;
}

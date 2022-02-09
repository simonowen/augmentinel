#include "SharedConstants.h"

#pragma pack_matrix(row_major)

#define ABIENT_INTENSITY	0.35f
#define BACK_FACE_INTENSITY	(ABIENT_INTENSITY / 2.0f)

#define LIGHT1_DIR			float3(1.0f, 0.5f, 0.5f)
#define LIGHT1_INTENSITY	0.8f

#define LIGHT2_DIR			float3(-1.0f, 0.5f, 0.5f)
#define LIGHT2_INTENSITY	0.2f

#define MAX_Z_FADE_DISTANCE	32.0f

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
	float3 normal : NORMAL;
	uint colour : COLOR;
	float2 uv : TEXCOORD;
};

struct VS_OUTPUT
{
	float4 pos : SV_POSITION;
	float4 colour : COLOR;
	float2 uv : TEXCOORD;
};

VS_OUTPUT main(VS_INPUT input)
{
	VS_OUTPUT output;
	output.pos = mul(float4(input.pos.xyz, 1.0f), WVP);
	output.uv = input.uv;

	float lightLevel = 1.0f;

	if (lighting)
	{
		// Transform the model normal into a world direction.
		float3 transformedNormal = mul(input.normal, (float3x3)W);

		// Determine of the vertex from the eye position.
		float3 vertexDir = mul(float4(input.pos.xyz, 1.0f), W).xyz - EyePos;

		// If the front face is visible we'll use normal lighting.
		if (dot(vertexDir, transformedNormal) < 0)
		{
			// Base ambient light level
			lightLevel = ABIENT_INTENSITY;

			// Light 1 from back right
			float lightIntensity = dot(transformedNormal, normalize(LIGHT1_DIR));
			if (lightIntensity > 0)
				lightLevel += lightIntensity * LIGHT1_INTENSITY;

			// Light 2 from front-left
			lightIntensity = dot(transformedNormal, normalize(LIGHT2_DIR));
			if (lightIntensity > 0)
				lightLevel += lightIntensity * LIGHT2_INTENSITY;
		}
		else
		{
			// Back faces (underneath map) get a fraction of ambient.
			lightLevel = BACK_FACE_INTENSITY;
		}
	}

	float4 face_colour = saturate(lightLevel) * Palette[input.colour];
	float fog_level = 1.0f / exp(length(output.pos.xyz) * fog_density);
	output.colour = lerp(Palette[fog_colour_idx], face_colour, fog_level);

	if (z_fade)
	{
		float z = mul(float4(input.pos.xyz, 1.0f), W).z;
		z = clamp(z, 0.0f, MAX_Z_FADE_DISTANCE);

		float fade = 1.0f / exp(z * z_fade);
		output.colour *= fade;
	}

	return output;
}

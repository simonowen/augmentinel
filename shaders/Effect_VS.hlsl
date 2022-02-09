struct VS_INPUT
{
	uint id : SV_VertexID;
};

struct VS_OUTPUT
{
	float4 pos : SV_POSITION;
	float2 uv : TEXCOORD;
};

VS_OUTPUT main(VS_INPUT input)
{
	VS_OUTPUT output;

	// Generate texture coords from vertex id.
	output.uv.x = float(input.id / 2);
	output.uv.y = float(input.id % 2);

	// Generate screen space coords from texture coords.
	output.pos.x = output.uv.x * 2.0f - 1.0f;
	output.pos.y = output.uv.y * 2.0f - 1.0f;
	output.pos.z = 0.0f;
	output.pos.w = 1.0f;

	// Flip y to correct orientation.
	output.uv.y = 1.0f - output.uv.y;

	return output;
}

#pragma once

struct Vertex
{
	Vertex(float x = 0.0f, float y = 0.0f, float z = 0.0f, uint32_t c = 0, float u = 0.0f, float v = 0.0f)
		: pos(x, y, z), colour(c), texcoord(u, v) {}

	XMFLOAT3 pos{};
	XMFLOAT3 normal{};
	UINT32 colour{};
	XMFLOAT2 texcoord{};
};

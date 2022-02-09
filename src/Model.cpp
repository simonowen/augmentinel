#include "stdafx.h"
#include "Model.h"

/*static*/ Model Model::CreateBlock(float width, float height, float depth, uint32_t colour_idx, ModelType type)
{
	auto w = width / 2;
	auto h = height / 2;
	auto d = depth / 2;

	std::vector<Vertex> vertices
	{
		{ -w, +h, +d, colour_idx, 0.0f, 1.0f },		// top back left
		{ +w, +h, +d, colour_idx, 1.0f, 1.0f },		// top back right
		{ -w, +h, -d, colour_idx, 0.0f, 0.0f },		// top front left
		{ +w, +h, -d, colour_idx, 1.0f, 0.0f },		// top front right

		{ -w, -h, -d, colour_idx, 0.0f, 1.0f },		// bottom front left
		{ +w, -h, -d, colour_idx, 1.0f, 1.0f },		// bottom front right
		{ -w, -h, +d, colour_idx, 0.0f, 0.0f },		// bottom back left
		{ +w, -h, +d, colour_idx, 1.0f, 0.0f },		// bottom back right
	};

	if (type == ModelType::PointerLine)
	{
		// Origin at end rather than centre.
		for (auto& v : vertices)
			v.pos.z += d;
	}

	std::vector<uint32_t> indices
	{
		0, 1, 2,  1, 3, 2,		// top face
		4, 5, 6,  5, 7, 6,		// bottom face
		0, 2, 6,  2, 4, 6,		// left face
		3, 1, 5,  1, 7, 5,		// right face
		2, 3, 4,  3, 5, 4,		// front face
		1, 0, 7,  0, 6, 7,		// back face
	};

	// Skyboxes invert the winding order so we can see through it from the outside.
	if (type == ModelType::SkyBox)
	{
		for (size_t i = 0; i < indices.size(); i += 3)
			std::swap(indices[i + 1], indices[i + 2]);
	}

	auto pVertices = std::make_shared<std::vector<Vertex>>(std::move(vertices));
	auto pIndices = std::make_shared<std::vector<uint32_t>>(std::move(indices));

	auto model = Model{ pVertices, pIndices, type };
	model.lighting = false;
	return model;
}

Model::Model(
	std::shared_ptr<std::vector<Vertex>>& pVertices,
	std::shared_ptr<std::vector<uint32_t>>& pIndices,
	ModelType type_,
	int id_)
{
	assert(pVertices->size() && pIndices->size());
	assert((pIndices->size() % 3) == 0);

	id = id_;
	type = type_;
	m_pVertices = pVertices;
	m_pIndices = pIndices;

	auto& indices = *m_pIndices;
	auto& vertices = *m_pVertices;

	// Calculate normals if they're missing.
	const auto& normal = vertices[0].normal;
	if (normal.x == 0.0f && normal.y == 0.0f && normal.z == 0.0f)
	{
		for (size_t i = 0; i < m_pIndices->size(); i += 3)
		{
			auto v1 = XMLoadFloat3(&vertices[indices[i + 0]].pos);
			auto v2 = XMLoadFloat3(&vertices[indices[i + 1]].pos);
			auto v3 = XMLoadFloat3(&vertices[indices[i + 2]].pos);
			auto n = XMVector3Normalize(XMVector3Cross(
				XMVectorSubtract(v2, v1), XMVectorSubtract(v3, v2)));

			XMStoreFloat3(&vertices[indices[i + 0]].normal, n);
			XMStoreFloat3(&vertices[indices[i + 1]].normal, n);
			XMStoreFloat3(&vertices[indices[i + 2]].normal, n);
		}
	}

	// Determine the bounding box to eliminate unnecessary triangle ray testing.
	BoundingBox::CreateFromPoints(
		m_boundingBox,
		vertices.size(),
		&vertices[0].pos,
		sizeof(vertices[0]));
}

Model::operator bool() const
{
	return type != ModelType::Unknown;
}

XMMATRIX Model::GetWorldMatrix(const Model& rel) const
{
	auto mRotation = XMMatrixRotationRollPitchYaw(rot.x, rot.y, rot.z);
	auto mScale = XMMatrixScaling(scale, scale, scale);
	auto mTranslation = XMMatrixTranslation(pos.x, pos.y, pos.z);
	auto mWorld = mRotation * mScale * mTranslation;

	// Is the model moving relative to another model?
	if (rel)
	{
		auto mRelRotation = XMMatrixRotationRollPitchYaw(rel.rot.x, rel.rot.y, rel.rot.z);
		auto mRelScale = XMMatrixScaling(rel.scale, rel.scale, rel.scale);
		auto mRelTranslation = XMMatrixTranslation(rel.pos.x, rel.pos.y, rel.pos.z);
		auto mRelInvTranslation = XMMatrixInverse(nullptr, mRelTranslation);

		return mWorld * mRelInvTranslation * mRelRotation * mRelScale * mRelTranslation;
	}

	return mWorld;
}

std::vector<XMVECTOR> Model::GetBoundingBox() const
{
	static constexpr auto CUBOID_VERTICES = 8;

	std::array <XMFLOAT3, CUBOID_VERTICES> model_corners;
	m_boundingBox.GetCorners(model_corners.data());

	std::vector<XMVECTOR> world_corners;
	world_corners.reserve(CUBOID_VERTICES);

	auto mWorld = GetWorldMatrix();
	for (auto& corner : model_corners)
	{
		XMVECTOR vCorner{ corner.x, corner.y, corner.z, 1.0f };
		world_corners.emplace_back(XMVector4Transform(vCorner, mWorld));
	}

	return world_corners;
}

bool Model::BoxTest(XMVECTOR vRayOrigin, XMVECTOR vRayDir, float& dist) const
{
	// Convert ray to model coordinates using the inverted model matrix.
	auto mInvWorld = XMMatrixInverse(nullptr, GetWorldMatrix());
	vRayOrigin = XMVector4Transform(vRayOrigin, mInvWorld);
	vRayDir = XMVector4Transform(vRayDir, mInvWorld);

	// Test the ray against the model's bounding box.
	return m_boundingBox.Intersects(vRayOrigin, vRayDir, dist);
}

bool Model::RayTest(XMVECTOR vRayOrigin, XMVECTOR vRayDir, RayTarget& hit) const
{
	auto dist = 0.0f;
	auto closest_dist = std::numeric_limits<float>::max();
	auto closest_idx = std::numeric_limits<size_t>::max();

	// Convert ray to model coordinates using the inverted model matrix.
	auto mInvWorld = XMMatrixInverse(nullptr, GetWorldMatrix());
	vRayOrigin = XMVector4Transform(vRayOrigin, mInvWorld);
	vRayDir = XMVector4Normalize(XMVector4Transform(vRayDir, mInvWorld));

	// Early rejection if ray doesn't touch bounding box.
	if (!m_boundingBox.Intersects(vRayOrigin, vRayDir, dist))
		return false;

	auto& indices = *m_pIndices;
	auto& vertices = *m_pVertices;

	for (size_t idx = 0; idx < m_pIndices->size(); idx += 3)
	{
		auto& pos0 = vertices[indices[idx + 0]].pos;
		auto& pos1 = vertices[indices[idx + 1]].pos;
		auto& pos2 = vertices[indices[idx + 2]].pos;

		auto V0 = XMVectorSet(pos0.x, pos0.y, pos0.z, 1.0f);
		auto V1 = XMVectorSet(pos1.x, pos1.y, pos1.z, 1.0f);
		auto V2 = XMVectorSet(pos2.x, pos2.y, pos2.z, 1.0f);

		if (TriangleTests::Intersects(vRayOrigin, vRayDir, V0, V1, V2, dist))
		{
			if (dist < closest_dist)
			{
				closest_dist = dist;
				closest_idx = idx;
			}
		}
	}

	if (closest_idx < m_pIndices->size())
	{
		hit.model = this;
		hit.distance = closest_dist;
		hit.index = closest_idx;
		return true;
	}

	return false;
}

std::vector<XMFLOAT3> Model::GetTileVertices(int x, int z) const
{
	std::vector<XMFLOAT3> tile_vertices(ZX_VERTICES_PER_TILE);

	auto& indices = *m_pIndices;
	auto& vertices = *m_pVertices;

	// This function expects a landscape with 6 indices per tile.
	assert(type == ModelType::Landscape);
	assert(indices.size() == (SENTINEL_MAP_SIZE - 1) * (SENTINEL_MAP_SIZE - 1) * ZX_VERTICES_PER_TILE);

	auto index_base = static_cast<int>(((z * (SENTINEL_MAP_SIZE - 1)) + x) * ZX_VERTICES_PER_TILE);
	for (auto& v : tile_vertices)
		v = vertices[indices[index_base++]].pos;

	return tile_vertices;
}

std::vector<XMVECTOR> Model::GetTileCorners(int x, int z) const
{
	std::set<std::pair<float, float>> seen;
	std::vector<XMVECTOR> world_corners;
	world_corners.reserve(4);

	auto mWorld = GetWorldMatrix();
	for (const auto& v : GetTileVertices(x, z))
	{
		auto key = std::make_pair(v.x, v.z);
		if (seen.find(key) == seen.end())
		{
			XMVECTOR vCorner{ v.x, v.y, v.z, 1.0f };
			world_corners.emplace_back(XMVector4Transform(vCorner, mWorld));
			seen.insert(key);
		}
	}

	assert(world_corners.size() == 4);
	return world_corners;
}

std::vector<Vertex>& Model::EditVertices()
{
	m_pHeapVertices.reset();
	return *m_pVertices;
}

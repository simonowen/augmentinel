#pragma once
#include "Vertex.h"
#include "BufferHeap.h"

enum class ModelType : uint8_t
{
	Robot, Sentry, Tree, Boulder, Meanie, Sentinel, Pedestal, Block1, Block2, Block3,
	Landscape, SkyBox, Letter, Icon, PointerLine, PointerTarget, Unknown
};

class Model;

struct RayTarget
{
	const Model* model{ nullptr };
	float distance{ 0.0f };
	size_t index{ 0 };
};

class Model
{
public:
	static Model CreateBlock(float width, float height, float depth, uint32_t colour_idx, ModelType type);

	Model() = default;
	Model(
		std::shared_ptr<std::vector<Vertex>>& pVertices,
		std::shared_ptr<std::vector<uint32_t>>& pIndices,
		ModelType type = ModelType::Unknown,
		int id = -1);
	virtual ~Model() = default;

	operator bool() const;

	XMMATRIX GetWorldMatrix(const Model& linkedModel = {}) const;
	std::vector<XMVECTOR> GetBoundingBox() const;
	std::vector<XMFLOAT3> GetTileVertices(int x, int z) const;
	std::vector<XMVECTOR> GetTileCorners(int x, int z) const;
	bool RayTest(XMVECTOR vRayOrigin, XMVECTOR vRayDir, RayTarget& hit) const;
	bool BoxTest(XMVECTOR vRayOrigin, XMVECTOR vRayDir, float& dist) const;
	std::vector<Vertex>& EditVertices();

	int id{ -1 };
	ModelType type{ ModelType::Unknown };
	XMFLOAT3 pos{};
	XMFLOAT3 rot{};
	float scale{ 1.0f };
	float dissolved{ 0.0f };
	bool lighting{ true };
	bool orthographic{ false };

	std::shared_ptr<std::vector<Vertex>> m_pVertices;
	std::shared_ptr<std::vector<uint32_t>> m_pIndices;
	std::shared_ptr<D3D11HeapAllocation> m_pHeapVertices;
	std::shared_ptr<D3D11HeapAllocation> m_pHeapIndices;
	BoundingBox m_boundingBox;

	ComPtr<ID3D11VertexShader> m_pVertexShader;
	ComPtr<ID3D11PixelShader> m_pPixelShader;
};


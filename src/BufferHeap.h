#pragma once
#include "SimpleHeap.h"

class D3D11HeapAllocation
{
public:
	D3D11HeapAllocation(UINT start, UINT count, UINT stride, ID3D11Buffer* pBuffer)
		: start_index(start), count(count), stride(stride), m_pBuffer(pBuffer) { }
	virtual ~D3D11HeapAllocation() = default;

	void update_ptr(ID3D11DeviceContext* pDeviceContext, const void* pItems)
	{
		D3D11_BOX box{};
		box.left = start_index * stride;
		box.right = box.left + (count * stride);
		box.back = box.bottom = 1;
		pDeviceContext->UpdateSubresource(m_pBuffer.Get(), 0, &box, pItems, 0, 0);
	}

	template <typename C>
	void update(ID3D11DeviceContext* pDeviceContext, const C& items)
	{
		update_ptr(pDeviceContext, items.data());
	}

	UINT start_index{ 0 };
	UINT stride{ 0 };
	UINT count{ 0 };
	ComPtr<ID3D11Buffer> m_pBuffer;
};

class ManagedD3D11HeapAllocation : public D3D11HeapAllocation
{
public:
	ManagedD3D11HeapAllocation(UINT start, UINT count, UINT stride, ID3D11Buffer* pBuffer, std::shared_ptr<SimpleHeap> pHeap)
		: D3D11HeapAllocation(start, count, stride, pBuffer), m_pHeap(pHeap) { }
	~ManagedD3D11HeapAllocation()
	{
		m_pHeap->free(start_index);
	}

protected:
	std::shared_ptr<SimpleHeap> m_pHeap;
};

template <typename T, UINT BindFlags>
class D3D11BufferHeap
{
public:
	D3D11BufferHeap(ID3D11Device* pDevice, int max_items)
		: m_pDevice(pDevice)
	{
		D3D11_BUFFER_DESC bufferDesc{};
		bufferDesc.Usage = D3D11_USAGE_DEFAULT;
		bufferDesc.ByteWidth = static_cast<DWORD>(sizeof(T) * max_items);
		bufferDesc.BindFlags = BindFlags;

		auto hr = pDevice->CreateBuffer(&bufferDesc, nullptr, m_pBuffer.GetAddressOf());
		if (FAILED(hr))
			throw std::exception("CreateBuffer (D3D11BufferHeap)");

		m_pHeap = std::make_shared<SimpleHeap>(max_items);
	}

	auto alloc_ptr(ID3D11DeviceContext* pDeviceContext, const T* pItems, int num_items)
	{
		auto start_index = m_pHeap->alloc(num_items);
		if (start_index < 0)
			throw std::exception("out of D3D11BufferHeap space");

		auto count = static_cast<UINT>(num_items);
		auto stride = static_cast<UINT>(sizeof(T));

		auto new_alloc = std::make_shared<ManagedD3D11HeapAllocation>(
			start_index, count, stride, m_pBuffer.Get(), m_pHeap);
		new_alloc->update_ptr(pDeviceContext, pItems);
		return new_alloc;
	}

	template <typename C>
	auto alloc(ID3D11DeviceContext* pDeviceContext, const C& items)
	{
		return alloc_ptr(pDeviceContext, items.data(), static_cast<int>(items.size()));
	}

protected:
	ComPtr<ID3D11Device> m_pDevice;
	ComPtr<ID3D11Buffer> m_pBuffer;
	std::shared_ptr<SimpleHeap> m_pHeap;
};

template <typename VertexType>
using D3D11VertexHeap = D3D11BufferHeap<VertexType, D3D11_BIND_VERTEX_BUFFER>;

template <typename IndexType>
using D3D11IndexHeap = D3D11BufferHeap<IndexType, D3D11_BIND_INDEX_BUFFER>;

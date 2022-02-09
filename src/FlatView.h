#pragma once
#include "View.h"

class FlatView final : public View
{
public:
	FlatView(HWND hwnd);

	XMVECTOR GetEyePositionVector() const override;
	XMVECTOR GetViewPositionVector() const override;
	XMVECTOR GetViewDirectionVector() const override;
	XMVECTOR GetViewUpVector() const override;
	XMMATRIX GetViewProjectionMatrix() const override;
	XMMATRIX GetOrthographicMatrix() const override;

	bool IsPointerVisible() const override;
	void GetSelectionRay(XMVECTOR& vPos, XMVECTOR& vDir) const override;
	void SetVerticalFOV(float fov) override;
	void OnResize(uint32_t rt_width, uint32_t rt_height) override;
	void Render(IGame* pRender) override;
	void EndScene() override;

	// Ensure 16-byte alignment for XMMATRIX below.
	void* operator new(size_t i) { return _mm_malloc(i, 16); }
	void operator delete(void* p) { _mm_free(p); }

protected:
	HWND m_hwnd{ NULL };
	float m_aspect_ratio{ 1.0f };
	float m_vertical_fov{ SENTINEL_VERT_FOV };

	XMMATRIX m_mProjection{};
	XMMATRIX m_mOrthographic{};
};

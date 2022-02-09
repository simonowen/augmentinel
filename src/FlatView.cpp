#include "stdafx.h"
#include "FlatView.h"

FlatView::FlatView(HWND hwnd) :
	m_hwnd(hwnd)
{
	RECT rClient{};
	GetClientRect(hwnd, &rClient);
	Init(hwnd, rClient.right, rClient.bottom);
}

XMVECTOR FlatView::GetEyePositionVector() const
{
	return m_camera.GetPositionVector();
}

XMVECTOR FlatView::GetViewPositionVector() const
{
	return m_camera.GetPositionVector();
}

XMVECTOR FlatView::GetViewDirectionVector() const
{
	return m_camera.GetDirectionVector();
}

XMVECTOR FlatView::GetViewUpVector() const
{
	return m_camera.GetUpVector();
}

XMMATRIX FlatView::GetViewProjectionMatrix() const
{
	return m_camera.GetViewMatrix() * m_mProjection;
}

XMMATRIX FlatView::GetOrthographicMatrix() const
{
	return m_mOrthographic;
}

bool FlatView::IsPointerVisible() const
{
	return false;
}

void FlatView::GetSelectionRay(XMVECTOR& vPos, XMVECTOR& vDir) const
{
	vPos = m_camera.GetPositionVector();
	vDir = m_camera.GetDirectionVector();
}

void FlatView::SetVerticalFOV(float fov)
{
	m_vertical_fov = fov;

	//auto vert_fov_radians = 2 * std::atan(std::tan(XMConvertToRadians(m_horiz_fov_degrees) / 2) / m_aspect_ratio);
	m_mProjection = XMMatrixPerspectiveFovLH(
		XMConvertToRadians(m_vertical_fov), m_aspect_ratio, NEAR_CLIP, FAR_CLIP);
}

void FlatView::OnResize(uint32_t rt_width, uint32_t rt_height)
{
	View::OnResize(rt_width, rt_height);

	m_aspect_ratio = static_cast<float>(rt_width) / rt_height;
	m_mOrthographic = XMMatrixOrthographicOffCenterLH(
		0.0f, 1000.0f * m_aspect_ratio, 0.0f, 1000.0f, NEAR_CLIP, FAR_CLIP);

	SetVerticalFOV(m_vertical_fov);
}

void FlatView::Render(IGame* pGame)
{
	auto& fill_colour = m_vertexConstants.Palette[m_fill_colour_idx];

	// Is MSAA enabled?
	if (m_msaa_samples > 1)
	{
		// Render to the MSAA texture.
		m_pDeviceContext->OMSetRenderTargets(1, m_pMsaaRenderTargetView.GetAddressOf(), m_pMsaaDepthStencilView.Get());

		// Clear back buffer, MSAA texture, and MSAA depth buffer.
		m_pDeviceContext->ClearRenderTargetView(m_pRenderTargetView.Get(), &fill_colour.x);
		m_pDeviceContext->ClearRenderTargetView(m_pMsaaRenderTargetView.Get(), &fill_colour.x);
		m_pDeviceContext->ClearDepthStencilView(m_pMsaaDepthStencilView.Get(), D3D11_CLEAR_DEPTH, 1.0f, 0);
	}
	else if (PixelShaderEffectsActive())
	{
		// Render to the effects texture (after unbinding the effects SRV).
		m_pStateTracker->SetPixelResource(nullptr);
		m_pDeviceContext->OMSetRenderTargets(1, m_pEffectRenderTargetView.GetAddressOf(), m_pMsaaDepthStencilView.Get());

		// Clear back buffer, effects texture, and depth buffer.
		m_pDeviceContext->ClearRenderTargetView(m_pRenderTargetView.Get(), &fill_colour.x);
		m_pDeviceContext->ClearRenderTargetView(m_pEffectRenderTargetView.Get(), &fill_colour.x);
		m_pDeviceContext->ClearDepthStencilView(m_pMsaaDepthStencilView.Get(), D3D11_CLEAR_DEPTH, 1.0f, 0);
	}
	else
	{
		// Render directly to the swapchain back buffer.
		m_pDeviceContext->OMSetRenderTargets(1, m_pRenderTargetView.GetAddressOf(), m_pMsaaDepthStencilView.Get());

		// Clear swap chain back buffer and depth buffer.
		m_pDeviceContext->ClearRenderTargetView(m_pRenderTargetView.Get(), &fill_colour.x);
		m_pDeviceContext->ClearDepthStencilView(m_pMsaaDepthStencilView.Get(), D3D11_CLEAR_DEPTH, 1.0f, 0);
	}

	pGame->Render(this);

	// MSAA enabled?
	if (m_msaa_samples > 1)
	{
		if (PixelShaderEffectsActive())
		{
			// Resolve MSAA to the effects texture for post-processing.
			m_pDeviceContext->ResolveSubresource(
				m_pEffectTexture.Get(), 0,
				m_pMsaaTexture.Get(), 0,
				DXGI_FORMAT_R8G8B8A8_UNORM);
		}
		else
		{
			ComPtr<ID3D11Resource> pBackBuffer;
			m_pRenderTargetView->GetResource(pBackBuffer.GetAddressOf());

			// Resolve MSAA directly to the swap chain back buffer.
			m_pDeviceContext->ResolveSubresource(
				pBackBuffer.Get(), 0,
				m_pMsaaTexture.Get(), 0,
				DXGI_FORMAT_R8G8B8A8_UNORM);
		}
	}

	// Perform any remaining rendering to the swap chain back buffer.
	m_pDeviceContext->OMSetRenderTargets(1, m_pRenderTargetView.GetAddressOf(), nullptr);

	if (PixelShaderEffectsActive())
	{
		// Bind the effects texture SRV.
		m_pStateTracker->SetPixelResource(m_pEffectShaderResourceView.Get());

		// Render the scene texture with added effects.
		m_pStateTracker->SetVertexShader(m_pEffectVertexShader.Get());
		m_pStateTracker->SetPixelShader(m_pEffectPixelShader.Get());
		m_pStateTracker->SetSamplerState(m_pEffectSamplerState.Get());
		m_pStateTracker->SetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
		m_pDeviceContext->Draw(4, 0);
	}
}

void FlatView::EndScene()
{
	m_pSwapChain->Present(1, 0);
}

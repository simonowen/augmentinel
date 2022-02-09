#include "stdafx.h"
#include "VRView.h"
#include "Settings.h"

#include "OpenVR_VS.h"
#include "OpenVR_PS.h"
#include "Mirror_VS.h"
#include "Mirror_PS.h"

static constexpr auto MIN_HEIGHT_METRES = 0.50f;
static constexpr auto MAX_HEIGHT_METRES = 2.50f;
static constexpr int MAX_HEAP_VERTICES = 262'144;
static constexpr int MAX_HEAP_INDICES = 524'288;

VRView::VRView(HWND hwnd)
{
	m_seated = GetFlag(SEATED_VR_KEY, DEFAULT_SEATED_VR);
	m_height_offset = m_seated ? 0.0f : -EYE_HEIGHT;

	m_pOpenVR = std::make_unique<OpenVR>(m_seated, NEAR_CLIP, FAR_CLIP);
	Init(hwnd);

	CreateControllers();

	// To keep things simple there's only one action set.
	m_hActionSet = m_pOpenVR->GetActionSetHandle("/actions/game");

	// Should we try to use the left controller for the pointer?
	m_hmd_pointer = GetFlag(HMD_POINTER_KEY, DEFAULT_HMD_POINTER);
	m_left_handed = GetFlag(LEFT_HANDED_KEY, DEFAULT_LEFT_HANDED);
}

void VRView::Init(HWND hwndPreview)
{
	uint32_t rt_width{}, rt_height{};
	m_pOpenVR->GetRenderTargetSize(rt_width, rt_height);

	View::Init(hwndPreview, rt_width, rt_height);

	auto eyeTextureDesc = CD3D11_TEXTURE2D_DESC{
		DXGI_FORMAT_R8G8B8A8_UNORM,
		rt_width,
		rt_height,
		1,
		1,
		D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE };

	auto hr = m_pDevice->CreateTexture2D(&eyeTextureDesc, nullptr, m_pLeftEyeTexture.GetAddressOf());
	if (FAILED(hr))
		Fail(hr, L"CreateTexture2D");
	hr = m_pDevice->CreateTexture2D(&eyeTextureDesc, nullptr, m_pRightEyeTexture.GetAddressOf());
	if (FAILED(hr))
		Fail(hr, L"CreateTexture2D");

	D3D11_RENDER_TARGET_VIEW_DESC eyeRTVDesc{};
	eyeRTVDesc.Format = eyeTextureDesc.Format;
	eyeRTVDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;

	hr = m_pDevice->CreateRenderTargetView(m_pLeftEyeTexture.Get(), &eyeRTVDesc, m_pLeftEyeRTV.GetAddressOf());
	if (FAILED(hr))
		Fail(hr, L"CreateRenderTargetView");
	hr = m_pDevice->CreateRenderTargetView(m_pRightEyeTexture.Get(), &eyeRTVDesc, m_pRightEyeRTV.GetAddressOf());
	if (FAILED(hr))
		Fail(hr, L"CreateRenderTargetView");

	D3D11_SHADER_RESOURCE_VIEW_DESC eyeSRVDesc{};
	eyeSRVDesc.Format = eyeTextureDesc.Format;
	eyeSRVDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
	eyeSRVDesc.Texture2D.MipLevels = 1;

	hr = m_pDevice->CreateShaderResourceView(m_pLeftEyeTexture.Get(), &eyeSRVDesc, m_pLeftEyeSRV.GetAddressOf());
	if (FAILED(hr))
		Fail(hr, L"CreateShaderResourceView");
	hr = m_pDevice->CreateShaderResourceView(m_pRightEyeTexture.Get(), &eyeSRVDesc, m_pRightEyeSRV.GetAddressOf());
	if (FAILED(hr))
		Fail(hr, L"CreateShaderResourceView");


	auto depthStencilDesc = CD3D11_TEXTURE2D_DESC{
		DXGI_FORMAT_D32_FLOAT,
		rt_width,
		rt_height,
		1,
		1,
		D3D11_BIND_DEPTH_STENCIL };

	hr = m_pDevice->CreateTexture2D(&depthStencilDesc, NULL, m_pEyeDepthStencilBuffer.GetAddressOf());
	if (FAILED(hr))
		Fail(hr, L"CreateTexture2D (depth)");

	hr = m_pDevice->CreateDepthStencilView(m_pEyeDepthStencilBuffer.Get(), NULL, m_pEyeDepthStencilView.GetAddressOf());
	if (FAILED(hr))
		Fail(hr, L"CreateDepthStencilView");
}

bool VRView::IsVR() const
{
	return true;
}

bool VRView::IsSuspended() const
{
	return m_pOpenVR->IsDashboardActive();
}

UINT VRView::GetDXGIAdapterIndex() const
{
	return m_pOpenVR->GetDXGIAdapterIndex();
}

XMMATRIX VRView::GetEyeViewMatrix(bool right_eye) const
{
	auto mHMD = XMMatrixInverse(nullptr, m_pOpenVR->GetViewMatrix(right_eye));
	auto mMirrorZ = XMMatrixReflect({ 0.0f, 0.0f, 1.0f, 0.0f });
	auto mInvScale = XMMatrixScaling(1.0f / m_world_scale, 1.0f / m_world_scale, 1.0f / m_world_scale);
	auto mInvHeight = XMMatrixTranslation(0.0f, m_height_offset, 0.0f);
	auto mInvEyeView = XMMatrixInverse(nullptr, m_camera.GetViewMatrix());

	return mHMD * mMirrorZ * mInvScale * mInvHeight * mInvEyeView;
}

XMMATRIX VRView::GetHMDViewMatrix() const
{
	auto mHMD = XMMatrixInverse(nullptr, m_pOpenVR->GetHMDMatrix());
	auto mMirrorZ = XMMatrixReflect({ 0.0f, 0.0f, 1.0f, 0.0f });
	auto mInvScale = XMMatrixScaling(1.0f / m_world_scale, 1.0f / m_world_scale, 1.0f / m_world_scale);
	auto mInvHeight = XMMatrixTranslation(0.0f, m_height_offset, 0.0f);
	auto mInvEyeView = XMMatrixInverse(nullptr, m_camera.GetViewMatrix());

	return mHMD * mMirrorZ * mInvScale * mInvHeight * mInvEyeView;
}

XMMATRIX VRView::GetControllerMatrix(bool left_hand) const
{
	auto campos = m_camera.GetPosition();
	auto camrot = m_camera.GetRotations();

	auto mController = m_pOpenVR->GetControllerMatrix(left_hand);
	auto mMirrorZ = XMMatrixReflect({ 0.0f, 0.0f, 1.0f, 0.0f });
	auto mRotation = XMMatrixRotationRollPitchYaw(camrot.x, camrot.y, camrot.z);
	auto mInvScale = XMMatrixScaling(1.0f / m_world_scale, 1.0f / m_world_scale, 1.0f / m_world_scale);
	auto mFloor = XMMatrixTranslation(campos.x, campos.y + m_height_offset, campos.z);

	return mController * mMirrorZ * mRotation * mInvScale * mFloor;
}

XMMATRIX VRView::GetPointerMatrix(bool left_hand) const
{
	auto campos = m_camera.GetPosition();
	auto camrot = m_camera.GetRotations();

	auto action = left_hand ? Action::Pose_LeftPointer : Action::Pose_RightPointer;
	auto hAction = m_controller_bindings.at(action);

	auto mHMD = XMMatrixInverse(nullptr, m_pOpenVR->GetHMDMatrix());
	auto mController = m_pOpenVR->GetActionPose(hAction);
	auto mPointer = (m_hmd_pointer || XMMatrixIsIdentity(mController)) ? mHMD : mController;

	auto mMirrorZ = XMMatrixReflect({ 0.0f, 0.0f, 1.0f, 0.0f });
	auto mRotation = XMMatrixRotationRollPitchYaw(camrot.x, camrot.y, camrot.z);
	auto mInvScale = XMMatrixScaling(1.0f / m_world_scale, 1.0f / m_world_scale, 1.0f / m_world_scale);
	auto mFloor = XMMatrixTranslation(campos.x, campos.y + m_height_offset, campos.z);

	return mPointer * mMirrorZ * mRotation * mInvScale * mFloor;
}

XMVECTOR VRView::GetEyePositionVector() const
{
	return XMVector4Transform({ 0.0f, 0.0f, 0.0f, 1.0f }, GetEyeViewMatrix(m_right_eye));
}

XMVECTOR VRView::GetViewPositionVector() const
{
	return XMVector4Transform({ 0.0f, 0.0f, 0.0f, 1.0f }, GetHMDViewMatrix());
}

XMVECTOR VRView::GetViewDirectionVector() const
{
	auto vDir = XMVector4Transform({ 0.0f, 0.0f, 1.0f, 0.0f }, GetHMDViewMatrix());
	return XMVectorNegate(XMVector4Normalize(vDir));
}

XMVECTOR VRView::GetViewUpVector() const
{
	auto vUp = XMVector4Transform({ 0.0f, 1.0f, 0.0f, 0.0f }, GetHMDViewMatrix());
	return XMVector4Normalize(vUp);
}

XMMATRIX VRView::GetViewProjectionMatrix() const
{
	auto camrot = m_camera.GetRotations();

	auto mRotation = XMMatrixRotationRollPitchYaw(camrot.x, camrot.y, camrot.z);
	auto vDirection = XMVector4Transform({ 0.0f, 0.0f, 1.0f, 0.0f }, mRotation);

	auto pos = m_camera.GetPosition();
	auto mView = XMMatrixLookToLH({ pos.x, pos.y + m_height_offset, pos.z }, vDirection, { 0.0f, 1.0f, 0.0f, 0.0f });

	auto mMirrorZ = XMMatrixReflect({ 0.0f, 0.0f, 1.0f, 0.0f });
	auto mScale = XMMatrixScaling(m_world_scale, m_world_scale, m_world_scale);

	auto mHMD = m_pOpenVR->GetViewMatrix(m_right_eye);
	auto mProjection = m_pOpenVR->GetProjectionMatrix(m_right_eye);

	return mView * mMirrorZ * mScale * mHMD * mProjection;
}

XMMATRIX VRView::GetOrthographicMatrix() const
{
	return XMMatrixIdentity();
}

bool VRView::IsPointerVisible() const
{
	return !m_pOpenVR->IsDashboardActive() && m_pOpenVR->IsControllerDetected() && !m_hmd_pointer;
}

void VRView::GetSelectionRay(XMVECTOR& vPos, XMVECTOR& vDir) const
{
	auto mController = GetPointerMatrix(m_left_handed);
	vPos = XMVector4Transform({ 0.0f, 0.0f, 0.0f, 1.0f }, mController);
	vDir = XMVector4Transform({ 0.0f, 0.0f, 1.0f, 0.0f }, mController);
	vDir = XMVectorNegate(XMVector4Normalize(vDir));
}

void VRView::SetCameraRotation(XMFLOAT3 rot)
{
	rot.x = rot.z = 0.0f;
	View::SetCameraRotation(rot);
}

void VRView::MouseMove(int x, int y)
{
	UNREFERENCED_PARAMETER(y);
	return View::MouseMove(x, 0);
}

void VRView::RenderEye(IGame* pGame, ID3D11Resource* pTexture, ID3D11RenderTargetView* pRTV)
{
	View::BeginScene();

	auto& fill_colour = m_vertexConstants.Palette[m_fill_colour_idx];

	// Is MSAA enabled?
	if (m_msaa_samples > 1)
	{
		// Render to the MSAA texture.
		m_pDeviceContext->OMSetRenderTargets(1, m_pMsaaRenderTargetView.GetAddressOf(), m_pMsaaDepthStencilView.Get());

		// Clear eye texture, MSAA texture, and MSAA depth buffer.
		m_pDeviceContext->ClearRenderTargetView(pRTV, &fill_colour.x);
		m_pDeviceContext->ClearRenderTargetView(m_pMsaaRenderTargetView.Get(), &fill_colour.x);
		m_pDeviceContext->ClearDepthStencilView(m_pMsaaDepthStencilView.Get(), D3D11_CLEAR_DEPTH, 1.0f, 0);
	}
	else if (PixelShaderEffectsActive())
	{
		// Render to the effects texture (after unbinding the effects SRV).
		m_pStateTracker->SetPixelResource(nullptr);
		m_pDeviceContext->OMSetRenderTargets(1, m_pEffectRenderTargetView.GetAddressOf(), m_pMsaaDepthStencilView.Get());

		// Clear back buffer, effects texture, and depth buffer.
		m_pDeviceContext->ClearRenderTargetView(m_pEffectRenderTargetView.Get(), &fill_colour.x);
		m_pDeviceContext->ClearRenderTargetView(pRTV, &fill_colour.x);
		m_pDeviceContext->ClearDepthStencilView(m_pMsaaDepthStencilView.Get(), D3D11_CLEAR_DEPTH, 1.0f, 0);
	}
	else
	{
		// Render directly to the swapchain back buffer.
		m_pDeviceContext->OMSetRenderTargets(1, &pRTV, m_pMsaaDepthStencilView.Get());

		// Clear swap chain back buffer and depth buffer.
		m_pDeviceContext->ClearRenderTargetView(pRTV, &fill_colour.x);
		m_pDeviceContext->ClearDepthStencilView(m_pMsaaDepthStencilView.Get(), D3D11_CLEAR_DEPTH, 1.0f, 0);
	}

	// Ask the game to render the current frame.
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
			// Resolve MSAA directly to the eye texture.
			m_pDeviceContext->ResolveSubresource(
				pTexture, 0,
				m_pMsaaTexture.Get(), 0,
				DXGI_FORMAT_R8G8B8A8_UNORM);
		}
	}

	// Perform any remaining rendering to the eye texture, without a depth buffer.
	m_pDeviceContext->OMSetRenderTargets(1, &pRTV, nullptr);

	// If pixel shader effects are active, apply them to the eye texture.
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

void VRView::Render(IGame* pGame)
{
	m_pOpenVR->ProcessEvents(m_hActionSet);
	m_pOpenVR->UpdatePoses();

	// Right eye.
	m_right_eye = true;
	RenderEye(pGame, m_pRightEyeTexture.Get(), m_pRightEyeRTV.Get());

	// Left eye.
	m_right_eye = false;
	RenderEye(pGame, m_pLeftEyeTexture.Get(), m_pLeftEyeRTV.Get());

	// Unbind the render target so it can be used as a source. (still needed?)
	ID3D11RenderTargetView* null_rtv = nullptr;
	m_pDeviceContext->OMSetRenderTargets(1, &null_rtv, nullptr);
}

void VRView::EndScene()
{
	m_pOpenVR->Present(m_pLeftEyeTexture.Get(), m_pRightEyeTexture.Get());

	auto& fill_colour = m_vertexConstants.Palette[m_fill_colour_idx];
	m_pDeviceContext->OMSetRenderTargets(1, m_pRenderTargetView.GetAddressOf(), nullptr);
	m_pDeviceContext->ClearRenderTargetView(m_pRenderTargetView.Get(), &fill_colour.x);

	// Draw the left eye texture into the main window.
	m_pStateTracker->SetPixelResource(m_pLeftEyeSRV.Get());
	m_pStateTracker->SetVertexShader(m_pMirrorVertexShader.Get());
	m_pStateTracker->SetPixelShader(m_pMirrorPixelShader.Get());
	m_pStateTracker->SetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
	m_pDeviceContext->Draw(4, 0);
	m_pStateTracker->SetPixelResource(nullptr);
	m_pSwapChain->Present(0, 0);
}

void VRView::OnResize(uint32_t rt_width, uint32_t rt_height)
{
	// Use the render target size OpenVR recommends.
	m_pOpenVR->GetRenderTargetSize(rt_width, rt_height);
	View::OnResize(rt_width, rt_height);
}

void VRView::CreateControllers()
{
	auto hr = m_pDevice->CreateVertexShader(g_OpenVR_VS, sizeof(g_OpenVR_VS), NULL, m_pOpenVRVertexShader.GetAddressOf());
	if (FAILED(hr))
		Fail(hr, L"CreateVertexShader (OpenVR)");

	hr = m_pDevice->CreatePixelShader(g_OpenVR_PS, sizeof(g_OpenVR_PS), NULL, m_pOpenVRPixelShader.GetAddressOf());
	if (FAILED(hr))
		Fail(hr, L"CreatePixelShader (OpenVR)");

	static const std::vector<D3D11_INPUT_ELEMENT_DESC> layout
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD", 1, DXGI_FORMAT_R32G32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
	};

	hr = m_pDevice->CreateInputLayout(layout.data(), static_cast<DWORD>(layout.size()), g_OpenVR_VS, static_cast<DWORD>(sizeof(g_OpenVR_VS)), m_pOpenVRInputLayout.GetAddressOf());
	if (FAILED(hr))
		Fail(hr, L"CreateInputLayout (OpenVR)");

	m_pOpenVRVertexHeap = std::make_unique<D3D11VertexHeap<vr::RenderModel_Vertex_t>>(m_pDevice.Get(), MAX_HEAP_VERTICES);
	m_pOpenVRIndexHeap = std::make_unique<D3D11IndexHeap<uint16_t>>(m_pDevice.Get(), MAX_HEAP_INDICES);

	hr = m_pDevice->CreateVertexShader(g_Mirror_VS, sizeof(g_Mirror_VS), NULL, m_pMirrorVertexShader.GetAddressOf());
	if (FAILED(hr))
		Fail(hr, L"CreateVertexShader (Mirror)");

	hr = m_pDevice->CreatePixelShader(g_Mirror_PS, sizeof(g_Mirror_PS), NULL, m_pMirrorPixelShader.GetAddressOf());
	if (FAILED(hr))
		Fail(hr, L"CreatePixelShader (Mirror)");
}

void VRView::DrawControllers()
{
	if (m_pOpenVR->IsDashboardActive())
		return;

	for (int idx = 0; idx < 2; ++idx)
	{
		vr::RenderModel_t model;
		vr::RenderModel_TextureMap_t texture;
		bool left_hand = idx == 0;

		if (m_pOpenVR->GetControllerModel(left_hand, model, texture))
		{
			if (!m_OpenVRVertices[idx] && model.rVertexData && model.rIndexData)
			{
				m_OpenVRVertices[idx] = m_pOpenVRVertexHeap->alloc_ptr(m_pDeviceContext.Get(), model.rVertexData, model.unVertexCount);
				m_OpenVRIndices[idx] = m_pOpenVRIndexHeap->alloc_ptr(m_pDeviceContext.Get(), model.rIndexData, model.unTriangleCount * 3);
			}

			auto it = m_OpenVRSRV.find(model.diffuseTextureId);
			if (it == m_OpenVRSRV.end())
			{
				auto textureDesc = CD3D11_TEXTURE2D_DESC{
					DXGI_FORMAT_R8G8B8A8_UNORM,
					texture.unWidth,
					texture.unHeight,
					1,
					1,
					D3D11_BIND_SHADER_RESOURCE };

				D3D11_SUBRESOURCE_DATA init_data{};
				init_data.pSysMem = texture.rubTextureMapData;
				init_data.SysMemPitch = texture.unWidth * sizeof(uint32_t);

				ComPtr<ID3D11Texture2D> pTexture;
				auto hr = m_pDevice->CreateTexture2D(&textureDesc, &init_data, pTexture.GetAddressOf());
				if (FAILED(hr))
					Fail(hr, L"CreateTexture2D (OpenVR)");

				D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc{};
				srvDesc.Format = textureDesc.Format;
				srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
				srvDesc.Texture2D.MipLevels = 1;

				ComPtr<ID3D11ShaderResourceView> pSRV;
				hr = m_pDevice->CreateShaderResourceView(pTexture.Get(), &srvDesc, pSRV.GetAddressOf());
				if (FAILED(hr))
					Fail(hr, L"CreateShaderResourceView (OpenVR)");

				m_OpenVRSRV[model.diffuseTextureId] = pSRV.Get();
				it = m_OpenVRSRV.find(model.diffuseTextureId);
			}

			if (m_OpenVRVertices[idx] && m_OpenVRIndices[idx] && it != m_OpenVRSRV.end())
			{
				m_vertexConstants.W = GetControllerMatrix(left_hand);
				m_vertexConstants.WVP = m_vertexConstants.W * GetViewProjectionMatrix();
				UpdateConstants(m_pVertexShaderConstantBuffer.Get(), m_vertexConstants);

				m_pStateTracker->SetPixelResource(it->second.Get());
				m_pStateTracker->SetVertexBuffer(m_OpenVRVertices[idx]->m_pBuffer.Get(), m_OpenVRVertices[idx]->stride);
				m_pStateTracker->SetIndexBuffer(m_OpenVRIndices[idx]->m_pBuffer.Get(), DXGI_FORMAT_R16_UINT);
				m_pStateTracker->SetVertexShader(m_pOpenVRVertexShader.Get());
				m_pStateTracker->SetPixelShader(m_pOpenVRPixelShader.Get());
				m_pStateTracker->SetRasterizerState(m_pRasterizerStateCullNone.Get());
				m_pStateTracker->SetInputLayout(m_pOpenVRInputLayout.Get());
				m_pStateTracker->SetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

				m_pDeviceContext->DrawIndexed(
					m_OpenVRIndices[idx]->count,
					m_OpenVRIndices[idx]->start_index,
					m_OpenVRVertices[idx]->start_index);
			}
		}
		else
		{
			// Clear model cache for this controller if it disappears.
			m_OpenVRVertices[idx].reset();
			m_OpenVRIndices[idx].reset();
		}
	}
}

void VRView::SetInputBindings(const std::vector<ActionBinding>& bindings)
{
	// Set up the keyboard mappings in the base view.
	View::SetInputBindings(bindings);

	m_controller_bindings.clear();

	// Set up the binding maps for use later.
	for (auto& binding : bindings)
	{
		if (!binding.openvr_path)
			continue;

		// Map path string to action handle for faster runtime use.
		auto hAction = m_pOpenVR->GetActionHandle(binding.openvr_path);
		m_action_handles[binding.openvr_path] = hAction;

		// Map game action to OpenVR action handle for input checking.
		if (binding.action != Action::None)
			m_controller_bindings[binding.action] = hAction;
	}
}

void VRView::PollInputBindings(const std::vector<ActionBinding>& bindings)
{
	for (auto& binding : bindings)
	{
		if (binding.action == Action::None)
			continue;

		auto it = m_controller_bindings.find(binding.action);
		if (it == m_controller_bindings.end())
			continue;

		auto hAction = it->second;
		bool pressed{ false };

		if (m_pOpenVR->ActionButtonChanged(hAction, pressed))
			m_action_states[hAction] = pressed;
	}
}

bool VRView::InputAction(Action action)
{
	// Check if there's a VR binding for this action.
	auto it = m_controller_bindings.find(action);
	if (it == m_controller_bindings.end())
		return false;

	// Read the state of the digital button bound to this action.
	auto hAction = it->second;
	if (m_action_states[hAction])
	{
		// Clear the press so it doesn't repeat.
		m_action_states[hAction] = false;
		return true;
	}

	// Fall back on checking keyboard mappings.
	return View::InputAction(action);
}

void VRView::OutputAction(Action action)
{
	static bool depleted{ false };

	auto it = m_controller_bindings.find(action);
	if (it == m_controller_bindings.end())
		return;

	switch (action)
	{
	case Action::Haptic_Seen:
		// Strong pulse for depleted, short weak pulse for seen.
		if (depleted)
			m_pOpenVR->HapticPulse(it->second, 0.1f, 75.0f, 1.0f);
		else
			m_pOpenVR->HapticPulse(it->second, 0.05f, 5.0f, 0.1f);

		depleted = false;
		break;
	case Action::Haptic_Depleted:
		// Flag that the next seen pulse should be stronger.
		depleted = true;
		break;
	case Action::Haptic_Dead:
		// 2 second stronger pulse.
		m_pOpenVR->HapticPulse(it->second, 2.0f, 50.0f, 1.0f);
		break;
	default:
		depleted = false;
		break;
	}
}

float VRView::GetHMDHeight() const
{
	auto mHMD = XMMatrixInverse(nullptr, m_pOpenVR->GetHMDMatrix());

	XMVECTOR vScale, vRot, vTrans;
	if (XMMatrixDecompose(&vScale, &vRot, &vTrans, mHMD))
	{
		XMFLOAT3 hmd_pos{};
		XMStoreFloat3(&hmd_pos, vTrans);
		return hmd_pos.y;
	}

	return 0.0f;
}

void VRView::ResetHMD(bool reset)
{
	m_world_scale = DEFAULT_WORLD_SCALE;

	if (m_seated)
	{
		if (reset)
			m_pOpenVR->ResetSeatedPose();
	}
	else
	{
		auto height_metres = GetHMDHeight();
		if (height_metres > 0.0f)
		{
			height_metres = std::max(MIN_HEIGHT_METRES, std::min(height_metres, MAX_HEIGHT_METRES));
			m_world_scale = height_metres / EYE_HEIGHT;
		}
	}
}

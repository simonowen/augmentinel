#include "stdafx.h"
#include "Settings.h"
#include "View.h"
#include "Sentinel_VS.h"
#include "Sentinel_PS.h"
#include "Effect_VS.h"
#include "Effect_PS.h"

static constexpr int MAX_HEAP_VERTICES = 65536;
static constexpr int MAX_HEAP_INDICES = 65536;

View::~View()
{
	if (m_pDeviceContext)
	{
		// As recommended by "Deferred Destruction Issues with Flip Presentation Swap Chains"
		m_pDeviceContext->ClearState();
		m_pDeviceContext->Flush();
	}

	if (m_pSwapChain)
		m_pSwapChain->SetFullscreenState(FALSE, nullptr);
}

XMFLOAT3 View::GetEyePosition() const
{
	XMFLOAT3 pos{};
	XMStoreFloat3(&pos, GetEyePositionVector());
	return pos;
}

XMFLOAT3 View::GetViewPosition() const
{
	XMFLOAT3 pos{};
	XMStoreFloat3(&pos, GetViewPositionVector());
	return pos;
}

XMFLOAT3 View::GetViewDirection() const
{
	XMFLOAT3 dir{};
	XMStoreFloat3(&dir, GetViewDirectionVector());
	return dir;
}

XMFLOAT3 View::GetUpDirection() const
{
	XMFLOAT3 up{};
	XMStoreFloat3(&up, GetViewUpVector());
	return up;
}

XMFLOAT3 View::GetCameraPosition() const
{
	return m_camera.GetPosition();
}

XMFLOAT3 View::GetCameraDirection() const
{
	return m_camera.GetDirection();
}

XMFLOAT3 View::GetCameraRotation() const
{
	return m_camera.GetRotations();
}

void View::SetCameraPosition(XMFLOAT3 pos)
{
	m_camera.SetPosition(pos);
}

void View::SetCameraRotation(XMFLOAT3 rot)
{
	m_camera.SetRotation(rot);
}

void View::SetMouseSpeed(int percent)
{
	percent = std::min(std::max(percent, 0), 100);
	m_mouse_divider = 2000.0f - (18.0f * percent);
}

void View::SetPitchLimits(float min_pitch, float max_pitch)
{
	m_camera.SetPitchLimits(min_pitch, max_pitch);
}

void View::SetFillColour(int fill_colour_idx)
{
	m_fill_colour_idx = fill_colour_idx;
}

void View::SetFogColour(int fog_colour_idx)
{
	m_vertexConstants.fog_colour_idx = fog_colour_idx;
}

void View::SetPalette(const std::vector<XMFLOAT4>& palette)
{
	auto copy_count = std::min(palette.size(), m_vertexConstants.Palette.size());
	std::copy(palette.begin(), palette.begin() + copy_count, m_vertexConstants.Palette.begin());
}

void View::EnableAnimatedNoise(bool enable)
{
	m_noise_enabled = enable;
}

bool View::PixelShaderEffectsActive() const
{
	// Pixel shader effects require an extra rendering step. If we know
	// none are active we can save a little time.
	return m_pixelConstants.view_dissolve != 0.0f ||
		m_pixelConstants.view_desaturate != 0.0f ||
		m_pixelConstants.view_fade != 0.0f;
}

float View::GetEffect(ViewEffect effect) const
{
	switch (effect)
	{
	case ViewEffect::Dissolve:
		return m_pixelConstants.view_dissolve;
	case ViewEffect::Desaturate:
		return m_pixelConstants.view_desaturate;
	case ViewEffect::Fade:
		return m_pixelConstants.view_fade;
	case ViewEffect::ZFade:
		return m_vertexConstants.z_fade;
	case ViewEffect::FogDensity:
		return m_vertexConstants.fog_density;
	default:
		assert(false);
		return 0.0f;
	}
}

void View::SetEffect(ViewEffect effect, float value)
{
	switch (effect)
	{
	case ViewEffect::Dissolve:
		m_pixelConstants.view_dissolve = value;
		break;
	case ViewEffect::Desaturate:
		m_pixelConstants.view_desaturate = value;
		break;
	case ViewEffect::Fade:
		m_pixelConstants.view_fade = value;
		break;
	case ViewEffect::ZFade:
		m_vertexConstants.z_fade = value;
		break;
	case ViewEffect::FogDensity:
		m_vertexConstants.fog_density = value;
		break;
	default:
		assert(false);
	}
}

bool View::TransitionEffect(ViewEffect effect, float target_value, float elapsed, float total_fade_time)
{
	auto current_value = GetEffect(effect);
	auto value = current_value;
	auto value_change = elapsed / total_fade_time;

	if (target_value > value)
		value = std::min(value + value_change, target_value);
	else
		value = std::max(value - value_change, target_value);

	SetEffect(effect, value);
	return current_value == target_value;
}

bool View::IsSuspended() const
{
	return false;
}

bool View::IsVR() const
{
	return false;
}

UINT View::GetDXGIAdapterIndex() const
{
	return 0;	// first adapter
}

HRESULT View::DisableAltEnter(HWND hwnd)
{
	ComPtr<IDXGIFactory4> factory4;
	auto hr = CreateDXGIFactory1(IID_PPV_ARGS(&factory4));
	if (SUCCEEDED(hr))
		hr = factory4->MakeWindowAssociation(hwnd, DXGI_MWA_NO_ALT_ENTER);

	return hr;
}

// Check if we can support variable refresh rate.
bool View::CheckTearingSupport()
{
	ComPtr<IDXGIFactory5> factory5;
	auto hr = CreateDXGIFactory1(IID_PPV_ARGS(&factory5));
	BOOL allowTearing = FALSE;
	if (SUCCEEDED(hr))
	{
		hr = factory5->CheckFeatureSupport(
			DXGI_FEATURE_PRESENT_ALLOW_TEARING, &allowTearing, sizeof(allowTearing));
	}

	return SUCCEEDED(hr) && allowTearing;
}

void View::Init(HWND hwnd, UINT width, UINT height)
{
	HRESULT hr;

	D3D_FEATURE_LEVEL featureLevel;
	std::vector<D3D_FEATURE_LEVEL> featureLevels =
	{
		D3D_FEATURE_LEVEL_11_0,
		D3D_FEATURE_LEVEL_10_1,
		D3D_FEATURE_LEVEL_10_0
	};

	DWORD createDeviceFlags = 0;
#ifdef _DEBUG
	createDeviceFlags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

	ComPtr<IDXGIFactory1> factory;
	hr = CreateDXGIFactory1(IID_PPV_ARGS(&factory));
	if (FAILED(hr))
		Fail(hr, L"CreateDXGIFactory1");

	ComPtr<IDXGIAdapter> pDXGIAdapter;
	hr = factory->EnumAdapters(GetDXGIAdapterIndex(), pDXGIAdapter.GetAddressOf());
	if (FAILED(hr))
		Fail(hr, L"EnumAdapters");

	hr = D3D11CreateDevice(
		pDXGIAdapter.Get(),
		D3D_DRIVER_TYPE_UNKNOWN,
		nullptr,
		createDeviceFlags,
		featureLevels.data(),
		static_cast<UINT>(featureLevels.size()),
		D3D11_SDK_VERSION,
		&m_pDevice,
		&featureLevel,
		&m_pDeviceContext);
	if (FAILED(hr))
		Fail(hr, L"D3D11CreateDevice");

	m_pStateTracker = std::make_unique<D3D11StateTracker>(m_pDeviceContext.Get());

	ComPtr<IDXGIDevice2> pDXGIDevice;
	if (FAILED(hr = m_pDevice.As(&pDXGIDevice)))
	{
		m_pDeviceContext = nullptr;
		Fail(hr, L"QueryInterface(IDXGIDevice2)");
	}

	ComPtr<IDXGIFactory2> pDXGIFactory;
	if (FAILED(hr = pDXGIAdapter->GetParent(IID_PPV_ARGS(pDXGIFactory.GetAddressOf()))))
		Fail(hr, L"pDXGIDevice->GetParent(IDXGIFactory2)");

	DisableAltEnter(hwnd);
	m_allow_tearing = CheckTearingSupport();

	DXGI_SWAP_CHAIN_DESC1 swapChainDesc{};
	swapChainDesc.Width = width;
	swapChainDesc.Height = height;
	swapChainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	swapChainDesc.Stereo = FALSE;
	swapChainDesc.SampleDesc.Count = 1;
	swapChainDesc.SampleDesc.Quality = 0;
	swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	swapChainDesc.Scaling = DXGI_SCALING_STRETCH;
	swapChainDesc.AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED;
	swapChainDesc.SwapEffect = IsWindows10OrGreater() ? DXGI_SWAP_EFFECT_FLIP_DISCARD :
		IsWindows8OrGreater() ? DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL :
		DXGI_SWAP_EFFECT_DISCARD;
	swapChainDesc.BufferCount = (swapChainDesc.SwapEffect == DXGI_SWAP_EFFECT_DISCARD) ? 1 : 2;
	swapChainDesc.Flags = (m_allow_tearing && swapChainDesc.BufferCount > 1) ?
		DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING : 0;

	hr = pDXGIFactory->CreateSwapChainForHwnd(
		m_pDevice.Get(), hwnd, &swapChainDesc, nullptr, nullptr, &m_pSwapChain);
	if (FAILED(hr))
		Fail(hr, L"CreateSwapChainForHwnd");

	m_invert_mouse = GetFlag(INVERT_MOUSE_KEY, DEFAULT_INVERT_MOUSE);
	m_msaa_samples = GetSetting(MSAA_SAMPLES_KEY, DEFAULT_MSAA_SAMPLES);

	OnResize(width, height);
	InitScene();
}

void View::InitScene()
{
	HRESULT hr;

	m_pVertexHeap = std::make_unique<D3D11VertexHeap<Vertex>>(m_pDevice.Get(), MAX_HEAP_VERTICES);
	m_pIndexHeap = std::make_unique<D3D11IndexHeap<uint32_t>>(m_pDevice.Get(), MAX_HEAP_INDICES);

	hr = m_pDevice->CreateVertexShader(g_Sentinel_VS, sizeof(g_Sentinel_VS), NULL, m_pSentinelVertexShader.GetAddressOf());
	if (FAILED(hr))
		Fail(hr, L"CreateVertexShader");

	hr = m_pDevice->CreatePixelShader(g_Sentinel_PS, sizeof(g_Sentinel_PS), NULL, m_pSentinelPixelShader.GetAddressOf());
	if (FAILED(hr))
		Fail(hr, L"CreatePixelShader");

	hr = m_pDevice->CreateVertexShader(g_Effect_VS, sizeof(g_Effect_VS), NULL, m_pEffectVertexShader.GetAddressOf());
	if (FAILED(hr))
		Fail(hr, L"CreateVertexShader (effect)");

	hr = m_pDevice->CreatePixelShader(g_Effect_PS, sizeof(g_Effect_PS), NULL, m_pEffectPixelShader.GetAddressOf());
	if (FAILED(hr))
		Fail(hr, L"CreatePixelShader (effect)");

	static const std::vector<D3D11_INPUT_ELEMENT_DESC> layout
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "COLOR", 0, DXGI_FORMAT_R32_UINT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
	};

	hr = m_pDevice->CreateInputLayout(layout.data(), static_cast<DWORD>(layout.size()), g_Sentinel_VS, static_cast<DWORD>(sizeof(g_Sentinel_VS)), m_pSentinelInputLayout.GetAddressOf());
	if (FAILED(hr))
		Fail(hr, L"CreateInputLayout");

	m_pDeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);


	auto RasterizerDesc = CD3D11_RASTERIZER_DESC(CD3D11_DEFAULT());
	RasterizerDesc.FillMode = D3D11_FILL_SOLID;

	hr = m_pDevice->CreateRasterizerState(&RasterizerDesc, m_pRasterizerStateCullBack.GetAddressOf());
	if (FAILED(hr))
		Fail(hr, L"CreateRasterizerState");

	RasterizerDesc.CullMode = D3D11_CULL_NONE;

	hr = m_pDevice->CreateRasterizerState(&RasterizerDesc, m_pRasterizerStateCullNone.GetAddressOf());
	if (FAILED(hr))
		Fail(hr, L"CreateRasterizerState");

	CD3D11_SAMPLER_DESC sd{ D3D11_DEFAULT };
	sd.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT;

	hr = m_pDevice->CreateSamplerState(&sd, m_pEffectSamplerState.ReleaseAndGetAddressOf());
	if (FAILED(hr))
		Fail(hr, L"CreateSamplerState (effect)");

	m_pDeviceContext->PSSetSamplers(0, 1, m_pEffectSamplerState.GetAddressOf());

	D3D11_BUFFER_DESC cbbd{};
	cbbd.Usage = D3D11_USAGE_DYNAMIC;
	cbbd.ByteWidth = sizeof(VertexConstants);
	cbbd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	cbbd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

	hr = m_pDevice->CreateBuffer(&cbbd, NULL, m_pVertexShaderConstantBuffer.GetAddressOf());
	if (FAILED(hr))
		Fail(hr, L"CreateBuffer (vs constants");

	m_pDeviceContext->VSSetConstantBuffers(0, 1, m_pVertexShaderConstantBuffer.GetAddressOf());

	cbbd.ByteWidth = sizeof(PixelConstants);

	hr = m_pDevice->CreateBuffer(&cbbd, NULL, m_pPixelShaderConstantBuffer.GetAddressOf());
	if (FAILED(hr))
		Fail(hr, L"CreateBuffer (ps constants");

	m_pDeviceContext->PSSetConstantBuffers(0, 1, m_pPixelShaderConstantBuffer.GetAddressOf());
}

void View::SetVerticalFOV(float /*fov*/)
{
	// Flat only.
}

void View::OnResize(uint32_t rt_width, uint32_t rt_height)
{
	if (!m_pDeviceContext)
		return;

	// Clear swapchain references to render target and depth stencil, and ensure it's flushed.
	m_pDeviceContext->OMSetRenderTargets(0, nullptr, nullptr);
	m_pRenderTargetView = nullptr;
	m_pEffectRenderTargetView = nullptr;
	m_pEffectShaderResourceView = nullptr;
	m_pEffectTexture = nullptr;
	m_pMsaaDepthStencilView = nullptr;
	m_pMsaaDepthStencilBuffer = nullptr;
	m_pMsaaRenderTargetView = nullptr;
	m_pMsaaTexture = nullptr;
	m_pDeviceContext->Flush();

	BOOL fullScreen;
	if (SUCCEEDED(m_pSwapChain->GetFullscreenState(&fullScreen, nullptr)))
		m_fullscreen = (fullScreen == TRUE);

	// Auto-resize to the current window client area.
	UINT swapChainFlags = m_allow_tearing ? DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING : 0;
	auto hr = m_pSwapChain->ResizeBuffers(0, rt_width, rt_height, DXGI_FORMAT_UNKNOWN, swapChainFlags);

	DXGI_SWAP_CHAIN_DESC1 swapChainDesc{};
	hr = m_pSwapChain->GetDesc1(&swapChainDesc);

	ComPtr<ID3D11Resource> pBackBufferPtr;
	hr = m_pSwapChain->GetBuffer(0, IID_PPV_ARGS(pBackBufferPtr.GetAddressOf()));
	if (FAILED(hr))
		Fail(hr, L"SwapChain->GetBuffer");

	hr = m_pDevice->CreateRenderTargetView(pBackBufferPtr.Get(), NULL, m_pRenderTargetView.GetAddressOf());
	pBackBufferPtr = nullptr;
	if (FAILED(hr))
		Fail(hr, L"CreateRenderTargetView (back)");

	UINT qualityLevels{ 0 };
	hr = m_pDevice->CheckMultisampleQualityLevels(
		swapChainDesc.Format, m_msaa_samples, &qualityLevels);

	// Disable MSAA if the requested MSAA sample count isn't available.
	if (FAILED(hr) || !qualityLevels)
		m_msaa_samples = 1;

	auto depthStencilDesc = CD3D11_TEXTURE2D_DESC{
		DXGI_FORMAT_D32_FLOAT,
		swapChainDesc.Width,
		swapChainDesc.Height,
		1,
		1,
		D3D11_BIND_DEPTH_STENCIL };

	depthStencilDesc.SampleDesc.Count = m_msaa_samples;
	depthStencilDesc.SampleDesc.Quality = 0;

	hr = m_pDevice->CreateTexture2D(&depthStencilDesc, NULL, m_pMsaaDepthStencilBuffer.GetAddressOf());
	if (FAILED(hr))
		Fail(hr, L"CreateTexture2D (msaa depth)");

	hr = m_pDevice->CreateDepthStencilView(m_pMsaaDepthStencilBuffer.Get(), NULL, m_pMsaaDepthStencilView.GetAddressOf());
	if (FAILED(hr))
		Fail(hr, L"CreateDepthStencilView (msaa)");

	auto textureDesc = CD3D11_TEXTURE2D_DESC{
		swapChainDesc.Format,
		swapChainDesc.Width,
		swapChainDesc.Height,
		1,
		1,
		D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE
	};

	hr = m_pDevice->CreateTexture2D(&textureDesc, NULL, m_pEffectTexture.GetAddressOf());
	if (FAILED(hr))
		Fail(hr, L"CreateTexture2D (effect)");

	hr = m_pDevice->CreateRenderTargetView(m_pEffectTexture.Get(), NULL, m_pEffectRenderTargetView.GetAddressOf());
	if (FAILED(hr))
		Fail(hr, L"CreateRenderTargetView (effect)");

	auto effectSRVDesc = CD3D11_SHADER_RESOURCE_VIEW_DESC{
		D3D11_SRV_DIMENSION_TEXTURE2D,
		textureDesc.Format,
		0,
		textureDesc.MipLevels
	};

	hr = m_pDevice->CreateShaderResourceView(m_pEffectTexture.Get(), &effectSRVDesc, m_pEffectShaderResourceView.GetAddressOf());
	if (FAILED(hr))
		Fail(hr, L"CreateShaderResourceView (effect)");

	textureDesc.SampleDesc.Count = m_msaa_samples;
	textureDesc.SampleDesc.Quality = 0;
	textureDesc.Format = swapChainDesc.Format;
	textureDesc.BindFlags = D3D11_BIND_RENDER_TARGET;

	hr = m_pDevice->CreateTexture2D(&textureDesc, NULL, m_pMsaaTexture.GetAddressOf());
	if (FAILED(hr))
		Fail(hr, L"CreateTexture2D (msaa)");

	hr = m_pDevice->CreateRenderTargetView(m_pMsaaTexture.Get(), NULL, m_pMsaaRenderTargetView.GetAddressOf());
	if (FAILED(hr))
		Fail(hr, L"CreateRenderTargetView (msaa)");

	auto viewport = CD3D11_VIEWPORT(
		0.0f,
		0.0f,
		static_cast<float>(swapChainDesc.Width),
		static_cast<float>(swapChainDesc.Height));
	m_pDeviceContext->RSSetViewports(1, &viewport);
}

void View::BeginScene()
{
	m_fRandom = static_cast<float>(random_uint32()) / std::numeric_limits<unsigned>::max();

	m_mViewProjection = GetViewProjectionMatrix();
}

void View::EndScene()
{
}

void View::DrawModel(Model& model, const Model& relativeModel)
{
	assert(model.type != ModelType::Unknown);

	if (!model.m_pHeapVertices)
		model.m_pHeapVertices = m_pVertexHeap->alloc(m_pDeviceContext.Get(), *(model.m_pVertices));

	if (!model.m_pHeapIndices)
		model.m_pHeapIndices = m_pIndexHeap->alloc(m_pDeviceContext.Get(), *(model.m_pIndices));

	if (!model.m_pVertexShader)
		model.m_pVertexShader = m_pSentinelVertexShader;

	if (!model.m_pPixelShader)
		model.m_pPixelShader = m_pSentinelPixelShader;

	m_vertexConstants.W = model.GetWorldMatrix(relativeModel);
	if (model.orthographic)
		m_vertexConstants.WVP = m_vertexConstants.W * GetOrthographicMatrix();
	else
		m_vertexConstants.WVP = m_vertexConstants.W * m_mViewProjection;
	m_vertexConstants.EyePos = GetEyePosition();
	m_vertexConstants.lighting = model.lighting ? 1 : 0;

	UpdateConstants(m_pVertexShaderConstantBuffer.Get(), m_vertexConstants);

	m_pixelConstants.dissolved = model.dissolved;
	m_pixelConstants.time = m_noise_enabled ? m_fRandom : 0.0f;
	UpdateConstants(m_pPixelShaderConstantBuffer.Get(), m_pixelConstants);

	m_pStateTracker->SetVertexBuffer(model.m_pHeapVertices->m_pBuffer.Get(), model.m_pHeapVertices->stride);
	m_pStateTracker->SetIndexBuffer(model.m_pHeapIndices->m_pBuffer.Get(), DXGI_FORMAT_R32_UINT);
	m_pStateTracker->SetVertexShader(model.m_pVertexShader.Get());
	m_pStateTracker->SetPixelShader(model.m_pPixelShader.Get());
	m_pStateTracker->SetRasterizerState(model.type == ModelType::Landscape ?
		m_pRasterizerStateCullNone.Get() : m_pRasterizerStateCullBack.Get());
	m_pStateTracker->SetInputLayout(m_pSentinelInputLayout.Get());
	m_pStateTracker->SetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	m_pDeviceContext->DrawIndexed(static_cast<UINT>(model.m_pIndices->size()), model.m_pHeapIndices->start_index, model.m_pHeapVertices->start_index);
}

void View::DrawControllers()
{
	// VR only.
}

void View::EnableFreeLook(bool enable)
{
	m_freelook = enable;
}

void View::MouseMove(int x, int y)
{
	if (m_freelook)
	{
		m_camera.Yaw(x / m_mouse_divider);
		m_camera.Pitch(y / m_mouse_divider * (m_invert_mouse ? -1 : 1));
	}
}

void View::UpdateKey(int virtKey, KeyState state)
{
	if (state == KeyState::UpEdge)
		m_keys.erase(virtKey);
	else
		m_keys[virtKey] = state;
}

KeyState View::GetKeyState(int key)
{
	auto state = m_keys[key];

	if (state == KeyState::DownEdge)
		m_keys[key] = KeyState::Down;
	else if (state == KeyState::UpEdge)
		m_keys[key] = KeyState::Up;

	return state;
}

bool View::AnyKeyPressed()
{
	for (auto& key : m_keys)
	{
		if (key.second == KeyState::DownEdge)
		{
			// Consume the down edge of the pressed key.
			key.second = KeyState::Down;
			return true;
		}
	}

	return false;
}

void View::ProcessDebugKeys()
{
	bool right_button = GetAsyncKeyState(VK_RBUTTON) < 0;
	auto move_step = right_button ? 0.5f : 0.1f;

	for (auto& key : m_keys)
	{
		auto& state = key.second;
		if (state == KeyState::Up)
			continue;

		switch (key.first)
		{
		case 'W':		m_camera.FlyForwards(move_step); break;
		case 'S':		m_camera.FlyBackwards(move_step); break;
		case 'A':		m_camera.FlyLeft(move_step); break;
		case 'D':		m_camera.FlyRight(move_step); break;
		case 'Q':		m_camera.FlyDown(move_step); break;
		case 'E':		m_camera.FlyUp(move_step); break;

		case VK_LEFT:	if (state == KeyState::DownEdge) m_camera.Yaw(-YawToRadians(PLAYER_YAW_DELTA)); break;
		case VK_RIGHT:	if (state == KeyState::DownEdge) m_camera.Yaw(+YawToRadians(PLAYER_YAW_DELTA)); break;
		case VK_UP:		if (state == KeyState::DownEdge) m_camera.Pitch(-XMConvertToRadians(SENTINEL_PITCH_DEGREES)); break;
		case VK_DOWN:	if (state == KeyState::DownEdge) m_camera.Pitch(+XMConvertToRadians(SENTINEL_PITCH_DEGREES)); break;

		case VK_HOME:	m_camera.FlyHome(); break;

		case VK_END:
			m_camera.SetPosition({ 0.0f, 0.0f, 0.0f });
			m_camera.SetRotation({ 0.0f, 0.0f, 0.0f });
			break;

		case VK_DELETE:
			m_camera.SetPosition({ 15.0f, 30.0f, -62.0f });
			m_camera.SetRotation({ PitchToRadians(0xea), YawToRadians(0x00), 0.0f });
			break;

		case 'I':
			if (state == KeyState::DownEdge)
				ToggleWireframe();
			break;

		default:
			// Don't change the state of unhandled keys.
			continue;
		}

		// Pressed keys become held after first processing.
		state = KeyState::Down;
	}
}

void View::ReleaseKeys()
{
	m_keys.clear();
}

void View::ToggleWireframe()
{
	D3D11_RASTERIZER_DESC rasterizerDesc{};

	m_pRasterizerStateCullNone->GetDesc(&rasterizerDesc);
	rasterizerDesc.FillMode = (rasterizerDesc.FillMode == D3D11_FILL_SOLID) ? D3D11_FILL_WIREFRAME : D3D11_FILL_SOLID;
	m_pDevice->CreateRasterizerState(&rasterizerDesc, m_pRasterizerStateCullNone.ReleaseAndGetAddressOf());

	m_pRasterizerStateCullBack->GetDesc(&rasterizerDesc);
	rasterizerDesc.FillMode = (rasterizerDesc.FillMode == D3D11_FILL_SOLID) ? D3D11_FILL_WIREFRAME : D3D11_FILL_SOLID;
	m_pDevice->CreateRasterizerState(&rasterizerDesc, m_pRasterizerStateCullBack.ReleaseAndGetAddressOf());
}

void View::SetInputBindings(const std::vector<ActionBinding>& bindings)
{
	m_key_bindings.clear();

	for (auto& binding : bindings)
	{
		if (binding.action != Action::None && !binding.virt_keys.empty())
			m_key_bindings[binding.action] = binding.virt_keys;
	}
}

void View::PollInputBindings(const std::vector<ActionBinding>&/*bindings*/)
{
	// VR only.
}

bool View::InputAction(Action action)
{
	auto it = m_key_bindings.find(action);
	if (it == m_key_bindings.end())
		return false;

	auto& virt_keys = it->second;
	for (auto& key : virt_keys)
	{
		if (key == VK_ANY)
			return AnyKeyPressed();
		else if (GetKeyState(key) == KeyState::DownEdge)
			return true;
	}

	return false;
}

void View::OutputAction(Action /*action*/)
{
	// VR only.
}

void View::ResetHMD(bool reset)
{
	// VR only.
	UNREFERENCED_PARAMETER(reset);
}

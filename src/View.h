#pragma once
#include "Action.h"
#include "Camera.h"
#include "Model.h"
#include "Game.h"
#include "BufferHeap.h"
#include "StateTracker.h"

// The window aspect ratio depends on the resolution and pixel aspect ratios.
static constexpr auto RESOLUTION_AR = static_cast<float>(SENTINEL_WIDTH) / SENTINEL_HEIGHT;
static constexpr auto WINDOW_AR = RESOLUTION_AR * PIXEL_ASPECT_RATIO;

static constexpr auto NEAR_CLIP = 0.1f;
static constexpr auto FAR_CLIP = 500.0f;

static constexpr auto MOUSE_DIVISOR_STEP = 100;
static constexpr auto DEFAULT_MOUSE_DIVISOR = MOUSE_DIVISOR_STEP * (50 + 1);
static constexpr auto DEFAULT_EFFECT_TRANSITION_TIME = 0.04f;

static constexpr auto INVERT_MOUSE_KEY{ L"InvertMouse" };
static constexpr auto MSAA_SAMPLES_KEY{ L"MsaaSamples" };

static constexpr auto DEFAULT_INVERT_MOUSE = false;
static constexpr auto DEFAULT_MSAA_SAMPLES = 4;

enum class ViewMode { Unspecified, Flat, VR };
enum class ViewEffect { Dissolve, Desaturate, Fade, ZFade, FogDensity };
enum class KeyState { Up, UpEdge, Down, DownEdge };
static_assert(static_cast<int>(KeyState::Up) == 0, "KeyState::Up must be first member");

struct VertexConstants
{
	XMMATRIX WVP{};
	XMMATRIX W{};
	std::array<XMFLOAT4, PALETTE_SIZE> Palette{};
	XMFLOAT3 EyePos;
	float z_fade{};
	float fog_density{};
	uint32_t fog_colour_idx{};
	uint32_t lighting{};
};
static_assert((sizeof(VertexConstants) & 0xf) == 0, "VS constants size must be multiple of 16");

struct PixelConstants
{
	float dissolved{ 0.0f };		// model dissolved level: 0.0f = fully visible, 1.0f = fully dissolved
	float time{ 0.0f };				// time-based noise (0.0f <= value < 1.0f)
	float view_dissolve{ 0.0f };	// 0.0f = normal, 1.0f = dissolved
	float view_desaturate{ 0.0f };	// 0.0f = colour, 1.0f = greysale
	float view_fade{ 0.0f };		// 0.0f = normal, 1.0f = black
	float padding[3];
};
static_assert((sizeof(PixelConstants) & 0xf) == 0, "PS constants size must be multiple of 16");

class View : public IScene
{
public:
	View() = default;
	virtual ~View();
	View(const View&) = delete;

	XMFLOAT3 GetEyePosition() const;
	XMFLOAT3 GetViewPosition() const;
	XMFLOAT3 GetViewDirection() const;
	XMFLOAT3 GetUpDirection() const;
	XMFLOAT3 GetCameraPosition() const;
	XMFLOAT3 GetCameraDirection() const;
	XMFLOAT3 GetCameraRotation() const;
	void SetCameraPosition(XMFLOAT3 pos);
	virtual void SetCameraRotation(XMFLOAT3 rot);

	void SetMouseSpeed(int percent);
	void SetPitchLimits(float min_pitch, float max_pitch);
	void SetFillColour(int fill_colour_idx);
	void SetFogColour(int fog_colour_idx);
	void SetPalette(const std::vector<XMFLOAT4>& palette);

	bool PixelShaderEffectsActive() const;
	float GetEffect(ViewEffect effect) const;
	void SetEffect(ViewEffect effect, float value);
	bool TransitionEffect(ViewEffect effect, float target_value, float elapsed, float total_fade_time = DEFAULT_EFFECT_TRANSITION_TIME);
	void EnableAnimatedNoise(bool enable);

	virtual bool IsVR() const;
	virtual bool IsSuspended() const;
	virtual UINT GetDXGIAdapterIndex() const;
	virtual void SetVerticalFOV(float fov);
	virtual void OnResize(uint32_t rt_width, uint32_t rt_height);
	virtual void SetInputBindings(const std::vector<ActionBinding>& bindings);
	virtual void PollInputBindings(const std::vector<ActionBinding>& bindings);
	virtual bool InputAction(Action action);
	virtual void OutputAction(Action action);
	virtual void ResetHMD(bool reset = false);

	virtual XMVECTOR GetEyePositionVector() const = 0;
	virtual XMVECTOR GetViewPositionVector() const = 0;
	virtual XMVECTOR GetViewDirectionVector() const = 0;
	virtual XMVECTOR GetViewUpVector() const = 0;
	virtual XMMATRIX GetViewProjectionMatrix() const = 0;
	virtual XMMATRIX GetOrthographicMatrix() const = 0;
	virtual bool IsPointerVisible() const override = 0;
	virtual void GetSelectionRay(XMVECTOR& vPos, XMVECTOR& vDir) const = 0;
	virtual void BeginScene();
	virtual void Render(IGame* pRender) = 0;
	virtual void EndScene() = 0;

	void DrawModel(Model& model, const Model& linkedModel = {}) override;
	void DrawControllers() override;

	void EnableFreeLook(bool enable);
	virtual void MouseMove(int x, int y);
	void UpdateKey(int virtKey, KeyState state);
	KeyState GetKeyState(int key);
	bool AnyKeyPressed();
	void ProcessDebugKeys();
	void ReleaseKeys();

protected:
	void Init(HWND hwnd, UINT width, UINT height);
	void InitScene();
	HRESULT DisableAltEnter(HWND hwnd);
	bool CheckTearingSupport();
	void ToggleWireframe();

	template <typename TData>
	HRESULT UpdateConstants(ID3D11Buffer* pBuffer, const TData& data)
	{
		HRESULT hr;
		D3D11_MAPPED_SUBRESOURCE ms{};
		if (SUCCEEDED(hr = m_pDeviceContext->Map(pBuffer, NULL, D3D11_MAP_WRITE_DISCARD, NULL, &ms)))
		{
			memcpy(ms.pData, &data, sizeof(data));
			m_pDeviceContext->Unmap(pBuffer, 0);
		}
		return hr;
	}

	int m_fill_colour_idx{ BLACK_PALETTE_INDEX };
	bool m_freelook{ true };
	Camera m_camera;
	XMMATRIX m_mViewProjection{};	// cached from BeginScene

	VertexConstants m_vertexConstants{};
	PixelConstants m_pixelConstants{};

	ComPtr<ID3D11Device> m_pDevice;
	ComPtr<ID3D11DeviceContext> m_pDeviceContext;
	ComPtr<IDXGISwapChain1> m_pSwapChain;
	ComPtr<ID3D11RenderTargetView> m_pRenderTargetView;
	ComPtr<ID3D11RenderTargetView> m_pEffectRenderTargetView;
	ComPtr<ID3D11Texture2D> m_pEffectTexture;
	ComPtr<ID3D11DepthStencilView> m_pMsaaDepthStencilView;
	ComPtr<ID3D11Texture2D> m_pMsaaDepthStencilBuffer;
	ComPtr<ID3D11RenderTargetView> m_pMsaaRenderTargetView;
	ComPtr<ID3D11Texture2D> m_pMsaaTexture;
	ComPtr<ID3D11RasterizerState> m_pRasterizerStateCullBack;
	ComPtr<ID3D11RasterizerState> m_pRasterizerStateCullNone;
	ComPtr<ID3D11VertexShader> m_pSentinelVertexShader;
	ComPtr<ID3D11PixelShader> m_pSentinelPixelShader;
	ComPtr<ID3D11VertexShader> m_pEffectVertexShader;
	ComPtr<ID3D11PixelShader> m_pEffectPixelShader;
	ComPtr<ID3D11SamplerState> m_pEffectSamplerState;
	ComPtr<ID3D11ShaderResourceView> m_pEffectShaderResourceView;
	ComPtr<ID3D11InputLayout> m_pSentinelInputLayout;
	ComPtr<ID3D11Buffer> m_pVertexShaderConstantBuffer;
	ComPtr<ID3D11Buffer> m_pPixelShaderConstantBuffer;

	std::unique_ptr<D3D11VertexHeap<Vertex>> m_pVertexHeap;
	std::unique_ptr<D3D11IndexHeap<uint32_t>> m_pIndexHeap;

	std::unique_ptr<D3D11StateTracker> m_pStateTracker;

	float m_mouse_divider{ 1.0f };
	int m_msaa_samples{ DEFAULT_MSAA_SAMPLES };	// 4x MSAA
	bool m_allow_tearing{ false };
	bool m_fullscreen{ false };

	bool m_invert_mouse{ false };
	bool m_noise_enabled{ true };
	float m_fRandom{};

	std::map<int, KeyState> m_keys;
	std::map<Action, std::vector<int>> m_key_bindings;
};

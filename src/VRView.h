#pragma once
#include "View.h"
#include "OpenVR.h"

static constexpr auto LEFT_HANDED_KEY = L"LeftHanded";
static constexpr auto SEATED_VR_KEY = L"SeatedVR";
static constexpr auto HMD_POINTER_KEY = L"HMDPointer";
static constexpr auto DEFAULT_LEFT_HANDED = false;
static constexpr auto DEFAULT_SEATED_VR = false;
static constexpr auto DEFAULT_HMD_POINTER = false;
static constexpr auto DEFAULT_WORLD_SCALE = 1.85f;

class VRView final : public View
{
public:
	VRView(HWND hwnd);

	bool IsVR() const override;
	bool IsSuspended() const override;
	UINT GetDXGIAdapterIndex() const override;

	XMVECTOR GetEyePositionVector() const override;
	XMVECTOR GetViewPositionVector() const override;
	XMVECTOR GetViewDirectionVector() const override;
	XMVECTOR GetViewUpVector() const override;
	XMMATRIX GetViewProjectionMatrix() const override;
	XMMATRIX GetOrthographicMatrix() const override;

	bool IsPointerVisible() const override;
	void GetSelectionRay(XMVECTOR& vPos, XMVECTOR& vDir) const override;
	void SetCameraRotation(XMFLOAT3 rot) override;
	void MouseMove(int x, int y) override;
	void Render(IGame* pRender) override;
	void DrawControllers() override;
	void EndScene() override;
	void OnResize(uint32_t rt_width, uint32_t rt_height) override;

	void SetInputBindings(const std::vector<ActionBinding>& bindings) override;
	void PollInputBindings(const std::vector<ActionBinding>& bindings) override;
	bool InputAction(Action action) override;
	void OutputAction(Action action) override;
	void ResetHMD(bool reset) override;

protected:
	void Init(HWND hwndPreview);
	void RenderEye(IGame* pGame, ID3D11Resource* pTexture, ID3D11RenderTargetView* pRTV);
	void CreateControllers();
	float GetHMDHeight() const;

	XMMATRIX GetControllerMatrix(bool left_hand) const;
	XMMATRIX GetPointerMatrix(bool left_hand) const;
	XMMATRIX GetEyeViewMatrix(bool right_eye) const;
	XMMATRIX GetHMDViewMatrix() const;

	std::unique_ptr<OpenVR> m_pOpenVR;

	bool m_right_eye{ false };
	bool m_hmd_pointer{ DEFAULT_HMD_POINTER };
	bool m_left_handed{ DEFAULT_LEFT_HANDED };
	float m_world_scale{ DEFAULT_WORLD_SCALE };
	bool m_seated{};
	float m_height_offset{};

	vr::VRActionSetHandle_t m_hActionSet{ vr::k_ulInvalidActionSetHandle };
	std::map<std::string, vr::VRActionHandle_t> m_action_handles;
	std::map<Action, vr::VRActionHandle_t> m_controller_bindings;
	std::map<vr::VRActionHandle_t, bool> m_action_states;

	ComPtr<ID3D11Texture2D> m_pEyeDepthStencilBuffer;
	ComPtr<ID3D11DepthStencilView> m_pEyeDepthStencilView;
	ComPtr<ID3D11Texture2D> m_pLeftEyeTexture;
	ComPtr<ID3D11Texture2D> m_pRightEyeTexture;
	ComPtr<ID3D11RenderTargetView> m_pLeftEyeRTV;
	ComPtr<ID3D11RenderTargetView> m_pRightEyeRTV;
	ComPtr<ID3D11ShaderResourceView> m_pLeftEyeSRV;
	ComPtr<ID3D11ShaderResourceView> m_pRightEyeSRV;

	ComPtr<ID3D11VertexShader> m_pOpenVRVertexShader;
	ComPtr<ID3D11PixelShader> m_pOpenVRPixelShader;
	ComPtr<ID3D11InputLayout> m_pOpenVRInputLayout;

	ComPtr<ID3D11VertexShader> m_pMirrorVertexShader;
	ComPtr<ID3D11PixelShader> m_pMirrorPixelShader;

	std::unique_ptr<D3D11VertexHeap<vr::RenderModel_Vertex_t>> m_pOpenVRVertexHeap;
	std::unique_ptr<D3D11IndexHeap<uint16_t>> m_pOpenVRIndexHeap;

	std::array<std::shared_ptr<D3D11HeapAllocation>, 2> m_OpenVRVertices;
	std::array<std::shared_ptr<D3D11HeapAllocation>, 2> m_OpenVRIndices;
	std::map<vr::TextureID_t, ComPtr<ID3D11ShaderResourceView>> m_OpenVRSRV;
};

#pragma once
#include <openvr.h>

static constexpr auto DISABLE_HAPTICS_KEY{ L"DisableHaptics" };
static constexpr auto DEFAULT_DISABLE_HAPTICS{ false };

#pragma comment(lib, "openvr_api.lib")
constexpr auto OPENVR_API_DLL = L"openvr_api.dll";

class OpenVR
{
public:
	OpenVR(bool seated, float near_clip, float far_clip);
	virtual ~OpenVR();
	OpenVR(const OpenVR&) = delete;

	static bool IsAvailable();

	UINT GetDXGIAdapterIndex() const;
	void GetRenderTargetSize(uint32_t& width, uint32_t& height);
	bool IsDashboardActive() const;
	bool IsControllerDetected() const;

	XMMATRIX GetHMDMatrix() const;
	XMMATRIX GetEyeMatrix(bool right_eye) const;
	XMMATRIX GetViewMatrix(bool right_eye);
	XMMATRIX GetProjectionMatrix(bool right_eye) const;
	XMMATRIX GetViewProjectionMatrix(bool right_eye);
	XMMATRIX GetControllerMatrix(bool left_handed);

	void ResetSeatedPose();
	bool UpdatePoses();
	void ProcessEvents(vr::VRActionSetHandle_t hActionSet);
	void Present(ID3D11Texture2D* pLeft, ID3D11Texture2D* pRight);

	std::string GetDeviceStringProperty(vr::TrackedDeviceIndex_t device, vr::TrackedDeviceProperty property);
	vr::VRActionSetHandle_t GetActionSetHandle(const char* path);
	vr::VRActionHandle_t GetActionHandle(const char* path);
	bool GetControllerModel(bool left_hand, vr::RenderModel_t& model, vr::RenderModel_TextureMap_t& texture);
	bool ActionButtonChanged(vr::VRActionHandle_t hAction, bool& state);
	XMMATRIX GetActionPose(vr::VRActionHandle_t hAction);
	void HapticPulse(vr::VRActionHandle_t hAction, float duration, float freq, float amplitude);

protected:
	void FailVR(vr::EVRInitError error);
	XMMATRIX GetHMDMatrixPoseEye(bool right_eye);
	XMMATRIX GetHMDMatrixProjectionEye(bool right_eye);

	XMMATRIX m_mHMDPose{ XMMatrixIdentity() };
	XMMATRIX m_mEyePosLeft{ XMMatrixIdentity() };
	XMMATRIX m_mEyePosRight{ XMMatrixIdentity() };
	XMMATRIX m_mProjectionLeft{ XMMatrixIdentity() };
	XMMATRIX m_mProjectionRight{ XMMatrixIdentity() };

	float m_near_clip{};
	float m_far_clip{};

	bool m_dashboard_active{ false };
	bool m_haptics_disabled{ true };
	bool m_controller_seen{ false };

	vr::IVRSystem* m_pHMD{ nullptr };
	vr::ETrackingUniverseOrigin m_tracking_origin{};
	std::array<vr::TrackedDevicePose_t, vr::k_unMaxTrackedDeviceCount> m_rTrackedDevicePose{};
	std::array<XMMATRIX, vr::k_unMaxTrackedDeviceCount> m_mDevicePoses{};

	std::array<vr::RenderModel_t, vr::k_unMaxTrackedDeviceCount> m_renderModels{};
	std::array<vr::RenderModel_TextureMap_t, vr::k_unMaxTrackedDeviceCount> m_modelTextures{};
};

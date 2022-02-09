#include "stdafx.h"
#include "OpenVR.h"
#include "Settings.h"

#include "OpenVR_VS.h"
#include "OpenVR_PS.h"

OpenVR::OpenVR(bool seated, float near_clip, float far_clip)
	: m_near_clip(near_clip), m_far_clip(far_clip)
{
	auto error = vr::VRInitError_None;
	m_pHMD = vr::VR_Init(&error, vr::VRApplication_Scene);
	if (error != vr::VRInitError_None)
		FailVR(error);

	auto pCompositor = vr::VRCompositor();
	if (!pCompositor)
		FailVR(vr::VRInitError_IPC_CompositorInitFailed);

	m_tracking_origin = seated ? vr::TrackingUniverseSeated : vr::TrackingUniverseStanding;
	pCompositor->SetTrackingSpace(m_tracking_origin);

	// Load the game actions and input bindings for the current controller.
	auto path = (WorkingDirectory() / "openvr/actions.json").make_preferred();
	vr::VRInput()->SetActionManifestPath(path.string().c_str());

	m_haptics_disabled = GetFlag(DISABLE_HAPTICS_KEY, DEFAULT_DISABLE_HAPTICS);
}

OpenVR::~OpenVR()
{
	vr::VR_Shutdown();
	m_pHMD = nullptr;
}


/*static*/ bool OpenVR::IsAvailable()
{
	// The DLL should be bundled with the application.
	auto hmodOpenVR = LoadLibrary(OPENVR_API_DLL);
	if (hmodOpenVR == NULL)
	{
		// Fall back on looking in the source path, for development convenience.
#ifdef WIN64
		fs::path path = L"openvr/bin/win64/";
#else
		fs::path path = L"openvr/bin/win32/";
#endif
		path.replace_filename(OPENVR_API_DLL);
		hmodOpenVR = LoadLibrary(path.c_str());
		if (hmodOpenVR == NULL)
			return false;
	}

	if (!vr::VR_IsRuntimeInstalled())
		return false;

	if (!vr::VR_IsHmdPresent())
		return false;

	return true;
}

UINT OpenVR::GetDXGIAdapterIndex() const
{
	if (m_pHMD)
	{
		int32_t adapter_index;
		m_pHMD->GetDXGIOutputInfo(&adapter_index);
		if (adapter_index >= 0)
			return static_cast<UINT>(adapter_index);
	}

	return 0;
}

void OpenVR::GetRenderTargetSize(uint32_t& width, uint32_t& height)
{
	if (!m_pHMD)
		FailVR(vr::VRInitError_Driver_HmdDisplayNotFound);

	m_pHMD->GetRecommendedRenderTargetSize(&width, &height);
}

bool OpenVR::IsDashboardActive() const
{
	return m_dashboard_active;
}

bool OpenVR::IsControllerDetected() const
{
	return m_controller_seen;
}

static XMMATRIX OpenVRMatrixToXMMATRIX(const vr::HmdMatrix34_t& matrix)
{
	return XMMATRIX{
		matrix.m[0][0], matrix.m[1][0], matrix.m[2][0], 0.0,
		matrix.m[0][1], matrix.m[1][1], matrix.m[2][1], 0.0,
		matrix.m[0][2], matrix.m[1][2], matrix.m[2][2], 0.0,
		matrix.m[0][3], matrix.m[1][3], matrix.m[2][3], 1.0f };
}

static XMMATRIX OpenVRMatrixToXMMATRIX(const vr::HmdMatrix44_t& matrix)
{
	return XMMATRIX{
		matrix.m[0][0], matrix.m[1][0], matrix.m[2][0], matrix.m[3][0],
		matrix.m[0][1], matrix.m[1][1], matrix.m[2][1], matrix.m[3][1],
		matrix.m[0][2], matrix.m[1][2], matrix.m[2][2], matrix.m[3][2],
		matrix.m[0][3], matrix.m[1][3], matrix.m[2][3], matrix.m[3][3] };
}

XMMATRIX OpenVR::GetHMDMatrixPoseEye(bool right_eye)
{
	if (!m_pHMD)
		return XMMatrixIdentity();

	auto eye = right_eye ? vr::Eye_Right : vr::Eye_Left;
	auto mEye = m_pHMD->GetEyeToHeadTransform(eye);
	return XMMatrixInverse(nullptr, OpenVRMatrixToXMMATRIX(mEye));
}

XMMATRIX OpenVR::GetHMDMatrixProjectionEye(bool right_eye)
{
	if (!m_pHMD)
		return XMMatrixIdentity();

	auto eye = right_eye ? vr::Eye_Right : vr::Eye_Left;
	auto mProj = m_pHMD->GetProjectionMatrix(eye, m_near_clip, m_far_clip);
	return OpenVRMatrixToXMMATRIX(mProj);
}

XMMATRIX OpenVR::GetControllerMatrix(bool left_handed)
{
	XMMATRIX mController = XMMatrixIdentity();

	for (vr::TrackedDeviceIndex_t device = 0; device < m_rTrackedDevicePose.size(); ++device)
	{
		const auto& pose = m_rTrackedDevicePose[device];

		if (pose.bDeviceIsConnected &&
			pose.bPoseIsValid &&
			vr::VRSystem()->GetTrackedDeviceClass(device) == vr::TrackedDeviceClass_Controller)
		{
			m_controller_seen = true;
			mController = m_mDevicePoses[device];

			auto preferred_hand = left_handed ?
				vr::TrackedControllerRole_LeftHand : vr::TrackedControllerRole_RightHand;

			// Stop searching if we found the preferred hand.
			if (vr::VRSystem()->GetControllerRoleForTrackedDeviceIndex(device) == preferred_hand)
				break;
		}

	}

	return mController;
}

bool OpenVR::GetControllerModel(bool left_hand, vr::RenderModel_t& model, vr::RenderModel_TextureMap_t& texture)
{
	auto preferred_hand = left_hand ?
		vr::TrackedControllerRole_LeftHand : vr::TrackedControllerRole_RightHand;

	for (vr::TrackedDeviceIndex_t device = 0; device < m_rTrackedDevicePose.size(); ++device)
	{
		const auto& pose = m_rTrackedDevicePose[device];

		if (pose.bDeviceIsConnected &&
			pose.bPoseIsValid &&
			vr::VRSystem()->GetTrackedDeviceClass(device) == vr::TrackedDeviceClass_Controller)
		{
			if (vr::VRSystem()->GetControllerRoleForTrackedDeviceIndex(device) == preferred_hand)
			{
				// Model not yet loaded?
				if (m_renderModels[device].unVertexCount == 0)
				{
					static vr::RenderModel_t* pRenderModel;
					auto model_name = GetDeviceStringProperty(device, vr::Prop_RenderModelName_String);
					auto error = vr::VRRenderModels()->LoadRenderModel_Async(model_name.c_str(), &pRenderModel);
					if (error == vr::VRRenderModelError_None)
						m_renderModels[device] = *pRenderModel;
				}

				// Model loaded but not texture?
				if (m_renderModels[device].unVertexCount && !m_modelTextures[m_renderModels[device].diffuseTextureId].rubTextureMapData)
				{
					vr::RenderModel_TextureMap_t* pModelTexture;
					auto error = vr::VRRenderModels()->LoadTexture_Async(m_renderModels[device].diffuseTextureId, &pModelTexture);
					if (error == vr::VRRenderModelError_None)
						m_modelTextures[m_renderModels[device].diffuseTextureId] = *pModelTexture;
				}

				if (m_renderModels[device].unVertexCount && m_modelTextures[m_renderModels[device].diffuseTextureId].rubTextureMapData)
				{
					model = m_renderModels[device];
					texture = m_modelTextures[m_renderModels[device].diffuseTextureId];
					return true;
				}

				// Controller found but model not ready.
				break;
			}
		}
	}

	return false;
}

void OpenVR::ResetSeatedPose()
{
	vr::VRSystem()->ResetSeatedZeroPose();
}

bool OpenVR::UpdatePoses()
{
	if (!m_pHMD)
		return false;

	auto error = vr::VRCompositor()->WaitGetPoses(
		m_rTrackedDevicePose.data(),
		static_cast<uint32_t>(m_rTrackedDevicePose.size()),
		NULL,
		0);

	if (error != vr::VRCompositorError_None)
		return false;

	for (int device = 0; device < vr::k_unMaxTrackedDeviceCount; ++device)
	{
		// If this device has a valid pose, convert it to XMMATRIX.
		if (m_rTrackedDevicePose[device].bPoseIsValid)
		{
			m_mDevicePoses[device] =
				OpenVRMatrixToXMMATRIX(m_rTrackedDevicePose[device].mDeviceToAbsoluteTracking);
		}
	}

	// Does the HMD have a valid pose?
	if (m_rTrackedDevicePose[vr::k_unTrackedDeviceIndex_Hmd].bPoseIsValid)
	{
		m_mHMDPose = XMMatrixInverse(nullptr, m_mDevicePoses[vr::k_unTrackedDeviceIndex_Hmd]);

		m_mProjectionLeft = GetHMDMatrixProjectionEye(false);
		m_mProjectionRight = GetHMDMatrixProjectionEye(true);
		m_mEyePosLeft = GetHMDMatrixPoseEye(false);
		m_mEyePosRight = GetHMDMatrixPoseEye(true);

		return true;
	}

	return false;
}

XMMATRIX OpenVR::GetHMDMatrix() const
{
	return m_mHMDPose;
}

XMMATRIX OpenVR::GetEyeMatrix(bool right_eye) const
{
	return right_eye ? m_mEyePosRight : m_mEyePosLeft;
}

XMMATRIX OpenVR::GetViewMatrix(bool right_eye)
{
	return m_mHMDPose * GetEyeMatrix(right_eye);
}

XMMATRIX OpenVR::GetProjectionMatrix(bool right_eye) const
{
	return right_eye ? m_mProjectionRight : m_mProjectionLeft;
}

XMMATRIX OpenVR::GetViewProjectionMatrix(bool right_eye)
{
	return GetViewMatrix(right_eye) * GetProjectionMatrix(right_eye);
}

void OpenVR::Present(ID3D11Texture2D* pLeft, ID3D11Texture2D* pRight)
{
	if (m_pHMD)
	{
		vr::Texture_t eyeTexture{ nullptr, vr::TextureType_DirectX, vr::ColorSpace_Auto };

		eyeTexture.handle = pLeft;
		vr::VRCompositor()->Submit(vr::Eye_Left, &eyeTexture);

		eyeTexture.handle = pRight;
		vr::VRCompositor()->Submit(vr::Eye_Right, &eyeTexture);

		vr::VRCompositor()->PostPresentHandoff();
	}
}

void OpenVR::FailVR(vr::EVRInitError error)
{
	throw std::exception(vr::VR_GetVRInitErrorAsEnglishDescription(error));
}

void OpenVR::ProcessEvents(vr::VRActionSetHandle_t hActionSet)
{
	if (!m_pHMD)
		return;

	vr::VREvent_t event{};
	while (m_pHMD->PollNextEvent(&event, sizeof(event)))
	{
		switch (event.eventType)
		{
		case vr::VREvent_DashboardActivated:
			m_dashboard_active = true;
			break;
		case vr::VREvent_DashboardDeactivated:
			m_dashboard_active = false;
			break;
		case vr::VREvent_Quit:
		case vr::VREvent_ProcessQuit:
			PostQuitMessage(0);
			break;
		case vr::VREvent_TrackedDeviceActivated:
			break;
		case vr::VREvent_TrackedDeviceDeactivated:
		case vr::VREvent_TrackedDeviceUpdated:
			m_renderModels[event.trackedDeviceIndex] = {};
			m_rTrackedDevicePose[event.trackedDeviceIndex] = {};
			m_controller_seen = false;
			break;
		}
	}

	if (m_controller_seen)
	{
		vr::VRActiveActionSet_t active_action_set{};
		active_action_set.ulActionSet = hActionSet;

		vr::VRInput()->UpdateActionState(
			&active_action_set, sizeof(active_action_set), 1);
	}
}

std::string OpenVR::GetDeviceStringProperty(vr::TrackedDeviceIndex_t device, vr::TrackedDeviceProperty property)
{
	auto string_len = m_pHMD->GetStringTrackedDeviceProperty(device, property, nullptr, 0, nullptr);
	if (string_len == 0)
		return "";

	std::vector<char> string_buf(string_len);
	string_len = m_pHMD->GetStringTrackedDeviceProperty(device, property, string_buf.data(), string_len, nullptr);
	return string_buf.data();
}

vr::VRActionSetHandle_t OpenVR::GetActionSetHandle(const char* path)
{
	vr::VRActionSetHandle_t hSet{};
	vr::VRInput()->GetActionSetHandle(path, &hSet);
	return hSet;
}

vr::VRActionHandle_t OpenVR::GetActionHandle(const char* path)
{
	vr::VRActionHandle_t hAction{};
	vr::VRInput()->GetActionHandle(path, &hAction);
	return hAction;
}

bool OpenVR::ActionButtonChanged(vr::VRActionHandle_t hAction, bool& state)
{
	vr::InputDigitalActionData_t actionData{};
	auto err = vr::VRInput()->GetDigitalActionData(
		hAction, &actionData,
		sizeof(actionData),
		vr::k_ulInvalidInputValueHandle);

	if (err == vr::VRInputError_None && actionData.bActive)
	{
		state = actionData.bState;
		return actionData.bChanged;
	}

	state = false;
	return false;
}

XMMATRIX OpenVR::GetActionPose(vr::VRActionHandle_t hAction)
{
	vr::InputPoseActionData_t poseData{};
	auto err = vr::VRInput()->GetPoseActionDataRelativeToNow(
		hAction,
		m_tracking_origin,
		0.0f,
		&poseData,
		sizeof(poseData),
		vr::k_ulInvalidInputValueHandle);

	if (err == vr::VRInputError_None && poseData.pose.bPoseIsValid)
		return OpenVRMatrixToXMMATRIX(poseData.pose.mDeviceToAbsoluteTracking);

	return XMMatrixIdentity();
}

void OpenVR::HapticPulse(vr::VRActionHandle_t hAction, float duration, float freq, float amplitude)
{
	if (m_haptics_disabled)
		return;

	vr::VRInput()->TriggerHapticVibrationAction(
		hAction, 0.0f, duration, freq, amplitude, vr::k_ulInvalidInputValueHandle);
}

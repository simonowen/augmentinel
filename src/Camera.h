#pragma once

class Camera
{
public:
	XMFLOAT3 GetPosition() const;
	XMFLOAT3 GetDirection() const;
	XMFLOAT3 GetRotations() const;
	XMVECTOR GetPositionVector() const;
	XMVECTOR GetDirectionVector() const;
	XMVECTOR GetUpVector() const;
	XMMATRIX GetViewMatrix() const;

	void SetPosition(XMFLOAT3 pos);
	void SetRotation(XMFLOAT3 rot);
	void SetPitchLimits(float min_pitch, float max_pitch);

	void FlyHome();
	void FlyForwards(float distance);
	void FlyBackwards(float distance);
	void FlyLeft(float distance);
	void FlyRight(float distance);
	void FlyUp(float distance);
	void FlyDown(float distance);

	void Pitch(float radians);
	void Yaw(float radians);
	void Roll(float radians);

protected:
	XMFLOAT3 m_eyeRotations{};
	float padding1{};

	XMVECTOR m_eyeHomePosition{};
	XMFLOAT3 m_eyeHomeRotation{};
	float padding2{};

	XMVECTOR m_eyePosition{};
	XMVECTOR m_eyeDirection{};
	XMVECTOR m_eyeUp{};
	XMVECTOR m_eyeRight{};

	XMMATRIX m_mView{};

	static constexpr auto PITCH_MARGIN = 0.0001f;
	float m_minPitch{ -XM_PIDIV2 + PITCH_MARGIN };
	float m_maxPitch{ XM_PIDIV2 - PITCH_MARGIN };

	void Update();
};

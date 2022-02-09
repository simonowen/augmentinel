#include "stdafx.h"
#include "Camera.h"

XMFLOAT3 Camera::GetPosition() const
{
	XMFLOAT3 pos{};
	XMStoreFloat3(&pos, m_eyePosition);
	return pos;
}

XMFLOAT3 Camera::GetDirection() const
{
	XMFLOAT3 dir{};
	XMStoreFloat3(&dir, m_eyeDirection);
	return dir;
}

XMFLOAT3 Camera::GetRotations() const
{
	return m_eyeRotations;
}

XMVECTOR Camera::GetPositionVector() const
{
	return m_eyePosition;
}

XMVECTOR Camera::GetDirectionVector() const
{
	return m_eyeDirection;
}

XMVECTOR Camera::GetUpVector() const
{
	return m_eyeUp;
}

XMMATRIX Camera::GetViewMatrix() const
{
	return m_mView;
}

void Camera::SetPosition(XMFLOAT3 pos)
{
	m_eyePosition = { pos.x, pos.y, pos.z, 1.0f };
	m_eyeHomePosition = m_eyePosition;
	Update();
}

void Camera::SetRotation(XMFLOAT3 rot)
{
	m_eyeRotations = rot;
	m_eyeHomeRotation = rot;
	Update();
}

void Camera::SetPitchLimits(float min_pitch, float max_pitch)
{
	m_minPitch = min_pitch;
	m_maxPitch = max_pitch;
}

void Camera::Update()
{
	m_eyeRotations.x = std::min(std::max(m_eyeRotations.x, m_minPitch), m_maxPitch);

	auto mRotation = XMMatrixRotationRollPitchYaw(m_eyeRotations.x, m_eyeRotations.y, m_eyeRotations.z);
	m_eyeRight = XMVector4Transform({ 1.0f, 0.0f, 0.0f, 0.0f }, mRotation);
	m_eyeUp = XMVector4Transform({ 0.0f, 1.0f, 0.0f, 0.0f }, mRotation);
	m_eyeDirection = XMVector4Transform({ 0.0f, 0.0f, 1.0f, 0.0f }, mRotation);

	m_mView = XMMatrixLookToLH(m_eyePosition, m_eyeDirection, m_eyeUp);
}

void Camera::FlyHome()
{
	m_eyePosition = m_eyeHomePosition;
	m_eyeRotations = m_eyeHomeRotation;
	Update();
}

void Camera::FlyForwards(float distance)
{
	m_eyePosition += m_eyeDirection * distance;
	Update();
}

void Camera::FlyBackwards(float distance)
{
	m_eyePosition -= m_eyeDirection * distance;
	Update();
}

void Camera::FlyLeft(float distance)
{
	m_eyePosition -= m_eyeRight * distance;
	Update();
}

void Camera::FlyRight(float distance)
{
	m_eyePosition += m_eyeRight * distance;
	Update();
}

void Camera::FlyUp(float distance)
{
	m_eyePosition += m_eyeUp * distance;
	Update();
}

void Camera::FlyDown(float distance)
{
	m_eyePosition -= m_eyeUp * distance;
	Update();
}

void Camera::Pitch(float radians)
{
	m_eyeRotations.x += radians;
	Update();
}

void Camera::Yaw(float radians)
{
	m_eyeRotations.y += radians;
	Update();
}

void Camera::Roll(float radians)
{
	m_eyeRotations.z += radians;
	Update();
}

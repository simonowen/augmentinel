#pragma once
#include "Model.h"

enum class AnimationType
{
	Dissolve, Yaw
};

struct Animation
{
	Animation(AnimationType type_, int id_, float duration_, float start_value_, float end_value_, bool interruptable_ = false)
		: type(type_), id(id_), elapsed_time(0.0f), total_time(duration_), start_value(start_value_), end_value(end_value_), interruptible(interruptable_) {}

	AnimationType type;
	float elapsed_time;
	float total_time;
	int id;
	float start_value;
	float end_value;
	bool interruptible;
};

struct IModelSource
{
	virtual Model* FindModelById(int id) = 0;
};

void AnimateModels(std::vector<Animation>& animations, float elapsed, IModelSource* model_source);

#include "stdafx.h"
#include "Animate.h"

void AnimateModels(std::vector<Animation>& animations, float elapsed, IModelSource* model_source)
{
	for (auto it = animations.begin(); it != animations.end(); )
	{
		auto model = model_source->FindModelById(it->id);
		if (!model)
		{
			// Delete animation for removed models.
			it = animations.erase(it);
			continue;
		}

		// Advance the animation by the elapsed time.
		it->elapsed_time += elapsed;

		// Calculate how far through we are, and the value at that point.
		auto proportion = std::min(it->elapsed_time, it->total_time) / it->total_time;
		auto new_value = it->start_value + (it->end_value - it->start_value) * proportion;

		switch (it->type)
		{
		case AnimationType::Dissolve:
			model->dissolved = new_value;
			break;

		case AnimationType::Yaw:
			model->rot.y = new_value;
			break;
		}

		// Animation complete?
		if (it->elapsed_time >= it->total_time)
		{
			it = animations.erase(it);
		}
		else
		{
			++it;
		}
	}
}

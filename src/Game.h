#pragma once
#include "Model.h"

class View;
class Audio;

struct IScene
{
	virtual void DrawModel(Model& model, const Model& linkedModel = {}) = 0;
	virtual void DrawControllers() = 0;
	virtual bool IsPointerVisible() const = 0;
};

struct IGame
{
	virtual void Render(IScene* pScene) = 0;
};


class Game : public IGame
{
public:
	virtual ~Game() = default;
	virtual void Frame(float elapsed_seconds) = 0;
};

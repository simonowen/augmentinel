#pragma once
#include "Game.h"
#include "Spectrum.h"
#include "Animate.h"

enum class GameState
{
	Unknown, Reset, TitleScreen, LandscapePreview, Game, SkyView, PlayerDead, ShowKiller, Complete
};

class Augmentinel : public Game, public IModelSource, public ISentinelEvents
{
public:
	Augmentinel(
		std::shared_ptr<View>& pView,
		std::shared_ptr<Audio>& pAudio);

	void Render(IScene* pScene) final override;
	void Frame(float elapsed_seconds) final override;

	static void Options(HINSTANCE hinst, HWND hwndParent);

protected:
	void PlayTune(const std::wstring& filename);
	void PlayMusic();

	bool SceneRayTest(XMVECTOR vRayPos, XMVECTOR vRayDir, RayTarget& hit, int ignore_id = -1);
	bool SceneModelVisible(XMVECTOR vRayPos, const Model& model, int ignore_id = -1);
	bool SceneTileVisible(XMVECTOR vRayPos, int tile_x, int tile_z);

	void ChangeState(GameState new_state);
	bool RunUntilStateChange();

	std::vector<Model> GetModelStack(int tile_x, int tile_z);
	void AddText(const std::string& str, float x_centre, float y, float z, int colour = 1, bool reversed = false);
	bool PlayerAnimationActive() const;
	void SetSeen(SeenState seen_state);

	void LoadLandscapeCodes();
	void SaveLastLandscape(int landscape_bcd);
	void AddLandscapeCode(int landscape_bcd, uint32_t secret_code_bcd);
	void RemoveLandscapeCode(int landscape_bcd);

	// IModelSource implementation.
	Model* FindModelById(int id) final override;

	// ISentinelEvents implementation.
	void OnTitleScreen() final override;
	void OnLandscapeInput(int& landscape_bcd, uint32_t& secret_code_bcd) final override;
	void OnLandscapeGenerated() final override;
	void OnNewPlayerView() final override;
	void OnPlayerDead() final override;
	void OnInputAction(uint8_t& action) final override;
	void OnGameModelChanged(int id, bool player_initiated) final override;
	bool OnTargetActionTile(InputAction action, int& tile_x, int& tile_z) final override;
	void OnPlayTune(int n) final override;
	void OnSoundEffect(int n, int idx) final override;
	void OnHideEnergyPanel() final override;
	void OnAddEnergySymbol(int symbol_idx, int x_offset) final override;

	std::shared_ptr<View> m_pView;
	std::shared_ptr<Audio> m_pAudio;

	Model m_landscape;
	Model m_player;
	Model m_skybox;
	Model m_pointer_line;
	Model m_pointer_target;

	std::vector<Model> m_drawn_models;
	std::vector<Model> m_text;
	std::vector<Model> m_icons;
	std::vector<Animation> m_animations;

	int m_seen_count{ 0 };
	bool m_seen_sound{ false };
	float m_frame_time{ 0.0f };

	GameState m_state{ GameState::Unknown };
	int m_substate{ 0 };
	bool m_title_shown{ false };
	bool m_rotate_landscape{ true };

	bool m_tunes_enabled{ true };
	bool m_music_enabled{ true };
	bool m_music_playing{ false };
	int m_music_volume{ 100 };

	int m_landscape_bcd{ 0 };
	std::map<int, uint32_t> m_codes;
	std::unique_ptr<Spectrum> m_spectrum;
};

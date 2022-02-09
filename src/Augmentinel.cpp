#include "stdafx.h"
#include "resource.h"
#include "Application.h"
#include "Augmentinel.h"
#include "Action.h"
#include "Audio.h"
#include "View.h"
#include "VRView.h"
#include "OpenVR.h"
#include "Settings.h"

constexpr auto MAX_STATE_FRAMES = 1000;		// max emulated frames in the current state.
constexpr auto SENTINEL_TURN_TIME = 0.25f;	// 0.25 second animation time for turns.
constexpr auto DISSOLVE_TIME = 1.4f;		// Dissolve time for created or absorbed objects
constexpr auto SEEN_FRAME_THRESHOLD = 2;	// Number of frames before trusting seen state.
constexpr auto PAGE_STEPS = 10;				// Page Up/Down steps for entire landscape list.
constexpr auto DEFAULT_MOUSE_SPEED = 70;	// default mouse move sensitivity.
constexpr auto HYPERSPACE_ANGLE = 60;		// a transfer to sky at >=60 degrees for hyperspace.
constexpr auto SKY_VIEW_ANGLE = 10;			// placing a robot in the sky at >=10 degrees is sky view.
constexpr auto SKY_VIEW_DISTANCE = 60.0f;	// view distance from player in sky view.
constexpr auto SKY_VIEW_DISTANCE_VR = 30.0f;// sky view distance in VR mode (lower, due to higher FOV).
constexpr auto SEEN_HAPTIC_FREQ = 0.1f;		// seconds between haptic pulses when seen.
constexpr auto TILE_AXIS_SAMPLES = 5;		// tile visibility hit test samples per axis.
constexpr auto VOLUME_STEP = 10;			// volume adjustment step percentage.
constexpr auto POINTER_SCALE = 4;			// 3D pointer block scale.
constexpr auto TEMP_ID_BASE = 0x100;		// Base id for temporary model.

constexpr auto SENTINEL_SNAPSHOT_FILE = L"./sentinel.sna";

static const auto LANDSCAPES_SECTION{ L"Landscapes" };
static const auto LAST_LANDSCAPE_KEY{ L"LastLandscape" };
static const auto MOUSE_SPEED_KEY{ L"MouseSpeed" };
static const auto SOUND_PACK_KEY{ L"SoundPack" };
static const auto GAME_SPEED_KEY{ L"GameSpeed" };
static const auto TUNES_ENABLED_KEY{ L"TunesEnabled" };
static const auto MUSIC_ENABLED_KEY{ L"MusicEnabled" };
static const auto MUSIC_VOLUME_KEY{ L"MusicVolume" };
static const auto VERTICAL_FOV_KEY{ L"VerticalFov" };

static const auto SOUND_PACK_DIR{ L"sounds" };
static const auto MUSIC_SUBDIR{ L"music" };
static const auto DEFAULT_SOUND_PACK{ L"Commodore Amiga" };
static const auto DEFAULT_GAME_SPEED{ 120 };
static const auto DEFAULT_TUNES_ENABLED{ true };
static const auto DEFAULT_MUSIC_ENABLED{ true };
static const auto DEFAULT_MUSIC_VOLUME{ 70 };
static const auto DEFAULT_VERTICAL_FOV{ 45 };

constexpr auto COMPLETE_TUNE = L"complete.wav";
constexpr auto DISINTEGRATE_SOUND = L"disintegrate.wav";
constexpr auto DISSOLVE_SOUND = L"dissolve.wav";
constexpr auto GAMEOVER_TUNE = L"gameover.wav";
constexpr auto HYPERSPACE_TUNE = L"hyperspace.wav";
constexpr auto MEANIE_SOUND = L"meanie.wav";
constexpr auto PING_SOUND = L"ping.wav";
constexpr auto PONG_SOUND = L"pong.wav";
constexpr auto SEEN_SOUND = L"seen.wav";
constexpr auto TITLE_TUNE = L"title.wav";
constexpr auto TRANSFER_TUNE = L"transfer.wav";
constexpr auto TURN_SOUND = L"turn.wav";
constexpr auto UTURN_TUNE = L"u-turn.wav";

static std::vector<const wchar_t*> effects_and_tunes
{
	COMPLETE_TUNE, DISINTEGRATE_SOUND, DISSOLVE_SOUND,
	GAMEOVER_TUNE, HYPERSPACE_TUNE, MEANIE_SOUND, PING_SOUND, PONG_SOUND,
	SEEN_SOUND, TITLE_TUNE, TRANSFER_TUNE, TURN_SOUND, UTURN_TUNE
};

static std::vector<std::wstring> music_files;
static decltype(music_files.begin()) it_music;

static std::vector<ActionBinding> action_bindings =
{
	{ Action::TitleContinue,	{ VK_ANY },					"/actions/game/in/select" },
	{ Action::LandscapeSelect,	{ VK_RETURN, VK_LBUTTON },	"/actions/game/in/select" },
	{ Action::LandscapePrev,	{ VK_LEFT },				"/actions/game/in/landscape_prev" },
	{ Action::LandscapeNext,	{ VK_RIGHT },				"/actions/game/in/landscape_next" },
	{ Action::LandscapeFirst,	{ VK_HOME },				"/actions/game/in/landscape_first" },
	{ Action::LandscapeLast,	{ VK_END },					"/actions/game/in/landscape_last" },
	{ Action::LandscapePgUp,	{ VK_PRIOR },				"/actions/game/in/landscape_pgup" },
	{ Action::LandscapePgDn,	{ VK_NEXT },				"/actions/game/in/landscape_pgdn" },
	{ Action::Quit,				{ VK_ESCAPE },				"/actions/game/in/quit" },
	{ Action::Pause,			{ 'P', VK_PAUSE },			"/actions/game/in/pause" },
	{ Action::Absorb,			{ 'A', VK_LBUTTON },		"/actions/game/in/select" },
	{ Action::Tree,				{ 'T' },					"/actions/game/in/tree"},
	{ Action::Boulder,			{ 'B', VK_RBUTTON },		"/actions/game/in/boulder"},
	{ Action::Robot,			{ 'R', VK_MBUTTON },		"/actions/game/in/robot"},
	{ Action::Transfer,			{ 'Q', VK_XBUTTON1 },		"/actions/game/in/transfer" },
	{ Action::Hyperspace,		{ 'H' },					"/actions/game/in/hyperspace" },
	{ Action::U_Turn,			{ 'U' },					"/actions/game/in/u_turn" },
	{ Action::ResetHMD,			{ VK_SPACE },				"/actions/game/in/reset_hmd" },
	{ Action::SkyViewContinue,	{ VK_ANY },					"/actions/game/in/select" },
	{ Action::Pose_LeftPointer,{},							"/actions/game/in/left_pointer" },
	{ Action::Pose_RightPointer,{},							"/actions/game/in/right_pointer" },
	{ Action::Haptic_Seen,		{},							"/actions/game/out/haptic" },
	{ Action::Haptic_Depleted,	{},							"/actions/game/out/haptic" },
	{ Action::Haptic_Dead,		{},							"/actions/game/out/haptic" },
	{ Action::TurnLeft,			{ VK_LEFT },				"/actions/game/in/turn_left" },
	{ Action::TurnRight,		{ VK_RIGHT },				"/actions/game/in/turn_right" },
	{ Action::TurnLeft45,		{ VK_PRIOR },				"/actions/game/in/turn_left_45" },
	{ Action::TurnRight45,		{ VK_NEXT },				"/actions/game/in/turn_right_45" },
	{ Action::TurnLeft90,		{},							"/actions/game/in/turn_left_90" },
	{ Action::TurnRight90,		{},							"/actions/game/in/turn_right_90" },
	{ Action::Turn180,			{},							"/actions/game/in/turn_180" },
	{ Action::LookUp,			{ VK_UP },					nullptr },
	{ Action::LookDown,			{ VK_DOWN },				nullptr },
	{ Action::ToggleTunes,		{ 'N' },					nullptr },
	{ Action::ToggleMusic,		{ 'M' },					nullptr },
	{ Action::MusicVolumeUp,	{ VK_OEM_PLUS },			nullptr },
	{ Action::MusicVolumeDown,	{ VK_OEM_MINUS },			nullptr },
};

std::vector<std::pair<int, std::wstring>> game_speeds
{
	{  80, L"Very Easy  (15 seconds)" },
	{ 100, L"Easy  (12 seconds)" },
	{ 120, L"Normal  (10 seconds)" },
	{ 150, L"Hard  (8 seconds)" },
	{ 200, L"Very Hard  (6 seconds)" },
};

std::vector<std::pair<int, std::wstring>> msaa_modes
{
	{ 1, L"Off" },
	{ 2, L"2x" },
	{ 4, L"4x" },
};

Augmentinel::Augmentinel(std::shared_ptr<View>& pView, std::shared_ptr<Audio>& pAudio)
	: m_pView(pView), m_pAudio(pAudio)
{
	// Pre-load all sound effects and music from the current sound pack.
	auto sound_path = fs::path(SOUND_PACK_DIR) / GetSetting(SOUND_PACK_KEY, DEFAULT_SOUND_PACK);
	for (auto& sound : effects_and_tunes)
		pAudio->LoadWAV(sound_path / sound);

	auto music_path = fs::path(SOUND_PACK_DIR) / MUSIC_SUBDIR;
	for (auto& p : fs::directory_iterator(music_path))
	{
		if (p.path().extension() == ".wav")
		{
			pAudio->LoadWAV(p.path());
			music_files.push_back(p.path().filename());
		}
	}

	// Shuffle music playback order, but play the selection in a loop.
	std::shuffle(music_files.begin(), music_files.end(), random_source());
	it_music = music_files.begin();

	// Sound settings.
	m_tunes_enabled = GetSetting(TUNES_ENABLED_KEY, DEFAULT_TUNES_ENABLED);
	m_music_enabled = GetSetting(MUSIC_ENABLED_KEY, DEFAULT_MUSIC_ENABLED);
	m_music_volume = GetSetting(MUSIC_VOLUME_KEY, DEFAULT_MUSIC_VOLUME);

	// Set input bindings.
	pView->SetInputBindings(action_bindings);

	// Set mouse speed from the options, and delete the old key name to avoid confusion.
	pView->SetMouseSpeed(GetSetting(MOUSE_SPEED_KEY, DEFAULT_MOUSE_SPEED));
	RemoveSetting(L"MouseSensitivity");

	// Create models for the pointer line and target square.
	m_pointer_line = Model::CreateBlock(0.002f, 0.002f, 100.0f, WHITE_PALETTE_INDEX, ModelType::PointerLine);
	m_pointer_target = Model::CreateBlock(1.0f, 1.0f, 0.25f, WHITE_PALETTE_INDEX, ModelType::PointerTarget);

	// Determine the game running speed.
	auto game_speed = GetSetting(GAME_SPEED_KEY, DEFAULT_GAME_SPEED);
	game_speed = std::max(game_speed, game_speeds[0].first);
	game_speed = std::min(game_speed, game_speeds[game_speeds.size() - 1].first);

	// Calculate the frame time to give the requested running speed.
	m_frame_time = 100.0f / game_speed / SPECTRUM_FRAMES_PER_SECOND;

	LoadLandscapeCodes();
	ChangeState(GameState::Reset);
}

void Augmentinel::PlayTune(const std::wstring& filename)
{
	if (m_tunes_enabled)
		m_pAudio->Play(filename, AudioType::Tune);
}

void Augmentinel::PlayMusic()
{
	if (music_files.empty())
		return;

	auto prev_volume = m_music_volume;
	if (m_pView->InputAction(Action::ToggleTunes))
	{
		m_tunes_enabled = !m_tunes_enabled;
		SetSetting(TUNES_ENABLED_KEY, m_tunes_enabled);
	}
	else if (m_pView->InputAction(Action::ToggleMusic))
	{
		m_music_enabled = !m_music_enabled;
		SetSetting(MUSIC_ENABLED_KEY, m_music_enabled);

		if (!m_music_enabled)
			m_pAudio->Stop(AudioType::Music);
	}
	else if (m_pView->InputAction(Action::MusicVolumeUp))
		m_music_volume += VOLUME_STEP;
	else if (m_pView->InputAction(Action::MusicVolumeDown))
		m_music_volume -= VOLUME_STEP;

	m_music_volume = std::max(std::min(m_music_volume, 100), 0);

	if (prev_volume != m_music_volume)
	{
		m_pAudio->SetMusicVolume(m_music_volume / 100.0f);
		SetSetting(MUSIC_VOLUME_KEY, m_music_volume);
	}

	// Music plays if enabled, in a playing state, and when no tune is playing.
	auto playing = m_music_enabled && m_music_playing &&
		!m_pAudio->IsPlaying(AudioType::Tune);

	if (!m_pAudio->SetMusicPlaying(playing))
	{
		if (it_music != music_files.end())
			it_music++;

		if (it_music == music_files.end())
			it_music = music_files.begin();

		if (m_pAudio->Play(*it_music, AudioType::Music))
			m_pAudio->SetMusicVolume(m_music_volume / 100.0f);
		else
			it_music = music_files.erase(it_music);
	}
}

void Augmentinel::Render(IScene* pScene)
{
	// Draw game controllers (VR only).
	pScene->DrawControllers();

	// Show energy panel icons only in game mode.
	if (m_state == GameState::Game && !m_icons.empty())
	{
		auto cam_pos = m_pView->GetViewPosition();
		cam_pos.y += 0.9f;
		auto vPos = XMLoadFloat3(&cam_pos);

		auto cam_dir = m_pView->GetViewDirection();
		auto pitch_deg = XMConvertToDegrees(pitch_from_dir(cam_dir));
		cam_dir.y = 0.0f;
		auto vDir = XMVector3Normalize(XMLoadFloat3(&cam_dir));
		auto vRight = -XMVector3Cross(vDir, { 0.0f, 1.0f, 0.0f });

		constexpr auto icon_spacing = 0.15f;
		vPos += vDir * 1.5f;
		vPos -= vRight * ((m_icons.size() + 1) * icon_spacing / 2.0f);

		auto dissolved = std::min(std::max((pitch_deg + 25.0f) / 5.0f, 0.0f), 1.0f);

		for (auto& icon : m_icons)
		{
			if (m_pView->IsVR())
			{
				vPos += vRight * icon_spacing;
				XMStoreFloat3(&icon.pos, vPos);
				icon.rot.x = -XM_PIDIV4;
				icon.rot.y = yaw_from_dir(cam_dir);
				icon.scale = 0.1f;
				icon.dissolved = dissolved;
			}

			pScene->DrawModel(icon);
		}
	}

	if (m_landscape)
		pScene->DrawModel(m_landscape);

	// Draw any models placed on the landscape.
	for (auto& model : m_drawn_models)
	{
		// Don't draw the player model (except in sky view) as it blocks our view.
		if (m_player && model.id == m_player.id && m_state != GameState::SkyView)
			continue;

		// Draw models relative to the landscape.
		pScene->DrawModel(model, m_landscape);
	}

	// Landscape title text.
	for (auto& letter : m_text)
		pScene->DrawModel(letter);

	if (m_skybox)
		pScene->DrawModel(m_skybox);

	// Show aiming pointer only in game mode.
	if (m_state == GameState::Game)
	{
		float distance;
		XMVECTOR vRayPos, vRayDir, vNormal;
		m_pView->GetSelectionRay(vRayPos, vRayDir);

		RayTarget hit;
		if (SceneRayTest(vRayPos, vRayDir, hit, m_player.id))
		{
			distance = hit.distance;

			// Transform model surface normal to a world direction vector.
			auto normal = (*hit.model->m_pVertices)[(*hit.model->m_pIndices)[hit.index]].normal;
			vNormal = XMVector4Transform(XMLoadFloat3(&normal), hit.model->GetWorldMatrix());
		}
		else
		{
			distance = std::sqrt(2.0f * SENTINEL_MAP_SIZE * SENTINEL_MAP_SIZE);
			vNormal = -vRayDir;
		}

		XMFLOAT3 normal;
		XMStoreFloat3(&normal, vNormal);
		XMStoreFloat3(&m_pointer_target.pos, vRayPos + (vRayDir * distance));

		m_pointer_target.rot.x = pitch_from_dir(normal);
		m_pointer_target.rot.y = yaw_from_dir(normal);
		m_pointer_target.scale = std::tan(XM_PI / 8.0f) * distance * 0.002f * POINTER_SCALE;
		pScene->DrawModel(m_pointer_target);

		if (m_pView->IsPointerVisible())
		{
			auto& vertices = m_pointer_line.EditVertices();
			for (auto& v : vertices)
			{
				if (v.pos.z > FLT_EPSILON)
					v.pos.z = distance;
			}

			XMStoreFloat3(&m_pointer_line.pos, vRayPos);
			XMStoreFloat3(&normal, vRayDir);

			m_pointer_line.rot.x = pitch_from_dir(normal);
			m_pointer_line.rot.y = yaw_from_dir(normal);
			pScene->DrawModel(m_pointer_line);
		}
	}
}

void Augmentinel::Frame(float fElapsed)
{
	// Update music state and process music keys.
	PlayMusic();

	// Animate models before any processing.
	AnimateModels(m_animations, fElapsed, this);

	// Poll the state of all action bindings (VR only).
	m_pView->PollInputBindings(action_bindings);

#if false
	m_pView->ProcessDebugKeys();
#endif

	switch (m_state)
	{
	case GameState::Reset:
	{
		if (!m_pView->TransitionEffect(ViewEffect::Fade, 1.0f, fElapsed))
			break;

		SetSeen(SeenState::Unseen);
		m_pView->SetVerticalFOV(SENTINEL_VERT_FOV);

		m_animations = {};
		m_landscape = {};
		m_player = {};
		m_skybox = {};
		m_text = {};
		m_icons = {};

		// Load the Spectrum game snapshot into an emulation object.
		m_spectrum = std::move(std::make_unique<Spectrum>(SENTINEL_SNAPSHOT_FILE, this));

		// Limit the number of emulated frames to advance beyond reset state.
		if (!RunUntilStateChange())
			throw std::exception("Failed to reach title screen.\n\nSnapshot not saved at controls menu?");

		break;
	}

	case GameState::TitleScreen:
	{
		switch (m_substate)
		{
		case 0:
		{
			// Skip title screen?
			if (m_title_shown)
			{
				m_substate = 2;
				break;
			}

			// Only play a tune on the first visit to the title screen.
			static bool first_play = true;
			if (first_play)
			{
				m_pAudio->Play(TITLE_TUNE, AudioType::Tune);
				first_play = false;
			}

			// Extract  text as models.
			m_drawn_models = m_spectrum->ExtractText();	// "THE SENTINEL"

			m_pView->EnableFreeLook(false);
			m_pView->SetCameraPosition({ -11.63f, 57.8f, -61.5f });
			m_pView->SetCameraRotation({ 0.64f, 0.42f, 0.0f });

			m_pView->SetFillColour(DARK_BLUE_PALETTE_INDEX);
			m_pView->SetPalette(m_spectrum->GetGamePalette(2));
			m_pView->SetEffect(ViewEffect::ZFade, 0.01f);

			auto sentinel = m_spectrum->GetModel(ModelType::Sentinel);
			sentinel.pos = { -8.31f, 54.06f, -57.11f };
			sentinel.rot = { -0.47f, 4.16f, -0.31f };
			m_drawn_models.push_back(std::move(sentinel));

			auto pedestal = m_spectrum->GetModel(ModelType::Pedestal);
			pedestal.pos = { -8.5f, 53.19f, -57.58f };
			pedestal.rot = { -0.47f, 4.16f, -0.31f };
			m_drawn_models.push_back(std::move(pedestal));

			m_pView->SetEffect(ViewEffect::Fade, 0.0f);

			m_title_shown = true;
			m_substate++;
			break;
		}

		case 1:
			if (m_pView->InputAction(Action::Quit))
			{
				m_pView->SetEffect(ViewEffect::Fade, 1.0f);
				PostQuitMessage(0);
			}
			else if (m_pView->InputAction(Action::TitleContinue))
				m_substate++;

			break;

		case 2:
			// Fade out the title screen.
			if (!m_pView->TransitionEffect(ViewEffect::Fade, 1.0f, fElapsed))
				break;

			if (!RunUntilStateChange())
				throw std::exception("Failed to reach landscape preview.\n\nPlease report this bug!");

			break;
		}
		break;
	}

	case GameState::LandscapePreview:
	{
		switch (m_substate)
		{
		case 0:
		{
			m_rotate_landscape = GetFlag(L"RotateLandscape", m_rotate_landscape);

			m_landscape = m_spectrum->ExtractLandscape();
			m_drawn_models = m_spectrum->ExtractPlacedModels();

			// Remove trees and double size of humanoids.
			for (auto it = m_drawn_models.begin(); it != m_drawn_models.end(); )
			{
				auto& model = *it;
				switch (model.type)
				{
				case ModelType::Sentinel:
				case ModelType::Sentry:
				case ModelType::Robot:
					model.scale = 2.0f;			// double model size
					model.pos.y += EYE_HEIGHT;	// raise scaled model standing position
					break;
				case ModelType::Tree:
					it = m_drawn_models.erase(it);
					continue;
				default:
					break;
				}
				++it;
			}

			m_text.clear();

			m_pView->SetVerticalFOV(SENTINEL_VERT_FOV);
			m_pView->SetFillColour(BLACK_PALETTE_INDEX);
			m_pView->SetPalette(m_spectrum->GetGamePalette());
			m_pView->SetEffect(ViewEffect::Dissolve, 0.0f);
			m_pView->SetEffect(ViewEffect::Desaturate, 0.0f);
			m_pView->SetEffect(ViewEffect::FogDensity, 0.0f);
			m_pView->EnableAnimatedNoise(true);

			m_pView->EnableFreeLook(false);

			if (m_pView->IsVR())
			{
				m_pView->SetEffect(ViewEffect::ZFade, 0.03f);
				m_pView->SetCameraPosition({ 15.0f, 30.5f - 8.0f, -57.0f + 35.0f });
				m_pView->SetCameraRotation({ 0.0f, 0.0f, 0.0f });
			}
			else
			{
				m_pView->SetEffect(ViewEffect::ZFade, 0.02f);
				m_pView->SetCameraPosition({ 15.0f, 30.5f, -57.0f });
				m_pView->SetCameraRotation({ PitchToRadians(0xea), YawToRadians(0x00), 0.0f });
			}

			SaveLastLandscape(m_landscape_bcd);

			// Show the landscape number title text.
			std::stringstream ss;
			ss << "LANDSCAPE " << std::hex << std::uppercase << std::setw(4) << std::setfill('0') << m_landscape_bcd;
			AddText(ss.str(), 15.0f, 20.0f, -1.0f);

			// Shown left arrow if there is a previous landscape in the unlocked list.
			if (std::prev(m_codes.find(m_landscape_bcd)) != m_codes.end())
				AddText("<", 2.0f, 20.0f, -1.0f);

			// Shown right arrow if there is a next landscape in the unlocked list.
			if (std::next(m_codes.find(m_landscape_bcd)) != m_codes.end())
				AddText(">", 28.0f, 20.0f, -1.0f);

			// If the player can see this they're facing the wrong way!
			AddText("TURN AROUND", 15.0f, 20.0f, -60.0f, 14, true);

			m_substate++;
			break;
		}

		case 1:
		{
			// Rotate the landscape a step.
			if (m_rotate_landscape)
				m_landscape.rot.y += fElapsed / 8.0f;

			// Fade in landscape preview without delaying keyboard interaction.
			m_pView->TransitionEffect(ViewEffect::Fade, 0.0f, fElapsed);

			const auto it_current = m_codes.find(m_landscape_bcd);
			auto it_new = it_current;

			if (m_pView->InputAction(Action::LandscapePrev))
			{
				if (std::prev(it_new) != m_codes.end())
					it_new = std::prev(it_new);
			}
			else if (m_pView->InputAction(Action::LandscapeNext))
			{
				if (std::next(it_new) != m_codes.end())
					it_new = std::next(it_new);
			}
			else if (m_pView->InputAction(Action::LandscapePgUp))
			{
				for (size_t i = 0; i < m_codes.size() / PAGE_STEPS; ++i)
				{
					if (std::prev(it_new) != m_codes.end())
						it_new = std::prev(it_new);
					else
						break;
				}
			}
			else if (m_pView->InputAction(Action::LandscapePgDn))
			{
				for (size_t i = 0; i < m_codes.size() / PAGE_STEPS; ++i)
				{
					if (std::next(it_new) != m_codes.end())
						it_new = std::next(it_new);
					else
						break;
				}
			}
			else if (m_pView->InputAction(Action::LandscapeFirst))
			{
				it_new = m_codes.begin();
			}
			else if (m_pView->InputAction(Action::LandscapeLast))
			{
				it_new = std::prev(m_codes.end());
			}
			else if (m_pView->InputAction(Action::Quit))
			{
				m_title_shown = false;
				ChangeState(GameState::Reset);
				break;
			}
			else if (m_pView->InputAction(Action::LandscapeSelect))
			{
				m_substate++;
				break;
			}
			else if (m_pView->InputAction(Action::ResetHMD))
			{
				m_pView->ResetHMD(true);
				break;
			}
			else
			{
				break;
			}

			if (it_new != it_current)
			{
				m_landscape_bcd = it_new->first;
				ChangeState(GameState::Reset);
			}
			break;
		}
		case 2:
			if (!m_pView->TransitionEffect(ViewEffect::Fade, 1.0f, fElapsed))
				break;

			m_landscape.rot.y = 0.0f;

			if (!RunUntilStateChange())
				throw std::exception("Failed to reach main game.\n\nPlease report this bug with landscape number.");

			break;
		}
		break;
	}

	case GameState::Game:
	{
		switch (m_substate)
		{
		case 0:	// init main game
		{
			m_pView->SetFogColour(SKY_PALETTE_INDEX);
			m_pView->SetEffect(ViewEffect::FogDensity, 0.025f);
			m_pView->SetEffect(ViewEffect::Desaturate, 0.0f);
			m_pView->SetEffect(ViewEffect::Dissolve, 0.0f);
			m_pView->SetEffect(ViewEffect::ZFade, 0.0f);

			m_drawn_models = m_spectrum->ExtractPlacedModels();
			m_player = m_spectrum->ExtractPlayerModel();
			m_animations.clear();
			m_text.clear();

			// Create a coloured skybox centred around the landscape.
			m_skybox = Model::CreateBlock(200.0f, 200.0f, 200.0f, SKY_PALETTE_INDEX, ModelType::SkyBox);
			m_skybox.pos = m_landscape.pos;

			// Limit the camera pitch to what the original game uses.
			constexpr auto min_pitch = PitchToRadians(SENTINEL_MIN_PITCH);
			constexpr auto max_pitch = PitchToRadians(SENTINEL_MAX_PITCH);
			m_pView->SetPitchLimits(min_pitch, max_pitch);

			m_pView->EnableFreeLook(true);
			m_pView->SetCameraPosition(m_player.pos);
			m_pView->SetCameraRotation(m_player.rot);

			auto vertical_fov = GetSetting(VERTICAL_FOV_KEY, DEFAULT_VERTICAL_FOV);
			vertical_fov = std::min(std::max(vertical_fov, 15), 90);
			m_pView->SetVerticalFOV(static_cast<float>(vertical_fov));

			// Reset HMD position and world scale (VR only).
			m_pView->ResetHMD();

			SetSeen(SeenState::Unseen);
			m_substate++;
			break;
		}

		case 1:	// fade in to main game
			if (!m_pView->TransitionEffect(ViewEffect::Fade, 0.0f, fElapsed, 0.5f))
				break;

			m_substate++;
			break;

		case 2:	// main game
		{
			if (m_pView->IsSuspended())
			{
				SetSeen(SeenState::Unseen);
				break;
			}

			if (m_pView->InputAction(Action::Quit))
			{
				ChangeState(GameState::Reset);
				break;
			}
			else if (m_pView->InputAction(Action::Pause))
			{
				m_pView->EnableFreeLook(false);

				SetSeen(SeenState::Unseen);
				m_substate++;
				break;
			}
			else if (m_pView->InputAction(Action::ResetHMD))
			{
				m_pView->ResetHMD(true);
			}

			float rot_x{ 0.0f }, rot_y{ 0.0f };

			// Cursor key rotations roughly matching the original game.
			if (m_pView->InputAction(Action::TurnLeft))
				rot_y -= XMConvertToRadians(30);
			else if (m_pView->InputAction(Action::TurnRight))
				rot_y += XMConvertToRadians(30);
			else if (m_pView->InputAction(Action::TurnLeft45))
				rot_y -= XMConvertToRadians(45);
			else if (m_pView->InputAction(Action::TurnRight45))
				rot_y += XMConvertToRadians(45);
			else if (m_pView->InputAction(Action::TurnLeft90))
				rot_y -= XMConvertToRadians(90);
			else if (m_pView->InputAction(Action::TurnRight90))
				rot_y += XMConvertToRadians(90);
			else if (m_pView->InputAction(Action::Turn180))
				rot_y += XMConvertToRadians(180);
			else if (m_pView->InputAction(Action::LookUp))
				rot_x -= XMConvertToRadians(10);
			else if (m_pView->InputAction(Action::LookDown))
				rot_x += XMConvertToRadians(10);

			if (rot_x || rot_y)
			{
				auto rot = m_pView->GetCameraRotation();
				rot.x += rot_x;
				rot.y += rot_y;
				m_pView->SetCameraRotation(rot);
			}

			// Remove any objects that fade been faded out of existence.
			m_drawn_models.erase(std::remove_if(m_drawn_models.begin(), m_drawn_models.end(), [](auto& m) {
				return m.dissolved == 1.0f;
				}), m_drawn_models.end());

			// Run the Spectrum game if there are no active dissolve animations.
			if (!PlayerAnimationActive())
				m_spectrum->RunFrame(false);

			static float total_elapsed = 0.0f;
			total_elapsed += fElapsed;

			// Run the Spectrum interrupt handler if it's due. This advances the Spectrum
			// game timers used for various game events.
			if (total_elapsed >= m_frame_time)
			{
				m_spectrum->RunInterrupt();
				total_elapsed -= m_frame_time;
			}

			// Require the seen state to persist for a certain number of
			// frames before we trust acting on it, with sound/vision.
			auto seen_state = m_spectrum->GetPlayerSeenState();
			if (seen_state != SeenState::Unseen)
			{
				if (++m_seen_count > SEEN_FRAME_THRESHOLD)
					m_seen_count = SEEN_FRAME_THRESHOLD;
			}
			else if (--m_seen_count < -SEEN_FRAME_THRESHOLD)
				m_seen_count = -SEEN_FRAME_THRESHOLD;

			// Seen state change? (after thresholding)
			if ((m_seen_count == SEEN_FRAME_THRESHOLD && !m_seen_sound) ||
				(m_seen_count == -SEEN_FRAME_THRESHOLD && m_seen_sound))
			{
				SetSeen(seen_state);
			}

			if (m_seen_sound)
			{
				static float next_haptic_time = 0.0f;
				next_haptic_time -= fElapsed;

				// Haptic pulse due?
				if (next_haptic_time < 0.0f)
				{
					m_pView->OutputAction(Action::Haptic_Seen);
					next_haptic_time += SEEN_HAPTIC_FREQ;
				}
			}
			break;
		}

		case 3:	// paused
			if (!m_pView->TransitionEffect(ViewEffect::Fade, 0.5f, fElapsed, 0.5f))
				break;

			if (m_pView->InputAction(Action::Pause))
			{
				// Enable mouse, then fade in to continue game.
				m_pView->EnableFreeLook(true);
				m_substate = 1;
				break;
			}
			break;
		}

		break;
	}

	case GameState::SkyView:
	{
		switch (m_substate)
		{
		case 0:
			if (!m_pView->TransitionEffect(ViewEffect::Fade, 1.0f, fElapsed))
				break;

			SetSeen(SeenState::Unseen);
			m_pView->SetVerticalFOV(SENTINEL_VERT_FOV);
			m_substate++;
			break;

		case 1:
		{
			PlayTune(UTURN_TUNE);

			// Extract player model with current facing direction, but clear any pitch.
			m_player = m_spectrum->ExtractPlayerModel();
			m_player.rot.x = 0.0f;

			XMFLOAT3 ray_pos{}, ray_dir{};
			XMVECTOR vRayPos, vRayDir;
			m_pView->GetSelectionRay(vRayPos, vRayDir);

			// Look in the ray direction for the view distance, and then back at the player.
			auto view_distance = m_pView->IsVR() ? SKY_VIEW_DISTANCE_VR : SKY_VIEW_DISTANCE;
			vRayPos += (vRayDir * view_distance);
			vRayDir = -vRayDir;

			XMStoreFloat3(&ray_pos, vRayPos);
			XMStoreFloat3(&ray_dir, vRayDir);

			m_pView->SetPitchLimits(-XM_PI, XM_PI);
			m_pView->SetCameraPosition(ray_pos);
			m_pView->SetCameraRotation({ pitch_from_dir(ray_dir), yaw_from_dir(ray_dir), 0.0f });
			m_pView->SetEffect(ViewEffect::FogDensity, 0.0125f);

			m_substate++;
			break;
		}

		case 2:
			if (!m_pView->TransitionEffect(ViewEffect::Fade, 0.0f, fElapsed, 0.5f))
				break;

			if (m_pView->InputAction(Action::SkyViewContinue))
				m_substate++;

			break;

		case 3:
			if (!m_pView->TransitionEffect(ViewEffect::Fade, 1.0f, fElapsed))
				break;

			ChangeState(GameState::Game);
			break;
		}

		break;
	}

	case GameState::PlayerDead:
	{
		switch (m_substate)
		{
		case 0:
			m_pAudio->Stop(AudioType::LoopingEffect);
			m_pAudio->Play(DISINTEGRATE_SOUND);
			m_pView->OutputAction(Action::Haptic_Dead);

			m_pView->EnableFreeLook(false);
			m_pView->EnableAnimatedNoise(m_pView->IsVR());

			m_substate++;
			break;

		case 1:
		{
			// Fade to black in VR rather than disintegrate.
			auto effect = m_pView->IsVR() ? ViewEffect::Fade : ViewEffect::Dissolve;
			if (!m_pView->TransitionEffect(effect, 1.0f, fElapsed, 3.0f))
				break;

			m_landscape = {};
			m_skybox = {};

			m_drawn_models = { m_spectrum->GetModel(1, true) };
			m_player = m_spectrum->GetModel(2, true);

			m_pView->SetCameraPosition(m_player.pos);
			m_pView->SetCameraRotation({ PitchToRadians(0xf4), 0.0f, 0.0f });
			m_pView->SetVerticalFOV(SENTINEL_VERT_FOV);

			if (m_pView->IsVR())
			{
				// Move killer closer (due to wider field of view), and down (due to the
				// lack of camera view pitch control in VR).
				m_drawn_models[0].pos.y -= 0.2f;
				m_drawn_models[0].pos.z -= 2.0f;

				// Determine how much the VR view is rotated compared to the camera,
				// and adjust the view so the killer is shown in front of the player.
				m_pView->SetCameraRotation({ 0.0f, 0.0f, 0.0f });
				auto view_yaw = yaw_from_dir(m_pView->GetViewDirection());
				m_pView->SetCameraRotation({ 0.0f, -view_yaw, 0.0f });
			}

			m_pView->SetEffect(ViewEffect::Fade, 1.0f);
			m_pView->SetEffect(ViewEffect::Dissolve, 0.0f);
			m_pView->SetEffect(ViewEffect::Desaturate, 0.0f);

			m_substate++;
			break;
		}

		default:
			m_spectrum->RunFrame();
			break;
		}

		break;
	}

	case GameState::ShowKiller:
	{
		constexpr auto min_fade = 0.4f;

		switch (m_substate)
		{
		case 0:	// fade in from black
			if (m_pView->TransitionEffect(ViewEffect::Fade, 1.0f - min_fade, fElapsed, 2.0f / min_fade))
				m_substate++;
			break;

		case 1:	// slow fade out to black
			if (m_pView->TransitionEffect(ViewEffect::Fade, 0.99f, fElapsed, 4.0f / min_fade))
				m_substate++;
			break;

		case 2:
			// slight pause on black
			if (m_pView->TransitionEffect(ViewEffect::Fade, 1.0f, fElapsed, 1.0f / 0.01f))
				m_substate++;
			break;
		default:
			ChangeState(GameState::Reset);
			break;
		}

		break;
	}

	case GameState::Complete:
	{
		// Get the new landscape number and its secret code.
		uint32_t secret_code_bcd{};
		m_spectrum->GetLandscapeAndCode(m_landscape_bcd, secret_code_bcd);

		// Store them in the settings file as a future selectable landscape.
		AddLandscapeCode(m_landscape_bcd, secret_code_bcd);

		// Reset the game back to preview the new landscape.
		ChangeState(GameState::Reset);
		break;
	}

	case GameState::Unknown:
		DebugBreak();
		break;
	}

	m_pAudio->PositionListener(
		m_pView->GetViewPosition(),
		m_pView->GetViewDirection(),
		m_pView->GetUpDirection());
}

void Augmentinel::AddText(const std::string& str, float x_centre, float y, float z, int colour, bool reversed)
{
	constexpr auto spacing = 1.4f;
	constexpr auto scale = 1.8f;

	auto str_size = str.length() * spacing;
	auto x_sign = reversed ? -1 : 1;
	auto x = x_centre - (x_sign * str_size / 2.0f);
	auto yaw = reversed ? XM_PI : 0.0f;

	for (auto ch : str)
	{
		if (ch != ' ')
		{
			if (ch == '0')
				ch = 'O';

			auto model = m_spectrum->CharToModel(ch, colour);
			model.pos = { x, y, z };
			model.rot.y = yaw;
			model.scale = scale;
			m_text.push_back(std::move(model));
		}

		x += spacing * x_sign;
	}
}

bool Augmentinel::PlayerAnimationActive() const
{
	return std::any_of(m_animations.begin(), m_animations.end(), [](const Animation& ani) {
		return ani.type == AnimationType::Dissolve && !ani.interruptible;
		});
}

void Augmentinel::SetSeen(SeenState seen_state)
{
	bool seen = seen_state != SeenState::Unseen;
	if (seen)
	{
		m_pAudio->Play(SEEN_SOUND, AudioType::LoopingEffect);
		m_seen_sound = true;

		auto dissolve = m_pView->IsVR() ? 0.05f : 0.1f;
		m_pView->SetEffect(ViewEffect::Dissolve, (seen_state == SeenState::FullSeen) ? dissolve : 0.0f);
		m_pView->SetEffect(ViewEffect::Desaturate, (seen_state == SeenState::FullSeen) ? 0.7f : 0.3f);
	}
	else
	{
		m_pAudio->Stop(AudioType::LoopingEffect);
		m_seen_sound = false;

		m_pView->SetEffect(ViewEffect::Desaturate, 0.0f);
		m_pView->SetEffect(ViewEffect::Dissolve, 0.0f);
	}
}

void Augmentinel::LoadLandscapeCodes()
{
	m_codes.clear();

	// Add the secret code for landscape 0000.
	m_codes[0x0000] = SPECTRUM_LANDSCAPE_0000_CODE;

	for (auto& landscape_key : GetSettingKeys(LANDSCAPES_SECTION))
	{
		auto landscape_bcd = std::stoul(landscape_key.c_str(), nullptr, 16);
		auto secret_code_bcd = std::stoul(GetSetting(landscape_key, L"0", LANDSCAPES_SECTION), nullptr, 16);
		m_codes[landscape_bcd] = secret_code_bcd;
	}

	auto last_landscape_bcd = GetSetting(LAST_LANDSCAPE_KEY, L"0");
	m_landscape_bcd = std::stoul(last_landscape_bcd, nullptr, 16);

	// If the last landscape isn't valid, use 0000.
	if (m_codes.find(m_landscape_bcd) == m_codes.end())
		m_landscape_bcd = 0;
}

void Augmentinel::SaveLastLandscape(int landscape_bcd)
{
	std::wstringstream ss_landscape;
	ss_landscape << std::hex << std::uppercase << std::setw(4) << std::setfill(L'0') << landscape_bcd;

	SetSetting(LAST_LANDSCAPE_KEY, ss_landscape.str());
}

void Augmentinel::AddLandscapeCode(int landscape_bcd, uint32_t secret_code_bcd)
{
	m_codes[landscape_bcd] = secret_code_bcd;

	std::wstringstream ss_landscape;
	ss_landscape << std::hex << std::uppercase << std::setw(4) << std::setfill(L'0') << landscape_bcd;

	std::wstringstream ss_secret_code;
	ss_secret_code << std::hex << std::setw(8) << std::setfill(L'0') << secret_code_bcd;

	SetSetting(ss_landscape.str(), ss_secret_code.str(), LANDSCAPES_SECTION);
}

void Augmentinel::RemoveLandscapeCode(int landscape_bcd)
{
	m_codes.erase(landscape_bcd);

	std::wstringstream ss_landscape;
	ss_landscape << std::hex << std::uppercase << std::setw(4) << std::setfill(L'0') << landscape_bcd;

	RemoveSetting(ss_landscape.str(), LANDSCAPES_SECTION);
}

bool Augmentinel::SceneRayTest(XMVECTOR vRayPos, XMVECTOR vRayDir, RayTarget& hit, int ignore_id)
{
	std::map<float, RayTarget> hits;

	if (m_landscape.RayTest(vRayPos, vRayDir, hit))
		hits[hit.distance] = hit;

	for (auto& model : m_drawn_models)
	{
		if (model.id >= TEMP_ID_BASE || ignore_id >= 0 && model.id == ignore_id)
			continue;

		if (model.RayTest(vRayPos, vRayDir, hit))
			hits[hit.distance] = hit;
	}

	if (!hits.empty())
	{
		// First item is closest, due to map key (distance) sorting.
		hit = hits.begin()->second;
		return true;
	}

	return false;
}

bool Augmentinel::SceneModelVisible(const XMVECTOR vRayPos, const Model& model, int ignore_id)
{
	struct CornerRay
	{
		XMVECTOR dir;
		float distance;
		uint32_t index;
	};

	std::vector<CornerRay> corner_rays;

	// Cast rays from the camera to each vertex of the bounding box.
	for (auto& corner : model.GetBoundingBox())
	{
		// Calculate the vector to the corner vertex in world space, and its unit direction vector.
		auto vRayCorner = corner - vRayPos;
		auto vRayDir = XMVector3Normalize(vRayCorner);

		// Calculate the distance from camera to corner.
		float corner_dist;
		XMStoreFloat(&corner_dist, XMVector3Length(vRayCorner));

		// Test if/where the ray intercepts the landscape.
		RayTarget hit;
		if (!m_landscape.RayTest(vRayPos, vRayDir, hit) || hit.distance > corner_dist)
		{
			// The ray didn't hit the landscape, or it hit the landscape behind the bounding box.
			corner_rays.push_back({ vRayDir, corner_dist, 0 });
		}
	}

	// If no rays reached the bounding box, the model is hidden behind the landscape.
	if (corner_rays.empty())
		return false;

	std::set<Model*> ray_hits;
	for (auto& m : m_drawn_models)
	{
		// Ignore the model we're testing against, and any supplied id.
		if (m.id == model.id || (ignore_id >= 0 && m.id == ignore_id))
			continue;

		// Test the remaining rays against the bounding box for each model.
		for (auto& ray : corner_rays)
		{
			float dist;

			// Does the ray touch the bounding box closer than the target?
			if (m.BoxTest(vRayPos, ray.dir, dist) && dist < ray.distance)
			{
				// This model is a candidate for detailed polygon testing.
				ray_hits.insert(&m);
				break;
			}
		}
	}

	auto mModelWorld = model.GetWorldMatrix();

	// Test the visibility of each vertex in the target model.
	for (auto& vertex : *(model.m_pVertices))
	{
		XMVECTOR vVertex{ vertex.pos.x, vertex.pos.y, vertex.pos.z, 1.0f };
		auto vVertexWorld = XMVector4Transform(vVertex, mModelWorld);

		auto vRayVertex = vVertexWorld - vRayPos;
		auto vRayDir = XMVector3Normalize(vRayVertex);

		float vertex_dist{};
		XMStoreFloat(&vertex_dist, XMVector3Length(vRayVertex));

		float closest_dist{ vertex_dist };
		RayTarget hit;

		// Detailed test against each tile in the landscape.
		if (m_landscape.RayTest(vRayPos, vRayDir, hit))
			closest_dist = std::min(hit.distance, closest_dist);

		// Detailed test against any models that may obscure the target.
		for (auto pModel : ray_hits)
		{
			if (pModel->RayTest(vRayPos, vRayDir, hit))
				closest_dist = std::min(hit.distance, closest_dist);
		}

		// If a ray reached the target vertex the model is visible.
		if (closest_dist >= vertex_dist)
			return true;
	}

	return false;
}

bool Augmentinel::SceneTileVisible(const XMVECTOR vRayPos, int tile_x, int tile_z)
{
	auto world_corners = m_landscape.GetTileCorners(tile_x, tile_z);

	XMFLOAT3 eye_pos{};
	XMStoreFloat3(&eye_pos, vRayPos);

	std::array<XMFLOAT3, 4> corner_pos;
	for (size_t i = 0; i < world_corners.size(); ++i)
	{
		XMStoreFloat3(&corner_pos[i], world_corners[i]);

		// Reject non-flat tiles, and those at/above eye height.
		if (corner_pos[i].y != corner_pos[0].y || corner_pos[i].y >= eye_pos.y)
			return false;
	}

	constexpr auto NUM_GRID_VERTICES = TILE_AXIS_SAMPLES * TILE_AXIS_SAMPLES;
	std::array<XMVECTOR, NUM_GRID_VERTICES> grid_vertices{};

	// Scan across the tile surface in a grid pattern.
	for (int z = 0; z < TILE_AXIS_SAMPLES; ++z)
	{
		for (int x = 0; x < TILE_AXIS_SAMPLES; ++x)
		{
			constexpr auto step = 1.0f / (TILE_AXIS_SAMPLES - 1);
			auto idx = (z * TILE_AXIS_SAMPLES) + x;

			const auto& [min_x, y, min_z] = corner_pos[0];
			grid_vertices[idx] = { min_x + step * x, y, min_z + step * z, 1.0f };
		}
	}

	// Cast rays from the camera to each grid position on the tile.
	for (auto& v : grid_vertices)
	{
		// Calculate the vector to the corner vertex in world space, and its unit direction vector.
		auto vRay = v - vRayPos;
		auto vRayDir = XMVector3Normalize(vRay);

		// Calculate the distance from camera to corner.
		float vertex_dist;
		XMStoreFloat(&vertex_dist, XMVector3Length(vRay));

		// It's visible if we don't hit another part of the landscape before the tile corner,
		// even if we miss the corner due to floating point precision errors.
		RayTarget hit;
		if (!m_landscape.RayTest(vRayPos, vRayDir, hit) || hit.distance > (vertex_dist - 0.001f))
			return true;
	}

	// Tile not visible.
	return false;
}

Model* Augmentinel::FindModelById(int id)
{
	auto it = std::find_if(m_drawn_models.begin(), m_drawn_models.end(), [&](auto& m) {
		return m.id == id;
		});

	return (it != m_drawn_models.end()) ? &(*it) : nullptr;
}

std::vector<Model> Augmentinel::GetModelStack(int tile_x, int tile_z)
{
	std::vector<Model> models;

	std::copy_if(m_drawn_models.begin(), m_drawn_models.end(),
		std::back_inserter(models), [&](auto& m) {
			return m.id < TEMP_ID_BASE&&
				static_cast<int>(m.pos.x) == tile_x &&
				static_cast<int>(m.pos.z) == tile_z;
			});

	std::sort(models.begin(), models.end(),
		[](const auto& a, const auto& b) {
			return a.pos.y > b.pos.y;
			});

	return models;
}

////////////////////////////////////////////////////////////////////////////////

void Augmentinel::ChangeState(GameState new_state)
{
	m_state = new_state;
	m_substate = 0;
}

bool Augmentinel::RunUntilStateChange()
{
	auto current_state = m_state;
	auto frame_count = MAX_STATE_FRAMES;

	while (m_state == current_state && frame_count-- > 0)
		m_spectrum->RunFrame();

	return frame_count > 0;
}

////////////////////////////////////////////////////////////////////////////////
// ISentinelEvents overrides.

void Augmentinel::OnTitleScreen()
{
	ChangeState(GameState::TitleScreen);
}

void Augmentinel::OnLandscapeInput(int& landscape_bcd, uint32_t& secret_code_bcd)
{
	landscape_bcd = m_landscape_bcd;
	secret_code_bcd = m_codes[landscape_bcd];
}

void Augmentinel::OnLandscapeGenerated()
{
	ChangeState(GameState::LandscapePreview);
	m_music_playing = true;
}

void Augmentinel::OnNewPlayerView()
{
	ChangeState(GameState::Game);
	m_music_playing = true;
}

void Augmentinel::OnPlayerDead()
{
	ChangeState(GameState::PlayerDead);
	m_music_playing = false;
}

void Augmentinel::OnInputAction(uint8_t& action)
{
	// Return if any player animations are still active.
	if (PlayerAnimationActive())
		return;

	XMFLOAT3 ray_dir{};
	XMVECTOR vRayPos, vRayDir;
	m_pView->GetSelectionRay(vRayPos, vRayDir);
	XMStoreFloat3(&ray_dir, vRayDir);

	// Break ray direction into yaw and pitch.
	auto ray_pitch = pitch_from_dir(ray_dir);
	auto ray_yaw = yaw_from_dir(ray_dir);

	// Update player yaw and pitch in emulated game.
	m_spectrum->SetPlayerYaw(ray_yaw);
	m_spectrum->SetPlayerPitch(ray_pitch);

	if (m_pView->InputAction(Action::Robot))
	{
		// Is the selection target pitched at a steep enough angle to consider?
		if (XMConvertToDegrees(ray_pitch) < -SKY_VIEW_ANGLE)
		{
			// If nothing is targetted (i.e. sky) change to sky view.
			RayTarget hit;
			if (!SceneRayTest(vRayPos, vRayDir, hit, m_player.id))
			{
				ChangeState(GameState::SkyView);
				return;
			}
		}

		action = 0x00;	// create robot
	}
	else if (m_pView->InputAction(Action::Tree))
		action = 0x02;	// create tree
	else if (m_pView->InputAction(Action::Boulder))
		action = 0x03;	// create boulder
	else if (m_pView->InputAction(Action::Absorb))
		action = 0x20;	// absorb
	else if (m_pView->InputAction(Action::Transfer))
	{
		action = 0x21;	// transfer

		// Is the selection target pitched at a steep enough angle to consider?
		if (XMConvertToDegrees(ray_pitch) < -HYPERSPACE_ANGLE)
		{
			// If nothing is targetted (i.e. sky) change the action to hyperspace.
			RayTarget hit;
			if (!SceneRayTest(vRayPos, vRayDir, hit, m_player.id))
				action = 0x22;
		}
	}
	else if (m_pView->InputAction(Action::Hyperspace))
		action = 0x22;	// hyperspace
	else if (m_pView->InputAction(Action::U_Turn))
		action = 0x23;	// u-turn
}

void Augmentinel::OnGameModelChanged(int id, bool player_initiated)
{
	// Temporary ids used for destroyed or changed models.
	static int fade_out_id = TEMP_ID_BASE;

	if (m_state != GameState::Game)
		return;

	auto new_model = m_spectrum->GetModel(id);
	auto existing_model = FindModelById(id);

	// Model removed?
	if (existing_model && new_model.type == ModelType::Unknown)
	{
		m_pAudio->Play(DISSOLVE_SOUND, AudioType::Effect, existing_model->pos);

		// Change the id of the destroyed model so the slot can be reused.
		existing_model->id = fade_out_id++;

		m_animations.push_back({
			AnimationType::Dissolve,
			existing_model->id,
			DISSOLVE_TIME,
			0.0f,
			1.0f,
			!player_initiated
			});
	}
	else
	{
		// Model added?
		if (!existing_model)
		{
			m_pAudio->Play(DISSOLVE_SOUND, AudioType::Effect, new_model.pos);

			new_model.dissolved = 1.0f;
			m_drawn_models.push_back(std::move(new_model));

			m_animations.push_back({
				AnimationType::Dissolve,
				id,
				DISSOLVE_TIME,
				1.0f,
				0.0f,
				!player_initiated
				});
		}
		else
		{
			if (new_model.type != existing_model->type)
			{
				// Change the id of the old model so the slot can be reused.
				existing_model->id = fade_out_id++;

				m_animations.push_back({
					AnimationType::Dissolve,
					existing_model->id,
					DISSOLVE_TIME,
					0.0f,
					1.0f,
					!player_initiated
					});

				m_pAudio->Play(DISSOLVE_SOUND, AudioType::Effect, new_model.pos);

				// Append the new model, initially faded out.
				new_model.dissolved = 1.0f;
				m_drawn_models.push_back(std::move(new_model));

				m_animations.push_back({
					AnimationType::Dissolve,
					id,
					DISSOLVE_TIME,
					1.0f,
					0.0f,
					!player_initiated
					});
			}
			// Model rotated?
			else if (new_model.rot.y != existing_model->rot.y)
			{
				// Ensure wrapping angles take the shortest route to the new position.
				auto current_rot_y = existing_model->rot.y;
				if ((new_model.rot.y - current_rot_y) >= XM_PI)
					current_rot_y += XM_2PI;
				else if ((current_rot_y - new_model.rot.y) >= XM_PI)
					current_rot_y -= XM_2PI;

				m_animations.push_back({
					AnimationType::Yaw,
					id,
					SENTINEL_TURN_TIME,
					current_rot_y,
					new_model.rot.y
					});
			}
		}
	}
}

std::pair<float, float> LandscapeSlopeUpOffset(const XMFLOAT3& v1, const XMFLOAT3& v2, const XMFLOAT3& v3)
{
	float dx = 0, dz = 0;
	std::array<XMFLOAT3, 3> tri{ v1, v2, v3 };
	std::sort(tri.begin(), tri.end(), [](auto& a, auto& b) { return a.y < b.y; });

	if (tri[1].y != tri[2].y)	// tri[2] at top
	{
		auto align0 = tri[2].x == tri[0].x || tri[2].z == tri[0].z;
		auto align1 = tri[2].x == tri[1].x || tri[2].z == tri[1].z;

		if (align0 && align1)
		{
			dx = tri[2].x - ((tri[0].x + tri[1].x) / 2);
			dz = tri[2].z - ((tri[0].z + tri[1].z) / 2);
		}
		else
		{
			auto& bot = align0 ? tri[0] : tri[1];
			dx = tri[2].x - bot.x;
			dz = tri[2].z - bot.z;
		}
	}
	else // tri[1] and tri[2] at top
	{
		auto align1 = tri[0].x == tri[1].x || tri[0].z == tri[1].z;
		auto align2 = tri[0].x == tri[2].x || tri[0].z == tri[2].z;

		if (align1 && align2)
		{
			dx = ((tri[1].x + tri[2].x) / 2) - tri[0].x;
			dz = ((tri[1].z + tri[2].z) / 2) - tri[0].z;
		}
		else
		{
			auto& top = align1 ? tri[1] : tri[2];
			dx = top.x - tri[0].x;
			dz = top.z - tri[0].z;
		}
	}

	return std::make_pair(dx, dz);
}

bool Augmentinel::OnTargetActionTile(InputAction action, int& tile_x, int& tile_z)
{
	bool allow{ false };

	// Final visibility is performed against the player position, which
	// differs from the selection ray position when playing in VR.
	XMVECTOR vPlayerPos{ m_player.pos.x, m_player.pos.y, m_player.pos.z, 1.0f };

	XMVECTOR vRayPos, vRayDir;
	m_pView->GetSelectionRay(vRayPos, vRayDir);

	RayTarget hit;
	if (SceneRayTest(vRayPos, vRayDir, hit, m_player.id))
	{
		auto model = hit.model;

		auto player_x = static_cast<int>(m_player.pos.x);
		auto player_z = static_cast<int>(m_player.pos.z);
		tile_x = static_cast<int>(model->pos.x);
		tile_z = static_cast<int>(model->pos.z);

		// Reject objects at the player position (only possible in VR).
		if (model->type != ModelType::Landscape && tile_x == player_x && tile_z == player_z)
			return false;

		switch (model->type)
		{
		case ModelType::Landscape:
		{
			// Get the index of the triangle and it's map location.
			auto vertex_index = hit.index;
			auto tri_index = (vertex_index % ZX_VERTICES_PER_TILE) / 3;
			vertex_index -= (vertex_index % ZX_VERTICES_PER_TILE);
			m_spectrum->LandscapeVertexIndexToTile(static_cast<int>(vertex_index), tile_x, tile_z);

			// Get the vertices of the triangle.
			auto& vertices = *(model->m_pVertices);
			auto& indices = *(model->m_pIndices);

			auto& v1 = vertices[indices[vertex_index + 0]].pos;
			auto& v2 = vertices[indices[vertex_index + 1]].pos;
			auto& v3 = vertices[indices[vertex_index + 2]].pos;

			// If the tile isn't level, the player may have selected a neighbouring slope.
			// Move the selection up the slope to help targetting distant tiles in VR.
			if (v1.y != v2.y || v2.y != v3.y)
			{
				auto& v4 = vertices[indices[vertex_index + 3]].pos;
				auto& v5 = vertices[indices[vertex_index + 4]].pos;
				auto& v6 = vertices[indices[vertex_index + 5]].pos;

				float dx = 0.0f, dz = 0.0f;
				auto [dx0, dz0] = LandscapeSlopeUpOffset(v1, v2, v3);
				auto [dx1, dz1] = LandscapeSlopeUpOffset(v4, v5, v6);

				switch (m_spectrum->GetTileShape(tile_x, tile_z))
				{
				case 0b0000:	// flat
				case 0b1000:	// unused
					break;

				case 0b0001:	// slope facing back
				case 0b0101:	// slope facing right
				case 0b1001:	// slope facing front
				case 0b1101:	// slope facing left
					dx = dx0;
					dz = dz0;
					break;

				case 0b0010:	// inside corner, facing front
				case 0b0011:	// inside corner, facing back
				case 0b0111:	// inside corner, facing front right
				case 0b1111:	// inside edge, facing back left
					dx = tri_index ? dx1 : dx0;
					dz = tri_index ? dz1 : dz0;
					break;

				case 0b1010:	// outside corner, facing front left
				case 0b1011:	// outside corner, facing back right
				case 0b0110:	// outside corner, facing front right
				case 0b1110:	// outside edge, facing back left
				case 0b1100:	// flat diagonal diamond
				case 0b0100:	// stretched faces
					dx = dx0 + dx1;
					dz = dz0 + dz1;
					break;
				}

				tile_x += (dx > 0) ? 1 : (dx < 0) ? -1 : 0;
				tile_z += (dz > 0) ? 1 : (dz < 0) ? -1 : 0;

				tile_x = std::max(0, std::min(tile_x, SENTINEL_MAP_SIZE - 2));
				tile_z = std::max(0, std::min(tile_z, SENTINEL_MAP_SIZE - 2));
			}

			// Calculate the map location of the hit.
			if (m_spectrum->GetTileShape(tile_x, tile_z) != 0)
				return false;

			// Reject the player tile so we can't absorb ourself!
			if (tile_x == player_x && tile_z == player_z)
				return false;

			// Reject access to the pedestal tile or we can t absorb the Sentinel too early.
			// The original game doesn't check this because the pedestal hides the tile.
			auto stack = GetModelStack(tile_x, tile_z);
			if (!stack.empty() && stack.back().type == ModelType::Pedestal)
				return false;

			// The player must also be able to see the tile.
			return SceneTileVisible(vPlayerPos, tile_x, tile_z);
		}

		case ModelType::Robot:
		case ModelType::Sentry:
		case ModelType::Tree:
		case ModelType::Meanie:
		case ModelType::Sentinel:
		{
			// Get the stack of models on the target tile, highest first.
			auto models = GetModelStack(tile_x, tile_z);

			// A single model on the tile?
			if (models.size() == 1)
			{
				assert(models[0].id == model->id);
				assert(models[0].type == model->type);

				// The player must be able to see the tile it's standing on.
				allow = SceneTileVisible(vPlayerPos, tile_x, tile_z);
			}
			else
			{
				assert(models[0].id == model->id);
				assert(models[0].type == model->type);

				// Check what the top-most item is standing on.
				switch (models[1].type)
				{
				case ModelType::Boulder:
					// Allow if any part of the underlying boulder is visible to the player.
					allow = SceneModelVisible(vPlayerPos, models[1], m_player.id);
					break;
				case ModelType::Pedestal:
					// Allow if player view is above pedestal (top surface is visible).
					allow = m_player.pos.y > models[1].pos.y;
					break;
				default:
					allow = false;
					break;
				}
			}
			break;
		}

		case ModelType::Boulder:
		{
			// The original game requires we're pointing at the top-most
			// boulder for interaction. We allow any boulder in the stack.
			allow = true;
			break;
		}

		case ModelType::Pedestal:
		{
			// Block placing a tree on the pedestal, as there's no way to remove
			// it to complete the landscape after the Sentinel has been absorbed.
			if (action == InputAction::CreateTree)
				break;

			// Allow if player view is above pedestal (top surface is visible).
			allow = m_player.pos.y > model->pos.y;
			break;
		}

		default:
			allow = false;
			break;
		}
	}

	return allow;
}

void Augmentinel::OnHideEnergyPanel()
{
	m_icons.clear();
}

void Augmentinel::OnAddEnergySymbol(int symbol_idx, int x_offset)
{
	static constexpr auto x_base = -10.0f;
	static constexpr auto y = 970.0f;
	static constexpr auto z = FAR_CLIP / 2.0f;
	static constexpr auto scale = 30.0f;
	static constexpr auto spacing = 20.0f;

	// Clear existing icons if the panel is being redrawn.
	if (x_offset == 0)
		m_icons.clear();

	auto colour_idx = -1;
	switch (symbol_idx)
	{
	case 1:					// robot
		colour_idx = 12;	// light blue
		break;
	case 2:					// tree
		colour_idx = 5;		// green
		break;
	case 4:					// boulder
		colour_idx = 11;	// cyan
		break;
	case 6:					// golden robot
		colour_idx = 9;		// yellow
		break;
	}

	if (colour_idx >= 0)
	{
		auto icon = m_spectrum->IconToModel(symbol_idx, colour_idx);

		if (!m_pView->IsVR())
		{
			icon.pos = { x_base + spacing * x_offset, y, z };
			icon.scale = scale;
			icon.orthographic = true;
			icon.lighting = false;
		}

		m_icons.push_back(std::move(icon));
	}
}

void Augmentinel::OnPlayTune(int tune_number)
{
	bool blank_view = false;

	// Tunes are played at breaks in play, so no sound effects should be playing.
	m_pAudio->Stop(AudioType::Effect);
	m_pAudio->Stop(AudioType::LoopingEffect);

	switch (tune_number)
	{
	case 0x00:	// Hyperspace
		PlayTune(HYPERSPACE_TUNE);
		blank_view = true;
		m_music_playing = !m_tunes_enabled;
		break;
	case 0x19:	// Robot transfer
		PlayTune(TRANSFER_TUNE);
		blank_view = true;
		break;
	case 0x20:	// U-turn
		PlayTune(UTURN_TUNE);
		blank_view = true;
		break;
	case 0x32:	// Game Over
		ChangeState(GameState::ShowKiller);
		m_pAudio->Play(GAMEOVER_TUNE, AudioType::Tune);	// always played
		break;
	case 0x42:	// Landscape complete
		ChangeState(GameState::Complete);
		PlayTune(COMPLETE_TUNE);
		break;
	default:
		DebugBreak();
		break;
	}

	// Some actions blank the view. Mouse movement should be ignored too.
	if (blank_view)
	{
		m_pView->SetEffect(ViewEffect::Fade, 1.0f);
		m_pView->EnableFreeLook(false);
	}
}

void Augmentinel::OnSoundEffect(int effect_number, int idx)
{
	// Some sound effects are positioned at their source position.
	auto model = FindModelById(idx);
	auto source_pos = model ? model->pos : XMFLOAT3{};

	switch (effect_number)
	{
	case 0:	// Sentinel/sentry turn
		m_pAudio->Play(TURN_SOUND, AudioType::Effect, source_pos);
		break;
	case 1:	// Meanie turn
		m_pAudio->Play(MEANIE_SOUND, AudioType::Effect, source_pos);
		break;
	case 2:	// Absorb sound (unused on Spectrum)
		break;
	case 5:	// Energy depleted
		m_pAudio->Play(PONG_SOUND);
		m_pView->OutputAction(Action::Haptic_Depleted);
		break;
	case 0xff:	// Error beep
		m_pAudio->Play(PING_SOUND);
		break;
	default:
		DebugBreak();
		break;
	}
}

bool AddToolTip(HWND hwndControl, LPCWSTR pszText)
{
	auto hwndTip = CreateWindowEx(
		NULL, TOOLTIPS_CLASS, NULL,
		WS_POPUP | TTS_NOPREFIX | TTS_ALWAYSTIP,
		CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
		GetParent(hwndControl), NULL, GetModuleHandle(NULL), NULL);

	if (!hwndTip)
		return false;

	TOOLINFO toolInfo{ sizeof(toolInfo) };
	toolInfo.hwnd = GetParent(hwndControl);
	toolInfo.uFlags = TTF_IDISHWND | TTF_SUBCLASS;
	toolInfo.uId = reinterpret_cast<UINT_PTR>(hwndControl);
	toolInfo.lpszText = const_cast<LPWSTR>(pszText); // NOLINT: Win32 API
	SendMessage(hwndTip, TTM_ADDTOOL, 0, reinterpret_cast<LPARAM>(&toolInfo));

	return true;
}

static INT_PTR CALLBACK OptionsDialogProc(HWND hdlg, UINT uMsg, WPARAM wParam, LPARAM /*lParam*/)
{
	static HWND hwndSeatedVR, hwndHMDPointer, hwndLeftHanded, hwndDisableHaptics, hwndHexLandscapes;
	static HWND hwndInvertMouse, hwndMouseSpeed, hwndSoundPack, hwndMsaaSamples;
	static HWND hwndGameSpeed, hwndHrtfEnabled, hwndTunesEnabled, hwndMusicEnabled, hwndMusicVolume;

	switch (uMsg)
	{
	case WM_INITDIALOG:
	{
		hwndSeatedVR = GetDlgItem(hdlg, IDC_SEATED_VR);
		hwndHMDPointer = GetDlgItem(hdlg, IDC_HMD_POINTER);
		hwndLeftHanded = GetDlgItem(hdlg, IDC_LEFT_HANDED);
		hwndDisableHaptics = GetDlgItem(hdlg, IDC_DISABLE_HAPTICS);
		hwndInvertMouse = GetDlgItem(hdlg, IDC_INVERT_MOUSE);
		hwndMouseSpeed = GetDlgItem(hdlg, IDC_MOUSE_SPEED);
		hwndHexLandscapes = GetDlgItem(hdlg, IDC_HEX_LANDSCAPES);
		hwndSoundPack = GetDlgItem(hdlg, IDC_SOUND_PACK);
		hwndMsaaSamples = GetDlgItem(hdlg, IDC_MSAA_SAMPLES);
		hwndGameSpeed = GetDlgItem(hdlg, IDC_GAME_SPEED);
		hwndHrtfEnabled = GetDlgItem(hdlg, IDC_HRTF);
		hwndTunesEnabled = GetDlgItem(hdlg, IDC_TUNES_ENABLED);
		hwndMusicEnabled = GetDlgItem(hdlg, IDC_MUSIC_ENABLED);
		hwndMusicVolume = GetDlgItem(hdlg, IDC_MUSIC_VOLUME);

		auto hdll = LoadLibrary(X3D_HRTF_HOOK_DLL);
		if (hdll)
			FreeLibrary(hdll);
		else
			EnableWindow(hwndHrtfEnabled, FALSE);

		AddToolTip(hwndSeatedVR, L"Centre player on seated position facing keyboard/monitor.");
		AddToolTip(hwndHMDPointer, L"Use VR headset as pointer, even if controllers are present");
		AddToolTip(hwndDisableHaptics, L"Disable VR controller vibration.");
		AddToolTip(hwndInvertMouse, L"Moving mouse up/down changes player view down/up instead.");
		AddToolTip(hwndHexLandscapes, L"Advance levels in hexadecimal values instead of decimal.");
		AddToolTip(hwndMsaaSamples, L"Number of MSAA samples for polygon smoothing.");
		AddToolTip(hwndHrtfEnabled, L"Spatial audio tuned for the human hearing (headphones recommended).");
		AddToolTip(hwndTunesEnabled, L"Play sampled tunes for transfer, hyperspace, etc.");
		AddToolTip(hwndMusicEnabled, L"Play background music from sounds/music/ directory.");
		AddToolTip(GetDlgItem(hdlg, IDC_DEFAULTS), L"Restore all settings to default values.");

		auto seated_vr = GetFlag(SEATED_VR_KEY, DEFAULT_SEATED_VR);
		auto hmd_pointer = GetFlag(HMD_POINTER_KEY, DEFAULT_HMD_POINTER);
		auto left_handed = GetFlag(LEFT_HANDED_KEY, DEFAULT_LEFT_HANDED);
		auto disable_haptics = GetFlag(DISABLE_HAPTICS_KEY, DEFAULT_DISABLE_HAPTICS);
		auto hex_landscapes = GetFlag(HEX_LANDSCAPES_KEY, DEFAULT_HEX_LANDSCAPES);
		auto mouse_inverted = GetFlag(INVERT_MOUSE_KEY, DEFAULT_INVERT_MOUSE);
		auto mouse_speed = GetSetting(MOUSE_SPEED_KEY, DEFAULT_MOUSE_SPEED);
		auto sound_pack = GetSetting(SOUND_PACK_KEY, DEFAULT_SOUND_PACK);
		auto msaa_samples = GetSetting(MSAA_SAMPLES_KEY, DEFAULT_MSAA_SAMPLES);
		auto game_speed = GetSetting(GAME_SPEED_KEY, DEFAULT_GAME_SPEED);
		auto hrtf_enabled = GetSetting(HRTF_ENABLED_KEY, DEFAULT_TUNES_ENABLED);
		auto tunes_enabled = GetSetting(TUNES_ENABLED_KEY, DEFAULT_TUNES_ENABLED);
		auto music_enabled = GetSetting(MUSIC_ENABLED_KEY, DEFAULT_MUSIC_ENABLED);
		auto music_volume = GetSetting(MUSIC_VOLUME_KEY, DEFAULT_MUSIC_VOLUME);

		Button_SetCheck(hwndSeatedVR, seated_vr ? BST_CHECKED : BST_UNCHECKED);
		Button_SetCheck(hwndHMDPointer, hmd_pointer ? BST_CHECKED : BST_UNCHECKED);
		Button_SetCheck(hwndLeftHanded, left_handed ? BST_CHECKED : BST_UNCHECKED);
		Button_SetCheck(hwndDisableHaptics, disable_haptics ? BST_CHECKED : BST_UNCHECKED);
		Button_SetCheck(hwndHexLandscapes, hex_landscapes ? BST_CHECKED : BST_UNCHECKED);
		Button_SetCheck(hwndInvertMouse, mouse_inverted ? BST_CHECKED : BST_UNCHECKED);
		Button_SetCheck(hwndHrtfEnabled, hrtf_enabled ? BST_CHECKED : BST_UNCHECKED);
		Button_SetCheck(hwndTunesEnabled, tunes_enabled ? BST_CHECKED : BST_UNCHECKED);
		Button_SetCheck(hwndMusicEnabled, music_enabled ? BST_CHECKED : BST_UNCHECKED);

		SendMessage(hwndMouseSpeed, TBM_SETRANGE, 0, MAKELONG(0, 100));
		SendMessage(hwndMouseSpeed, TBM_SETPAGESIZE, 0, 10);
		SendMessage(hwndMouseSpeed, TBM_SETTICFREQ, 5, 0);
		SendMessage(hwndMouseSpeed, TBM_SETPOS, TRUE, mouse_speed);
		SendMessage(hwndMusicVolume, TBM_SETRANGE, 0, MAKELONG(0, 10));
		SendMessage(hwndMusicVolume, TBM_SETPOS, TRUE, music_volume / 10);

		std::vector<std::wstring> sound_packs;
		for (auto& p : fs::directory_iterator(SOUND_PACK_DIR))
		{
			auto filename = p.path().filename().wstring();

			// Any sub-directory not starting with a dot is considered a sound pack.
			if (p.is_directory() && filename[0] != '.')
			{
				auto missing_files{ false };

				for (auto& wav_file : effects_and_tunes)
				{
					// Reject the pack if any required sounds are missing.
					if (!fs::exists(p / wav_file))
					{
						missing_files = true;
						break;
					}
				}

				if (!missing_files)
					sound_packs.push_back(filename);
			}
		}

		SendMessage(hwndSoundPack, CB_RESETCONTENT, 0, 0L);
		for (auto& sp : sound_packs)
			SendMessage(hwndSoundPack, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(sp.c_str()));

		auto cursel = SendMessage(hwndSoundPack, CB_FINDSTRING, 0, reinterpret_cast<LPARAM>(sound_pack.c_str()));
		SendMessage(hwndSoundPack, CB_SETCURSEL, (cursel == CB_ERR) ? 0 : cursel, 0L);


		if (msaa_samples != 1 && msaa_samples != 2 && msaa_samples != 4)
			msaa_samples = DEFAULT_MSAA_SAMPLES;

		SendMessage(hwndMsaaSamples, CB_RESETCONTENT, 0, 0L);
		for (size_t i = 0; i < msaa_modes.size(); ++i)
		{
			auto& mm = msaa_modes[i];
			SendMessage(hwndMsaaSamples, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(mm.second.c_str()));

			if (mm.first == msaa_samples)
				SendMessage(hwndMsaaSamples, CB_SETCURSEL, i, 0L);
		}

		SendMessage(hwndGameSpeed, CB_RESETCONTENT, 0, 0L);
		for (size_t i = 0; i < game_speeds.size(); ++i)
		{
			auto& gs = game_speeds[i];
			SendMessage(hwndGameSpeed, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(gs.second.c_str()));

			if (gs.first == game_speed)
				SendMessage(hwndGameSpeed, CB_SETCURSEL, i, 0L);
		}

		if (SendMessage(hwndGameSpeed, CB_GETCURSEL, 0, 0L) == CB_ERR)
			SendMessage(hwndGameSpeed, CB_SETCURSEL, 1, 0L);

		return TRUE;
	}

	case WM_COMMAND:
		switch (LOWORD(wParam))
		{
		case IDOK:
		{
			auto seated_vr = Button_GetCheck(hwndSeatedVR) == BST_CHECKED;
			auto hmd_pointer = Button_GetCheck(hwndHMDPointer) == BST_CHECKED;
			auto left_handed = Button_GetCheck(hwndLeftHanded) == BST_CHECKED;
			auto disable_haptics = Button_GetCheck(hwndDisableHaptics) == BST_CHECKED;
			auto hex_landscapes = Button_GetCheck(hwndHexLandscapes) == BST_CHECKED;
			auto mouse_inverted = Button_GetCheck(hwndInvertMouse) == BST_CHECKED;
			auto mouse_speed = static_cast<int>(SendMessage(hwndMouseSpeed, TBM_GETPOS, 0, 0L));
			auto sound_pack = WindowText(hwndSoundPack);
			auto msaa_samples = msaa_modes[SendMessage(hwndMsaaSamples, CB_GETCURSEL, 0, 0L)].first;
			auto game_speed = game_speeds[SendMessage(hwndGameSpeed, CB_GETCURSEL, 0, 0L)].first;
			auto hrtf_enabled = Button_GetCheck(hwndHrtfEnabled) == BST_CHECKED;
			auto tunes_enabled = Button_GetCheck(hwndTunesEnabled) == BST_CHECKED;
			auto music_enabled = Button_GetCheck(hwndMusicEnabled) == BST_CHECKED;
			auto music_volume = static_cast<int>(SendMessage(hwndMusicVolume, TBM_GETPOS, 0, 0L));

			SetSetting(SEATED_VR_KEY, seated_vr);
			SetSetting(HMD_POINTER_KEY, hmd_pointer);
			SetSetting(LEFT_HANDED_KEY, left_handed);
			SetSetting(DISABLE_HAPTICS_KEY, disable_haptics);
			SetSetting(HEX_LANDSCAPES_KEY, hex_landscapes);
			SetSetting(INVERT_MOUSE_KEY, mouse_inverted);
			SetSetting(MOUSE_SPEED_KEY, mouse_speed);
			SetSetting(SOUND_PACK_KEY, sound_pack);
			SetSetting(MSAA_SAMPLES_KEY, msaa_samples);
			SetSetting(GAME_SPEED_KEY, game_speed);
			SetSetting(HRTF_ENABLED_KEY, hrtf_enabled);
			SetSetting(TUNES_ENABLED_KEY, tunes_enabled);
			SetSetting(MUSIC_ENABLED_KEY, music_enabled);
			SetSetting(MUSIC_VOLUME_KEY, music_volume * 10);

			EndDialog(hdlg, TRUE);
			break;
		}

		case IDC_DEFAULTS:
		{
			Button_SetCheck(hwndSeatedVR, DEFAULT_SEATED_VR ? BST_CHECKED : BST_UNCHECKED);
			Button_SetCheck(hwndHMDPointer, DEFAULT_HMD_POINTER ? BST_CHECKED : BST_UNCHECKED);
			Button_SetCheck(hwndLeftHanded, DEFAULT_LEFT_HANDED ? BST_CHECKED : BST_UNCHECKED);
			Button_SetCheck(hwndDisableHaptics, DEFAULT_DISABLE_HAPTICS ? BST_CHECKED : BST_UNCHECKED);
			Button_SetCheck(hwndHexLandscapes, DEFAULT_HEX_LANDSCAPES ? BST_CHECKED : BST_UNCHECKED);
			Button_SetCheck(hwndInvertMouse, DEFAULT_INVERT_MOUSE ? BST_CHECKED : BST_UNCHECKED);
			SendMessage(hwndMouseSpeed, TBM_SETPOS, TRUE, DEFAULT_MOUSE_SPEED);
			SendMessage(hwndSoundPack, CB_SELECTSTRING, 0, reinterpret_cast<LPARAM>(DEFAULT_SOUND_PACK));
			SendMessage(hwndMsaaSamples, CB_SETCURSEL, 2, 0L);	// index of "4x"!
			SendMessage(hwndGameSpeed, CB_SETCURSEL, 2, 0L);	// index of "Normal"!
			Button_SetCheck(hwndHrtfEnabled, DEFAULT_HRTF_ENABLED ? BST_CHECKED : BST_UNCHECKED);
			Button_SetCheck(hwndTunesEnabled, DEFAULT_TUNES_ENABLED ? BST_CHECKED : BST_UNCHECKED);
			Button_SetCheck(hwndMusicEnabled, DEFAULT_MUSIC_ENABLED ? BST_CHECKED : BST_UNCHECKED);
			SendMessage(hwndMusicVolume, TBM_SETPOS, TRUE, DEFAULT_MUSIC_VOLUME / 10);
			break;
		}
		case IDCANCEL:
			EndDialog(hdlg, FALSE);
			break;
		}
		break;
	}

	return FALSE;
}

/*static*/ void Augmentinel::Options(HINSTANCE hinst, HWND hwndParent)
{
	DialogBox(hinst, MAKEINTRESOURCE(IDD_OPTIONS), hwndParent, OptionsDialogProc);
}

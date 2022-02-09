#pragma once

// Special constant meaning any key press not previously checked.
constexpr auto VK_ANY = -1;

enum class Action
{
	None,
	TitleExit,
	TitleContinue,
	LandscapeExit,
	LandscapeSelect,
	LandscapePrev,
	LandscapeNext,
	LandscapeFirst,
	LandscapeLast,
	LandscapePgUp,
	LandscapePgDn,
	Quit,
	Pause,
	Absorb,
	Tree,
	Boulder,
	Robot,
	Transfer,
	Hyperspace,
	U_Turn,
	TurnLeft,
	TurnRight,
	TurnLeft45,
	TurnRight45,
	TurnLeft90,
	TurnRight90,
	Turn180,
	LookUp,
	LookDown,
	ResetHMD,
	SkyViewContinue,
	ToggleTunes,
	ToggleMusic,
	MusicVolumeUp,
	MusicVolumeDown,
	Haptic_Seen,
	Haptic_Depleted,
	Haptic_Dead,
	Pose_LeftPointer,
	Pose_RightPointer,
};

struct ActionBinding
{
	Action action;
	std::vector<int> virt_keys;
	const char* openvr_path;
};

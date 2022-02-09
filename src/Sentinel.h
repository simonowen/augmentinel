#pragma once

// PAL C64 aspect ratio: http://codebase64.org/doku.php?id=base:pixel_aspect_ratio
// The PC version has square pixels, but feels a little more stretched than the originals.
static constexpr auto PIXEL_ASPECT_RATIO = 0.9365f;

// C64 game display resolution.
constexpr int SENTINEL_WIDTH = 320;
constexpr int SENTINEL_HEIGHT = 192;

// Determined empirically, to give the correct starting view.
constexpr float SENTINEL_VERT_FOV = 17.4f;	// 27.0f horizontal

// 32x32 vertices for 31x31 map tiles.
constexpr int SENTINEL_MAP_SIZE = 32;

// Minimum and maximum view pitch, when looking up/down.
constexpr uint8_t SENTINEL_MIN_PITCH = 0x35;	// +53
constexpr uint8_t SENTINEL_MAX_PITCH = 0xcd;	// -51

// Yaw step sizes for player and sentinel (in 1/256ths of 360 degrees).
constexpr auto PLAYER_YAW_DELTA = 8;
constexpr auto SENTINEL_YAW_DELTA = 20;

// Player pitch step in degrees (approximate!).
constexpr auto SENTINEL_PITCH_DEGREES = 5.5f;

// 10 models using a total of 160 vertices. Maximum of 64 world objects.
constexpr int NUM_MODELS = 10;
constexpr int NUM_VERTICES = 0xa0;
constexpr int MAX_OBJECTS = 0x40;

constexpr int BASE_VERTEX_INDEX = 0x40;
constexpr int ZX_VERTICES_PER_TILE = 6;

constexpr float EYE_HEIGHT = 0.875f;

constexpr uint32_t SPECTRUM_LANDSCAPE_0000_CODE = 0x75914644;

////////////////////////////////////////////////////////////////////////////////

// Model face colours from the DOS version.
// Low nibble is fill colour (palette index), high nibble is outline colour.
//										Used by:
//	0x00:	black								robot sentry tree boulder meanie sentinel pedestal block
//	0x02:	white with black outline			robot                     meanie
//	0x03:	green with black outline			                          meanie
//	0x09:	yellow with black outline			      sentry                     sentinel
//	0x0c:	mid-light blue with black outline	robot
//	0x20:	black with white outline			robot
//	0x22:	white								                                          pedestal block
//	0x33:	green								                                 sentinel pedestal block
//	0x44:	dark green							             tree         meanie
//	0x55:	mid green							             tree         meanie
//	0x66:	red									robot sentry                     sentinel
//	0x99:	yellow								robot                     meanie          pedestal
//	0xbb:	mid-cyan							                  boulder
//	0xcc:	mid-light blue						robot
//	0xdd:	brown								             tree         meanie
//	0xff:	dark blue							                  boulder
//
static const std::array<uint8_t, NUM_VERTICES> face_colours
{
	0x00, 0x00, 0x00, 0x02, 0x02, 0x20, 0x20, 0x99, 0x99, 0xCC, 0x0C, 0x0C, 0x00, 0x00, 0x0C, 0x0C,
	0x99, 0x99, 0x0C, 0x00, 0x66, 0x66, 0x99, 0x99, 0x00, 0x00, 0x99, 0x00, 0x00, 0x00, 0x00, 0x66,
	0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x00, 0x66, 0x00, 0x00, 0x00, 0x00, 0x66, 0x66, 0x00,
	0x09, 0x09, 0x09, 0x09, 0x00, 0x00, 0x00, 0x00, 0xDD, 0xDD, 0x00, 0x44, 0x55, 0x44, 0x55, 0x44,
	0x55, 0x44, 0x55, 0xFF, 0xFF, 0xFF, 0xFF, 0xBB, 0xBB, 0xBB, 0xBB, 0x00, 0xFF, 0x00, 0xDD, 0xDD,
	0x03, 0x03, 0x00, 0x55, 0x55, 0x44, 0x00, 0x00, 0x02, 0x02, 0x02, 0x00, 0x00, 0x99, 0x02, 0x02,
	0x00, 0x00, 0x99, 0x99, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x66, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x00, 0x00, 0x33, 0x33, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x09, 0x09, 0x09, 0x09, 0x00, 0x66, 0x66, 0x00, 0x22, 0x00, 0x22, 0x99, 0x99, 0x99,
	0x99, 0x33, 0x33, 0x33, 0x00, 0x00, 0x22, 0x33, 0x00, 0x00, 0x22, 0x33, 0x00, 0x00, 0x22, 0x33,
};

// Convert EGA colours (6 bits per channel) to normalised floats.
constexpr XMFLOAT4 ega_to_rgb(int r, int g, int b)
{
	return { (r / 63.0f), (g / 63.0f), (b / 63.0f), 1.0f };
}

// 16-colour EGA palette from DOS version.
static const std::vector<XMFLOAT4> game_ega_palette
{
	ega_to_rgb(0x00, 0x00, 0x00),	// 0: black
	ega_to_rgb(0x27, 0x27, 0x3f),	// 1: mid-light blue [dynamic]
	ega_to_rgb(0x3f, 0x3f, 0x3f),	// 2: white [dynamic]
	ega_to_rgb(0x00, 0x3f, 0x00),	// 3: green [dynamic]
	ega_to_rgb(0x00, 0x1f, 0x00),	// 4: dark green
	ega_to_rgb(0x17, 0x37, 0x17),	// 5: mid green
	ega_to_rgb(0x3f, 0x00, 0x00),	// 6: red
	ega_to_rgb(0x00, 0x0f, 0x27),	// 7: mid blue
	ega_to_rgb(0x00, 0x3f, 0x00),	// 8: green
	ega_to_rgb(0x3f, 0x3f, 0x00),	// 9: yellow
	ega_to_rgb(0x27, 0x00, 0x27),	// a: mid magenta
	ega_to_rgb(0x17, 0x2f, 0x2f),	// b: mid cyan
	ega_to_rgb(0x27, 0x27, 0x3f),	// c: mid-light blue
	ega_to_rgb(0x1f, 0x0f, 0x00),	// d: brown
	ega_to_rgb(0x3f, 0x3f, 0x3f),	// e: white
	ega_to_rgb(0x00, 0x00, 0x17),	// f: dark blue
};

static const std::vector<XMFLOAT4> title_ega_palette
{
	ega_to_rgb(0x00, 0x00, 0x00),
	ega_to_rgb(0x00, 0x27, 0x17),
	ega_to_rgb(0x17, 0x3F, 0x3F),
	ega_to_rgb(0x1F, 0x1F, 0x37),
	ega_to_rgb(0x27, 0x0F, 0x0F),
	ega_to_rgb(0x0F, 0x0F, 0x27),
	ega_to_rgb(0x37, 0x1F, 0x1F),
	ega_to_rgb(0x00, 0x00, 0x1F),
	ega_to_rgb(0x0F, 0x0F, 0x27),
	ega_to_rgb(0x27, 0x27, 0x0F),
	ega_to_rgb(0x00, 0x00, 0x17),
	ega_to_rgb(0x00, 0x00, 0x0F),
	ega_to_rgb(0x0F, 0x0F, 0x37),
	ega_to_rgb(0x00, 0x00, 0x0F),
	ega_to_rgb(0x2F, 0x2F, 0x2F),
	ega_to_rgb(0x37, 0x37, 0x17),
};

// Dynamic ega_palette[1] entries, indexed by number of sentries.
static const std::vector<XMFLOAT4> pal1_colours
{
	ega_to_rgb(0x00, 0x27, 0x27),
	ega_to_rgb(0x27, 0x00, 0x27),
	ega_to_rgb(0x00, 0x27, 0x27),
	ega_to_rgb(0x37, 0x17, 0x00),
	ega_to_rgb(0x17, 0x17, 0x3f),
	ega_to_rgb(0x3f, 0x00, 0x00),
	ega_to_rgb(0x27, 0x00, 0x27),
	ega_to_rgb(0x17, 0x17, 0x3f),
};

// Dynamic ega_palette[2] entries, indexed by number of sentries.
static const std::vector<XMFLOAT4> pal2_colours
{
	ega_to_rgb(0x3f, 0x3f, 0x3f),
	ega_to_rgb(0x27, 0x3f, 0x3f),
	ega_to_rgb(0x3f, 0x2f, 0x2f),
	ega_to_rgb(0x3f, 0x3f, 0x3f),
	ega_to_rgb(0x27, 0x3f, 0x3f),
	ega_to_rgb(0x3f, 0x3f, 0x27),
	ega_to_rgb(0x3f, 0x27, 0x3f),
	ega_to_rgb(0x3f, 0x00, 0x00),
};

// Dynamic ega_palette[3] entries, indexed by number of sentries.
static const std::vector<XMFLOAT4> pal3_colours
{
	ega_to_rgb(0x00, 0x3f, 0x00),
	ega_to_rgb(0x3f, 0x3f, 0x27),
	ega_to_rgb(0x27, 0x3f, 0x3f),
	ega_to_rgb(0x3f, 0x3f, 0x27),
	ega_to_rgb(0x3f, 0x3f, 0x3f),
	ega_to_rgb(0x3f, 0x2f, 0x2f),
	ega_to_rgb(0x3f, 0x3f, 0x3f),
	ega_to_rgb(0x3f, 0x3f, 0x00),
};

static constexpr auto BLACK_PALETTE_INDEX = 0;
static constexpr auto SKY_PALETTE_INDEX = 7;
static constexpr auto WHITE_PALETTE_INDEX = 14;
static constexpr auto DARK_BLUE_PALETTE_INDEX = 15;

enum class InputAction
{
	CreateRobot = 0x00,
	CreateTree = 0x02,
	CreateBoulder = 0x03,
	Absorb = 0x20,
	Transfer = 0x21,
	Hyperspace = 0x22,
	UTurn = 0x23
};

////////////////////////////////////////////////////////////////////////////////

constexpr inline float PitchToRadians(uint8_t val)
{
	// Approximate! Need to work out what the game is doing.
	return (static_cast<signed char>(0xf5 - val) * 6.25f + 11.0f) / 256.0f;
}

constexpr inline float YawToRadians(uint8_t val)
{
	// 1/256ths of 360 degrees, with 0 facing away.
	return XMConvertToRadians(val / 256.0f * 360.0f);
}

////////////////////////////////////////////////////////////////////////////////

struct ISentinelEvents
{
	virtual void OnTitleScreen() = 0;
	virtual void OnLandscapeInput(int& landscape_bcd, uint32_t& secret_code_bcd) = 0;
	virtual void OnLandscapeGenerated() = 0;
	virtual void OnNewPlayerView() = 0;
	virtual void OnPlayerDead() = 0;
	virtual void OnInputAction(uint8_t& action) = 0;
	virtual void OnGameModelChanged(int id, bool player_initiated) = 0;
	virtual bool OnTargetActionTile(InputAction action, int& tile_x, int& tile_z) = 0;
	virtual void OnHideEnergyPanel() = 0;
	virtual void OnAddEnergySymbol(int symbol_idx, int x_offset) = 0;
	virtual void OnPlayTune(int n) = 0;
	virtual void OnSoundEffect(int n, int idx) = 0;
};

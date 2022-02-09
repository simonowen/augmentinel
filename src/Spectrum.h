#pragma once
#include "Model.h"

static constexpr auto HEX_LANDSCAPES_KEY = L"HexLandscapes";
static constexpr auto DEFAULT_HEX_LANDSCAPES = false;

#include "Z80.h"

#define Z80_AF		(Z_Z80_STATE_AF(&m_z80.state))
#define Z80_BC		(Z_Z80_STATE_BC(&m_z80.state))
#define Z80_DE		(Z_Z80_STATE_DE(&m_z80.state))
#define Z80_HL		(Z_Z80_STATE_HL(&m_z80.state))
#define Z80_AF_		(Z_Z80_STATE_AF_(&m_z80.state))
#define Z80_BC_		(Z_Z80_STATE_BC_(&m_z80.state))
#define Z80_DE_		(Z_Z80_STATE_DE_(&m_z80.state))
#define Z80_HL_		(Z_Z80_STATE_HL_(&m_z80.state))
#define Z80_IX		(Z_Z80_STATE_IX(&m_z80.state))
#define Z80_IY		(Z_Z80_STATE_IY(&m_z80.state))
#define Z80_PC		(Z_Z80_STATE_PC(&m_z80.state))
#define Z80_SP		(Z_Z80_STATE_SP(&m_z80.state))
#define Z80_A		(Z_Z80_STATE_A(&m_z80.state))
#define Z80_F		(Z_Z80_STATE_F(&m_z80.state))
#define Z80_B		(Z_Z80_STATE_B(&m_z80.state))
#define Z80_C		(Z_Z80_STATE_C(&m_z80.state))
#define Z80_D		(Z_Z80_STATE_D(&m_z80.state))
#define Z80_E		(Z_Z80_STATE_E(&m_z80.state))
#define Z80_H		(Z_Z80_STATE_H(&m_z80.state))
#define Z80_L		(Z_Z80_STATE_L(&m_z80.state))
#define Z80_IXH		(Z_Z80_STATE_IXH(&m_z80.state))
#define Z80_IXL		(Z_Z80_STATE_IXL(&m_z80.state))
#define Z80_IYH		(Z_Z80_STATE_IYH(&m_z80.state))
#define Z80_IYL		(Z_Z80_STATE_IYL(&m_z80.state))
#define Z80_I		(Z_Z80_STATE_I(&m_z80.state))
#define Z80_R		(Z_Z80_STATE_R(&m_z80.state))
#define Z80_CYCLES	(m_z80.cycles)
#define Z80_STATE	(m_z80.state)

#define EmulateCycles(cycles)		z80_run(&m_z80, cycles)
#define ActivateInterrupt(enable)	z80_int(&m_z80, enable)
#define EndFrame()					Z80_CYCLES += SPECTRUM_CYCLES_PER_FRAME

const uint8_t BREAKPOINT_OPCODE = 0x64;	// LD H,H

static constexpr int SPECTRUM_ROM_SIZE = 0x4000;
static constexpr int SPECTRUM_RAM_SIZE = 0xc000;
static constexpr int SPECTRUM_MEM_SIZE = SPECTRUM_ROM_SIZE + SPECTRUM_RAM_SIZE;

static constexpr int SPECTRUM_CYCLES_PER_SECOND = 3'500'000;
static constexpr int SPECTRUM_FRAMES_PER_SECOND = 50;
static constexpr int SPECTRUM_CYCLES_PER_FRAME = SPECTRUM_CYCLES_PER_SECOND / SPECTRUM_FRAMES_PER_SECOND;
static constexpr int SPECTRUM_CYCLES_PER_INT = 32;
static constexpr int SPECTRUM_CYCLES_BEFORE_INT = SPECTRUM_CYCLES_PER_FRAME - SPECTRUM_CYCLES_PER_INT;

enum class SeenState { Unseen, HalfSeen, FullSeen };

class Spectrum
{
public:
	Spectrum(std::wstring filename, ISentinelEvents* pCallback = nullptr);

	void LoadSnapshot(const std::wstring& filename);
	void RunFrame(bool interrupt = true);
	void RunInterrupt();

	void GetLandscapeAndCode(int& landscape_bcd, uint32_t& secret_code_bcd) const;
	Model GetModel(ModelType type) const;
	Model GetModel(int idx, bool ignore_under = false) const;
	uint8_t GetTileShape(int x, int z) const;
	void LandscapeVertexIndexToTile(int vertex_index, int& tile_x, int& tile_z);
	Model ExtractLandscape() const;
	std::vector<Model> ExtractText() const;
	Model ExtractPlayerModel() const;
	std::vector<Model> ExtractPlacedModels() const;
	Model CharToModel(char ch, int colour) const;
	Model IconToModel(int icon_idx, int colour);
	std::vector<XMFLOAT4> GetGamePalette(int num_sentries = -1) const;
	std::vector<XMFLOAT4> GetTitlePalette() const;
	void SetPlayerPitch(float radians);
	void SetPlayerYaw(float radians);
	SeenState GetPlayerSeenState() const;

protected:
	ISentinelEvents* m_pEvents{ nullptr };

	std::vector<Model> ExtractModels();
	Vertex PolarToCartesian(uint8_t yaw, float y, uint8_t mag) const;

	Z80 m_z80{};

	uint32_t m_secret_code_bcd{};
	std::vector<uint8_t> m_mem;
	std::vector<Model> m_models;
	std::map<std::pair<int, int>, Model> m_icon_cache;

	void Push(uint16_t value);
	uint16_t Pop();
	void Jump(uint16_t address);
	void Call(uint16_t address);
	void Ret();
	uint16_t DPeek(uint16_t address);
	uint16_t DPoke(uint16_t address, uint16_t value);

	using HookFunction = std::function<void()>;
	void Hook(uint16_t address, uint8_t expected_opcode, HookFunction fn);
	void OnHook(uint16_t address);

	struct HookData
	{
		HookFunction func{ nullptr };
		uint8_t orig_opcode{ 0 };
	};
	std::map<uint16_t, HookData> m_hooks;
};

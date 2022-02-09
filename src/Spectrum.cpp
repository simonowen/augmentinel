#include "stdafx.h"
#include "Spectrum.h"
#include "Settings.h"
#include "Vertex.h"

static constexpr int SNA_HEADER_SIZE = 27;

static constexpr int ZX_SEEN_INDICATOR_FLAGS_ADDR = 0x6004;
static constexpr int ZX_PLAYER_ENERGY_ADDR = 0x600a;
static constexpr int ZX_PLAYER_SEEN_FLAGS_ADDR = 0x604f;
static constexpr int ZX_BCD_SECRET_CODE_ADDR = 0x60f0;
static constexpr int ZX_BCD_LANDSCAPE_LSB = 0x60fd;
static constexpr int ZX_BCD_LANDSCAPE_MSB = 0x60fe;
static constexpr int ZX_OBJ_ACTION = 0x6061;
static constexpr int ZX_NUM_SENTS = 0x606f;
static constexpr int ZX_MAP_ADDR = 0x6100;
static constexpr int ZX_PLACED_OBJ_IDX_ADDR = 0x6501;
static constexpr int ZX_PLAYER_OBJ_IDX_ADDR = 0x650b;
static constexpr int ZX_VERTEX_INDICES_ADDR = 0x6600;
static constexpr int ZX_FACE_INDICES_ADDR = 0x660b;
static constexpr int ZX_COORDS_ADDR = 0x66a0;
static constexpr int ZX_FACE_COLOURS_ADDR = 0x6880;
static constexpr int ZX_FACE_LSBS_ADDR = 0x6920;
static constexpr int ZX_FACE_MSBS_ADDR = 0x69c0;
static constexpr int ZX_PANEL_ICONS_ADDR = 0x6ca0;
static constexpr int ZX_GAME_FONT_ADDR = 0x7400;

static constexpr int ZX_OBJS_UNDER = 0x5e00;
static constexpr int ZX_OBJS_PITCH = 0x5e40;
static constexpr int ZX_OBJS_X = 0xf800;
static constexpr int ZX_OBJS_Y = 0xf840;
static constexpr int ZX_OBJS_Z = 0xf880;
static constexpr int ZX_OBJS_YAW = 0xf8c0;
static constexpr int ZX_OBJS_Y_FRAC = 0xf9c0;
static constexpr int ZX_OBJS_TYPE = 0xfac0;

Spectrum::Spectrum(std::wstring filename, ISentinelEvents* pEvents)
	: m_pEvents(pEvents)
{
	// Initialise the emulation.
	z80_power(&m_z80, TRUE);
	z80_reset(&m_z80);

	// Load the Sentinel snapshot, and check it really is The Sentinel.
	LoadSnapshot(filename);
	if (std::string(reinterpret_cast<char*>(&m_mem[0xC33D]), 8) != "SENTINEL")
		throw std::runtime_error("not a Sentinel snapshot");

	// Automation patches.
	m_mem[0xe22f] = 0x18;	// select 2 for Sinclair Joystick from main menu.
	m_mem[0xb8ff] = 0xc9;	// skip all wait_key_press calls
	m_mem[0x8375] = 0x18;	// actions work outside cursor mode.
	m_mem[0xbb58] = 0xc9;	// skip screen disintegration effect.
	m_mem[0x83B9] = 0x3e;	// remove 0.8x multiplier for original game speed.
	m_mem[0x76bf] = 0xff;	// error uses higher pitch ping
	m_mem[0x85D8] = 0x3e;	// ignore invalid secret codes
	m_mem[0x9c84] = 0xc3;	// skip game start code check

	// Blind Sentinel and sentries?
	if (GetFlag(L"Invisible", false))
		m_mem[0x9024] = 0xc3;

	// No special treatment for level 0000?
	if (GetFlag(L"True0000", false))
		m_mem[0xafe3] = 0x22;

	// Enable extended landscapes containing hex digits?
	if (GetFlag(HEX_LANDSCAPES_KEY, false))
	{
		// Remove DAA instructions used for landscape arithmetic.
		m_mem[0x92b9] = m_mem[0x92c0] = m_mem[0xb064] = 0x00;
	}

	// Return at end of IM 2 handler.
	Hook(0xBD97, 0xc9 /*RET*/, [&]
		{
			// Force end of interrupt emulation.
			EndFrame();
		});

	// Call to wait_for_key on title screen.
	Hook(0x7fe5, 0xcd /*CALL nn*/, [&]
		{
			m_pEvents->OnTitleScreen();

			// Skip CALL and end emulated frame
			Z80_PC += 3;
			EndFrame();
		});

	// About to prompt for landscape number.
	Hook(0x7FF8, 0x3e /*LD A,n*/, [&]
		{
			// Ask the game for the landscape number and corresponding secret code.
			int landscape_bcd{};
			uint32_t secret_code_bcd{};
			m_pEvents->OnLandscapeInput(landscape_bcd, secret_code_bcd);

			// Set secret code and resume point after secret code input.
			m_mem[ZX_BCD_SECRET_CODE_ADDR + 0] = (secret_code_bcd >> 0) & 0xff;
			m_mem[ZX_BCD_SECRET_CODE_ADDR + 1] = (secret_code_bcd >> 8) & 0xff;
			m_mem[ZX_BCD_SECRET_CODE_ADDR + 2] = (secret_code_bcd >> 16) & 0xff;
			m_mem[ZX_BCD_SECRET_CODE_ADDR + 3] = (secret_code_bcd >> 24) & 0xff;
			m_mem[ZX_BCD_SECRET_CODE_ADDR + 4] =
				m_mem[ZX_BCD_SECRET_CODE_ADDR + 5] =
				m_mem[ZX_BCD_SECRET_CODE_ADDR + 6] =
				m_mem[ZX_BCD_SECRET_CODE_ADDR + 7] = 0xff;	// ASCII hiding filler.
			Z80_PC = 0x803a;

			// Set landscape number (BCD), and insert a call to seed the RNG using it.
			Z80_C = landscape_bcd & 0xff;
			Z80_E = (landscape_bcd >> 8) & 0xff;
			Call(0xafc9);
		});

	// Initialise landscape number.
	Hook(0xAFC9, 0x21 /*LD HL,nn*/, [&]
		{
			// Wrap after DFFF or the game hangs when trying to calculate the number
			// of sentries, due to adding 2 to high nibble of the landscape MSB.
			Z80_E %= 0xE0;
		});

	// Remaining models are placed on generated landscape.
	Hook(0xB1B0, 0x3e /*LD A,n*/, [&]
		{
			m_pEvents->OnLandscapeGenerated();
		});

	// Start of refreshed player view.
	Hook(0xB2bc, 0xcd /*CALL nn*/, [&]
		{
			m_pEvents->OnNewPlayerView();
			EndFrame();
		});

#ifdef _DEBUG
	// Secret code check -- log expected/correct secret code in DEBUG ONLY!
	Hook(0x85a9, 0x96, [&]
		{
			if (Z80_HL >= 0x60f0 && Z80_HL <= 0x60f3)
			{
				std::wstringstream ss;
				ss << std::hex << Z80_HL << " = " << std::setw(2) << std::setfill(L'0') << (int)Z80_A << "\n";
				OutputDebugString(ss.str().c_str());
			}
		});
#endif

	// Secret code generation -- capture newly generated code.
	Hook(0xafa9, 0xcd /*CALL nn*/, [&]
		{
			// Shift in the next pair of generated digits.
			m_secret_code_bcd = (m_secret_code_bcd << 8) | Z80_A;
		});

	// Fetching any keyboard triggered action.
	Hook(0x822c, 0xe6 /*AND n*/, [&]
		{
			m_pEvents->OnInputAction(Z80_A);
			m_mem[DPeek(Z80_PC - 2)] = Z80_A;
		});

	// Player-triggered object change.
	Hook(0x839d, 0xcd /*CALL nn*/, [&]
		{
			auto idx = m_mem[ZX_PLACED_OBJ_IDX_ADDR];
			m_pEvents->OnGameModelChanged(idx, true);
			Z80_PC += 3;	// skip drawing CALL
		});

	// Sentinel/sentry-triggered object change.
	Hook(0x9007, 0xcd /*CALL nn*/, [&]
		{
			auto idx = m_mem[ZX_PLACED_OBJ_IDX_ADDR];
			m_pEvents->OnGameModelChanged(idx, false);
			Z80_PC += 3;	// skip drawing CALL
		});

	// Cursor target check when performing actions.
	Hook(0x7648, 0x38 /*JR C,e*/, [&]
		{
			int tile_x{}, tile_z{};
			auto action = static_cast<InputAction>(m_mem[ZX_OBJ_ACTION]);

			// Perform our own pixel-perfect scene test.
			if (m_pEvents->OnTargetActionTile(action, tile_x, tile_z))
			{
				// Target valid, mark its tile location.
				m_mem[0x6524] = m_mem[0x6591] = static_cast<uint8_t>(tile_x);
				m_mem[0x6526] = m_mem[0x6595] = static_cast<uint8_t>(tile_z);
				Z80_F &= ~1;	// clear carry
			}
			else
			{
				// Target not valid (will result in error beep).
				Z80_F |= 1;	// set carry
			}
		});

	// Energy panel being hidden.
	Hook(0xb342, 0x3e /*LD A,n*/, [&]
		{
			m_pEvents->OnHideEnergyPanel();
		});

	// Energy panel symbol being drawn.
	Hook(0xbd64, 0x21 /*LD HL,nn*/, [&]
		{
			auto symbol_idx = Z80_C / 8;
			auto x_offset = Z80_A;
			m_pEvents->OnAddEnergySymbol(symbol_idx, x_offset);
		});

	// Tune playback.
	Hook(0xbbfd, 0x32 /*LD (nn),A*/, [&]
		{
			m_pEvents->OnPlayTune(Z80_A);
			Ret();	// Skip Spectrum tune player.
		});

	// Sound effects.
	Hook(0xbcaf, 0xb7 /*OR A*/, [&]
		{
			m_pEvents->OnSoundEffect(Z80_A, Z80_C);
			Ret();	// Skip Spectrum sound effect.
		});

	// Player has run out of energy.
	Hook(0xbb34, 0xcd /*CALL nn*/, [&]
		{
			m_pEvents->OnPlayerDead();
		});

	// Set object context for callbacks.
	m_z80.context = this;

	// Set event hooks for emulation.
	m_z80.read = [](void* context, zuint16 address) -> zuint8 {
		auto& zx = *reinterpret_cast<Spectrum*>(context);
		return zx.m_mem[address];
	};
	m_z80.write = [](void* context, zuint16 address, zuint8 value) {
		auto& zx = *reinterpret_cast<Spectrum*>(context);
		if (address >= 0x4000)
			zx.m_mem[address] = value;
	};
	m_z80.in = [](void* /*context*/, zuint16 /*address*/) -> zuint8 { return 0xff; };
	m_z80.out = [](void* /*context*/, zuint16 /*address*/, zuint8 /*value*/) {};
	m_z80.int_data = [](void* /*context*/) -> zuint32 { return 0xffff; };
	m_z80.hook = [](void* context, zuint16 address) {
		auto& zx = *reinterpret_cast<Spectrum*>(context);
		zx.OnHook(address);
	};

	// Extract the master models list from the snapshot.
	m_models = ExtractModels();
}

void Spectrum::Push(uint16_t value)
{
	Z80_SP -= 2;
	DPoke(Z80_SP, value);
}

uint16_t Spectrum::Pop()
{
	uint16_t value = DPeek(Z80_SP);
	Z80_SP += 2;
	return value;
}

void Spectrum::Jump(uint16_t address)
{
	Z80_PC = address;
}

void Spectrum::Call(uint16_t address)
{
	Push(Z80_PC);
	Jump(address);
}

void Spectrum::Ret()
{
	Jump(Pop());
}

uint16_t Spectrum::DPeek(uint16_t address)
{
	uint16_t value = m_mem[address++];
	value |= (m_mem[address] << 8);
	return value;
}

uint16_t Spectrum::DPoke(uint16_t address, uint16_t value)
{
	m_mem[address++] = value & 0xff;
	m_mem[address] = value >> 8;
	return value;
}

void Spectrum::LoadSnapshot(const std::wstring& filename)
{
	m_mem = FileContents(L"48.rom");
	m_mem.resize(SPECTRUM_MEM_SIZE);

	auto file = FileContents(filename);
	std::copy(file.begin() + 27, file.end(), m_mem.begin() + 0x4000);

	Z80_SP = file[23] + (file[24] << 8);
	Z80_AF = file[21] + (file[22] << 8);
	Z80_BC = file[13] + (file[14] << 8);
	Z80_DE = file[11] + (file[12] << 8);
	Z80_HL = file[9] + (file[10] << 8);
	Z80_IX = file[17] + (file[18] << 8);
	Z80_IY = file[15] + (file[16] << 8);
	Z80_AF_ = file[7] + (file[8] << 8);
	Z80_BC_ = file[5] + (file[6] << 8);
	Z80_DE_ = file[3] + (file[4] << 8);
	Z80_HL_ = file[1] + (file[2] << 8);
	Z80_I = file[0];
	Z80_R = file[20];

	auto cpu = &m_z80.state;
	Z_Z80_STATE_IRQ(cpu) = 0;
	Z_Z80_STATE_NMI(cpu) = 0;
	Z_Z80_STATE_HALT(cpu) = 0;
	Z_Z80_STATE_IFF1(cpu) = (file[19] & 4) ? 1 : 0;
	Z_Z80_STATE_IFF2(cpu) = Z_Z80_STATE_IFF1(cpu);
	Z_Z80_STATE_EI(cpu) = Z_Z80_STATE_IFF2(cpu);
	Z_Z80_STATE_IM(cpu) = (file[25] & 3);

	Z80_PC = m_mem[Z80_SP] + (m_mem[Z80_SP + 1] << 8);
	Z80_SP = Z80_SP + 2;
}

void Spectrum::RunFrame(bool interrupt)
{
	EmulateCycles(SPECTRUM_CYCLES_BEFORE_INT);

	if (interrupt)
		RunInterrupt();
}

void Spectrum::RunInterrupt()
{
	// Run for interrupt active period.
	ActivateInterrupt(true);
	EmulateCycles(SPECTRUM_CYCLES_PER_INT);
	ActivateInterrupt(false);

	// Run until IM 2 handler returns.
	EmulateCycles(SPECTRUM_CYCLES_PER_FRAME);
}

void Spectrum::Hook(uint16_t address, uint8_t expected_opcode, HookFunction fn)
{
	if (m_mem[address] != expected_opcode)
	{
		DebugBreak();
		throw std::exception("Snapshot is incompatible with code hooks.");
	}

	HookData new_hook{};
	new_hook.func = fn;
	new_hook.orig_opcode = expected_opcode;

	m_mem[address] = BREAKPOINT_OPCODE;
	m_hooks[address] = new_hook;
}

void Spectrum::OnHook(uint16_t address)
{
	auto it = m_hooks.find(address);
	if (it != m_hooks.end())
	{
		const auto& hook = it->second;

		// Unhook
		m_mem[address] = hook.orig_opcode;

		// Call the hook handler
		hook.func();

		// If PC hasn't been changed, single-step past the hooked instruction.
		if (Z80_PC == address)
		{
			auto old_cycles = Z80_CYCLES;
			EmulateCycles(1);
			Z80_CYCLES += old_cycles;
		}

		// Re-hook
		m_mem[address] = BREAKPOINT_OPCODE;

		// Fail hooking of unusual instructions, including block repeats.
		if (Z80_PC == address)
			throw std::exception("Failed to single-step past hooked instruction");
	}
	else
	{
		// Real LD H,H encountered, so step past it
		Z80_PC++;
		Z80_CYCLES += 4;
	}
}

void Spectrum::GetLandscapeAndCode(int& landscape_bcd, uint32_t& secret_code_bcd) const
{
	landscape_bcd = (m_mem[ZX_BCD_LANDSCAPE_MSB] << 8) | m_mem[ZX_BCD_LANDSCAPE_LSB];
	secret_code_bcd = m_secret_code_bcd;
}

////////////////////////////////////////////////////////////////////////////////

constexpr int GetMapAddress(int x, int z)
{
	return ZX_MAP_ADDR + ((x & 3) << 8) | ((x << 3) & 0xe0) | z;
}

Model Spectrum::GetModel(ModelType type) const
{
	return m_models[static_cast<uint8_t>(type)];
}

Model Spectrum::GetModel(int idx, bool ignore_under) const
{
	// If there's no object at this index, return the default model (ModelType::Unknown)
	if (!ignore_under && (m_mem[ZX_OBJS_UNDER + idx] & 0x80))
		return {};

	auto model_type = m_mem[ZX_OBJS_TYPE + (idx & 0x3f)];
	auto model = m_models[model_type];
	model.id = idx;
	model.pos.x = m_mem[ZX_OBJS_X + (idx & 0x3f)] * 1.0f;
	model.pos.y = m_mem[ZX_OBJS_Y + (idx & 0x3f)] + (m_mem[ZX_OBJS_Y_FRAC + (idx & 0x3f)] / 256.0f);
	model.pos.z = m_mem[ZX_OBJS_Z + (idx & 0x3f)] * 1.0f;
	model.rot.y = YawToRadians(m_mem[ZX_OBJS_YAW + (idx & 0x3f)]);

	// Player object contains view pitch, rather than draw pitch.
	if (model.type == ModelType::Robot)
		model.rot.x = PitchToRadians(m_mem[ZX_OBJS_PITCH + (idx & 0x3f)]);

	return model;
}

uint8_t Spectrum::GetTileShape(int x, int z) const
{
	auto map_entry = m_mem[GetMapAddress(x, z)];
	if (map_entry >= 0xc0)
	{
		for (auto entry = map_entry; entry > 0x40; )
		{
			map_entry = m_mem[ZX_OBJS_Y + (entry & 0x3f)] << 4;
			entry = m_mem[ZX_OBJS_UNDER + (entry & 0x3f)];
		}
	}

	return map_entry & 0xf;
}

void Spectrum::LandscapeVertexIndexToTile(int vertex_index, int& tile_x, int& tile_z)
{
	// This calculation must match the order in ExtractLandscape below.
	auto tile_index = vertex_index / ZX_VERTICES_PER_TILE;
	tile_x = tile_index % (SENTINEL_MAP_SIZE - 1);
	tile_z = tile_index / (SENTINEL_MAP_SIZE - 1);
}

Model Spectrum::ExtractLandscape() const
{
	std::vector<Vertex> vertices;
	std::vector<uint32_t> indices;

	for (int z = 0; z < SENTINEL_MAP_SIZE - 1; ++z)
	{
		for (int x = 0; x < SENTINEL_MAP_SIZE - 1; ++x)
		{
			uint8_t tile_entry = 0;
			std::array<Vertex, 4> tile_vertices;
			uint32_t colour{};

			for (int zz = 0; zz < 2; ++zz)
			{
				for (int xx = 0; xx < 2; ++xx)
				{
					auto map_x = (x + xx);
					auto map_z = (z + zz);
					auto offset = GetMapAddress(map_x, map_z);
					auto map_entry = m_mem[offset];

					if (map_entry >= 0xc0)
					{
						for (auto entry = map_entry; entry > 0x40; )
						{
							map_entry = m_mem[ZX_OBJS_Y + (entry & 0x3f)] << 4;
							entry = m_mem[ZX_OBJS_UNDER + (entry & 0x3f)];
						}
					}

					if (xx == 0 && zz == 0)
					{
						tile_entry = map_entry;
						bool alt_colour = ((x ^ z) & 1) != 0;

						if (tile_entry & 0xf)
							colour = alt_colour ? 0x10 : 0x11;	// sloped
						else
							colour = alt_colour ? 0x1 : 0x3;	// flat
					}

					float xxx = (map_x - (SENTINEL_MAP_SIZE / 2)) - 0.5f;
					float yyy = (map_entry >> 4) * 1.0f;
					float zzz = (map_z - (SENTINEL_MAP_SIZE / 2)) - 0.5f;

					Vertex v{ xxx, yyy, zzz, colour, xx ? 1.0f : 0.0f, zz ? 1.0f : 0.0f };
					tile_vertices[zz * 2 + xx] = std::move(v);
				}
			}

			auto base_vertex = static_cast<uint32_t>(vertices.size());

			switch (tile_entry & 0xf)
			{
			// Simple case: 4 vertices for 2 triangles (shared normals)
			case 0b0000:	// flat
			case 0b0001:	// slope facing back
			case 0b0101:	// slope facing right
			case 0b1001:	// slope facing front
			case 0b1101:	// slope facing left
				vertices.insert(vertices.end(), tile_vertices.begin(), tile_vertices.end());

				indices.push_back(base_vertex + 0);
				indices.push_back(base_vertex + 2);
				indices.push_back(base_vertex + 3);

				indices.push_back(base_vertex + 0);
				indices.push_back(base_vertex + 3);
				indices.push_back(base_vertex + 1);
				break;

			// Corners: 6 vertices for 2 triangles (separate normals)
			case 0b0010:	// inside corner, facing front
			case 0b0011:	// inside corner, facing back
			case 0b0100:	// stretched faces
			case 0b1010:	// outside corner, facing front left
			case 0b1011:	// outside corner, facing back right
			case 0b1100:	// flat diagonal diamond
				vertices.push_back(tile_vertices[0]);
				vertices.push_back(tile_vertices[2]);
				vertices.push_back(tile_vertices[3]);
				indices.push_back(base_vertex + 0);
				indices.push_back(base_vertex + 1);
				indices.push_back(base_vertex + 2);

				vertices.push_back(tile_vertices[0]);
				vertices.push_back(tile_vertices[3]);
				vertices.push_back(tile_vertices[1]);
				indices.push_back(base_vertex + 3);
				indices.push_back(base_vertex + 4);
				indices.push_back(base_vertex + 5);
				break;

			// Corners (other diagonal): 6 vertices for 2 triangles (separate normals)
			case 0b0110:	// outside corner, facing front right
			case 0b0111:	// inside corner, facing front right
			case 0b1110:	// outside edge, facing back left
			case 0b1111:	// inside edge, facing back left
				vertices.push_back(tile_vertices[0]);
				vertices.push_back(tile_vertices[2]);
				vertices.push_back(tile_vertices[1]);
				indices.push_back(base_vertex + 0);
				indices.push_back(base_vertex + 1);
				indices.push_back(base_vertex + 2);

				vertices.push_back(tile_vertices[1]);
				vertices.push_back(tile_vertices[2]);
				vertices.push_back(tile_vertices[3]);
				indices.push_back(base_vertex + 3);
				indices.push_back(base_vertex + 4);
				indices.push_back(base_vertex + 5);
				break;

			case 0b1000:	// unused
				break;
			}
		}
	}

	auto pVertices = std::make_shared<std::vector<Vertex>>(std::move(vertices));
	auto pIndices = std::make_shared<std::vector<uint32_t>>(std::move(indices));

	auto landscape = Model{ pVertices, pIndices, ModelType::Landscape };
	landscape.pos.x = SENTINEL_MAP_SIZE / 2;
	landscape.pos.z = SENTINEL_MAP_SIZE / 2;
	return landscape;
}

std::vector<Model> Spectrum::ExtractText() const
{
	std::vector<Model> models;

	for (int z = 0; z < SENTINEL_MAP_SIZE - 1; ++z)
	{
		for (int x = 0; x < SENTINEL_MAP_SIZE - 1; ++x)
		{
			auto offset = GetMapAddress(x, z);
			auto map_entry = m_mem[offset] & 0xf;

			if (map_entry)
			{
				auto model = GetModel(static_cast<ModelType>(map_entry & 0x0f));
				model.pos.x = static_cast<float>(x);
				model.pos.y = 2.0f;
				model.pos.z = static_cast<float>(z);
				model.rot.y = XM_PI;	// rotate 180 degrees.
				models.push_back(std::move(model));
			}
		}
	}

	return models;
}

std::vector<Model> Spectrum::ExtractPlacedModels() const
{
	std::vector<Model> placed_models;

	for (int idx = 0; idx < MAX_OBJECTS; ++idx)
	{
		auto model = GetModel(idx);

		// Models are always upright, the pitch is for player view only.
		model.rot.x = 0.0f;

		if (model.type != ModelType::Unknown)
			placed_models.push_back(std::move(model));
	}

	return placed_models;
}

Model Spectrum::ExtractPlayerModel() const
{
	auto idx = m_mem[ZX_PLAYER_OBJ_IDX_ADDR];
	auto model = GetModel(idx, true);

	// Note: Player object contains view pitch, not draw pitch!
	model.rot.x = PitchToRadians(m_mem[ZX_OBJS_PITCH + (idx & 0x3f)]);

	return model;
}

std::vector<Model> Spectrum::ExtractModels()
{
	auto vertex_indices = array_slice<NUM_MODELS + 1>(m_mem, ZX_VERTEX_INDICES_ADDR);
	auto face_indices = array_slice<NUM_MODELS + 1>(m_mem, ZX_FACE_INDICES_ADDR);

	auto angles = array_slice<NUM_VERTICES>(m_mem, ZX_COORDS_ADDR + NUM_VERTICES * 0);
	auto ys = array_slice<NUM_VERTICES>(m_mem, ZX_COORDS_ADDR + NUM_VERTICES * 1);
	auto mags = array_slice<NUM_VERTICES>(m_mem, ZX_COORDS_ADDR + NUM_VERTICES * 2);

	auto face_addr_lsbs = array_slice<NUM_VERTICES>(m_mem, ZX_FACE_LSBS_ADDR);
	auto face_addr_msbs = array_slice<NUM_VERTICES>(m_mem, ZX_FACE_MSBS_ADDR);

	std::array<uint16_t, NUM_VERTICES> addrs;
	std::array<Vertex, NUM_VERTICES> all_vertices;

	for (int i = 0; i < NUM_VERTICES; ++i)
	{
		addrs[i] = (face_addr_msbs[i] << 8) | face_addr_lsbs[i];
		all_vertices[i] = PolarToCartesian(angles[i], ys[i], mags[i]);
	}

	m_models.clear();

	for (int m = 0; m < NUM_MODELS; ++m)
	{
		std::vector<Vertex> vertices;
		std::vector<uint32_t> indices;

		for (auto f = face_indices[m]; f < face_indices[m + 1]; ++f)
		{
			auto base_face_index = static_cast<DWORD>(vertices.size());

			int fv;
			auto poly_base = &m_mem[addrs[f]];
			for (fv = 0; !fv || poly_base[fv] != poly_base[0]; ++fv)
			{
				auto vertex = all_vertices[vertex_indices[m] + poly_base[fv] - BASE_VERTEX_INDEX];
				vertex.colour = face_colours[f] & 0xf;	// use fill colour
				vertex.texcoord.x = (fv & 1) ? 1.0f : 0.0f;
				vertex.texcoord.y = (fv & 2) ? 1.0f : 0.0f;

				vertices.push_back(std::move(vertex));
			}

			// Add a triangle.
			indices.push_back(base_face_index);
			indices.push_back(base_face_index + 1);
			indices.push_back(base_face_index + 2);

			// A faces with 4 is a quad, so add the second triangle.
			if (fv == 4)
			{
				indices.push_back(base_face_index);
				indices.push_back(base_face_index + 2);
				indices.push_back(base_face_index + 3);
			}
		}

		auto pVertices = std::make_shared<std::vector<Vertex>>(std::move(vertices));
		auto pIndices = std::make_shared<std::vector<uint32_t>>(std::move(indices));

		auto model = Model{ pVertices, pIndices, static_cast<ModelType>(m) };
		m_models.push_back(std::move(model));
	}

	return m_models;
}

struct CharBlock
{
	int x{}, y{};
	int w{}, h{};
};

std::vector<CharBlock> BitsToBlocks(std::vector<uint8_t> bits)
{
	std::vector<CharBlock> blocks;
	bool again{};

	do
	{
		again = false;

		for (auto it = bits.begin(); it != bits.end(); ++it)
		{
			if (uint8_t bitmap = *it)
			{
				int start, end;
				for (start = 7; (start > 0) && !(bitmap & (1 << start)); --start);
				for (end = start; (end >= 0) && (bitmap & (1 << end)); --end);

				uint8_t mask = ((1 << (start + 1)) - 1) & ~((1 << (end + 1)) - 1);


				auto it2 = it;
				for (; it2 != bits.end() && ((*it2 & mask) == mask); ++it2)
					*it2 &= ~mask;

				blocks.push_back({
				   7 - start,
				   static_cast<int>(std::distance(bits.begin(), it)),
				   start - end,
				   static_cast<int>(std::distance(it, it2))
					});

				again = true;
				break;
			}
		}
	} while (again);

	return blocks;
}

void AppendExtrudedBlock(const CharBlock& block, std::vector<Vertex>& char_vertices, std::vector<uint32_t>& char_indices, uint32_t colour, float scale_x, float scale_y)
{
	float z_front = 0.0f;
	float z_back = 0.2f;

	constexpr auto origin_adjust = 0.0f;

	auto x = (block.x - origin_adjust) * scale_x;
	auto y = ((7 - block.y) - origin_adjust) * scale_y;
	auto w = block.w * scale_x;
	auto h = block.h * scale_y;

	std::vector<Vertex> corner_vertices
	{
	   { x,     y,     z_front,  colour,  0.0f, 1.0f },
	   { x + w, y,     z_front,  colour,  1.0f, 1.0f },
	   { x,     y - h, z_front,  colour,  0.0f, 0.0f },
	   { x + w, y - h, z_front,  colour,  1.0f, 0.0f },

	   { x,     y,     z_back,   colour,  0.0f, 1.0f },
	   { x + w, y,     z_back,   colour,  1.0f, 1.0f },
	   { x,     y - h, z_back,   colour,  0.0f, 0.0f },
	   { x + w, y - h, z_back,   colour,  1.0f, 0.0f },
	};

	std::vector<std::vector<uint32_t>> face_indices
	{
		{ 0, 1, 2, 3 },   // front
		{ 4, 5, 0, 1 },   // top
		{ 1, 5, 3, 7 },   // right
		{ 2, 3, 6, 7 },   // bottom
		{ 4, 0, 6, 2 },   // left
		{ 5, 4, 7, 6 },   // back
	};

	// Offset indices to account for vertices already in the buffer.
	for (auto fi : face_indices)
	{
		auto base_vertex = static_cast<int>(char_vertices.size());

		for (auto i : fi)
			char_vertices.push_back(corner_vertices[i]);

		static std::vector<uint32_t> triangle_indices{ 0, 1, 2,  1, 3, 2 };
		for (auto& idx : triangle_indices)
			char_indices.push_back(base_vertex + idx);
	}
}

Model Spectrum::CharToModel(char ch, int colour) const
{
	constexpr float scale_x = 0.1f;
	constexpr float scale_y = 0.05f;

	auto addr = ZX_GAME_FONT_ADDR + (ch - ' ') * 8;
	std::vector<uint8_t> char_data(m_mem.begin() + addr, m_mem.begin() + addr + 8);

	auto blocks = BitsToBlocks(char_data);

	std::vector<Vertex> vertices;
	std::vector<uint32_t> indices;
	for (const auto& block : blocks)
		AppendExtrudedBlock(block, vertices, indices, colour, scale_x, scale_y);

	auto pVertices = std::make_shared<std::vector<Vertex>>(std::move(vertices));
	auto pIndices = std::make_shared<std::vector<uint32_t>>(std::move(indices));

	return Model{ pVertices, pIndices, ModelType::Letter };
}

Model Spectrum::IconToModel(int icon_idx, int colour)
{
	constexpr float scale_x = 0.1f;
	constexpr float scale_y = 0.1f;

	auto key = std::make_pair(icon_idx, colour);
	auto it = m_icon_cache.find(key);
	if (it != m_icon_cache.end())
		return it->second;

	auto addr = ZX_PANEL_ICONS_ADDR + icon_idx * 8;
	std::vector<uint8_t> char_data(m_mem.begin() + addr, m_mem.begin() + addr + 7);

	auto blocks = BitsToBlocks(char_data);

	std::vector<Vertex> vertices;
	std::vector<uint32_t> indices;
	for (const auto& block : blocks)
		AppendExtrudedBlock(block, vertices, indices, colour, scale_x, scale_y);

	auto pVertices = std::make_shared<std::vector<Vertex>>(std::move(vertices));
	auto pIndices = std::make_shared<std::vector<uint32_t>>(std::move(indices));

	auto model = Model{ pVertices, pIndices, ModelType::Icon };
	m_icon_cache[key] = model;
	return model;
}

std::vector<XMFLOAT4> Spectrum::GetGamePalette(int num_sentries) const
{
	auto palette = game_ega_palette;

	// If no count given, read the number of sentries on the current landscape.
	if (num_sentries < 0)
		num_sentries = m_mem[ZX_NUM_SENTS] - 1;

	// Some palette colours are determined by the number of sentries.
	num_sentries &= 0x7;
	palette[1] = pal1_colours[num_sentries];
	palette[2] = pal2_colours[num_sentries];
	palette[3] = pal3_colours[num_sentries];

	XMFLOAT4 colour1, colour2;
	constexpr auto factor1 = 0.6f, factor2 = 0.7f;
	auto slope_colour = XMLoadFloat4(&palette[2]);

	// Generate two similar colour shades for the slopes, based off the sentry count colour.
	XMStoreFloat4(&colour1, XMVectorMultiply(slope_colour, { factor1, factor1, factor1, 1.0f }));
	XMStoreFloat4(&colour2, XMVectorMultiply(slope_colour, { factor2, factor2, factor2, 1.0f }));

	palette.push_back(colour1);	// index 0x10: darker slope colour
	palette.push_back(colour2);	// index 0x11: lighter slope colour

	return palette;
}

std::vector<XMFLOAT4> Spectrum::GetTitlePalette() const
{
	return title_ega_palette;
}

void Spectrum::SetPlayerPitch(float radians)
{
	// Convert to internal yaw value, and round to nearest rotation step.
	uint8_t pitch_value = 0xf5 -
		static_cast<signed char>(((radians * 256.0f - 11.0f) / 6.25f));

	auto player_idx = m_mem[ZX_PLAYER_OBJ_IDX_ADDR];
	m_mem[ZX_OBJS_PITCH + player_idx] = static_cast<uint8_t>(pitch_value);
}

void Spectrum::SetPlayerYaw(float radians)
{
	auto yaw_value = static_cast<int>(radians * 256.0f / XM_2PI);

	auto player_idx = m_mem[ZX_PLAYER_OBJ_IDX_ADDR];
	m_mem[ZX_OBJS_YAW + player_idx] = static_cast<uint8_t>(yaw_value);
}

SeenState Spectrum::GetPlayerSeenState() const
{
	auto seen_indicator = m_mem[ZX_SEEN_INDICATOR_FLAGS_ADDR];
	auto player_seen_flags = m_mem[ZX_PLAYER_SEEN_FLAGS_ADDR];

	if (!seen_indicator)
		return SeenState::Unseen;
	else if (player_seen_flags != 0x40)
		return SeenState::FullSeen;
	else
		return SeenState::HalfSeen;
}

Vertex Spectrum::PolarToCartesian(uint8_t yaw, float y, uint8_t mag) const
{
	constexpr auto y_scale = 2.0f;

	Vertex vertex;
	vertex.pos.x = mag * sin(YawToRadians(yaw)) / 256.0f;
	vertex.pos.y = ((y < 128.0f) ? y : (128.0f - y)) * y_scale / 256.0f;
	vertex.pos.z = mag * cos(YawToRadians(yaw)) / 256.0f;
	return vertex;
}

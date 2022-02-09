#include "stdafx.h"
#include "Audio.h"

#define fourccRIFF 'FFIR'
#define fourccDATA 'atad'
#define fourccFMT  ' tmf'
#define fourccWAVE 'EVAW'

Audio::Audio()
{
	DWORD creationFlags = XAUDIO2_1024_QUANTUM;
#if defined(_DEBUG)
	creationFlags |= XAUDIO2_DEBUG_ENGINE;
#endif

	auto hr = XAudio2Create(m_pXAudio2.GetAddressOf(), creationFlags);
	if (SUCCEEDED(hr))
	{
		IXAudio2MasteringVoice* pMasteringVoice{ nullptr };
		hr = m_pXAudio2->CreateMasteringVoice(&pMasteringVoice, 2, 44100);
		if (SUCCEEDED(hr))
			m_pMasteringVoice.reset(pMasteringVoice);
		else
			m_pXAudio2.Reset();
	}

	DWORD dwChannelMask{};
	if (SUCCEEDED(hr))
		hr = m_pMasteringVoice->GetChannelMask(&dwChannelMask);

	if (SUCCEEDED(hr))
		X3DAudioInitialize(dwChannelMask, X3DAUDIO_SPEED_OF_SOUND, m_hX3D);
}

Audio::~Audio()
{
	Stop();
	m_pMasteringVoice.reset();
	m_pXAudio2.Reset();
}

bool Audio::Available() const
{
	return m_pXAudio2 && m_pMasteringVoice;
}

bool Audio::IsPlaying(AudioType type) const
{
	for (auto& sound : m_playingSounds)
	{
		if (type != AudioType::Unknown && sound.type != type)
			continue;

		return IsPlaying(sound.voice);
	}

	return false;
}

bool Audio::SetMusicVolume(float volume)
{
	for (auto& sound : m_playingSounds)
	{
		if (sound.type == AudioType::Music)
		{
			std::array<float, 2> volumes{ volume, volume };
			auto hr = sound.voice->SetChannelVolumes(
				static_cast<UINT32>(volumes.size()), volumes.data());
			return SUCCEEDED(hr);
		}
	}

	return false;
}

bool Audio::SetMusicPlaying(bool play)
{
	for (auto& sound : m_playingSounds)
	{
		if (sound.type == AudioType::Music)
		{
			HRESULT hr{ S_OK };

			if (sound.playing != play)
			{
				hr = play ? sound.voice->Start() : sound.voice->Stop();
				sound.playing = play;
			}

			return SUCCEEDED(hr);
		}
	}

	return false;
}

float Audio::LengthInSeconds(const std::wstring& filename)
{
	const auto it = m_soundBank.find(filename);
	if (it == m_soundBank.end())
		return 0.0f;

	auto& wfx = *reinterpret_cast<WAVEFORMATEX*>(it->second.first.data());
	return static_cast<float>(it->second.second.size()) / wfx.nAvgBytesPerSec;
}

void Audio::LoadWAV(fs::path path)
{
	HANDLE hFile = CreateFile(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, 0, NULL);
	if (INVALID_HANDLE_VALUE == hFile)
	{
		auto str = "File not found: " + to_string(path);
		throw std::runtime_error(str);
	}

	SetFilePointer(hFile, 0, NULL, FILE_BEGIN);

	try
	{
		DWORD dwChunkSize, dwChunkPosition;
		FindChunk(hFile, fourccRIFF, dwChunkSize, dwChunkPosition);

		DWORD filetype;
		ReadChunkData(hFile, &filetype, sizeof(filetype), dwChunkPosition);
		if (filetype != fourccWAVE)
			throw std::exception("not a WAV");

		WAVEFORMATEXTENSIBLE wfx{};
		FindChunk(hFile, fourccFMT, dwChunkSize, dwChunkPosition);
		std::vector<uint8_t> format(dwChunkSize);
		ReadChunkData(hFile, format.data(), dwChunkSize, dwChunkPosition);

		FindChunk(hFile, fourccDATA, dwChunkSize, dwChunkPosition);
		std::vector<uint8_t> data(dwChunkSize);
		ReadChunkData(hFile, data.data(), dwChunkSize, dwChunkPosition);

		m_soundBank[path.filename()] = { format, data };
	}
	catch (...)
	{
		auto str = "Invalid WAV: " + to_string(path);
		throw std::runtime_error(str);
	}
}

void Audio::FindChunk(HANDLE hFile, DWORD fourcc, DWORD& dwChunkSize, DWORD& dwChunkDataPosition)
{
	SetFilePointer(hFile, 0, NULL, FILE_BEGIN);

	DWORD dwChunkType;
	DWORD dwChunkDataSize;
	DWORD dwRIFFDataSize = 0;
	DWORD dwFileType;
	DWORD bytesRead = 0;
	DWORD dwOffset = 0;

	for (;;)
	{
		DWORD dwRead;
		if (0 == ReadFile(hFile, &dwChunkType, sizeof(DWORD), &dwRead, NULL))
			throw std::exception("short file");

		if (0 == ReadFile(hFile, &dwChunkDataSize, sizeof(DWORD), &dwRead, NULL))
			throw std::exception("short file");

		switch (dwChunkType)
		{
		case fourccRIFF:
			dwRIFFDataSize = dwChunkDataSize;
			dwChunkDataSize = 4;
			if (0 == ReadFile(hFile, &dwFileType, sizeof(DWORD), &dwRead, NULL))
				throw std::exception("short file");
			break;

		default:
			if (INVALID_SET_FILE_POINTER == SetFilePointer(hFile, dwChunkDataSize, NULL, FILE_CURRENT))
				throw std::exception("short file");
		}

		dwOffset += sizeof(DWORD) * 2;

		if (dwChunkType == fourcc)
		{
			dwChunkSize = dwChunkDataSize;
			dwChunkDataPosition = dwOffset;
			return;
		}

		dwOffset += dwChunkDataSize;

		if (bytesRead >= dwRIFFDataSize)
			throw std::exception("short file");
	}
}

void Audio::ReadChunkData(HANDLE hFile, void* buffer, DWORD buffersize, DWORD bufferoffset)
{
	if (INVALID_SET_FILE_POINTER == SetFilePointer(hFile, bufferoffset, NULL, FILE_BEGIN))
		throw std::exception("short file");

	DWORD dwRead;
	if (0 == ReadFile(hFile, buffer, buffersize, &dwRead, NULL))
		throw std::exception("short file");
}

void Audio::PositionSound(Sound& sound)
{
	if (!Available())
		return;

	if (sound.pos.x == 0.0f && sound.pos.y == 0.0f && sound.pos.z == 0.0f)
		return;

	X3DAUDIO_EMITTER emitter{};
	emitter.CurveDistanceScaler = 6.0f;
	emitter.Position = { sound.pos.x, sound.pos.y, sound.pos.z };
	emitter.ChannelCount = sound.wfx.nChannels;

	XAUDIO2_VOICE_DETAILS master_details{};
	m_pMasteringVoice->GetVoiceDetails(&master_details);

	std::vector<FLOAT32> matrix(master_details.InputChannels);

	X3DAUDIO_DSP_SETTINGS dsp_settings{};
	dsp_settings.SrcChannelCount = emitter.ChannelCount;
	dsp_settings.DstChannelCount = master_details.InputChannels;
	dsp_settings.pMatrixCoefficients = matrix.data();

	X3DAudioCalculate(m_hX3D, &m_listener, &emitter, X3DAUDIO_CALCULATE_MATRIX, &dsp_settings);
	sound.voice->SetOutputMatrix(m_pMasteringVoice.get(), emitter.ChannelCount, master_details.InputChannels, matrix.data());
}

bool Audio::IsPlaying(const VoicePtr<IXAudio2SourceVoice>& voice) const
{
	XAUDIO2_VOICE_STATE state{};
	voice->GetState(&state, XAUDIO2_VOICE_NOSAMPLESPLAYED);
	return state.BuffersQueued != 0;
}

void Audio::PositionListener(XMFLOAT3 pos, XMFLOAT3 front_dir, XMFLOAT3 up_dir)
{
	if (!Available())
		return;

	m_listener.OrientFront = { front_dir.x, front_dir.y, front_dir.z };
	m_listener.OrientTop = { up_dir.x, up_dir.y, up_dir.z };
	m_listener.Position = { pos.x, pos.y, pos.z };

	for (auto it = m_playingSounds.begin(); it != m_playingSounds.end(); )
	{
		if (IsPlaying(it->voice))
			PositionSound(*it++);
		else
			it = m_playingSounds.erase(it);
	}
}

bool Audio::Play(const std::wstring& filename, AudioType type, XMFLOAT3 pos, float speed)
{
	if (!Available())
		return false;

	auto it = m_soundBank.find(filename);
	if (it == m_soundBank.end())
		return false;

	IXAudio2SourceVoice* pSourceVoice{ nullptr };
	auto& wfx = *reinterpret_cast<WAVEFORMATEX*>(it->second.first.data());
	auto hr = m_pXAudio2->CreateSourceVoice(&pSourceVoice, &wfx, 0U, 4.0f);
	VoicePtr<IXAudio2SourceVoice> spSourceVoice(pSourceVoice);
	if (FAILED(hr))
		return false;

	XAUDIO2_BUFFER buffer{};
	buffer.AudioBytes = static_cast<DWORD>(it->second.second.size());
	buffer.pAudioData = it->second.second.data();
	buffer.Flags = XAUDIO2_END_OF_STREAM;
	buffer.LoopCount = (type == AudioType::LoopingEffect) ? XAUDIO2_LOOP_INFINITE : 0;

	pSourceVoice->SetFrequencyRatio(speed);
	hr = pSourceVoice->SubmitSourceBuffer(&buffer);
	if (FAILED(hr))
		return false;

	Sound sound{};
	sound.type = type;
	sound.voice = std::move(spSourceVoice);
	sound.wfx = wfx;
	sound.pos = pos;
	sound.loop = (buffer.LoopCount > 0);
	sound.playing = (type != AudioType::Music);	// music not auto-started

	PositionSound(sound);

	if (sound.playing)
	{
		hr = sound.voice->Start();
		if (FAILED(hr))
			return false;
	}

	// Only one tune or music track at a time.
	if (type == AudioType::Tune || type == AudioType::Music)
		Stop(type);

	m_playingSounds.push_back(std::move(sound));

	return true;
}

void Audio::Stop(AudioType type)
{
	for (auto it = m_playingSounds.begin(); it != m_playingSounds.end(); )
	{
		if (type == AudioType::Unknown || it->type == type)
		{
			it->voice->Stop();
			it = m_playingSounds.erase(it);
		}
		else
			++it;
	}
}

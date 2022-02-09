#pragma once

template <typename T>
struct VoiceDeleter { void operator()(T* p) { if (p) p->DestroyVoice(); } };

template <typename T>
using VoicePtr = std::unique_ptr<T, VoiceDeleter<T>>;

enum class AudioType { Unknown, Effect, LoopingEffect, Tune, Music };

class Audio
{
public:
	Audio();
	virtual ~Audio();

	bool Available() const;
	bool IsPlaying(AudioType type = AudioType::Unknown) const;
	bool SetMusicVolume(float volume);
	bool SetMusicPlaying(bool play);
	void LoadWAV(fs::path path);
	float LengthInSeconds(const std::wstring& filename);
	bool Play(const std::wstring& filename, AudioType type = AudioType::Effect, XMFLOAT3 pos = {}, float speed = 1.0f);
	void Stop(AudioType type = AudioType::Unknown);
	void PositionListener(XMFLOAT3 pos, XMFLOAT3 front_dir, XMFLOAT3 up_dir);

protected:
	struct Sound
	{
		AudioType type{ AudioType::Unknown };
		VoicePtr<IXAudio2SourceVoice> voice;
		WAVEFORMATEX wfx{};
		XMFLOAT3 pos{};
		bool loop{ false };
		bool playing{ false };
	};

	void FindChunk(HANDLE hFile, DWORD fourcc, DWORD& dwChunkSize, DWORD& dwChunkDataPosition);
	void ReadChunkData(HANDLE hFile, void* buffer, DWORD buffersize, DWORD bufferoffset);
	bool IsPlaying(const VoicePtr<IXAudio2SourceVoice>& voice) const;
	void PositionSound(Sound& sound);

protected:
	ComPtr<IXAudio2> m_pXAudio2;
	VoicePtr<IXAudio2MasteringVoice> m_pMasteringVoice;

	X3DAUDIO_HANDLE m_hX3D;
	X3DAUDIO_LISTENER m_listener{};

	std::map<std::wstring, std::pair<std::vector<uint8_t>, std::vector<uint8_t>>> m_soundBank;
	std::vector<Sound> m_playingSounds;
};

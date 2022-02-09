#pragma once
#include "Game.h"
#include "Audio.h"
#include "View.h"

static constexpr auto HRTF_ENABLED_KEY{ L"EnableHRTF" };
static constexpr auto DEFAULT_HRTF_ENABLED = true;

class Application
{
public:
	Application(HINSTANCE hinst);
	virtual ~Application() = default;
	Application(const Application&) = delete;

	bool Init();
	void Run();

	static HWND Hwnd();
	static LRESULT CALLBACK StaticWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
	static INT_PTR CALLBACK StaticDialogProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

protected:
	void ProcessCommandLine();
	bool InitializeWindow(int width, int height);
	void ActivateWindow(bool active);
	void SaveWindowPosition(HWND hwnd_);
	bool RestoreWindowPosition(HWND hwnd_, const RECT& rDefault);

	LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
	INT_PTR CALLBACK DialogProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

	static Application* s_pApp;
	std::shared_ptr<View> m_pView;
	std::shared_ptr<Audio> m_pAudio;
	std::unique_ptr<Game> m_pGame;

	HINSTANCE m_hinst{ NULL };
	HWND m_hwnd{ NULL };
	bool m_maximised{ true };
	ViewMode m_viewMode{ ViewMode::Unspecified };

	bool m_bWindowActive{ false };
	bool m_bIgnoreMouseMove{ false };
};

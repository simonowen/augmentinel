#include "stdafx.h"
#include "resource.h"
#include "Application.h"
#include "Augmentinel.h"
#include "FlatView.h"
#include "VRView.h"
#include "Settings.h"

// Initial window size, aspect corrected.
static constexpr auto WINDOW_WIDTH = 1600;
static constexpr auto WINDOW_HEIGHT = WINDOW_WIDTH * 9 / 16;
static constexpr auto WINDOW_CAPTION = TEXT(APP_NAME) TEXT(" ") TEXT(APP_VERSION);

static constexpr auto MAX_ACCUMULATED_TIME = 0.25f;

static constexpr auto WINDOW_POS_KEY = L"WindowPos";

/*static*/ Application* Application::s_pApp;

Application::Application(HINSTANCE hinst)
	: m_hinst(hinst)
{
}

bool Application::Init()
{
	auto hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
	if (FAILED(hr))
		return false;

	INITCOMMONCONTROLSEX icce{ sizeof(icce), ICC_BAR_CLASSES };
	InitCommonControlsEx(&icce);

	InitSettings(APP_NAME);
	ProcessCommandLine();

	if (!InitializeWindow(WINDOW_WIDTH, WINDOW_HEIGHT))
		throw std::exception("failed to create window");

	if (m_viewMode == ViewMode::Unspecified)
	{
		auto ret = DialogBox(m_hinst, MAKEINTRESOURCE(IDD_LAUNCHER), NULL, StaticDialogProc);
		switch (ret)
		{
		case IDB_PLAY:
			m_viewMode = ViewMode::Flat;
			break;
		case IDB_PLAY_VR:
			m_viewMode = ViewMode::VR;
			break;
		default:
			return false;
		}
	}

	if (m_viewMode != ViewMode::VR)
		m_pView = make_shared_aligned<FlatView>(m_hwnd);
	else
	{
		if (OpenVR::IsAvailable())
			m_pView = make_shared_aligned<VRView>(m_hwnd);
		else
			throw std::exception("OpenVR-compatible headset not found.");
	}

	auto hrtf_enabled = GetFlag(HRTF_ENABLED_KEY, DEFAULT_HRTF_ENABLED);
	if (hrtf_enabled)
	{
		// Load the hook DLL that adds HRTF support to X3D applications.
		LoadLibrary(X3D_HRTF_HOOK_DLL);
	}

	m_pAudio = std::make_shared<Audio>();
	m_pGame = std::make_unique<Augmentinel>(m_pView, m_pAudio);

	ShowWindow(m_hwnd, m_maximised ? SW_SHOWMAXIMIZED : SW_SHOW);

	if (m_viewMode == ViewMode::VR)
		ActivateWindow(true);

	return true;
}

void Application::Run()
{
	auto tLastFrame = std::chrono::high_resolution_clock::now();

	MSG msg{};
	while (msg.message != WM_QUIT)
	{
		while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);

			if (msg.message == WM_QUIT)
				break;
		}

		auto tNow = std::chrono::high_resolution_clock::now();
		auto elapsed_seconds = std::chrono::duration<float, std::chrono::seconds::period>(tNow - tLastFrame).count();
		elapsed_seconds = std::min(elapsed_seconds, MAX_ACCUMULATED_TIME);	// limit accumulated time
		tLastFrame = tNow;

		m_pGame->Frame(elapsed_seconds);
		m_pView->BeginScene();
		m_pView->Render(m_pGame.get());
		m_pView->EndScene();
	}

	DestroyWindow(m_hwnd);
	m_hwnd = NULL;
}

void Application::ProcessCommandLine()
{
	for (int arg = 1; arg < __argc; ++arg)
	{
		if (!lstrcmpiA(__argv[arg], "--vr") || !lstrcmpiA(__argv[arg], "--openvr"))
			m_viewMode = ViewMode::VR;
		else if (!lstrcmpiA(__argv[arg], "--flat") || !lstrcmpiA(__argv[arg], "--no-vr"))
			m_viewMode = ViewMode::Flat;
	}
}

bool Application::InitializeWindow(int width, int height)
{
	WNDCLASSEX wc{};
	wc.cbSize = sizeof(WNDCLASSEX);
	wc.style = CS_HREDRAW | CS_VREDRAW;
	wc.lpfnWndProc = StaticWndProc;
	wc.hInstance = GetModuleHandle(NULL);
	wc.hIcon = LoadIcon(wc.hInstance, MAKEINTRESOURCE(IDI_ICON));
	wc.hCursor = LoadCursor(NULL, IDC_ARROW);
	wc.hbrBackground = GetStockBrush(BLACK_BRUSH);
	wc.lpszClassName = TEXT(APP_NAME);

	if (!RegisterClassEx(&wc))
		return false;

	s_pApp = this;
	m_hwnd = CreateWindowEx(WS_EX_APPWINDOW, wc.lpszClassName, WINDOW_CAPTION, WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN,
		CW_USEDEFAULT, CW_USEDEFAULT, width, height, NULL, NULL, wc.hInstance, nullptr);

	return m_hwnd != NULL;
}

void Application::ActivateWindow(bool active)
{
	if (active)
	{
		m_bWindowActive = m_bIgnoreMouseMove = true;

		RECT rect;
		GetClientRect(m_hwnd, &rect);
		MapWindowPoints(m_hwnd, NULL, reinterpret_cast<POINT*>(&rect), 2);
		ClipCursor(&rect);
	}
	else
	{
		m_bWindowActive = false;
		ClipCursor(nullptr);
		if (m_pView)
			m_pView->ReleaseKeys();
	}
}

/*static*/ HWND Application::Hwnd()
{
	return s_pApp ? s_pApp->m_hwnd : NULL;
}

/*static*/ LRESULT CALLBACK Application::StaticWndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	return s_pApp ? s_pApp->WndProc(hwnd, uMsg, wParam, lParam) : 0;
}

/*static*/ INT_PTR CALLBACK Application::StaticDialogProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	return s_pApp ? s_pApp->DialogProc(hwnd, uMsg, wParam, lParam) : 0;
}

LRESULT CALLBACK Application::WndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch (uMsg)
	{
	case WM_CREATE:
	{
		auto pcs = reinterpret_cast<CREATESTRUCT*>(lParam);
		RECT rect = { pcs->x, pcs->y, pcs->x + pcs->cx, pcs->y + pcs->cy };
		RestoreWindowPosition(hwnd, rect);
		break;
	}

	case WM_ACTIVATE:
	case WM_ENTERMENULOOP:
		if (uMsg != WM_ACTIVATE || wParam == WA_INACTIVE)
			ActivateWindow(false);
		break;

	case WM_KEYDOWN:
		if (!(lParam & (1 << 30)))
			m_pView->UpdateKey(static_cast<int>(wParam), KeyState::DownEdge);
		break;

	case WM_KEYUP:
		m_pView->UpdateKey(static_cast<int>(wParam), KeyState::UpEdge);
		break;

	case WM_LBUTTONDOWN:
		// Pass on mouse click only if active.
		if (m_bWindowActive)
			m_pView->UpdateKey(VK_LBUTTON, KeyState::DownEdge);
		else
			ActivateWindow(true);
		break;

	case WM_LBUTTONUP:
		m_pView->UpdateKey(VK_LBUTTON, KeyState::UpEdge);
		break;

	case WM_MBUTTONDOWN:
		m_pView->UpdateKey(VK_MBUTTON, KeyState::DownEdge);
		break;

	case WM_MBUTTONUP:
		m_pView->UpdateKey(VK_MBUTTON, KeyState::UpEdge);
		break;

	case WM_RBUTTONDOWN:
		m_pView->UpdateKey(VK_RBUTTON, KeyState::DownEdge);
		break;

	case WM_RBUTTONUP:
		m_pView->UpdateKey(VK_RBUTTON, KeyState::UpEdge);
		break;

	case WM_XBUTTONDOWN:
		m_pView->UpdateKey(VK_XBUTTON1 + HIWORD(wParam) - 1, KeyState::DownEdge);
		break;

	case WM_XBUTTONUP:
		m_pView->UpdateKey(VK_XBUTTON1 + HIWORD(wParam) - 1, KeyState::UpEdge);
		break;

	case WM_MOUSEMOVE:
		if (m_bWindowActive)
		{
			RECT rClient;
			GetClientRect(m_hwnd, &rClient);

			POINT ptCursor;
			GetCursorPos(&ptCursor);
			ScreenToClient(m_hwnd, &ptCursor);

			POINT ptCentre = { rClient.right / 2, rClient.bottom / 2 };
			auto dx = ptCursor.x - ptCentre.x;
			auto dy = ptCursor.y - ptCentre.y;

			if (dx || dy)
			{
				if (m_bIgnoreMouseMove)
					m_bIgnoreMouseMove = false;
				else
					m_pView->MouseMove(dx, dy);

				ClientToScreen(m_hwnd, &ptCentre);
				SetCursorPos(ptCentre.x, ptCentre.y);
			}
		}
		break;

	case WM_SETCURSOR:
		if (m_bWindowActive)
		{
			if (LOWORD(lParam) == HTCLIENT)
				SetCursor(NULL);
			return TRUE;
		}
		break;

	case WM_SIZE:
		if (m_pView)
		{
			RECT rClient{};
			GetClientRect(m_hwnd, &rClient);
			m_pView->OnResize(rClient.right, rClient.bottom);
		}
		break;

	case WM_DESTROY:
		SaveWindowPosition(m_hwnd);
		PostQuitMessage(0);
		return 0;
	}

	return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

INT_PTR CALLBACK Application::DialogProc(HWND hdlg, UINT uMsg, WPARAM wParam, LPARAM /*lParam*/)
{
	switch (uMsg)
	{
	case WM_INITDIALOG:
		SetDlgItemText(hdlg, IDC_LAUNCHER_HEADER, WINDOW_CAPTION);
		EnableWindow(GetDlgItem(hdlg, IDB_PLAY_VR), OpenVR::IsAvailable());
		return TRUE;

	case WM_COMMAND:
		switch (LOWORD(wParam))
		{
		case IDB_PLAY:
		case IDB_PLAY_VR:
		case IDCANCEL:
			EndDialog(hdlg, LOWORD(wParam));
			break;
		case IDB_OPTIONS:
			Augmentinel::Options(m_hinst, hdlg);
			break;
		}
		break;
	}

	return FALSE;
}

void Application::SaveWindowPosition(HWND hwnd)
{
	WINDOWPLACEMENT wp = { sizeof(wp) };
	GetWindowPlacement(hwnd, &wp);

	RECT rect{};
	AdjustWindowRectEx(&rect, GetWindowLong(hwnd, GWL_STYLE), FALSE, GetWindowLong(hwnd, GWL_EXSTYLE));
	rect.left = wp.rcNormalPosition.left - rect.left;
	rect.top = wp.rcNormalPosition.top - rect.top;
	rect.right = wp.rcNormalPosition.right - rect.right;
	rect.bottom = wp.rcNormalPosition.bottom - rect.bottom;

	std::wstringstream ss;
	ss << rect.left << "," << rect.top << "," <<
		(rect.right - rect.left) << "," <<
		(rect.bottom - rect.top) << "," <<
		IsMaximized(hwnd) ? 1 : 0;
	SetSetting(WINDOW_POS_KEY, ss.str());
}

bool Application::RestoreWindowPosition(HWND hwnd, const RECT& rDefault)
{
	std::wstringstream ss(GetSetting(WINDOW_POS_KEY, L""));
	std::vector<int> values;

	while (ss.good())
	{
		int value{};
		ss >> value;
		values.push_back(value);

		if (ss.peek() == ',')
			ss.ignore();
	}

	WINDOWPLACEMENT wp = { sizeof(wp) };
	wp.showCmd = SW_HIDE;	// keep hidden!

	if (values.size() >= 5)
	{
		auto x = values[0], y = values[1];
		auto width = values[2], height = values[3];
		m_maximised = values[4] != 0;
		SetRect(&wp.rcNormalPosition, x, y, x + width, y + height);
	}
	else
	{
		wp.rcNormalPosition = rDefault;
		m_maximised = true;
	}

	AdjustWindowRectEx(&wp.rcNormalPosition, GetWindowLong(hwnd, GWL_STYLE), FALSE, GetWindowLong(hwnd, GWL_EXSTYLE));

	return SetWindowPlacement(hwnd, &wp) == TRUE;
}

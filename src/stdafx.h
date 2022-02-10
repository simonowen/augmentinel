#pragma once
#define _SILENCE_CXX17_CODECVT_HEADER_DEPRECATION_WARNING	// ToDo: switch to ANSI for portability?

#include "targetver.h"

#define APP_NAME	"Augmentinel"
#define APP_VERSION	"v1.6.0"

#include <array>
#include <vector>
#include <map>
#include <set>
#include <fstream>
#include <chrono>
#include <functional>
#include <locale>
#include <codecvt>
#include <random>
#include <functional>
#include <sstream>
#include <iomanip>
#include <filesystem>
namespace fs = std::filesystem;

#define NOMINMAX
#include <windows.h>
#include <windowsx.h>
#include <shlobj.h>
#include <shlobj_core.h>
#include <VersionHelpers.h>

#include <d3d11.h>
#include <math.h>
#include <DirectXMath.h>
#include <DirectXCollision.h>

#ifdef HAVE_XAUDIO2REDIST
#include <xaudio2redist/xaudio2redist.h>
#include <xaudio2redist/xaudio2fx.h>
#include <xaudio2redist/x3daudio.h>
#include <xaudio2redist/xapofx.h>
#elif (_WIN32_WINNT < _WIN32_WINNT_WIN8)
#error Windows 7 support requires xaudio2redist from vcpkg
#else
#include <xaudio2.h>
#include <xaudio2fx.h>
#include <x3daudio.h>
#include <xapofx.h>
#endif


#ifdef _WIN64
constexpr auto X3D_HRTF_HOOK_DLL = L"hrtf/x64/x3daudio1_7.dll";
#else
constexpr auto X3D_HRTF_HOOK_DLL = L"hrtf/x86/x3daudio1_7.dll";
#endif

#include <wrl/client.h>
using Microsoft::WRL::ComPtr;

#include <dxgi1_5.h>
#include <d3d11.h>
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3d11.lib")
#include <DirectXMath.h>
using namespace DirectX;

#include "SharedConstants.h"
#include "Utils.h"
#include "Sentinel.h"

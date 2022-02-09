#include "stdafx.h"
#include "Application.h"

void Fail(HRESULT hr, LPCWSTR pszOperation)
{
	if (FAILED(hr))
	{
		char sz[256];
		wsprintfA(sz, "%S failed with %08lx", pszOperation, hr);
		ShowWindow(Application::Hwnd(), SW_HIDE);
#ifdef _DEBUG
		__debugbreak();
#endif
		throw std::exception(sz);
	}
}

std::wstring WindowText(HWND hwnd)
{
	auto len = GetWindowTextLength(hwnd) + 1;	// include null terminator
	std::vector<wchar_t> str(len);
	GetWindowText(hwnd, str.data(), static_cast<int>(str.size()));
	return str.data();
}

fs::path ModulePath(HMODULE hmod)
{
	wchar_t wpath[MAX_PATH];
	GetModuleFileName(hmod, wpath, _countof(wpath));
	return wpath;
}

fs::path ModuleDirectory(HMODULE hmod)
{
	return ModulePath(hmod).remove_filename();
}

fs::path WorkingDirectory()
{
	wchar_t wpath[MAX_PATH];
	GetCurrentDirectory(_countof(wpath), wpath);
	return wpath;
}

std::vector<uint8_t> FileContents(const std::wstring& filename)
{
	wchar_t szPath[MAX_PATH];
	GetModuleFileName(NULL, szPath, _countof(szPath));

	// Form a full path relative to the module path.
	std::wstring full_path = szPath;
	full_path = full_path.substr(0, full_path.rfind(L'\\') + 1);
	full_path += filename;

	// Try module relative, falling back to CWD relative path.
	HANDLE hfile = CreateFile(full_path.c_str(), GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
	if (hfile == INVALID_HANDLE_VALUE)
		hfile = CreateFile(filename.c_str(), GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);

	if (hfile == INVALID_HANDLE_VALUE)
	{
		auto str = "File not found: " + to_string(filename);
		throw std::runtime_error(str);
	}

	DWORD dwSize = GetFileSize(hfile, NULL), dwRead;
	std::vector<BYTE> file(dwSize);
	if (!ReadFile(hfile, file.data(), dwSize, &dwRead, NULL))
		file.clear();
	CloseHandle(hfile);

	return file;
}

std::mt19937& random_source()
{
	static std::random_device rd;
	static std::mt19937 rng(rd());
	return rng;
}

uint32_t random_uint32()
{
	return (random_source())();
}

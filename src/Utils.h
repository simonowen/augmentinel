#pragma once

void Fail(HRESULT hr, LPCWSTR pszOperation);
fs::path ModulePath(HMODULE hmod = NULL);
fs::path ModuleDirectory(HMODULE hmod = NULL);
fs::path WorkingDirectory();
std::vector<uint8_t> FileContents(const std::wstring& filename);
std::wstring WindowText(HWND hwnd);


template <typename T>
void aspect_correct(T& current_width, T& current_height, const float aspect_ratio)
{
	auto current_aspect = static_cast<float>(current_width) / current_height;
	if (current_aspect > aspect_ratio)
		current_width = static_cast<T>(current_height * aspect_ratio);
	else
		current_height = static_cast<T>(current_width / aspect_ratio);
}

template<size_t size, typename T>
constexpr auto array_slice(const T& src, const size_t start)
{
	std::array<uint8_t, size> dst;
	std::copy(src.begin() + start, src.begin() + start + size, dst.begin());
	return dst;
}

// Work-around for using make_shared with extended aligned storage sizes.
template <typename T, typename ...Args>
std::shared_ptr<T> make_shared_aligned(Args&& ...args)
{
	return std::shared_ptr<T>(new T(std::forward<Args>(args)...));
}

inline bool operator==(XMFLOAT3& l, XMFLOAT3& r)
{
	return l.x == r.x && l.y == r.y && l.z == r.z;
}

inline bool operator!=(XMFLOAT3& l, XMFLOAT3& r)
{
	return l.x != r.x || l.y != r.y || l.z != r.z;
}

inline std::wstring to_wstring(const std::string& str)
{
	using convert_typeX = std::codecvt_utf8<wchar_t>;
	std::wstring_convert<convert_typeX, wchar_t> conv;
	return conv.from_bytes(str);
}

inline std::string to_string(const std::wstring& wstr)
{
	using convert_typeX = std::codecvt_utf8<wchar_t>;
	std::wstring_convert<convert_typeX, wchar_t> conv;
	return conv.to_bytes(wstr);
}

inline float pitch_from_dir(const XMFLOAT3& dir)
{
	return std::asin(-dir.y);
}

inline float yaw_from_dir(const XMFLOAT3& dir)
{
	return std::atan2(dir.x, dir.z);
}

std::mt19937& random_source();
uint32_t random_uint32();

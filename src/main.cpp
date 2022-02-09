#include "stdafx.h"
#include "Application.h"

int WINAPI WinMain(
	_In_ HINSTANCE hInstance,
	_In_opt_ HINSTANCE /*hPrevInstance*/,
	_In_ LPSTR /*lpCmdLine*/,
	_In_ int /*nShowCmd*/)
{
	std::unique_ptr<Application> pApplication;

	try
	{
		pApplication = std::make_unique<Application>(hInstance);
		if (pApplication->Init())
			pApplication->Run();
	}
	catch (std::exception& e)
	{
		MessageBoxA(NULL, e.what(), APP_NAME, MB_ICONERROR);
	}

	return 0;
}

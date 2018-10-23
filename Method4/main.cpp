#include "stdafx.h"
#include "..\\hotpatch.h"


__declspec(noinline) void PatchableFunction1()
{
	MessageBox(0, L"Before patch", 0, MB_OK);
}

HOTPATCH hp;
XML3::XML xPatch;


__declspec(noinline) void PatchedFunction1()
{
	MessageBox(0, L"After patch 1", L"Patched function", MB_OK);
}
extern "C" HRESULT __stdcall Patch(HMODULE hM)
{
	HOTPATCH hp2;
	HRESULT hr = hp2.ApplyPatchFor(hM, L"PatchableFunction1", PatchedFunction1, &xPatch);
	return hr;
}

#ifdef _WIN64
#pragma comment(linker, "/EXPORT:Patch=Patch")
#else
#pragma comment(linker, "/EXPORT:Patch=_Patch@4")
#endif


HRESULT PostBuildPatchExecutable()
{
	wchar_t my[1000] = { 0 };
	GetModuleFileName(0, my, 1000);
	auto hr = hp.AutoPatchExecutable({ L"main.obj" });
	if (FAILED(hr))
		MessageBox(0, L"Executable post-patching failed.", 0, 0);
	return hr;
}


extern "C" void wWinMainCRTStartup();
HANDLE hEV = 0;
extern "C" void __stdcall InitCRT(HANDLE hE)
{
	hEV = hE;
	wWinMainCRTStartup();
}


#ifdef _WIN64
#pragma comment(linker, "/EXPORT:InitCRT=InitCRT")
#else
#pragma comment(linker, "/EXPORT:InitCRT=_InitCRT@4")
#endif


int __stdcall wWinMain(HINSTANCE,HINSTANCE,LPWSTR t,int)
{
	if (hEV)
	{
		SetEvent(hEV);
		MSG msg;
		while (GetMessage(&msg, 0, 0, 0))
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
		return 0;
	}
	CoInitialize(0);
	TCHAR a[1000] = { 0 };
	GetModuleFileName(0, a, 1000);

	if (_wcsicmp(t, L"/postbuildpatch") == 0)
	{
		PostBuildPatchExecutable();
		return 0; 
	}

	PatchableFunction1();

	// Do the hotpatching now...
	wstring aa = a;
	wcscat_s(a, 1000, L".patch.exe");
	if (!CopyFile(aa.c_str(), a, false))
	{
		MessageBox(0, L"Patching failed.", 0, 0);
		return 0;
	}

	auto hL = LoadLibrary(a);
	if (!hL)
	{
		MessageBox(0, L"Patching failed.", 0, 0);
		return 0;
	}
	hp.PatchIAT(hL);

	typedef HRESULT(__stdcall *pff)(HANDLE);
	FARPROC fp1 = GetProcAddress(hL, "Patch");
	FARPROC fp2 = GetProcAddress(hL, "InitCRT");
	if (!fp1 || !fp2)
	{
		MessageBox(0, L"Patching failed.", 0, 0);
		return 0;
	}
	pff P = (pff)fp2;
	HANDLE hE = CreateEvent(0, 0, 0, 0);
	std::thread tx([&] {
		P(hE);
	});
	tx.detach();
	pff P2 = (pff)fp1;
	P2(GetModuleHandle(0));

	PatchableFunction1();
	return 0;
}
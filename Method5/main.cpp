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

HINSTANCE hDLL = 0;
BOOL WINAPI DllMain(
	_In_ HINSTANCE hinstDLL,
	_In_ DWORD     fdwReason,
	_In_ LPVOID    lpvReserved
)
{
	hDLL = hinstDLL;
	return true;
}


HRESULT PostBuildPatchExecutable()
{
	wchar_t my[1000] = { 0 };
	GetModuleFileName(hDLL, my, 1000);
	auto hr = hp.AutoPatchExecutable({ L"main.obj" },hDLL);
	if (FAILED(hr))
		MessageBox(0, L"Executable post-patching failed.", 0, 0);
	return hr;
}


extern "C" int __stdcall PostBuildPatch()
{
	CoInitialize(0);
	PostBuildPatchExecutable();
	return 1;
}

#ifdef _WIN64
#pragma comment(linker, "/EXPORT:PostBuildPatch=PostBuildPatch")
#else
#pragma comment(linker, "/EXPORT:PostBuildPatch=_PostBuildPatch@0")
#endif


#ifdef _WIN64
#pragma comment(linker, "/EXPORT:dmain=dmain")
#else
#pragma comment(linker, "/EXPORT:dmain=_dmain@0")
#endif

extern "C" int __stdcall dmain()
{
	CoInitialize(0);
	TCHAR a[1000] = { 0 };
	GetModuleFileName(hDLL, a, 1000);

	PatchableFunction1();

	// Hot patch
	wstring aa = a;
	wcscat_s(a, 1000, L".patch.dll");
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
	typedef HRESULT(__stdcall *pff)(HANDLE);
	FARPROC fp1 = GetProcAddress(hL, "Patch");
	if (!fp1)
	{
		MessageBox(0, L"Patching failed.", 0, 0);
		return 0;
	}
	pff P2 = (pff)fp1;
	P2(hDLL);

	PatchableFunction1();
	return 0;
}
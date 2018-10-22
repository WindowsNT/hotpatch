#include "stdafx.h"



#include "..\\..\\hotpatch.h"
HOTPATCH hp;

XML3::XML xPatch;

__declspec(noinline) void PatchableFunction1()
{
	MessageBox(0, L"After patch 1", L"Patched function", MB_OK);
}


__declspec(noinline) void PatchableFunction2(int x)
{
	wchar_t z[100] = { 0 };
	swprintf_s(z, 100, L"Message: %u", x);
	MessageBox(0, z, L"After patch 2", MB_OK);
}

__declspec(noinline) void PatchableFunction3()
{
	MessageBox(0, L"After patch 3", L"Patched function", MB_OK);
}

__declspec(noinline) void PatchableFunction4()
{
	MessageBox(0, L"After patch 4", L"Patched function", MB_OK);
}

__declspec(noinline) void PatchableFunction4X2()
{
	MessageBox(0, L"After patch 4 in X2", L"Patched function", MB_OK);
}


FARPROC OldMsgBox;

__declspec(noinline) UINT __stdcall NewMessageBox(HWND hh, LPCWSTR mm, LPCWSTR ww, UINT ll)
{
	auto boo = MessageBox;
	unsigned long long a = (unsigned long long)boo;
	a += 2;
	memcpy(&boo, &a, sizeof(void*));
	return boo(hh, mm, L"Hacked Message Box", ll);
}

extern "C" HRESULT __stdcall Patch()
{
	HMODULE hM = GetModuleHandle(0);
	HRESULT hr = 0;

	auto l = LoadLibrary(L"USER32.DLL");
	int* f = (int*)GetProcAddress(l, "MessageBoxW");
	//hr = hp.ApplyPatchForDirect(f, NewMessageBox);

	hr = hp.ApplyPatchFor(hM, L"FOO::PatchableFunction1", PatchableFunction1, &xPatch);
	if (FAILED(hr)) MessageBox(0, L"FOO::PatchableFunction1 failed to be patched", 0, 0);

	hr = hp.ApplyPatchFor(hM, L"PatchableFunction2", PatchableFunction2, &xPatch);
	if (FAILED(hr)) MessageBox(0, L"FOO::PatchableFunction1 failed to be patched", 0, 0);

	hr = hp.ApplyPatchFor(hM, L"X::PatchableFunction3", PatchableFunction3, &xPatch);
	if (FAILED(hr)) MessageBox(0, L"FOO::PatchableFunction1 failed to be patched", 0, 0);
	hr = hp.ApplyPatchFor(hM, L"X::PatchableFunction4", PatchableFunction4, &xPatch);
	if (FAILED(hr)) MessageBox(0, L"FOO::PatchableFunction1 failed to be patched", 0, 0);
	hr = hp.ApplyPatchFor(hM, L"X2::PatchableFunction4", PatchableFunction4X2, &xPatch);
	if (FAILED(hr)) MessageBox(0, L"FOO::PatchableFunction1 failed to be patched", 0, 0);

	return hr;
}



#ifdef _WIN64
#pragma comment(linker, "/EXPORT:Patch=Patch")
#else
#pragma comment(linker, "/EXPORT:Patch=_Patch@0")
#endif


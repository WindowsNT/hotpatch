#include "stdafx.h"
#include "..\\hotpatch.h"


__declspec(noinline) void PatchableFunction1()
{
	MessageBox(0, L"Before patch", 0, MB_OK);
}

HOTPATCH hp;
XML3::XML xPatch;

HRESULT PostBuildPatchExecutable()
{
	wchar_t my[1000] = { 0 };
	GetModuleFileName(0, my, 1000);
	auto hr = hp.AutoPatchExecutable({ L"main.obj" });
	if (FAILED(hr))
		MessageBox(0, L"Executable post-patching failed.", 0, 0);
	return hr;
}

// {F9838246-8E9C-4804-9A39-FCCA17FDE1B6}
static const GUID GUID_TEST =
{ 0xf9838246, 0x8e9c, 0x4804, { 0x9a, 0x39, 0xfc, 0xca, 0x17, 0xfd, 0xe1, 0xb6 } };
#define APPIDTEST L"app.id.{F9838246-8E9C-4804-9A39-FCCA17FDE1B6}"

DWORD mtid = 0;

void EmbeddingStart()
{
	hp.StartCOMServer(GUID_TEST, [](vector<HOTPATCH::NAMEANDPOINTER>& w) -> HRESULT
	{
		w.clear();
		HOTPATCH::NAMEANDPOINTER nap;
		nap.n = L"PatchableFunction1";
		nap.f = [](size_t*) -> size_t
		{
			MessageBox(0, L"Patch from COM Patcher", L"Patched", MB_ICONINFORMATION);
			return 0;
		};
		w.push_back(nap);
		return S_OK;
	},

	[]()
	{
		// We are closing...
		PostThreadMessage(mtid, WM_QUIT, 0, 0);
	}
	);

}

int wmain(int argc,wchar_t** wargv)
{
	CoInitialize(0);
	TCHAR a[1000] = { 0 };
	GetModuleFileName(0, a, 1000);
	hp.PrepareExecutableForCOMPatching(a, GUID_TEST, APPIDTEST);

	if (argc == 2 && (_wcsicmp(wargv[1], L"-embedding") == 0 || _wcsicmp(wargv[1], L"/embedding") == 0))
	{
		mtid = GetCurrentThreadId();
		EmbeddingStart();
		MSG msg;
		while (GetMessage(&msg, 0, 0, 0))
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
		return 0;
	}

	if (argc == 2 && _wcsicmp(wargv[1], L"/postbuildpatch") == 0)
	{
		PostBuildPatchExecutable();
		return 0; 
	}

	PatchableFunction1();

	// Do the hotpatching now...
	hp.StartCOMPatching();
	vector<wstring> pns;
	hp.AckGetPatchNames(pns);
	for (auto& aa : pns)
	{
		hp.ApplyCOMPatchFor(xPatch, GetModuleHandle(0), aa.c_str());
	}
	PatchableFunction1();
	hp.FinishCOMPatching();
}
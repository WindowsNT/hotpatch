#include "stdafx.h"
#include "..\\hotpatch.h"


__declspec(noinline) void PatchableFunction1()
{
	MessageBox(0, L"Before patch", 0, MB_OK);
}

HOTPATCH hp;
XML3::XML xPatch;

DWORD mtid = 0;

// {F9838246-8E9C-4804-9A39-FCCA17FDE1B7}
static const GUID GUID_TEST =
{ 0xf9838246, 0x8e9c, 0x4804, { 0x9a, 0x39, 0xfc, 0xca, 0x17, 0xfd, 0xe1, 0xb7 } };


void USMStart()
{
	hp.StartUSMServer(GUID_TEST, [](vector<HOTPATCH::NAMEANDPOINTER>& w) -> HRESULT
	{
		HOTPATCH::NAMEANDPOINTER nap;
		nap.n = L"PatchableFunction1";

		TCHAR cidx[1000] = { 0 };
		StringFromGUID2(GUID_TEST, cidx, 1000);
		swprintf_s(cidx + wcslen(cidx), 1000 - wcslen(cidx), L"-%u", 0);
		nap.mu = (mutual*)new mutual(cidx, [&](unsigned long long)
		{
			MessageBox(0, L"Patch from USM Patcher", L"Patched", MB_ICONINFORMATION);
			return;
		}, 0, true);
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

HRESULT PostBuildPatchExecutable()
{
	wchar_t my[1000] = { 0 };
	GetModuleFileName(0, my, 1000);
	auto hr = hp.AutoPatchExecutable({ L"main.obj" });
	if (FAILED(hr))
		MessageBox(0, L"Executable post-patching failed.", 0, 0);
	return hr;
}




int wmain(int argc,wchar_t** wargv)
{
	CoInitialize(0);

	TCHAR a[1000] = { 0 };
	GetModuleFileName(0, a, 1000);
	hp.PrepareExecutableForUSMPatching(a, GUID_TEST);

	if (argc == 2 && (_wcsicmp(wargv[1], L"-usm") == 0 || _wcsicmp(wargv[1], L"/usm") == 0))
	{
		mtid = GetCurrentThreadId();
		USMStart();
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

	wcscat_s(a, 1000, L" /usm");
	// Run it with /USM
	STARTUPINFO sInfo = { 0 };
	sInfo.cb = sizeof(sInfo);
	PROCESS_INFORMATION pi = { 0 };
	if (!CreateProcess(0, a, 0, 0, 0, 0, 0, 0, &sInfo, &pi))
		return 0;
	CloseHandle(pi.hThread);
	CloseHandle(pi.hProcess);

	// Get the names
	vector<wstring> pns;
	hp.AckGetPatchNames(pns);
	for (size_t i = 0; i < pns.size(); i++)
	{
		auto& aa = pns[i];
		hp.ApplyUSMPatchFor(xPatch, GetModuleHandle(0), aa.c_str(), i);
	}


	PatchableFunction1();


	hp.EndUSMServer();
}
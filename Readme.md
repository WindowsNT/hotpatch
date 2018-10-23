# HotPatch
A single-header C++ library for Windows to create hotpatchable images and apply hotpatches with 5 methods

Article at CodeProject: https://www.codeproject.com/Articles/1043089/HotPatching-Deep-Inside

## Executable to be hot-patched preparation

1. Build the executable in ***release*** mode from solution
2. The solution is automatically configured to run the executable with parameter /postbuildpatch which updates itself with the patch information. It uses BeginUpdateResource and EndUpdateResource.
Note that these are frequently stopped by antivirus. You can also use the included pig.exe which is a standalone app to read a file and its pdf, and put the hotpatch data inside it.

## Method 1: Using a DLL to patch an Executable 

1. Load the DLL from the executable and call an exported function (say, Patch()).
2. Call hp.ApplyPatchFor() for each function you want to be patched:


```C++
hr = hp.ApplyPatchFor(hM, L"FOO::PatchableFunction1", PatchableFunction1, &xPatch);
```

## Method 2: Using the same executable as a COM server

1. Call hp.PrepareExecutableForCOMPatching();
2. If embedding, start the COM server, specifying the patches and installing a message loop:

```C++

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

// main
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
	
```

If main, start the patching process

```C++
hp.StartCOMPatching();
vector<wstring> pns;
hp.AckGetPatchNames(pns);
for (auto& aa : pns)
{
	hp.ApplyCOMPatchFor(xPatch, GetModuleHandle(0), aa.c_str());
}
...
// End app
hp.FinishCOMPatching();
```

More methods to follow. The article already explains them, stay alert.




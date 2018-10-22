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

More methods to follow. The article already explains them, stay alert.




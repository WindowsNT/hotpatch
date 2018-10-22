#include "stdafx.h"
#include "..\\..\\hotpatch.h"


namespace FOO
{

	__declspec(noinline) void PatchableFunction1()
	{
		MessageBox(0, L"Before patch", 0, MB_OK);
	}
};


__declspec(noinline) void PatchableFunction2(int x)
{
	wchar_t z[100] = { 0 };
	swprintf_s(z, 100, L"Message: %u", x);
	MessageBox(0, z, L"Before patch", MB_OK);
}

class X
{
public:

	__declspec(noinline) void PatchableFunction3()
	{
		MessageBox(0, L"Before patch", 0, MB_OK);
	}

	__declspec(noinline) virtual void PatchableFunction4()
	{
		MessageBox(0, L"Before patch", 0, MB_OK);
	}
};

class X2 : public X
{
public:

	__declspec(noinline) virtual void PatchableFunction4()
	{
		MessageBox(0, L"Before patch", 0, MB_OK);
	}

};

HOTPATCH hp;

HRESULT PostBuildPatchExecutable()
{
	wchar_t my[1000] = { 0 };
	GetModuleFileName(0, my, 1000);


	HRESULT hr = hp.PrepareExecutable(my,{ L"main.obj" });
	if (FAILED(hr))
	{
		MessageBox(0, L"Executable post-patching failed.", 0, 0);
		return E_FAIL;
	}
	FILE* fw = 0;
	_wfopen_s(&fw,L"patch.xml",L"wb");
	if (!fw)
	{
		MessageBox(0, L"Executable post-patching failed.", 0, 0);
		return E_FAIL;
	}
	auto s = hp.getser();
	fwrite(s.data(), 1, s.size(), fw);
	fclose(fw);
	return S_OK;
}


int main(int argc,char** argv)
{
	CoInitialize(0);
	if (argc == 2 && strcmp(argv[1], "/postbuildpatch") == 0)
	{
		PostBuildPatchExecutable();
		return 0; 
	}
	if (true)
	{
		FOO::PatchableFunction1();
		PatchableFunction2(5);
		PatchableFunction2(6);
		X x;
		x.PatchableFunction3();
		x.PatchableFunction4();
		X2 x2;
		x2.PatchableFunction4();
	}

	HINSTANCE hL = LoadLibrary(L"..\\dll\\dll.dll");
	if (!hL)
		return 0;
	HRESULT(__stdcall *patch)() = (HRESULT(__stdcall *)())GetProcAddress(hL, "Patch");
	if (patch)
		patch();

	if (true)
	{
		FOO::PatchableFunction1();
		PatchableFunction2(5);
		PatchableFunction2(6);
		X x;
		x.PatchableFunction3();
		x.PatchableFunction4();
		X2 x2;
		x2.PatchableFunction4();
	}

}
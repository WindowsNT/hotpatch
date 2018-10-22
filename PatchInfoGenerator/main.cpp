#include "stdafx.h"
#include "..\\hotpatch.h"

#include <iostream>
using namespace std;
int wmain(int argc,wchar_t** argv)
{
	CoInitialize(0);
	if (argc <= 1)
	{
		cout << "Usage: pig <exe> [compilands]";
		exit(1);
	}

	HOTPATCH hp;


	vector<wstring> comps;
	for (int i = 2; i < argc; i++)
		comps.push_back(argv[i]);

	wchar_t my[1000] = { 0 };
	swprintf_s(my, 1000, L"%s.afterpatch", argv[1]);
	if (!CopyFile(argv[1], my, FALSE))
	{
		cout << "Preparation failed";
		exit(2);
	}

	HRESULT hr = hp.PrepareExecutable(my, comps);
	if (FAILED(hr))
	{
		cout << "Preparation failed";
		exit(3);
	}
	hr = hp.PatchExecutable();
	if (FAILED(hr))
	{
		cout << "Preparation failed";
		exit(4);
	}
	if (!CopyFile(my,argv[1], FALSE))
	{
		cout << "Preparation failed";
		exit(5);
	}
	cout << "Preparation succeeded";
	DeleteFile(my);
	exit(0);
}
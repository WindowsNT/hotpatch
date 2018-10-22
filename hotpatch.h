#include <comdef.h>
#include <atlbase.h>
#include <atlsafe.h>
#include <vector>
#include <memory>
#include <DbgHelp.h>
#include "C:\Program Files (x86)\Microsoft Visual Studio\2017\Community\DIA SDK\include\dia2.h"

#include <psapi.h>
#include <sstream>
#include <thread>
#include <map>
#include <functional>



#ifdef _WIN64
#pragma comment(lib,"C:\\Program Files (x86)\\Microsoft Visual Studio\\2017\\Community\\DIA SDK\\lib\\amd64\\diaguids.lib")

#else
#pragma comment(lib,"C:\\Program Files (x86)\\Microsoft Visual Studio\\2017\\Community\\DIA SDK\\lib\\diaguids.lib")
#endif
#pragma comment(lib,"psapi.lib")
#pragma comment(lib,"dbghelp.lib")


#define DISPID_GETNAMES 71001
#define DISPID_CALL 71002

//#define HOTPATCH_NO_INTEROP


const HKEY hkr = HKEY_CURRENT_USER;


template <typename T, typename T2, typename C>
std::vector<T> &split(const T &s, C delim, std::vector<T> &elems)
	{
	T2 ss(s);
	T item;
	while (std::getline(ss, item, delim))
		{
		elems.push_back(item);
		}
	return elems;
	}

template <typename T, typename T2, typename C>
std::vector<T> split(const T &s, C delim)
	{
	std::vector<T> elems;
	split<T, T2, C>(s, delim, elems);
	return elems;
	}


struct IATRESULTS
	{
	enum class FAILUREREASON
		{
		SUCCESS = 0,
		OTHER = 1,
		NOTFOUND = 2,
		CANNOTPATCH = 3,
		};
	struct FUNCTIONINFO
		{
		string name;
		size_t ord = 0;
		FAILUREREASON f = FAILUREREASON::SUCCESS;
		};
	struct MODULEINFO
		{
		string name;
		HINSTANCE handle = 0;
		FAILUREREASON f = FAILUREREASON::SUCCESS;
		vector<FUNCTIONINFO> functions;
		};

	vector<MODULEINFO> modules;
	};


#ifndef HOTPATCH_NO_INTEROP


// USM
#ifndef _USM_H
#define _USM_H


#ifndef _NO_MUTUAL

class mutual
{
private:

	wstring ev1;
	wstring str;
	HANDLE h1 = 0;
	HANDLE h2 = 0;
	std::function<void(unsigned long long lp)> lpf;
	unsigned long long lpa = 0;

	mutual(const mutual&) = delete;
	void operator =(const mutual&) = delete;

	HANDLE CreateEventX(int i)
	{
		TCHAR n[1000] = { 0 };
		swprintf_s(n, L"%s{401F3B1E-6090-4DF2-95C0-F11C10F9F285}_%u", str.c_str(), i);
		HANDLE hX = CreateEvent(0, 0, 0, n);
		return hX;
	}

	void WaitForRequestNoLoop()
	{
		WaitForSingleObject(h1, INFINITE);
		if (lpf)
			lpf(lpa);
		SetEvent(h2);
	}

	void WaitForRequestLoop()
	{
		for (;;)
		{
			if (WaitForSingleObject(h1, INFINITE) != WAIT_OBJECT_0)
				break;
			if (lpf)
				lpf(lpa);
			SetEvent(h2);
		}
	}



public:

	mutual(const wchar_t* strn, std::function<void(unsigned long long lp)> req, unsigned long long lp, bool LoopRequest)
	{
		str = strn;
		lpf = req;
		lpa = lp;
		h1 = CreateEventX(1);
		h2 = CreateEventX(2);
		if (req)
		{
			if (LoopRequest)
			{
				std::thread t(&mutual::WaitForRequestLoop, this);
				t.detach();
			}
			else
			{
				std::thread t(&mutual::WaitForRequestNoLoop, this);
				t.detach();
			}
		}
	}

	void request(DWORD Wait = INFINITE)
	{
		ResetEvent(h2);
		SetEvent(h1);
		if (!Wait)
			return;
		WaitForSingleObject(h2, Wait);
	}

	~mutual()
	{
		CloseHandle(h2);
		h2 = 0;
		CloseHandle(h1);
		h1 = 0;
	}
};
#endif 

template <typename T = char>
class usm
{
private:
	struct USMHEADER
	{
	};

	struct USMTHREAD
	{
		DWORD id;
		int evidx;
	};

	// Strings of handle ids
	wstring cwmn;
	wstring fmn;
	wstring evrn;
	wstring evrn2;
	wstring evwn;

	wstring stringid;

	bool WasFirst = false;

	// Auto reset event that is set when reading thread finishes reading
	HANDLE hEventRead = 0;

	// Auto reset event that is set when writing thread finishes writing
	HANDLE hEventWrote = 0;

	// Locked when this thread is writing
	// Or when a thread prepares for initializing or exiting
	HANDLE hMutexWriting = 0;

	// Set when this thread is not reading
	// Unset when this thread is reading
	HANDLE hEventMeReading = 0;


	HANDLE hFM = 0;
	unsigned long long ClientSZ = 0;
	DWORD MaxThreads = 0;
	PVOID  Buff = 0;
	bool Executable = false;

	SECURITY_ATTRIBUTES sattr;
	SECURITY_DESCRIPTOR SD;



	void FillSA()
	{
		sattr.nLength = sizeof(sattr);
		BOOL fx = InitializeSecurityDescriptor(&SD, SECURITY_DESCRIPTOR_REVISION);
		fx = SetSecurityDescriptorDacl(&SD, TRUE, NULL, FALSE);
		sattr.bInheritHandle = true;
		sattr.lpSecurityDescriptor = &SD;
	}


	HANDLE CreateEvR(int idx)
	{
		TCHAR n[1000] = { 0 };
		swprintf_s(n, L"%s%i", evrn.c_str(), idx);
		HANDLE hX = CreateEvent(&sattr, TRUE, TRUE, n);
		return hX;
	}

	HANDLE CreateEvR2(int idx)
	{
		TCHAR n[1000] = { 0 };
		swprintf_s(n, L"%s%i", evrn2.c_str(), idx);
		HANDLE hX = CreateEvent(&sattr, 0, 0, n);
		return hX;
	}

	HANDLE CreateEvW()
	{
		TCHAR n[1000] = { 0 };
		swprintf_s(n, L"%s", evwn.c_str());
		HANDLE hX = CreateEvent(&sattr, 0, 0, n);
		return hX;
	}

	HANDLE CreateCWM()
	{
		HANDLE hX = OpenMutex(MUTEX_MODIFY_STATE | SYNCHRONIZE, false, cwmn.c_str());
		if (hX != 0)
			return hX;
		hX = CreateMutex(&sattr, 0, cwmn.c_str());
		return hX;
	}

	HANDLE CreateFM()
	{
		// Try to open the map , or else create it
		WasFirst = true;
		HANDLE hX = OpenFileMapping(FILE_MAP_READ | FILE_MAP_WRITE | (Executable ? FILE_MAP_EXECUTE : 0), false, fmn.c_str());
		if (hX != 0)
		{
			WasFirst = false;
			return hX;
		}

		unsigned long long  FinalSize = ClientSZ * sizeof(T) + MaxThreads * sizeof(USMTHREAD) + sizeof(USMHEADER);
		ULARGE_INTEGER ulx = { 0 };
		ulx.QuadPart = FinalSize;

		hX = CreateFileMapping(INVALID_HANDLE_VALUE, &sattr, (Executable ? PAGE_EXECUTE_READWRITE : PAGE_READWRITE), ulx.HighPart, ulx.LowPart, fmn.c_str());
		if (hX != 0)
		{
			LPVOID Buff4 = MapViewOfFile(hX, FILE_MAP_READ | FILE_MAP_WRITE | (Executable ? FILE_MAP_EXECUTE : 0), 0, 0, 0);
			if (Buff4)
			{
				memset(Buff4, 0, (size_t)FinalSize);
				UnmapViewOfFile(Buff4);
			}
		}
		return hX;
	}


public:


	HANDLE fmh()
	{
		return hX;
	}

	HANDLE GetFM() { return hFM; }
	wstring GetFMN() { return fmn; }
	int GetMaxThreads() { return MaxThreads; }

	void End()
	{
		// Remove the ID from the thread
		if (Buff)
		{
			USMTHREAD* th = (USMTHREAD*)((char*)((char*)Buff + sizeof(USMHEADER)));
			WaitForSingleObject(hMutexWriting, INFINITE);
			// Find 
			for (unsigned int y = 0; y < MaxThreads; y++)
			{
				USMTHREAD& tt = th[y];
				DWORD myid = GetCurrentThreadId();
				if (tt.id == myid)
				{
					tt.id = 0;
					tt.evidx = 0;
					break;
				}
			}
			ReleaseMutex(hMutexWriting);
		}


		if (hEventRead)
			CloseHandle(hEventRead);
		hEventRead = 0;
		if (hEventWrote)
			CloseHandle(hEventWrote);
		hEventWrote = 0;
		if (hFM)
			CloseHandle(hFM);
		hFM = 0;
		if (hEventMeReading)
			CloseHandle(hEventMeReading);
		hEventMeReading = 0;
		if (hMutexWriting)
			CloseHandle(hMutexWriting);
		hMutexWriting = 0;
	}


	bool IsFirst() { return WasFirst; }
	usm(const wchar_t* string_id = 0, bool Init = false, unsigned long long csz = 1048576, DWORD MaxTh = 100)
	{
		if (!string_id)
			return;
		CreateInit(string_id, Init, csz, MaxTh);
	}

	void operator =(const usm &x)
	{
		// Terminate current
		End();

		// Recreate
		CreateInit(x.stringid.c_str(), true, x.ClientSZ, x.MaxThreads);
	}

	usm(const usm& x)
	{
		operator=(x);
	}


	void CreateInit(const wchar_t* string_id, bool Init = false, unsigned long long csz = 1048576, DWORD MaxTh = 100, bool LocalOnly = false, bool Exex = false)
	{
		Executable = Exex;
		if (!string_id)
			return;
		if (wcslen(string_id) == 0)
			return;

		TCHAR xup[1000] = { 0 };
		stringid = string_id;

		FillSA();
		if (LocalOnly)
		{
			swprintf_s(xup, 1000, L"%s_cwmn", stringid.c_str());
			cwmn = xup;
			swprintf_s(xup, 1000, L"%s_evrn", stringid.c_str());
			evrn = xup;
			swprintf_s(xup, 1000, L"%s_evrn2", stringid.c_str());
			evrn2 = xup;
			swprintf_s(xup, 1000, L"%s_evwn", stringid.c_str());
			evwn = xup;
			swprintf_s(xup, 1000, L"%s_fmn", stringid.c_str());
			fmn = xup;
		}
		else
		{
			swprintf_s(xup, 1000, L"Global\\%s_cwmn", stringid.c_str());
			cwmn = xup;
			swprintf_s(xup, 1000, L"Global\\%s_evrn", stringid.c_str());
			evrn = xup;
			swprintf_s(xup, 1000, L"Global\\%s_evrn2", stringid.c_str());
			evrn2 = xup;
			swprintf_s(xup, 1000, L"Global\\%s_evwn", stringid.c_str());
			evwn = xup;
			swprintf_s(xup, 1000, L"Global\\%s_fmn", stringid.c_str());
			fmn = xup;
		}

		if (!csz)
			csz = 1048576;
		ClientSZ = csz;
		if (!MaxTh)
			MaxTh = 100;
		MaxThreads = MaxTh;
		if (Init)
		{
			int iv = Initialize();
			if (iv <= 0)
			{
				End();
				throw iv;
			}
		}
	}

	~usm()
	{
		End();
	}

	int Initialize()
	{
		hEventRead = 0;
		hEventWrote = 0;
		hMutexWriting = 0;
		hFM = 0;
		Buff = 0;
		hEventMeReading = 0;

		if (hMutexWriting == 0)
			hMutexWriting = CreateCWM();
		if (hMutexWriting == 0)
			return -1;
		if (hFM == 0)
			hFM = CreateFM();
		if (hFM == 0)
			return -1;
		if (hEventWrote == 0)
			hEventWrote = CreateEvW();
		if (hEventWrote == 0)
			return -1;
		if (Buff == 0)
			Buff = MapViewOfFile(hFM, FILE_MAP_READ | FILE_MAP_WRITE | (Executable ? FILE_MAP_EXECUTE : 0), 0, 0, 0);
		if (!Buff)
			return -1;

		// Acquire lock for Count variable
		// USMHEADER* h = (USMHEADER*)Buff;
		USMTHREAD* th = (USMTHREAD*)((char*)((char*)Buff + sizeof(USMHEADER)));
		WaitForSingleObject(hMutexWriting, INFINITE);
		// Find 
		for (unsigned int y = 0; y < MaxThreads; y++)
		{
			USMTHREAD& tt = th[y];
			if (tt.id == 0)
			{
				tt.id = GetCurrentThreadId();
				tt.evidx = (y + 1);
				hEventMeReading = CreateEvR(y + 1);
				hEventRead = CreateEvR2(y + 1);
				break;
			}
		}
		ReleaseMutex(hMutexWriting);

		if (!hEventMeReading)
			return -1;

		return 1;
	}

	const T* BeginRead(bool FailOnNotReady = false)
	{
		if (!Buff)
			return 0;

		// Is someone writing 
		if (FailOnNotReady)
		{
			DWORD x = WaitForSingleObject(hMutexWriting, 0);
			if (x != WAIT_OBJECT_0)
				return 0;
		}
		else
			WaitForSingleObject(hMutexWriting, INFINITE);

		// Reset our reading event
		ResetEvent(hEventMeReading);

		// Release the mutex, but now any writing thread that locks it must wait for is
		ReleaseMutex(hMutexWriting);

		// Return the pointer
		const char* a1 = (const char*)Buff;
		a1 += sizeof(USMHEADER);
		a1 += sizeof(USMTHREAD)*MaxThreads;

		return (T*)a1;
	}

	void EndRead()
	{
		SetEvent(hEventMeReading);
		SetEvent(hEventRead);
	}

	unsigned long long ReadData(T* b, size_t sz, size_t offset = 0, bool FailIfNotReady = false)
	{
		const T* ptr = BeginRead(FailIfNotReady);
		if (!ptr)
			return (unsigned long long) - 1;
		memcpy(b, ptr + offset, sz);
		EndRead();
		return sz;
	}


	DWORD NotifyOnRead(bool Wait)
	{
		// See if any thread is reading
		USMTHREAD* th = (USMTHREAD*)((char*)((char*)Buff + sizeof(USMHEADER)));
		vector<HANDLE> evs;

		// Find 
		bool S = true;
		for (unsigned int y = 0; y < MaxThreads; y++)
		{
			USMTHREAD& tt = th[y];
			if (tt.evidx > 0)
			{
				// Open the event
				TCHAR n[1000] = { 0 };
				swprintf_s(n, L"%s%i", evrn.c_str(), tt.evidx);
				HANDLE hEv = OpenEvent(SYNCHRONIZE, 0, n);
				if (hEv == 0) // duh
				{
					S = false;
					break;
				}
				evs.push_back(hEv);
			}
		}
		DWORD fi = 0;
		if (!S)
			return (DWORD)-1;
		if (evs.empty())
			return (DWORD)-2;

		// Wait for any thread to terminate reading
		fi = WaitForMultipleObjects((DWORD)evs.size(), &evs[0], FALSE, Wait ? INFINITE : 0);

		// Cleanup
		for (unsigned int i = 0; i < evs.size(); i++)
			CloseHandle(evs[i]);
		evs.clear();

		return fi;
	}

	T* BeginWrite(bool FailOnNotReady = false)
	{
		// Lock the writing mutex
		if (FailOnNotReady)
		{
			DWORD x = WaitForSingleObject(hMutexWriting, 0);
			if (x != WAIT_OBJECT_0)
				return 0;
		}
		else
			WaitForSingleObject(hMutexWriting, INFINITE);

		// Having locked the writing mutex, no reading thread can start now
		// After that, no new threads can read
		vector<HANDLE> evs;
		evs.reserve(MaxThreads);

		// Wait for threads that are already in read state
		USMTHREAD* th = (USMTHREAD*)((char*)((char*)Buff + sizeof(USMHEADER)));

		// Find 
		bool S = true;
		for (unsigned int y = 0; y < MaxThreads; y++)
		{
			USMTHREAD& tt = th[y];
			if (tt.evidx > 0)
			{
				// Open the event
				TCHAR n[1000] = { 0 };
				swprintf_s(n, L"%s%i", evrn.c_str(), tt.evidx);
				HANDLE hEv = OpenEvent(SYNCHRONIZE, 0, n);
				if (hEv == 0) // duh
				{
					S = false;
					break;
				}
				evs.push_back(hEv);
			}
		}
		DWORD fi = 0;
		if (S)
		{
			// Wait for all these threads to terminate reading
			fi = WaitForMultipleObjects((DWORD)evs.size(), &evs[0], TRUE, FailOnNotReady ? 0 : INFINITE);
			if (fi == -1 || fi == WAIT_TIMEOUT)
				S = false;
		}
		else
		{
			fi = (DWORD)-1;
		}

		// Cleanup
		for (unsigned int i = 0; i < evs.size(); i++)
			CloseHandle(evs[i]);
		evs.clear();
		if (!S)
		{
			ReleaseMutex(hMutexWriting);
			return 0;
		}

		// Return the pointer
		char* a1 = (char*)Buff;
		a1 += sizeof(USMHEADER);
		a1 += sizeof(USMTHREAD)*MaxThreads;

		ResetEvent(hEventWrote);
		return (T*)a1;
	}

	void EndWrite()
	{
		ReleaseMutex(hMutexWriting);
		SetEvent(hEventWrote);
	}

	DWORD NotifyWrite(bool Wait)
	{
		// Wait for all these threads to terminate reading
		return WaitForSingleObject(hEventWrote, Wait ? INFINITE : 0);
	}

	unsigned long long WriteData(const T* b, size_t sz, size_t offset = 0, bool FailIfNotReady = false)
	{
		T* ptr = BeginWrite(FailIfNotReady);
		if (!ptr)
			return (unsigned long long) - 1;
		memcpy(ptr + offset, b, sz);
		EndWrite();
		return sz;
	}

	// Sends data, then waits until all threads have read that data
	unsigned long long SendDataAndWait(const T*b, size_t sz, size_t offset = 0)
	{
		unsigned long long r = WriteData(b, sz, offset);
		if (r != sz)
			return r;

		USMTHREAD* th = (USMTHREAD*)((char*)((char*)Buff + sizeof(USMHEADER)));
		vector<HANDLE> evs;

		// Find 
		bool S = true;
		for (unsigned int y = 0; y < MaxThreads; y++)
		{
			USMTHREAD& tt = th[y];
			if (tt.id == GetCurrentThreadId())
				continue;
			if (tt.evidx > 0)
			{
				// Open the event
				TCHAR n[1000] = { 0 };
				swprintf_s(n, L"%s%i", evrn2.c_str(), tt.evidx);
				HANDLE hEv = OpenEvent(SYNCHRONIZE, 0, n);
				if (hEv == 0) // duh
				{
					S = false;
					break;
				}
				evs.push_back(hEv);
			}
		}
		if (!S)
			return (DWORD)-1;
		if (evs.empty())
			return (DWORD)-2;

		// Wait for all thread to terminate reading
		WaitForMultipleObjects((DWORD)evs.size(), &evs[0], TRUE, INFINITE);
		return r;
	}

};


#endif // USM_H


#endif // HOTPATCH_NO_INTEROP
#include "xml\\xml3all.h"
using namespace XML3;

struct HPFUNCTION
	{
	wstring n;
	DWORD loctype = 0;
	unsigned long long a = 0;
	unsigned long long va = 0;

	void ToEl(XML3::XMLElement& e)
		{
		e.FindVariableZ("n",true)->SetValue(XML3::XMLU(n.c_str()));
		e.FindVariableZ("lt",true)->SetValueUInt(loctype);
		e.FindVariableZ("a",true)->SetValueULongLong(a);
		e.FindVariableZ("va",true)->SetValueULongLong(va);
		}

	void FromEl(XML3::XMLElement& e)
		{
		n = XML3::XMLU(e.FindVariableZ("n",true)->GetValue().c_str());
		loctype = e.FindVariableZ("lt",true)->GetValueUInt();
		a = e.FindVariableZ("a",true)->GetValueULongLong();
		va = e.FindVariableZ("va",true)->GetValueULongLong();
		}
	};


inline void gethpgid(void* hp, TCHAR*, int ms);
#define XCXCALL


#ifndef XCXCALL
size_t COMCALPTR = 0;
#endif

#pragma pack(push,1)

struct COMCALL
	{
	unsigned char jmp1 = 0xE9;
	unsigned char jmp2 = 0x27;
	unsigned char jmp3 = 0x01;
	unsigned char jmp4 = 0x00;
	unsigned char jmp5 = 0x00; // JMP $ + 300

	IDispatch* HPPointer = 0;
	void* HPClass = 0;
#ifdef _WIN64
	char data[179];
#else
	char data[187];
#endif
	char name[100];

	// Push pop to stack
#ifdef _WIN64
	struct MOVER
		{
		unsigned char pushreg = 0;
		unsigned char poprax = 0x58;
		unsigned short movrax = 0xA348;
		unsigned long long addr = 0;
		};
#else
	struct MOVER
		{
		unsigned char pushreg = 0x66;
		unsigned char popeax = 0x58;
		unsigned char moveax = 0xA3;
		unsigned long addr = 0;
		};
#endif

	MOVER m1[8]; // base registers

#ifdef _WIN64
	struct MOVER2
		{
		unsigned char pushregp = 0x41;
		unsigned char pushreg = 0;
		unsigned char poprax = 0x58;
		unsigned short movrax = 0xA348;
		unsigned long long addr = 0;
		};

	MOVER2 m2[8]; // r8-r15 registers
#endif

				  // Call the COMPatchGeneric
#ifdef _WIN64

#ifndef XCXCALL
	unsigned short movd1 = 0xB848;
	unsigned long long regaddr = 0;
	unsigned short movd2x = 0xA348;
	unsigned long long movd2 = 0;
#else
	unsigned short movrcx = 0xB948;
	unsigned long long regaddr = 0;
	unsigned short subrsp100_1 = 0x8148;
	unsigned short subrsp100_2 = 0x00EC;
	unsigned short subrsp100_3 = 0x0001;
	unsigned char subrsp100_4 = 0;
#endif // XCXCALL
	unsigned short movrax = 0xB848;
	unsigned long long calladdr = 0;
	unsigned short callrax = 0xD0FF;
#ifdef XCXCALL
	// Restore the stack
	unsigned short addrsp100_1 = 0x8148;
	unsigned short addrsp100_2 = 0x00C4;
	unsigned short addrsp100_3 = 0x0001;
	unsigned char addrsp100_4 = 0;
#endif
	unsigned char ret = 0xC3;
#else
				  // Call the COMPatchGeneric
#ifndef XCXCALL
	unsigned short movd1 = 0x05C7;
	unsigned long movd2 = 0;
	unsigned long regaddr = 0;
#else
	unsigned char movecx = 0xB9;
	unsigned long regaddr = 0;
	// Give the callee some stack to work with
	unsigned short subesp100_1 = 0xEC81;
	unsigned short subesp100_2 = 0x0100;
	unsigned short subesp100_3 = 0x0000;
#endif

	unsigned char moveax = 0xB8;
	unsigned long calladdr = 0;
	unsigned short calleax = 0xD0FF;
#ifdef XCXCALL
	// Restore the stack
	unsigned short addesp100_1 = 0xC481;
	unsigned short addesp100_2 = 0x0100;
	unsigned short addesp100_3 = 0x0000;
#endif
	unsigned char ret = 0xC3;
#endif // WIN64

	COMCALL(IDispatch*dispp, class HOTPATCH* hpx, size_t targetcall, const wchar_t* fname)
		{
		HPPointer = dispp;
		HPClass = hpx;
		calladdr = targetcall;
		regaddr = (size_t)this;
#ifndef XCXCALL
		movd2 = (size_t)&COMCALPTR;
#endif
		for (int i = 0; i < 8; i++)
			{
			m1[i].pushreg = (unsigned char)(0x50 + i);
			m1[i].addr = (size_t)(data + i * sizeof(size_t));
			}
#ifdef _WIN64
		for (int i = 0; i < 8; i++)
			{
			m2[i].pushreg = (unsigned char)(0x50 + i);
			m2[i].addr = (unsigned long long)(data + (i + 8) * 8);
			}
#endif
		size_t le = wcslen(fname);
		if (le > 50)
			le = 50;
		memcpy(name, fname, le * 2);
		}
	};


#pragma pack(pop)


#pragma optimize("",off)
#ifndef XCXCALL
void __fastcall COMPatchGeneric()
	{
	COMCALL* cc = (COMCALL*)COMCALPTR;
#else
void __fastcall COMPatchGeneric(size_t dx)
	{
	COMCALL* cc = (COMCALL*)dx;
#endif
	IDispatch* d = cc->HPPointer;
	if (!d)
		return;
	CComSafeArray<size_t> a(16);
	for (int i = 0; i < 16; i++)
		{
		size_t aa = 0;
		memcpy(&aa, cc->data + (i * sizeof(size_t)), sizeof(aa));
		a.SetAt(i, aa);
		}


//	d->Call(_bstr_t((wchar_t*)cc->name), a);

	DISPPARAMS dp = { 0 };
	dp.cArgs = 2;
	VARIANT v[2] = { 0 };
	BSTR b1 = SysAllocString((wchar_t*)cc->name);
	v[0].vt = VT_ARRAY;
	v[0].parray = a;
	v[1].bstrVal = b1;
	v[1].vt = VT_BSTR;
	dp.rgvarg = v;

	d->Invoke(DISPID_CALL, IID_NULL, 0, 0, &dp, 0, 0, 0);
	SysFreeString(b1);
	}
#pragma optimize("",on)

#pragma optimize("",off)
#ifndef XCXCALL
void __fastcall USMPatchGeneric()
	{
	COMCALL* cc = (COMCALL*)COMCALPTR;
#else
void __fastcall USMPatchGeneric(size_t dx)
	{
	COMCALL* cc = (COMCALL*)dx;
#endif
	size_t midx = (size_t)cc->HPPointer;

	vector<TCHAR> cidx(1000);

	gethpgid(cc->HPClass, cidx.data(), 1000);

	swprintf_s(cidx.data() + wcslen(cidx.data()), 500, L"-%u", (unsigned int)midx);
	mutual mu(cidx.data(), nullptr, 0,true);
	mu.request();
	}
#pragma optimize("",on)

class SUSPENDEDPROCESS
	{
	public:

	HANDLE hP = 0;

	SUSPENDEDPROCESS(HANDLE hX) { hP = hX; }
	void Term()
		{
		if (hP)
			{
			TerminateProcess(hP,0);
			CloseHandle(hP);
			hP = 0;
			}
		}
	~SUSPENDEDPROCESS()
		{
		Term();
		}
	};


class HOTPATCH
	{
	public:

		struct NAMEANDPOINTER
			{
			wstring n;
			std::function<size_t(size_t*)> f;
			void* mu = 0;
			};

		string getser() { return ser; }

	private:

		vector<void*> VirtualAllocPointers;
		friend class MyHotPatch;
		friend class LocalClassFactory;
		string ser;
		wstring exe;

#ifndef HOTPATCH_NO_INTEROP
		// USM stuff
		shared_ptr<usm<char>> u_sm;


		// COM stuff
		CComPtr<IDispatch> RemoteInterface;
		CComPtr<ITypeLib> pTLib = NULL;
		CLSID ccid;
		wstring progid;
		DWORD regi = 0;
		std::function<HRESULT(vector<NAMEANDPOINTER>&)> PatchNamesFunction;
		std::function<void()> ReleaseInterfacesCallback;
		vector<HOTPATCH::NAMEANDPOINTER> nap; // For USM caching



		// Privates for COM
		HRESULT HOTPATCH::IsRegistered(HKEY root)
			{
			TCHAR cidx[1000] = { 0 };
			wcscpy_s(cidx, 1000, L"Software\\Classes\\CLSID\\");
			StringFromGUID2(ccid, cidx + wcslen(cidx), (int)(1000 - wcslen(cidx)));

			HKEY k = 0;
			RegOpenKeyEx(root, cidx, 0, KEY_READ, &k);
			if (!k)
				return E_FAIL;
			RegCloseKey(k);
			return S_OK;
			}

		HRESULT HOTPATCH::Unregister(HKEY root)
			{
			TCHAR cidx[1000] = { 0 };
			TCHAR T[1000] = { 0 };
			wcscpy_s(T, 1000, exe.c_str());

			wcscpy_s(cidx, 1000, L"Software\\Classes\\CLSID\\");
			StringFromGUID2(ccid, cidx + wcslen(cidx), (int)(1000 - wcslen(cidx)));
			wcscat_s(cidx, 1000, L"\\ProgID");
			RegDeleteKey(root, cidx);

			wcscpy_s(cidx, 1000, L"Software\\Classes\\CLSID\\");
			StringFromGUID2(ccid, cidx + wcslen(cidx), (int)(1000 - wcslen(cidx)));
			wcscat_s(cidx, 1000, L"\\LocalServer32");
			RegDeleteKey(root, cidx);

			wcscpy_s(cidx, 1000, L"Software\\Classes\\CLSID\\");
			StringFromGUID2(ccid, cidx + wcslen(cidx), (int)(1000 - wcslen(cidx)));
			wcscat_s(cidx, 1000, L"\\Version");
			RegDeleteKey(root, cidx);

			wcscpy_s(cidx, 1000, L"Software\\Classes\\CLSID\\");
			StringFromGUID2(ccid, cidx + wcslen(cidx), (int)(1000 - wcslen(cidx)));
			RegDeleteKey(root, cidx);


			// ---
			wcscpy_s(cidx, 1000, L"Software\\Classes\\");
			wcscat_s(cidx, 1000, progid.c_str());
			wcscat_s(cidx, 1000, L"\\CLSID");
			RegDeleteKey(root, cidx);

			wcscpy_s(cidx, 1000, L"Software\\Classes\\");
			wcscat_s(cidx, 1000, progid.c_str());
			RegDeleteKey(root, cidx);

			wcscpy_s(cidx, 1000, L"Software\\Classes\\AppID\\");
			TCHAR* fionly = wcsrchr(T, '\\');
			fionly++;
			wcscat_s(cidx, 1000, fionly);
			RegDeleteKey(root, cidx);

			// ---
			wcscpy_s(cidx, 1000, L"Software\\Classes\\AppID\\");
			StringFromGUID2(ccid, cidx + wcslen(cidx), (int)(1000 - wcslen(cidx)));
			RegDeleteKey(root, cidx);

			return S_OK;
			}



		HRESULT HOTPATCH::LoadRemoteInterface()
			{
			if (RemoteInterface)
				return S_FALSE;
			CComPtr<IClassFactory> f;
			CoCreateInstance(ccid, 0, CLSCTX_LOCAL_SERVER, __uuidof(IClassFactory), (void**)&f);
			if (!f)
				return E_FAIL;
			HRESULT hr = f->CreateInstance(0, __uuidof(IDispatch), (void**)&RemoteInterface);
			f = 0;
			return hr;
			}

#endif
		unsigned long long BaseAddress(HANDLE hP)
			{
			vector<wchar_t> pn(1024);
			//	GetProcessImageFileName(hP,pn.data(),1024);
			DWORD pnd = 1024;
			QueryFullProcessImageName(hP, 0, pn.data(), &pnd);

			DWORD dw = 0;

			vector<HMODULE> v(1024);
			if (!EnumProcessModules(hP, v.data(), (DWORD)(v.size() * sizeof(HMODULE)), &dw))
				return 0;
			int numh = dw / sizeof(HMODULE);
			for (int i = 0; i < numh; i++)
				{
				vector<wchar_t> n(1024);
				GetModuleFileNameEx(hP, v[i], n.data(), 1024);
				if (_wcsicmp(n.data(), pn.data()) == 0)
					{
					// It's that one
					MODULEINFO mi = { 0 };
					GetModuleInformation(hP, v[i], &mi, sizeof(mi));
					return (unsigned long long)mi.lpBaseOfDll;
					}
				}
			return 0;
			}


	public:

#ifndef HOTPATCH_NO_INTEROP
		enum DISPIDS
			{
			REQUEST_NONE = 0,
			REQUEST_PATCH_NAMES = 1,
			REQUEST_EXECUTE_FUNCTION = 2
			};
#endif

		HOTPATCH()
			{

			}



#ifndef HOTPATCH_NO_INTEROP
		CLSID GetCID() { return ccid; }
		shared_ptr<usm<char>> GetUSM() { return u_sm; }
		std::function<void()> GetReleaseInterface() { return ReleaseInterfacesCallback; }
#endif

		void PatchIAT(HINSTANCE h, IATRESULTS* rres = 0)
			{
			IATRESULTS rx;
			IATRESULTS& res = (rres) ? *rres : rx;
			// Get IAT size
			DWORD ulsize = 0;
			PIMAGE_IMPORT_DESCRIPTOR pImportDesc = (PIMAGE_IMPORT_DESCRIPTOR)ImageDirectoryEntryToData(h, TRUE, IMAGE_DIRECTORY_ENTRY_IMPORT, &ulsize);
			if (!pImportDesc)
				return;

			// Loop names
			for (; pImportDesc->Name; pImportDesc++)
				{
				PSTR pszModName = (PSTR)((PBYTE)h + pImportDesc->Name);
				if (!pszModName)
					break;

				IATRESULTS::MODULEINFO m;

				m.name = pszModName;

				HINSTANCE hImportDLL = LoadLibraryA(pszModName);
				if (!hImportDLL)
					{
					m.f = IATRESULTS::FAILUREREASON::NOTFOUND;
					res.modules.push_back(m);
					continue;
					}
				m.handle = hImportDLL;
				m.f = IATRESULTS::FAILUREREASON::SUCCESS;

				// Get caller's import address table (IAT) for the callee's functions
				PIMAGE_THUNK_DATA pThunk = (PIMAGE_THUNK_DATA)
					((PBYTE)h + pImportDesc->FirstThunk);

				// Replace current function address with new function address
				for (; pThunk->u1.Function; pThunk++)
					{
					IATRESULTS::FUNCTIONINFO fu;

					FARPROC pfnNew = 0;
					size_t rva = 0;
#ifdef _WIN64
					if (pThunk->u1.Ordinal & IMAGE_ORDINAL_FLAG64)
#else
					if (pThunk->u1.Ordinal & IMAGE_ORDINAL_FLAG32)
#endif
						{
						// Ordinal
#ifdef _WIN64
						size_t ord = IMAGE_ORDINAL64(pThunk->u1.Ordinal);
#else
						size_t ord = IMAGE_ORDINAL32(pThunk->u1.Ordinal);
#endif

						fu.ord = ord;
						m.functions.push_back(fu);
						PROC* ppfn = (PROC*)&pThunk->u1.Function;
						if (!ppfn)
							{
							fu.f = IATRESULTS::FAILUREREASON::NOTFOUND;
							m.functions.push_back(fu);
							continue;
							}
						rva = (size_t)pThunk;

						char fe[100] = { 0 };
						sprintf_s(fe, 100, "#%u", (unsigned int)ord);
						pfnNew = GetProcAddress(hImportDLL, (LPCSTR)ord);
						if (!pfnNew)
							{
							fu.f = IATRESULTS::FAILUREREASON::NOTFOUND;
							m.functions.push_back(fu);
							continue;
							}
						}
					else
						{
						// Get the address of the function address
						PROC* ppfn = (PROC*)&pThunk->u1.Function;
						if (!ppfn)
							{
							fu.f = IATRESULTS::FAILUREREASON::NOTFOUND;
							m.functions.push_back(fu);
							continue;
							}
						rva = (size_t)pThunk;
						PSTR fName = (PSTR)h;
						fName += pThunk->u1.Function;
						fName += 2;
						if (!fName)
							break;
						fu.name = fName;
						pfnNew = GetProcAddress(hImportDLL, fName);
						if (!pfnNew)
							{
							fu.f = IATRESULTS::FAILUREREASON::NOTFOUND;
							m.functions.push_back(fu);
							continue;
							}
						}

					// Patch it now...
					auto hp = GetCurrentProcess();

					if (!WriteProcessMemory(hp, (LPVOID*)rva, &pfnNew, sizeof(pfnNew), NULL) && (ERROR_NOACCESS == GetLastError()))
						{
						DWORD dwOldProtect;
						if (VirtualProtect((LPVOID)rva, sizeof(pfnNew), PAGE_WRITECOPY, &dwOldProtect))
							{
							if (!WriteProcessMemory(GetCurrentProcess(), (LPVOID*)rva, &pfnNew,
								sizeof(pfnNew), NULL))
								{
								fu.f = IATRESULTS::FAILUREREASON::CANNOTPATCH;
								continue;
								}
							if (!VirtualProtect((LPVOID)rva, sizeof(pfnNew), dwOldProtect,
								&dwOldProtect))
								{
								fu.f = IATRESULTS::FAILUREREASON::CANNOTPATCH;
								continue;
								}
							}
						}
					m.functions.push_back(fu);
					}
				res.modules.push_back(m);
				}
			}



			HRESULT PatchExecutable()
				{
				HANDLE hX = BeginUpdateResource(exe.c_str(), false);
				if (!hX)
					return E_NOINTERFACE;
				BOOL fx = UpdateResource(hX, RT_RCDATA, L"HOTPATCHDATA", MAKELANGID(LANG_ENGLISH, SUBLANG_ENGLISH_US), (LPVOID)ser.c_str(), (DWORD)ser.length());
				if (!fx)
					return E_NOTIMPL;

				Sleep(1000);
				fx = EndUpdateResource(hX, false);
				if (!fx)
					return E_FAIL;
				return S_OK; 
				}
			 
			HRESULT AutoPatchExecutable(std::vector<wstring> Compilands = { L"" })
				{
				wchar_t my[1000] = { 0 };
				wchar_t my2[1000] = { 0 };
				wchar_t my3[1000] = { 0 };
				GetModuleFileName(0, my, 1000);
				swprintf_s(my2, 1000, L"%s.prepatch", my);
				swprintf_s(my3, 1000, L"%s.afterpatch", my);
				if (!MoveFileEx(my, my2, MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
					return E_FAIL;
				if (!CopyFile(my2, my3, FALSE))
					return E_FAIL;
				HRESULT hr = PrepareExecutable(my3, Compilands);
				if (FAILED(hr))
					return E_FAIL;
				hr = PatchExecutable();
				if (!CopyFile(my3, my, FALSE))
					return E_FAIL;
				if (FAILED(hr))
					return E_FAIL;

				return S_OK;
				}



		HRESULT PrepareExecutable(wchar_t* e,std::vector<wstring> Compilands)
			{
			if (!e)
				return E_INVALIDARG;

			vector<wchar_t> bu(10000);
			GetFullPathName(e, 10000, bu.data(), 0);
			exe = bu.data();

			HRESULT hr = E_FAIL;
			bool AlsoVirtual = false;

			// Create the process
			SUSPENDEDPROCESS sp(0);
			if (AlsoVirtual)
				{
				STARTUPINFO sInfo = { 0 };
				sInfo.cb = sizeof(sInfo);
				PROCESS_INFORMATION pi = { 0 };
				if (!CreateProcess(0, e, 0, 0, 0, 0, 0, 0, &sInfo, &pi))
					return E_FAIL;
				CloseHandle(pi.hThread);
				sp.hP = pi.hProcess;
				}

			CComPtr<IDiaDataSource> pSource;
			hr = CoCreateInstance(CLSID_DiaSource, NULL, CLSCTX_INPROC_SERVER, __uuidof(IDiaDataSource), (void **)&pSource);
			if (FAILED(hr))
				return hr;

			hr = pSource->loadDataForExe(e, NULL, NULL);

			if (FAILED(hr))
				{
				hr = pSource->loadDataFromPdb(e);
				if (FAILED(hr))
					return E_FAIL;
				}

			CComPtr<IDiaSession> psession;
			if (FAILED(pSource->openSession(&psession)))
				return hr;

			ULONGLONG ba = BaseAddress(sp.hP);
			if (AlsoVirtual)
				psession->put_loadAddress(ba);

			CComPtr<IDiaSymbol> pGlobal;
			hr = psession->get_globalScope(&pGlobal);
			if (FAILED(hr))
				return E_FAIL;

			map<wstring, HPFUNCTION> fs;

			auto parsesymbols = [](CComPtr<IDiaSymbol> pGlobal, map<wstring, HPFUNCTION>& fs) -> HRESULT
				{
				ULONG celt = 0;
				CComPtr<IDiaSymbol> pSymbol;
				CComPtr<IDiaEnumSymbols> pEnumSymbols;
				HRESULT hr = pGlobal->findChildren(SymTagFunction, NULL, nsNone, &pEnumSymbols);
				if (FAILED(hr))
					return E_FAIL;
				for (;;)
					{
					pSymbol = 0;
					if (SUCCEEDED(pEnumSymbols->Next(1, &pSymbol, &celt)) && (celt == 1))
						{
						CComBSTR bstrName;
						if (pSymbol->get_name(&bstrName) == S_OK)
							{
							HPFUNCTION f;
							f.n = bstrName;
							DWORD loctype = 0;
							hr = pSymbol->get_locationType(&loctype);
							DWORD a = 0;
							hr = pSymbol->get_addressOffset(&a);
							f.loctype = loctype;
							f.a = a;
							ULONGLONG va = 0;
							hr = pSymbol->get_virtualAddress(&va);
							f.va = va;

							if (wcsstr(bstrName, L"std::"))
								continue;
							if (wcsstr(bstrName, L"ATL::"))
								continue;
							if (wcsstr(bstrName, L"XML3::"))
								continue;

							fs[f.n] = f;
							}
						}
					else
						break;
					}
				return S_OK;
				};


			if (Compilands.size() == 0)
				{
				// All
				parsesymbols(pGlobal, fs);
				}
			else
				{
				// Enumerate compilands
				CComPtr<IDiaEnumSymbols> pEnumSymbols;
				pGlobal->findChildren(SymTagCompiland, 0, nsNone, &pEnumSymbols);
				if (!pEnumSymbols)
					return E_FAIL;
				CComPtr<IDiaSymbol> pCompiland;
				ULONG celt = 0;
				while (SUCCEEDED(pEnumSymbols->Next(1, &pCompiland, &celt)) && (celt == 1))
					{
					// Include this compiland ?
					CComBSTR bstrName;
					if (pCompiland->get_name(&bstrName) == S_OK)
						{
						const wchar_t*f = wcsrchr(bstrName, '\\');
						if (f)
							{
							f++;
							bool Include = false;

							for (auto& a : Compilands)
								{
								auto zz = a.c_str();
								if (_wcsicmp(f, zz) == 0)
									{
									Include = true;
									break;
									}
								}
							if (Include)
								{
								parsesymbols(pCompiland, fs);
								}
							}
						}
					pCompiland = 0;
					}

				}



			// Serialize it
			XML3::XML x;
			x.GetRootElement().FindVariableZ("ba", true)->SetValueULongLong(ba);
			for (auto& a : fs)
				{
				XML3::XMLElement& ee = x.GetRootElement().AddElement("<e />");
				a.second.ToEl(ee);
				}

			ser = x.Serialize();
			return S_OK;
			}

		
			HRESULT ApplyPatch64(unsigned long long& va, unsigned long long nf)
				{
				va -= 14;

				// Method with JMP RAX
				unsigned char jmp[12] = { 0 };
				// mov rax, 64 bit addr, ret
				jmp[0] = 0x48;
				jmp[1] = 0xb8;
				unsigned long long* xa = (unsigned long long*)(jmp + 2);
				*xa = (unsigned long long)nf;
				jmp[10] = 0xFF;
				jmp[11] = 0xE0;

				BOOL Ffx = false;
				SIZE_T ts = 0;
				Ffx = WriteProcessMemory(GetCurrentProcess(), (LPVOID)va, (char*)jmp, 12, &ts);
				if (!Ffx)
					return E_FAIL;

				/*
				// Method with long jmp 0xFF 0x25
				unsigned char jmp[14] = {0};
				jmp[0] = 0xFF;
				jmp[1] = 0x25;
				jmp[2] = 0x00;
				jmp[3] = 0x00;
				jmp[4] = 0x00;
				jmp[5] = 0x00;
				unsigned long long* xa = (unsigned long long*)(jmp + 6);
				*xa = (unsigned long long)nf;
				Ffx = WriteProcessMemory(GetCurrentProcess(),(LPVOID)va,(char*)jmp,14,&ts);
				if (!Ffx)
				return E_FAIL;
				*/
				/*
				// Method with push and pop
				unsigned long long paddr = (unsigned long long)nf;
				unsigned char jmp[14] = {0};
				jmp[0] = 0x68;
				unsigned long* xa = (unsigned long*)(jmp + 1);
				memcpy(xa,&paddr,4);
				jmp[5] = 0x68;
				xa = (unsigned long*)(jmp + 6);
				paddr >>= 32;
				memcpy(xa,&paddr,4);
				jmp[10] = 0xC3;

				Ffx = WriteProcessMemory(GetCurrentProcess(),(LPVOID)va,(char*)jmp,11,&ts);
				if (!Ffx)
				return E_FAIL;
				*/

				// And the jump as an atomic write
				va += 14;
				unsigned short b = 0xF0EB; // jmp $-14
				unsigned short* ps = (unsigned short*)va;
				DWORD oldp = 0, oldp2 = 0;
				if (!VirtualProtect(ps, 2, PAGE_EXECUTE_READWRITE, &oldp))
					return E_FAIL;
				*ps = b;
				VirtualProtect(ps, 2, oldp, &oldp2);
				return S_OK;
				}

			HRESULT ApplyPatch32(unsigned long long& va,DWORD nf)
				{
				// 5 bytes back va
				va -= 5;


				// jmp 0xE9 + address - 5
				unsigned char jmp[5] = { 0 };
				jmp[0] = 0xE9;
				DWORD* d = (DWORD*)(jmp + 1);
				unsigned long df = (DWORD)nf;
				df -= (unsigned long)va;
				*d = df;
				*d -= 5;

				BOOL Ffx = false;
				SIZE_T ts = 0;
				Ffx = WriteProcessMemory(GetCurrentProcess(), (LPVOID)va, (char*)jmp, 5, &ts);
				if (!Ffx)
					return E_FAIL;

				// And the jump as an atomic write
				va += 5;
				unsigned short b = 0xF9EB; // jmp $-5
				unsigned short* ps = (unsigned short*)va;
				DWORD oldp = 0, oldp2 = 0;
				if (!VirtualProtect(ps, 2, PAGE_EXECUTE_READWRITE, &oldp))
					return E_FAIL;
				*ps = b;
				VirtualProtect(ps, 2, oldp, &oldp2);
				return S_OK;
				}

HRESULT HOTPATCH::ApplyPatchFor(HMODULE hM,const wchar_t* funcname,void* nf, XML3::XML* pxPatch  = 0)
	{
	int a[10];
	a[1] = 0;
	1[a] = 3;

	if (!funcname || !nf)
		return E_INVALIDARG;


	XML3::XML xPatch;
	if (pxPatch)
		xPatch = *pxPatch;

	// Load the resource now...
	if (xPatch.GetRootElement().GetChildrenNum() == 0)
		{
		HRSRC R = FindResource(hM,L"HOTPATCHDATA",RT_RCDATA);
		if (!R)
			return E_FAIL;
		HGLOBAL hG = LoadResource(hM,R);
		if (!hG)
			return E_NOINTERFACE;
		DWORD S = SizeofResource(hM,R);
		char* p = (char*)LockResource(hG);
		if (!p)
			{
			FreeResource(R);
			return E_NOINTERFACE;
			}
		vector<char> d(S + 1);
		memcpy(d.data(),p,S);
		FreeResource(R);
		xPatch.Parse(d.data(),d.size());
		}

	XML3::XML& x = xPatch;

	unsigned long long OldBA = x.GetRootElement().FindVariableZ("ba",true)->GetValueULongLong();

	for (size_t i = 0; i < x.GetRootElement().GetChildrenNum(); i++)
		{	
		auto e = x.GetRootElement().GetChildren()[i];
		wstring fn = XML3::XMLU(e->vv("n").GetValue().c_str());
		if (fn != funcname)
			continue;

		unsigned long long va = e->vv("va").GetValueULongLong();
		if (!va)
			break;

		va -= OldBA;
		va += (unsigned long long)hM;

#ifdef _WIN64
		ApplyPatch64(va, (unsigned long long)nf);
#else
		ApplyPatch32(va,(DWORD)nf);
#endif
		break; // End
		}

	return S_OK;
	}


HRESULT ApplyPatchForDirect(void*of, void* nf)
	{

	if (!of || !nf)
		return E_INVALIDARG;

	unsigned long long va = (unsigned long long)of;
#ifdef _WIN64
		ApplyPatch64(va, (unsigned long long)nf);
#else
		ApplyPatch32(va, (DWORD)nf);
#endif
	return S_OK;
	}

#ifndef HOTPATCH_NO_INTEROP
	

		HRESULT ApplyUSMPatchFor(XML3::XML& xPatch,HMODULE hm, const wchar_t* funcname, size_t midx)
			{
			if (!u_sm)
				return E_FAIL;
			size_t p = sizeof(COMCALL)*midx;
			char* p2 = u_sm->BeginWrite();
			p2 += 1;
			p2 += p;
			COMCALL* cc = new (p2) COMCALL((IDispatch*)midx, this, (size_t)&USMPatchGeneric, funcname);
			HRESULT hx = ApplyPatchFor(hm, funcname, cc,&xPatch);
			u_sm->EndWrite();
			return hx;
			}

		HRESULT ApplyCOMPatchFor(XML3::XML& xPatch,HMODULE hm, const wchar_t* funcname)
			{
			DWORD oldp = 0;

			LoadRemoteInterface();
			if (!RemoteInterface)
				return E_FAIL;

			char* px = (char*)VirtualAlloc(0, sizeof(COMCALL), MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
			if (!px)
				return E_FAIL;
			VirtualProtect(px, sizeof(COMCALL), PAGE_EXECUTE_READWRITE, &oldp);

			VirtualAllocPointers.push_back(px);

			COMCALL* cc = new (px)COMCALL(RemoteInterface.operator IDispatch *(), this, (size_t)&COMPatchGeneric, funcname);
			return ApplyPatchFor(hm, funcname, cc,&xPatch);
			}






		HRESULT PrepareExecutableForCOMPatching(const wchar_t*e, CLSID cid, const wchar_t* pid)
			{
			ccid = cid;
			exe = e;
			progid = pid;
			return S_OK;
			}



		HRESULT PrepareExecutableForUSMPatching(const wchar_t*e, CLSID cid)
			{
			ccid = cid;
			exe = e;

			TCHAR cidx[1000] = { 0 };
			StringFromGUID2(cid, cidx, 1000);

			u_sm = std::make_shared<usm<char>>(usm<char>(0));
			try
				{
				u_sm->CreateInit(cidx, true, 1024 * 1024,100,true,true);
				if (u_sm->Initialize() == -1)
					{
					u_sm->End();
					return E_FAIL;
					}
				}
			catch (...)
				{
				return E_FAIL;
				}

			return S_OK;
			}

#endif
		HRESULT HOTPATCH::AckGetPatchNames(vector<wstring>& s)
			{
			if (u_sm)
				{
				TCHAR cidx[1000] = { 0 };
				StringFromGUID2(ccid, cidx, 1000);
				mutual u_mu(cidx, nullptr, 0, true);
				DWORD dw = 1;
				u_sm->WriteData((const char*)&dw, 4);
				u_mu.request();

				// Read the stuff...
				vector<char> d(1024 * 1024);
				u_sm->ReadData(d.data(), 1024 * 1024);
				s = split<std::wstring, std::wstringstream, wchar_t>(wstring((wchar_t*)d.data()), L' ');
				return S_OK;
				}

			HRESULT hrx = LoadRemoteInterface();
			if (!RemoteInterface || FAILED(hrx))
				return E_POINTER;


			DISPPARAMS dp = { 0 };
			dp.cArgs = 1;
			VARIANT vup = { 0 };
			BSTR val = SysAllocString(L"");
			vup.vt = VT_BSTR | VT_BYREF;
			vup.pbstrVal = &val;
			dp.rgvarg = &vup;
			HRESULT hr = RemoteInterface->Invoke(DISPID_GETNAMES, IID_NULL, 0, 0, &dp, 0, 0, 0);

			if (FAILED(hr))
				return hr;


			s = split<std::wstring, std::wstringstream, wchar_t>(wstring(*vup.pbstrVal), L' ');
			VariantClear(&vup);

			return hr;
			}

#ifndef HOTPATCH_NO_INTEROP
		


		HRESULT HOTPATCH::StartCOMPatching()
			{
			return RegisterCOMServer(hkr);
			}

		HRESULT HOTPATCH::FinishCOMPatching()
			{
			return Unregister(hkr);
			}

		HRESULT HOTPATCH::RegisterCOMServer(HKEY root)
			{
			Unregister(root);



			// Register the ActiveX Control
			HKEY pK1 = 0, pK2 = 0;
			DWORD pos;

			TCHAR T[1000] = { 0 };
			GetCurrentDirectory(1000, T);
			wcscat_s(T, 1000, L"\\");

			wcscpy_s(T, 1000, exe.c_str());

			TCHAR cidx[1000] = { 0 };
			wcscpy_s(cidx, 1000, L"Software\\Classes\\CLSID\\");
			StringFromGUID2(ccid, cidx + wcslen(cidx), (int)(1000 - wcslen(cidx)));
			RegCreateKeyEx(root, cidx, 0, 0, REG_OPTION_NON_VOLATILE, KEY_ALL_ACCESS, 0, &pK1, &pos);
			if (!pK1)
				return E_FAIL;

			wstring pid = progid;
			const TCHAR* y1 = pid.c_str();
			RegSetValue(pK1, 0, REG_SZ, y1, (int)_tcslen(y1) * sizeof(TCHAR));


			cidx[0] = 0;
			StringFromGUID2(ccid, cidx + wcslen(cidx), (int)(1000 - wcslen(cidx)));
			RegSetValueEx(pK1, L"AppID", 0, REG_SZ, (LPBYTE)cidx, (int)wcslen(cidx) * 2);
			cidx[0] = '@';
			wcscpy_s(cidx + 1, 999, exe.c_str());
			wcscat_s(cidx, 1000, L",-5000");
			RegSetValueEx(pK1, L"LocalizedString", 0, REG_EXPAND_SZ, (LPBYTE)cidx, (int)wcslen(cidx) * 2);

			RegCreateKeyEx(pK1, L"ProgID", 0, 0, REG_OPTION_NON_VOLATILE, KEY_ALL_ACCESS, 0, &pK2, &pos);
			RegSetValue(pK2, 0, REG_SZ, y1, (int)_tcslen(y1) * sizeof(TCHAR));
			RegCloseKey(pK2);

			RegCreateKeyEx(pK1, L"LocalServer32", 0, 0, REG_OPTION_NON_VOLATILE, KEY_ALL_ACCESS, 0, &pK2, &pos);
			RegSetValue(pK2, 0, REG_SZ, T, (int)wcslen(T) * sizeof(TCHAR));

			RegCreateKeyEx(pK1, L"Version", 0, 0, REG_OPTION_NON_VOLATILE, KEY_ALL_ACCESS, 0, &pK2, &pos);
			RegSetValue(pK2, 0, REG_SZ, L"1.0", 3);
			RegCloseKey(pK2);

			RegCloseKey(pK1);

			pK2 = 0;
			swprintf_s(cidx, 1000, L"Software\\Classes\\%s", progid.c_str());
			RegCreateKeyEx(root, cidx, 0, 0, REG_OPTION_NON_VOLATILE, KEY_ALL_ACCESS, 0, &pK1, &pos);

			if (!pK1)
				return E_FAIL;

			pK2 = 0;
			RegCreateKeyEx(pK1, L"CLSID", 0, 0, REG_OPTION_NON_VOLATILE, KEY_ALL_ACCESS, 0, &pK2, &pos);
			if (pK2)
				{
				StringFromGUID2(ccid, cidx, 1000);
				RegSetValue(pK2, 0, REG_SZ, cidx, 3);
				RegCloseKey(pK2);
				}

			RegCloseKey(pK1);

			wcscpy_s(cidx, 1000, L"Software\\Classes\\AppID\\");
			TCHAR* fionly = wcsrchr(T, '\\');
			fionly++;
			wcscat_s(cidx, 1000, fionly);
			RegCreateKeyEx(root, cidx, 0, 0, REG_OPTION_NON_VOLATILE, KEY_ALL_ACCESS, 0, &pK1, &pos);
			if (!pK1)
				return E_FAIL;
			StringFromGUID2(ccid, cidx, 1000);
			RegSetValueEx(pK1, L"AppID", 0, REG_SZ, (LPBYTE)cidx, (int)wcslen(cidx) * 2);
			RegCloseKey(pK1);



			// AppID InteractiveUser
			wcscpy_s(cidx, 1000, L"Software\\Classes\\AppID\\");
			StringFromGUID2(ccid, cidx + wcslen(cidx), (int)(1000 - wcslen(cidx)));
			RegCreateKeyEx(root, cidx, 0, 0, REG_OPTION_NON_VOLATILE, KEY_ALL_ACCESS, 0, &pK1, &pos);
			if (!pK1)
				return E_FAIL;

			RegSetValue(pK1, 0, REG_SZ, progid.c_str(), 0);
			RegCloseKey(pK1);
			return S_OK;
			}

		HRESULT HOTPATCH::QueryEndInterfaces()
			{
			if (ReleaseInterfacesCallback)
				ReleaseInterfacesCallback();
			return S_OK;
			}

		




		HRESULT HOTPATCH::StartUSMServer(CLSID cid, std::function<HRESULT(vector<NAMEANDPOINTER>&)> pnf, std::function<void()> releasecallback)
			{
			if (!u_sm)
				return E_FAIL;
			bool EndL = false;
			TCHAR cidx[1000] = { 0 };
			StringFromGUID2(cid, cidx, 1000);
			this->ReleaseInterfacesCallback = releasecallback;
			mutual mu(cidx, [&](unsigned long long)
				{
				// Requests
				DWORD dw = 0;
				u_sm->ReadData((char*)&dw, 4);
				if (dw == 1)
					{
					wstring n;
					PatchNamesFunction = pnf;
					pnf(nap);

					for (auto& a : nap)
						{
						n += a.n;
						n += L" ";
						}
					u_sm->WriteData((const char*)n.c_str(), n.length() * sizeof(wchar_t));
					EndL = true;
					return;
					}
				}
			, 0, true);
			for (;;)
				{
				if (EndL)
					break;
				Sleep(500);
				}

			// mutual for ending
			StringFromGUID2(cid, cidx, 1000);
			wcscat_s(cidx, 1000, L"-end");
			new mutual(cidx, [&](unsigned long long h)
				{
				HOTPATCH* hp = (HOTPATCH*)h;
				hp->GetReleaseInterface()();
				}, (unsigned long long)this, true);

			return S_OK;
			}

		inline HRESULT StartCOMServer(CLSID cid, std::function<HRESULT(vector<NAMEANDPOINTER>&)> pnf, std::function<void()> releasecallback);

		HRESULT HOTPATCH::EndUSMServer()
			{
			if (u_sm)
				{
				// Mutual request
				TCHAR cidx[1000] = { 0 };
				StringFromGUID2(ccid, cidx, 1000);
				wcscat_s(cidx, 1000, L"-end");
				mutual m(cidx, nullptr, 0, true);
				m.request(0);
				}
			return S_OK;
			}

		HRESULT HOTPATCH::EndCOMServer()
			{
			if (regi == 0)
				return S_FALSE;

			HRESULT hr = CoRevokeClassObject(regi);
			if (SUCCEEDED(hr))
				regi = 0;
			return hr;
			}


#endif

	};



class MyHotPatch : public IDispatch
{
private:

	ULONG r = 0;
	HOTPATCH* hp = 0;

public:

	MyHotPatch(HOTPATCH* hh)
	{
		hp = hh;
		r = 0;
		AddRef();
	}

	// IUnknown
	virtual ULONG WINAPI AddRef()
	{
		return ++r;
	}
	virtual HRESULT WINAPI QueryInterface(REFIID riid, void ** object)
	{
		if (!object)
			return E_POINTER;
		*object = 0;
		if (IsEqualIID(riid, __uuidof(IDispatch))) *object = this;
		if (IsEqualIID(riid, IID_IUnknown)) *object = this;

		if (*object)
		{
			IUnknown* u = (IUnknown*)*object;
			u->AddRef();
		}
		return *object ? S_OK : E_NOINTERFACE;
	}
	virtual ULONG WINAPI Release()
	{
		if (r == 1)
		{
			r = 0;
			hp->QueryEndInterfaces();
			delete this;
			return 0;
		}
		return --r;
	}


	// IDispatch
	HRESULT _stdcall GetTypeInfoCount(unsigned int * c)
	{
		if (!c)
			return E_POINTER;
		*c = 1;
		return S_OK;
	}

	HRESULT __stdcall GetTypeInfo(unsigned int idx, LCID, ITypeInfo ** ty)
	{
		if (idx != 0)
			return DISP_E_BADINDEX;
		if (ty)
			return E_POINTER;
		if (!hp->pTLib)
			return E_FAIL;
		return hp->pTLib->GetTypeInfo(0, ty);
	}

	HRESULT _stdcall GetIDsOfNames(REFIID, LPOLESTR* n1, unsigned int n2, LCID, DISPID FAR* n4)
	{
		if (!hp->pTLib)
			return E_FAIL;
		CComPtr<ITypeInfo> ty = 0;
		hp->pTLib->GetTypeInfo(0, &ty);
		return DispGetIDsOfNames(ty, n1, n2, n4);
	}

	HRESULT _stdcall Invoke(DISPID d, REFIID, LCID, WORD flg, DISPPARAMS* p, VARIANT FAR* res, EXCEPINFO FAR* e, unsigned int FAR* a)
	{
	// Check DISPIDs
	if (d == DISPID_GETNAMES)
		{
		// One param BSTR
		if (p->cArgs != 1) return E_INVALIDARG;
		return GetNames(p->rgvarg[0].pbstrVal);
		}
	if (d == DISPID_CALL)
		{
		// One param BSTR
		if (p->cArgs != 2) return E_INVALIDARG;
		return Call(p->rgvarg[1].bstrVal,p->rgvarg[0].parray);
		}

		CComPtr<ITypeInfo> ty = 0;
		hp->pTLib->GetTypeInfo(0, &ty);
		return DispInvoke(this, ty, d, flg, p, res, e, a);
	}




	// IHotPatch
	virtual HRESULT __stdcall GetNames(BSTR* b)
	{
		if (!b)
			return E_POINTER;

		// Request names
		vector<HOTPATCH::NAMEANDPOINTER> ns;
		if (hp->PatchNamesFunction == 0)
			return E_FAIL;
		if (FAILED(hp->PatchNamesFunction(ns)))
			return E_FAIL;
		wstring fx;
		for (auto& a : ns)
		{
			fx += a.n;
			fx += L" ";
		}
		BSTR val = SysAllocString(fx.c_str());
		*b = val;
		return S_OK;

	}


	virtual HRESULT __stdcall Call(BSTR b, SAFEARRAY*s)
	{
		if (!b || !s)
			return E_INVALIDARG;

		// BSTR of function name, and ARRAY of ULONGLONG
		if (hp->PatchNamesFunction == 0)
			return E_FAIL;
		vector<HOTPATCH::NAMEANDPOINTER> ns;
		if (FAILED(hp->PatchNamesFunction(ns)))
			return E_FAIL;

		CComSafeArray<size_t> sz;
		sz.Attach(s);

		// Call it...
		for (size_t i = 0; i < ns.size(); i++)
		{
			if (ns[i].n == b)
			{
				vector<size_t> ps;
				for (size_t y = 0; y < sz.GetCount(0); y++)
					ps.push_back((size_t)sz.GetAt((LONG)y));
				ns[i].f(ps.data());
			}
		}
		sz.Detach();
		return S_OK;
	}



};


class LocalClassFactory : public IClassFactory
	{
	public:

		HOTPATCH* hp = 0;
		int refNum;
		LocalClassFactory(HOTPATCH* hh)
			{
			hp = hh;
			refNum = 0;
			AddRef();
			}

		long  WINAPI QueryInterface(REFIID riid,void ** object)
			{
			if (!object)
				return E_POINTER;
			*object = 0;
			if (IsEqualIID(riid,IID_IClassFactory)) *object = this;
			if (IsEqualIID(riid,IID_IUnknown)) *object = this;
			if (*object)
				{
				IUnknown* u = (IUnknown*)*object;
				u->AddRef();
				}
			return *object ? S_OK : E_NOINTERFACE;
			}

		DWORD WINAPI AddRef()
			{
			return ++refNum;
			}
		DWORD WINAPI Release()
			{
			if (refNum == 1)
				{
				refNum = 0;
				delete this;
				return 0;
				}
			return --refNum;
			}

		// IClassFactory
		STDMETHODIMP LockServer(BOOL)
			{
			return S_OK;
			}

		STDMETHODIMP CreateInstance(LPUNKNOWN,REFIID riid,void** ppvObj)
			{
			if (riid == __uuidof(IUnknown) || riid == __uuidof(IClassFactory))
				{
				AddRef();
				*ppvObj = (void*)this;
				return S_OK;
				}
			if (riid == __uuidof(IDispatch))
				{
				MyHotPatch* x = new MyHotPatch(hp);
				*ppvObj = (void*)x;
				return S_OK;
				}

			return  E_NOINTERFACE;
			}


	};
inline HRESULT HOTPATCH::StartCOMServer(CLSID cid, std::function<HRESULT(vector<NAMEANDPOINTER>&)> pnf, std::function<void()> releasecallback)
	{
	regi = 0;
	PatchNamesFunction = pnf;
	ReleaseInterfacesCallback = releasecallback;
	HRESULT hr = E_FAIL;
	if (SUCCEEDED(IsRegistered(hkr)))
		{
		LocalClassFactory* x = new LocalClassFactory(this);
		hr = CoRegisterClassObject(cid, (IUnknown*)x, CLSCTX_LOCAL_SERVER | CLSCTX_REMOTE_SERVER, REGCLS_SINGLEUSE, &regi);
		}
	return hr;
	}

inline void gethpgid(void* hpp, TCHAR* cidx, int ms)
	{
	HOTPATCH* hp = (HOTPATCH*)hpp;
	StringFromGUID2(hp->GetCID(), cidx, ms);
	}
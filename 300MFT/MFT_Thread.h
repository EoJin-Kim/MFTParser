#pragma once


DWORD WINAPI MyThreadMFTList(LPVOID lpParam);

typedef struct {
	ULONGLONG startCount;
	ULONGLONG Endcount;
	int threadNum;
}ThreadParam;
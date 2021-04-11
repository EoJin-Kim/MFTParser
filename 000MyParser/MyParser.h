#pragma once
#include "stdafx.h"


class MyParser
{
private:

private:

public:
	int ReadByte(HANDLE hdrive, OUT UCHAR* buf, ULONG len, DWORD pos = FILE_CURRENT, ULONGLONG offset=0);
	int Read512Byte(HANDLE hdrive, UCHAR* buf, ULONGLONG offset = 0);
	ULONGLONG LittleE2Dec(UCHAR* dataPT, int len);

	void LittleE2BigE(UCHAR* dataPT, int len);
	void writeFile();
};
#include "stdafx.h"
#include "MyParser.h"


int MyParser::ReadByte(HANDLE hdrive, OUT UCHAR* buf, ULONG len, DWORD pos ,ULONGLONG offset)
{
	//UCHAR buf[SECTOR_SIZE];
	DWORD readn;
	BOOL brtv;

	//파일 포인터 변경
	LARGE_INTEGER curPtr;
	curPtr.QuadPart = offset; //파일 포인터 조정

	//FILE_BEGIN           0
	//FILE_CURRENT         1
	//FILE_END             2
	SetFilePointerEx(hdrive, curPtr, NULL, pos);
	
	//brtv=ReadFile(this->hdrive, buf,SECTOR_SIZE, &readn,NULL);
	brtv = ReadFile(hdrive, buf, len, &readn, NULL);
	if (readn == 0)
	{
		_tprintf(TEXT("Byte Read Error!!\n"));
		return 0;
	}
	//_tprintf(TEXT("%lu Byte Read!!\n"), len);
	return 1;
}

int MyParser::Read512Byte(HANDLE hdrive, OUT UCHAR* buf, ULONGLONG offset)
{
	//UCHAR buf[SECTOR_SIZE];
	DWORD readn;
	BOOL brtv;

	//파일 포인터 변경
	LARGE_INTEGER curPtr;
	curPtr.QuadPart = offset; //파일 포인터 조정

	//FILE_BEGIN           0
	//FILE_CURRENT         1
	//FILE_END             2
	SetFilePointerEx(hdrive, curPtr, NULL, FILE_BEGIN);

	//brtv=ReadFile(this->hdrive, buf,SECTOR_SIZE, &readn,NULL);
	brtv = ReadFile(hdrive, buf, 512, &readn, NULL);
	if (readn == 0)
	{
		//_tprintf(TEXT("512 Byte Read Error!!\n"));
		return 0;
	}
	//_tprintf(TEXT("512 Byte Read!!\n"));
	return 1;
}



ULONGLONG MyParser::LittleE2Dec(UCHAR* dataPT, int len)
{
	ULONGLONG result = 0;
	for (int i = 0; i < len; i++)
	{
		result = result | (ULONGLONG)(dataPT[i] << i * 8);
	}
	//_tprintf(TEXT("test : %llu \n"), result);
	return result;
}
void LittleE2BigE(UCHAR* dataPT, int len)
{

	int idx = len - 1;
	//UCHAR* tmpPT=(UCHAR*)malloc(sizeof(UCHAR)*len);
	UCHAR* tmpPT = new UCHAR(sizeof(UCHAR) * len);
	UCHAR tmp;
	for (int i = 0; i < len / 2; i++)
	{
		tmp = tmpPT[i];
		tmpPT[i] = dataPT[idx - i];
		tmpPT[idx - i] = dataPT[i];

	}
	delete tmpPT;
}
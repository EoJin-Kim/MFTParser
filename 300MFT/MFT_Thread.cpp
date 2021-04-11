#include "stdafx.h"
#include "MFT_Thread.h"

DWORD WINAPI MyThreadMFTList(LPVOID lpParam)
{
	
    
	ThreadParam threadParam = *(ThreadParam*)lpParam;
	printf("Thread %d Start %llu ~ %llu\n", threadParam.threadNum, threadParam.startCount,threadParam.Endcount);
	MFT_Parser *MFTtmp = new MFT_Parser();
	// txt에 저장하는 코드
	
	wchar_t wfName[20]= L"";
	swprintf(wfName, L"write%d.txt", threadParam.threadNum);
	
	
	DWORD dwWrite, dwRead;
	HANDLE wFile;
	std::wstring wDatas = L"asdasd";
	wchar_t lineFeed = L'\n';
	wFile = CreateFileW(wfName, GENERIC_READ | GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);

	if (wFile == INVALID_HANDLE_VALUE)
	{
		_tprintf(TEXT("File Not Open\n"));
		return 0;
	}

	MFTtmp->MFTTraversal(threadParam.startCount, threadParam.Endcount);

	//WriteFile(wFile, wDatas.c_str(), wDatas.size() * sizeof(wchar_t), &dwWrite, NULL);
	wDatas.clear();
	CloseHandle(wFile);
	delete MFTtmp;
    return 0;
}



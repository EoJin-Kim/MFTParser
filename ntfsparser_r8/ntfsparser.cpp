/************************************************************************
ntfsparser
Copyright (C) 2013  greenfish, greenfish77 @ gmail.com

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
************************************************************************/

#include "stdafx.h"

// inode cache ���� ���Ǹ� ����, map�� ����ϴµ�,
// ��� visual studio���� stl::map::insert�� ����ϸ� leak�� ����ȴ�.
// �װ��� ����, visual studio���� service pack�� �����Ǵµ�,
// patch�� �������� ���� �ý��ۿ����� leak�� ����� ���̴�.
// �׷� ���, atl map�� ����Ѵ�.
// �⺻���� atl map�� ����ϰ�,
// �ش� patch�� �ذ�Ǿ��ٸ�, �Ʒ� define�� �ּ� ó���϶�.
// �׷���, atl ��� stl�� ����ϰ� �� ���̴�.
#define USING_INODECACHE_ATL_MAP

//////////////////////////////////////////////////////////////////////////
// My~Cache �Լ������� std ���
#ifdef USING_INODECACHE_ATL_MAP
	// atl ���
	#include <atlcoll.h>
	#include <atlstr.h>
#else
	// stl ���
	#include <map>
	#include <string>
	#include <iostream>
	using namespace std;
#endif
// 
//////////////////////////////////////////////////////////////////////////

#include "NTFSHelper.h"
#include <io.h>
#include <time.h>
#include <assert.h>
#if defined(_WIN32) && defined(_DEBUG)
	#include <crtdbg.h> 
#endif
#if defined(_WIN32)
	#include <locale.h>
	#include <strsafe.h>
#endif

// �ܺ� I/O �Լ� ����
static CNTFSHelper::IO_LPVOID __cdecl MyOpenIo(IN LPCSTR lpszPath, OPTIONAL IN LPVOID pUserIoContext);
static BOOL __cdecl MyCloseIo(IN CNTFSHelper::IO_LPVOID pIoHandle);
static BOOL __cdecl MySeekIo(IN CNTFSHelper::IO_LPVOID pIoHandle, IN CNTFSHelper::UBYTE8 nPos, IN INT nOrigin);
static INT __cdecl MyReadIo(IN CNTFSHelper::IO_LPVOID pIoHandle, OUT LPVOID pBuffer, IN unsigned int count);

// Inode cache �Լ� ����
static CNTFSHelper::HANDLE_INODE_CACHE MyCreateInodeCache(OPTIONAL IN LPVOID pUserContext);
static BOOL MyGetCache(IN CNTFSHelper::HANDLE_INODE_CACHE hHandle, IN CNTFSHelper::TYPE_INODE nStartInode, IN LPCWSTR lpszNameUpperCase, OUT PBOOL pbFound, OUT CNTFSHelper::LPST_INODEDATA pstInodeData, OPTIONAL IN LPVOID pUserContext);
static BOOL MySetCache(IN CNTFSHelper::HANDLE_INODE_CACHE hHandle, IN CNTFSHelper::TYPE_INODE nStartInode, IN LPCWSTR lpszNameUpperCase, IN CNTFSHelper::LPST_INODEDATA pstInodeData, OPTIONAL IN LPVOID pUserContext);
static VOID MyDestroyCache(IN CNTFSHelper::HANDLE_INODE_CACHE hHandle, OPTIONAL IN LPVOID pUserContext);

VOID DUMP_MEMORY(IN char* p, IN INT nSize)
{
	INT	i		= 0;
	INT	j		= 0;
	INT	t		= 0;
	INT	l		= 0;
	CHAR	szLine[17]	= {0,};

	if (NULL == p)
	{
		return;
	}

	printf("Addr = 0x%x\r\n", p);
	for (i=0; i<(nSize + 15) / 16; i++)
	{
		t = i * 16;
		if (t > nSize)
		{
			t = nSize;
		}

		fprintf(stdout, "[%.5X] ", i*16);

		l = 0;
		for (j=i*16; j<(i+1)*16; j++)
		{
			if (j>=nSize)
			{
				fprintf(stdout, "   ");
				szLine[l] = ' ';
			}
			else
			{
				fprintf(stdout, "%0.2X ", (unsigned char)p[j]);
				if ((32 <= p[j]) && (p[j] <= 127))
				{
					szLine[l] = p[j];
				}
				else
				{
					szLine[l] = '.';
				}
			}
			l++;
		}

		fprintf(stdout, " %s\r\n", szLine);
	}
}

INT g_nCount = 0;

// �ش� ��γ����� \�� ������ ����.
//
// ���� Win32 API ���·� ����ϸ�,
// CNTFSHelper::FindFirstFile���� Inode�� ã�� ������ �ִµ�,
// �� �������� I/O�� ���� �ɷ� ���ϰ� �ִ�.
// �׷��� bFastAccessByInode�� TRUE�� �ϸ�, CNTFSHelper�� Ȯ�� API�� ����Ѵ�.
// Ȯ�� API���, Inode ������ �����Ͽ�, Inode�� ã�� ������ Skip�ϵ��� �ϴ� ���̴�.
VOID PrintDirectory(IN LPCWSTR lpszPath, IN BOOL bRecurse, IN BOOL bFastAccessByInode, OPTIONAL IN CNTFSHelper::LPST_INODEDATA pstInodeData, OPTIONAL IN CNTFSHelper::LPST_INODE_CACHE pstInodeCache)
{
	PWCHAR						lpszPathAlloc	= NULL;
	INT							nCchLength		= 0;
	INT							nCchRemain		= 0;
	CNTFSHelper::NTFS_HANDLE	hFind			= INVALID_HANDLE_VALUE_NTFS;
	WIN32_FIND_DATA				stData			= {0,};
	BOOL						bDotDirectory	= FALSE;

	// �ӵ� ����� ���� �߰� Data
	CNTFSHelper::ST_INODEDATA	stInodeData		= {0,};

	// http://msdn.microsoft.com/en-us/library/windows/desktop/aa365247(v=vs.85).aspx
	// �� ������ File system���� �ִ� ���̴� 32767 �����̴�.
	// �̴�,
	// C:\component\component\component\...
	// �� ���̰� 32767�� �ִ밪�̶� ���̴�.
	// �ϴ�, �̰������� 1024 ���̸� �������.
	// component�� ���̴� MAX_PATH�� ������ ���´�.
	// ���� 32767 ���̷� �����Ϸ���, naming a file, ��, \\?\�� ������ �ϴµ�,
	// �̰������� �ش� �κ��� ������� �ʴ��� �����ȴ�.
	lpszPathAlloc = (PWCHAR)malloc(sizeof(WCHAR)*1024);
	if (NULL == lpszPath)
	{
		goto FINAL;
	}
	memset(lpszPathAlloc, 0, sizeof(WCHAR)*1024);

	StringCchCopyW(lpszPathAlloc, 1024, lpszPath);

	if (TEXT('\0') != lpszPathAlloc[3])
	{
		// C:\ �̿��� ��쿡�� ���� \�� �ٿ��ش�.
		StringCchCatW(lpszPathAlloc, 1024, TEXT("\\"));
	}

	// ���̸� ���Ѵ�.
	// ��,
	// C:\abc �� ���� ���,
	// C:\abc\
	// �� ���� ����, ��, 7�� ������.
	nCchLength = wcslen(lpszPathAlloc);
	nCchRemain = 1024 - nCchLength;
	if (nCchLength < 2)
	{
		goto FINAL;
	}

	if (FAILED(StringCchCat(lpszPathAlloc, 1024, TEXT("*"))))
	{
		goto FINAL;
	}

	// ���� *�� ���� ������ �����Ѵ�.
	if (FALSE == bFastAccessByInode)
	{
		// Win32 API ����� ȣ��
		if (NULL == pstInodeCache)
		{
			// FindFirstFile ���ο� Inode ã�� ������ ����,
			// �� �Լ��� �ӵ��� ������ �� �ִ�.
			hFind = CNTFSHelper::FindFirstFile(lpszPathAlloc, &stData);
		}
		else
		{
			// Cache�� ����ϴ� ���,...
			hFind = CNTFSHelper::FindFirstFileExt(lpszPathAlloc, &stData, NULL, pstInodeCache);
		}
	}
	else
	{
		// Inode�� ����Ͽ� �����ϵ��� �Ѵ�.
		if (NULL != pstInodeData)
		{
			hFind = CNTFSHelper::FindFirstFileByInodeExt(lpszPathAlloc, pstInodeData, TRUE, &stInodeData, &stData, NULL, pstInodeCache);
		}
		else
		{
			// Inode�� ����Ͽ� �����ض�� �ϸ鼭,
			// Inode ������ ���� ������ ���� ������� �����Ѵ�.
			// (PrintDirectory�� �� ó�� ȣ���Ҷ��� �����)
			hFind = CNTFSHelper::FindFirstFileWithInodeExt(lpszPathAlloc, &stData, &stInodeData, NULL, pstInodeCache);
		}
	}

	if (INVALID_HANDLE_VALUE_NTFS == hFind)
	{
		goto FINAL;
	}

	if (FAILED(StringCchCopy(&lpszPathAlloc[nCchLength], nCchRemain, stData.cFileName)))
	{
		goto FINAL;
	}

	if (FILE_ATTRIBUTE_DIRECTORY == (stData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
	{
		// Directory�� ���,...
		// ���ȣ������.

		if (FILE_ATTRIBUTE_REPARSE_POINT == (stData.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT))
		{
			// ���� Reparse Point���,..
			_tprintf(TEXT("[REPARSE]"));
		}
		else
		{
			// Reparse Point�� �ƴ϶��, ���ȣ��� �����Ѵ�.
			if (TRUE == bRecurse)
			{
				bDotDirectory = FALSE;
				if (L'.' == stData.cFileName[0])
				{
					if ((L'.' == stData.cFileName[1]) && (L'\0' == stData.cFileName[2]))
					{
						// .. ���
						bDotDirectory = TRUE;
					}
					else if (L'\0' == stData.cFileName[1])
					{
						// . ���
						bDotDirectory = TRUE;
					}
				}

				if (FALSE == bDotDirectory)
				{
					if (FALSE == bFastAccessByInode)
					{
						PrintDirectory(lpszPathAlloc, bRecurse, FALSE, NULL, pstInodeCache);
					}
					else
					{
						PrintDirectory(lpszPathAlloc, bRecurse, TRUE, &stInodeData, pstInodeCache);
					}
				}
			}
		}
	}

	g_nCount++;
	_tprintf(TEXT("[%.6d] %s\r\n"), g_nCount, lpszPathAlloc);

	for (;;)
	{
		if (FALSE == bFastAccessByInode)
		{
			// ���� Win32 API ������ ȣ��
			if (FALSE == CNTFSHelper::FindNextFile(hFind, &stData))
			{
				break;
			}
		}
		else
		{
			// Inode ���� ��û
			if (FALSE == CNTFSHelper::FindNextFileWithInodeExt(hFind, &stData, &stInodeData))
			{
				break;
			}
		}

		StringCchCopy(&lpszPathAlloc[nCchLength], nCchRemain, stData.cFileName);

		if (FILE_ATTRIBUTE_DIRECTORY == (stData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
		{
			// Directory�� ���,...
			// ���ȣ������.

			if (FILE_ATTRIBUTE_REPARSE_POINT == (stData.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT))
			{
				// ���� Reparse Point���,..
				_tprintf(TEXT("[REPARSE]"));
			}
			else
			{
				// Reparse Point�� �ƴ϶��, ���ȣ��� �����Ѵ�.
				if (TRUE == bRecurse)
				{
					bDotDirectory = FALSE;
					if (L'.' == stData.cFileName[0])
					{
						if ((L'.' == stData.cFileName[1]) && (L'\0' == stData.cFileName[2]))
						{
							// .. ���
							bDotDirectory = TRUE;
						}
						else if (L'\0' == stData.cFileName[1])
						{
							// . ���
							bDotDirectory = TRUE;
						}
					}

					if (FALSE == bDotDirectory)
					{
						if (FALSE == bFastAccessByInode)
						{
							PrintDirectory(lpszPathAlloc, bRecurse, FALSE, NULL, pstInodeCache);
						}
						else
						{
							PrintDirectory(lpszPathAlloc, bRecurse, TRUE, &stInodeData, pstInodeCache);
						}
					}
				}
			}
		}

		g_nCount++;
		_tprintf(TEXT("[%.6d] %s\r\n"), g_nCount, lpszPathAlloc);
	}

FINAL:

	if (NULL != lpszPathAlloc)
	{
		free(lpszPathAlloc);
		lpszPathAlloc = NULL;
	}

	if (INVALID_HANDLE_VALUE_NTFS != hFind)
	{
		CNTFSHelper::FindClose(hFind);
		hFind = INVALID_HANDLE_VALUE_NTFS;
	}
	return;
}

VOID DumpFile(IN CNTFSHelper::NTFS_HANDLE hFile)
{
	unsigned char	buf[65536]	= {0,};
	DWORD			dwReaded	= 0;

	if (INVALID_HANDLE_VALUE_NTFS == hFile)
	{
		goto FINAL;
	}

	for (;;)
	{
		if (FALSE == CNTFSHelper::ReadFile(hFile, buf, 65536, &dwReaded, NULL))
		{
			printf("[ERROR] Fail to ReadFile, GetLastError=%d\r\n", ::GetLastError());
			goto FINAL;
		}
		if (0 == dwReaded)
		{
			break;
		}
		DUMP_MEMORY((char*)buf, dwReaded);
	}
FINAL:
	return;
}

VOID DisplayFileAttrib(IN LPCWSTR lpszPath, IN CNTFSHelper::TYPE_ATTRIB nTypeAttrib)
{
	CNTFSHelper::NTFS_HANDLE hFile = INVALID_HANDLE_VALUE_NTFS;

	if (NULL == lpszPath)
	{
		assert(FALSE);
		return;
	}

	hFile = CNTFSHelper::CreateFileByAttrib(lpszPath, GENERIC_READ, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL, nTypeAttrib, NULL);
	if (INVALID_HANDLE_VALUE_NTFS == hFile)
	{
		printf("[ERROR] CreateFileByAttrib Failed, GetLastError=%d\r\n", ::GetLastError());
		goto FINAL;
	}

	wprintf(L"[INFO] %s (attrib: 0x%x) size : %d\r\n", lpszPath, nTypeAttrib, CNTFSHelper::GetFileSize(hFile, NULL));

	DumpFile(hFile);

FINAL:
	if (INVALID_HANDLE_VALUE_NTFS != hFile)
	{
		CNTFSHelper::CloseHandle(hFile);
		hFile = INVALID_HANDLE_VALUE_NTFS;
	}
	return;
}

VOID DisplayFile(IN LPCWSTR lpszPath)
{
	CNTFSHelper::NTFS_HANDLE hFile = INVALID_HANDLE_VALUE_NTFS;

	if (NULL == lpszPath)
	{
		assert(FALSE);
		return;
	}

	hFile = CNTFSHelper::CreateFile(lpszPath, GENERIC_READ, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	if (INVALID_HANDLE_VALUE_NTFS == hFile)
	{
		printf("[ERROR] CreateFileByAttrib Failed, GetLastError=%d\r\n", ::GetLastError());
		goto FINAL;
	}

	wprintf(L"[INFO] %s size : %d\r\n", lpszPath, CNTFSHelper::GetFileSize(hFile, NULL));

	DumpFile(hFile);

FINAL:
	if (INVALID_HANDLE_VALUE_NTFS != hFile)
	{
		CNTFSHelper::CloseHandle(hFile);
		hFile = INVALID_HANDLE_VALUE_NTFS;
	}
	return;
}

#ifdef USING_INODECACHE_ATL_MAP
	// inode cache�� ������ ���� ���� ������� �Ѵ�.
	// atl ���
	CAtlMap<CAtlString, CNTFSHelper::ST_INODEDATA> g_cache;
#else
	// inode cache�� ������ ���� ���� ������� �Ѵ�.
	// stl ���
	map<wstring, CNTFSHelper::ST_INODEDATA> g_cache;
#endif

int _tmain(int argc, _TCHAR* argv[])
{
	CNTFSHelper::LPST_NTFS					pstNtfs					= NULL;
	CNTFSHelper::LPST_NTFS					pstNtfsIoDefine			= NULL;
	CNTFSHelper::LPST_BITMAP_INFO			pstBitmap				= NULL;
	CNTFSHelper::UBYTE8						nLCN					= 0;
	CNTFSHelper::LPST_FIND_ATTRIB			pstFindAttrib			= NULL;
	CNTFSHelper::LPST_FIND_ATTRIB			pstFindAttribFileName	= NULL;
	CNTFSHelper::LPST_FIND_ATTRIB			pstFindAttribFileData	= NULL;
	CNTFSHelper::LPST_ATTRIB_FILE_NAME		pstAttribFileName		= NULL;
	CNTFSHelper::LPST_RUNLIST				pstRunlist				= NULL;
	CNTFSHelper::_OFF64_T					nReaded					= 0;
	INT										nRunListCount			= 0;
	LPCWSTR									lpszVolumeName			= NULL;
	TCHAR									szVolume[MAX_PATH]		= {0,};
	char*									pBuf					= NULL;
	CNTFSHelper::_OFF64_T					nBufSize				= 0;
	BOOL									bFound					= FALSE;
	CHAR									lpszContext[]			= "hello, world";

	// Leak üũ ����
#if defined(_WIN32) && defined(_DEBUG)
	_CrtSetReportMode(_CRT_WARN, _CRTDBG_MODE_DEBUG);
	_CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_DEBUG);
//	_CrtSetBreakAlloc(250); // Leak �߻��� ������
#endif

#if defined(_WIN32)
	//////////////////////////////////////////////////////////////////////////
	// console���� �ѱ� ���
	_wsetlocale(LC_ALL, _T("korean"));
#endif

	//////////////////////////////////////////////////////////////////////////
	// Ntfs�� Open�Ѵ�.
	pstNtfs = CNTFSHelper::OpenNtfs("\\\\.\\C:");
	if (NULL == pstNtfs)
	{
		ERRORLOG("[ERROR] Fail to OpenNtfs");
		assert(FALSE);
		goto FINAL;
	}

	// �ʿ��� ������ �ִٸ� �߰��Ѵ�.
	INFOLOG("[INFO] Ntfs Opened, %s", "\\\\.\\C:");
	INFOLOG("[INFO] Revision : %s", CNTFSHelper::GetRevision(NULL));
	INFOLOG("");
	INFOLOG("[INFO] Ntfs - Total Sector : %llu", pstNtfs->stBpb.llSectorNum);
	INFOLOG("");

	//////////////////////////////////////////////////////////////////////////
	// 1. n�� cluster�� ������ΰ�?
	srand((unsigned)time(NULL));
	nLCN = rand() % pstNtfs->stNtfsInfo.nTotalClusterCount;

	// �Ʒ� BitmapAlloc ���θ� ���� Ntfs Attribute�� ã�� ���� ���ϴ� ������ Ȯ���� �� �ִ�.
	// Bitmap�� Resident���·�, RunList�� ���ϴ� ������ ���ԵǾ� ���� ���̴�.
	pstBitmap = CNTFSHelper::BitmapAlloc(pstNtfs);
	if (NULL == pstBitmap)
	{
		ERRORLOG("[ERROR] Fail to get Bitmap");
		assert(FALSE);
		goto FINAL;
	}

	if (TRUE == CNTFSHelper::IsClusterUsed(nLCN, pstBitmap))
	{
		INFOLOG("[INFO] LCN(Logical Cluster Number) %llu : Cluster Used", nLCN);
	}
	else
	{
		INFOLOG("[INFO] LCN(Logical Cluster Number) %llu : Cluster Not Used", nLCN);
	}

	//////////////////////////////////////////////////////////////////////////
	// 2. Volume �̸��� ���غ���. (Example)

	// http://inform.pucp.edu.pe/~inf232/Ntfs/ntfs_doc_v0.5/files/index.html
	// �� ���� $Volume�� "Serial number, creation time, dirty flag"�� ���� �ִµ�,
	// $Volume�� Volume �̸��� ���� ���̴�.
	// �׷���, TYPE_INODE_VOLUME���� �����Ѵ�.
	// ���� URL�� $Volume�� ��ũ�ϸ�,
	// http://inform.pucp.edu.pe/~inf232/Ntfs/ntfs_doc_v0.5/files/volume.html
	// �� ����.
	// �ش� Inode�� ����ִ� ������ list�Ǿ� �ִµ�,
	// $VOLUME_NAME�� Volume�̸��� ������ �ϴ�.
	// �׷���, TYPE_ATTRIB_VOLUME_NAME�� �����Ѵ�.
	pstFindAttrib = CNTFSHelper::FindAttribAlloc(pstNtfs, TYPE_INODE_VOLUME, TYPE_ATTRIB_VOLUME_NAME);
	if (NULL == pstFindAttrib)
	{
		ERRORLOG("[ERROR] Fail to Find Volume, VolumeName");
		assert(FALSE);
		goto FINAL;
	}

	// �־��� Attribute�� Type�� �翬�� Volume �̸����� �����Ǿ� �־�� �Ѵ�.
	if (TYPE_ATTRIB_VOLUME_NAME != pstFindAttrib->pstAttrib->dwType)
	{
		ERRORLOG("[ERROR] Invalid Find Attrib");
		assert(FALSE);
		goto FINAL;
	}

	// �� Attribute�� Resident type����, Runlist�� ���� ������,
	// Offset��ŭ �ڷ� ���� ���� �ִ�.
	// ��, �־��� ���� ��ŭ�̴�.
	// �Ʒ� ���� NULL Terminate�� �ƴ� ���ڿ��̴�.
	lpszVolumeName = (LPCWSTR)((CNTFSHelper::UBYTE1*)pstFindAttrib->pstAttrib + pstFindAttrib->pstAttrib->stAttr.stResident.wValueOffset);

	// �־��� ��ŭ ����!
	CopyMemory(szVolume, lpszVolumeName, pstFindAttrib->pstAttrib->stAttr.stResident.dwValueLength);

	// ���!
	wprintf(TEXT("[INFO] Volume name is [%s]\r\n"), szVolume);

	// ���� ���� ���������, Ntfs�� �ٸ� �⺻ ������ ������ �� �ִ�.

	//////////////////////////////////////////////////////////////////////////
	// 3. FindFirstFile �迭�� Win32 API�� ����Ͽ�, ������ ��������.

	// C:\�� ������ �����Ѵ�.
	// �Ϻ� ��� ���Ͽ� ���� �ʴ´�.
	// �ּ� ó������. �ʿ��ϴٸ� ������ ���ƶ�.
	// printf("\r\n[INFO] C:\\ file list (not recurse)\r\n");
	// PrintDirectory(TEXT("C:\\"), FALSE);

	// Vista �̻󿡼� �Ϲ������� Ž���� ���� �ʴ� ��θ� �õ��� ����.
	// C:\Windows\System32\config ������ �����Ѵ�. (Registry hive ���ϵ�)
	// �Ϻ� ��� ���Ͽ� ����.
	{
		DWORD dwStart				= 0;
		DWORD dwDurationNormal		= 0;
		DWORD dwDurationFast		= 0;
		LPCWSTR lpszTraversePath	= L"C:\\Windows\\System32\\config";

		dwStart = ::GetTickCount();
		g_nCount = 0;
		wprintf(L"\r\n[INFO] %s file list (recurse)\r\n", lpszTraversePath);
		PrintDirectory(lpszTraversePath, TRUE, FALSE, NULL, NULL);
		dwDurationNormal = ::GetTickCount() - dwStart;

		dwStart = ::GetTickCount();
		g_nCount = 0;
		wprintf(L"\r\n[INFO] %s file list Fast Mode (recurse)\r\n", lpszTraversePath);
		PrintDirectory(lpszTraversePath, TRUE, TRUE, NULL, NULL);
		dwDurationFast = ::GetTickCount() - dwStart;

		printf("\r\n[INFO] Duration(TickCount), Normal=%d, Fast=%d\r\n", dwDurationNormal, dwDurationFast);
	}

	// Inode Cache�� �̿��� ����.
	{
		DWORD dwStart				= 0;
		DWORD dwDurationNormal		= 0;
		DWORD dwDurationFast		= 0;
		LPCWSTR lpszTraversePath	= L"C:\\Windows\\System32\\config";

		// Inode Cache ������ ä���.
		CNTFSHelper::ST_INODE_CACHE	stInodeCache = {0,};
		stInodeCache.pfnCreateInodeCache	= MyCreateInodeCache;
		stInodeCache.pfnDestroyCache		= MyDestroyCache;
		stInodeCache.pfnGetCache			= MyGetCache;
		stInodeCache.pfnSetCache			= MySetCache;

		dwStart = ::GetTickCount();
		g_nCount = 0;
		wprintf(L"\r\n[INFO] %s file list (recurse)\r\n", lpszTraversePath);
		PrintDirectory(lpszTraversePath, TRUE, FALSE, NULL, &stInodeCache);
		dwDurationNormal = ::GetTickCount() - dwStart;

		dwStart = ::GetTickCount();
		g_nCount = 0;
		wprintf(L"\r\n[INFO] %s file list Fast Mode (recurse)\r\n", lpszTraversePath);
		PrintDirectory(lpszTraversePath, TRUE, TRUE, NULL, &stInodeCache);
		dwDurationFast = ::GetTickCount() - dwStart;

		printf("\r\n[INFO] Duration[Cache] (TickCount), Normal=%d, Fast=%d\r\n", dwDurationNormal, dwDurationFast);

		// ���� cache�� �����Ѵ�.
		#ifdef USING_INODECACHE_ATL_MAP
			g_cache.RemoveAll();
		#else
			g_cache.clear();
		#endif
	}

	//////////////////////////////////////////////////////////////////////////
	// 4. CreateFile �迭�� Win32 API�� �̿�����.
	{
		printf("\r\n");
		printf("[INFO] ReadFile C:\\Windows\\system.ini\r\n       (may be resident file)\r\n");
		DisplayFile(L"C:\\Windows\\system.ini");

		printf("\r\n");
		printf("[INFO] ReadFile C:\\Windows\\System32\\keyboard.drv contents\r\n       (may be non-resident file)\r\n");
		DisplayFile(L"C:\\Windows\\System32\\keyboard.drv");

		printf("\r\n");
		printf("[INFO] ReadFile C:\\$Volume -> $VOLUME_NAME attribute\r\n       (may be the volume name, C:\\Volume is meta-data file)\r\n");
		DisplayFileAttrib(L"C:\\$Volume", TYPE_ATTRIB_VOLUME_NAME);

		printf("\r\n");
		printf("[INFO] ReadFile C:\\Windows\\System32 -> $BITMAP\r\n");
		DisplayFileAttrib(L"C:\\Windows\\System32", TYPE_ATTRIB_BITMAP);
	}

	//////////////////////////////////////////////////////////////////////////
	// 5. �ܺ� I/O�� ����� ������ ����.
	{
		WIN32_FIND_DATA				stFD			= {0,};
		WCHAR						lpszContext[]	= L"hello, world";
		CNTFSHelper::ST_IO_FUNCTION	stIo			= {0,};
		CNTFSHelper::NTFS_HANDLE	hHandle			= INVALID_HANDLE_VALUE_NTFS;

		printf("\r\n");
		printf("[INFO] Test user define I/O (FindFirstFile C:\\)\r\n");

		// �ܺ� I/O �Լ����� �����Ѵ�.
		stIo.dwCbSize   = sizeof(stIo);
		stIo.pfnOpenIo  = MyOpenIo;
		stIo.pfnReadIo  = MyReadIo;
		stIo.pfnSeekIo  = MySeekIo;
		stIo.pfnCloseIo = MyCloseIo;
		stIo.pUserIoContext = lpszContext;	// MyOpenIo(...)�� ���޵ȴ�.

		hHandle = CNTFSHelper::FindFirstFileExt(L"C:\\*.*", &stFD, &stIo, NULL);
		if (INVALID_HANDLE_VALUE_NTFS != hHandle)
		{
			wprintf(L"***** FindFirstFile Finished: %s\r\n", stFD.cFileName);

			if (FALSE != CNTFSHelper::FindNextFile(hHandle, &stFD))
			{
				wprintf(L"***** FindNextFile Finished:  %s\r\n", stFD.cFileName);
			}

			CNTFSHelper::FindClose(hHandle);
			wprintf(L"***** FindCloseFile Finished\r\n");
			hHandle = INVALID_HANDLE_VALUE_NTFS;
		}
		else
		{
			wprintf(L"[ERROR] Fail to FindFirstFileIo\r\n");
		}
	}

FINAL:

	if (NULL != pBuf)
	{
		free(pBuf);
		pBuf = NULL;
		nBufSize = 0;
	}

	if (NULL != pstRunlist)
	{
		CNTFSHelper::GetRunListFree(pstRunlist);
		pstRunlist = NULL;
	}

	if (NULL != pstFindAttribFileData)
	{
		CNTFSHelper::FindAttribFree(pstFindAttribFileData);
		pstFindAttribFileData = NULL;
	}

	if (NULL != pstFindAttribFileName)
	{
		CNTFSHelper::FindAttribFree(pstFindAttribFileName);
		pstFindAttribFileName = NULL;
	}

	if (NULL != pstFindAttrib)
	{
		CNTFSHelper::FindAttribFree(pstFindAttrib);
		pstFindAttrib = NULL;
	}

	if (NULL != pstBitmap)
	{
		CNTFSHelper::BitmapFree(pstBitmap);
		pstBitmap = NULL;
	}

	if (NULL != pstNtfs)
	{
		CNTFSHelper::CloseNtfs(pstNtfs);
		pstNtfs  = NULL;
	}

	printf("\r\n");
	printf("******************************************************************\r\n");
	printf("ntfsparser Copyright (C) 2013,  greenfish, greenfish77 @ gmail.com\r\n");
	printf("This program comes with ABSOLUTELY NO WARRANTY.\r\n");
	printf("This is free software, and you are welcome to redistribute it\r\n");
	printf("under certain conditions.\r\n");
	printf("******************************************************************\r\n");

	// Leak üũ ����
#if defined(_WIN32) && defined(_DEBUG)
	if (TRUE == _CrtDumpMemoryLeaks())
	{
		printf("\r\n[WARNING] MEMORY LEAK DETECTED !!!!\r\n");
		assert(FALSE);
	}
	else
	{
		printf("[INFO] There is no memory leak\r\n");
	}
#endif

	return 0;
}

//////////////////////////////////////////////////////////////////////////
// �ܺ� ���� I/O �Լ�

// �ܺ� ���� I/O���� ���� Handle Context
typedef struct tagST_MY_IO
{
	HANDLE	hHandle;
	char	foobar[1024];
	LPCWSTR	lpszContext;
} ST_MY_IO, *LPST_MY_IO;

// �Ʒ��� �帧��� �ڵ带 �ۼ��Ѵ�.
CNTFSHelper::IO_LPVOID __cdecl MyOpenIo(IN LPCSTR lpszPath, OPTIONAL IN LPVOID pUserIoContext)
{
	ST_MY_IO	stIo		= {0,};
	LPST_MY_IO	pstRtnValue	= NULL;

	stIo.hHandle = INVALID_HANDLE_VALUE;

	// ���� I/O�� �����ϱ����� �ʱ�ȭ �۾��� �����Ѵ�.
	{
		if (NULL != pUserIoContext)
		{
			wprintf(L"\tMyOpenIo called, context=%s\r\n", (LPCWSTR)pUserIoContext);
		}

		// ...
		// ...
		// ...
	}

	stIo.hHandle = ::CreateFileA(lpszPath, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	if (INVALID_HANDLE_VALUE == stIo.hHandle)
	{
		assert(FALSE);
		goto FINAL;
	}

	// CNTFSHelper::ST_IO_FUNCTION::pUserIoContext
	// ���� ���޵ȴ�.
	// ���⿡���� L"hello, world"�� ���ŵ� ���̴�.
	stIo.lpszContext = (LPCWSTR)pUserIoContext;

	// stIo�� ���� �߰� �۾��� �ִٸ�, �����Ѵ�.
	{

	}

	pstRtnValue = (LPST_MY_IO)malloc(sizeof(*pstRtnValue));
	if (NULL == pstRtnValue)
	{
		assert(FALSE);
		goto FINAL;
	}

	*pstRtnValue = stIo;

FINAL:
	if (NULL == pstRtnValue)
	{
		// ����
		if (INVALID_HANDLE_VALUE == stIo.hHandle)
		{
			::CloseHandle(stIo.hHandle);
			stIo.hHandle = INVALID_HANDLE_VALUE;
		}
	}
	return pstRtnValue;
}

BOOL __cdecl MyCloseIo(IN CNTFSHelper::IO_LPVOID pIoHandle)
{
	BOOL		bRtnValue	= FALSE;
	LPST_MY_IO	pstIo		= NULL;

	pstIo = (LPST_MY_IO)pIoHandle;
	if (NULL == pstIo)
	{
		// NULL üũ�� �ʿ����.
		// caller���� ������ �ش�.
		goto FINAL;
	}

	// Close �۾��� ��û�Ǿ���.
	// ��ó���� �ʿ��Ѱ�?
	{
		wprintf(L"\tMyCloseIo called, context=%s\r\n", pstIo->lpszContext);

		// ...
		// ...
		// ...
	}

	if (INVALID_HANDLE_VALUE != pstIo->hHandle)
	{
		::CloseHandle(pstIo->hHandle);
	}
	pstIo->hHandle = INVALID_HANDLE_VALUE;

	// Close �۾��� ��û�Ǿ���.
	// ��ó���� �ʿ��Ѱ�?
	{

	}

	// ������� �Դٸ� ����
	bRtnValue = TRUE;

FINAL:
	if (NULL != pstIo)
	{
		free(pstIo);
		pstIo = NULL;
	}

	return bRtnValue;
}

BOOL __cdecl MySeekIo(IN CNTFSHelper::IO_LPVOID pIoHandle, IN CNTFSHelper::UBYTE8 nPos, IN INT nOrigin)
{
	BOOL			bRtnValue	= FALSE;
	LPST_MY_IO		pstIo		= NULL;
	LARGE_INTEGER	nLargeInt	= {0,};

	pstIo = (LPST_MY_IO)pIoHandle;

	// Seek �۾��� ��û�Ǿ���.
	// ��ó���� �ʿ��Ѱ�?
	{
		wprintf(L"\tMySeekIo called, context=%s, pos=%llu\r\n", pstIo->lpszContext, nPos);

		// ...
		// ...
		// ...
	}

	nLargeInt.QuadPart = nPos;

	// SEEK_CUR, FILE_CURRENT
	// SEEK_SET, FILE_BEGIN
	// SEEK_END, FILE_END
	// ���� ���� ����.
	if (INVALID_SET_FILE_POINTER == ::SetFilePointer(pstIo->hHandle, nLargeInt.LowPart, &nLargeInt.HighPart, nOrigin))
	{
		assert(FALSE);
		goto FINAL;
	}

	// Seek �۾��� ��û�Ǿ���.
	// ��ó���� �ʿ��Ѱ�?
	{

	}

	// ������� �Դٸ� ����
	bRtnValue = TRUE;

FINAL:
	return bRtnValue;
}

INT __cdecl MyReadIo(IN CNTFSHelper::IO_LPVOID pIoHandle, OUT LPVOID pBuffer, IN unsigned int count)
{
	INT			nRtnValue	= -1;
	LPST_MY_IO	pstIo		= NULL;
	DWORD		dwReaded	= 0;

	pstIo = (LPST_MY_IO)pIoHandle;

	// �б� �۾��� ��û�Ǿ���.
	// ��ó���� �ʿ��Ѱ�?
	{
		wprintf(L"\tMyReadIo called, context=%s, count=%d\r\n", pstIo->lpszContext, count);

		// ...
		// ...
		// ...
	}

	if (FALSE == ::ReadFile(pstIo->hHandle, pBuffer, count, &dwReaded, NULL))
	{
		// ����
		nRtnValue = -1;
		assert(FALSE);
		goto FINAL;
	}

	nRtnValue = (INT)dwReaded;

	// �б� �۾��� ��û�Ǿ���.
	// ��ó���� �ʿ��Ѱ�?
	{

	}

FINAL:
	return nRtnValue;
}

#ifdef USING_INODECACHE_ATL_MAP

	//////////////////////////////////////////////////////////////////////////
	// atl ��� �ڵ�
	//		atl�� visual studio������ �����Ǵ� �ڵ��̹Ƿ�,
	//		���� visual studio �̿��� compile ȯ�濡���� �Ʒ��� stl ��� �ڵ带 ����ٶ�

	CNTFSHelper::HANDLE_INODE_CACHE MyCreateInodeCache(OPTIONAL IN LPVOID pUserContext)
	{
		// ���� ������ ����Ѵ�.
		// ��, �������� Ntfs ��ü�� �������,
		// �ϳ��� ���� cache�� ����Ѵ�.
		// ���� Ntfs ��ü �ϳ��� ������ cache�� ����ϰ��� �Ѵٸ�,
		// �̰����� ���ο� cache�� �����Ҵ� �޾� ���� ������ �ֵ��� �ؾ� �Ѵ�.

		// ����ư, ���� ������ cache ����,
		// ������ �����ڸ�, �������� CRITICAL_SECTION���� Lock�� �ɾ�,
		// Thread-safe�ϰ� ���� �ʿ�� �ִ�.
		// �ϴ�, ���������� CRITICAL_SECTION�� ��������� �ʴ´�.
		return &g_cache;
	}

	BOOL MyGetCache(IN CNTFSHelper::HANDLE_INODE_CACHE hHandle, IN CNTFSHelper::TYPE_INODE nStartInode, IN LPCWSTR lpszNameUpperCase, OUT PBOOL pbFound, OUT CNTFSHelper::LPST_INODEDATA pstInodeData, OPTIONAL IN LPVOID pUserContext)
	{
		static WCHAR szTmp[MAX_PATH * 2] = {0,};

		*pbFound = FALSE;

		if (&g_cache != hHandle)
		{
			// ���� cache ���� �ٸ� ���,...
			assert(FALSE);
			return FALSE;
		}

		// StartInode�� name�� �ϳ��� string���� ���´�.
		StringCchPrintf(szTmp, MAX_PATH*2, L"%lld_%s", nStartInode, lpszNameUpperCase);

		// map�� lookup�Ѵ�.
		if (TRUE == g_cache.Lookup(szTmp, *pstInodeData))
		{
			*pbFound = TRUE;
		}

		return TRUE;
	}

	BOOL MySetCache(IN CNTFSHelper::HANDLE_INODE_CACHE hHandle, IN CNTFSHelper::TYPE_INODE nStartInode, IN LPCWSTR lpszNameUpperCase, IN CNTFSHelper::LPST_INODEDATA pstInodeData, OPTIONAL IN LPVOID pUserContext)
	{
		static WCHAR szTmp[MAX_PATH * 2] = {0,};

		if (&g_cache != hHandle)
		{
			// ���� cache ���� �ٸ� ���,...
			assert(FALSE);
			return FALSE;
		}

		// StartInode�� name�� �ϳ��� string���� ���´�.
		StringCchPrintf(szTmp, MAX_PATH*2, L"%lld_%s", nStartInode, lpszNameUpperCase);

		// map�� �߰��Ѵ�.
		g_cache.SetAt(szTmp, *pstInodeData);

		return TRUE;
	}

	VOID MyDestroyCache(IN CNTFSHelper::HANDLE_INODE_CACHE hHandle, OPTIONAL IN LPVOID pUserContext)
	{
		// do nothing...
	}

#else	// USING_INODECACHE_ATL_MAP

	//////////////////////////////////////////////////////////////////////////
	// stl ��� �ڵ�
	//		USING_INODECACHE_ATL_MAP define�� �ּ�ó���ϸ�, stl ������� compile�ȴ�.
	//		��� Visual studio ���������� _CrtDumpMemoryLeaks �� ���� leak�� ����Ǿ�,
	//		�⺻���� atl�� ����ϵ��� �Ǿ� �ִ�.

	CNTFSHelper::HANDLE_INODE_CACHE MyCreateInodeCache(OPTIONAL IN LPVOID pUserContext)
	{
		return &g_cache;
	}

	BOOL MyGetCache(IN CNTFSHelper::HANDLE_INODE_CACHE hHandle, IN CNTFSHelper::TYPE_INODE nStartInode, IN LPCWSTR lpszNameUpperCase, OUT PBOOL pbFound, OUT CNTFSHelper::LPST_INODEDATA pstInodeData, OPTIONAL IN LPVOID pUserContext)
	{
		static WCHAR szTmp[MAX_PATH * 2] = {0,};
		map<wstring, CNTFSHelper::ST_INODEDATA>::const_iterator mapIter;

		*pbFound = FALSE;

		if (&g_cache != hHandle)
		{
			assert(FALSE);
			return FALSE;
		}

		StringCchPrintf(szTmp, MAX_PATH*2, L"%lld_%s", nStartInode, lpszNameUpperCase);

		mapIter = g_cache.find(szTmp);
		if (mapIter == g_cache.end())
		{
		}
		else	
		{
			*pbFound = TRUE;
			*pstInodeData = mapIter->second;
		}

		return TRUE;
	}

	BOOL MySetCache(IN CNTFSHelper::HANDLE_INODE_CACHE hHandle, IN CNTFSHelper::TYPE_INODE nStartInode, IN LPCWSTR lpszNameUpperCase, IN CNTFSHelper::LPST_INODEDATA pstInodeData, OPTIONAL IN LPVOID pUserContext)
	{
		static WCHAR szTmp[MAX_PATH * 2] = {0,};

		if (&g_cache != hHandle)
		{
			assert(FALSE);
			return FALSE;
		}

		StringCchPrintf(szTmp, MAX_PATH*2, L"%lld_%s", nStartInode, lpszNameUpperCase);

		g_cache.insert(pair<wstring, CNTFSHelper::ST_INODEDATA>(szTmp, *pstInodeData));

		return TRUE;
	}

	VOID MyDestroyCache(IN CNTFSHelper::HANDLE_INODE_CACHE hHandle, OPTIONAL IN LPVOID pUserContext)
	{
	}
#endif	// USING_INODECACHE_ATL_MAP
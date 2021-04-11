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

// inode cache 구현 편의를 위해, map을 사용하는데,
// 몇몇 visual studio에서 stl::map::insert를 사용하면 leak이 보고된다.
// 그것을 위해, visual studio에서 service pack이 제공되는데,
// patch를 적용하지 않은 시스템에서는 leak이 보고될 것이다.
// 그런 경우, atl map을 사용한다.
// 기본으로 atl map을 사용하고,
// 해당 patch가 해결되었다면, 아래 define을 주석 처리하라.
// 그러면, atl 대신 stl을 사용하게 될 것이다.
#define USING_INODECACHE_ATL_MAP

//////////////////////////////////////////////////////////////////////////
// My~Cache 함수군에서 std 사용
#ifdef USING_INODECACHE_ATL_MAP
	// atl 사용
	#include <atlcoll.h>
	#include <atlstr.h>
#else
	// stl 사용
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

// 외부 I/O 함수 정의
static CNTFSHelper::IO_LPVOID __cdecl MyOpenIo(IN LPCSTR lpszPath, OPTIONAL IN LPVOID pUserIoContext);
static BOOL __cdecl MyCloseIo(IN CNTFSHelper::IO_LPVOID pIoHandle);
static BOOL __cdecl MySeekIo(IN CNTFSHelper::IO_LPVOID pIoHandle, IN CNTFSHelper::UBYTE8 nPos, IN INT nOrigin);
static INT __cdecl MyReadIo(IN CNTFSHelper::IO_LPVOID pIoHandle, OUT LPVOID pBuffer, IN unsigned int count);

// Inode cache 함수 정의
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

// 해당 경로끝에는 \를 붙이지 마라.
//
// 기존 Win32 API 형태로 사용하면,
// CNTFSHelper::FindFirstFile에서 Inode를 찾는 과정이 있는데,
// 그 과정에서 I/O가 많이 걸려 부하가 있다.
// 그래서 bFastAccessByInode를 TRUE로 하면, CNTFSHelper의 확장 API를 사용한다.
// 확장 API라면, Inode 정보를 전달하여, Inode를 찾는 과정을 Skip하도록 하는 것이다.
VOID PrintDirectory(IN LPCWSTR lpszPath, IN BOOL bRecurse, IN BOOL bFastAccessByInode, OPTIONAL IN CNTFSHelper::LPST_INODEDATA pstInodeData, OPTIONAL IN CNTFSHelper::LPST_INODE_CACHE pstInodeCache)
{
	PWCHAR						lpszPathAlloc	= NULL;
	INT							nCchLength		= 0;
	INT							nCchRemain		= 0;
	CNTFSHelper::NTFS_HANDLE	hFind			= INVALID_HANDLE_VALUE_NTFS;
	WIN32_FIND_DATA				stData			= {0,};
	BOOL						bDotDirectory	= FALSE;

	// 속도 향상을 위한 추가 Data
	CNTFSHelper::ST_INODEDATA	stInodeData		= {0,};

	// http://msdn.microsoft.com/en-us/library/windows/desktop/aa365247(v=vs.85).aspx
	// 에 따르면 File system에서 최대 길이는 32767 길이이다.
	// 이는,
	// C:\component\component\component\...
	// 의 길이가 32767이 최대값이란 뜻이다.
	// 일단, 이곳에서는 1024 길이만 사용하자.
	// component의 길이는 MAX_PATH의 제한을 갖는다.
	// 원래 32767 길이로 접근하려면, naming a file, 즉, \\?\를 씌워야 하는데,
	// 이곳에서는 해당 부분을 사용하지 않더라도 지원된다.
	lpszPathAlloc = (PWCHAR)malloc(sizeof(WCHAR)*1024);
	if (NULL == lpszPath)
	{
		goto FINAL;
	}
	memset(lpszPathAlloc, 0, sizeof(WCHAR)*1024);

	StringCchCopyW(lpszPathAlloc, 1024, lpszPath);

	if (TEXT('\0') != lpszPathAlloc[3])
	{
		// C:\ 이외의 경우에는 끝에 \를 붙여준다.
		StringCchCatW(lpszPathAlloc, 1024, TEXT("\\"));
	}

	// 길이를 구한다.
	// 즉,
	// C:\abc 가 들어온 경우,
	// C:\abc\
	// 의 문자 길이, 즉, 7을 가진다.
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

	// 끝에 *를 붙인 것으로 전달한다.
	if (FALSE == bFastAccessByInode)
	{
		// Win32 API 방식의 호출
		if (NULL == pstInodeCache)
		{
			// FindFirstFile 내부에 Inode 찾는 과정에 의해,
			// 본 함수의 속도가 느려질 수 있다.
			hFind = CNTFSHelper::FindFirstFile(lpszPathAlloc, &stData);
		}
		else
		{
			// Cache를 사용하는 경우,...
			hFind = CNTFSHelper::FindFirstFileExt(lpszPathAlloc, &stData, NULL, pstInodeCache);
		}
	}
	else
	{
		// Inode를 사용하여 진행하도록 한다.
		if (NULL != pstInodeData)
		{
			hFind = CNTFSHelper::FindFirstFileByInodeExt(lpszPathAlloc, pstInodeData, TRUE, &stInodeData, &stData, NULL, pstInodeCache);
		}
		else
		{
			// Inode를 사용하여 진행해라고 하면서,
			// Inode 정보를 주지 않으면 이전 방식으로 진행한다.
			// (PrintDirectory를 맨 처음 호출할때의 경우임)
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
		// Directory인 경우,...
		// 재귀호출하자.

		if (FILE_ATTRIBUTE_REPARSE_POINT == (stData.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT))
		{
			// 만약 Reparse Point라면,..
			_tprintf(TEXT("[REPARSE]"));
		}
		else
		{
			// Reparse Point가 아니라면, 재귀호출로 진입한다.
			if (TRUE == bRecurse)
			{
				bDotDirectory = FALSE;
				if (L'.' == stData.cFileName[0])
				{
					if ((L'.' == stData.cFileName[1]) && (L'\0' == stData.cFileName[2]))
					{
						// .. 경로
						bDotDirectory = TRUE;
					}
					else if (L'\0' == stData.cFileName[1])
					{
						// . 경로
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
			// 과거 Win32 API 형태의 호출
			if (FALSE == CNTFSHelper::FindNextFile(hFind, &stData))
			{
				break;
			}
		}
		else
		{
			// Inode 정보 요청
			if (FALSE == CNTFSHelper::FindNextFileWithInodeExt(hFind, &stData, &stInodeData))
			{
				break;
			}
		}

		StringCchCopy(&lpszPathAlloc[nCchLength], nCchRemain, stData.cFileName);

		if (FILE_ATTRIBUTE_DIRECTORY == (stData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
		{
			// Directory인 경우,...
			// 재귀호출하자.

			if (FILE_ATTRIBUTE_REPARSE_POINT == (stData.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT))
			{
				// 만약 Reparse Point라면,..
				_tprintf(TEXT("[REPARSE]"));
			}
			else
			{
				// Reparse Point가 아니라면, 재귀호출로 진입한다.
				if (TRUE == bRecurse)
				{
					bDotDirectory = FALSE;
					if (L'.' == stData.cFileName[0])
					{
						if ((L'.' == stData.cFileName[1]) && (L'\0' == stData.cFileName[2]))
						{
							// .. 경로
							bDotDirectory = TRUE;
						}
						else if (L'\0' == stData.cFileName[1])
						{
							// . 경로
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
	// inode cache는 유일한 전역 변수 사용으로 한다.
	// atl 사용
	CAtlMap<CAtlString, CNTFSHelper::ST_INODEDATA> g_cache;
#else
	// inode cache는 유일한 전역 변수 사용으로 한다.
	// stl 사용
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

	// Leak 체크 시작
#if defined(_WIN32) && defined(_DEBUG)
	_CrtSetReportMode(_CRT_WARN, _CRTDBG_MODE_DEBUG);
	_CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_DEBUG);
//	_CrtSetBreakAlloc(250); // Leak 발생시 추적함
#endif

#if defined(_WIN32)
	//////////////////////////////////////////////////////////////////////////
	// console에서 한글 출력
	_wsetlocale(LC_ALL, _T("korean"));
#endif

	//////////////////////////////////////////////////////////////////////////
	// Ntfs를 Open한다.
	pstNtfs = CNTFSHelper::OpenNtfs("\\\\.\\C:");
	if (NULL == pstNtfs)
	{
		ERRORLOG("[ERROR] Fail to OpenNtfs");
		assert(FALSE);
		goto FINAL;
	}

	// 필요한 정보가 있다면 추가한다.
	INFOLOG("[INFO] Ntfs Opened, %s", "\\\\.\\C:");
	INFOLOG("[INFO] Revision : %s", CNTFSHelper::GetRevision(NULL));
	INFOLOG("");
	INFOLOG("[INFO] Ntfs - Total Sector : %llu", pstNtfs->stBpb.llSectorNum);
	INFOLOG("");

	//////////////////////////////////////////////////////////////////////////
	// 1. n번 cluster가 사용중인가?
	srand((unsigned)time(NULL));
	nLCN = rand() % pstNtfs->stNtfsInfo.nTotalClusterCount;

	// 아래 BitmapAlloc 내부를 통해 Ntfs Attribute를 찾고 값을 구하는 과정을 확인할 수 있다.
	// Bitmap은 Resident형태로, RunList를 구하는 과정이 포함되어 있을 것이다.
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
	// 2. Volume 이름을 구해본다. (Example)

	// http://inform.pucp.edu.pe/~inf232/Ntfs/ntfs_doc_v0.5/files/index.html
	// 에 보면 $Volume에 "Serial number, creation time, dirty flag"라 나와 있는데,
	// $Volume에 Volume 이름이 있을 것이다.
	// 그래서, TYPE_INODE_VOLUME으로 선택한다.
	// 위의 URL에 $Volume을 링크하면,
	// http://inform.pucp.edu.pe/~inf232/Ntfs/ntfs_doc_v0.5/files/volume.html
	// 에 들어간다.
	// 해당 Inode에 들어있는 정보가 list되어 있는데,
	// $VOLUME_NAME에 Volume이름이 있을듯 하다.
	// 그래서, TYPE_ATTRIB_VOLUME_NAME를 선택한다.
	pstFindAttrib = CNTFSHelper::FindAttribAlloc(pstNtfs, TYPE_INODE_VOLUME, TYPE_ATTRIB_VOLUME_NAME);
	if (NULL == pstFindAttrib)
	{
		ERRORLOG("[ERROR] Fail to Find Volume, VolumeName");
		assert(FALSE);
		goto FINAL;
	}

	// 주어진 Attribute의 Type은 당연히 Volume 이름으로 지정되어 있어야 한다.
	if (TYPE_ATTRIB_VOLUME_NAME != pstFindAttrib->pstAttrib->dwType)
	{
		ERRORLOG("[ERROR] Invalid Find Attrib");
		assert(FALSE);
		goto FINAL;
	}

	// 이 Attribute는 Resident type으로, Runlist가 없기 때문에,
	// Offset만큼 뒤로 가면 값이 있다.
	// 단, 주어진 길이 만큼이다.
	// 아래 값은 NULL Terminate가 아닌 문자열이다.
	lpszVolumeName = (LPCWSTR)((CNTFSHelper::UBYTE1*)pstFindAttrib->pstAttrib + pstFindAttrib->pstAttrib->stAttr.stResident.wValueOffset);

	// 주어진 만큼 복사!
	CopyMemory(szVolume, lpszVolumeName, pstFindAttrib->pstAttrib->stAttr.stResident.dwValueLength);

	// 출력!
	wprintf(TEXT("[INFO] Volume name is [%s]\r\n"), szVolume);

	// 위와 같은 응용법으로, Ntfs의 다른 기본 정보를 가져올 수 있다.

	//////////////////////////////////////////////////////////////////////////
	// 3. FindFirstFile 계열의 Win32 API를 사용하여, 파일을 열거하자.

	// C:\의 파일을 나열한다.
	// 하부 경로 파일에 들어가지 않는다.
	// 주석 처리하자. 필요하다면 실행해 보아라.
	// printf("\r\n[INFO] C:\\ file list (not recurse)\r\n");
	// PrintDirectory(TEXT("C:\\"), FALSE);

	// Vista 이상에서 일반적으로 탐색이 되지 않는 경로를 시도해 보자.
	// C:\Windows\System32\config 파일을 나열한다. (Registry hive 파일들)
	// 하부 경로 파일에 들어간다.
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

	// Inode Cache를 이용해 본다.
	{
		DWORD dwStart				= 0;
		DWORD dwDurationNormal		= 0;
		DWORD dwDurationFast		= 0;
		LPCWSTR lpszTraversePath	= L"C:\\Windows\\System32\\config";

		// Inode Cache 정보를 채운다.
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

		// 이제 cache를 제거한다.
		#ifdef USING_INODECACHE_ATL_MAP
			g_cache.RemoveAll();
		#else
			g_cache.clear();
		#endif
	}

	//////////////////////////////////////////////////////////////////////////
	// 4. CreateFile 계열의 Win32 API를 이용하자.
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
	// 5. 외부 I/O를 만들어 연결해 보자.
	{
		WIN32_FIND_DATA				stFD			= {0,};
		WCHAR						lpszContext[]	= L"hello, world";
		CNTFSHelper::ST_IO_FUNCTION	stIo			= {0,};
		CNTFSHelper::NTFS_HANDLE	hHandle			= INVALID_HANDLE_VALUE_NTFS;

		printf("\r\n");
		printf("[INFO] Test user define I/O (FindFirstFile C:\\)\r\n");

		// 외부 I/O 함수들을 정의한다.
		stIo.dwCbSize   = sizeof(stIo);
		stIo.pfnOpenIo  = MyOpenIo;
		stIo.pfnReadIo  = MyReadIo;
		stIo.pfnSeekIo  = MySeekIo;
		stIo.pfnCloseIo = MyCloseIo;
		stIo.pUserIoContext = lpszContext;	// MyOpenIo(...)에 전달된다.

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

	// Leak 체크 종료
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
// 외부 정의 I/O 함수

// 외부 정의 I/O에서 사용될 Handle Context
typedef struct tagST_MY_IO
{
	HANDLE	hHandle;
	char	foobar[1024];
	LPCWSTR	lpszContext;
} ST_MY_IO, *LPST_MY_IO;

// 아래의 흐름대로 코드를 작성한다.
CNTFSHelper::IO_LPVOID __cdecl MyOpenIo(IN LPCSTR lpszPath, OPTIONAL IN LPVOID pUserIoContext)
{
	ST_MY_IO	stIo		= {0,};
	LPST_MY_IO	pstRtnValue	= NULL;

	stIo.hHandle = INVALID_HANDLE_VALUE;

	// 실제 I/O를 수행하기전의 초기화 작업을 진행한다.
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
	// 값이 전달된다.
	// 여기에서는 L"hello, world"가 수신될 것이다.
	stIo.lpszContext = (LPCWSTR)pUserIoContext;

	// stIo에 넣을 추가 작업이 있다면, 진행한다.
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
		// 실패
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
		// NULL 체크는 필요없다.
		// caller에서 보장해 준다.
		goto FINAL;
	}

	// Close 작업이 요청되었다.
	// 전처리가 필요한가?
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

	// Close 작업이 요청되었다.
	// 후처리가 필요한가?
	{

	}

	// 여기까지 왔다면 성공
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

	// Seek 작업이 요청되었다.
	// 전처리가 필요한가?
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
	// 값은 서로 같다.
	if (INVALID_SET_FILE_POINTER == ::SetFilePointer(pstIo->hHandle, nLargeInt.LowPart, &nLargeInt.HighPart, nOrigin))
	{
		assert(FALSE);
		goto FINAL;
	}

	// Seek 작업이 요청되었다.
	// 후처리가 필요한가?
	{

	}

	// 여기까지 왔다면 성공
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

	// 읽기 작업이 요청되었다.
	// 전처리가 필요한가?
	{
		wprintf(L"\tMyReadIo called, context=%s, count=%d\r\n", pstIo->lpszContext, count);

		// ...
		// ...
		// ...
	}

	if (FALSE == ::ReadFile(pstIo->hHandle, pBuffer, count, &dwReaded, NULL))
	{
		// 실패
		nRtnValue = -1;
		assert(FALSE);
		goto FINAL;
	}

	nRtnValue = (INT)dwReaded;

	// 읽기 작업이 요청되었다.
	// 후처리가 필요한가?
	{

	}

FINAL:
	return nRtnValue;
}

#ifdef USING_INODECACHE_ATL_MAP

	//////////////////////////////////////////////////////////////////////////
	// atl 사용 코드
	//		atl은 visual studio에서만 제공되는 코드이므로,
	//		만일 visual studio 이외의 compile 환경에서는 아래의 stl 사용 코드를 참고바람

	CNTFSHelper::HANDLE_INODE_CACHE MyCreateInodeCache(OPTIONAL IN LPVOID pUserContext)
	{
		// 전역 변수를 사용한다.
		// 즉, 여러개의 Ntfs 개체와 상관없이,
		// 하나의 공유 cache를 사용한다.
		// 만일 Ntfs 개체 하나당 각각의 cache를 사용하고자 한다면,
		// 이곳에서 새로운 cache를 동적할당 받아 개별 전달해 주도록 해야 한다.

		// 여하튼, 전역 변수의 cache 사용운,
		// 엄밀히 말하자면, 내부적인 CRITICAL_SECTION으로 Lock을 걸어,
		// Thread-safe하게 만들 필요는 있다.
		// 일단, 예제에서는 CRITICAL_SECTION을 사용하지는 않는다.
		return &g_cache;
	}

	BOOL MyGetCache(IN CNTFSHelper::HANDLE_INODE_CACHE hHandle, IN CNTFSHelper::TYPE_INODE nStartInode, IN LPCWSTR lpszNameUpperCase, OUT PBOOL pbFound, OUT CNTFSHelper::LPST_INODEDATA pstInodeData, OPTIONAL IN LPVOID pUserContext)
	{
		static WCHAR szTmp[MAX_PATH * 2] = {0,};

		*pbFound = FALSE;

		if (&g_cache != hHandle)
		{
			// 만일 cache 값이 다른 경우,...
			assert(FALSE);
			return FALSE;
		}

		// StartInode와 name을 하나의 string으로 엮는다.
		StringCchPrintf(szTmp, MAX_PATH*2, L"%lld_%s", nStartInode, lpszNameUpperCase);

		// map에 lookup한다.
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
			// 만일 cache 값이 다른 경우,...
			assert(FALSE);
			return FALSE;
		}

		// StartInode와 name을 하나의 string으로 엮는다.
		StringCchPrintf(szTmp, MAX_PATH*2, L"%lld_%s", nStartInode, lpszNameUpperCase);

		// map에 추가한다.
		g_cache.SetAt(szTmp, *pstInodeData);

		return TRUE;
	}

	VOID MyDestroyCache(IN CNTFSHelper::HANDLE_INODE_CACHE hHandle, OPTIONAL IN LPVOID pUserContext)
	{
		// do nothing...
	}

#else	// USING_INODECACHE_ATL_MAP

	//////////////////////////////////////////////////////////////////////////
	// stl 사용 코드
	//		USING_INODECACHE_ATL_MAP define을 주석처리하면, stl 사용으로 compile된다.
	//		몇몇 Visual studio 버전에서는 _CrtDumpMemoryLeaks 를 통해 leak이 보고되어,
	//		기본으로 atl을 사용하도록 되어 있다.

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
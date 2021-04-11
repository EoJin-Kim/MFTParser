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
#include "NTFSHelper.h"
#include <io.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <share.h>
#include <assert.h>
// [TBD] multi-platform(os) 지원
#include <strsafe.h>

CNTFSHelper::CNTFSHelper(void)
{
}

CNTFSHelper::~CNTFSHelper(void)
{
}

CNTFSHelper::LPST_NTFS CNTFSHelper::OpenNtfs(IN LPCSTR lpszPath)
{
	// 일반적인 내부 I/O를 이용하여 호출한다.
	return OpenNtfsIo(lpszPath, NULL);
}

CNTFSHelper::LPST_NTFS CNTFSHelper::OpenNtfsIo(IN LPCSTR lpszPath, IN LPST_IO_FUNCTION pstIoFunction)
{
	LPST_NTFS	pstRtnValue	= NULL;
	ST_NTFS		stNtfs		= {0,};
	_OFF64_T	nSize		= 0;

	// LocalDisk로 Open한다.
	if (NULL == pstIoFunction)
	{
		// 일상적인, 내부 I/O 함수 사용
		stNtfs.pstIo = OpenIo(lpszPath);
	}
	else
	{
		// 일반적이지 않은, 외부 I/O 함수 사용
		stNtfs.pstIo = OpenUserIo(lpszPath, pstIoFunction);
	}

	if (NULL == stNtfs.pstIo)
	{
		ERRORLOG("Fail to Open");
		assert(FALSE);
		goto FINAL;
	}
	
	// NTFS의 BPB를 구한다.
	if (FALSE == GetBootSectorBPB(&stNtfs, &stNtfs.stBpb))
	{
		ERRORLOG("Fail to Get BPB");
		assert(FALSE);
		goto FINAL;
	}

	//////////////////////////////////////////////////////////////////////////
	// 이제 NTFS INFO를 채운다.

	// cluster size를 결정한다.
	stNtfs.stNtfsInfo.dwClusterSize = stNtfs.stBpb.wBytePerSector * stNtfs.stBpb.cSectorPerCluster;
	stNtfs.stNtfsInfo.wSectorSize   = stNtfs.stBpb.wBytePerSector;

	// MFT의 크기를 구한다.
	// 음수인 경우에는,
	// http://inform.pucp.edu.pe/~inf232/Ntfs/ntfs_doc_v0.5/files/boot.html 의 (c) 참고
	// ntfsprogs 코드 : ntfs_boot_sector_parse(...) 참고

	if ((CHAR)stNtfs.stBpb.cClusterPerMFT < 0)
	{
		stNtfs.stNtfsInfo.dwMftSize = 1 << -stNtfs.stBpb.cClusterPerMFT;
	}
	else
	{
		stNtfs.stNtfsInfo.dwMftSize = stNtfs.stBpb.cClusterPerMFT * stNtfs.stNtfsInfo.dwClusterSize;
	}

	if (0 != stNtfs.stNtfsInfo.dwMftSize % stNtfs.stNtfsInfo.wSectorSize)
	{
		// MFT 크기는 Sector 크기의 배수여야 함
		assert(FALSE);
		goto FINAL;
	}
	stNtfs.stNtfsInfo.dwMftSectorCount = stNtfs.stNtfsInfo.dwMftSize / stNtfs.stNtfsInfo.wSectorSize;

	// MFT를 읽을때 읽어야 하는 Buffer의 크기를 구한다.
	stNtfs.stNtfsInfo.dwReadBufSizeForMft = stNtfs.stNtfsInfo.dwMftSize;

	if (0 != stNtfs.stNtfsInfo.dwMftSize % stNtfs.stNtfsInfo.wSectorSize)
	{
		// Mft 크기가 Sector 크기로 나눠떨어지지 않는다면,...
		assert(FALSE);
		goto FINAL;
	}

	// Mft의 Sector pos를 미리 구한다.
	stNtfs.stNtfsInfo.llMftSectorPos = stNtfs.stBpb.llMftLcn * stNtfs.stBpb.cSectorPerCluster;

	// 크기 계산
	nSize = GetDiskSize(&stNtfs);
	if ((0 != nSize) &&
		(0 == (nSize % stNtfs.stNtfsInfo.dwClusterSize)))
	{
		// 최소 크기가 0 이상이고,
		// Cluster 크기에 비례해야 한다.
		// good
	}
	else
	{
		// bad
		assert(FALSE);
		goto FINAL;
	}

	// 전체 Cluster 개수를 구한다.
	stNtfs.stNtfsInfo.nTotalClusterCount = nSize / stNtfs.stNtfsInfo.dwClusterSize;

	// 여기까지 왔다면 성공
	pstRtnValue = (LPST_NTFS)malloc(sizeof(ST_NTFS));
	if (NULL == pstRtnValue)
	{
		assert(FALSE);
		goto FINAL;
	}
	memset(pstRtnValue, 0, sizeof(ST_NTFS));

	// 복사!
	*pstRtnValue = stNtfs;

FINAL:
	if (NULL == pstRtnValue)
	{
		// 실패했다면,...
		if (NULL != stNtfs.pstIo)
		{
			CloseIo(stNtfs.pstIo);
			stNtfs.pstIo = NULL;
		}
	}
	return pstRtnValue;
}

BOOL CNTFSHelper::SetInodeCache(IN LPST_NTFS pstNtfs, IN LPFN_CreateInodeCache pfnCreateInodeCache, IN LPFN_GetCache pfnGetCache, IN LPFN_SetCache pfnSetCAche, IN LPFN_DestroyCache pfnDestroyCache, OPTIONAL IN LPVOID pUserContext)
{
	BOOL bRtnValue = FALSE;

	if ((NULL == pstNtfs) || (NULL == pfnCreateInodeCache) || (NULL == pfnGetCache) || (NULL == pfnSetCAche) || (NULL == pfnDestroyCache))
	{
		assert(FALSE);
		goto FINAL;
	}

	if (NULL != pstNtfs->hInodeCache)
	{
		// 이미 Cache가 생성되었음
		assert(FALSE);
		goto FINAL;
	}

	if (NULL != pstNtfs->pstInodeCache)
	{
		// callback 정보는 NULL이여야 한다.
		assert(FALSE);
		goto FINAL;
	}

	pstNtfs->pstInodeCache = (LPST_INODE_CACHE)malloc(sizeof(*pstNtfs->pstInodeCache));
	if (NULL == pstNtfs->pstInodeCache)
	{
		assert(FALSE);
		goto FINAL;
	}
	memset(pstNtfs->pstInodeCache, 0, sizeof(*pstNtfs->pstInodeCache));

	pstNtfs->hInodeCache = (*pfnCreateInodeCache)(pUserContext);
	if (NULL == pstNtfs->hInodeCache)
	{
		// cache 생성 실패
		assert(FALSE);
		goto FINAL;
	}

	// 정보를 전달한다.
	pstNtfs->pstInodeCache->pfnCreateInodeCache = pfnCreateInodeCache;
	pstNtfs->pstInodeCache->pfnDestroyCache		= pfnDestroyCache;
	pstNtfs->pstInodeCache->pfnGetCache			= pfnGetCache;
	pstNtfs->pstInodeCache->pfnSetCache			= pfnSetCAche;
	pstNtfs->pstInodeCache->pUserContext		= pUserContext;

	// 여기까지 왔다면 성공
	bRtnValue = TRUE;

FINAL:
	if (FALSE == bRtnValue)
	{
		// 실패
		if (NULL != pstNtfs->pstInodeCache)
		{
			free(pstNtfs->pstInodeCache);
			pstNtfs->pstInodeCache = NULL;
		}
	}
	return bRtnValue;
}

VOID CNTFSHelper::CloseNtfs(IN LPST_NTFS pstNtfs)
{
	if (NULL != pstNtfs)
	{
		if (NULL != pstNtfs->hInodeCache)
		{
			// cache가 남아있는 경우,...
			if ((NULL != pstNtfs->pstInodeCache) && (NULL != pstNtfs->pstInodeCache->pfnDestroyCache))
			{
				(*pstNtfs->pstInodeCache->pfnDestroyCache)(pstNtfs->hInodeCache, pstNtfs->pstInodeCache->pUserContext);
				pstNtfs->hInodeCache = NULL;
			}
		}

		if (NULL != pstNtfs->pstInodeCache)
		{
			free(pstNtfs->pstInodeCache);
			pstNtfs->pstInodeCache = NULL;
		}

		if (NULL != pstNtfs->pstBitmapInfo)
		{
			BitmapFree(pstNtfs->pstBitmapInfo);
			pstNtfs->pstBitmapInfo = NULL;
		}

		// Close한다.
		if (NULL != pstNtfs->pstIo)
		{
			CloseIo(pstNtfs->pstIo);
			pstNtfs->pstIo = NULL;
		}

		free(pstNtfs);
		pstNtfs = NULL;
	}
}

CNTFSHelper::LPST_IO CNTFSHelper::OpenUserIo(IN LPCSTR lpszPath, IN LPST_IO_FUNCTION pstIoFunction)
{
	LPST_IO	pstRtnValue	= NULL;
	ST_IO	stIo		= {0,};

	if ((NULL == pstIoFunction) || (NULL == pstIoFunction->pfnOpenIo) || (NULL == pstIoFunction->pfnCloseIo) || (NULL == pstIoFunction->pfnSeekIo) || (NULL == pstIoFunction->pfnReadIo))
	{
		assert(FALSE);
		goto FINAL;
	}

	if (sizeof(*pstIoFunction) != pstIoFunction->dwCbSize)
	{
		assert(FALSE);
		goto FINAL;
	}

	stIo.nType			= TYPE_IO_USERDEFINE;
	stIo.nFd			= -1;
	stIo.stIoFunction	= *pstIoFunction;

	// 이제 사용자 함수를 호출한다.
	stIo.pUserIoHandle	= (*stIo.stIoFunction.pfnOpenIo)(lpszPath, pstIoFunction->pUserIoContext);
	if (NULL == stIo.pUserIoHandle)
	{
		// 실패를 전달했다.
		assert(FALSE);
		goto FINAL;
	}

	pstRtnValue = (LPST_IO)malloc(sizeof(*pstRtnValue));
	if (NULL == pstRtnValue)
	{
		assert(FALSE);
		goto FINAL;
	}

	// 마무리
	*pstRtnValue = stIo;

FINAL:
	if (NULL == pstRtnValue)
	{
		// 실패
		if (NULL != stIo.pUserIoHandle)
		{
			// Open되었다면 Close한다.
			(*stIo.stIoFunction.pfnCloseIo)(stIo.pUserIoHandle);
			stIo.pUserIoHandle = NULL;
		}
	}
	return pstRtnValue;
}

CNTFSHelper::LPST_IO CNTFSHelper::OpenIo(IN LPCSTR lpszPath)
{
	LPST_IO	pstRtnValue = NULL;
	ST_IO	stIo		= {0,};
	HANDLE	hDisk		= INVALID_HANDLE_VALUE;

	if (NULL == lpszPath)
	{
		assert(FALSE);
		goto FINAL;
	}

	stIo.nType	= TYPE_IO_LOCALDISK;
	stIo.nFd	= -1;

#ifdef _WIN32
	#if _MSC_VER >= 1400
	{
		// _open_osfhandle이 지원되는 Visual Studio 2005 부터...
		// FILE_FLAG_NO_BUFFERING
		// 옵션을 이용하여 Disk를 열고, 해당 handle을 전달한다.
		// hDisk는 CloseHandle할 필요가 없는데,
		// 연결된 fd가 _close되면 CloseHandle된다.
		hDisk = ::CreateFileA(lpszPath, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_FLAG_NO_BUFFERING, NULL);
		if (INVALID_HANDLE_VALUE == hDisk)
		{
			assert(FALSE);
			goto FINAL;
		}
		stIo.nFd = _open_osfhandle((intptr_t)hDisk, 0);
	}
	#else
	{
		stIo.nFd = _open(lpszPath, O_RDONLY | O_BINARY);
	}
	#endif
#elif	// _WIN32
	{
		// Other Platform
		// [TBD]
		// not yet...
		assert(FALSE);
		goto FINAL;
	}
#endif

	if (stIo.nFd <= -1)
	{
		#ifdef _WIN32
			if (INVALID_HANDLE_VALUE != hDisk)
			{
				::CloseHandle(hDisk);
				hDisk = INVALID_HANDLE_VALUE;
			}
		#else
			assert(FALSE);
			goto FINAL;
		#endif
	}

	pstRtnValue = (LPST_IO)malloc(sizeof(*pstRtnValue));
	if (NULL == pstRtnValue)
	{
		assert(FALSE);
		goto FINAL;
	}
	memset(pstRtnValue, 0, sizeof(*pstRtnValue));

	*pstRtnValue = stIo;

FINAL:
	if (NULL == pstRtnValue)
	{
		// 실패...
		if (stIo.nFd <= -1)
		{
			// _open되지 못함
			if (INVALID_HANDLE_VALUE != hDisk)
			{
				::CloseHandle(hDisk);
				hDisk = INVALID_HANDLE_VALUE;
			}
		}
		else
		{
			// hDisk도 함께 자동으로 ::CloseHandle()된다.
			_close(stIo.nFd);
			stIo.nFd = -1;
		}
	}
	return pstRtnValue;
}

BOOL CNTFSHelper::CloseIo(IN LPST_IO pstIo)
{
	BOOL bRtnValue = FALSE;

	if (NULL == pstIo)
	{
		assert(FALSE);
		goto FINAL;
	}

	if (TYPE_IO_LOCALDISK == pstIo->nType)
	{
		if (pstIo->nFd <= -1)
		{
			// 열려있지 않음...
			assert(FALSE);
		}
		else
		{
			_close(pstIo->nFd);
			pstIo->nFd = -1;
		}
	}
	else if (TYPE_IO_USERDEFINE == pstIo->nType)
	{
		if (NULL == pstIo->pUserIoHandle)
		{
			// 열려있지 않음...
			assert(FALSE);
		}
		else if (NULL != pstIo->stIoFunction.pfnCloseIo)
		{
			// User Define Close 함수를 호출한다.
			bRtnValue = (*pstIo->stIoFunction.pfnCloseIo)(pstIo->pUserIoHandle);
			pstIo->pUserIoHandle = NULL;
		}
	}
	else
	{
		// 지원되지 않음
		assert(FALSE);
		goto FINAL;
	}

	free(pstIo);
	pstIo = NULL;
	bRtnValue = TRUE;

FINAL:
	return bRtnValue;
}

INT CNTFSHelper::ReadIo(IN LPST_IO pstIo, OUT LPVOID pBuffer, IN unsigned int count)
{
	INT nRtnValue = -1;

	if ((NULL == pstIo) || (NULL == pBuffer) || (0 == count))
	{
		assert(FALSE);
		goto FINAL;
	}

	if (TYPE_IO_LOCALDISK == pstIo->nType)
	{
		nRtnValue = _read(pstIo->nFd, pBuffer, count);
	}
	else if (TYPE_IO_USERDEFINE == pstIo->nType)
	{
		// User Define Close 함수를 호출한다.
		nRtnValue = (*pstIo->stIoFunction.pfnReadIo)(pstIo->pUserIoHandle, pBuffer, count);
	}
	else
	{
		assert(FALSE);
		goto FINAL;
	}

FINAL:
	return nRtnValue;
}

BOOL CNTFSHelper::SeekIo(IN LPST_IO pstIo, IN UBYTE8 nPos, IN INT nOrigin/*SEEK_SET,SEEK_CUR,SEEK_END*/)
{
	BOOL bRtnValue = FALSE;

	if (NULL == pstIo)
	{
		assert(FALSE);
		goto FINAL;
	}

	if (TYPE_IO_LOCALDISK == pstIo->nType)
	{
		if (_lseeki64(pstIo->nFd, nPos, nOrigin) < 0)
		{
			assert(FALSE);
			goto FINAL;
		}
	}
	else if (TYPE_IO_USERDEFINE == pstIo->nType)
	{
		// User Define Close 함수를 호출한다.
		bRtnValue = (*pstIo->stIoFunction.pfnSeekIo)(pstIo->pUserIoHandle, nPos, nOrigin);
	}
	else
	{
		assert(FALSE);
		goto FINAL;
	}

	// 여기까지 왔다면 성공
	bRtnValue = TRUE;

FINAL:
	return bRtnValue;
}

BOOL CNTFSHelper::SeekLcn(IN LPST_NTFS pstNtfs, IN UBYTE8 llLcn)
{
	if (NULL == pstNtfs)
	{
		return FALSE;
	}

	if (FALSE == SeekIo(pstNtfs->pstIo, llLcn * pstNtfs->stBpb.wBytePerSector * pstNtfs->stBpb.cSectorPerCluster, SEEK_SET))
	{
		assert(FALSE);
		return FALSE;
	}

	return TRUE;
}

BOOL CNTFSHelper::SeekSector(IN LPST_NTFS pstNtfs, IN UBYTE8 llSectorNum)
{
	if (NULL == pstNtfs)
	{
		return FALSE;
	}

	if (FALSE == SeekIo(pstNtfs->pstIo, llSectorNum * pstNtfs->stBpb.wBytePerSector, SEEK_SET))
	{
		assert(FALSE);
		return FALSE;
	}

	return TRUE;
}

BOOL CNTFSHelper::SeekByte(IN LPST_NTFS pstNtfs, IN UBYTE8 nByte)
{
	if (NULL == pstNtfs)
	{
		return FALSE;
	}

	if (0 == (nByte % pstNtfs->stNtfsInfo.wSectorSize))
	{
		// nByte는 sector 크기의 배수여야 한다.
		// good
	}
	else
	{
		// bad
		assert(FALSE);
		return FALSE;
	}

	if (FALSE == SeekIo(pstNtfs->pstIo, nByte, SEEK_SET))
	{
		assert(FALSE);
		return FALSE;
	}

	return TRUE;
}

BOOL CNTFSHelper::GetBootSectorBPB(IN LPST_NTFS pstNtfs, OUT LPST_NTFS_BPB pstBpb)
{
	BOOL			bRtnValue		= FALSE;
	ST_BOOT_SECTOR	stBootSector	= {0,};
	_OFF64_T		nReaded			= 0;

	if ((NULL == pstNtfs) || (NULL == pstBpb))
	{
		bRtnValue = FALSE;
		goto FINAL;
	}

	if (FALSE == SeekSector(pstNtfs, 0))
	{
		bRtnValue = FALSE;
		goto FINAL;
	}

	// BPB를 읽을 때에는 기본 Sector 크기로 가져온다.
	if (sizeof(stBootSector) != ReadIo(pstNtfs->pstIo, (UBYTE1*)&stBootSector, sizeof(stBootSector)))
	{
		bRtnValue = FALSE;
		goto FINAL;
	}

	// OEMID는 NTFS로 시작해야 한다.
	if (0 != strncmp((char*)stBootSector.szOemId, "NTFS\x20\x20\x20\x20", 8))
	{
		bRtnValue = FALSE;
		ERRORLOG("Invalid oemid");
		goto FINAL;
	}

	// 0xaa55로 끝나야 한다.
	if (0xaa55 != stBootSector.wMagic)
	{
		bRtnValue = FALSE;
		ERRORLOG("Invalid magic code, 0x%x", stBootSector.wMagic);
		goto FINAL;
	}

	*pstBpb = stBootSector.stBpb;
	bRtnValue = TRUE;

FINAL:
	return bRtnValue;
}

// 8byte를 받아, 뒷편 n byte 값(1~8)을 전달한다.
//     0x1234567890abcdef이 들어가는 경우 nByte 값에 따른 리턴값은 다음과 같다.
// 1 :               0xef
// 2 :             0xcdef
// 3 :           0xabcdef
// 4 :         0x90abcdef
// 5 :       0x7890abcdef
// 6 :     0x567890abcdef
// 7 :   0x34567890abcdef
// 8 : 0x1234567890abcdef
inline CNTFSHelper::UBYTE8 CNTFSHelper::GetBytes(IN UBYTE8 n8Byte, IN INT nByte)
{
	if ((1 <= nByte) && (nByte <= 8))
	{
		// byte가 1~8 사이이면 ok
	}
	else
	{
		// 부적합
		assert(FALSE);
		return 0;
	}

	return (n8Byte & (0xffffffffffffffff >> (8 * (8 - nByte))));

	/*
	Big-Endian인 경우, 아래가 더 편할지 모르겠다. 테스트가 필요하다.
	// 8byte를 받아, 앞선 n byte 값(1~8)을 전달한다.
	//     0x1234567890abcdef이 들어가는 경우 nByte 값에 따른 리턴값은 다음과 같다.
	// 1 : 0x12
	// 2 : 0x1234
	// 3 : 0x123456
	// 4 : 0x12345678
	// 5 : 0x1234567890
	// 6 : 0x1234567890ab
	// 7 : 0x1234567890abcd
	// 8 : 0x1234567890abcdef
	return (n8Byte & ~(0xffffffffffffffff >> 8 * nByte)) >> (8 * (8 - nByte));
	*/
}

// Byte stream을 받아 UBYTE8을 전달한다. Little Endian으로 변환한다.
inline CNTFSHelper::UBYTE8 CNTFSHelper::GetUBYTE8(IN UBYTE1* pBuf)
{
	if (NULL == pBuf)
	{
		return 0;
	}

	return *(UBYTE8*)pBuf;

	// Big-Endian인 경우 다음 과정이 필요한지 모르겠다.
	// 테스트가 필요하다.
	/*
	ST_BYTE8 stNode = {0,};

	stNode = *(LPST_BYTE8)pBuf;

	stNode.val[0] = pBuf[7];
	stNode.val[1] = pBuf[6];
	stNode.val[2] = pBuf[5];
	stNode.val[3] = pBuf[4];
	stNode.val[4] = pBuf[3];
	stNode.val[5] = pBuf[2];
	stNode.val[6] = pBuf[1];
	stNode.val[7] = pBuf[0];
	return *(UBYTE8*)&stNode;
	*/
}

CNTFSHelper::UBYTE1* CNTFSHelper::GetRunListByte(IN LPST_ATTRIB_RECORD pstAttrib)
{
	UBYTE1* pRtnValue = NULL;

	if (NULL == pstAttrib)
	{
		pRtnValue = NULL;
		assert(FALSE);
		goto FINAL;
	}

	if (0 == pstAttrib->cNonResident)
	{
		// runlist가 없다.
		pRtnValue = NULL;
		goto FINAL;
	}

	pRtnValue = (UBYTE1*)pstAttrib + pstAttrib->stAttr.stNonResident.wOffsetDataRun;

FINAL:
	return pRtnValue;
}

// http://ftp.kolibrios.org/users/Asper/docs/NTFS/ntfsdoc.html#id4795876를 참고한다.
CNTFSHelper::LPST_RUNLIST CNTFSHelper::GetRunListAlloc(IN UBYTE1* pRunList, OUT PINT pnCount)
{
	LPST_RUNLIST	pstRtnValue		= NULL;
	INT				i				= 0;
	INT				nCount			= 0;
	UBYTE1			cHeader			= 0;
	UBYTE1			cHeader_Legnth	= 0;
	UBYTE1			cHeader_Offset	= 0;
	UBYTE1*			pRunListIndex	= NULL;
	LPST_RUNLIST	pstRunList		= NULL;

	if ((NULL == pRunList) || (NULL == pnCount))
	{
		pstRtnValue = NULL;
		goto FINAL;
	}

	if (NULL != pnCount)
	{
		*pnCount = 0;
	}

	// 우선 RunList의 개수를 구하자.
	pRunListIndex = pRunList;
	for (;;nCount++)
	{
		cHeader = pRunListIndex[0];
		if (0x0 == cHeader)
		{
			break;
		}

		cHeader_Legnth = (cHeader & 0x0F);
		cHeader_Offset = (cHeader & 0xF0) >> 4;

		if (0 == cHeader_Legnth)
		{
			pstRtnValue = NULL;
			goto FINAL;
		}

		// 다음번으로 이동 !!!
		pRunListIndex += (cHeader_Legnth + cHeader_Offset + 1);
	}

	if (0 == nCount)
	{
		pstRtnValue = NULL;
		goto FINAL;
	}

	pstRtnValue = (LPST_RUNLIST)malloc(sizeof(ST_RUNLIST)*nCount);
	if (NULL == pstRtnValue)
	{
		pstRtnValue = NULL;
		goto FINAL;
	}
	memset(pstRtnValue, 0, sizeof(ST_RUNLIST)*nCount);

	// 이제 할당하자.
	pRunListIndex = pRunList;
	for (i=0; i<nCount; i++)
	{
		cHeader = pRunListIndex[0];

		cHeader_Legnth = (cHeader & 0x0F);
		cHeader_Offset = (cHeader & 0xF0) >> 4;

		if (0 == cHeader_Offset)
		{
			// Sparse 파일 형태
			pstRtnValue[i].cType	= TYPE_RUNTYPE_SPARSE;
			pstRtnValue[i].llLength = GetBytes(GetUBYTE8(pRunListIndex+1), cHeader_Legnth);
			pstRtnValue[i].llLCN	= 0;
		}
		else
		{
			// 일반 파일 형태
			pstRtnValue[i].cType	= TYPE_RUNTYPE_NORMAL;
			pstRtnValue[i].llLength = GetBytes(GetUBYTE8(pRunListIndex+1), cHeader_Legnth);
			pstRtnValue[i].llLCN	= GetBytes(GetUBYTE8(pRunListIndex+1+cHeader_Legnth), cHeader_Offset);
		}

		if (i > 0)
		{
			pstRtnValue[i].llLCN = pstRtnValue[i-1].llLCN + GetOffsetSBYTE8(pstRtnValue[i].llLCN, cHeader_Offset);
		}

		// 다음번으로 이동 !!!
		pRunListIndex += (cHeader_Legnth + cHeader_Offset + 1);
	}

FINAL:
	if ((NULL != pnCount) && (NULL != pstRtnValue))
	{
		*pnCount = nCount;
	}
	return pstRtnValue;
}

// 8byte라 함은,
// 0x1234567812345678
// 숫자를 의미한다.
// 0x0000000000ff2222
// 는 8byte 숫자(__int64/int64_t)에서 +를 의미하는데,
// 본 함수는 0x80~0xff로 시작하면, -로 인식하도록 한다.
// 즉, 0xff2222를 -로 인식하여 해당 음수값을 8byte값으로 전달한다.
//
// 자세한것은,
// http://homepage.cs.uri.edu/~thenry/csc487/video/66_NTFS_Data_Runs.pdf
// http://homepage.cs.uri.edu/~thenry/csc487/video/66_NTFS_Data_Runs.mov @ 08:45
// 를 참고 바람
//
// 다시 설명하자면,
// 0x7999 ==> 0x7999 를 리턴하고,
// 0x8001 ==> 0xffffffffffff8001 을 리턴하도록 한다. (첫 바이트가 0x80 ~ 0xff로 시작하기 때문에)
//           (실제 메모리는 0x8001ffffffffffff 이다)
// [TBD] Other cpu platform (big endian) 처리 이슈
CNTFSHelper::SBYTE8 CNTFSHelper::GetOffsetSBYTE8(IN UBYTE8 n8Byte, IN INT nOffset)
{
	UBYTE1* pByte	  = NULL;
	UBYTE8  nRtnValue = 0;

	// 주어진 값으로 복사한다.
	nRtnValue = n8Byte;

	// 첫 바이트를 읽는다.
	pByte = (UBYTE1*)&n8Byte;

	if ((1 <= nOffset) && (nOffset <= 8))
	{
		// good
	}
	else
	{
		// 오류 상황
		assert(FALSE);
		return nRtnValue;
	}

	if (pByte[nOffset-1] & 0x80)
	{
		// 만일 0x80 bit가 포함되었다면,...
		nRtnValue = -1;
		memcpy(&nRtnValue, pByte, nOffset);
	}

	return nRtnValue;
}

VOID CNTFSHelper::GetRunListFree(IN LPST_RUNLIST pRunList)
{
	if (NULL != pRunList)
	{
		free(pRunList);
		pRunList = NULL;
	}
}

CNTFSHelper::LPST_ATTRIB_FILE_NAME CNTFSHelper::GetAttrib_FileName(IN LPST_ATTRIB_RECORD pstAttrib)
{
	LPST_ATTRIB_FILE_NAME pstFileName = NULL;

	if (NULL == pstAttrib)
	{
		pstFileName = NULL;
		goto FINAL;
	}

	if ((TYPE_ATTRIB_FILE_NAME  == pstAttrib->dwType) &&
		(0						== pstAttrib->cNonResident))
	{
		// File Name Type이여야 하고, 
		// non-resident 이여야 한다.
		// good
	}
	else
	{
		// bad
		pstFileName = NULL;
		assert(FALSE);
		goto FINAL;
	}

	if (0 != pstAttrib->wNameOffset)
	{
		pstFileName = (LPST_ATTRIB_FILE_NAME)((UBYTE1*)pstAttrib + pstAttrib->wNameOffset);
	}
	else
	{
		pstFileName = (LPST_ATTRIB_FILE_NAME)((UBYTE1*)pstAttrib + pstAttrib->stAttr.stResident.wValueOffset);
	}

FINAL:
	return pstFileName;
}

CNTFSHelper::ST_FILENAME CNTFSHelper::GetFileName(IN LPST_ATTRIB_FILE_NAME pstFileName)
{
	ST_FILENAME stFileName = {0,};

	if ((NULL == pstFileName) || (NULL == pstFileName->lpszFileNameW) || (MAX_PATH <= pstFileName->cFileNameLength))
	{
		goto FINAL;
	}

	StringCchCopyN(stFileName.szFileName, MAX_PATH, pstFileName->lpszFileNameW, pstFileName->cFileNameLength);

FINAL:
	return stFileName;
}

LPCSTR CNTFSHelper::GetRevision(OPTIONAL OUT LPINT pnRevision)
{
	static CHAR szVersion[32] = {0,};

	if (NULL != pnRevision)
	{
		*pnRevision = NTFS_PARSER_REVISION;
	}

	StringCchPrintfA(szVersion, 32, "ntfs.parser.revision.%.4d.beta", NTFS_PARSER_REVISION);
	return szVersion;
}

BOOL CNTFSHelper::IsValidMftHeader(IN LPST_NTFS pstNtfs, IN UBYTE1* pReadBuf, IN LPST_MFT_HEADER pstMftHeader)
{
	BOOL				bRtnValue = FALSE;
	BOOL				bFound	  = FALSE;
	LPST_ATTRIB_RECORD	pstAttrib = NULL;

	if ((NULL == pstNtfs) || (NULL == pstMftHeader) || (NULL == pReadBuf))
	{
		assert(FALSE);
		goto FINAL;
	}

	// Signature 체크
	if (0 != strncmp("FILE", (char*)pstMftHeader->szFileSignature, 4))
	{
		assert(FALSE);
		goto FINAL;
	}

	// Alloc size 체크
	if (pstMftHeader->dwAllocatedSize != pstNtfs->stNtfsInfo.dwMftSize)
	{
		assert(FALSE);
		goto FINAL;
	}

	// http://inform.pucp.edu.pe/~inf232/Ntfs/ntfs_doc_v0.5/concepts/file_record.html
	// 에 따르면,
	// FILE Record는,
	//		Header
	//		Attribute
	//		Attribute
	//		...
	//		End Marker (0xFFFFFFFF)
	// 로 되어 있음. 0xFFFFFFFF를 체크한다.

	pstAttrib = (LPST_ATTRIB_RECORD)((UBYTE1*)pstMftHeader + pstMftHeader->wAttribOffset);
	for (;;)
	{
		if (0xFFFFFFFF == pstAttrib->dwType)
		{
			// End Marker로 되어 있음
			bFound = TRUE;
			break;
		}

		if (0 == pstAttrib->dwLength)
		{
			// End Marker없이 종료됨
			assert(FALSE);
			break;
		}

		// 계산된 현재 위치의 buffer pointer가
		// MFT Header에 정의된 used size보다 넘어갈 때,...
		if ((UBYTE1*)pstAttrib >= (UBYTE1*)pReadBuf + pstNtfs->stNtfsInfo.dwReadBufSizeForMft)
		{
			// End Marker없이 범위를 벗어남
			assert(FALSE);
			break;
		}

		// 다음 Attribute로 간다.
		pstAttrib = (LPST_ATTRIB_RECORD)((UBYTE1*)pstAttrib + pstAttrib->dwLength);
	}

	if (TRUE == bFound)
	{
		// End Marker가 있어 Valid 함.
		bRtnValue = TRUE;
	}
	else
	{
		// Valid하지 못함
		bRtnValue = FALSE;
	}

FINAL:
	return bRtnValue;
}

CNTFSHelper::LPST_ATTRIB_RECORD CNTFSHelper::GetAttrib(IN LPST_NTFS pstNtfs, IN LPST_MFT_HEADER pstMftHeader, IN UBYTE1* pReadBuf, IN TYPE_ATTRIB nType, OUT PBOOL pbAttribList, OUT TYPE_INODE* pnInodeAttribList)
{
	LPST_ATTRIB_RECORD	pstRtnValue			= NULL;

	// http://inform.pucp.edu.pe/~inf232/Ntfs/ntfs_doc_v0.5/attributes/index.html
	// http://inform.pucp.edu.pe/~inf232/Ntfs/ntfs_doc_v0.5/attributes/attribute_list.html
	// 에 따르면, MFT record 공간이 부족한 경우, $ATTRIBUTE_LIST가 활용될 수 있다.
	LPST_ATTRIB_RECORD	pstAttribList		= NULL;
	LPST_RUNLIST		pstRunList			= NULL;
	INT					nCountRunList		= 0;
	LPST_ATTRIBUTE_LIST pstAttribListNode	= NULL;
	UBYTE4				nOffset				= 0;
	BOOL				bFound				= FALSE;
	UBYTE1*				pBufAttrList		= NULL;
	INT					i					= 0;
	INT					nBufPos				= 0;
	UBYTE4				nAttribListSize		= 0;

	if (NULL != pbAttribList)
	{
		*pbAttribList = FALSE;
	}

	if (NULL != pnInodeAttribList)
	{
		*pnInodeAttribList = 0;
	}

	if ((NULL == pstNtfs) || (NULL == pReadBuf) || (NULL == pstMftHeader) || (NULL == pbAttribList) || (NULL == pnInodeAttribList))
	{
		pstRtnValue = NULL;
		goto FINAL;
	}

	pstRtnValue = (LPST_ATTRIB_RECORD)((UBYTE1*)pstMftHeader + pstMftHeader->wAttribOffset);
	for (;;)
	{
		// 종료 조건 0
		if (0xFFFFFFFF == pstRtnValue->dwType)
		{
			// End Marker로 되어 있음
			break;
		}

		// 종료 조건 1
		// MFT Header에 Length가 0이면 종료
		if (0 == pstRtnValue->dwLength)
		{
			break;
		}

		// 종료 조건 2
		// 계산된 현재 위치의 buffer pointer가
		// MFT Header에 정의된 used size보다 넘어갈 때,...
		if ((UBYTE1*)pstRtnValue >= (UBYTE1*)pReadBuf + pstNtfs->stNtfsInfo.dwReadBufSizeForMft)
		{
			break;
		}

		if (nType == pstRtnValue->dwType)
		{
			// 찾았다
			goto FINAL;
		}

		if (TYPE_ATTRIB_ATTRIBUTE_LIST == pstRtnValue->dwType)
		{
			// $ATTRIBUTE_LIST가 지정되었다.
			if (NULL != pstAttribList)
			{
				// 만일 하나의 Inode(Record)에 두개의 $ATTRIBUTE_LIST가 존재 가능한가?
				pstRtnValue = NULL;
				assert(FALSE);
				goto FINAL;
			}

			pstAttribList = pstRtnValue;
		}

		// 다음 Attribute로 간다.
		pstRtnValue = (LPST_ATTRIB_RECORD)((UBYTE1*)pstRtnValue + pstRtnValue->dwLength);
	}

	// 찾지 못했다.
	if (NULL == pstAttribList)
	{
		// 찾지 못했는데에다가, $ATTRIBUTE_LIST도 없었다.
		// 그럼 없는 것이다.
		pstRtnValue = NULL;
		goto FINAL;
	}

	// $ATTRIBUTE_LIST에서 찾아본다.
	//		http://www.alex-ionescu.com/NTFS.pdf 4.4 $ATTRIBUTE_LIST
	//		http://inform.pucp.edu.pe/~inf232/Ntfs/ntfs_doc_v0.5/attributes/attribute_list.html
	// 를 참고한다.

	// 일단 찾지 못한걸로 기본값 설정한다.
	pstRtnValue = NULL;

	// 찾지 못했지만, $ATTRIBUTE_LIST가 있어, 해당 부분에서 찾아본다.
	// $ATTRIBUTE_LIST는 흔하지 않은 속성이다.
	if (1 == pstAttribList->cNonResident)
	{
		// non-resident ==> RunList를 구한다.
		pstRunList = GetRunListAlloc(GetRunListByte(pstAttribList), &nCountRunList);
		if ((NULL == pstRunList) || (0 == nCountRunList))
		{
			assert(FALSE);
			goto FINAL;
		}

		if (pstAttribList->stAttr.stNonResident.llAllocSize > MAXINT32)
		{
			// INT 값 range가 벗어나는 경우는 없을 것이다.
			assert(FALSE);
			goto FINAL;
		}

		pBufAttrList = (UBYTE1*)malloc((size_t)pstAttribList->stAttr.stNonResident.llAllocSize);
		if (NULL == pBufAttrList)
		{
			assert(FALSE);
			goto FINAL;
		}
		memset(pBufAttrList, 0, (size_t)pstAttribList->stAttr.stNonResident.llAllocSize);

		// [TBD]
		// C:\Windows\winsxs\x86_wiabr00a.inf_31bf3856ad364e35_6.1.7600.16385_none_c3d5d0f14aac8dfa\Brmf3wia.dll
		// 경로의 inode에 non-resident 형태의 $ATTRIBUTE_LIST가 포함되었다.
		// 일단, Runlist의 개수가 1이였는데,
		// 2이상인 경우는 아직 확인하지 못했다.
		// 우선, 예상컨데, 2이상인 경우에,
		// RunList에 해당되는 data를 연속해서 읽어서,
		// 처리하도록 하자.

		// RunList를 한꺼번에 읽는다.
		// $ATTRIBUTE_LIST는 그 크기가 크지 않을 것이다.
		for (i=0; i<nCountRunList; i++)
		{
			if (FALSE == SeekLcn(pstNtfs, pstRunList[i].llLCN))
			{
				assert(FALSE);
				goto FINAL;
			}

			if (pstRunList[i].llLength * pstNtfs->stNtfsInfo.dwClusterSize + nBufPos > pstAttribList->stAttr.stNonResident.llAllocSize)
			{
				// buffer overrun
				// stNonResident 정보의 Alloc 크기가,
				// RunList 크기를 벗어나고 있음
				assert(FALSE);
				goto FINAL;
			}

			if ((size_t)(pstRunList[i].llLength * pstNtfs->stNtfsInfo.dwClusterSize) != (size_t)ReadIo(pstNtfs->pstIo, &pBufAttrList[nBufPos], (size_t)(pstRunList[i].llLength * pstNtfs->stNtfsInfo.dwClusterSize)))
			{
				assert(FALSE);
				goto FINAL;
			}

			nBufPos += (INT)(pstRunList[i].llLength * pstNtfs->stNtfsInfo.dwClusterSize);
		}

		// RunList에 의한 Data 버퍼 위치에 ATTRIBUTE_LIST 값이 저장되어 있다.
		pstAttribListNode = (LPST_ATTRIBUTE_LIST)pBufAttrList;
		nAttribListSize   = (UBYTE4)pstAttribList->stAttr.stNonResident.llDataSize;
	}
	else
	{
		// resident 위치에 ATTRIBUTE_LIST 값이 저장되어 있다.
		pstAttribListNode = (LPST_ATTRIBUTE_LIST)((UBYTE1*)pstAttribList + pstAttribList->stAttr.stResident.wValueOffset);
		nAttribListSize   = pstAttribList->stAttr.stResident.dwValueLength;
	}

	// $ATTRIBUTE_LIST 내부를 조사한다.
	for (;;)
	{
		if (nOffset >= nAttribListSize)
		{
			break;
		}

		if (pstMftHeader->dwMftRecordNumber == (pstAttribListNode->nReference & 0x0000FFFFFFFFFFFFUL))
		{
			// 해당 inode에 이미 포함되어 있는것.
			// 예를 들어,
			// inode 5번, INODE_DOT에,
			//		TYPE_ATTRIB_STANDARD_INFORMATION
			//		TYPE_ATTRIB_ATTRIBUTE_LIST
			//		TYPE_ATTRIB_FILE_NAME
			// 가 있다고 할때,
			// TYPE_ATTRIB_ATTRIBUTE_LIST 내부에,
			//		TYPE_ATTRIB_STANDARD_INFORMATION	<--
			//		TYPE_ATTRIB_FILE_NAME				<--
			//		TYPE_ATTRIB_INDEX_ROOT				<==
			//		TYPE_ATTRIB_INDEX_ALLOCATION		<==
			// 가 있다고 하자.
			// <-- 표시한 것인, 해당 INODE에 포함되어 있기 때문에,
			// 앞에서 처리했을 것이다.
			// 그러니 Skip할 수 있다.
			// 따라서, 여기 loop에서는,
			// <== 부분을 처리하도록 한다.

			// skip 처리를 위해,
			// do nothing...
		}
		else
		{
			// 실질적인 처리 루틴
			if (pstAttribListNode->nType == nType)
			{
				// found!!!
				bFound = TRUE;
				break;
			}
		}

		// List의 다음 Node로 이동한다.
		nOffset += pstAttribListNode->nRecordLength;
		pstAttribListNode = (LPST_ATTRIBUTE_LIST)((UBYTE1*)pstAttribListNode + pstAttribListNode->nRecordLength);
	}

	if (TRUE == bFound)
	{
		*pbAttribList = TRUE;
		*pnInodeAttribList = pstAttribListNode->nReference & 0x0000FFFFFFFFFFFFUL;
	}

FINAL:

	if (NULL != pBufAttrList)
	{
		free(pBufAttrList);
		pBufAttrList = NULL;
	}

	if (NULL != pstRunList)
	{
		GetRunListFree(pstRunList);
		pstRunList = NULL;
	}
	return pstRtnValue;
}

BOOL CNTFSHelper::RunFixup(IN LPST_NTFS pstNtfs, IN LPST_MFT_HEADER pstMftHeader, IN OUT UBYTE1* pBuf)
{
	BOOL	bRtnValue	= FALSE;
	UBYTE4	i			= 0;
	UBYTE1*	pBufFix		= NULL;
	UBYTE1* pFix		= NULL;
	UBYTE1* pArray		= NULL;
	INT		nIndex		= 0;

	if ((NULL == pstNtfs) || (NULL == pstMftHeader) || (NULL == pBuf))
	{
		bRtnValue = FALSE;
		assert(FALSE);
		goto FINAL;
	}

	// Fix값을 구한다.
	pFix = pBuf + pstMftHeader->wFixupArrayOffset;

	// Buf 크기를 Sector 크기만큼 loop 돈다.
	// 그래서 Fixup에 대한 Valid 체크를 한다.
	for (i=1; i<=pstNtfs->stNtfsInfo.dwReadBufSizeForMft / pstNtfs->stNtfsInfo.wSectorSize; i++)
	{
		// 섹터 끝 마지막 2바이트가 fix 대상이다.
		pBufFix = &pBuf[i*pstNtfs->stNtfsInfo.wSectorSize-2];

		if ((pFix[0] == pBufFix[0]) &&
			(pFix[1] == pBufFix[1]))
		{
			// Fix값이 같아야 한다.
			// http://inform.pucp.edu.pe/~inf232/Ntfs/ntfs_doc_v0.5/concepts/fixup.html
			// 의 Here the Update Sequence Number is 0xABCD and the Update Sequence Array is still empty.
			// 를 참고바람.
			// good
		}
		else
		{
			// bad
			bRtnValue = FALSE;
			assert(FALSE);
			goto FINAL;
		}
	}

	// Valid 체크가 끝났으니,
	// 이제 Fix하여 버퍼를 수정하자.
	pArray = pBuf + pstMftHeader->wFixupArrayOffset + 2;
	for (i=1; i<=pstNtfs->stNtfsInfo.dwReadBufSizeForMft / pstNtfs->stNtfsInfo.wSectorSize; i++)
	{
		// 섹터 끝 마지막 2바이트가 fix 대상이다.
		pBufFix = &pBuf[i*pstNtfs->stNtfsInfo.wSectorSize-2];

		pBufFix[0] = pArray[nIndex];
		nIndex++;
		pBufFix[1] = pArray[nIndex];
		nIndex++;
	}

	bRtnValue = TRUE;

FINAL:
	return bRtnValue;
}

CNTFSHelper::LPST_FIND_ATTRIB CNTFSHelper::FindAttribAlloc(IN LPST_NTFS pstNtfs, IN TYPE_INODE nTypeInode, IN TYPE_ATTRIB nTypeAttrib)
{
	LPST_FIND_ATTRIB	pstRtnValue				= NULL;
	ST_FIND_ATTRIB		stFindAttrib			= {0,};
	UBYTE8				llMftSectorPos			= 0;
	_OFF64_T			nReaded					= 0;
	INT					nMftCountPerReadBuffer	= 0;
	UBYTE4				nMftHeaderPos			= 0;
	LPST_ATTRIB_RECORD	pstAttribFileName		= NULL;
	BOOL				bNormalFile				= FALSE;
	UBYTE8				nLcnNormalFile			= 0;
	INT					nOffsetByteNormalFile	= 0;
	BOOL				bAttribList				= FALSE;
	TYPE_INODE			nInodeAttribList		= 0;

	if (NULL == pstNtfs)
	{
		pstRtnValue = NULL;
		assert(FALSE);
		goto FINAL;
	}

	// MFT Header를 읽을 버퍼를 구한다.
	stFindAttrib.pBuf = (UBYTE1*)malloc(pstNtfs->stNtfsInfo.dwReadBufSizeForMft);
	if (NULL == stFindAttrib.pBuf)
	{
		assert(FALSE);
		goto FINAL;
	}
	memset(stFindAttrib.pBuf, 0, pstNtfs->stNtfsInfo.dwReadBufSizeForMft);

	for (;;)
	{
		// Normal File인가?
		// Metadata File인가?
		bNormalFile = IsNormalFile(nTypeInode);

		if (TRUE == bNormalFile)
		{
			// 일반 파일인 경우,...
			// 즉, $MFT, $MFTMirr, $LogFile, ... 가 아닌,
			// C:\config.sys 같은 일반 파일인 경우,...

			// 해당 Inode 정보가 있는, LCN 값과 offset을,
			// $MFT의 DATA 정보로 부터 가져온다.

			// 만일,
			//		sector : 512 bytes
			//		cluster : 4096 bytes
			//		inode : 1024 bytes (inode = MFT Record)
			// 인 경우라 가정하고,
			// ...[ssssssss][ssssssss]...와 같이 1 cluster는 8개의 sector로 나눠진다.
			// ...[R R R R ][R R R R ]...와 같이 1 cluster는 4개로 구성되어 있다.
			//                 ^
			//				   |
			// 만일 위 화살표 위치가 실제 inode의 위치라면,
			// Lcn을 통해 [....] 인 Cluster 시작 sector로 이동할 수 있다.
			// 그리고, offset 값을 통해 3번째 sector로 추가적으로 이동할 수 있다.
			if (FALSE == GetLcnFromMftVcn(pstNtfs, 
										  nTypeInode, 
										  &nLcnNormalFile, 
										  &nOffsetByteNormalFile))
			{
				assert(FALSE);
				goto FINAL;
			}
		}

		// 해당 inode가 있는 위치의 Sector를 구한다.
		if (FALSE == bNormalFile)
		{
			// Metadata File인 경우,...
			llMftSectorPos = pstNtfs->stNtfsInfo.llMftSectorPos + (INT)nTypeInode * pstNtfs->stNtfsInfo.dwMftSectorCount;
		}
		else
		{
			// Normal File인 경우,..
			// Lcn 값과 offset으로,
			// 정확한 inode의 위치(sector단위)를 구한다.
			llMftSectorPos = nLcnNormalFile * pstNtfs->stNtfsInfo.dwClusterSize / pstNtfs->stNtfsInfo.wSectorSize;
			llMftSectorPos += nOffsetByteNormalFile / pstNtfs->stNtfsInfo.wSectorSize;
		}

		// 해당 Sector로 간다.
		if (FALSE == SeekSector(pstNtfs, llMftSectorPos))
		{
			pstRtnValue = NULL;
			assert(FALSE);
			goto FINAL;
		}

		// inode record를 읽는다.
		if (pstNtfs->stNtfsInfo.dwReadBufSizeForMft != ReadIo(pstNtfs->pstIo, stFindAttrib.pBuf, pstNtfs->stNtfsInfo.dwReadBufSizeForMft))
		{
			assert(FALSE);
			goto FINAL;
		}

		// Read Buffer에 들어간 MFT Header의 개수를 구한다.
		if (0 != (pstNtfs->stNtfsInfo.dwReadBufSizeForMft % pstNtfs->stNtfsInfo.dwMftSize))
		{
			// 읽은 버퍼의 크기가 MFT 크기 개수의 정수배가 아니다...
			assert(FALSE);
			goto FINAL;
		}
		nMftCountPerReadBuffer = pstNtfs->stNtfsInfo.dwReadBufSizeForMft / pstNtfs->stNtfsInfo.dwMftSize;

		// Inode에 해당되는 MFT Header의 위치를 구한다.
		nMftHeaderPos = pstNtfs->stNtfsInfo.dwMftSize * ((INT)nTypeInode % nMftCountPerReadBuffer);
		if (nMftHeaderPos >= pstNtfs->stNtfsInfo.dwReadBufSizeForMft)
		{
			// 버퍼의 크기를 벗어남
			assert(FALSE);
			goto FINAL;
		}

		// MFT Header를 구한다.
		stFindAttrib.pstMftHeader = (LPST_MFT_HEADER)(stFindAttrib.pBuf + nMftHeaderPos);
		if (NULL == stFindAttrib.pstMftHeader)
		{
			assert(FALSE);
			goto FINAL;
		}

		// Fixup 처리한다.
		if (FALSE == RunFixup(pstNtfs, stFindAttrib.pstMftHeader, stFindAttrib.pBuf))
		{
			assert(FALSE);
			goto FINAL;
		}

		// 올바른가?
		if (FALSE == IsValidMftHeader(pstNtfs, stFindAttrib.pBuf, stFindAttrib.pstMftHeader))
		{
			assert(FALSE);
			goto FINAL;
		}

		// File 이름을 가져오자. (모든 MFT Record는 File name을 가진다.)
		if (FALSE == bAttribList)
		{
			// $ATTRIBUTE_LIST 내부를 볼 때에는,
			// 파일 이름 검증을 하지 않는다.
			// $ATTRIBUTE_LIST 내부에 $FILE_NAME이 없을 수 있기 때문이다.
			pstAttribFileName = GetAttrib(pstNtfs, stFindAttrib.pstMftHeader, stFindAttrib.pBuf, TYPE_ATTRIB_FILE_NAME, &bAttribList, &nInodeAttribList);
			if (NULL == pstAttribFileName)
			{
				assert(FALSE);
				goto FINAL;
			}
		}

		// File 이름이 제대로 되었는가? ($MFT, $MFTMirr, $LogFile, ...)
		if ((nTypeInode <= TYEP_INODE_DEFAULT) && (FALSE == bAttribList))
		{
			// 기본 inode인 경우, file 이름 조회를 한다.
			if (0 != _tcscmp(GetDefaultFileName(nTypeInode), GetFileName(GetAttrib_FileName(pstAttribFileName)).szFileName))
			{
				assert(FALSE);
				goto FINAL;
			}
		}

		// MFT Header에서 caller가 요구한 Attribute를 가져온다.
		stFindAttrib.pstAttrib = GetAttrib(pstNtfs, stFindAttrib.pstMftHeader, stFindAttrib.pBuf, nTypeAttrib, &bAttribList, &nInodeAttribList);
		if ((NULL != stFindAttrib.pstAttrib) && (FALSE == bAttribList))
		{
			// 정상
			// 대다수 일반적인 상황
			break;
		}

		if ((NULL == stFindAttrib.pstAttrib) && (FALSE == bAttribList))
		{
			// 없는 경우도 있기 때문에, assert 하지 않는다.
			// assert(FALSE);
			goto FINAL;
		}

		if ((NULL != stFindAttrib.pstAttrib) && (TRUE == bAttribList))
		{
			// $ATTRIBUTE_LIST에 포함된 경우,
			// 리턴값이 NULL 들어온다.
			assert(FALSE);
			goto FINAL;
		}

		// ((NULL == stFindAttrib.pstAttrib) && (TRUE == bAttribList))
		// 상황이다.
		// 이 뜻은,
		// $ATTRIBUTE_LIST에 해당 Attribute가 포함되어 있다는 뜻이다.
		// 관련된 Inode로 가서 다시 읽어야 된다.
		nTypeInode = nInodeAttribList;

		// 해당 Inode에서 해당 Attribute를 읽도록 한다.
		// 다시 Loop를 수행하도록 한다.
		continue;
	}

	// 여기까지 왔다면 성공이다!
	pstRtnValue = (LPST_FIND_ATTRIB)malloc(sizeof(ST_FIND_ATTRIB));
	if (NULL == pstRtnValue)
	{
		assert(FALSE);
		goto FINAL;
	}
	memset(pstRtnValue, 0, sizeof(ST_FIND_ATTRIB));

	// 마지막으로 복사
	*pstRtnValue = stFindAttrib;

FINAL:

	if (NULL == pstRtnValue)
	{
		// 본 함수 실패
		if (NULL != stFindAttrib.pBuf)
		{
			// 만약 buffer가 할당되었다면,
			// 해제한다.
			free(stFindAttrib.pBuf);
			stFindAttrib.pBuf = NULL;
		}
	}

	return pstRtnValue;
}

VOID CNTFSHelper::FindAttribFree(IN LPST_FIND_ATTRIB pstFindAttrib)
{
	if (NULL == pstFindAttrib)
	{
		return;
	}

	if (NULL != pstFindAttrib->pBuf)
	{
		free(pstFindAttrib->pBuf);
		pstFindAttrib->pBuf			= NULL;
		pstFindAttrib->pstAttrib	= NULL;
	}

	free(pstFindAttrib);
	pstFindAttrib = NULL;
}

// http://inform.pucp.edu.pe/~inf232/Ntfs/ntfs_doc_v0.5/files/index.html
// 를 참고한다.
LPCWSTR CNTFSHelper::GetDefaultFileName(IN TYPE_INODE nInode)
{
	// Windows는 기본이 UTF16이다.
	switch (nInode)
	{
	case TYPE_INODE_MFT:
		return TEXT("$MFT");
	case TYPE_INODE_MFTMIRR:
		return TEXT("$MFTMirr");
	case TYPE_INODE_LOGFILE:
		return TEXT("$LogFile");
	case TYPE_INODE_VOLUME:
		return TEXT("$Volume");
	case TYPE_INODE_ATTRDEF:
		return TEXT("$AttrDef");
	case TYPE_INODE_DOT:
		return TEXT(".");
	case TYPE_INODE_BITMAP:
		return TEXT("$Bitmap");
	case TYPE_INODE_BOOT:
		return TEXT("$Boot");
	case TYPE_INODE_BADCLUS:
		return TEXT("$BadClus");
	case TYPE_INODE_SECURE:
		return TEXT("$Secure");
	case TYPE_INODE_UPCASE:
		return TEXT("$UpCase");
	case TYPE_INODE_EXTEND:
		return TEXT("$Extend");
	}
	
	assert(FALSE);
	return TEXT("");
}

// http://www.ceng.metu.edu.tr/~isikligil/ceng334/fsa.pdf
// 책의 275 페이지 윗 부분을 참고한다.
BOOL CNTFSHelper::CheckUsedCluster(IN UBYTE1* pBitmap, IN _OFF64_T nBufSize, IN UBYTE8 nLcn)
{
	BOOL	bRtnValue	= FALSE;
	UBYTE8	nByte		= 0;
	INT		nOffset		= 0;

	if (NULL == pBitmap)
	{
		bRtnValue = FALSE;
		assert(FALSE);
		goto FINAL;
	}

	if (nBufSize <= nLcn / 8)
	{
		// Lcn의 크기가 Bitmap 크기보다 큼
		bRtnValue = FALSE;
		assert(FALSE);
		goto FINAL;
	}

	nByte	= nLcn / 8;
	nOffset	= (INT)(nLcn - 8 * nByte);

	if ((0 <= nOffset) && (nOffset <= 8))
	{
		// offset은 0~8사이에 있어야 한다.
		// good
	}
	else
	{
		// bad
		bRtnValue = FALSE;
		assert(FALSE);
		goto FINAL;
	}

	if (0 == (pBitmap[nByte] & (1 << nOffset)))
	{
		// unsed
		bRtnValue = FALSE;
	}
	else
	{
		// used
		bRtnValue = TRUE;
	}

FINAL:
	return bRtnValue;
}

CNTFSHelper::LPST_BITMAP_INFO CNTFSHelper::BitmapAlloc(IN LPST_NTFS pstNtfs)
{
	LPST_BITMAP_INFO	pstRtnValue			= NULL;
	ST_BITMAP_INFO		stBitmapInfo		= {0,};
	LPST_FIND_ATTRIB	pstFindAttrib		= NULL;
	LPST_RUNLIST		pstRunlist			= NULL;
	INT					nRunListCount		= 0;
	INT					i					= 0;
	INT					nPos				= 0;
	INT					nCountBitmapCluster	= 0;
	INT					nTotalLcnCount		= 0;
	_OFF64_T			nReaded				= 0;

	if (NULL == pstNtfs)
	{
		pstRtnValue = NULL;
		goto FINAL;
	}

	// $BITMAP의 $DATA를 찾는다.
	pstFindAttrib = FindAttribAlloc(pstNtfs, TYPE_INODE_BITMAP, TYPE_ATTRIB_DATA);
	if (NULL == pstFindAttrib)
	{
		pstRtnValue = NULL;
		assert(FALSE);
		goto FINAL;
	}

	// RunList를 구한다.
	pstRunlist = GetRunListAlloc(GetRunListByte(pstFindAttrib->pstAttrib), &nRunListCount);
	if (NULL == pstRunlist)
	{
		pstRtnValue = NULL;
		assert(FALSE);
		goto FINAL;
	}

	if (nRunListCount <= 0)
	{
		// runlist 항목이 있어야 한다.
		pstRtnValue = NULL;
		assert(FALSE);
		goto FINAL;
	}

	// 보통 bitmap은 fragment되어 있지 않고,
	// bitmap이 저장된 cluster개수는 그 많지 않다.
	for (i=0; i<nRunListCount; i++)
	{
		nCountBitmapCluster += (INT)pstRunlist[i].llLength;
	}

	stBitmapInfo.pBitmap = (UBYTE1*)malloc(pstNtfs->stNtfsInfo.dwClusterSize * nCountBitmapCluster);
	if (NULL == stBitmapInfo.pBitmap)
	{
		pstRtnValue = NULL;
		assert(FALSE);
		goto FINAL;
	}
	memset(stBitmapInfo.pBitmap, 0, pstNtfs->stNtfsInfo.dwClusterSize * nCountBitmapCluster);

	for (i=0; i<nRunListCount; i++)
	{
		if (FALSE == SeekLcn(pstNtfs, pstRunlist->llLCN))
		{
			pstRtnValue = NULL;
			assert(FALSE);
			goto FINAL;
		}

		if (nPos + pstNtfs->stNtfsInfo.dwClusterSize * pstRunlist[i].llLength > pstNtfs->stNtfsInfo.dwClusterSize * nCountBitmapCluster)
		{
			// buffer overflow 체크
			pstRtnValue = NULL;
			assert(FALSE);
			goto FINAL;
		}

		nReaded = pstNtfs->stNtfsInfo.dwClusterSize * pstRunlist[i].llLength;
		if (nReaded != ReadIo(pstNtfs->pstIo, &stBitmapInfo.pBitmap[nPos], (INT)nReaded))
		{
			pstRtnValue = NULL;
			assert(FALSE);
			goto FINAL;
		}

		nPos += pstNtfs->stNtfsInfo.dwClusterSize * (INT)pstRunlist[i].llLength;
	}

	// bitmap 크기 (byte)
	stBitmapInfo.nBitmapBufSize = pstNtfs->stNtfsInfo.dwClusterSize * nCountBitmapCluster;

	// 여기까지 왔다면 성공
	pstRtnValue = (LPST_BITMAP_INFO)malloc(sizeof(ST_BITMAP_INFO));
	if (NULL == pstRtnValue)
	{
		assert(FALSE);
		goto FINAL;
	}
	memset(pstRtnValue, 0, sizeof(ST_BITMAP_INFO));

	// 복사로 마무리
	*pstRtnValue = stBitmapInfo;

	// 내부 Alloc으로 생성된 bitmap
	pstRtnValue->bAlloc = TRUE;

FINAL:
	if (NULL == pstRtnValue)
	{
		// 본 함수 실패
		if (NULL != stBitmapInfo.pBitmap)
		{
			free(stBitmapInfo.pBitmap);
			stBitmapInfo.pBitmap = NULL;
		}
	}

	if (NULL != pstFindAttrib)
	{
		FindAttribFree(pstFindAttrib);
		pstFindAttrib = NULL;
	}

	if (NULL != pstRunlist)
	{
		GetRunListFree(pstRunlist);
		pstRunlist = NULL;
	}

	return pstRtnValue;
}

VOID CNTFSHelper::BitmapFree(IN LPST_BITMAP_INFO pstBitmapInfo)
{
	if (NULL == pstBitmapInfo)
	{
		return;
	}

	if (FALSE == pstBitmapInfo->bAlloc)
	{
		// alloc으로 생성된 bitmap이 아닌 경우에는,...
		// 외부 파일등에 의해 load된 경우임...
		// 이때는 기본적으로 free하면 좋지 않다.
		// 자체적인 free를 하도록 한다.
		assert(FALSE);
		return;
	}

	if (NULL != pstBitmapInfo->pBitmap)
	{
		free(pstBitmapInfo->pBitmap);
		pstBitmapInfo->pBitmap = NULL;
	}

	free(pstBitmapInfo);
	pstBitmapInfo = NULL;
}

BOOL CNTFSHelper::IsClusterUsed(IN UBYTE8 nLcn, IN LPST_BITMAP_INFO pstBitmapInfo)
{
	BOOL bRtnValue = FALSE;

	if (NULL == pstBitmapInfo)
	{
		bRtnValue = FALSE;
		assert(FALSE);
		goto FINAL;
	}

	bRtnValue = CheckUsedCluster(pstBitmapInfo->pBitmap, 
								 pstBitmapInfo->nBitmapBufSize, 
								 nLcn);

FINAL:
	return bRtnValue;
}

// 사용중인 Total count를 리턴하고,
// 사용중인 Cluster number중 최대값을 optional로 전달한다. (NULL 가능)
CNTFSHelper::UBYTE8 CNTFSHelper::GetUsedClusterCount(IN LPST_NTFS pstNtfs, IN LPST_BITMAP_INFO pstBitmapInfo, OPTIONAL OUT UBYTE8* pnMaxUsedClusterNum)
{
	UBYTE8 nRtnValue	= 0;
	UBYTE8 i			= 0;

	if ((NULL == pstNtfs) || (NULL == pstBitmapInfo))
	{
		nRtnValue = 0;
		assert(FALSE);
		goto FINAL;
	}

	for (i=0; i<pstNtfs->stNtfsInfo.nTotalClusterCount; i++)
	{
		if (TRUE == IsClusterUsed(i, pstBitmapInfo))
		{
			nRtnValue++;
		}

		if (NULL != pnMaxUsedClusterNum)
		{
			*pnMaxUsedClusterNum = i;
		}
	}

FINAL:
	return nRtnValue;
}

// http://inform.pucp.edu.pe/~inf232/Ntfs/ntfs_doc_v0.5/files/index.html
// 에 따르면, Ntfs의 File은 크게 Metadata와 Normal, 두개의 Category로
// 나눠진다.
// 일반적인 Normal file에 해당되는 inode인지 리턴한다.
BOOL CNTFSHelper::IsNormalFile(IN TYPE_INODE nInode)
{
	if (nInode >= TYPE_INODE_FILE)
	{
		return TRUE;
	}

	return FALSE;
}

// [TBD] Disk 크기를 구하는 것은 쉽지 않다.
CNTFSHelper::_OFF64_T CNTFSHelper::GetDiskSize(IN LPST_NTFS pstNtfs)
{
	_OFF64_T	nRtnValue	= 0;
	INT			nMod		= 0;

	if (NULL == pstNtfs)
	{
		assert(FALSE);
		return 0;
	}

	/*
	// http://thestarman.pcministry.com/asm/mbr/NTFSBR.htm
	// 의
	// 28h Total Sectors
	// 에 따르면,
	// Bios parameter block에 있는 sector 개수에 +1 해줘야
	// 전체 개수가 된다고 나와있음
	return (_OFF64_T)((pstNtfs->stBpb.llSectorNum+1) * pstNtfs->stNtfsInfo.wSectorSize);
	*/

	// 앞의 계산에서, Cluster 크기 한개 만큼 빼야 되더라.
	nRtnValue = (_OFF64_T)((pstNtfs->stBpb.llSectorNum+1) * pstNtfs->stNtfsInfo.wSectorSize) - pstNtfs->stNtfsInfo.dwClusterSize;

	// [TBD] 아랫부분이 명확해져야 함
	{
		nMod = (nRtnValue % pstNtfs->stNtfsInfo.dwClusterSize);
		if (0 != nMod)
		{
			// Disk 크기가 cluster 단위에 맞아 떨어지지 않을 때,...
			// 나누어 떨어지게 만들어 준다.
			nRtnValue -= nMod;

			// 그리고, 이때도 cluster 크기 한개가 증가되었다.
			nRtnValue += pstNtfs->stNtfsInfo.dwClusterSize;
		}
	}

	return nRtnValue;
}

// INDEX_ENTRY의 stream 값을 FILE_NAME_ATTRIBUTE로 받아들인다.
// 이것이 과연 합당한지는 확인이 필요하다.
CNTFSHelper::LPST_ATTRIB_FILE_NAME CNTFSHelper::GetFileNameAttrib(IN LPST_INDEX_ENTRY pstIndexEntry)
{
	LPST_ATTRIB_FILE_NAME pstRtnValue = NULL;

	if (NULL == pstIndexEntry)
	{
		assert(FALSE);
		goto FINAL;
	}

	if (NULL == pstIndexEntry->stream)
	{
		assert(FALSE);
		goto FINAL;
	}

	if (0 == pstIndexEntry->nLengthStream)
	{
		assert(FALSE);
		goto FINAL;
	}

	pstRtnValue = (LPST_ATTRIB_FILE_NAME)pstIndexEntry->stream;

FINAL:
	return pstRtnValue;
}

// INDEX_ENTRY의 File Name 정보를 구한다.
// File Name 정보가 없는 경우 FALSE를 전달한다. (혹은 오류 발생시)
// [TBD] multi-platform (ex, linux) 준비한다. (StringCch~ 함수군)
BOOL CNTFSHelper::GetIndexEntryFileName(IN LPST_INDEX_ENTRY pstEntry, OUT LPWSTR lpszName, IN DWORD dwCchLength)
{
	BOOL					bRtnValue			= FALSE;
	LPST_ATTRIB_FILE_NAME	pstAttribFileName	= NULL;

	if ((NULL == pstEntry) || (NULL == lpszName) || (dwCchLength < MAX_PATH_NTFS))
	{
		assert(FALSE);
		goto FINAL;
	}

	if (0 == pstEntry->nLengthStream)
	{
		// 해당 값이 0이라면, Data가 없다는 뜻이다.
		goto FINAL;
	}

	pstAttribFileName = GetFileNameAttrib(pstEntry);
	if (NULL == pstAttribFileName)
	{
		assert(FALSE);
		goto FINAL;
	}

	StringCchCopyW(lpszName, dwCchLength, GetFileName(pstAttribFileName).szFileName);

	// 여기까지 왔다면 성공
	bRtnValue = TRUE;

FINAL:
	return bRtnValue;
}

// Update Sequence 처리를 한다.
// Cluster당 보통 8개의 sector가 들어가는데,
// sector의 값이 잘 저장되어 있는지 검증을 하고,
// 검증을 통해 sector 일부의 값을 원래의 값으로 복원한다.
// nIndexSize는 ST_INDEX_ROOT::nSize를 의미한다.
BOOL CNTFSHelper::UpdateSequenceIndexAlloc(IN UBYTE4 nIndexSize, IN UBYTE1* pBufCluster, IN INT nClusterSize, IN INT nSectorSize)
{
	BOOL						bRtnValue			= FALSE;
	UBYTE1*						pUpdateSequenceAddr	= NULL;
	UBYTE2						nUpdateSequence		= 0;
	UBYTE2*						pArrUpdateSequence	= NULL;
	INT							nArrayIndex			= 0;
	LPST_STANDARD_INDEX_HEADER	pstHeader			= NULL;
	INT							i					= 0;
	INT							nSector				= 0;
	INT							nSequenceCount		= 0;

	if ((NULL == pBufCluster) || (0 == nClusterSize) || (0 == nSectorSize) || (0 == nIndexSize) || (0 != (nClusterSize % nSectorSize)))
	{
		assert(FALSE);
		goto FINAL;
	}

	pstHeader = (LPST_STANDARD_INDEX_HEADER)pBufCluster;

	// 각 값들을 가져온다.
	pUpdateSequenceAddr	= pBufCluster + pstHeader->nOffsetUpdateSequence;
	nUpdateSequence		= *(UBYTE2*)pUpdateSequenceAddr;
	pArrUpdateSequence	= (UBYTE2*)(&pUpdateSequenceAddr[2]);

	// pstHeader->nSizeUpdateSequence
	// 값은 +1인듯 하다.
	// 아래 값으로 사용한다.
	nSequenceCount = nIndexSize / nSectorSize;

	// Sector의 마지막 2바이트가 대상
	for (i=nSectorSize-2; i<nClusterSize; i+=nSectorSize)
	{
		if (nSector >= nSequenceCount)
		{
			// 처리 종료
			break;
		}

		if (nUpdateSequence != *(UBYTE2*)&pBufCluster[i])
		{
			// 검증 실패
			assert(FALSE);
			goto FINAL;
		}

		// Sequence Array로 부터 값을 변경 처리한다.
		*(UBYTE2*)&pBufCluster[i] = pArrUpdateSequence[nSector];

		nSector++;
	}

	// 여기까지 왔다면 성공
	bRtnValue = TRUE;

FINAL:
	return bRtnValue;
}

// http://inform.pucp.edu.pe/~inf232/Ntfs/ntfs_doc_v0.5/concepts/file_reference.html
// 와 같이, File Reference를 가지고 Inode 값(FILE record number)을 구한다.
CNTFSHelper::TYPE_INODE CNTFSHelper::GetInode(IN LPST_INDEX_ENTRY pstIndexEntry, OPTIONAL OUT UBYTE2* pnSequenceNumber)
{
	TYPE_INODE nRtnValue = 0;

	if (NULL != pnSequenceNumber)
	{
		*pnSequenceNumber = 0;
	}

	if (NULL == pstIndexEntry)
	{
		assert(FALSE);
		goto FINAL;
	}

	nRtnValue = pstIndexEntry->nFileReference & 0x0000FFFFFFFFFFFFUL;

	if (NULL != pnSequenceNumber)
	{
		*pnSequenceNumber = (UBYTE2)(pstIndexEntry->nFileReference >> 48);
	}

FINAL:
	return nRtnValue;
}

// 일반적인 File, Directory의 Traverse Index 정보($INDEX_ALLOCATION)
// 에서, Sub-Directory의 정보는,
// File Reference라는 값으로 저장되어 있는데, 해당 값은,
// $MFT의 Vcn 값으로 되어 있다.
// 그래서 본 함수를 이용하면, Sub-directory로 Traverse할 수 있도록 해준다.
//
// TYPE_INODE_FILE 이후의 inode에 대한 위치를,
// $MFT를 이용하여 계산한다.
// 본 함수로 구해진 Lcn과 *pnOffsetBytes 값으로,
// 해당 inode의 위치를 Seek할 수 있다.
// 즉, LCN * Cluster 크기 + *pnOffsetBytes로
// Seek한다.
//
// 예를 들어,
// 
// # 문자가 $MFT의 Data라고 할 때 (Cluster 단위),
// 아래와 같이 $MFT가 3개로 Fragmented되어 있다고 가정하자.
// 
// ##########.....########....###
// 0123456789.....01234567....890 ==> MFT의 VCN
// 012345678901234567890123456789 ==> Disk의 LCN
//                  ^
//                  |
//
// 만일, 요청한 inode가 순서상으로 12번째 Cluster에 포함되어 있다면,
// 12번째 Cluster인 #는 다음과 같이 8개(8 x 512 = 4096)의 sector(@)로 구성되어 있다.
//
// ...@@@@@@@@..
//          ^
//          |
//
// 만약 7번째 sector라고 한다면,
// 다음과 같이 전달된다.
//
// Lcn    : 17
// Offset : 7 * 512
//
// 그러면, 해당 값을, Seek하려면, 17 * 4096(cluster 크기) + 7 * 512(sector 크기)
// 값으로 seek하면, inode에 접근할 수 있다.
//
// 다시 설명하자면,
// 만일,
//		sector : 512 bytes
//		cluster : 4096 bytes
//		inode : 1024 bytes (inode = MFT Record)
// 인 경우라 가정하고,
// ...[ssssssss][ssssssss]...와 같이 1 cluster는 8개의 sector로 나눠진다.
// ...[R R R R ][R R R R ]...와 같이 1 cluster는 4개로 구성되어 있다.
//                 ^
//				   |
// 만일 위 화살표 위치가 실제 inode의 위치라면,
// Lcn을 통해 [....] 인 Cluster 시작 sector로 이동할 수 있다.
// 그리고, offset 값을 통해 3번째 sector로 추가적으로 이동할 수 있다.
BOOL CNTFSHelper::GetLcnFromMftVcn(IN LPST_NTFS pstNtfs, IN TYPE_INODE nInode, OUT UBYTE8* pnLcn, OUT PINT pnOffsetBytes)
{
	BOOL				bRtnValue		= FALSE;
	LPST_FIND_ATTRIB	pstAttribAlloc	= NULL;
	UBYTE8				nVcn			= 0;
	UBYTE8				nLcn			= 0;
	UBYTE8				nPos			= 0;
	INT					nOffsetBytes	= 0;
	LPST_RUNLIST		pstRunlistAlloc	= NULL;
	INT					nCountRunlist	= 0;
	UBYTE8				nTotal			= 0;
	INT					i				= 0;

	if (NULL != pnLcn)
	{
		*pnLcn = 0;
	}

	if (NULL != pnOffsetBytes)
	{
		*pnOffsetBytes = 0;
	}

	if ((NULL == pstNtfs) || (NULL == pnLcn) || (NULL == pnOffsetBytes) || (TYPE_INODE_FILE > nInode))
	{
		assert(FALSE);
		goto FINAL;
	}

	// Attribute를 구한다.
	pstAttribAlloc = FindAttribAlloc(pstNtfs, TYPE_INODE_MFT, TYPE_ATTRIB_DATA);
	if (NULL == pstAttribAlloc)
	{
		assert(FALSE);
		goto FINAL;
	}

	// Runlist를 구한다.
	pstRunlistAlloc = GetRunListAlloc(GetRunListByte(pstAttribAlloc->pstAttrib), &nCountRunlist);
	if ((NULL == pstRunlistAlloc) || (nCountRunlist <= 0))
	{
		assert(FALSE);
		goto FINAL;
	}

	// 우선, Attrib record에 의한 vcn 개수와,
	// runlist의 vcn개수가 같은지 검증한다.
	for (i=0; i<nCountRunlist; i++)
	{
		nTotal += pstRunlistAlloc[i].llLength;
	}
	
	if (nTotal != (pstAttribAlloc->pstAttrib->stAttr.stNonResident.llEndVCN - pstAttribAlloc->pstAttrib->stAttr.stNonResident.llStartVCN + 1))
	{
		// 같지 않다.
		assert(FALSE);
		goto FINAL;
	}

	if (1 == pstAttribAlloc->pstAttrib->cNonResident)
	{
		// good
		// 이곳은 non-resident only 이다.
	}
	else
	{
		assert(FALSE);
		goto FINAL;
	}

	// Position을 구한다.
	nPos = (UBYTE8)nInode * pstNtfs->stNtfsInfo.dwMftSectorCount * pstNtfs->stNtfsInfo.wSectorSize;
	nVcn =  nPos / pstNtfs->stNtfsInfo.dwClusterSize;
	nOffsetBytes = (INT)(nPos % pstNtfs->stNtfsInfo.dwClusterSize);

	// [TBD]
	// StartVCN은 $MFT에서 0이던데,
	// 만약 StartVCN이 0을 넘어가는 경우는 어떠할까?
	if (nVcn > pstAttribAlloc->pstAttrib->stAttr.stNonResident.llEndVCN + pstAttribAlloc->pstAttrib->stAttr.stNonResident.llStartVCN)
	{
		// 그 크기를 벗어남
		assert(FALSE);
		goto FINAL;
	}

	// 이제 Vcn에 해당되는 Lcn을 구하자.
	if (FALSE == GetLcnFromVcn(pstRunlistAlloc, 
							   nCountRunlist, 
							   nVcn, 
							   pstAttribAlloc->pstAttrib->stAttr.stNonResident.llStartVCN, 
							   &nLcn))
	{
		// 실패
		assert(FALSE);
		goto FINAL;
	}

	// 여기까지 왔다면 성공
	*pnLcn = nLcn;
	*pnOffsetBytes = nOffsetBytes;
	bRtnValue = TRUE;

FINAL:

	if (NULL != pstRunlistAlloc)
	{
		GetRunListFree(pstRunlistAlloc);
		pstRunlistAlloc = NULL;
	}

	if (NULL != pstAttribAlloc)
	{
		FindAttribFree(pstAttribAlloc);
		pstAttribAlloc = NULL;
	}

	return bRtnValue;
}

// Inode를 찾는다.
// nStartInode : 시작할 Inode. 즉, 시작할 Directory의 inode.
// lpszName : 찾을 파일명
// pbFound : 찾은 경우 TRUE를 전달함. 아니면 FALSE를 전달함
// pstInodeData : NULL 가능. 해당 Inode의 정보
//
// 리턴 : Inode 값 (*pbFound가 TRUE인 경우 의미있음)
//
// Remark : b-tree traverse 논리는 CNTFSHelper::CreateStack(...) 주석 참고
CNTFSHelper::TYPE_INODE CNTFSHelper::FindInode(IN LPST_NTFS pstNtfs, IN TYPE_INODE nStartInode, IN LPCWSTR lpszName, OUT PBOOL pbFound, OPTIONAL OUT LPST_INODEDATA pstInodeData)
{
	TYPE_INODE				nRtnValue			= 0;
	LPST_INDEX_ENTRY_ARRAY	pstArray			= NULL;
	LPST_INDEX_ENTRY_ARRAY	pstArraySubnode		= NULL;
	UBYTE4					nIndexBlockSize		= 0;
	INT						i					= 0;
	INT						nFind				= 0;
	BOOL					bFound				= FALSE;
	LPWSTR					lpszNameUpper		= NULL;
	BOOL					bGotoSubnode		= FALSE;
	LPST_RUNLIST			pstRunListAlloc		= NULL;
	INT						nCountRunList		= 0;
	LPST_FIND_ATTRIB		pstIndexAlloc		= NULL;
	LPST_INODEDATA			pstInodeDataAlloc	= NULL;

	if (NULL != pbFound)
	{
		*pbFound = FALSE;
	}

	if (NULL != pstInodeData)
	{
		memset(pstInodeData, 0, sizeof(*pstInodeData));
	}

	if ((NULL == pstNtfs) || (NULL == lpszName) || (NULL == pbFound))
	{
		assert(FALSE);
		goto FINAL;
	}

	lpszNameUpper = (LPWSTR)malloc(sizeof(WCHAR)*MAX_PATH_NTFS);
	if (NULL == lpszNameUpper)
	{
		assert(FALSE);
		goto FINAL;
	}
	memset(lpszNameUpper, 0, sizeof(WCHAR)*MAX_PATH_NTFS);

	// [TBD] multi-platform(os) 지원 (such as linux)
	StringCchCopyW(lpszNameUpper, MAX_PATH_NTFS, lpszName);
	if (0 == _wcsupr_s(lpszNameUpper, MAX_PATH_NTFS))
	{
		// 대문자로 변환한다.
		// [TBD] $UpCase 정보를 검도한다.
		// 성공
	}
	else
	{
		assert(FALSE);
		goto FINAL;
	}

	if ((NULL != pstNtfs->hInodeCache) && (NULL != pstNtfs->pstInodeCache) && (NULL != pstNtfs->pstInodeCache->pfnGetCache))
	{
		// 만일 inode cache가 있다면,...

		if (NULL == pstInodeData)
		{
			// 만일, NULL인 경우,...
			pstInodeDataAlloc = (LPST_INODEDATA)malloc(sizeof(*pstInodeDataAlloc));
			if (NULL == pstInodeDataAlloc)
			{
				assert(FALSE);
				goto FINAL;
			}
			memset(pstInodeDataAlloc, 0, sizeof(*pstInodeDataAlloc));

			// 메모리 할당된것 사용함
			pstInodeData = pstInodeData;
		}

		if (TRUE == (*pstNtfs->pstInodeCache->pfnGetCache)(pstNtfs->hInodeCache, nStartInode, lpszNameUpper, &bFound, pstInodeData, pstNtfs->pstInodeCache->pUserContext))
		{
			// Cache 호출 성공
			if (TRUE == bFound)
			{
				// Cache에서 찾기 성공...

				// 아래의 코드들을 수행하지 않고,
				// 바로 나간다.
				nRtnValue = pstInodeData->nInode;
				*pbFound = TRUE;
				goto FINAL;
			}
			else
			{
				// Cache에서 찾는데 실패했다.
				// do nothing...
				// 아래 코드들을 수행한다.
			}
		}
	}

	// $INDEX_ALLOCATION의 Runlist를 구한다.
	pstIndexAlloc = FindAttribAlloc(pstNtfs, nStartInode, TYPE_ATTRIB_INDEX_ALLOCATION);
	if (NULL != pstIndexAlloc)
	{
		pstRunListAlloc = GetRunListAlloc(GetRunListByte(pstIndexAlloc->pstAttrib), &nCountRunList);
		if (NULL == pstRunListAlloc)
		{
			assert(FALSE);
			goto FINAL;
		}
	}

	// $INDEX_ROOT에 포함된 목록을 구한다.
	pstArray = GetIndexEntryArrayAllocByIndexRoot(pstNtfs, nStartInode, &nIndexBlockSize);
	if ((NULL == pstArray) || (NULL == pstArray->pstArray))
	{
		assert(FALSE);
		goto FINAL;
	}

	for (;;)
	{
		bGotoSubnode = FALSE;
		for (i=0; i<pstArray->nItemCount; i++)
		{
			if (TRUE == pstArray->pstArray[i].bHasData)
			{
				// 만일 data(즉, 파일 이름)가 포함되어 있다면,...
				nFind = wcscmpUppercase(lpszNameUpper, pstArray->pstArray[i].lpszFileNameAlloc);
				if (0 == nFind)
				{
					// 찾았다 !!!
					*pbFound = TRUE;
					nRtnValue = pstArray->pstArray[i].nInode;
					break;
				}
				else if (nFind < 0)
				{
					// 대문자 이름 비교하여, 작은 값이 전달된 경우엔,...
					// b-tree의 subnode로 간다.
					if (TRUE == pstArray->pstArray[i].bHasSubNode)
					{
						// 만일 subnode가 있다면,...
						// subnode로 간다.
						bGotoSubnode = TRUE;
						break;
					}
				}
			}
			else
			{
				// 만일 data가 포함되어 있지 않다면,...
				if (TRUE == pstArray->pstArray[i].bHasSubNode)
				{
					// sudnode가 있는 경우,...
					// 즉, data는 없고 subnode만 있는 경으...
					// subnode로 간다.
					bGotoSubnode = TRUE;
					break;
				}
			}
		}

		if (TRUE == bGotoSubnode)
		{
			// subnode로 가야 한다면,...
			if (NULL == pstRunListAlloc)
			{
				// subnode로 가야하는데,
				// $INDEX_ALLOCATION의 runlist가 없다면,...
				assert(FALSE);
				goto FINAL;
			}

			pstArraySubnode = GetIndexEntryArrayAllocByVcn(pstNtfs, nIndexBlockSize, pstRunListAlloc, nCountRunList, pstArray->pstArray[i].nVcnSubnode);
			if (NULL == pstArraySubnode)
			{
				assert(FALSE);
				goto FINAL;
			}

			// subnode로 내려가는 것이 성공하였다.
			// loop를 다시 시작하자.
			// 그러기 위해선, 이전의 Index entry array를 삭제하고,
			// subnode의 것으로 교체하자.
			GetIndexEntryArrayFree(pstArray);
			pstArray		= pstArraySubnode;
			pstArraySubnode	= NULL;
			continue;
		}

		// 루프를 벗어난다.
		break;
	}

	if (TRUE == *pbFound)
	{
		// 찾은 경우, 호출자의 요청에 따라, 추가 정보를 전달한다.
		if (NULL != pstInodeData)
		{
			if (FALSE == GetInodeData(&pstArray->pstArray[i], pstInodeData, NULL, 0))
			{
				// 추가 정보 획득 실패시,
				// 찾지 못한 것으로 한다.
				assert(FALSE);
				*pbFound = FALSE;
			}
		}

		// inode cache가 있는 경우, cache에 쓰도록 한다.
		if ((NULL != pstNtfs->hInodeCache) && (NULL != pstNtfs->pstInodeCache) && (NULL != pstNtfs->pstInodeCache->pfnGetCache))
		{
			if (TRUE == (*pstNtfs->pstInodeCache->pfnSetCache)(pstNtfs->hInodeCache, nStartInode, lpszNameUpper, pstInodeData, pstNtfs->pstInodeCache->pUserContext))
			{
				// inode cache에 쓰기 성공
			}
			else
			{
				// inode cache에 쓰기 실패
				assert(FALSE);
			}
		}
	}

FINAL:

	if (NULL != pstInodeDataAlloc)
	{
		free(pstInodeDataAlloc);
		pstInodeDataAlloc	= NULL;
		pstInodeData		= NULL;
	}

	if (NULL != lpszNameUpper)
	{
		free(lpszNameUpper);
		lpszNameUpper = NULL;
	}

	if (NULL != pstRunListAlloc)
	{
		GetRunListFree(pstRunListAlloc);
		pstRunListAlloc = NULL;
	}

	if (NULL != pstIndexAlloc)
	{
		FindAttribFree(pstIndexAlloc);
		pstIndexAlloc = NULL;
	}

	if (NULL != pstArraySubnode)
	{
		GetIndexEntryArrayFree(pstArraySubnode);
		pstArraySubnode = NULL;
	}

	if (NULL != pstArray)
	{
		GetIndexEntryArrayFree(pstArray);
		pstArray = NULL;
	}
	return nRtnValue;
}

// [TBD] support multi-os (such as linux)
// lpszFindNameUpper는 대문자로 되어야 한다.
// 함수 전달시 파라미터의 순서를 주의한다.
INT CNTFSHelper::wcscmpUppercase(IN LPCWSTR lpszFindNameUpper, IN LPCWSTR lpszEntry)
{
	INT		nRtnValue		= 1;
	LPWSTR	lpszUpperEntry	= NULL;

	if ((NULL == lpszEntry) || (NULL == lpszFindNameUpper))
	{
		assert(FALSE);
		goto FINAL;
	}

	lpszUpperEntry = (LPWSTR)malloc(sizeof(WCHAR) * MAX_PATH_NTFS);
	if (NULL == lpszUpperEntry)
	{
		assert(FALSE);
		goto FINAL;
	}
	memset(lpszUpperEntry, 0, sizeof(WCHAR)*MAX_PATH_NTFS);

	StringCchCopyW(lpszUpperEntry, MAX_PATH_NTFS, lpszEntry);

	if (0 == _wcsupr_s(lpszUpperEntry, MAX_PATH_NTFS))
	{
		// 성공
	}
	else
	{
		// bad
		assert(FALSE);
		goto FINAL;
	}

	nRtnValue = wcscmp(lpszFindNameUpper, lpszUpperEntry);

FINAL:
	if (NULL != lpszUpperEntry)
	{
		free(lpszUpperEntry);
		lpszUpperEntry = NULL;
	}
	return nRtnValue;
}

// Index block에는 여러개의 index entry가 포함되어 있다.
// 주어진 index entry의 다음 항목을 전달한다.
//
// 만일 index block의 마지막에 도착하였다면, 동일한 값을 리턴한다.
CNTFSHelper::LPST_INDEX_ENTRY CNTFSHelper::GetNextEntry(IN LPST_INDEX_ENTRY pstCurIndexEntry)
{
	LPST_INDEX_ENTRY pstRtnValue = NULL;

	if (NULL == pstCurIndexEntry)
	{
		assert(FALSE);
		goto FINAL;
	}

	// Subnode, Last entry
	// 옵션 이외의 값이 bitwise된 경우,...
	if (0 != (~((INDEX_ENTRY_FLAG_SUBNODE | INDEX_ENTRY_FLAG_LAST_ENTRY)) & pstCurIndexEntry->nFlag))
	{
		assert(FALSE);
		goto FINAL;
	}

	if (INDEX_ENTRY_FLAG_LAST_ENTRY == (INDEX_ENTRY_FLAG_LAST_ENTRY & pstCurIndexEntry->nFlag))
	{
		// 만일 현재가 마지막 이라면,...
		// 현재와 동일값을 리턴한다.
		pstRtnValue = pstCurIndexEntry;
		goto FINAL;
	}

	// [TBD]
	// 아래의 값이 해당 memory의 boundary를 벗어나는지,
	// validation 체크를 해주면 좋을 듯 하다.

	pstRtnValue = (LPST_INDEX_ENTRY)((UBYTE1*)pstCurIndexEntry + pstCurIndexEntry->nLengthIndexEntry);
	if (NULL == pstRtnValue)
	{
		assert(FALSE);
		goto FINAL;
	}


	// 옵션 이외의 값이 bitwise된 경우,...
	if (0 != (~((INDEX_ENTRY_FLAG_SUBNODE | INDEX_ENTRY_FLAG_LAST_ENTRY)) & pstRtnValue->nFlag))
	{
		pstRtnValue = NULL;
		assert(FALSE);
		goto FINAL;
	}

FINAL:
	return pstRtnValue;
}

// Vcn을 Lcn으로 변환한다.
//
// 만약,
// 다음과 같이 RunList가 되어 있을 떄,
// [0] : LCN = 10, Length = 3
// [1] : LCN = 100, Length = 4
// [2] : LCN = 1000, Length = 5
// 이는 다음과 같이 총 12개의 Cluster로 구성되어 있음을 의미한다.
// 10,11,12,100,101,102,103,1000,1001,1002,1003,1004
// 만약, 5(nVcn=4)번째 Vcn을 요청했다면,
// 101을 리턴한다.
// 만약 범위를 벗어나거나 잘 못된 입력에 대해서는 FALSE를 리턴한다.
// 그리고, ATTRIB_RECORD에 시작 Vcn(llStartVCN)이 있는데,
// 실제 시작하는 Vcn값을 의마한다.(확인 필요)
// 그래서, 만약,
// llStartVCN = 3
// nVcn = 4 이라면,
// 실제 nVcn은 2가 되는 것이고,
// 그럼 위 예에서는 11이 된다.
// 물론, llStartVCN이 0이였다면, 101을 전달한다.
BOOL CNTFSHelper::GetLcnFromVcn(IN LPST_RUNLIST pstRunList, IN INT nCountRunList, IN UBYTE8 nVcn, OPTIONAL IN UBYTE8 llStartVCN, OUT UBYTE8* pnLcn)
{
	BOOL	bRtnValue = FALSE;
	UBYTE8	nVcnTotal = 0;
	INT		i		  = 0;
	BOOL	bFound	  = FALSE;

	if (NULL != pnLcn)
	{
		*pnLcn = 0;
	}

	if ((NULL == pstRunList) || (0 >= nCountRunList) || (NULL == pnLcn) || (nVcn < llStartVCN))
	{
		assert(FALSE);
		goto FINAL;
	}

	nVcnTotal = llStartVCN;
	for (i=0; i<nCountRunList; i++)
	{
		if (nVcnTotal + pstRunList[i].llLength > nVcn)
		{
			bFound = TRUE;
			*pnLcn = pstRunList[i].llLCN + (nVcn - nVcnTotal);
			break;
		}

		nVcnTotal += pstRunList[i].llLength;
	}

	if (FALSE == bFound)
	{
		// 찾지 못했다.
		// 즉, 아마도 nVcn이 그 범위를 벗어났을 확률이 있다는 뜻이다.
		assert(FALSE);
		goto FINAL;
	}

	// 여기까지 왔다면 성공
	bRtnValue = TRUE;

FINAL:
	return bRtnValue;
}

// 일반적으로 b-tree를 탐색할 때,
// 재귀적 방법이 응용된다.
// 본 함수는, 외부 호출자 또한 재귀 호출을 사용할 가능성이 있는데,
// 내부 함수 마저 재귀호출을 하면 Stack overflow 위험이 있다.
// 또한, 재귀함수 기반의 논리를 가지고,
//
//		FindFirst -> FindNext -> FindNext -> FindNext -> ... -> FindClose
//
// 와 같은 논리를 만들기가 쉽지 않다.
// 재귀함수 기반의 논리라면,
//
//		Start --------------... Long block ...---------------> returned
//              |        |            |           |        |
//              v        v            v           v        v
//           callback  callback    callback    callback callback
//
// 와 같은 논리가 될 가능성이 있다.
// 여하튼, FindFirst / FindNext / FindClose와 같은 비-재귀적 논리를 만들기 위해선,
// 내부 Stack이 필요하다.
// 해당 Stack을 구현한다.
//
// nGrowthCount : STACK_GROWTH_COUNT
//                일반적인 Stack의 Growth 크기. 해당 define의 주석 참고
//
// [Remark]
//
// Stack의 node는 INDEX_ENTRY array가 들어있다.
// 즉, Stack의 node는 index block에 해당된다고 볼 수 있다.
// 만일, 임의의 directory에 001.txt, 002.txt, ..., 100.txt가 있다고 가정하자.
//
// 예를 들어,
// 그럼 해당 부분을 b-tree로 도식화하면, (박스는 index block이라고 가정하자.)
//
//                  ============================================================
//                 || 030.txt  060.txt  090.txt    091.txt 092.txt ... 100.txt ||    // $INDEX_ROOT (030, 060, 090은 sub-node 있음)
//                  ============================================================
//                     ||      |         |
//      ..=============..      |         .________.
//      ||                     |                  |
//      VV                     |                  |
//   ====================   -------------------  -------------------
//  || 010.txt  020.txt ||  | 040.txt 050.txt |  | 070.txt 080.txt |
//   ====================   -------------------  -------------------
//      ||        |
//      ||        .___________________.
//      ||                            |
//      VV                            |
//  ===============================  -----------------------
// || 001.txt 002.txt ... 009.txt || | 011.txt ... 019.txt |
//  ===============================  -----------------------
//
// 와 같을 것이다. (040.txt, 050.txt, 070.txt, 080.txt는 지면 관계상 subnode는 표시하지 않음)
// 두줄로 이뤄진 Link와 box는 현재 Stack의 상황을 표기한 것이다.
//
// 첫번째 FindFirstInode에는 $INDEX_ROOT로 부터 무조건 제일 왼쪽으로 Traverse하여,
//
//			001.txt
//
// 를 전달 한다.
// 그때 Stack은,
//
// -------------------------------  
// | 001.txt 002.txt ... 009.txt | 현지 위치 : 1
// ------------------------------- 
// -------------------- 
// | 010.txt  020.txt | 현재 위치 : 0
// -------------------- 
// ------------------------------------------------------------
// | 030.txt  060.txt  090.txt    091.txt 092.txt ... 100.txt | : 현재 위치 : 0
// ------------------------------------------------------------
//
// 와 같이 구성된다.
// 그다음 FindNextInode 호출시에는,
//
// 		002.txt
//
// 를 전달 하고, Stack은,
//
// -------------------------------  
// | 001.txt 002.txt ... 009.txt | 현지 위치 : 2
// -------------------------------
// -------------------- 
// | 010.txt  020.txt | 현재 위치 : 0
// -------------------- 
// ------------------------------------------------------------
// | 030.txt  060.txt  090.txt    091.txt 092.txt ... 100.txt | : 현재 위치 : 0
// ------------------------------------------------------------
//
// 와 같이 된다.
//
// 만일 009.txt까지 다 갔을 때에는,
// 맨 위의 Stacknode가 pop이 된다.
// 
// -------------------- 
// | 010.txt  020.txt | 현재 위치 : 1
// -------------------- 
// ------------------------------------------------------------
// | 030.txt  060.txt  090.txt    091.txt 092.txt ... 100.txt | : 현재 위치 : 0
// ------------------------------------------------------------
//
// 와 같이 되고 010.txt를 전달한다. [remark.01]
// 그리고, 다시 FindNextInode를 하게되면,
// 020.txt가 있는데, 해당 entry는 subnode가 있으므로, subnode로 이동하게 된다.
// subnode로 이동하게 된다는 것은,
// Stack에 해당 subnode의 index block을 push하는 것이다.
//
// (참고) 여기서 의심할 것은,
//        [remark.01]에서 010.txt는 subnode가 있음에도 push하지 않았다는 것이다.
//        만일, 010.txt subnode를 push하면, 001.txt ~ 009.txt가 다시 list되어,
//        계속 순환하게 된다.
//        따라서, 각 index entry array의 node에서 만일 subnode가 있을 때,
//        해당 subnode를 Stack push하는데, 단, 한번만 하는 조건으로 만든다.
//        즉, 한번 push를 수행한 subnode인 경우에는, push하지 않도록 한다.
//        그래서 [remark.01]에서 010.txt를 push하지 않고 전달만 한 것이다.
//        다시, 아래 tree를 보면, 두 점(:)으로 이뤄진 Link가 있는데
//        해당 Link는 한번 다녀간 부분이기 때문에, 다시 탐색하지 않는다는 것을
//        도식화한 것이다.
//      * 코드에서, ST_INDEX_ENTRY_NODE::bSubnodeTraversed를 참고
//
// 그렇게 되면,
//
//                  ============================================================
//                 || 030.txt  060.txt  090.txt    091.txt 092.txt ... 100.txt ||    // $INDEX_ROOT (30, 60, 90은 sub-node 있음)
//                  ============================================================
//                     ||      |         |
//      ..=============..      |         .________.
//      ||                     |                  |
//      VV                     |                  |
//   ====================   -------------------  -------------------
//  || 010.txt  020.txt ||  | 040.txt 050.txt |  | 070.txt 080.txt |
//   ====================   -------------------  -------------------
//      :         ||
//      :         ..==================..
//      :                             ||
//      :                             VV
// -------------------------------   =======================
// | 001.txt 002.txt ... 009.txt |  || 011.txt ... 019.txt ||
// -------------------------------   =======================
//
// 와 같이 되며, Stack은,
//
// -----------------------
// | 011.txt ... 019.txt | 현재 위치 : 0
// -----------------------
// -------------------- 
// | 010.txt  020.txt | 현재 위치 : 1
// -------------------- 
// ------------------------------------------------------------
// | 030.txt  060.txt  090.txt    091.txt 092.txt ... 100.txt | : 현재 위치 : 0
// ------------------------------------------------------------
// 
// 와 같이 되며, 다음번에 011.txt를 전달할 수 있게 된다.
//
// 이런 논리로 000.txt ~ 100.txt를 모두 Traverse할 수 있게 된다.
CNTFSHelper::LPST_STACK CNTFSHelper::CreateStack(IN INT nGrowthCount)
{
	LPST_STACK	pstRtnValue	= NULL;
	ST_STACK	stStack		= {0,};

	if (0 == nGrowthCount)
	{
		assert(FALSE);
		goto FINAL;
	}

	stStack.nGrowthCount = nGrowthCount;
	stStack.nAllocSize   = nGrowthCount;
	stStack.pstNodeArray = (LPST_STACK_NODE)malloc(sizeof(*stStack.pstNodeArray)*stStack.nAllocSize);
	if (NULL == stStack.pstNodeArray)
	{
		assert(FALSE);
		goto FINAL;
	}
	memset(stStack.pstNodeArray, 0, sizeof(*stStack.pstNodeArray)*stStack.nAllocSize);

	pstRtnValue = (LPST_STACK)malloc(sizeof(*pstRtnValue));
	*pstRtnValue = stStack;

FINAL:
	if (NULL == pstRtnValue)
	{
		// 실패
		if (NULL != stStack.pstNodeArray)
		{
			free(stStack.pstNodeArray);
			stStack.pstNodeArray = NULL;
		}
	}
	return pstRtnValue;
}

// Stack을 삭제한다.
VOID CNTFSHelper::DestroyStack(IN LPST_STACK pstStack)
{
	INT i = 0;

	if (NULL != pstStack)
	{
		if (pstStack->nItemCount > 0)
		{
			// Stack에 항목이 있는 상태에서 Destroy가 들어왔다.
			for (i=0; i<pstStack->nItemCount; i++)
			{
				if (NULL != pstStack->pstNodeArray[i].pstIndexEntryArray)
				{
					GetIndexEntryArrayFree(pstStack->pstNodeArray[i].pstIndexEntryArray);
					pstStack->pstNodeArray[i].pstIndexEntryArray = NULL;
				}
			}
		}

		if (NULL != pstStack->pstNodeArray)
		{
			free(pstStack->pstNodeArray);
			pstStack->pstNodeArray = NULL;
		}

		free(pstStack);
		pstStack = NULL;
	}
}

// Stack을 Pop한다.
// pstNode는 pointer 복사가 아니라, 메모리 복사가 이뤄진다.
// 즉, 호출자는 pstNode에 &stNode를 전달해야 한다.
BOOL CNTFSHelper::StackPop(IN LPST_STACK pstStack, OUT LPST_STACK_NODE pstNode)
{
	BOOL bRtnValue = FALSE;

	if (NULL != pstNode)
	{
		memset(pstNode, 0, sizeof(*pstNode));
	}

	if ((NULL == pstStack) || (NULL == pstStack->pstNodeArray) || (NULL == pstNode))
	{
		assert(FALSE);
		goto FINAL;
	}

	if (TRUE == IsStackEmptry(pstStack))
	{
		// stack 비었음
		goto FINAL;
	}

	if (pstStack->nItemCount > pstStack->nAllocSize)
	{
		assert(FALSE);
		goto FINAL;
	}

	bRtnValue = TRUE;
	*pstNode = pstStack->pstNodeArray[pstStack->nItemCount - 1];
	pstStack->nItemCount--;

FINAL:
	return bRtnValue;
}

// Stack에 Push한다.
BOOL CNTFSHelper::StackPush(IN LPST_STACK pstStack, IN LPST_STACK_NODE pstNode)
{
	BOOL			bRtnValue		= FALSE;
	LPST_STACK_NODE	pstNodeArray	= NULL;

	if ((NULL == pstStack) || (NULL == pstNode) || (NULL == pstStack->pstNodeArray) || (pstStack->nAllocSize <= 0))
	{
		assert(FALSE);
		goto FINAL;
	}

	if (pstStack->nItemCount > pstStack->nAllocSize)
	{
		assert(FALSE);
		goto FINAL;
	}

	if (pstStack->nItemCount == pstStack->nAllocSize)
	{
		// 재할당 해야 한다.
		pstNodeArray = (LPST_STACK_NODE)malloc(sizeof(*pstNodeArray)*(pstStack->nAllocSize + pstStack->nGrowthCount));
		if (NULL == pstNodeArray)
		{
			assert(FALSE);
			goto FINAL;
		}
		memset(pstNodeArray, 0, sizeof(*pstNodeArray)*(pstStack->nAllocSize + pstStack->nGrowthCount));

		// 이전것을 복사한뒤 메로리 해제한다.
		memcpy(pstNodeArray, pstStack->pstNodeArray, sizeof(*pstNodeArray)*(pstStack->nAllocSize));
		free(pstStack->pstNodeArray);
		pstStack->pstNodeArray = pstNodeArray;
		pstStack->nAllocSize += pstStack->nGrowthCount;
	}

	pstStack->nItemCount++;
	pstStack->pstNodeArray[pstStack->nItemCount - 1] = *pstNode;
	bRtnValue = TRUE;

FINAL:
	return bRtnValue;
}

// Stack을 Pop하지는 않고, 맨 윗 항목을 전달한다.
// pstNode는 pointer 복사가 아니라, 메모리 복사가 이뤄진다.
// 즉, 호출자는 pstNode에 &stNode를 전달해야 한다.
BOOL CNTFSHelper::StackGetTop(IN LPST_STACK pstStack, OUT LPST_STACK_NODE pstNode)
{
	BOOL bRtnValue = FALSE;

	if (NULL != pstNode)
	{
		memset(pstNode, 0, sizeof(*pstNode));
	}

	if ((NULL == pstStack) || (NULL == pstNode))
	{
		assert(FALSE);
		goto FINAL;
	}

	if (TRUE == IsStackEmptry(pstStack))
	{
		assert(FALSE);
		goto FINAL;
	}

	// 맨위에 있는 항목을 전달한다.
	*pstNode = pstStack->pstNodeArray[pstStack->nItemCount-1];

	// 여기까지 왔다면 성공
	bRtnValue = TRUE;

FINAL:
	return bRtnValue;
}

// Stack이 비었는가?
BOOL CNTFSHelper::IsStackEmptry(IN LPST_STACK pstStack)
{
	BOOL bRtnValue = FALSE;

	if (FALSE == pstStack)
	{
		assert(FALSE);
		goto FINAL;
	}

	if (pstStack->nItemCount <= 0)
	{
		bRtnValue = TRUE;
	}

FINAL:
	return bRtnValue;
}

// FindFirstFile
//
// 성공시 LPST_WIN32_HANDLE 메모리 전달 (내부 값을 변경하지 마시오)
// 실패시 INVALID_HANDLE_VALUE_NTFS(=INVALID_HANDLE_VALUE와 동일)값 전달
//
// 참고) - FindFirstFile에 Wile-card(*,?)등은 msdn에 명시된 스펙처럼 지원되지 않는다.
//         즉, *.* 혹은 *만 지원한다. 그 이외의 값이 들어오면 ERROR_NOT_SUPPORTED를 SetLastError한다.
//         물론, *.* 혹은 *는 요청하는 경로 끝에 와야 한다.
//
//		 - Win32 API에서는 *.* / *를 사용하여 sub-directory를 검색할 때에는,
//		   .
//		   ..
//		   두 경로가 우선 탐색되는것이 지원됨. (Revision 7 부터 지원됨)
//
//		 - Win32 API에서 "C:"를 전달하면, IDE 값을 전달하는데,
//		   본 함수는 지원하지 않는다.
//       
//       - \\?\ prefix는 사용가능하다. (역시 내부적으로는 무시한다)
//
//       - WIN32_FIND_DATA의 다음 값들은 지원되지 않는다.
//         cAlternateFileName / dwReserved0 / dwReserved1
//
// [TBD] multi-platform (ex, linux) 준비한다.
CNTFSHelper::NTFS_HANDLE WINAPI CNTFSHelper::FindFirstFile(IN LPCWSTR lpFileName, OUT LPWIN32_FIND_DATAW lpFindFileData)
{
	// 외부 정의 I/O 없이 전달한다.
	return FindFirstFileExt(lpFileName, lpFindFileData, NULL, NULL);
}

// 외부 I/O 정의된 FindFirstFile 함수
CNTFSHelper::NTFS_HANDLE CNTFSHelper::FindFirstFileExt(IN LPCWSTR lpFileName, OUT LPWIN32_FIND_DATAW lpFindFileData, OPTIONAL IN LPST_IO_FUNCTION pstIoFunction, OPTIONAL IN LPST_INODE_CACHE pstInodeCache)
{
	NTFS_HANDLE			pHandle				= INVALID_HANDLE_VALUE_NTFS;
	LPST_NTFS			pstNtfs				= NULL;
 	BOOL				bFindSubDir			= FALSE;
	DWORD				dwErrorCode			= ERROR_INVALID_PARAMETER;
	LPWSTR				lpszPath			= NULL;
	TYPE_INODE			nInode				= 0;
	LPST_INODEDATA		pstInodeDataAlloc	= {0,};

	if (NULL != lpFindFileData)
	{
		memset(lpFindFileData, 0, sizeof(*lpFindFileData));
	}

	if ((NULL == lpFileName) || (NULL == lpFindFileData))
	{
		assert(FALSE);
		goto FINAL;
	}

	// 본 함수는 대체로 caller에 의해 재귀적 호출이 발생할 수 있다.
	// 따라서 되도록이면, Big size의 로컬 변수 사용을 금지한다. (Stack overflow 방지)
	// 동적으로 할당받아 사용하자.
	lpszPath			= (LPWSTR)malloc(sizeof(WCHAR) * (MAX_PATH_NTFS + 1));
	pstInodeDataAlloc	= (LPST_INODEDATA)malloc(sizeof(*pstInodeDataAlloc));
	if ((NULL == lpszPath) || (NULL == pstInodeDataAlloc))
	{
		dwErrorCode = ERROR_NOT_ENOUGH_MEMORY;
		assert(FALSE);
		goto FINAL;
	}
	memset(lpszPath, 0, sizeof(WCHAR) * (MAX_PATH_NTFS + 1));
	memset(pstInodeDataAlloc, 0, sizeof(*pstInodeDataAlloc));

	// Ntfs를 연다.
	pstNtfs = FindFirstFileOpenNtfs(lpFileName, lpszPath, MAX_PATH_NTFS, pstIoFunction, pstInodeCache, &dwErrorCode, &bFindSubDir);
	if (NULL == pstNtfs)
	{
		assert(FALSE);
		goto FINAL;
	}

	// Path의 Inode를 찾는다.
	// FindInodeFromPath 함수는 시간이 제법 걸리는 함수이다.
	/*
	if (TRUE == bFindSubDir)
	{
		// Inode값만 찾는다.
		dwErrorCode = FindInodeFromPath(pstNtfs, lpszPath, &nInode, pstInodeDataAlloc, NULL);
	}
	else
	{
		// 만일, *.*가 사용되지 않아,
		// 단일 파일 정보 획득용으로 호출된 경우,...
		// Inode 뿐만 아니라, 확장 Data를 구한다.
		dwErrorCode = FindInodeFromPath(pstNtfs, lpszPath, &nInode, pstInodeDataAlloc);
	}
	*/

	dwErrorCode = FindInodeFromPath(pstNtfs, lpszPath, &nInode, pstInodeDataAlloc);
	
	if (ERROR_SUCCESS != dwErrorCode)
	{
		if (ERROR_FILE_NOT_FOUND != dwErrorCode)
		{
			assert(FALSE);
		}
		goto FINAL;
	}

	// 이제, FindFirstFileMain 함수를 호출한다.
	/*
	if (TRUE == bFindSubDir)
	{
		// Sub-directory를 찾을 때는,
		// 확장 Data는 필요없다.
		pHandle = FindFirstFileMain(pstNtfs, pstInodeDataAlloc, NULL, bFindSubDir, NULL, NULL, lpFindFileData, &dwErrorCode);
	}
	else
	{
		// 단일 파일 정보 획득용으로 호출되는 경우,...
		// Inode 뿐만 아니라, 확장 Data도 전달해야 한다.
		pHandle = FindFirstFileMain(pstNtfs, pstInodeDataAlloc, bFindSubDir, NULL, NULL, lpFindFileData, &dwErrorCode);
	}
	*/

	pHandle = FindFirstFileMain(pstNtfs, pstInodeDataAlloc, bFindSubDir, NULL, lpFindFileData, &dwErrorCode);

FINAL:
	if (NULL != lpszPath)
	{
		free(lpszPath);
		lpszPath = NULL;
	}
	
	if (NULL != pstInodeDataAlloc)
	{
		free(pstInodeDataAlloc);
		pstInodeDataAlloc = NULL;
	}

	if (INVALID_HANDLE_VALUE_NTFS == pHandle)
	{
		if (NULL != pstNtfs)
		{
			CloseNtfs(pstNtfs);
			pstNtfs = NULL;
		}

		// 오류 발생
		::SetLastError(dwErrorCode);

		if (NULL != lpFindFileData)
		{
			memset(lpFindFileData, 0, sizeof(*lpFindFileData));
		}
	}
	return pHandle;
}

// FindFirstFile 함수군에서 사용될 lpFileName을 받아,
// ntfs를 열어서 전달한다.
// 사용될 lpFileName 경로가 lpszPath로 전달된다. (\\?\, *.* 등이 제거됨)
CNTFSHelper::LPST_NTFS CNTFSHelper::FindFirstFileOpenNtfs(IN LPCWSTR lpFileName, OUT LPWSTR lpszPath, IN DWORD dwCchPath, OPTIONAL IN LPST_IO_FUNCTION pstIoFunction, OPTIONAL IN LPST_INODE_CACHE pstInodeCache, OUT LPDWORD pdwErrorCode, OPTIONAL OUT PBOOL pbFindSubDir)
{
	LPST_NTFS	pstRtnValue	= NULL;
	BOOL		bFindSubDir	= FALSE;
	CHAR		szVolume[7]	= {0,};

	if (NULL != pdwErrorCode)
	{
		*pdwErrorCode = ERROR_INVALID_PARAMETER;
	}

	if (NULL != pbFindSubDir)
	{
		*pbFindSubDir = FALSE;
	}

	if ((NULL == lpFileName) || (NULL == pdwErrorCode) || (NULL == lpszPath))
	{
		assert(FALSE);
		goto FINAL;
	}

	// _ 문자는 앞으로 a~z 사이의 값으로 치환될 것임
	StringCchCopyA(szVolume, 7, "\\\\.\\_:");

	// *.*나 \\?\등이 제거된 경로를 구한다.
	*pdwErrorCode = GetFindFirstFilePath(lpFileName, lpszPath, dwCchPath, &szVolume[4], &bFindSubDir);
	if (ERROR_SUCCESS != *pdwErrorCode)
	{
		assert(FALSE);
		goto FINAL;
	}

	// Ntfs를 Open한다.
	if (NULL == pstIoFunction)
	{
		pstRtnValue = OpenNtfs(szVolume);
	}
	else
	{
		// 외부 I/O 지원
		pstRtnValue = OpenNtfsIo(szVolume, pstIoFunction);
	}

	if (NULL != pstInodeCache)
	{
		if (FALSE == SetInodeCache(pstRtnValue, pstInodeCache->pfnCreateInodeCache, pstInodeCache->pfnGetCache, pstInodeCache->pfnSetCache, pstInodeCache->pfnDestroyCache, pstInodeCache->pUserContext))
		{
			// Cache가 실패됨
			CloseNtfs(pstRtnValue);
			pstRtnValue = NULL;
			assert(FALSE);
			goto FINAL;
		}
	}

	if (NULL == pstRtnValue)
	{
		*pdwErrorCode = ERROR_OPEN_FAILED;
		assert(FALSE);
		goto FINAL;
	}

	if (NULL != pbFindSubDir)
	{
		*pbFindSubDir = bFindSubDir;
	}

	// 성공
	*pdwErrorCode = ERROR_SUCCESS;

FINAL:
	return pstRtnValue;
}

// FindFirstFile과 유사하나. Inode 정보를 받는다.
// 그렇기 때문에, 속도가 향상된다.
//
// 해당 Inode 정보는, FindFirstFile을 통해 얻어진 NTFS_HANDLE을 가지고,
// FindNextFileWithInode(...)를 호출하면, 현재 위치의 파일에 대한 Inode 정보를
// 가져올 수 있다.
//
// bFindSubDir는 lpVolumePath에 정확한 의미를 받아들이지 않기 때문에 발생하였다.
// 만일 lpVolumePath에 C:\*.* 나 C:\*와 같이 하부 Directory를 list해야 한다면,
// TRUE를 전달한다. 단말 File 혹은 Directory의 정보를 원한다면 FALSE를 전달한다.
// lpFileName은 Ntfs을 Open할 Volume을 구하는데에만 사용된다. 즉, C:냐 D:\냐 이런 정보만 가져온다.
// (lpFileName은 \\.\C:와 같이 Naming된 경우도 허용한다.)
// 즉, C:\\만 선언되어도 무방한다.
// 만일,
//
//			FindFirstFileByInode(L"C:\\Data", &stInode, &stInodeEx, TRUE, ...)
//
// 와 같이 호출하였다고 가정하자.
// stInode가 C:\Windows\system32에 해당되었다면,
//
//			FindFirstFile(L"C:\Windows\system32\*.*", ...)
//
// 와 같은 효과가 있다. 물론, Inode를 통한 경우가 속도가 빠르다.
// 
// 외부 I/O를 사용하려면 pstIoFunction 값을 채워 전달한다. 물론, 일반적인 경우 NULL 가능하다.
CNTFSHelper::NTFS_HANDLE CNTFSHelper::FindFirstFileByInodeExt(IN LPCWSTR lpVolumePath, IN LPST_INODEDATA pstInodeData, IN BOOL bFindSubDir, OPTIONAL OUT LPST_INODEDATA pstSubDirInodeData, OUT LPWIN32_FIND_DATAW lpFindFileData, OPTIONAL IN LPST_IO_FUNCTION pstIoFunction, OPTIONAL IN LPST_INODE_CACHE pstInodeCache)
{
	NTFS_HANDLE	pHandle		= INVALID_HANDLE_VALUE_NTFS;
	LPST_NTFS	pstNtfs		= NULL;
	DWORD		dwErrorCode	= ERROR_INVALID_PARAMETER;
	LPWSTR		lpszPath	= NULL;
	TYPE_INODE	nInode		= 0;

	if (NULL != lpFindFileData)
	{
		memset(lpFindFileData, 0, sizeof(*lpFindFileData));
	}

	if ((NULL == lpVolumePath) || (NULL == pstInodeData) || (NULL == lpFindFileData))
	{
		assert(FALSE);
		goto FINAL;
	}

	lpszPath = (LPWSTR)malloc(sizeof(WCHAR) * (MAX_PATH_NTFS + 1));
	if (NULL == lpszPath)
	{
		dwErrorCode = ERROR_NOT_ENOUGH_MEMORY;
		assert(FALSE);
		goto FINAL;
	}
	memset(lpszPath, 0, sizeof(WCHAR) * (MAX_PATH_NTFS + 1));

	// Ntfs를 연다.
	pstNtfs = FindFirstFileOpenNtfs(lpVolumePath, lpszPath, MAX_PATH_NTFS, pstIoFunction, pstInodeCache, &dwErrorCode, NULL);
	if (NULL == pstNtfs)
	{
		assert(FALSE);
		goto FINAL;
	}

	// Path의 Inode를 찾지 않는다.
	// 호출시 넘어온 inode 정보를 가지고 FindFirstFileMain을 호출한다.
	// 즉, FindInodeFromPath를 호출하지 않아 시간을 절약시킨다.

	// 이제, FindFirstFileMain 함수를 호출한다.
	/*
	if (TRUE == bFindSubDir)
	{
		// Sub-directory를 찾을 때는,
		// 확장 Data는 필요없다.
		pHandle = FindFirstFileMain(pstNtfs, pstInodeData, NULL, bFindSubDir, pstSubDirInodeData, lpFindFileData, &dwErrorCode);
	}
	else
	{
		// 단일 파일 정보 획득용으로 호출되는 경우,...
		// Inode 뿐만 아니라, 확장 Data도 전달해야 한다.
		pHandle = FindFirstFileMain(pstNtfs, pstInodeData, bFindSubDir, pstSubDirInodeData, lpFindFileData, &dwErrorCode);
	}
	*/

	pHandle = FindFirstFileMain(pstNtfs, pstInodeData, bFindSubDir, pstSubDirInodeData, lpFindFileData, &dwErrorCode);

FINAL:

	if (NULL != lpszPath)
	{
		free(lpszPath);
		lpszPath = NULL;
	}

	if (INVALID_HANDLE_VALUE_NTFS == pHandle)
	{
		if (NULL != pstNtfs)
		{
			CloseNtfs(pstNtfs);
			pstNtfs = NULL;
		}

		// 오류 발생
		::SetLastError(dwErrorCode);

		if (NULL != lpFindFileData)
		{
			memset(lpFindFileData, 0, sizeof(*lpFindFileData));
		}
	}
	return pHandle;
}

// FindFirstFile과 유사하고,
// 찾은 항목의 Inode 값을 전달한다.
// 외부 I/O 정의가 지원되는 FindFirstFileWithInode 함수
CNTFSHelper::NTFS_HANDLE CNTFSHelper::FindFirstFileWithInodeExt(IN LPCWSTR lpFileName, OUT LPWIN32_FIND_DATAW lpFindFileData, OUT LPST_INODEDATA pstInodeData, OPTIONAL IN LPST_IO_FUNCTION pstIoFunction, OPTIONAL IN LPST_INODE_CACHE pstInodeCache)
{
	NTFS_HANDLE			pHandle						= INVALID_HANDLE_VALUE_NTFS;
	LPST_NTFS			pstNtfs						= NULL;
	BOOL				bFindSubDir					= FALSE;
	DWORD				dwErrorCode					= ERROR_INVALID_PARAMETER;
	LPWSTR				lpszPath					= NULL;
	TYPE_INODE			nInode						= 0;
	LPST_INODEDATA		pstFileNameInodeDataAlloc	= NULL;

	if (NULL != lpFindFileData)
	{
		memset(lpFindFileData, 0, sizeof(*lpFindFileData));
	}

	if (NULL != pstInodeData)
	{
		memset(pstInodeData, 0, sizeof(*pstInodeData));
	}

	if ((NULL == lpFileName) || (NULL == lpFindFileData) || (NULL == pstInodeData))
	{
		assert(FALSE);
		goto FINAL;
	}

	// 본 함수는 대체로 caller에 의해 재귀적 호출이 발생할 수 있다.
	// 따라서 되도록이면, Big size의 로컬 변수 사용을 금지한다. (Stack overflow 방지)
	// 동적으로 할당받아 사용하자.
	lpszPath					= (LPWSTR)malloc(sizeof(WCHAR) * (MAX_PATH_NTFS + 1));
	pstFileNameInodeDataAlloc	= (LPST_INODEDATA)malloc(sizeof(*pstFileNameInodeDataAlloc));
	if ((NULL == lpszPath) || (NULL == pstFileNameInodeDataAlloc))
	{
		dwErrorCode = ERROR_NOT_ENOUGH_MEMORY;
		assert(FALSE);
		goto FINAL;
	}
	memset(lpszPath, 0, sizeof(WCHAR) * (MAX_PATH_NTFS + 1));
	memset(pstFileNameInodeDataAlloc, 0, sizeof(*pstFileNameInodeDataAlloc));

	// Ntfs를 연다.
	pstNtfs = FindFirstFileOpenNtfs(lpFileName, lpszPath, MAX_PATH_NTFS, pstIoFunction, pstInodeCache, &dwErrorCode, &bFindSubDir);
	if (NULL == pstNtfs)
	{
		assert(FALSE);
		goto FINAL;
	}

	// Path의 Inode를 찾는다.
	// FindInodeFromPath 함수는 시간이 제법 걸리는 함수이다.
	/*
	if (TRUE == bFindSubDir)
	{
		// Inode값만 찾는다.
		dwErrorCode = FindInodeFromPath(pstNtfs, lpszPath, &nInode, pstFileNameInodeDataAlloc, NULL);
	}
	else
	{
		// 만일, *.*가 사용되지 않아,
		// 단일 파일 정보 획득용으로 호출된 경우,...
		// Inode 뿐만 아니라, 확장 Data를 구한다.
		dwErrorCode = FindInodeFromPath(pstNtfs, lpszPath, &nInode, pstFileNameInodeDataAlloc);
	}
	*/

	dwErrorCode = FindInodeFromPath(pstNtfs, lpszPath, &nInode, pstFileNameInodeDataAlloc);

	if (ERROR_SUCCESS != dwErrorCode)
	{
		if (ERROR_FILE_NOT_FOUND != dwErrorCode)
		{
			assert(FALSE);
		}
		goto FINAL;
	}

	// 이제, FindFirstFileMain 함수를 호출한다.
	/*
	if (TRUE == bFindSubDir)
	{
		// Sub-directory를 찾을 때는,
		// 확장 Data는 필요없다.
		pHandle = FindFirstFileMain(pstNtfs, pstFileNameInodeDataAlloc, NULL, bFindSubDir, pstInodeData, lpFindFileData, &dwErrorCode);
	}
	else
	{
		// 단일 파일 정보 획득용으로 호출되는 경우,...
		// Inode 뿐만 아니라, 확장 Data도 전달해야 한다.
		pHandle = FindFirstFileMain(pstNtfs, pstFileNameInodeDataAlloc, bFindSubDir, pstInodeData, lpFindFileData, &dwErrorCode);
	}
	*/

	pHandle = FindFirstFileMain(pstNtfs, pstFileNameInodeDataAlloc, bFindSubDir, pstInodeData, lpFindFileData, &dwErrorCode);

FINAL:

	if (NULL != lpszPath)
	{
		free(lpszPath);
		lpszPath = NULL;
	}

	if (NULL != pstFileNameInodeDataAlloc)
	{
		free(pstFileNameInodeDataAlloc);
		pstFileNameInodeDataAlloc = NULL;
	}

	if (INVALID_HANDLE_VALUE_NTFS == pHandle)
	{
		if (NULL != pstNtfs)
		{
			CloseNtfs(pstNtfs);
			pstNtfs = NULL;
		}

		// 오류 발생
		::SetLastError(dwErrorCode);

		if (NULL != lpFindFileData)
		{
			memset(lpFindFileData, 0, sizeof(*lpFindFileData));
		}
	}
	return pHandle;
}

// FindFirstFile, FindFirstFileByInode, FindFirstFileWithInode
// 에서 호출하는 실제 Main Body 함수
//
// - lpszVolumeA는 \\.\C: 와 같은 char ansi string을 받아들인다.
//
// - Win32 API의 FindFirstFile("C:\*.*", ...)와 같이 *.*이나 *가 포함되어,
//   Sub-directory를 탐색해야 하는 경우, bFindSubDir에 TRUE를 전달한다.
//
// - 실패시 INVALID_HANDLE_VALUE_NTFS가 리턴되는데,
//   이때 pdwErrorCode 값은 ERROR_SUCCESS가 아님이 보장된다.
//
// - Sub-directory를 탐색하는 경우, 호출자 요청에 따라,
//   Sub-directory의 첫 항목에 해당되는 inode 정보를
//   pstSubDirInodeData에 전달한다.
//   각각 NULL 가능하다.
//   (bFindSubDir가 FALSE인 경우, pstSubDirInodeData는,
//    Sub-directory가 아니라 자기 자신을 의미한다.)
CNTFSHelper::NTFS_HANDLE CNTFSHelper::FindFirstFileMain(IN LPST_NTFS pstNtfs, IN LPST_INODEDATA pstInodeData, IN BOOL bFindSubDir, OPTIONAL OUT LPST_INODEDATA pstSubDirInodeData, OUT LPWIN32_FIND_DATAW lpFindFileData, OUT LPDWORD pdwErrorCode)
{
	NTFS_HANDLE			pHandle						= INVALID_HANDLE_VALUE_NTFS;
	ST_WIN32_HANDLE		stHandle					= {0,};
	TYPE_INODE			nInode						= 0;
	LPST_INODEDATA		pstSubDirInodeDataAlloc		= NULL;

	if (NULL != lpFindFileData)
	{
		memset(lpFindFileData, 0, sizeof(*lpFindFileData));
	}

	if (NULL != pdwErrorCode)
	{
		*pdwErrorCode = ERROR_SUCCESS;
	}

	if ((NULL == pstNtfs) || (NULL == lpFindFileData) || (NULL == pstInodeData) || (NULL == pdwErrorCode))
	{
		if (NULL != pdwErrorCode)
		{
			*pdwErrorCode = ERROR_INVALID_PARAMETER;
		}

		assert(FALSE);
		goto FINAL;
	}

	// FindFirstFile 계열 함수에서,
	// 내부에서 사용될 경로 관련 임시 저장소 생성
	stHandle.stFindFirstFile.lpszNameTmp = (PWCHAR)malloc(sizeof(*stHandle.stFindFirstFile.lpszNameTmp)*MAX_PATH_NTFS);
	if (NULL == stHandle.stFindFirstFile.lpszNameTmp)
	{
		*pdwErrorCode = ERROR_NOT_ENOUGH_MEMORY;
		assert(FALSE);
		goto FINAL;
	}
	memset(stHandle.stFindFirstFile.lpszNameTmp, 0, sizeof(*stHandle.stFindFirstFile.lpszNameTmp)*MAX_PATH_NTFS);

	// Ntfs를 가져온다.
	stHandle.pstNtfs = pstNtfs;
	if (NULL == stHandle.pstNtfs)
	{
		*pdwErrorCode = ERROR_OPEN_FAILED;
		assert(FALSE);
		goto FINAL;
	}

	// 호출자의 요청에 따라 플레그를 설정한다.
	stHandle.stFindFirstFile.bFindSubDir = bFindSubDir;

	// Path의 Inode를 찾는다.
	if (TRUE == stHandle.stFindFirstFile.bFindSubDir)
	{
		// Inode 값을 전달받는다.
		nInode = pstInodeData->nInode;

		pstSubDirInodeDataAlloc = (LPST_INODEDATA)malloc(sizeof(*pstSubDirInodeDataAlloc));
		if (NULL == pstSubDirInodeDataAlloc)
		{
			*pdwErrorCode = ERROR_NOT_ENOUGH_MEMORY;
			assert(FALSE);
			goto FINAL;
		}
		memset(pstSubDirInodeDataAlloc, 0, sizeof(*pstSubDirInodeDataAlloc));
	}
	else
	{
		// 만일, *.*가 사용되지 않아,
		// 단일 파일 정보 획득용으로 호출된 경우,...
		// do nothing,...
	}

	if ((TRUE			== stHandle.stFindFirstFile.bFindSubDir) &&
		(TYPE_INODE_DOT	!= pstInodeData->nInode))
	{
		// 만일, subdirectory 검색,
		// 즉, FindFirstFile("C:\...\*", ...)
		// 형식이 호출되었다면,
		// .
		// ..
		// 가 우선 Find되도록 한다.
		// 단, C:\*는 .과 ..이 호출되지 않았음으로 예외 처리한다.
		//
		// 만일, FindFirstFile이나 FindNextFile 호출시,
		// . 혹은 .. 가
		// 필요하지 않다면, 아래 부분을 주석 처리하면 된다.
		stHandle.stFindFirstFile.bUseFindDot = TRUE;
	}

	if (TRUE == stHandle.stFindFirstFile.bFindSubDir)
	{
		// 만일 *.*를 사용하여, Sub-directory까지 검색해야 한다면,...

		if (TRUE == stHandle.stFindFirstFile.bUseFindDot)
		{
			*pstSubDirInodeDataAlloc = *pstInodeData;

			// 파일 이름을 강제로 '.'로 변경한다.
			stHandle.stFindFirstFile.lpszNameTmp[0] = L'.';
			stHandle.stFindFirstFile.lpszNameTmp[1] = L'\0';

			// . 한개 Find 됨
			stHandle.stFindFirstFile.nDotCount = 1;

			// ..에 해당되는 parent inode를 저장한다.
			stHandle.stFindFirstFile.nInodeParent = pstInodeData->llParentDirMftRef &  0x0000FFFFFFFFFFFFUL;

			// 시작 inode를 저장한다.
			stHandle.stFindFirstFile.nInodeStart = pstInodeData->nInode;

			// .. 에 전달될 값을 저장한다.
			stHandle.stFindFirstFile.dotdot_dwFileAttributes	= pstInodeData->dwFileAttrib;
			stHandle.stFindFirstFile.dotdot_ftCreationTime		= GetLocalFileTime(pstInodeData->llCreationTime);
			stHandle.stFindFirstFile.dotdot_ftLastAccessTime	= GetLocalFileTime(pstInodeData->llLastAccessTime);
			stHandle.stFindFirstFile.dotdot_ftLastWriteTime		= GetLocalFileTime(pstInodeData->llLastDataChangeTime);
		}
		else
		{
			// . 처리가 아닌 경우,...

			// 첫 항목을 찾는다.
			stHandle.stFindFirstFile.pstFind = FindFirstInode(stHandle.pstNtfs, nInode, pstSubDirInodeDataAlloc, stHandle.stFindFirstFile.lpszNameTmp, MAX_PATH_NTFS);
			if (NULL == stHandle.stFindFirstFile.pstFind)
			{
				// 첫 항목을 찾지 못했다.
				// C:\aaaa
				// 에 아무런 파일이나 디렉토리가 없을때,
				// FindFirstFile하면 여기에 해당되었다.
				// assert하지는 않는다.
				stHandle.stFindFirstFile.bEmptryDirectory = TRUE;
				*pdwErrorCode = ERROR_NO_MORE_FILES;
				goto FINAL;
			}

			for (;;)
			{
				// PROGRA~1(dos path), $MFT(meta-data file)
				// 와 같연 경로를 Filter한다.
				if (TRUE == FilterFindFirstFile(pstSubDirInodeDataAlloc))
				{
					// 조건에 만족한다.
					break;
				}

				// 조건에 맞지 않는 경우, 조건에 맞는 inode가 나타날때까지
				// Next한다.
				if (FALSE == FindNextInode(stHandle.stFindFirstFile.pstFind, pstSubDirInodeDataAlloc, stHandle.stFindFirstFile.lpszNameTmp, MAX_PATH_NTFS))
				{
					// 없다...
					// 이런 경우 가능한가?
					// 즉, FindFirstInode는 성공했는데,
					// Filter를 통해서는 한개도 나오지 않았을때...
					*pdwErrorCode = ERROR_NO_MORE_FILES;
					assert(FALSE);
					goto FINAL;
				}
			}
		}
	}

	if (TRUE == stHandle.stFindFirstFile.bFindSubDir)
	{
		// 만일 *.*를 사용하여, Sub-directory까지 검색해야 한다면,...

		if (NULL != pstSubDirInodeData)
		{
			*pstSubDirInodeData = *pstSubDirInodeDataAlloc;
		}
	}
	else
	{
		// 만일 단말 파일 조회인 경우,...
		// 사실, Subnode 정보가 아니라, 자신의 정보이다.
		// 전달 받은 inode 정보를 그대로 전달한다.
		if (NULL != pstSubDirInodeData)
		{
			*pstSubDirInodeData = *pstInodeData;
		}
	}

	// Handle의 종류를 선택
	stHandle.nType = TYPE_WIN32_HANDLE_FINDFIRSTFILE;

	// Lock Object 생성
	stHandle.pLock = CREATE_LOCK();

	pHandle = (LPST_WIN32_HANDLE)malloc(sizeof(ST_WIN32_HANDLE));
	if (NULL == pHandle)
	{
		*pdwErrorCode = ERROR_NOT_ENOUGH_MEMORY;
		pHandle = INVALID_HANDLE_VALUE_NTFS;
		assert(FALSE);
		goto FINAL;
	}
	memset(pHandle, 0, sizeof(ST_WIN32_HANDLE));

	// WIN32_FIND_DATA 값을 전달한다.
	if (TRUE == stHandle.stFindFirstFile.bFindSubDir)
	{
		// Sub-directory의 inode 정보로 전달한다.
		if (FALSE == GetWin32FindData(pstSubDirInodeDataAlloc, stHandle.stFindFirstFile.lpszNameTmp, lpFindFileData))
		{
			*pdwErrorCode = ERROR_INTERNAL_ERROR;
			assert(FALSE);
			goto FINAL;
		}
	}
	else
	{
		// 현재 inode 정보로 전달한다.
		if (FALSE == GetWin32FindData(pstInodeData, stHandle.stFindFirstFile.lpszNameTmp, lpFindFileData))
		{
			*pdwErrorCode = ERROR_INTERNAL_ERROR;
			assert(FALSE);
			goto FINAL;
		}
	}

	// Handle 값 복사
	*pHandle = stHandle;

FINAL:
	if (INVALID_HANDLE_VALUE_NTFS == pHandle)
	{
		if (NULL != pdwErrorCode)
		{
			if (ERROR_SUCCESS == *pdwErrorCode)
			{
				*pdwErrorCode = ERROR_INTERNAL_ERROR;
			}
		}

		if (NULL != lpFindFileData)
		{
			memset(lpFindFileData, 0, sizeof(*lpFindFileData));
		}

		if (NULL != stHandle.stFindFirstFile.lpszNameTmp)
		{
			free(stHandle.stFindFirstFile.lpszNameTmp);
			stHandle.stFindFirstFile.lpszNameTmp = NULL;
		}

		if (NULL != stHandle.pLock)
		{
			DESTROY_LOCK(stHandle.pLock);
			stHandle.pLock = NULL;
		}
	}

	if (NULL != pstSubDirInodeDataAlloc)
	{
		free(pstSubDirInodeDataAlloc);
		pstSubDirInodeDataAlloc = NULL;
	}

	return pHandle;
}

// Win32 API의 FindNextFile과 동일하게 구현된다.
BOOL CNTFSHelper::FindNextFile(IN NTFS_HANDLE hFindFile, OUT LPWIN32_FIND_DATA lpFindFileData)
{
	// Inode 정보 요청없이 FindNextFileMain 함수를 실행한다.
	// 해당 함수 내에서, LOCK/UNLOCK이 수행된다.
	return FindNextFileMain(hFindFile, lpFindFileData, NULL);
}

// FindNextFile과 유사하나,
// Find한 항목의 Inode 정보도 함께 전달한다.
BOOL CNTFSHelper::FindNextFileWithInodeExt(IN NTFS_HANDLE hFindFile, OUT LPWIN32_FIND_DATA lpFindFileData, OUT LPST_INODEDATA pstInodeData)
{
	// Inode 정보 요청하였다.
	if (NULL == pstInodeData)
	{
		::SetLastError(ERROR_INVALID_PARAMETER);
		assert(FALSE);
		return FALSE;
	}

	// Inode 정보 요청한다.
	// 해당 함수 내에서, LOCK/UNLOCK이 수행된다.
	return FindNextFileMain(hFindFile, lpFindFileData, pstInodeData);
}

BOOL CNTFSHelper::FindNextFileMain(IN NTFS_HANDLE hFindFile, OUT LPWIN32_FIND_DATA lpFindFileData, OPTIONAL OUT LPST_INODEDATA pstInodeData)
{
	BOOL				bRtnValue			= FALSE;
	DWORD				dwErrorCode			= ERROR_INVALID_PARAMETER;
	LPST_WIN32_HANDLE	pstHandle			= NULL;
	LPST_INODEDATA		pstInodeDataAlloc	= NULL;

	if (NULL != lpFindFileData)
	{
		memset(lpFindFileData, 0, sizeof(*lpFindFileData));
	}

	if ((INVALID_HANDLE_VALUE_NTFS == hFindFile) || (NULL == hFindFile) || (NULL == lpFindFileData))
	{
		// Invalid handle
		if (INVALID_HANDLE_VALUE_NTFS == hFindFile)
		{
			dwErrorCode = ERROR_INVALID_HANDLE;
		}
		// 굳이 assert 하지는 말자.
		// assert(FALSE);
		// Lock으로 인해 goto FINAL 대신 바로 return 한다.
		return dwErrorCode;
	}

	pstHandle = (LPST_WIN32_HANDLE)hFindFile;
	if (TYPE_WIN32_HANDLE_FINDFIRSTFILE != pstHandle->nType)
	{
		// 잘못된 Handle이 들어옴
		dwErrorCode = ERROR_INVALID_HANDLE;
		assert(FALSE);
		// goto FINAL;
		// Lock으로 인해 goto FINAL 대신 바로 return 한다.
		return dwErrorCode;
	}

	pstInodeDataAlloc = (LPST_INODEDATA)malloc(sizeof(*pstInodeDataAlloc));
	if (NULL == pstInodeDataAlloc)
	{
		dwErrorCode = ERROR_NOT_ENOUGH_MEMORY;
		assert(FALSE);
		// goto FINAL;
		// Lock으로 인해 goto FINAL 대신 바로 return 한다.
		if (NULL != pstInodeDataAlloc)
		{
			free(pstInodeDataAlloc);
			pstInodeDataAlloc = NULL;
		}

		return dwErrorCode;
	}
	memset(pstInodeDataAlloc, 0, sizeof(*pstInodeDataAlloc));

	//////////////////////////////////////////////////////////////////////////
	// LOCK
	LOCK(pstHandle->pLock);

	if (FALSE == pstHandle->stFindFirstFile.bFindSubDir)
	{
		// 만일 FindFirstFile에 *나 *.*가 사용되지 않아,
		// 단말 파일만 하는 경우엔, FindNextFile은 진행하지 않는다.
		dwErrorCode = ERROR_NO_MORE_FILES;
		goto FINAL;
	}

	if ((TRUE	== pstHandle->stFindFirstFile.bUseFindDot) &&
		(1		== pstHandle->stFindFirstFile.nDotCount))
	{
		// 만일, FindFirstFile에 . 와 .. 가 탐색되는 경우에
		// . 까지 탐색된 경우에,...
		// 이제 ..가 탐색되도록 해야 한다.
		lpFindFileData->cFileName[0] = L'.';
		lpFindFileData->cFileName[1] = L'.';
		lpFindFileData->cFileName[2] = L'\0';

		lpFindFileData->dwFileAttributes	= pstHandle->stFindFirstFile.dotdot_dwFileAttributes;
		lpFindFileData->ftCreationTime		= pstHandle->stFindFirstFile.dotdot_ftCreationTime;
		lpFindFileData->ftLastAccessTime	= pstHandle->stFindFirstFile.dotdot_ftLastAccessTime;
		lpFindFileData->ftLastWriteTime		= pstHandle->stFindFirstFile.dotdot_ftLastWriteTime;

		// 이제 .. 처리했다.
		pstHandle->stFindFirstFile.nDotCount = 2;

		// 성공
		bRtnValue = TRUE;
		goto FINAL;
	}

	if (2 == pstHandle->stFindFirstFile.nDotCount)
	{
		// .. 처리 이후의 FindNextFile을 실행한다.
		if (NULL == pstHandle->stFindFirstFile.pstFind)
		{
			// NULL이 당연하다.
			// 아직, 정식 FindFirstFile, FindNextFile이 이뤄지지 않았기 때문이다.
			// good
		}
		else
		{
			// bad
			dwErrorCode = ERROR_INTERNAL_ERROR;
			assert(FALSE);
			goto FINAL;
		}

		// 첫 항목을 찾는다.
		pstHandle->stFindFirstFile.pstFind = FindFirstInode(pstHandle->pstNtfs, pstHandle->stFindFirstFile.nInodeStart, pstInodeDataAlloc, pstHandle->stFindFirstFile.lpszNameTmp, MAX_PATH_NTFS);

		// 이제 . 그리고 .. 처리를 끝낸다.
		pstHandle->stFindFirstFile.nDotCount = 3;

		// 성공
		bRtnValue = TRUE;

		// 그러나...
		if (NULL == pstHandle->stFindFirstFile.pstFind)
		{
			// 첫 항목을 찾지 못했다.
			// 즉,
			// .
			// ..
			// 는 탐색이 되었으나 그 이후가 탐색이 안된 경우이다.
			// 비어있는 directory인 경우에 해당된다.
			bRtnValue = FALSE;
			dwErrorCode = ERROR_NO_MORE_FILES;
			pstHandle->stFindFirstFile.bEmptryDirectory = TRUE;
			goto FINAL;
		}
	}
	else
	{
		// 다음 파일로 이동한다.
		bRtnValue = FindNextInode(pstHandle->stFindFirstFile.pstFind, pstInodeDataAlloc, pstHandle->stFindFirstFile.lpszNameTmp, MAX_PATH_NTFS);
		if (FALSE == bRtnValue)
		{
			// [TBD]
			// FindNextFileNtfs에서 비-정규적인 오류들이 대부분이다.
			// 이때는 좀더 오류값을 세분화할 필요가 있다.
			// 그러기 위해서는 pstFind 정보등에 Last Error 원인에 대한 분석이 필요할 것이다.
			// 현재는 FindNextFile 시나리오에서 나름 성공의 의미인,
			// ERROR_NO_MORE_FILES 만 전달하도록 한다.
			dwErrorCode = ERROR_NO_MORE_FILES;
		}
	}

	// 다음 Inode를 찾는데 성공했다.

	// PROGRA~1,
	// $MFT
	// 와 같연 경로를 Filter한다.
	for (;;)
	{
		if (TRUE == FilterFindFirstFile(pstInodeDataAlloc))
		{
			// 조건에 만족한다.
			break;
		}

		// 조건에 맞지 않는 경우, 조건에 맞는 inode가 나타날때까지
		// Next한다.
		bRtnValue = FindNextInode(pstHandle->stFindFirstFile.pstFind, pstInodeDataAlloc, pstHandle->stFindFirstFile.lpszNameTmp, MAX_PATH_NTFS);
		if (FALSE == bRtnValue)
		{
			// 없다...
			// 이런 경우 가능한가?
			// 즉, FindFirstInode는 성공했는데,
			// Filter를 통해서는 한개도 나오지 않았을때...
			dwErrorCode = ERROR_NO_MORE_FILES;
			break;
		}
	}

	if (TRUE == bRtnValue)
	{
		// 성공일 때메만,...
		if (FALSE == GetWin32FindData(pstInodeDataAlloc, pstHandle->stFindFirstFile.lpszNameTmp, lpFindFileData))
		{
			bRtnValue = FALSE;
			dwErrorCode = ERROR_INTERNAL_ERROR;
			goto FINAL;
		}

		if (NULL != pstInodeData)
		{
			*pstInodeData = *pstInodeDataAlloc;
		}
	}

FINAL:

	if (NULL != pstHandle)
	{
		UNLOCK(pstHandle->pLock);
	}
	// UNLOCK
	//////////////////////////////////////////////////////////////////////////

	if (NULL != pstInodeDataAlloc)
	{
		free(pstInodeDataAlloc);
		pstInodeDataAlloc = NULL;
	}

	if (FALSE == bRtnValue)
	{
		// 실패...
		// 호출자는 GetLastError를 할 것이다.
		::SetLastError(dwErrorCode);

		if (NULL != pstInodeData)
		{
			memset(pstInodeData, 0, sizeof(*pstInodeData));
		}
	}

	return bRtnValue;
}

// Win32 API의 FindClose와 동일하게 구현된다.
BOOL CNTFSHelper::FindClose(IN NTFS_HANDLE hFindFile)
{
	BOOL				bRtnValue	= FALSE;
	DWORD				dwErrorCode	= ERROR_INVALID_PARAMETER;
	LPST_WIN32_HANDLE	pstHandle	= NULL;
	LPVOID				pLock		= NULL;

	if ((INVALID_HANDLE_VALUE_NTFS == hFindFile) || (NULL == hFindFile))
	{
		dwErrorCode = ERROR_INVALID_HANDLE;
		// 굳이 assert하지는 말자.
		// assert(FALSE);
		::SetLastError(dwErrorCode);
		return FALSE;
	}

	pstHandle = (LPST_WIN32_HANDLE)hFindFile;
	if (TYPE_WIN32_HANDLE_FINDFIRSTFILE != pstHandle->nType)
	{
		// 잘못된 Handle이 들어옴
		dwErrorCode = ERROR_INVALID_HANDLE;
		assert(FALSE);
		::SetLastError(dwErrorCode);
		return FALSE;
	}

	//////////////////////////////////////////////////////////////////////////
	// LOCK
	pLock = pstHandle->pLock;
	LOCK(pLock);

	if (NULL != pstHandle->pstNtfs)
	{
		CloseNtfs(pstHandle->pstNtfs);
		pstHandle->pstNtfs = NULL;
	}

	// 닫는다.
	if (NULL != pstHandle->stFindFirstFile.pstFind)
	{
		bRtnValue = FindCloseInode(pstHandle->stFindFirstFile.pstFind);
	}
	else
	{
		// 내부 Handle이 NULL임
		if (FALSE == pstHandle->stFindFirstFile.bFindSubDir)
		{
			// FindFirstFile에 *나 *.*가 사용되어 단말 파일만 탐색하는 경우에는,
			// 해당 Handle은 NULL이다.
			bRtnValue = TRUE;
		}
		else if ((TRUE == pstHandle->stFindFirstFile.bUseFindDot) &&
				 (TRUE == pstHandle->stFindFirstFile.bEmptryDirectory))
		{
			// . 과 .. 탐색을 사용하는 경우,
			// Directory에 대해,
			// FindFirstFile이 성공한다.
			// 만일 비어있는 경로인 경우, pstFind는 NULL일 수 있다.
			bRtnValue = TRUE;
		}
		else
		{
			dwErrorCode = ERROR_INVALID_HANDLE;
			bRtnValue = FALSE;
			assert(FALSE);
		}
	}

	if (NULL != pstHandle->stFindFirstFile.lpszNameTmp)
	{
		free(pstHandle->stFindFirstFile.lpszNameTmp);
		pstHandle->stFindFirstFile.lpszNameTmp = NULL;
	}

	// Handle 메모리 해제한다.
	free(pstHandle);
	pstHandle = NULL;

// 사용되지 않음
// FINAL:
	if (NULL != pLock)
	{
		UNLOCK(pLock);
		DESTROY_LOCK(pLock);
	}
	// UNLOCK
	//////////////////////////////////////////////////////////////////////////

	if (FALSE == bRtnValue)
	{
		// 실패
		::SetLastError(dwErrorCode);
	}

	return bRtnValue;
}

// Wild-card나 \\?\등이 제거된 Path를 리턴한다.
// 즉,
// C:\Windows
// 와 같은 경로를 구한다.
// 마지막 \문자는 Trim 시켜 준다.
// 최소 길이가 3이상(X:\)임을 보장시켜 준다.
// pchVolumeDrive는 CHAR 문자 하나만 해당되며, CHAR로 된 volume이름을 전달한다.
// 즉, C:\Windows라면, 'C'를 전달한다.
// 그리고, Win32 API에서,
// FindFirstFile(L"C:", &data);
// 를 하면 IDE라는 파일이름으로 성공 리턴을 받는다.
// 실제 해당 Handle로 FindNextFile은 실패된다.
// 아마도 Volume에 대한 정보를 간략히 전달하는듯 한데,
// 여기에서는 C:와 같은 경로를 허용하지 않도록 한다.
// 만일, *.* 혹은 *가 경로의 끝에 사용되었다면, *pbFindSubDir = TRUE를 전달한다.
//
// [참고] CreateFile(lpFileName, ...)의 lpFileName의 volume 경로와 canonical을 위해 호출된다.
DWORD CNTFSHelper::GetFindFirstFilePath(IN LPCWSTR lpFileName, OUT LPWSTR lpszPath, IN DWORD dwCchPath, OUT LPSTR pchVolumeDrive, OUT PBOOL pbFindSubDir)
{
	DWORD	dwErrorCode = ERROR_INVALID_PARAMETER;
	INT		nCchLength	= 0;
	LPCWSTR	lpszFind	= NULL;

	if (NULL != lpszPath)
	{
		lpszPath[0] = TEXT('\0');
	}

	if (NULL != pbFindSubDir)
	{
		*pbFindSubDir = FALSE;
	}

	if (NULL != pchVolumeDrive)
	{
		*pchVolumeDrive = 0;
	}

	if ((NULL == lpFileName) || (NULL == lpszPath) || (MAX_PATH_NTFS < dwCchPath) || (NULL == pchVolumeDrive) || (NULL == pbFindSubDir))
	{
		dwErrorCode = ERROR_INVALID_PARAMETER;
		assert(FALSE);
		goto FINAL;
	}

	nCchLength = wcslen(lpFileName);
	if ((nCchLength > MAX_PATH_NTFS) || (nCchLength <= 2))
	{
		dwErrorCode = ERROR_INVALID_PARAMETER;
		assert(FALSE);
		goto FINAL;
	}

	if (TEXT('\\') == lpFileName[0])
	{
		// 만일 \로 시작한다면,
		// \\?\가 사용되었는지 확인한다.
		if (nCchLength <= 5)
		{
			dwErrorCode = ERROR_INVALID_PARAMETER;
			assert(FALSE);
			goto FINAL;
		}

		if ((TEXT('\\') == lpFileName[1]) &&
			(TEXT('?')  == lpFileName[2]) &&
			(TEXT('\\') == lpFileName[3]))
		{
			// good
			// 4문자 뒤로 간다.
			lpFileName += 4;
			nCchLength = wcslen(lpFileName);
			if (nCchLength <= 2)
			{
				dwErrorCode = ERROR_INVALID_PARAMETER;
				assert(FALSE);
				goto FINAL;
			}
		}
		else
		{
			dwErrorCode = ERROR_INVALID_PARAMETER;
			assert(FALSE);
			goto FINAL;
		}
	}

	if (((TEXT('a') <= lpFileName[0]) && (lpFileName[0] <= TEXT('z'))) ||
		((TEXT('A') <= lpFileName[0]) && (lpFileName[0] <= TEXT('Z'))))
	{
		// 첫 문자는 반드시 영문
		// good
	}
	else
	{
		dwErrorCode = ERROR_INVALID_PARAMETER;
		assert(FALSE);
		goto FINAL;
	}

	lpszFind = wcsstr(lpFileName, L"?");
	if (NULL != lpszFind)
	{
		// ?문자는 사용 금지
		dwErrorCode = ERROR_NOT_SUPPORTED;
		assert(FALSE);
		goto FINAL;
	}

	// *.*는 Skip할 수 있도록 한다.
	lpszFind = wcsstr(lpFileName, L"*.*");
	if (NULL != lpszFind)
	{
		// 만일 *.*가 사용된 경우,...
		if ((TEXT('\0') == lpszFind[3]) && (TEXT('\\') == lpszFind[-1]))
		{
			// 앞에 \문자이고, 끝에 *가 사용되었음
			*pbFindSubDir = TRUE;
			StringCchCopyN(lpszPath, dwCchPath, lpFileName, lpszFind-lpFileName);
		}
		else
		{
			// *.* 문자가 사용되었지만,
			// 끝에 사용되지 않았다.
			// 지원되지 않는다.
			memset(lpszPath, 0, sizeof(WCHAR)*dwCchPath);
			dwErrorCode = ERROR_NOT_SUPPORTED;
			assert(FALSE);
			goto FINAL;
		}
	}
	else
	{
		StringCchCopy(lpszPath, dwCchPath, lpFileName);
	}

	lpszFind = wcsstr(lpszPath, L"*");
	if (NULL != lpszFind)
	{
		// 만일 * 문자가 포함되어 있는 경우,...
		if ((TEXT('\0') == lpszFind[1]) && (TEXT('\\') == lpszFind[-1]))
		{
			// 그런데, 끝에 *가 사용되었음
			// 그것은 *.*과 동일한 효과
			*pbFindSubDir = TRUE;
			StringCchCopyN(lpszPath, dwCchPath, lpFileName, lpszFind-lpszPath);
		}
		else
		{
			// * 문자가 사용되었지만,
			// 끝에 사용되지 않았다.
			// 지원되지 않는다.
			memset(lpszPath, 0, sizeof(WCHAR)*dwCchPath);
			dwErrorCode = ERROR_NOT_SUPPORTED;
			assert(FALSE);
			goto FINAL;
		}
	}

	if ((TEXT('a') <= lpszPath[0]) && (lpszPath[0] <= TEXT('z')))
	{
		// 만약 소문자 a~z
		*pchVolumeDrive = 'a' + (lpszPath[0] - TEXT('a'));
	}
	else if ((TEXT('A') <= lpszPath[0]) && (lpszPath[0] <= TEXT('Z')))
	{
		*pchVolumeDrive = 'A' + (lpszPath[0] - TEXT('A'));
	}
	else
	{
		memset(lpszPath, 0, sizeof(WCHAR)*dwCchPath);
		dwErrorCode = ERROR_INVALID_PARAMETER;
		assert(FALSE);
		goto FINAL;
	}

	// 끝의 \를 Trim한다.
	// 단, C:\는 Trim하지 않는다.
	nCchLength = wcslen(lpszPath);
	if (nCchLength > 3)
	{
		for (;;)
		{
			nCchLength = wcslen(lpszPath);
			if (nCchLength <= 2)
			{
				memset(lpszPath, 0, sizeof(WCHAR)*dwCchPath);
				dwErrorCode = ERROR_INVALID_PARAMETER;
				assert(FALSE);
				goto FINAL;
			}

			if (TEXT('\\') == lpszPath[nCchLength-1])
			{
				lpszPath[nCchLength-1] = TEXT('\0');
			}
			else
			{
				break;
			}
		}
	}

	if (wcslen(lpszPath) <= 2)
	{
		memset(lpszPath, 0, sizeof(WCHAR)*dwCchPath);
		dwErrorCode = ERROR_INVALID_PARAMETER;
		assert(FALSE);
		goto FINAL;
	}

	// 여기까지 왔다면 성공
	dwErrorCode = ERROR_SUCCESS;

FINAL:
	return dwErrorCode;
}

// C:\ABC\DEF\FG
// 같은 경우, 계속 호출하면,
// ABC(=ERROR_SUCCESS), DEF(=ERROR_SUCCESS), FG(=ERROR_HANDLE_EOF)를 각각 전달한다.
// 단, 초기 pnPos에는 0값을 전달해야 한다.
//
// ERROR_SUCCESS : Component 들어옴
// ERROR_HANDLE_EOF : 끝까지 갔음
// 이외 : 오류
DWORD CNTFSHelper::GetPathComponent(IN LPCWSTR lpszPath, OUT LPWSTR lpszComponent, IN DWORD dwCchComponent, IN OUT PINT pnPos)
{
	DWORD	dwRtnValue	= ERROR_INVALID_PARAMETER;
	INT		nCchLength	= 0;
	LPCWSTR	lpszFind	= NULL;

	if (NULL != lpszComponent)
	{
		lpszComponent[0] = TEXT('\0');
	}

	if ((NULL == lpszPath) || (NULL == lpszComponent) || (dwCchComponent < MAX_PATH_NTFS) || (NULL == pnPos))
	{
		dwRtnValue = ERROR_INVALID_PARAMETER;
		assert(FALSE);
		goto FINAL;
	}

	nCchLength = wcslen(lpszPath);
	if (nCchLength <= 2)
	{
		dwRtnValue = ERROR_INVALID_PARAMETER;
		assert(FALSE);
		goto FINAL;
	}

	if (0 == *pnPos)
	{
		if (((TEXT('a') <= lpszPath[0]) && (lpszPath[0] <= TEXT('z'))) ||
			((TEXT('A') <= lpszPath[0]) && (lpszPath[0] <= TEXT('Z'))))
		{
			// good
		}
		else
		{
			dwRtnValue = ERROR_INVALID_PARAMETER;
			assert(FALSE);
			goto FINAL;
		}

		if ((TEXT(':') == lpszPath[1]) && (TEXT('\\') == lpszPath[2]))
		{
			// good
		}
		else
		{
			dwRtnValue = ERROR_INVALID_PARAMETER;
			assert(FALSE);
			goto FINAL;
		}

		// C:\ABC
		// 에서 \위치로 가도록 함
		*pnPos = 2;
	}

	if (TEXT('\0') == lpszPath[*pnPos])
	{
		dwRtnValue = ERROR_HANDLE_EOF;
		goto FINAL;
	}

	// \ 문자에 대해 파싱한다.
	lpszFind = wcsstr(&lpszPath[*pnPos + 1], TEXT("\\"));

	if (NULL == lpszFind)
	{
		// 끝까지 간 경우임.
		dwRtnValue = ERROR_HANDLE_EOF;
		StringCchCopyW(lpszComponent, dwCchComponent, &lpszPath[*pnPos + 1]);
		*pnPos += wcslen(lpszComponent) + 1;
		if (wcslen(lpszComponent) > 0)
		{
			// 마지막 항목이 있다면,..
			dwRtnValue = ERROR_SUCCESS;
		}
		goto FINAL;
	}

	// 끝까지 가지 않은 경우임
	dwRtnValue = ERROR_SUCCESS;
	StringCchCopyNW(lpszComponent, dwCchComponent, &lpszPath[*pnPos + 1], lpszFind - &lpszPath[*pnPos + 1]);
	*pnPos += (wcslen(lpszComponent) + 1);

FINAL:
	return dwRtnValue;
}

// pstNtfs는 lpszPath의 volume이 열려있어야 한다.
// 즉, C:\windows\system32
// 인 경우, pstNtfs는 \\?\C:에 대한 값이어야 한다.
// 만일, pstNtfs가 \\?\D:로 열려있었다면,
// D:\windows\system32
// 에 해당되는 inode가 전달될 수 있다.
// pstInodeData는 불필요시 NULL 가능하다.
DWORD CNTFSHelper::FindInodeFromPath(IN LPST_NTFS pstNtfs, IN LPCWSTR lpszPath, OUT TYPE_INODE* pnInode, OPTIONAL OUT LPST_INODEDATA pstInodeData)
{
	DWORD		dwRtnValue		= ERROR_INVALID_PARAMETER;
	DWORD		dwResult		= 0;
	LPWSTR		lpszComponent	= NULL;
	INT			nPos			= 0;
	TYPE_INODE	nInode			= 0;
	BOOL		bFound			= FALSE;

	if (NULL != pnInode)
	{
		*pnInode = 0;
	}

	if (NULL != pstInodeData)
	{
		memset(pstInodeData, 0, sizeof(*pstInodeData));
	}

	if ((NULL == pstNtfs) || (NULL == lpszPath) || (NULL == pnInode))
	{
		dwRtnValue = ERROR_INVALID_PARAMETER;
		assert(FALSE);
		goto FINAL;
	}

	lpszComponent = (LPWSTR)malloc(sizeof(WCHAR) * (MAX_PATH_NTFS + 1));
	if (NULL == lpszComponent)
	{
		dwRtnValue = ERROR_NOT_ENOUGH_MEMORY;
		assert(FALSE);
		goto FINAL;
	}
	memset(lpszComponent, 0, sizeof(WCHAR) * (MAX_PATH_NTFS + 1));

	// 초기 Inode
	nInode = TYPE_INODE_DOT;

	// C:\123\abc\qwe 라면
	// DOT inode로 부터,
	// "123"에 해당되는 inode를 찾고,
	// 해당 inode로 부터 시작된 "abc" inode를 찾고,
	// 해당 inode로 부터 시작된 "qwe" inode를 찾는다.

	for (;;)
	{
		dwResult = GetPathComponent(lpszPath, lpszComponent, MAX_PATH_NTFS, &nPos);
		if (ERROR_HANDLE_EOF == dwResult)
		{
			break;
		}
		else if (ERROR_SUCCESS != dwResult)
		{
			assert(FALSE);
			goto FINAL;
		}

		nInode = CNTFSHelper::FindInode(pstNtfs, nInode, lpszComponent, &bFound, pstInodeData);
		if (FALSE == bFound)
		{
			// 찾지 못했음
			dwRtnValue = ERROR_FILE_NOT_FOUND;
			goto FINAL;
		}
	}

	if (3 == nPos)
	{
		// 만일 C:\와 같이 들어온 경우,...
		// INODE_DOT 위치와 data를 전달한다.
		// INODE_DOT에서 부터 시작하여 "."을 찾으면,
		// INODE_DOT이 리턴된다.
		nInode = CNTFSHelper::FindInode(pstNtfs, TYPE_INODE_DOT, L".", &bFound, pstInodeData);
		if ((FALSE == bFound) || (TYPE_INODE_DOT != nInode))
		{
			dwRtnValue = ERROR_FILE_NOT_FOUND;
			assert(FALSE);
			goto FINAL;
		}
	}

	// 값 전달
	*pnInode = nInode;

	// 여기까지 왔다면 성공
	dwRtnValue = ERROR_SUCCESS;

FINAL:
	if (NULL != lpszComponent)
	{
		free(lpszComponent);
		lpszComponent = NULL;
	}
	return dwRtnValue;
}

// [TBD] Other Platform (예, linux) 지원
FILETIME CNTFSHelper::GetLocalFileTime(IN UBYTE8 nUtc)
{
	FILETIME		nRtnValue	= {0,};
	FILETIME		nTime		= {0,};
	ULARGE_INTEGER	nLargeInt	= {0,};

	nLargeInt.QuadPart = nUtc;

	nTime.dwHighDateTime = nLargeInt.HighPart;
	nTime.dwLowDateTime  = nLargeInt.LowPart;
	if (FALSE == ::FileTimeToLocalFileTime(&nTime, &nRtnValue))
	{
		// 실패시...
		assert(FALSE);
		nRtnValue = nTime;
	}

	return nRtnValue;
}

// INDEX_ENTRY ==> INDEX_ENTRY_NODE로 변환한다.
// INDEX_ENTRY는 관련된 정보가 저장된 Cluster 메모리가 Free되면, 정보가 휘발하는데,
// INDEX_ENTRY_NODE는 직접 메모리를 가지고 있어, dangling reference가 발생하지 않는다.
BOOL CNTFSHelper::IndexEntryToIndexEntryNode(IN LPST_INDEX_ENTRY pstIndexEntry, OUT LPST_INDEX_ENTRY_NODE pstIndexEntryNode)
{
	BOOL					bRtnValue			= FALSE;
	LPWSTR					lpszName			= NULL;
	LPST_ATTRIB_FILE_NAME	pstAttribFileName	= NULL;

	if ((NULL == pstIndexEntry) || (NULL == pstIndexEntryNode))
	{
		assert(FALSE);
		goto FINAL;
	}

	memset(pstIndexEntryNode, 0, sizeof(*pstIndexEntryNode));

	lpszName = (LPWSTR)malloc(sizeof(WCHAR) * MAX_PATH_NTFS);
	if (NULL == lpszName)
	{
		assert(FALSE);
		goto FINAL;
	}
	memset(lpszName, 0, sizeof(WCHAR)*MAX_PATH_NTFS);

	if (INDEX_ENTRY_FLAG_LAST_ENTRY == (pstIndexEntry->nFlag & INDEX_ENTRY_FLAG_LAST_ENTRY))
	{
		// 마지막 항목이라면,...
		pstIndexEntryNode->bLast = TRUE;
	}

	if (INDEX_ENTRY_FLAG_SUBNODE == (pstIndexEntry->nFlag & INDEX_ENTRY_FLAG_SUBNODE))
	{
		// B-Tree에서 subnode가 있는 경우에,...
		pstIndexEntryNode->bHasSubNode = TRUE;

		// http://inform.pucp.edu.pe/~inf232/Ntfs/ntfs_doc_v0.5/attributes/index_allocation.html
		// 의 Index Entry 부분을 보면,
		// 마지막 -8 byte가 subnode 항목들이 있는 vcn이 저장되어 있는 곳이다.
		pstIndexEntryNode->nVcnSubnode = *(UBYTE8*)((UBYTE1*)pstIndexEntry + pstIndexEntry->nLengthIndexEntry - 8);
	}

	if (pstIndexEntry->nLengthStream > 0)
	{
		// Data가 있다면,...
		if (FALSE == GetIndexEntryFileName(pstIndexEntry, lpszName, MAX_PATH_NTFS))
		{
			assert(FALSE);
			goto FINAL;
		}

		// 이름을 할당한다.
		pstIndexEntryNode->lpszFileNameAlloc = _wcsdup(lpszName);
		if (NULL == pstIndexEntryNode->lpszFileNameAlloc)
		{
			assert(FALSE);
			goto FINAL;
		}

		// Inode를 구한다.
		pstIndexEntryNode->nInode = GetInode(pstIndexEntry, NULL);

		// 나머지 정보들을 채운다.
		pstAttribFileName = GetFileNameAttrib(pstIndexEntry);
		if (NULL == pstAttribFileName)
		{
			assert(FALSE);
			goto FINAL;
		}

		pstIndexEntryNode->llParentDirMftRef	= pstAttribFileName->llParentDirMftRef;
		pstIndexEntryNode->dwFileAttrib			= pstAttribFileName->dwFileAttrib;
		pstIndexEntryNode->cFileNameSpace		= pstAttribFileName->cFileNameSpace;
		pstIndexEntryNode->llCreationTime		= pstAttribFileName->llCreationTime;
		pstIndexEntryNode->llDataSize			= pstAttribFileName->llDataSize;
		pstIndexEntryNode->llLastAccessTime		= pstAttribFileName->llLastAccessTime;
		pstIndexEntryNode->llLastDataChangeTime	= pstAttribFileName->llLastDataChangeTime;

		// Data가 있다는 설정으로 마무리한다.
		pstIndexEntryNode->bHasData = TRUE;
	}

	// 여기까지 왔다면 성공
	bRtnValue = TRUE;

FINAL:

	if (NULL != lpszName)
	{
		free(lpszName);
		lpszName = NULL;
	}

	if (FALSE == bRtnValue)
	{
		// 실패
		if (NULL != pstIndexEntryNode)
		{
			if (NULL != pstIndexEntryNode->lpszFileNameAlloc)
			{
				free(pstIndexEntryNode->lpszFileNameAlloc);
				pstIndexEntryNode->lpszFileNameAlloc = NULL;
			}

			memset(pstIndexEntryNode, 0, sizeof(*pstIndexEntryNode));
		}
	}
	return bRtnValue;
}

// 주어진 index entry로 부터 시작된 index entry의 list를,
// 동적 할당된 배열로 만들어 전달한다.
CNTFSHelper::LPST_INDEX_ENTRY_ARRAY CNTFSHelper::GetIndexEntryArrayAlloc(IN LPST_INDEX_ENTRY pstIndexEntry)
{
	BOOL					bRtnValue			= FALSE;
	LPST_INDEX_ENTRY_ARRAY	pstRtnValue			= NULL;
	ST_INDEX_ENTRY_ARRAY	stIndexEntryArray	= {0,};
	LPST_INDEX_ENTRY		pstIndexEntryPos	= NULL;
	INT						i					= 0;
	BOOL					bForceLastSetting	= FALSE;

	if (NULL == pstIndexEntry)
	{
		assert(FALSE);
		goto FINAL;
	}

	// 우선 개수를 구한다.
	// [TBD] Maximum을 설정해야 하나?
	pstIndexEntryPos = pstIndexEntry;
	for (;;)
	{
		if (INDEX_ENTRY_FLAG_LAST_ENTRY == (pstIndexEntryPos->nFlag & INDEX_ENTRY_FLAG_LAST_ENTRY))
		{
			// 마지막 항목

			if (INDEX_ENTRY_FLAG_SUBNODE == (pstIndexEntryPos->nFlag & INDEX_ENTRY_FLAG_SUBNODE))
			{
				// 그런데, 마지막 항목에 subnode 정보가 있다면,
				// 추가한다.
				stIndexEntryArray.nItemCount++;
			}

			break;
		}

		pstIndexEntryPos = GetNextEntry(pstIndexEntryPos);
		if ((INDEX_ENTRY_FLAG_LAST_ENTRY != (pstIndexEntryPos->nFlag & INDEX_ENTRY_FLAG_LAST_ENTRY)) &&
			(0							 == pstIndexEntryPos->nLengthIndexEntry))
		{
			// 만일, Index Entry의 크기는 0인데,
			// Last Entry 값이 없는 경우,...
			bForceLastSetting = TRUE;
			break;
		}

		stIndexEntryArray.nItemCount++;
	}

	if (0 == stIndexEntryArray.nItemCount)
	{
		// 비어있는 Directory인 경우이다.
		goto FINAL;
	}

	stIndexEntryArray.pstArray = (LPST_INDEX_ENTRY_NODE)malloc(sizeof(ST_INDEX_ENTRY_NODE)*stIndexEntryArray.nItemCount);
	if (NULL == stIndexEntryArray.pstArray)
	{
		assert(FALSE);
		goto FINAL;
	}
	memset(stIndexEntryArray.pstArray, 0, sizeof(ST_INDEX_ENTRY_NODE)*stIndexEntryArray.nItemCount);

	pstIndexEntryPos = pstIndexEntry;
	for (i=0; i<stIndexEntryArray.nItemCount; i++)
	{
		// INDEX_ENTRY ==> INDEX_ENTRY_NODE
		// 로 변환한다.
		if (FALSE == IndexEntryToIndexEntryNode(pstIndexEntryPos, &stIndexEntryArray.pstArray[i]))
		{
			assert(FALSE);
			goto FINAL;
		}

		pstIndexEntryPos = GetNextEntry(pstIndexEntryPos);
	}

	if (TRUE == bForceLastSetting)
	{
		stIndexEntryArray.pstArray[i-1].bLast = TRUE;
	}

	pstRtnValue = (LPST_INDEX_ENTRY_ARRAY)malloc(sizeof(*pstRtnValue));
	if (NULL == pstRtnValue)
	{
		assert(FALSE);
		goto FINAL;
	}

	*pstRtnValue = stIndexEntryArray;

FINAL:
	if (NULL == pstRtnValue)
	{
		// 실패
		if (NULL != stIndexEntryArray.pstArray)
		{
			free(stIndexEntryArray.pstArray);
			stIndexEntryArray.pstArray = NULL;
		}
	}
	return pstRtnValue;
}

// INDEX_ENTRY의 array를 동적해제한다.
VOID CNTFSHelper::GetIndexEntryArrayFree(IN LPST_INDEX_ENTRY_ARRAY pstArrayAlloc)
{
	INT i = 0;

	if (NULL == pstArrayAlloc)
	{
		return;
	}

	if (NULL != pstArrayAlloc->pstArray)
	{
		for (i=0; i<pstArrayAlloc->nItemCount; i++)
		{
			if (NULL != pstArrayAlloc->pstArray[i].lpszFileNameAlloc)
			{
				free(pstArrayAlloc->pstArray[i].lpszFileNameAlloc);
				pstArrayAlloc->pstArray[i].lpszFileNameAlloc = NULL;
			}
		}

		free(pstArrayAlloc->pstArray);
		pstArrayAlloc->pstArray = NULL;
	}

	free(pstArrayAlloc);
	pstArrayAlloc = NULL;
}

// INDEX_ENTRY의 Array를 구한다.
// 이는, 임의의 INDEX_ENTRY에 subnode가 있을 때,
// 해당 subnode의 Vcn 위치에 있는 Index block을 읽어,
// 그곳의 INDEX_ENTRY Array를 전달하는 것이다.
// 다시 말하자면, 임의의 INDEX_ENTRY의 B-Tree 상의 subnode에
// 있는 INDEX_ENTRY의 배열을 리턴한다고 보면 된다.
// Input에 해당되는 Run list는 $INDEX_ALLOCATION의 Run list를 전달해야 한다.
CNTFSHelper::LPST_INDEX_ENTRY_ARRAY CNTFSHelper::GetIndexEntryArrayAllocByVcn(IN LPST_NTFS pstNtfs, IN UBYTE4 nIndexBlockSize, IN LPST_RUNLIST pstRunList, IN INT nCountRunList, IN UBYTE8 nVcn)
{
	LPST_INDEX_ENTRY_ARRAY		pstRtnValue			= NULL;
	UBYTE8						nLcn				= 0;
	UBYTE1*						pBufCluster			= NULL;
	LPST_STANDARD_INDEX_HEADER	pstStdIndexHeader	= NULL;
	LPST_INDEX_ENTRY			pstIndexEntry		= NULL;
	UBYTE8						nVcnPos				= 0;
	UBYTE8						nPos				= 0;
	UBYTE8						nVcnCalc			= 0;

	if ((NULL == pstNtfs) || (0 == nIndexBlockSize) || (NULL == pstRunList) || (0 == nCountRunList))
	{
		assert(FALSE);
		goto FINAL;
	}

	// Buffer를 할당한다.
	// Buffer는 Cluster 크기 단위가 아니라,
	// Index Block 크기만큼 할당하게 된다.
	pBufCluster = (UBYTE1*)malloc(nIndexBlockSize);
	if (NULL == pBufCluster)
	{
		assert(FALSE);
		goto FINAL;
	}
	memset(pBufCluster, 0, nIndexBlockSize);

	if ((pstNtfs->stNtfsInfo.dwClusterSize <= nIndexBlockSize) ||
		(0 == nVcn))
	{
		// 보통 IndexBlockSize는 4K(=4096) byte이다.
		// 만일 Cluster 크기가 4KB 이하라면,
		// 1 VCN 크기는 Cluster 크기로 매핑된다.
		// 보통 Cluster 크기는 4KB로 설정되므로,
		// 일반적인 경우, 이곳에 해당된다.

		// 또한, 요청한 Vcn 위치가 0 인 경우에도 해당된다.

		// Vcn을 Lcn으로 변환한다.
		// 보통, ntfs document에 알려진 방법으로 Vcn -> Lcn 값으로 변환한다.
		if (FALSE == GetLcnFromVcn(pstRunList, nCountRunList, nVcn, 0, &nLcn))
		{
			// 실패
			assert(FALSE);
			goto FINAL;
		}

		// 해당 Vcn의 위치의 Index Block의 위치는,
		// 매핑된 Lcn값에 해당되는 Cluster 위치이므로,
		// 단순히 Lcn 값에 Cluster 크기를 곱하면 된다.
		nPos = nLcn * pstNtfs->stNtfsInfo.dwClusterSize;
	}
	else
	{
		// 예를 들어, $INDEX_ALLOCATION의 runlist를 구해서, vcn 값이 0~3이 나왔다고 가정하자.
		// 그러면, 요청하는 vcn값도 0~3 사이에 있어야 한다.
		// 그런데, 64KB의 Cluster로 포맷된 경우,
		// 해당 0~3 범위를 벗어났었다. (예를 들어, 16)
		// 즉, 64KB의 Cluster인 경우, INDEX_ENTRY에 subnode가 있는 경우,
		// 해당 subnode의 Vcn값의 단위가 일반적이지 않는다는 뜻이다.

		// 이 부분은 일반적은 알려진 ntfs document에 잘 나타나 있지 않다.

		// document를 통한 확인)
		// http://inform.pucp.edu.pe/~inf232/Ntfs/ntfs_doc_v0.5/attributes/index_root.html
		// 의
		// __u8 clusters_per_index_block;
		// 주석을 참고한다.

		// ntfs-3g를 통한 확인)
		//
		// 만일 Cluster 크기가 IndexBlockSize(일반적으로, 4KB)보다 크다면,...
		// 1 VCN 크기는 일반적인 Cluster 크기 단위에 해당되지 않는다.
		// 참고)
		// ntfsprogs(ntfs-3g)의 source : http://www.tuxera.com/community/ntfs-3g-download/
		// 의 dir.c 부분
		// 혹은
		// http://fossies.org/linux/misc/old/ntfsprogs-1.12.1.tar.gz:a/ntfsprogs-1.12.1/libntfs/dir.c
		//
		// ntfs_inode_lookup_by_name()
		// 함수 중간에 보면,
		//
		// if (vol->cluster_size <= index_block_size)
		//		index_vcn_size = vol->cluster_size;
		// else
		//		index_vcn_size = vol->sector_size;
		//
		// 논리가 나온다. 해당 부분을 참고한다.
		//
		// 다시 정리하자면, 
		//	ntfs_inode_lookup_by_name(...)
		//		-> ntfs_attr_mst_pread(..., Vcn=16 << index_vcn_size_bits=9 ,...)
		//		   (즉, 16번 Vcn을 읽으라라는 뜻임. 그런데, 16을 512만큼 곱한 효과로, 8192를 전달)
		//			-> ntfs_attr_pread_i(..., 8192, ...)
		//			   (내부에서 $INDEX_ALLOCATION의 runlist를 구함.
		//			    참조 runlist의 Vcn은 넘어온 8192에서 >> 16(cluster_size_bits), 즉, Cluster 크기로 나눔
		//			    그러면, 
		//				-> ntfs_attr_find_vcn(...)
		// 으로 호출되는 callstack을 확인한다.

		// 다시말하자면,
		// $INDEX_ROOT로부터 탐색된 INDEX_ENTRY list에서, subnode가 있는 경우,
		// subnode가 저장된 INDEX_ENTRY의 vcn이 저장되어 있다.
		// 그런데,
		// 4K를 초과하는 Cluster로 포맷된 시스템인 경우,
		// $INDEX_ALLOCATION의 runlist를 통해 지원되는 vcn 범위를 벗어나는 경우가 있다.
		// 즉, INDEX_ENTRY의 subnode vcn값은, 그 단위가 vcn이 아니라는 뜻이다.
		// 확인결과, 해당 단위는 Lcn-Vcn 매핑으로 이뤄진 Cluster 크기 단위가 아니라,
		// Sector단위였다.
		// 즉, 만약, $INDEX_ALLOCATION의 runlist에서 vcn의 범위가 0~3인 상황에서,
		// subnode의 vcn 값이 16이라면,
		// 실제 그 16은 Cluster 단위가 아니라, sector 단위이다.
		// 그래서, 16 * 512(sector 크기) ==> 8192(byte 위치)에
		// subnode INDEX_ENTRY가 저장되어 있다는 뜻이다.
		// 그러면, 8192 byte의 위치를 찾는 일인데,
		// 8192 / 64K(Cluster 크기) ==> 0(cluster 위치, Lcn)
		// 과 같이 0번 Lcn에 위치한다는 뜻이다.
		// 그렇지만, 정확히 0번 Lcn 위치를 뜻하는것은 아니다.
		// 왜냐하면, 8192가 64K에 정확히 나눠 떨어지지 않았기 때문이다.
		// 즉, 0번 Lcn에 그 나머지 값(8192 byte)으로 offset 처리를 해야 한다.
		//
		// 그렇다면, 16번 vcn에 해당되는 INDEX_ENTRY를 찾기 위해서는
		// 0번 Lcn 위치에서, 8192 offset을 더한 위치가 정확한 subnode 위치가 된다.

		// 해당 Vcn 값에 해당되는 
		nVcnPos = nVcn * pstNtfs->stNtfsInfo.wSectorSize;

		// 즉, 해당 Vcn에 해당되는 값은 위 nVcnPos 값(byte 단위)에 해당된다.
		// 그럼, 해당 byte단위가 $INDEX_ALLOCATION의 runlist상 어떤 Vcn에 포함되는지 찾아보자.
		// 찾는건 간단하다. Cluster 크기로 나누면 된다.
		nVcnCalc = nVcnPos / pstNtfs->stNtfsInfo.dwClusterSize;

		// 이제, 계산된 Vcn값으로 Lcn으로 변환한다.
		if (FALSE == GetLcnFromVcn(pstRunList, nCountRunList, nVcnCalc, 0, &nLcn))
		{
			// 실패
			assert(FALSE);
			goto FINAL;
		}

		// 그런데, 이런 경우, Offset값이 존재한다.
		// 즉, 정확하게 요청된 byte 위치(nVcnPos)가 Cluster 경계가 아닐 수 있다는 뜻이다.
		// 그래서 정확한 Sector 위치(byte 단위)를 구한다.
		nPos = nLcn * pstNtfs->stNtfsInfo.dwClusterSize + nVcnPos % pstNtfs->stNtfsInfo.dwClusterSize;
	}

	// 해당 위치로 간다.
	if (FALSE == SeekByte(pstNtfs, nPos))
	{
		assert(FALSE);
		goto FINAL;
	}

	// 위치로 이동했다.
	// 그러니, 이제 읽자.
	// 읽을때 크기는 Cluster 크기가 아니라, Index Block Size 만큼 읽는다.
	if (nIndexBlockSize != ReadIo(pstNtfs->pstIo, pBufCluster, nIndexBlockSize))
	{
		assert(FALSE);
		goto FINAL;
	}

	// $INDEX_ALLOCATION의 STANDARD_INDEX_HEADER를 가져온다.
	pstStdIndexHeader = (LPST_STANDARD_INDEX_HEADER)pBufCluster;

	// INDX 검증
	if (('I' == pstStdIndexHeader->szMagic[0]) &&
		('N' == pstStdIndexHeader->szMagic[1]) &&
		('D' == pstStdIndexHeader->szMagic[2]) &&
		('X' == pstStdIndexHeader->szMagic[3]))
	{
		// good
	}
	else
	{
		// bad
		assert(FALSE);
		goto FINAL;
	}

	// Update Sequence 처리를 한다.
	if (FALSE == UpdateSequenceIndexAlloc(nIndexBlockSize, pBufCluster, pstNtfs->stNtfsInfo.dwClusterSize, pstNtfs->stNtfsInfo.wSectorSize))
	{
		assert(FALSE);
		goto FINAL;
	}

	// 이제 드디어 초기 Index Entry가 계산된다.
	pstIndexEntry = (LPST_INDEX_ENTRY)((UBYTE1*)&pstStdIndexHeader->nOffsetEntry + pstStdIndexHeader->nOffsetEntry);

	// 그럼 이제 Index Entry 기반으로 Array를 구한다.
	pstRtnValue = GetIndexEntryArrayAlloc(pstIndexEntry);
	if (NULL == pstRtnValue)
	{
		assert(FALSE);
		goto FINAL;
	}

FINAL:
	if (NULL != pBufCluster)
	{
		free(pBufCluster);
		pBufCluster = NULL;
	}
	return pstRtnValue;
}

// directory에 해당되는 inode의 $INDEX_ROOT를 읽으면,
// b-tree 상의 root에 해당되는 index block을 구할 수 있다.
// 해당 index block의 INDEX_ENTRY의 array를 구한다.
// index block의 크기는 $INDEX_ROOT에 정의되기 때문에,
// caller에 전달한다. 해당 index block의 크기는,
// $INDEX_ALLOCATION의 값을 읽을때 사용된다.
CNTFSHelper::LPST_INDEX_ENTRY_ARRAY CNTFSHelper::GetIndexEntryArrayAllocByIndexRoot(IN LPST_NTFS pstNtfs, IN TYPE_INODE nInode, OUT UBYTE4* pnIndexSize)
{
	LPST_INDEX_ENTRY_ARRAY	pstRtnValue			= NULL;
	LPST_INDEX_ROOT			pstIndexRoot		= NULL;
	LPST_INDEX_HEADER		pstIndexHeader		= NULL;
	LPST_FIND_ATTRIB		pstIndexRootAlloc	= NULL;
	LPST_INDEX_ENTRY		pstIndexEntry		= NULL;

	if (NULL != pnIndexSize)
	{
		*pnIndexSize = 0;
	}

	if ((NULL == pstNtfs) || (NULL == pnIndexSize))
	{
		assert(FALSE);
		goto FINAL;
	}

	// $INDEX_ROOT를 구한다.
	pstIndexRootAlloc = FindAttribAlloc(pstNtfs, nInode, TYPE_ATTRIB_INDEX_ROOT);
	if (NULL == pstIndexRootAlloc)
	{
		assert(FALSE);
		goto FINAL;
	}

	// $INDEX_ROOT를 통해 첫번째 Entry를 구한다.
	pstIndexRoot   = (LPST_INDEX_ROOT)((UBYTE1*)pstIndexRootAlloc->pstAttrib + pstIndexRootAlloc->pstAttrib->stAttr.stResident.wValueOffset);
	pstIndexHeader = (LPST_INDEX_HEADER)((UBYTE1*)pstIndexRoot + sizeof(*pstIndexRoot));
	pstIndexEntry  = (LPST_INDEX_ENTRY)((UBYTE1*)&pstIndexHeader->nOffsetFirstIndexEntry + pstIndexHeader->nOffsetFirstIndexEntry);

	pstRtnValue = GetIndexEntryArrayAlloc(pstIndexEntry);
	if (NULL == pstRtnValue)
	{
		// 비어있는 Directory인 경우가 포함된다.
		goto FINAL;
	}

	// Index의 크기를 전달한다.
	*pnIndexSize = pstIndexRoot->nSize;

FINAL:
	if (NULL != pstIndexRootAlloc)
	{
		FindAttribFree(pstIndexRootAlloc);
		pstIndexRootAlloc = NULL;
	}
	return pstRtnValue;
}

// Win32 API의 FindFirstFile / FindNextFile와 같은 형태로, Directory의 파일을 나열할때 처음 호출한다.
// Win32 API 함수와 다르게,
// $MFT, $BITMAP과 같은 meta-data file과 PROGRA~1과 같은 dos 파일 이름도 들어온다.
//
// nInode : 시작할 inode
// pstInodeData : 첫번째 항목의 inode 정보
//
// Remark : b-tree traverse 논리는 CNTFSHelper::CreateStack(...) 주석 참고
CNTFSHelper::LPST_FINDFIRSTINODE CNTFSHelper::FindFirstInode(IN LPST_NTFS pstNtfs, IN TYPE_INODE nInode, OUT LPST_INODEDATA pstInodeData, OUT LPWSTR lpszName, IN DWORD dwCchName)
{
	LPST_FINDFIRSTINODE		pstRtnValue		= NULL;
	ST_FINDFIRSTINODE		stFind			= {0,};
	LPST_INDEX_ENTRY_ARRAY	pstArray		= NULL;
	LPST_FIND_ATTRIB		pstIndexAlloc	= NULL;
	ST_STACK_NODE			stStackNode		= {0,};

	if (NULL != pstInodeData)
	{
		memset(pstInodeData, 0, sizeof(*pstInodeData));
	}

	if (NULL != lpszName)
	{
		lpszName[0] = L'\0';
	}

	if ((NULL == pstNtfs) || (NULL == pstInodeData) || (NULL == lpszName) || (dwCchName < MAX_PATH_NTFS))
	{
		assert(FALSE);
		goto FINAL;
	}

	stFind.pstNtfs = pstNtfs;

	// $INDEX_ROOT에 포함된 목록을 구한다.
	pstArray = GetIndexEntryArrayAllocByIndexRoot(pstNtfs, nInode, &stFind.nIndexBlockSize);
	if (NULL == pstArray)
	{
		// 비어있는 Directory인 경우가 포함된다.
		goto FINAL;
	}

	// $INDEX_ROOT의 목록을 성공적으로 구해왔다면,
	// $INDEX_ALLOCATION으로 부터 RunList를 구해온다.
	pstIndexAlloc = FindAttribAlloc(pstNtfs, nInode, TYPE_ATTRIB_INDEX_ALLOCATION);
	if (NULL != pstIndexAlloc)
	{
		stFind.pstRunListAlloc = GetRunListAlloc(GetRunListByte(pstIndexAlloc->pstAttrib), &stFind.nCountRunList);
		if (NULL == stFind.pstRunListAlloc)
		{
			assert(FALSE);
			goto FINAL;
		}
	}

	// Stack을 생성한다.
	stFind.pstStack = CreateStack(5);
	if (NULL == stFind.pstStack)
	{
		assert(FALSE);
		goto FINAL;
	}

	// Stack에 push 한다.
	stStackNode.pstIndexEntryArray = pstArray;
	if (FALSE == StackPush(stFind.pstStack, &stStackNode))
	{
		assert(FALSE);
		goto FINAL;
	}

	// FindNextInode를 이용하여 첫 항목을 찾는다.
	if (FALSE == FindNextInode(&stFind, pstInodeData, lpszName, dwCchName))
	{
		assert(FALSE);
		goto FINAL;
	}

	// 리턴값을 결정한다.
	pstRtnValue = (LPST_FINDFIRSTINODE)malloc(sizeof(*pstRtnValue));
	if (NULL == pstRtnValue)
	{
		assert(FALSE);
		goto FINAL;
	}

	*pstRtnValue = stFind;

FINAL:

	if (NULL == pstRtnValue)
	{
		// 실패
		if (NULL != stFind.pstRunListAlloc)
		{
			GetRunListFree(stFind.pstRunListAlloc);
			stFind.pstRunListAlloc = NULL;
		}

		if (NULL != stFind.pstStack)
		{
			DestroyStack(stFind.pstStack);
			stFind.pstStack = NULL;
		}

		if (NULL != pstInodeData)
		{
			memset(pstInodeData, 0, sizeof(*pstInodeData));
		}

		// Stack을 비운다. [TBD]
	}

	if (NULL != pstIndexAlloc)
	{
		FindAttribFree(pstIndexAlloc);
		pstIndexAlloc = NULL;
	}

	return pstRtnValue;
}

// FindFirstInode 이후의 File을 나열해 준다.
// 논리는 Win32 API의 FindNextFile과 유사하다.
// FindFirstInode의 주석 참조.
BOOL CNTFSHelper::FindNextInode(IN LPST_FINDFIRSTINODE pstFind, OUT LPST_INODEDATA pstInodeData, OUT LPWSTR lpszName, IN DWORD dwCchName)
{
	BOOL					bRtnValue	= FALSE;
	ST_STACK_NODE			stNode		= {0,};
	LPST_INDEX_ENTRY_ARRAY	pstArray	= NULL;

	if (NULL != lpszName)
	{
		lpszName[0] = L'\0';
	}

	if ((NULL == pstFind) || (NULL == pstInodeData) || (NULL == lpszName) || (dwCchName < MAX_PATH_NTFS))
	{
		assert(FALSE);
		goto FINAL;
	}

	if (NULL != pstInodeData)
	{
		memset(pstInodeData, 0, sizeof(*pstInodeData));
	}

	if (TRUE == pstFind->bFinished)
	{
		// Traverse 종료
		goto FINAL;
	}

	if (TRUE == pstFind->bErrorOccurred)
	{
		// 오류로 인한 Traverse 종료
		assert(FALSE);
		goto FINAL;
	}

	if (TRUE == IsStackEmptry(pstFind->pstStack))
	{
		// Traverse 종료
		goto FINAL;
	}

	// B-Tree상, Array의 현재 위치 부분에 Subnode가 있으면 그쪽으로 넘어간다.
	// 즉, Leaf node때 까지 B-Tree를 Traverse해야 한다.
	for (;;)
	{
		// Stack의 맨 윗 항목이 작업 대상이다.
		if (FALSE == StackGetTop(pstFind->pstStack, &stNode))
		{
			pstFind->bErrorOccurred = TRUE;
			assert(FALSE);
			goto FINAL;
		}

		if (NULL == stNode.pstIndexEntryArray)
		{
			pstFind->bErrorOccurred = TRUE;
			assert(FALSE);
			goto FINAL;
		}

		if (stNode.pstIndexEntryArray->nCurPos >= stNode.pstIndexEntryArray->nItemCount)
		{
			// 만일 다음 항목이 범위를 벗어나는 경우에,...

			// Stack의 Top 부분이 이미 모두 진행되었다.
			// 그러니 진행된 것을 버리자.
			if (FALSE == StackPop(pstFind->pstStack, &stNode))
			{
				pstFind->bErrorOccurred = TRUE;
				assert(FALSE);
				goto FINAL;
			}

			GetIndexEntryArrayFree(stNode.pstIndexEntryArray);
			stNode.pstIndexEntryArray = NULL;

			if (TRUE == IsStackEmptry(pstFind->pstStack))
			{
				// Traverse가 종료된다.
				pstFind->bFinished = TRUE;
				break;
			}

			// 다시 최상단을 가져온다.
			if (FALSE == StackGetTop(pstFind->pstStack, &stNode))
			{
				pstFind->bErrorOccurred = TRUE;
				assert(FALSE);
				goto FINAL;
			}

			if (NULL == stNode.pstIndexEntryArray)
			{
				pstFind->bErrorOccurred = TRUE;
				assert(FALSE);
				goto FINAL;
			}
		}

		// 아래는, 현재 탐색할 Index Entry가,
		//		- Subnode가 있다.
		//		- 아직 Subnode에 들어간 적 없다.
		// 연 경우,
		// Subnode의 탐색 정보를 가져와 Stack 최 상단에 Push한다.
		if ((TRUE  == stNode.pstIndexEntryArray->pstArray[stNode.pstIndexEntryArray->nCurPos].bHasSubNode) &&
			(FALSE == stNode.pstIndexEntryArray->pstArray[stNode.pstIndexEntryArray->nCurPos].bSubnodeTraversed))
		{
			stNode.pstIndexEntryArray->pstArray[stNode.pstIndexEntryArray->nCurPos].bSubnodeTraversed = TRUE;

			// Subnode가 있다...
			// 그리고, 아직 Traverse하지 않았다.
			// 그쪽으로 넘어간다.
			pstArray = GetIndexEntryArrayAllocByVcn(pstFind->pstNtfs, pstFind->nIndexBlockSize, pstFind->pstRunListAlloc, pstFind->nCountRunList, stNode.pstIndexEntryArray->pstArray[stNode.pstIndexEntryArray->nCurPos].nVcnSubnode);
			if (NULL == pstArray)
			{
				// subnode로 가는데 실패함
				pstFind->bErrorOccurred = TRUE;
				assert(FALSE);
				goto FINAL;
			}

			// Stack에 Push 한다.
			stNode.pstIndexEntryArray = pstArray;
			if (FALSE == StackPush(pstFind->pstStack, &stNode))
			{
				pstFind->bErrorOccurred = TRUE;
				assert(FALSE);
				goto FINAL;
			}

			// Loop를 다시 시작하자.
			continue;
		}
		else
		{
			// do nothing...
		}

		// 여기왔다는 것은, subnode가 아니라는 뜻이다.
		if (FALSE == stNode.pstIndexEntryArray->pstArray[stNode.pstIndexEntryArray->nCurPos].bHasData)
		{
			// 만일 data가 없다...
			// 그럼 다음 항목으로 넘어간다.
			stNode.pstIndexEntryArray->nCurPos++;
			continue;
		}

		// 여기왔다는 것은, subnode가 아니면서, data가 있다는 뜻이다.
		if (stNode.pstIndexEntryArray->nCurPos >= stNode.pstIndexEntryArray->nItemCount)
		{
			// 넘어섰다.

			// Stack의 Top 부분이 이미 모두 진행되었다.
			// 그러니 진행된 것을 버리자.
			if (FALSE == StackPop(pstFind->pstStack, &stNode))
			{
				pstFind->bErrorOccurred = TRUE;
				assert(FALSE);
				goto FINAL;
			}

			GetIndexEntryArrayFree(stNode.pstIndexEntryArray);
			stNode.pstIndexEntryArray = NULL;

			if (TRUE == IsStackEmptry(pstFind->pstStack))
			{
				// Traverse가 종료된다.
				pstFind->bFinished = TRUE;
				break;
			}

			// 다시 최상단을 가져온다.
			if (FALSE == StackGetTop(pstFind->pstStack, &stNode))
			{
				pstFind->bErrorOccurred = TRUE;
				assert(FALSE);
				goto FINAL;
			}

			if (NULL == stNode.pstIndexEntryArray)
			{
				pstFind->bErrorOccurred = TRUE;
				assert(FALSE);
				goto FINAL;
			}

			// continue;
			// 즉, Loop를 다시 시작하지 않는다.
			// 즉, 현재 위치는, Subnode가 다 종료되고,
			// 이제 다시 상위 node로 올라온 경우이다.
			// 이때는, 아래, 즉, Data를 꺼내는 과정을 수행한다.
		}

		// 여기왔다는 것은, subnode가 아니고, data가 있으며, array의 중간에 있다는 뜻이다.
		if (FALSE == GetInodeData(&stNode.pstIndexEntryArray->pstArray[stNode.pstIndexEntryArray->nCurPos], pstInodeData, lpszName, dwCchName))
		{
			pstFind->bErrorOccurred = TRUE;
			assert(FALSE);
			goto FINAL;
		}

		// 항목을 발견하였다.
		bRtnValue = TRUE;

		// Loop를 벗어난다.
		break;
	}

	if (TRUE == bRtnValue)
	{
		// 다음 항목으로 이동하자.
		stNode.pstIndexEntryArray->nCurPos++;
	}

FINAL:
	return bRtnValue;
}

// Inode의 정보를 전달한다.
// [TBD] Other platform (ex, linux) 지원
BOOL CNTFSHelper::GetInodeData(IN LPST_INDEX_ENTRY_NODE pstIndexEntryNode, OUT LPST_INODEDATA pstData, OPTIONAL OUT LPTSTR lpszName, OPTIONAL IN DWORD dwCchName)
{
	BOOL			bRtnValue	= FALSE;
	ULARGE_INTEGER	nLargeInt	= {0,};

	if (NULL != pstData)
	{
		memset(pstData, 0, sizeof(*pstData));
	}

	if (NULL != lpszName)
	{
		lpszName[0] = L'\0';
	}

	if ((NULL == pstIndexEntryNode) || (NULL == pstData))
	{
		assert(FALSE);
		goto FINAL;
	}

	if (FALSE == pstIndexEntryNode->bHasData)
	{
		// Data가 없다.
		assert(FALSE);
		goto FINAL;
	}

	// Inode
	pstData->nInode = pstIndexEntryNode->nInode;

	// 파일 크기
	pstData->nFileSize = pstIndexEntryNode->llDataSize;

	// 이름
	if ((NULL != lpszName) && (dwCchName >= MAX_PATH_NTFS))
	{
		StringCchCopyW(lpszName, dwCchName, pstIndexEntryNode->lpszFileNameAlloc);
	}

	// http://inform.pucp.edu.pe/~inf232/Ntfs/ntfs_doc_v0.5/concepts/filename_namespace.html
	switch (pstIndexEntryNode->cFileNameSpace)
	{
	case NAMESPACE_POSIX:
	case NAMESPACE_WIN32:
	case NAMESPACE_WIN32DOS:
		pstData->bWin32Namespace = TRUE;
		break;
	case NAMESPACE_DOS:
		// C:\PROGRA~1과 같은것...
		pstData->bWin32Namespace = FALSE;
		break;
	default:
		pstData->bWin32Namespace = FALSE;
		assert(FALSE);
	}

	// Parent directory mft
	pstData->llParentDirMftRef = pstIndexEntryNode->llParentDirMftRef;

	// 정보를 채운다.
	pstData->dwFileAttrib = pstIndexEntryNode->dwFileAttrib;
	pstData->llCreationTime = pstIndexEntryNode->llCreationTime;
	pstData->llLastAccessTime = pstIndexEntryNode->llLastAccessTime;
	pstData->llLastDataChangeTime = pstIndexEntryNode->llLastDataChangeTime;

	// 여기까지 왔다면 성공
	bRtnValue = TRUE;

FINAL:

	if (FALSE == bRtnValue)
	{
		// 함수 실패
		if (NULL != pstData)
		{
			memset(pstData, 0, sizeof(*pstData));
		}
	}

	return bRtnValue;
}

// FindFirstInode로 생성된 Object를 삭제한다.
BOOL CNTFSHelper::FindCloseInode(IN LPST_FINDFIRSTINODE pstFind)
{
	BOOL			bRtnValue	= TRUE;	// 기본값 성공
	ST_STACK_NODE	stNode		= {0,};

	if (NULL == pstFind)
	{
		bRtnValue = FALSE;
		assert(FALSE);
		goto FINAL;
	}

	if (NULL != pstFind->pstRunListAlloc)
	{
		GetRunListFree(pstFind->pstRunListAlloc);
		pstFind->pstRunListAlloc = NULL;
		pstFind->nCountRunList = 0;
	}

	if (NULL != pstFind->pstStack)
	{
		// Stack이 있음
		for (;;)
		{
			if (TRUE == IsStackEmptry(pstFind->pstStack))
			{
				// Stack이 비었으면 Exit
				break;
			}

			if (FALSE == StackPop(pstFind->pstStack, &stNode))
			{
				bRtnValue = FALSE;
				assert(FALSE);
				break;
			}

			if (NULL != stNode.pstIndexEntryArray)
			{
				// Stack에 Index Entry Array가 있다면,
				// 삭제한다.
				GetIndexEntryArrayFree(stNode.pstIndexEntryArray);
				stNode.pstIndexEntryArray = NULL;
			}
		}

		DestroyStack(pstFind->pstStack);
		pstFind->pstStack = NULL;
	}

	free(pstFind);
	pstFind = NULL;

FINAL:
	return bRtnValue;
}

// Inode Traverse와 다르게 File Traverse는,
//		- $MFT와 같은 metadata file
//		- PROGRA~1과 같은 Dos namespace
// 는 스킵한다.
// 본 함수가 TRUE가 전달되면, File Traverse에 만족하는 항목이고,
// FALSE가 리턴되면 Skip하도록 한다.
BOOL CNTFSHelper::FilterFindFirstFile(IN LPST_INODEDATA pstData)
{
	BOOL bRtnValue = FALSE;

	if (NULL == pstData)
	{
		assert(FALSE);
		goto FINAL;
	}

	if (FALSE == pstData->bWin32Namespace)
	{
		goto FINAL;
	}

	if (FALSE == IsNormalFile(pstData->nInode))
	{
		goto FINAL;
	}

	// 여기까지 왔다면 성공
	bRtnValue = TRUE;

FINAL:
	return bRtnValue;
}

// Inode 정보를 가지고 WIN32_FIND_DATA 구조체 값을 구한다.
BOOL CNTFSHelper::GetWin32FindData(IN LPST_INODEDATA pstData, IN LPCWSTR lpszName, OUT PWIN32_FIND_DATAW pstFD)
{
	BOOL			bRtnValue	= FALSE;
	ULARGE_INTEGER	nLargeInt	= {0,};

	if (NULL != pstFD)
	{
		memset(pstFD, 0, sizeof(*pstFD));
	}

	if (((NULL == pstData) || (NULL == pstFD) || (NULL == lpszName)))
	{
		assert(FALSE);
		goto FINAL;
	}

	// http://inform.pucp.edu.pe/~inf232/Ntfs/ntfs_doc_v0.5/attributes/file_name.html
	// http://msdn.microsoft.com/en-us/library/windows/desktop/gg258117(v=vs.85).aspx
	// 변환
	if (ATTRIB_FILENAME_FLAG_READONLY == (ATTRIB_FILENAME_FLAG_READONLY & pstData->dwFileAttrib))		pstFD->dwFileAttributes |= FILE_ATTRIBUTE_READONLY;
	if (ATTRIB_FILENAME_FLAG_HIDDEN == (ATTRIB_FILENAME_FLAG_HIDDEN & pstData->dwFileAttrib))			pstFD->dwFileAttributes |= FILE_ATTRIBUTE_HIDDEN;
	if (ATTRIB_FILENAME_FLAG_SYSTEM == (ATTRIB_FILENAME_FLAG_SYSTEM & pstData->dwFileAttrib))			pstFD->dwFileAttributes |= FILE_ATTRIBUTE_SYSTEM;
	if (ATTRIB_FILENAME_FLAG_ARCHIVE == (ATTRIB_FILENAME_FLAG_ARCHIVE & pstData->dwFileAttrib))			pstFD->dwFileAttributes |= FILE_ATTRIBUTE_ARCHIVE;
	if (ATTRIB_FILENAME_FLAG_DEVICE == (ATTRIB_FILENAME_FLAG_DEVICE & pstData->dwFileAttrib))			pstFD->dwFileAttributes |= FILE_ATTRIBUTE_DEVICE;
	if (ATTRIB_FILENAME_FLAG_NORMAL == (ATTRIB_FILENAME_FLAG_NORMAL & pstData->dwFileAttrib))			pstFD->dwFileAttributes |= FILE_ATTRIBUTE_NORMAL;
	if (ATTRIB_FILENAME_FLAG_TEMP == (ATTRIB_FILENAME_FLAG_TEMP & pstData->dwFileAttrib))				pstFD->dwFileAttributes |= FILE_ATTRIBUTE_TEMPORARY;
	if (ATTRIB_FILENAME_FLAG_SPARSE == (ATTRIB_FILENAME_FLAG_SPARSE & pstData->dwFileAttrib))			pstFD->dwFileAttributes |= FILE_ATTRIBUTE_SPARSE_FILE;
	if (ATTRIB_FILENAME_FLAG_REPARSE == (ATTRIB_FILENAME_FLAG_REPARSE & pstData->dwFileAttrib))			pstFD->dwFileAttributes |= FILE_ATTRIBUTE_REPARSE_POINT;
	if (ATTRIB_FILENAME_FLAG_COMPRESSED == (ATTRIB_FILENAME_FLAG_COMPRESSED & pstData->dwFileAttrib))	pstFD->dwFileAttributes |= FILE_ATTRIBUTE_COMPRESSED;
	if (ATTRIB_FILENAME_FLAG_OFFLINE == (ATTRIB_FILENAME_FLAG_OFFLINE & pstData->dwFileAttrib))			pstFD->dwFileAttributes |= FILE_ATTRIBUTE_OFFLINE;
	if (ATTRIB_FILENAME_FLAG_NCI == (ATTRIB_FILENAME_FLAG_NCI & pstData->dwFileAttrib))					pstFD->dwFileAttributes |= FILE_ATTRIBUTE_NOT_CONTENT_INDEXED;
	if (ATTRIB_FILENAME_FLAG_ENCRYPTED == (ATTRIB_FILENAME_FLAG_ENCRYPTED & pstData->dwFileAttrib))		pstFD->dwFileAttributes |= FILE_ATTRIBUTE_ENCRYPTED;
	if (ATTRIB_FILENAME_FLAG_DIRECTORY == (ATTRIB_FILENAME_FLAG_DIRECTORY & pstData->dwFileAttrib))		pstFD->dwFileAttributes |= FILE_ATTRIBUTE_DIRECTORY;

	pstFD->ftCreationTime	= GetLocalFileTime(pstData->llCreationTime);
	pstFD->ftLastAccessTime	= GetLocalFileTime(pstData->llLastAccessTime);
	pstFD->ftLastWriteTime  = GetLocalFileTime(pstData->llLastDataChangeTime);

	nLargeInt.QuadPart = pstData->nFileSize;
	pstFD->nFileSizeHigh = nLargeInt.HighPart;
	pstFD->nFileSizeLow = nLargeInt.LowPart;
	StringCchCopyW(pstFD->cFileName, MAX_PATH, lpszName);

	// 여기까지 왔다면 성공
	bRtnValue = TRUE;

FINAL:
	return bRtnValue;
}

LPVOID CNTFSHelper::CREATE_LOCK(VOID)
{
	LPVOID pLock = NULL;
#ifdef _WIN32
	pLock = (PCRITICAL_SECTION)malloc(sizeof(CRITICAL_SECTION));
	if (NULL == pLock)
	{
		assert(FALSE);
		goto FINAL;
	}
	memset(pLock, 0, sizeof(CRITICAL_SECTION));
	if (FALSE == ::InitializeCriticalSectionAndSpinCount((PCRITICAL_SECTION)pLock, 4000))
	{
		assert(FALSE);
		goto FINAL;
	}
#endif
FINAL:
	return pLock;
}

VOID CNTFSHelper::DESTROY_LOCK(IN LPVOID pLock)
{
#ifdef _WIN32
	if (NULL != pLock)
	{
		::DeleteCriticalSection((PCRITICAL_SECTION)pLock);
		free(pLock);
		pLock = NULL;
	}
#endif
}

VOID CNTFSHelper::LOCK(IN LPVOID pLock)
{
#ifdef _WIN32
	if (NULL != pLock)
	{
		::EnterCriticalSection((PCRITICAL_SECTION)pLock);
	}
#endif
}

VOID CNTFSHelper::UNLOCK(IN LPVOID pLock)
{
#ifdef _WIN32
	if (NULL != pLock)
	{
		::LeaveCriticalSection((PCRITICAL_SECTION)pLock);
	}
#endif
}

// Win32 API인 CreateFile을 emulation한다. (Read-only)
// NOT_USED가 명시된 argument는 내부에서 사용하지 않으니 주의하기 바람.
// dwDesiredAccess에 GENERIC_WRITE는 지원되지 않는다. (ERROR_NOT_SUPPORTED)
// dwCreationDisposition에 OPEN_EXISTING 이외의 값은 지원되지 않는다. (ERROR_NOT_SUPPORTED)
CNTFSHelper::NTFS_HANDLE CNTFSHelper::CreateFile(IN LPCWSTR lpFileName, IN DWORD dwDesiredAccess, NOT_USED IN DWORD dwShareMode, NOT_USED IN LPSECURITY_ATTRIBUTES lpSecurityAttributes, IN DWORD dwCreationDisposition, NOT_USED IN DWORD dwFlagsAndAttributes, NOT_USED IN HANDLE hTemplateFile)
{
	// File을 Open하는 것은
	// $DATA 속성 값을 사용한다는 뜻이다.
	return CreateFileByAttrib(lpFileName, dwDesiredAccess, dwShareMode, lpSecurityAttributes, dwCreationDisposition, dwFlagsAndAttributes, hTemplateFile, TYPE_ATTRIB_DATA, NULL);
}

CNTFSHelper::NTFS_HANDLE WINAPI CNTFSHelper::CreateFileIo(IN LPCWSTR lpFileName, IN DWORD dwDesiredAccess, NOT_USED IN DWORD dwShareMode, NOT_USED IN LPSECURITY_ATTRIBUTES lpSecurityAttributes, IN DWORD dwCreationDisposition, NOT_USED IN DWORD dwFlagsAndAttributes, NOT_USED IN HANDLE hTemplateFile, OPTIONAL IN LPST_IO_FUNCTION pstIoFunction)
{
	// 외부 I/O 함수 전달
	return CreateFileByAttrib(lpFileName, dwDesiredAccess, dwShareMode, lpSecurityAttributes, dwCreationDisposition, dwFlagsAndAttributes, hTemplateFile, TYPE_ATTRIB_DATA, pstIoFunction);
}

// 실질적인 CreateFile의 Main함수이다.
// File은 여러개의 Attribute가 있는데,
// 해당 Attribute의 값을 읽는데 사용된다.
// 일반적인 File 내용은 $DATA이다. (TYPE_ATTRIB_DATA)
CNTFSHelper::NTFS_HANDLE CNTFSHelper::CreateFileByAttrib(IN LPCWSTR lpFileName, IN DWORD dwDesiredAccess, NOT_USED IN DWORD dwShareMode, NOT_USED IN LPSECURITY_ATTRIBUTES lpSecurityAttributes, IN DWORD dwCreationDisposition, NOT_USED IN DWORD dwFlagsAndAttributes, NOT_USED IN HANDLE hTemplateFile, IN TYPE_ATTRIB nTypeAttrib, OPTIONAL IN LPST_IO_FUNCTION pstIoFunction)
{
	NTFS_HANDLE			hRtnValue		= INVALID_HANDLE_VALUE_NTFS;
	DWORD				dwErrorCode		= ERROR_INVALID_PARAMETER;
	LPST_WIN32_HANDLE	pstHandle		= NULL;
	LPWSTR				lpszPath		= NULL;
	CHAR				szVolume[7]		= {0,};
	BOOL				bFindSubDir		= FALSE;
	LPST_INODEDATA		pstInodeData	= {0,};
	LPST_FIND_ATTRIB	pstAttribAlloc	= NULL;
	INT					i				= 0;

	if (NULL == lpFileName)
	{
		assert(FALSE);
		goto FINAL;
	}

	if (GENERIC_WRITE == (dwDesiredAccess & GENERIC_WRITE))
	{
		// 쓰기 권한은 지원되지 않는다.
		dwErrorCode = ERROR_NOT_SUPPORTED;
		assert(FALSE);
		goto FINAL;
	}

	if (OPEN_EXISTING != dwCreationDisposition)
	{
		dwErrorCode = ERROR_NOT_SUPPORTED;
		assert(FALSE);
		goto FINAL;
	}

	pstHandle = (LPST_WIN32_HANDLE)malloc(sizeof(*pstHandle));
	lpszPath  = (LPWSTR)malloc(sizeof(WCHAR)*(MAX_PATH_NTFS+1));
	pstInodeData = (LPST_INODEDATA)malloc(sizeof(*pstInodeData));
	if ((NULL == pstHandle) || (NULL == lpszPath) || (NULL == pstInodeData))
	{
		dwErrorCode = ERROR_NOT_ENOUGH_MEMORY;
		assert(FALSE);
		goto FINAL;
	}
	memset(pstHandle, 0, sizeof(*pstHandle));
	memset(lpszPath, 0, sizeof(WCHAR)*(MAX_PATH_NTFS+1));
	memset(pstInodeData, 0, sizeof(*pstInodeData));

	// 볼륨, 경로를 구한다.
	StringCchCopyA(szVolume, 7, "\\\\.\\_:");	// _ 문자는 앞으로 a~z 사이의 값으로 치환될 것임
	dwErrorCode = GetFindFirstFilePath(lpFileName, lpszPath, MAX_PATH_NTFS, &szVolume[4], &bFindSubDir);
	if (TRUE == bFindSubDir)
	{
		// CreateFile에는 FindFirstFile처럼 *나 *.*등이 사용되지 않는다.
		dwErrorCode = ERROR_INVALID_PARAMETER;
		assert(FALSE);
		goto FINAL;
	}

	if (ERROR_SUCCESS != dwErrorCode)
	{
		dwErrorCode = ERROR_INVALID_PARAMETER;
		assert(FALSE);
		goto FINAL;
	}

	// CreateFile 관련 Handle이다.
	pstHandle->nType = TYPE_WIN32_HANDLE_CREATEFILE;

	// Lock을 생성하자.
	pstHandle->pLock = CREATE_LOCK();

	// Ntfs를 Open한다.
	if (NULL == pstIoFunction)
	{
		pstHandle->pstNtfs = OpenNtfs(szVolume);
	}
	else
	{
		// 외부 I/O 지원
		pstHandle->pstNtfs = OpenNtfsIo(szVolume, pstIoFunction);
	}

	if (NULL == pstHandle->pstNtfs)
	{
		dwErrorCode = ERROR_OPEN_FAILED;
		assert(FALSE);
		goto FINAL;
	}

	// Path의 inode를 찾는다.
	// 시간이 제법 걸린다.
	dwErrorCode = FindInodeFromPath(pstHandle->pstNtfs, lpszPath, &pstHandle->stCreateFile.nInode, pstInodeData);
	if (ERROR_SUCCESS != dwErrorCode)
	{
		if (ERROR_FILE_NOT_FOUND != dwErrorCode)
		{
			assert(FALSE);
		}
		goto FINAL;
	}

	// $DATA를 연다.
	pstAttribAlloc = FindAttribAlloc(pstHandle->pstNtfs, pstHandle->stCreateFile.nInode, nTypeAttrib);
	if (NULL == pstAttribAlloc)
	{
		dwErrorCode = ERROR_FILE_CORRUPT;
		assert(FALSE);
		goto FINAL;
	}

	if (1 == pstAttribAlloc->pstAttrib->cNonResident)
	{
		// non-Resident.

		// 만약 압축이 이뤄졌다면,...
		// [TBD]
		// 아직 compressed는 지원하지 않는다.
		if (0 != pstAttribAlloc->pstAttrib->stAttr.stNonResident.wCompressionSize)
		{
			dwErrorCode = ERROR_NOT_SUPPORTED;
			assert(FALSE);
			goto FINAL;
		}

		// Runlist를 구해 값을 가져온다.
		pstHandle->stCreateFile.bResident = FALSE;

		// Attribute 값의 크기를 가져온다.
		pstHandle->stCreateFile.nFileSize = pstAttribAlloc->pstAttrib->stAttr.stNonResident.llDataSize;
	}
	else
	{
		// Resident.
		pstHandle->stCreateFile.bResident = TRUE;

		// Attribute 값의 크기를 가져온다.
		pstHandle->stCreateFile.nFileSize = pstAttribAlloc->pstAttrib->stAttr.stResident.dwValueLength;
	}

	if (TRUE == pstHandle->stCreateFile.bResident)
	{
		// Resident data이라면,...
		// 그리 용량이 크지 않을 것이기 때문에,
		// 그냥 파일 내용을 가져와 복사하자.

		// 우선 버퍼 크기를 결정한다.
		pstHandle->stCreateFile.nBufResidentDataSize = pstAttribAlloc->pstAttrib->stAttr.stResident.dwValueLength;

		// [TBD]
		// pstHandle->stCreateFile.nBufResidentDataSize
		// 의 길이가 File record 범위가 벗어나는지 조사

		// 버퍼를 할당한다.
		pstHandle->stCreateFile.pBufResidentData = (UBYTE1*)malloc(pstHandle->stCreateFile.nBufResidentDataSize);
		if (NULL == pstHandle->stCreateFile.pBufResidentData)
		{
			dwErrorCode = ERROR_FILE_CORRUPT;
			assert(FALSE);
			goto FINAL;
		}
		memset(pstHandle->stCreateFile.pBufResidentData, 0, pstHandle->stCreateFile.nBufResidentDataSize);

		// Resident data를 가져온다.
		memcpy(pstHandle->stCreateFile.pBufResidentData, (UBYTE1*)pstAttribAlloc->pstAttrib + pstAttribAlloc->pstAttrib->stAttr.stResident.wValueOffset, pstHandle->stCreateFile.nBufResidentDataSize);
	}
	else
	{
		// non-Resident data이라면,...
		// Runlist를 가져오자.
		pstHandle->stCreateFile.pstRunList = GetRunListAlloc(GetRunListByte(pstAttribAlloc->pstAttrib), &pstHandle->stCreateFile.nCountRunList);
		if (NULL == pstHandle->stCreateFile.pstRunList)
		{
			dwErrorCode = ERROR_FILE_CORRUPT;
			assert(FALSE);
			goto FINAL;
		}

		// RunList를 통한 Vcn 개수를 구한다.
		for (i=0; i<pstHandle->stCreateFile.nCountRunList; i++)
		{
			pstHandle->stCreateFile.nCountVcn += pstHandle->stCreateFile.pstRunList[i].llLength;
		}

		// Attribute값에 표기된 Vcn 개수와 비교한다.
		if (pstAttribAlloc->pstAttrib->stAttr.stNonResident.llEndVCN - pstAttribAlloc->pstAttrib->stAttr.stNonResident.llStartVCN + 1 != pstHandle->stCreateFile.nCountVcn)
		{
			dwErrorCode = ERROR_FILE_CORRUPT;
			assert(FALSE);
			goto FINAL;
		}
	}

	// 여기까지 왔다면 성공
	// 즉,
	// $DATA가 resident라면,
	//  - buf 생성후 file data 복사
	// $DATA가 non-resident라면,
	//  - runlist 생성 성공
	// 상태이다.

	hRtnValue = pstHandle;

FINAL:

	if (NULL != pstAttribAlloc)
	{
		FindAttribFree(pstAttribAlloc);
		pstAttribAlloc = NULL;
	}

	if (NULL != pstInodeData)
	{
		free(pstInodeData);
		pstInodeData = NULL;
	}

	if (NULL != lpszPath)
	{
		free(lpszPath);
		lpszPath = NULL;
	}

	if (INVALID_HANDLE_VALUE_NTFS == hRtnValue)
	{
		// 실패
		if (NULL != pstHandle)
		{
			if (NULL != pstHandle->stCreateFile.pstRunList)
			{
				GetRunListFree(pstHandle->stCreateFile.pstRunList);
				pstHandle->stCreateFile.pstRunList = NULL;
			}

			if (NULL != pstHandle->stCreateFile.pBufResidentData)
			{
				free(pstHandle->stCreateFile.pBufResidentData);
				pstHandle->stCreateFile.pBufResidentData = NULL;
			}

			if (NULL != pstHandle->pstNtfs)
			{
				CloseNtfs(pstHandle->pstNtfs);
				pstHandle->pstNtfs = NULL;
			}

			if (NULL != pstHandle->pLock)
			{
				DESTROY_LOCK(pstHandle->pLock);
				pstHandle->pLock = NULL;
			}

			free(pstHandle);
			pstHandle = NULL;
		}

		// 오류 발생
		::SetLastError(dwErrorCode);
	}
	return hRtnValue;
}

// WIn32 API Emulation
// lpNumberOfBytesRead는 NULL이 전달되어서는 안된다.
BOOL CNTFSHelper::ReadFile(IN NTFS_HANDLE hFile, OUT LPVOID lpBuffer, IN DWORD nNumberOfBytesToRead, OUT LPDWORD lpNumberOfBytesRead, NOT_USED OPTIONAL IN OUT LPOVERLAPPED lpOverlapped)
{
	BOOL				bRtnValue	= FALSE;
	DWORD				dwErrorCode	= ERROR_INVALID_PARAMETER;
	LPST_WIN32_HANDLE	pstHandle	= NULL;
	UBYTE8				nVcn		= 0;
	INT					nOffset		= 0;
	UBYTE8				nLcn		= 0;
	DWORD				dwBufferPos	= 0;

	if (NULL != lpNumberOfBytesRead)
	{
		*lpNumberOfBytesRead = 0;
	}

	if ((NULL == hFile) || (INVALID_HANDLE_VALUE_NTFS == hFile) || (NULL == lpBuffer) || (0 == nNumberOfBytesToRead) || (NULL == lpNumberOfBytesRead))
	{
		assert(FALSE);
		::SetLastError(dwErrorCode);
		return FALSE;
	}

	pstHandle = (LPST_WIN32_HANDLE)hFile;
	if (TYPE_WIN32_HANDLE_CREATEFILE != pstHandle->nType)
	{
		// CreateFile로 만들어진 handle이 아닌 경우,...
		dwErrorCode = ERROR_INVALID_HANDLE;
		assert(FALSE);
		::SetLastError(dwErrorCode);
		return FALSE;
	}

	if (TRUE == pstHandle->stCreateFile.bErrorOccurred)
	{
		// 한번이라도 오류가 발생했다면,....
		dwErrorCode = ERROR_FILE_INVALID;
		assert(FALSE);
		::SetLastError(dwErrorCode);
		return FALSE;
	}

	//////////////////////////////////////////////////////////////////////////
	// Lock
	LOCK(pstHandle->pLock);

	// 만약 File 크기를 벗어나려면,...
	if (pstHandle->stCreateFile.nFilePointer >= pstHandle->stCreateFile.nFileSize)
	{
		// 즉, 끝까지 갔다는 뜻인데,
		// TRUE를 리턴하고, 0byte를 읽었다고 전달하더라.
		bRtnValue = TRUE;
		*lpNumberOfBytesRead = 0;
		goto FINAL;
	}

	if (pstHandle->stCreateFile.nFilePointer + nNumberOfBytesToRead > pstHandle->stCreateFile.nFileSize)
	{
		// 만일 읽을 크기가 File 크기를 벗어나는 경우,...
		// 읽을 크기를 조정한다.
		nNumberOfBytesToRead = (DWORD)(pstHandle->stCreateFile.nFileSize - pstHandle->stCreateFile.nFilePointer);
	}

	if (TRUE == pstHandle->stCreateFile.bResident)
	{
		// resident인 경우,...

		// resident 메모리를 복사한다.
		memcpy(lpBuffer, &pstHandle->stCreateFile.pBufResidentData[pstHandle->stCreateFile.nFilePointer], nNumberOfBytesToRead);

		// 읽은 크기를 전달한다.
		*lpNumberOfBytesRead = nNumberOfBytesToRead;

		// file pointer를 뒤로 옮긴다.
		pstHandle->stCreateFile.nFilePointer += nNumberOfBytesToRead;

		// 성공
		bRtnValue = TRUE;

		goto FINAL;
	}

	// non-Resident인 경우,...

	if (NULL == pstHandle->stCreateFile.pBufCluster)
	{
		// 아직, buffer가 생성되지 않았다.
		pstHandle->stCreateFile.pBufCluster = (UBYTE1*)malloc(pstHandle->pstNtfs->stNtfsInfo.dwClusterSize);
		if (NULL == pstHandle->stCreateFile.pBufCluster)
		{
			bRtnValue = FALSE;
			dwErrorCode = ERROR_NOT_ENOUGH_MEMORY;
			assert(FALSE);
			goto FINAL;
		}
		memset(pstHandle->stCreateFile.pBufCluster, 0, pstHandle->pstNtfs->stNtfsInfo.dwClusterSize);
	}

	for (;;)
	{
		// 해당 file pointer 위치의 vcn은 어디일까?
		nVcn = pstHandle->stCreateFile.nFilePointer / pstHandle->pstNtfs->stNtfsInfo.dwClusterSize;

		// vcn 위치에서, offset은 얼마일까?
		nOffset = pstHandle->stCreateFile.nFilePointer % pstHandle->pstNtfs->stNtfsInfo.dwClusterSize;

		// 그럼, nVcn에 대한 Lcn값을 구하자.
		if (FALSE == GetLcnFromVcn(pstHandle->stCreateFile.pstRunList, 
								   pstHandle->stCreateFile.nCountRunList, 
								   nVcn, 
								   0, 
								   &nLcn))
		{
			bRtnValue = FALSE;
			dwErrorCode = ERROR_FILE_INVALID;
			assert(FALSE);
			goto FINAL;
		}

		// 이제, Lcn값을 구했으니,
		// Lcn + offset 부터 값을 읽자.
		if (FALSE == SeekLcn(pstHandle->pstNtfs, nLcn))
		{
			// Lcn 위치로 간다.
			dwErrorCode = ERROR_FILE_INVALID;
			assert(FALSE);
			goto FINAL;
		}

		if (pstHandle->pstNtfs->stNtfsInfo.dwClusterSize != ReadIo(pstHandle->pstNtfs->pstIo, pstHandle->stCreateFile.pBufCluster, pstHandle->pstNtfs->stNtfsInfo.dwClusterSize))
		{
			// 읽는데 실패함
			dwErrorCode = ERROR_FILE_INVALID;
			assert(FALSE);
			goto FINAL;
		}

		// 읽는데 성공하였다.
		if (nNumberOfBytesToRead - dwBufferPos > pstHandle->pstNtfs->stNtfsInfo.dwClusterSize - nOffset)
		{
			// 남은 양이 Cluster 보다 클때,...
			memcpy((UBYTE1*)lpBuffer + dwBufferPos, &pstHandle->stCreateFile.pBufCluster[nOffset], pstHandle->pstNtfs->stNtfsInfo.dwClusterSize - nOffset);

			// File Pointer를 뒤로 옮긴다.
			pstHandle->stCreateFile.nFilePointer += pstHandle->pstNtfs->stNtfsInfo.dwClusterSize - nOffset;

			// lpBuffer의 위치를 뒤로 옮긴다.
			dwBufferPos += pstHandle->pstNtfs->stNtfsInfo.dwClusterSize - nOffset;

			continue;
		}

		// 남은 양이 Cluster 보다 작을 떄,...
		// Loop의 마지막이다.
		memcpy((UBYTE1*)lpBuffer + dwBufferPos, &pstHandle->stCreateFile.pBufCluster[nOffset], nNumberOfBytesToRead - dwBufferPos);

		// 읽었다.
		pstHandle->stCreateFile.nFilePointer += nNumberOfBytesToRead - dwBufferPos;

		break;
	}

	// 읽은 크기
	*lpNumberOfBytesRead = nNumberOfBytesToRead;

	// 여기까지 왔다면 성공
	bRtnValue = TRUE;

FINAL:

	UNLOCK(pstHandle->pLock);
	// UnLock
	//////////////////////////////////////////////////////////////////////////

	if (FALSE == bRtnValue)
	{
		// 실패
		::SetLastError(dwErrorCode);

		if (NULL != pstHandle)
		{
			pstHandle->stCreateFile.bErrorOccurred = TRUE;
		}
	}
	return bRtnValue;
}

BOOL CNTFSHelper::CloseHandle(IN NTFS_HANDLE hObject)
{
	BOOL				bRtnValue	= FALSE;
	DWORD				dwErrorCode	= ERROR_INVALID_PARAMETER;
	LPST_WIN32_HANDLE	pstHandle	= NULL;
	LPVOID				pLock		= NULL;

	if ((INVALID_HANDLE_VALUE_NTFS == hObject) || (NULL == hObject))
	{
		dwErrorCode = ERROR_INVALID_HANDLE;
		// 굳이 assert 하지는 말자.
		// assert(FALSE);
		::SetLastError(dwErrorCode);
		return FALSE;
	}

	pstHandle = (LPST_WIN32_HANDLE)hObject;
	if (TYPE_WIN32_HANDLE_FINDFIRSTFILE == pstHandle->nType)
	{
		return FindClose(hObject);
	}

	if (TYPE_WIN32_HANDLE_CREATEFILE == pstHandle->nType)
	{
		// good
	}
	else
	{
		// bad
		dwErrorCode = ERROR_INVALID_HANDLE;
		assert(FALSE);
		::SetLastError(dwErrorCode);
		return FALSE;
	}

	//////////////////////////////////////////////////////////////////////////
	// LOCK
	pLock = pstHandle->pLock;
	LOCK(pLock);

	if (TYPE_WIN32_HANDLE_CREATEFILE == pstHandle->nType)
	{
		if (NULL != pstHandle->pstNtfs)
		{
			CloseNtfs(pstHandle->pstNtfs);
			pstHandle->pstNtfs = NULL;
		}

		if (NULL != pstHandle->stCreateFile.pstRunList)
		{
			GetRunListFree(pstHandle->stCreateFile.pstRunList);
			pstHandle->stCreateFile.pstRunList = NULL;
		}

		if (NULL != pstHandle->stCreateFile.pBufCluster)
		{
			free(pstHandle->stCreateFile.pBufCluster);
			pstHandle->stCreateFile.pBufCluster = NULL;
		}

		if (NULL != pstHandle->stCreateFile.pBufResidentData)
		{
			free(pstHandle->stCreateFile.pBufResidentData);
			pstHandle->stCreateFile.pBufResidentData = NULL;
		}

		// Handle 메모리 해제한다.
		free(pstHandle);
		pstHandle = NULL;

		bRtnValue = TRUE;
	}

// 사용되지 않음
// FINAL:

	UNLOCK(pLock);
	DESTROY_LOCK(pLock);
	pLock = NULL;

	// UNLOCK
	//////////////////////////////////////////////////////////////////////////
	return bRtnValue;
}

DWORD CNTFSHelper::GetFileSize(IN NTFS_HANDLE hFile, OPTIONAL OUT LPDWORD lpFileSizeHigh)
{
	DWORD				dwRtnValue	= 0;
	DWORD				dwErrorCode	= ERROR_INVALID_PARAMETER;
	LPST_WIN32_HANDLE	pstHandle	= NULL;
	ULARGE_INTEGER		nLargeInt	= {0,};

	if (INVALID_HANDLE_VALUE_NTFS == hFile)
	{
		dwErrorCode = ERROR_INVALID_HANDLE;
		::SetLastError(dwErrorCode);
		return 0;
	}

	pstHandle = (LPST_WIN32_HANDLE)hFile;
	if (TYPE_WIN32_HANDLE_CREATEFILE != pstHandle->nType)
	{
		dwErrorCode = ERROR_INVALID_HANDLE;
		::SetLastError(dwErrorCode);
		return 0;
	}

	//////////////////////////////////////////////////////////////////////////
	// LOCK
	LOCK(pstHandle->pLock);

	// CreateFile 할 당시의 크기를 전달하자.
	//
	// [TBD]
	// Win32 API에서는 어떻게 동작하는가?
	// 즉, CreateFile 할 당시의 크기인가?
	// GetFileSize를 호출할 싯점의 크기인가?
	nLargeInt.QuadPart = pstHandle->stCreateFile.nFileSize;

	dwRtnValue = nLargeInt.LowPart;

	if (NULL != lpFileSizeHigh)
	{
		*lpFileSizeHigh = nLargeInt.HighPart;
	}

// 사용되지 않음
// FINAL:

	UNLOCK(pstHandle->pLock);
	// UNLOCK
	//////////////////////////////////////////////////////////////////////////
	return dwRtnValue;
}

DWORD CNTFSHelper::SetFilePointer(IN NTFS_HANDLE hFile, IN LONG lDistanceToMove, OPTIONAL IN OUT PLONG lpDistanceToMoveHigh, IN DWORD dwMoveMethod)
{
	DWORD				dwRtnValue	= 0;
	DWORD				dwErrorCode	= ERROR_INVALID_PARAMETER;
	LPST_WIN32_HANDLE	pstHandle	= NULL;
	BOOL				bBack		= FALSE;
	LARGE_INTEGER		nLargeInt	= {0,};
	ULARGE_INTEGER		unLargeInt	= {0,};
	UBYTE8				nFilePointer= 0;

	if (INVALID_HANDLE_VALUE_NTFS == hFile)
	{
		dwErrorCode = ERROR_INVALID_HANDLE;
		::SetLastError(dwErrorCode);
		return INVALID_SET_FILE_POINTER;
	}

	pstHandle = (LPST_WIN32_HANDLE)hFile;
	if (TYPE_WIN32_HANDLE_CREATEFILE != pstHandle->nType)
	{
		dwErrorCode = ERROR_INVALID_HANDLE;
		::SetLastError(dwErrorCode);
		return INVALID_SET_FILE_POINTER;
	}

	if ((FILE_BEGIN   == dwMoveMethod) ||
		(FILE_CURRENT == dwMoveMethod) ||
		(FILE_END     == dwMoveMethod))
	{
		// good
	}
	else
	{
		dwErrorCode = ERROR_INVALID_PARAMETER;
		::SetLastError(dwErrorCode);
		return INVALID_SET_FILE_POINTER;
	}

	//////////////////////////////////////////////////////////////////////////
	// LOCK
	LOCK(pstHandle->pLock);

	if (lDistanceToMove < 0)
	{
		bBack = TRUE;
		lDistanceToMove = -lDistanceToMove;
	}

	nLargeInt.LowPart = lDistanceToMove;
	if (NULL != lpDistanceToMoveHigh)
	{
		nLargeInt.HighPart = *lpDistanceToMoveHigh;
	}

	// if (pstHandle->stCreateFile.nFileSize < nLargeInt.QuadPart)
	{
		// 실제 Win32 API에서,
		// File 크기보다 큰 위치를 요구하는 경우에도
		// 성공을 전달하였다.
		// do nothing...
		// not alert
	}

	if (TRUE == bBack)
	{
		nLargeInt.QuadPart = -nLargeInt.QuadPart;
	}

	// File Pointer를 설정한다.
	nFilePointer = pstHandle->stCreateFile.nFilePointer;
	if (FILE_BEGIN == dwMoveMethod)
	{
		nFilePointer = nLargeInt.QuadPart;
	}
	else if (FILE_CURRENT == dwMoveMethod)
	{
		nFilePointer += nLargeInt.QuadPart;
	}
	else if (FILE_END == dwMoveMethod)
	{
		nFilePointer = pstHandle->stCreateFile.nFileSize + nLargeInt.QuadPart;
	}

	if ((LONGLONG)nFilePointer < 0)
	{
		dwRtnValue  = INVALID_SET_FILE_POINTER;
		dwErrorCode = ERROR_NEGATIVE_SEEK;
		goto FINAL;
	}

	// 여기까지 왔다면 성공
	pstHandle->stCreateFile.nFilePointer = nFilePointer;

	// 리턴값 결정.
	// unsigned로 변환
	unLargeInt.QuadPart = pstHandle->stCreateFile.nFilePointer;

	dwRtnValue = unLargeInt.LowPart;
	if (NULL != lpDistanceToMoveHigh)
	{
		*lpDistanceToMoveHigh = unLargeInt.HighPart;
	}

FINAL:
	UNLOCK(pstHandle->pLock);
	// UNLOCK
	//////////////////////////////////////////////////////////////////////////

	if (INVALID_SET_FILE_POINTER == dwRtnValue)
	{
		::SetLastError(dwErrorCode);
	}

	return dwRtnValue;
}
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
// [TBD] multi-platform(os) ����
#include <strsafe.h>

CNTFSHelper::CNTFSHelper(void)
{
}

CNTFSHelper::~CNTFSHelper(void)
{
}

CNTFSHelper::LPST_NTFS CNTFSHelper::OpenNtfs(IN LPCSTR lpszPath)
{
	// �Ϲ����� ���� I/O�� �̿��Ͽ� ȣ���Ѵ�.
	return OpenNtfsIo(lpszPath, NULL);
}

CNTFSHelper::LPST_NTFS CNTFSHelper::OpenNtfsIo(IN LPCSTR lpszPath, IN LPST_IO_FUNCTION pstIoFunction)
{
	LPST_NTFS	pstRtnValue	= NULL;
	ST_NTFS		stNtfs		= {0,};
	_OFF64_T	nSize		= 0;

	// LocalDisk�� Open�Ѵ�.
	if (NULL == pstIoFunction)
	{
		// �ϻ�����, ���� I/O �Լ� ���
		stNtfs.pstIo = OpenIo(lpszPath);
	}
	else
	{
		// �Ϲ������� ����, �ܺ� I/O �Լ� ���
		stNtfs.pstIo = OpenUserIo(lpszPath, pstIoFunction);
	}

	if (NULL == stNtfs.pstIo)
	{
		ERRORLOG("Fail to Open");
		assert(FALSE);
		goto FINAL;
	}
	
	// NTFS�� BPB�� ���Ѵ�.
	if (FALSE == GetBootSectorBPB(&stNtfs, &stNtfs.stBpb))
	{
		ERRORLOG("Fail to Get BPB");
		assert(FALSE);
		goto FINAL;
	}

	//////////////////////////////////////////////////////////////////////////
	// ���� NTFS INFO�� ä���.

	// cluster size�� �����Ѵ�.
	stNtfs.stNtfsInfo.dwClusterSize = stNtfs.stBpb.wBytePerSector * stNtfs.stBpb.cSectorPerCluster;
	stNtfs.stNtfsInfo.wSectorSize   = stNtfs.stBpb.wBytePerSector;

	// MFT�� ũ�⸦ ���Ѵ�.
	// ������ ��쿡��,
	// http://inform.pucp.edu.pe/~inf232/Ntfs/ntfs_doc_v0.5/files/boot.html �� (c) ����
	// ntfsprogs �ڵ� : ntfs_boot_sector_parse(...) ����

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
		// MFT ũ��� Sector ũ���� ������� ��
		assert(FALSE);
		goto FINAL;
	}
	stNtfs.stNtfsInfo.dwMftSectorCount = stNtfs.stNtfsInfo.dwMftSize / stNtfs.stNtfsInfo.wSectorSize;

	// MFT�� ������ �о�� �ϴ� Buffer�� ũ�⸦ ���Ѵ�.
	stNtfs.stNtfsInfo.dwReadBufSizeForMft = stNtfs.stNtfsInfo.dwMftSize;

	if (0 != stNtfs.stNtfsInfo.dwMftSize % stNtfs.stNtfsInfo.wSectorSize)
	{
		// Mft ũ�Ⱑ Sector ũ��� ������������ �ʴ´ٸ�,...
		assert(FALSE);
		goto FINAL;
	}

	// Mft�� Sector pos�� �̸� ���Ѵ�.
	stNtfs.stNtfsInfo.llMftSectorPos = stNtfs.stBpb.llMftLcn * stNtfs.stBpb.cSectorPerCluster;

	// ũ�� ���
	nSize = GetDiskSize(&stNtfs);
	if ((0 != nSize) &&
		(0 == (nSize % stNtfs.stNtfsInfo.dwClusterSize)))
	{
		// �ּ� ũ�Ⱑ 0 �̻��̰�,
		// Cluster ũ�⿡ ����ؾ� �Ѵ�.
		// good
	}
	else
	{
		// bad
		assert(FALSE);
		goto FINAL;
	}

	// ��ü Cluster ������ ���Ѵ�.
	stNtfs.stNtfsInfo.nTotalClusterCount = nSize / stNtfs.stNtfsInfo.dwClusterSize;

	// ������� �Դٸ� ����
	pstRtnValue = (LPST_NTFS)malloc(sizeof(ST_NTFS));
	if (NULL == pstRtnValue)
	{
		assert(FALSE);
		goto FINAL;
	}
	memset(pstRtnValue, 0, sizeof(ST_NTFS));

	// ����!
	*pstRtnValue = stNtfs;

FINAL:
	if (NULL == pstRtnValue)
	{
		// �����ߴٸ�,...
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
		// �̹� Cache�� �����Ǿ���
		assert(FALSE);
		goto FINAL;
	}

	if (NULL != pstNtfs->pstInodeCache)
	{
		// callback ������ NULL�̿��� �Ѵ�.
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
		// cache ���� ����
		assert(FALSE);
		goto FINAL;
	}

	// ������ �����Ѵ�.
	pstNtfs->pstInodeCache->pfnCreateInodeCache = pfnCreateInodeCache;
	pstNtfs->pstInodeCache->pfnDestroyCache		= pfnDestroyCache;
	pstNtfs->pstInodeCache->pfnGetCache			= pfnGetCache;
	pstNtfs->pstInodeCache->pfnSetCache			= pfnSetCAche;
	pstNtfs->pstInodeCache->pUserContext		= pUserContext;

	// ������� �Դٸ� ����
	bRtnValue = TRUE;

FINAL:
	if (FALSE == bRtnValue)
	{
		// ����
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
			// cache�� �����ִ� ���,...
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

		// Close�Ѵ�.
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

	// ���� ����� �Լ��� ȣ���Ѵ�.
	stIo.pUserIoHandle	= (*stIo.stIoFunction.pfnOpenIo)(lpszPath, pstIoFunction->pUserIoContext);
	if (NULL == stIo.pUserIoHandle)
	{
		// ���и� �����ߴ�.
		assert(FALSE);
		goto FINAL;
	}

	pstRtnValue = (LPST_IO)malloc(sizeof(*pstRtnValue));
	if (NULL == pstRtnValue)
	{
		assert(FALSE);
		goto FINAL;
	}

	// ������
	*pstRtnValue = stIo;

FINAL:
	if (NULL == pstRtnValue)
	{
		// ����
		if (NULL != stIo.pUserIoHandle)
		{
			// Open�Ǿ��ٸ� Close�Ѵ�.
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
		// _open_osfhandle�� �����Ǵ� Visual Studio 2005 ����...
		// FILE_FLAG_NO_BUFFERING
		// �ɼ��� �̿��Ͽ� Disk�� ����, �ش� handle�� �����Ѵ�.
		// hDisk�� CloseHandle�� �ʿ䰡 ���µ�,
		// ����� fd�� _close�Ǹ� CloseHandle�ȴ�.
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
		// ����...
		if (stIo.nFd <= -1)
		{
			// _open���� ����
			if (INVALID_HANDLE_VALUE != hDisk)
			{
				::CloseHandle(hDisk);
				hDisk = INVALID_HANDLE_VALUE;
			}
		}
		else
		{
			// hDisk�� �Բ� �ڵ����� ::CloseHandle()�ȴ�.
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
			// �������� ����...
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
			// �������� ����...
			assert(FALSE);
		}
		else if (NULL != pstIo->stIoFunction.pfnCloseIo)
		{
			// User Define Close �Լ��� ȣ���Ѵ�.
			bRtnValue = (*pstIo->stIoFunction.pfnCloseIo)(pstIo->pUserIoHandle);
			pstIo->pUserIoHandle = NULL;
		}
	}
	else
	{
		// �������� ����
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
		// User Define Close �Լ��� ȣ���Ѵ�.
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
		// User Define Close �Լ��� ȣ���Ѵ�.
		bRtnValue = (*pstIo->stIoFunction.pfnSeekIo)(pstIo->pUserIoHandle, nPos, nOrigin);
	}
	else
	{
		assert(FALSE);
		goto FINAL;
	}

	// ������� �Դٸ� ����
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
		// nByte�� sector ũ���� ������� �Ѵ�.
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

	// BPB�� ���� ������ �⺻ Sector ũ��� �����´�.
	if (sizeof(stBootSector) != ReadIo(pstNtfs->pstIo, (UBYTE1*)&stBootSector, sizeof(stBootSector)))
	{
		bRtnValue = FALSE;
		goto FINAL;
	}

	// OEMID�� NTFS�� �����ؾ� �Ѵ�.
	if (0 != strncmp((char*)stBootSector.szOemId, "NTFS\x20\x20\x20\x20", 8))
	{
		bRtnValue = FALSE;
		ERRORLOG("Invalid oemid");
		goto FINAL;
	}

	// 0xaa55�� ������ �Ѵ�.
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

// 8byte�� �޾�, ���� n byte ��(1~8)�� �����Ѵ�.
//     0x1234567890abcdef�� ���� ��� nByte ���� ���� ���ϰ��� ������ ����.
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
		// byte�� 1~8 �����̸� ok
	}
	else
	{
		// ������
		assert(FALSE);
		return 0;
	}

	return (n8Byte & (0xffffffffffffffff >> (8 * (8 - nByte))));

	/*
	Big-Endian�� ���, �Ʒ��� �� ������ �𸣰ڴ�. �׽�Ʈ�� �ʿ��ϴ�.
	// 8byte�� �޾�, �ռ� n byte ��(1~8)�� �����Ѵ�.
	//     0x1234567890abcdef�� ���� ��� nByte ���� ���� ���ϰ��� ������ ����.
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

// Byte stream�� �޾� UBYTE8�� �����Ѵ�. Little Endian���� ��ȯ�Ѵ�.
inline CNTFSHelper::UBYTE8 CNTFSHelper::GetUBYTE8(IN UBYTE1* pBuf)
{
	if (NULL == pBuf)
	{
		return 0;
	}

	return *(UBYTE8*)pBuf;

	// Big-Endian�� ��� ���� ������ �ʿ����� �𸣰ڴ�.
	// �׽�Ʈ�� �ʿ��ϴ�.
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
		// runlist�� ����.
		pRtnValue = NULL;
		goto FINAL;
	}

	pRtnValue = (UBYTE1*)pstAttrib + pstAttrib->stAttr.stNonResident.wOffsetDataRun;

FINAL:
	return pRtnValue;
}

// http://ftp.kolibrios.org/users/Asper/docs/NTFS/ntfsdoc.html#id4795876�� �����Ѵ�.
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

	// �켱 RunList�� ������ ������.
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

		// ���������� �̵� !!!
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

	// ���� �Ҵ�����.
	pRunListIndex = pRunList;
	for (i=0; i<nCount; i++)
	{
		cHeader = pRunListIndex[0];

		cHeader_Legnth = (cHeader & 0x0F);
		cHeader_Offset = (cHeader & 0xF0) >> 4;

		if (0 == cHeader_Offset)
		{
			// Sparse ���� ����
			pstRtnValue[i].cType	= TYPE_RUNTYPE_SPARSE;
			pstRtnValue[i].llLength = GetBytes(GetUBYTE8(pRunListIndex+1), cHeader_Legnth);
			pstRtnValue[i].llLCN	= 0;
		}
		else
		{
			// �Ϲ� ���� ����
			pstRtnValue[i].cType	= TYPE_RUNTYPE_NORMAL;
			pstRtnValue[i].llLength = GetBytes(GetUBYTE8(pRunListIndex+1), cHeader_Legnth);
			pstRtnValue[i].llLCN	= GetBytes(GetUBYTE8(pRunListIndex+1+cHeader_Legnth), cHeader_Offset);
		}

		if (i > 0)
		{
			pstRtnValue[i].llLCN = pstRtnValue[i-1].llLCN + GetOffsetSBYTE8(pstRtnValue[i].llLCN, cHeader_Offset);
		}

		// ���������� �̵� !!!
		pRunListIndex += (cHeader_Legnth + cHeader_Offset + 1);
	}

FINAL:
	if ((NULL != pnCount) && (NULL != pstRtnValue))
	{
		*pnCount = nCount;
	}
	return pstRtnValue;
}

// 8byte�� ����,
// 0x1234567812345678
// ���ڸ� �ǹ��Ѵ�.
// 0x0000000000ff2222
// �� 8byte ����(__int64/int64_t)���� +�� �ǹ��ϴµ�,
// �� �Լ��� 0x80~0xff�� �����ϸ�, -�� �ν��ϵ��� �Ѵ�.
// ��, 0xff2222�� -�� �ν��Ͽ� �ش� �������� 8byte������ �����Ѵ�.
//
// �ڼ��Ѱ���,
// http://homepage.cs.uri.edu/~thenry/csc487/video/66_NTFS_Data_Runs.pdf
// http://homepage.cs.uri.edu/~thenry/csc487/video/66_NTFS_Data_Runs.mov @ 08:45
// �� ���� �ٶ�
//
// �ٽ� �������ڸ�,
// 0x7999 ==> 0x7999 �� �����ϰ�,
// 0x8001 ==> 0xffffffffffff8001 �� �����ϵ��� �Ѵ�. (ù ����Ʈ�� 0x80 ~ 0xff�� �����ϱ� ������)
//           (���� �޸𸮴� 0x8001ffffffffffff �̴�)
// [TBD] Other cpu platform (big endian) ó�� �̽�
CNTFSHelper::SBYTE8 CNTFSHelper::GetOffsetSBYTE8(IN UBYTE8 n8Byte, IN INT nOffset)
{
	UBYTE1* pByte	  = NULL;
	UBYTE8  nRtnValue = 0;

	// �־��� ������ �����Ѵ�.
	nRtnValue = n8Byte;

	// ù ����Ʈ�� �д´�.
	pByte = (UBYTE1*)&n8Byte;

	if ((1 <= nOffset) && (nOffset <= 8))
	{
		// good
	}
	else
	{
		// ���� ��Ȳ
		assert(FALSE);
		return nRtnValue;
	}

	if (pByte[nOffset-1] & 0x80)
	{
		// ���� 0x80 bit�� ���ԵǾ��ٸ�,...
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
		// File Name Type�̿��� �ϰ�, 
		// non-resident �̿��� �Ѵ�.
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

	// Signature üũ
	if (0 != strncmp("FILE", (char*)pstMftHeader->szFileSignature, 4))
	{
		assert(FALSE);
		goto FINAL;
	}

	// Alloc size üũ
	if (pstMftHeader->dwAllocatedSize != pstNtfs->stNtfsInfo.dwMftSize)
	{
		assert(FALSE);
		goto FINAL;
	}

	// http://inform.pucp.edu.pe/~inf232/Ntfs/ntfs_doc_v0.5/concepts/file_record.html
	// �� ������,
	// FILE Record��,
	//		Header
	//		Attribute
	//		Attribute
	//		...
	//		End Marker (0xFFFFFFFF)
	// �� �Ǿ� ����. 0xFFFFFFFF�� üũ�Ѵ�.

	pstAttrib = (LPST_ATTRIB_RECORD)((UBYTE1*)pstMftHeader + pstMftHeader->wAttribOffset);
	for (;;)
	{
		if (0xFFFFFFFF == pstAttrib->dwType)
		{
			// End Marker�� �Ǿ� ����
			bFound = TRUE;
			break;
		}

		if (0 == pstAttrib->dwLength)
		{
			// End Marker���� �����
			assert(FALSE);
			break;
		}

		// ���� ���� ��ġ�� buffer pointer��
		// MFT Header�� ���ǵ� used size���� �Ѿ ��,...
		if ((UBYTE1*)pstAttrib >= (UBYTE1*)pReadBuf + pstNtfs->stNtfsInfo.dwReadBufSizeForMft)
		{
			// End Marker���� ������ ���
			assert(FALSE);
			break;
		}

		// ���� Attribute�� ����.
		pstAttrib = (LPST_ATTRIB_RECORD)((UBYTE1*)pstAttrib + pstAttrib->dwLength);
	}

	if (TRUE == bFound)
	{
		// End Marker�� �־� Valid ��.
		bRtnValue = TRUE;
	}
	else
	{
		// Valid���� ����
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
	// �� ������, MFT record ������ ������ ���, $ATTRIBUTE_LIST�� Ȱ��� �� �ִ�.
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
		// ���� ���� 0
		if (0xFFFFFFFF == pstRtnValue->dwType)
		{
			// End Marker�� �Ǿ� ����
			break;
		}

		// ���� ���� 1
		// MFT Header�� Length�� 0�̸� ����
		if (0 == pstRtnValue->dwLength)
		{
			break;
		}

		// ���� ���� 2
		// ���� ���� ��ġ�� buffer pointer��
		// MFT Header�� ���ǵ� used size���� �Ѿ ��,...
		if ((UBYTE1*)pstRtnValue >= (UBYTE1*)pReadBuf + pstNtfs->stNtfsInfo.dwReadBufSizeForMft)
		{
			break;
		}

		if (nType == pstRtnValue->dwType)
		{
			// ã�Ҵ�
			goto FINAL;
		}

		if (TYPE_ATTRIB_ATTRIBUTE_LIST == pstRtnValue->dwType)
		{
			// $ATTRIBUTE_LIST�� �����Ǿ���.
			if (NULL != pstAttribList)
			{
				// ���� �ϳ��� Inode(Record)�� �ΰ��� $ATTRIBUTE_LIST�� ���� �����Ѱ�?
				pstRtnValue = NULL;
				assert(FALSE);
				goto FINAL;
			}

			pstAttribList = pstRtnValue;
		}

		// ���� Attribute�� ����.
		pstRtnValue = (LPST_ATTRIB_RECORD)((UBYTE1*)pstRtnValue + pstRtnValue->dwLength);
	}

	// ã�� ���ߴ�.
	if (NULL == pstAttribList)
	{
		// ã�� ���ߴµ����ٰ�, $ATTRIBUTE_LIST�� ������.
		// �׷� ���� ���̴�.
		pstRtnValue = NULL;
		goto FINAL;
	}

	// $ATTRIBUTE_LIST���� ã�ƺ���.
	//		http://www.alex-ionescu.com/NTFS.pdf 4.4 $ATTRIBUTE_LIST
	//		http://inform.pucp.edu.pe/~inf232/Ntfs/ntfs_doc_v0.5/attributes/attribute_list.html
	// �� �����Ѵ�.

	// �ϴ� ã�� ���Ѱɷ� �⺻�� �����Ѵ�.
	pstRtnValue = NULL;

	// ã�� ��������, $ATTRIBUTE_LIST�� �־�, �ش� �κп��� ã�ƺ���.
	// $ATTRIBUTE_LIST�� ������ ���� �Ӽ��̴�.
	if (1 == pstAttribList->cNonResident)
	{
		// non-resident ==> RunList�� ���Ѵ�.
		pstRunList = GetRunListAlloc(GetRunListByte(pstAttribList), &nCountRunList);
		if ((NULL == pstRunList) || (0 == nCountRunList))
		{
			assert(FALSE);
			goto FINAL;
		}

		if (pstAttribList->stAttr.stNonResident.llAllocSize > MAXINT32)
		{
			// INT �� range�� ����� ���� ���� ���̴�.
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
		// ����� inode�� non-resident ������ $ATTRIBUTE_LIST�� ���ԵǾ���.
		// �ϴ�, Runlist�� ������ 1�̿��µ�,
		// 2�̻��� ���� ���� Ȯ������ ���ߴ�.
		// �켱, ��������, 2�̻��� ��쿡,
		// RunList�� �ش�Ǵ� data�� �����ؼ� �о,
		// ó���ϵ��� ����.

		// RunList�� �Ѳ����� �д´�.
		// $ATTRIBUTE_LIST�� �� ũ�Ⱑ ũ�� ���� ���̴�.
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
				// stNonResident ������ Alloc ũ�Ⱑ,
				// RunList ũ�⸦ ����� ����
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

		// RunList�� ���� Data ���� ��ġ�� ATTRIBUTE_LIST ���� ����Ǿ� �ִ�.
		pstAttribListNode = (LPST_ATTRIBUTE_LIST)pBufAttrList;
		nAttribListSize   = (UBYTE4)pstAttribList->stAttr.stNonResident.llDataSize;
	}
	else
	{
		// resident ��ġ�� ATTRIBUTE_LIST ���� ����Ǿ� �ִ�.
		pstAttribListNode = (LPST_ATTRIBUTE_LIST)((UBYTE1*)pstAttribList + pstAttribList->stAttr.stResident.wValueOffset);
		nAttribListSize   = pstAttribList->stAttr.stResident.dwValueLength;
	}

	// $ATTRIBUTE_LIST ���θ� �����Ѵ�.
	for (;;)
	{
		if (nOffset >= nAttribListSize)
		{
			break;
		}

		if (pstMftHeader->dwMftRecordNumber == (pstAttribListNode->nReference & 0x0000FFFFFFFFFFFFUL))
		{
			// �ش� inode�� �̹� ���ԵǾ� �ִ°�.
			// ���� ���,
			// inode 5��, INODE_DOT��,
			//		TYPE_ATTRIB_STANDARD_INFORMATION
			//		TYPE_ATTRIB_ATTRIBUTE_LIST
			//		TYPE_ATTRIB_FILE_NAME
			// �� �ִٰ� �Ҷ�,
			// TYPE_ATTRIB_ATTRIBUTE_LIST ���ο�,
			//		TYPE_ATTRIB_STANDARD_INFORMATION	<--
			//		TYPE_ATTRIB_FILE_NAME				<--
			//		TYPE_ATTRIB_INDEX_ROOT				<==
			//		TYPE_ATTRIB_INDEX_ALLOCATION		<==
			// �� �ִٰ� ����.
			// <-- ǥ���� ����, �ش� INODE�� ���ԵǾ� �ֱ� ������,
			// �տ��� ó������ ���̴�.
			// �׷��� Skip�� �� �ִ�.
			// ����, ���� loop������,
			// <== �κ��� ó���ϵ��� �Ѵ�.

			// skip ó���� ����,
			// do nothing...
		}
		else
		{
			// �������� ó�� ��ƾ
			if (pstAttribListNode->nType == nType)
			{
				// found!!!
				bFound = TRUE;
				break;
			}
		}

		// List�� ���� Node�� �̵��Ѵ�.
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

	// Fix���� ���Ѵ�.
	pFix = pBuf + pstMftHeader->wFixupArrayOffset;

	// Buf ũ�⸦ Sector ũ�⸸ŭ loop ����.
	// �׷��� Fixup�� ���� Valid üũ�� �Ѵ�.
	for (i=1; i<=pstNtfs->stNtfsInfo.dwReadBufSizeForMft / pstNtfs->stNtfsInfo.wSectorSize; i++)
	{
		// ���� �� ������ 2����Ʈ�� fix ����̴�.
		pBufFix = &pBuf[i*pstNtfs->stNtfsInfo.wSectorSize-2];

		if ((pFix[0] == pBufFix[0]) &&
			(pFix[1] == pBufFix[1]))
		{
			// Fix���� ���ƾ� �Ѵ�.
			// http://inform.pucp.edu.pe/~inf232/Ntfs/ntfs_doc_v0.5/concepts/fixup.html
			// �� Here the Update Sequence Number is 0xABCD and the Update Sequence Array is still empty.
			// �� ����ٶ�.
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

	// Valid üũ�� ��������,
	// ���� Fix�Ͽ� ���۸� ��������.
	pArray = pBuf + pstMftHeader->wFixupArrayOffset + 2;
	for (i=1; i<=pstNtfs->stNtfsInfo.dwReadBufSizeForMft / pstNtfs->stNtfsInfo.wSectorSize; i++)
	{
		// ���� �� ������ 2����Ʈ�� fix ����̴�.
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

	// MFT Header�� ���� ���۸� ���Ѵ�.
	stFindAttrib.pBuf = (UBYTE1*)malloc(pstNtfs->stNtfsInfo.dwReadBufSizeForMft);
	if (NULL == stFindAttrib.pBuf)
	{
		assert(FALSE);
		goto FINAL;
	}
	memset(stFindAttrib.pBuf, 0, pstNtfs->stNtfsInfo.dwReadBufSizeForMft);

	for (;;)
	{
		// Normal File�ΰ�?
		// Metadata File�ΰ�?
		bNormalFile = IsNormalFile(nTypeInode);

		if (TRUE == bNormalFile)
		{
			// �Ϲ� ������ ���,...
			// ��, $MFT, $MFTMirr, $LogFile, ... �� �ƴ�,
			// C:\config.sys ���� �Ϲ� ������ ���,...

			// �ش� Inode ������ �ִ�, LCN ���� offset��,
			// $MFT�� DATA ������ ���� �����´�.

			// ����,
			//		sector : 512 bytes
			//		cluster : 4096 bytes
			//		inode : 1024 bytes (inode = MFT Record)
			// �� ���� �����ϰ�,
			// ...[ssssssss][ssssssss]...�� ���� 1 cluster�� 8���� sector�� ��������.
			// ...[R R R R ][R R R R ]...�� ���� 1 cluster�� 4���� �����Ǿ� �ִ�.
			//                 ^
			//				   |
			// ���� �� ȭ��ǥ ��ġ�� ���� inode�� ��ġ���,
			// Lcn�� ���� [....] �� Cluster ���� sector�� �̵��� �� �ִ�.
			// �׸���, offset ���� ���� 3��° sector�� �߰������� �̵��� �� �ִ�.
			if (FALSE == GetLcnFromMftVcn(pstNtfs, 
										  nTypeInode, 
										  &nLcnNormalFile, 
										  &nOffsetByteNormalFile))
			{
				assert(FALSE);
				goto FINAL;
			}
		}

		// �ش� inode�� �ִ� ��ġ�� Sector�� ���Ѵ�.
		if (FALSE == bNormalFile)
		{
			// Metadata File�� ���,...
			llMftSectorPos = pstNtfs->stNtfsInfo.llMftSectorPos + (INT)nTypeInode * pstNtfs->stNtfsInfo.dwMftSectorCount;
		}
		else
		{
			// Normal File�� ���,..
			// Lcn ���� offset����,
			// ��Ȯ�� inode�� ��ġ(sector����)�� ���Ѵ�.
			llMftSectorPos = nLcnNormalFile * pstNtfs->stNtfsInfo.dwClusterSize / pstNtfs->stNtfsInfo.wSectorSize;
			llMftSectorPos += nOffsetByteNormalFile / pstNtfs->stNtfsInfo.wSectorSize;
		}

		// �ش� Sector�� ����.
		if (FALSE == SeekSector(pstNtfs, llMftSectorPos))
		{
			pstRtnValue = NULL;
			assert(FALSE);
			goto FINAL;
		}

		// inode record�� �д´�.
		if (pstNtfs->stNtfsInfo.dwReadBufSizeForMft != ReadIo(pstNtfs->pstIo, stFindAttrib.pBuf, pstNtfs->stNtfsInfo.dwReadBufSizeForMft))
		{
			assert(FALSE);
			goto FINAL;
		}

		// Read Buffer�� �� MFT Header�� ������ ���Ѵ�.
		if (0 != (pstNtfs->stNtfsInfo.dwReadBufSizeForMft % pstNtfs->stNtfsInfo.dwMftSize))
		{
			// ���� ������ ũ�Ⱑ MFT ũ�� ������ �����谡 �ƴϴ�...
			assert(FALSE);
			goto FINAL;
		}
		nMftCountPerReadBuffer = pstNtfs->stNtfsInfo.dwReadBufSizeForMft / pstNtfs->stNtfsInfo.dwMftSize;

		// Inode�� �ش�Ǵ� MFT Header�� ��ġ�� ���Ѵ�.
		nMftHeaderPos = pstNtfs->stNtfsInfo.dwMftSize * ((INT)nTypeInode % nMftCountPerReadBuffer);
		if (nMftHeaderPos >= pstNtfs->stNtfsInfo.dwReadBufSizeForMft)
		{
			// ������ ũ�⸦ ���
			assert(FALSE);
			goto FINAL;
		}

		// MFT Header�� ���Ѵ�.
		stFindAttrib.pstMftHeader = (LPST_MFT_HEADER)(stFindAttrib.pBuf + nMftHeaderPos);
		if (NULL == stFindAttrib.pstMftHeader)
		{
			assert(FALSE);
			goto FINAL;
		}

		// Fixup ó���Ѵ�.
		if (FALSE == RunFixup(pstNtfs, stFindAttrib.pstMftHeader, stFindAttrib.pBuf))
		{
			assert(FALSE);
			goto FINAL;
		}

		// �ùٸ���?
		if (FALSE == IsValidMftHeader(pstNtfs, stFindAttrib.pBuf, stFindAttrib.pstMftHeader))
		{
			assert(FALSE);
			goto FINAL;
		}

		// File �̸��� ��������. (��� MFT Record�� File name�� ������.)
		if (FALSE == bAttribList)
		{
			// $ATTRIBUTE_LIST ���θ� �� ������,
			// ���� �̸� ������ ���� �ʴ´�.
			// $ATTRIBUTE_LIST ���ο� $FILE_NAME�� ���� �� �ֱ� �����̴�.
			pstAttribFileName = GetAttrib(pstNtfs, stFindAttrib.pstMftHeader, stFindAttrib.pBuf, TYPE_ATTRIB_FILE_NAME, &bAttribList, &nInodeAttribList);
			if (NULL == pstAttribFileName)
			{
				assert(FALSE);
				goto FINAL;
			}
		}

		// File �̸��� ����� �Ǿ��°�? ($MFT, $MFTMirr, $LogFile, ...)
		if ((nTypeInode <= TYEP_INODE_DEFAULT) && (FALSE == bAttribList))
		{
			// �⺻ inode�� ���, file �̸� ��ȸ�� �Ѵ�.
			if (0 != _tcscmp(GetDefaultFileName(nTypeInode), GetFileName(GetAttrib_FileName(pstAttribFileName)).szFileName))
			{
				assert(FALSE);
				goto FINAL;
			}
		}

		// MFT Header���� caller�� �䱸�� Attribute�� �����´�.
		stFindAttrib.pstAttrib = GetAttrib(pstNtfs, stFindAttrib.pstMftHeader, stFindAttrib.pBuf, nTypeAttrib, &bAttribList, &nInodeAttribList);
		if ((NULL != stFindAttrib.pstAttrib) && (FALSE == bAttribList))
		{
			// ����
			// ��ټ� �Ϲ����� ��Ȳ
			break;
		}

		if ((NULL == stFindAttrib.pstAttrib) && (FALSE == bAttribList))
		{
			// ���� ��쵵 �ֱ� ������, assert ���� �ʴ´�.
			// assert(FALSE);
			goto FINAL;
		}

		if ((NULL != stFindAttrib.pstAttrib) && (TRUE == bAttribList))
		{
			// $ATTRIBUTE_LIST�� ���Ե� ���,
			// ���ϰ��� NULL ���´�.
			assert(FALSE);
			goto FINAL;
		}

		// ((NULL == stFindAttrib.pstAttrib) && (TRUE == bAttribList))
		// ��Ȳ�̴�.
		// �� ����,
		// $ATTRIBUTE_LIST�� �ش� Attribute�� ���ԵǾ� �ִٴ� ���̴�.
		// ���õ� Inode�� ���� �ٽ� �о�� �ȴ�.
		nTypeInode = nInodeAttribList;

		// �ش� Inode���� �ش� Attribute�� �е��� �Ѵ�.
		// �ٽ� Loop�� �����ϵ��� �Ѵ�.
		continue;
	}

	// ������� �Դٸ� �����̴�!
	pstRtnValue = (LPST_FIND_ATTRIB)malloc(sizeof(ST_FIND_ATTRIB));
	if (NULL == pstRtnValue)
	{
		assert(FALSE);
		goto FINAL;
	}
	memset(pstRtnValue, 0, sizeof(ST_FIND_ATTRIB));

	// ���������� ����
	*pstRtnValue = stFindAttrib;

FINAL:

	if (NULL == pstRtnValue)
	{
		// �� �Լ� ����
		if (NULL != stFindAttrib.pBuf)
		{
			// ���� buffer�� �Ҵ�Ǿ��ٸ�,
			// �����Ѵ�.
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
// �� �����Ѵ�.
LPCWSTR CNTFSHelper::GetDefaultFileName(IN TYPE_INODE nInode)
{
	// Windows�� �⺻�� UTF16�̴�.
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
// å�� 275 ������ �� �κ��� �����Ѵ�.
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
		// Lcn�� ũ�Ⱑ Bitmap ũ�⺸�� ŭ
		bRtnValue = FALSE;
		assert(FALSE);
		goto FINAL;
	}

	nByte	= nLcn / 8;
	nOffset	= (INT)(nLcn - 8 * nByte);

	if ((0 <= nOffset) && (nOffset <= 8))
	{
		// offset�� 0~8���̿� �־�� �Ѵ�.
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

	// $BITMAP�� $DATA�� ã�´�.
	pstFindAttrib = FindAttribAlloc(pstNtfs, TYPE_INODE_BITMAP, TYPE_ATTRIB_DATA);
	if (NULL == pstFindAttrib)
	{
		pstRtnValue = NULL;
		assert(FALSE);
		goto FINAL;
	}

	// RunList�� ���Ѵ�.
	pstRunlist = GetRunListAlloc(GetRunListByte(pstFindAttrib->pstAttrib), &nRunListCount);
	if (NULL == pstRunlist)
	{
		pstRtnValue = NULL;
		assert(FALSE);
		goto FINAL;
	}

	if (nRunListCount <= 0)
	{
		// runlist �׸��� �־�� �Ѵ�.
		pstRtnValue = NULL;
		assert(FALSE);
		goto FINAL;
	}

	// ���� bitmap�� fragment�Ǿ� ���� �ʰ�,
	// bitmap�� ����� cluster������ �� ���� �ʴ�.
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
			// buffer overflow üũ
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

	// bitmap ũ�� (byte)
	stBitmapInfo.nBitmapBufSize = pstNtfs->stNtfsInfo.dwClusterSize * nCountBitmapCluster;

	// ������� �Դٸ� ����
	pstRtnValue = (LPST_BITMAP_INFO)malloc(sizeof(ST_BITMAP_INFO));
	if (NULL == pstRtnValue)
	{
		assert(FALSE);
		goto FINAL;
	}
	memset(pstRtnValue, 0, sizeof(ST_BITMAP_INFO));

	// ����� ������
	*pstRtnValue = stBitmapInfo;

	// ���� Alloc���� ������ bitmap
	pstRtnValue->bAlloc = TRUE;

FINAL:
	if (NULL == pstRtnValue)
	{
		// �� �Լ� ����
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
		// alloc���� ������ bitmap�� �ƴ� ��쿡��,...
		// �ܺ� ���ϵ ���� load�� �����...
		// �̶��� �⺻������ free�ϸ� ���� �ʴ�.
		// ��ü���� free�� �ϵ��� �Ѵ�.
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

// ������� Total count�� �����ϰ�,
// ������� Cluster number�� �ִ밪�� optional�� �����Ѵ�. (NULL ����)
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
// �� ������, Ntfs�� File�� ũ�� Metadata�� Normal, �ΰ��� Category��
// ��������.
// �Ϲ����� Normal file�� �ش�Ǵ� inode���� �����Ѵ�.
BOOL CNTFSHelper::IsNormalFile(IN TYPE_INODE nInode)
{
	if (nInode >= TYPE_INODE_FILE)
	{
		return TRUE;
	}

	return FALSE;
}

// [TBD] Disk ũ�⸦ ���ϴ� ���� ���� �ʴ�.
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
	// ��
	// 28h Total Sectors
	// �� ������,
	// Bios parameter block�� �ִ� sector ������ +1 �����
	// ��ü ������ �ȴٰ� ��������
	return (_OFF64_T)((pstNtfs->stBpb.llSectorNum+1) * pstNtfs->stNtfsInfo.wSectorSize);
	*/

	// ���� ��꿡��, Cluster ũ�� �Ѱ� ��ŭ ���� �Ǵ���.
	nRtnValue = (_OFF64_T)((pstNtfs->stBpb.llSectorNum+1) * pstNtfs->stNtfsInfo.wSectorSize) - pstNtfs->stNtfsInfo.dwClusterSize;

	// [TBD] �Ʒ��κ��� ��Ȯ������ ��
	{
		nMod = (nRtnValue % pstNtfs->stNtfsInfo.dwClusterSize);
		if (0 != nMod)
		{
			// Disk ũ�Ⱑ cluster ������ �¾� �������� ���� ��,...
			// ������ �������� ����� �ش�.
			nRtnValue -= nMod;

			// �׸���, �̶��� cluster ũ�� �Ѱ��� �����Ǿ���.
			nRtnValue += pstNtfs->stNtfsInfo.dwClusterSize;
		}
	}

	return nRtnValue;
}

// INDEX_ENTRY�� stream ���� FILE_NAME_ATTRIBUTE�� �޾Ƶ��δ�.
// �̰��� ���� �մ������� Ȯ���� �ʿ��ϴ�.
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

// INDEX_ENTRY�� File Name ������ ���Ѵ�.
// File Name ������ ���� ��� FALSE�� �����Ѵ�. (Ȥ�� ���� �߻���)
// [TBD] multi-platform (ex, linux) �غ��Ѵ�. (StringCch~ �Լ���)
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
		// �ش� ���� 0�̶��, Data�� ���ٴ� ���̴�.
		goto FINAL;
	}

	pstAttribFileName = GetFileNameAttrib(pstEntry);
	if (NULL == pstAttribFileName)
	{
		assert(FALSE);
		goto FINAL;
	}

	StringCchCopyW(lpszName, dwCchLength, GetFileName(pstAttribFileName).szFileName);

	// ������� �Դٸ� ����
	bRtnValue = TRUE;

FINAL:
	return bRtnValue;
}

// Update Sequence ó���� �Ѵ�.
// Cluster�� ���� 8���� sector�� ���µ�,
// sector�� ���� �� ����Ǿ� �ִ��� ������ �ϰ�,
// ������ ���� sector �Ϻ��� ���� ������ ������ �����Ѵ�.
// nIndexSize�� ST_INDEX_ROOT::nSize�� �ǹ��Ѵ�.
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

	// �� ������ �����´�.
	pUpdateSequenceAddr	= pBufCluster + pstHeader->nOffsetUpdateSequence;
	nUpdateSequence		= *(UBYTE2*)pUpdateSequenceAddr;
	pArrUpdateSequence	= (UBYTE2*)(&pUpdateSequenceAddr[2]);

	// pstHeader->nSizeUpdateSequence
	// ���� +1�ε� �ϴ�.
	// �Ʒ� ������ ����Ѵ�.
	nSequenceCount = nIndexSize / nSectorSize;

	// Sector�� ������ 2����Ʈ�� ���
	for (i=nSectorSize-2; i<nClusterSize; i+=nSectorSize)
	{
		if (nSector >= nSequenceCount)
		{
			// ó�� ����
			break;
		}

		if (nUpdateSequence != *(UBYTE2*)&pBufCluster[i])
		{
			// ���� ����
			assert(FALSE);
			goto FINAL;
		}

		// Sequence Array�� ���� ���� ���� ó���Ѵ�.
		*(UBYTE2*)&pBufCluster[i] = pArrUpdateSequence[nSector];

		nSector++;
	}

	// ������� �Դٸ� ����
	bRtnValue = TRUE;

FINAL:
	return bRtnValue;
}

// http://inform.pucp.edu.pe/~inf232/Ntfs/ntfs_doc_v0.5/concepts/file_reference.html
// �� ����, File Reference�� ������ Inode ��(FILE record number)�� ���Ѵ�.
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

// �Ϲ����� File, Directory�� Traverse Index ����($INDEX_ALLOCATION)
// ����, Sub-Directory�� ������,
// File Reference��� ������ ����Ǿ� �ִµ�, �ش� ����,
// $MFT�� Vcn ������ �Ǿ� �ִ�.
// �׷��� �� �Լ��� �̿��ϸ�, Sub-directory�� Traverse�� �� �ֵ��� ���ش�.
//
// TYPE_INODE_FILE ������ inode�� ���� ��ġ��,
// $MFT�� �̿��Ͽ� ����Ѵ�.
// �� �Լ��� ������ Lcn�� *pnOffsetBytes ������,
// �ش� inode�� ��ġ�� Seek�� �� �ִ�.
// ��, LCN * Cluster ũ�� + *pnOffsetBytes��
// Seek�Ѵ�.
//
// ���� ���,
// 
// # ���ڰ� $MFT�� Data��� �� �� (Cluster ����),
// �Ʒ��� ���� $MFT�� 3���� Fragmented�Ǿ� �ִٰ� ��������.
// 
// ##########.....########....###
// 0123456789.....01234567....890 ==> MFT�� VCN
// 012345678901234567890123456789 ==> Disk�� LCN
//                  ^
//                  |
//
// ����, ��û�� inode�� ���������� 12��° Cluster�� ���ԵǾ� �ִٸ�,
// 12��° Cluster�� #�� ������ ���� 8��(8 x 512 = 4096)�� sector(@)�� �����Ǿ� �ִ�.
//
// ...@@@@@@@@..
//          ^
//          |
//
// ���� 7��° sector��� �Ѵٸ�,
// ������ ���� ���޵ȴ�.
//
// Lcn    : 17
// Offset : 7 * 512
//
// �׷���, �ش� ����, Seek�Ϸ���, 17 * 4096(cluster ũ��) + 7 * 512(sector ũ��)
// ������ seek�ϸ�, inode�� ������ �� �ִ�.
//
// �ٽ� �������ڸ�,
// ����,
//		sector : 512 bytes
//		cluster : 4096 bytes
//		inode : 1024 bytes (inode = MFT Record)
// �� ���� �����ϰ�,
// ...[ssssssss][ssssssss]...�� ���� 1 cluster�� 8���� sector�� ��������.
// ...[R R R R ][R R R R ]...�� ���� 1 cluster�� 4���� �����Ǿ� �ִ�.
//                 ^
//				   |
// ���� �� ȭ��ǥ ��ġ�� ���� inode�� ��ġ���,
// Lcn�� ���� [....] �� Cluster ���� sector�� �̵��� �� �ִ�.
// �׸���, offset ���� ���� 3��° sector�� �߰������� �̵��� �� �ִ�.
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

	// Attribute�� ���Ѵ�.
	pstAttribAlloc = FindAttribAlloc(pstNtfs, TYPE_INODE_MFT, TYPE_ATTRIB_DATA);
	if (NULL == pstAttribAlloc)
	{
		assert(FALSE);
		goto FINAL;
	}

	// Runlist�� ���Ѵ�.
	pstRunlistAlloc = GetRunListAlloc(GetRunListByte(pstAttribAlloc->pstAttrib), &nCountRunlist);
	if ((NULL == pstRunlistAlloc) || (nCountRunlist <= 0))
	{
		assert(FALSE);
		goto FINAL;
	}

	// �켱, Attrib record�� ���� vcn ������,
	// runlist�� vcn������ ������ �����Ѵ�.
	for (i=0; i<nCountRunlist; i++)
	{
		nTotal += pstRunlistAlloc[i].llLength;
	}
	
	if (nTotal != (pstAttribAlloc->pstAttrib->stAttr.stNonResident.llEndVCN - pstAttribAlloc->pstAttrib->stAttr.stNonResident.llStartVCN + 1))
	{
		// ���� �ʴ�.
		assert(FALSE);
		goto FINAL;
	}

	if (1 == pstAttribAlloc->pstAttrib->cNonResident)
	{
		// good
		// �̰��� non-resident only �̴�.
	}
	else
	{
		assert(FALSE);
		goto FINAL;
	}

	// Position�� ���Ѵ�.
	nPos = (UBYTE8)nInode * pstNtfs->stNtfsInfo.dwMftSectorCount * pstNtfs->stNtfsInfo.wSectorSize;
	nVcn =  nPos / pstNtfs->stNtfsInfo.dwClusterSize;
	nOffsetBytes = (INT)(nPos % pstNtfs->stNtfsInfo.dwClusterSize);

	// [TBD]
	// StartVCN�� $MFT���� 0�̴���,
	// ���� StartVCN�� 0�� �Ѿ�� ���� ��ұ�?
	if (nVcn > pstAttribAlloc->pstAttrib->stAttr.stNonResident.llEndVCN + pstAttribAlloc->pstAttrib->stAttr.stNonResident.llStartVCN)
	{
		// �� ũ�⸦ ���
		assert(FALSE);
		goto FINAL;
	}

	// ���� Vcn�� �ش�Ǵ� Lcn�� ������.
	if (FALSE == GetLcnFromVcn(pstRunlistAlloc, 
							   nCountRunlist, 
							   nVcn, 
							   pstAttribAlloc->pstAttrib->stAttr.stNonResident.llStartVCN, 
							   &nLcn))
	{
		// ����
		assert(FALSE);
		goto FINAL;
	}

	// ������� �Դٸ� ����
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

// Inode�� ã�´�.
// nStartInode : ������ Inode. ��, ������ Directory�� inode.
// lpszName : ã�� ���ϸ�
// pbFound : ã�� ��� TRUE�� ������. �ƴϸ� FALSE�� ������
// pstInodeData : NULL ����. �ش� Inode�� ����
//
// ���� : Inode �� (*pbFound�� TRUE�� ��� �ǹ�����)
//
// Remark : b-tree traverse ���� CNTFSHelper::CreateStack(...) �ּ� ����
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

	// [TBD] multi-platform(os) ���� (such as linux)
	StringCchCopyW(lpszNameUpper, MAX_PATH_NTFS, lpszName);
	if (0 == _wcsupr_s(lpszNameUpper, MAX_PATH_NTFS))
	{
		// �빮�ڷ� ��ȯ�Ѵ�.
		// [TBD] $UpCase ������ �˵��Ѵ�.
		// ����
	}
	else
	{
		assert(FALSE);
		goto FINAL;
	}

	if ((NULL != pstNtfs->hInodeCache) && (NULL != pstNtfs->pstInodeCache) && (NULL != pstNtfs->pstInodeCache->pfnGetCache))
	{
		// ���� inode cache�� �ִٸ�,...

		if (NULL == pstInodeData)
		{
			// ����, NULL�� ���,...
			pstInodeDataAlloc = (LPST_INODEDATA)malloc(sizeof(*pstInodeDataAlloc));
			if (NULL == pstInodeDataAlloc)
			{
				assert(FALSE);
				goto FINAL;
			}
			memset(pstInodeDataAlloc, 0, sizeof(*pstInodeDataAlloc));

			// �޸� �Ҵ�Ȱ� �����
			pstInodeData = pstInodeData;
		}

		if (TRUE == (*pstNtfs->pstInodeCache->pfnGetCache)(pstNtfs->hInodeCache, nStartInode, lpszNameUpper, &bFound, pstInodeData, pstNtfs->pstInodeCache->pUserContext))
		{
			// Cache ȣ�� ����
			if (TRUE == bFound)
			{
				// Cache���� ã�� ����...

				// �Ʒ��� �ڵ���� �������� �ʰ�,
				// �ٷ� ������.
				nRtnValue = pstInodeData->nInode;
				*pbFound = TRUE;
				goto FINAL;
			}
			else
			{
				// Cache���� ã�µ� �����ߴ�.
				// do nothing...
				// �Ʒ� �ڵ���� �����Ѵ�.
			}
		}
	}

	// $INDEX_ALLOCATION�� Runlist�� ���Ѵ�.
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

	// $INDEX_ROOT�� ���Ե� ����� ���Ѵ�.
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
				// ���� data(��, ���� �̸�)�� ���ԵǾ� �ִٸ�,...
				nFind = wcscmpUppercase(lpszNameUpper, pstArray->pstArray[i].lpszFileNameAlloc);
				if (0 == nFind)
				{
					// ã�Ҵ� !!!
					*pbFound = TRUE;
					nRtnValue = pstArray->pstArray[i].nInode;
					break;
				}
				else if (nFind < 0)
				{
					// �빮�� �̸� ���Ͽ�, ���� ���� ���޵� ��쿣,...
					// b-tree�� subnode�� ����.
					if (TRUE == pstArray->pstArray[i].bHasSubNode)
					{
						// ���� subnode�� �ִٸ�,...
						// subnode�� ����.
						bGotoSubnode = TRUE;
						break;
					}
				}
			}
			else
			{
				// ���� data�� ���ԵǾ� ���� �ʴٸ�,...
				if (TRUE == pstArray->pstArray[i].bHasSubNode)
				{
					// sudnode�� �ִ� ���,...
					// ��, data�� ���� subnode�� �ִ� ����...
					// subnode�� ����.
					bGotoSubnode = TRUE;
					break;
				}
			}
		}

		if (TRUE == bGotoSubnode)
		{
			// subnode�� ���� �Ѵٸ�,...
			if (NULL == pstRunListAlloc)
			{
				// subnode�� �����ϴµ�,
				// $INDEX_ALLOCATION�� runlist�� ���ٸ�,...
				assert(FALSE);
				goto FINAL;
			}

			pstArraySubnode = GetIndexEntryArrayAllocByVcn(pstNtfs, nIndexBlockSize, pstRunListAlloc, nCountRunList, pstArray->pstArray[i].nVcnSubnode);
			if (NULL == pstArraySubnode)
			{
				assert(FALSE);
				goto FINAL;
			}

			// subnode�� �������� ���� �����Ͽ���.
			// loop�� �ٽ� ��������.
			// �׷��� ���ؼ�, ������ Index entry array�� �����ϰ�,
			// subnode�� ������ ��ü����.
			GetIndexEntryArrayFree(pstArray);
			pstArray		= pstArraySubnode;
			pstArraySubnode	= NULL;
			continue;
		}

		// ������ �����.
		break;
	}

	if (TRUE == *pbFound)
	{
		// ã�� ���, ȣ������ ��û�� ����, �߰� ������ �����Ѵ�.
		if (NULL != pstInodeData)
		{
			if (FALSE == GetInodeData(&pstArray->pstArray[i], pstInodeData, NULL, 0))
			{
				// �߰� ���� ȹ�� ���н�,
				// ã�� ���� ������ �Ѵ�.
				assert(FALSE);
				*pbFound = FALSE;
			}
		}

		// inode cache�� �ִ� ���, cache�� ������ �Ѵ�.
		if ((NULL != pstNtfs->hInodeCache) && (NULL != pstNtfs->pstInodeCache) && (NULL != pstNtfs->pstInodeCache->pfnGetCache))
		{
			if (TRUE == (*pstNtfs->pstInodeCache->pfnSetCache)(pstNtfs->hInodeCache, nStartInode, lpszNameUpper, pstInodeData, pstNtfs->pstInodeCache->pUserContext))
			{
				// inode cache�� ���� ����
			}
			else
			{
				// inode cache�� ���� ����
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
// lpszFindNameUpper�� �빮�ڷ� �Ǿ�� �Ѵ�.
// �Լ� ���޽� �Ķ������ ������ �����Ѵ�.
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
		// ����
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

// Index block���� �������� index entry�� ���ԵǾ� �ִ�.
// �־��� index entry�� ���� �׸��� �����Ѵ�.
//
// ���� index block�� �������� �����Ͽ��ٸ�, ������ ���� �����Ѵ�.
CNTFSHelper::LPST_INDEX_ENTRY CNTFSHelper::GetNextEntry(IN LPST_INDEX_ENTRY pstCurIndexEntry)
{
	LPST_INDEX_ENTRY pstRtnValue = NULL;

	if (NULL == pstCurIndexEntry)
	{
		assert(FALSE);
		goto FINAL;
	}

	// Subnode, Last entry
	// �ɼ� �̿��� ���� bitwise�� ���,...
	if (0 != (~((INDEX_ENTRY_FLAG_SUBNODE | INDEX_ENTRY_FLAG_LAST_ENTRY)) & pstCurIndexEntry->nFlag))
	{
		assert(FALSE);
		goto FINAL;
	}

	if (INDEX_ENTRY_FLAG_LAST_ENTRY == (INDEX_ENTRY_FLAG_LAST_ENTRY & pstCurIndexEntry->nFlag))
	{
		// ���� ���簡 ������ �̶��,...
		// ����� ���ϰ��� �����Ѵ�.
		pstRtnValue = pstCurIndexEntry;
		goto FINAL;
	}

	// [TBD]
	// �Ʒ��� ���� �ش� memory�� boundary�� �������,
	// validation üũ�� ���ָ� ���� �� �ϴ�.

	pstRtnValue = (LPST_INDEX_ENTRY)((UBYTE1*)pstCurIndexEntry + pstCurIndexEntry->nLengthIndexEntry);
	if (NULL == pstRtnValue)
	{
		assert(FALSE);
		goto FINAL;
	}


	// �ɼ� �̿��� ���� bitwise�� ���,...
	if (0 != (~((INDEX_ENTRY_FLAG_SUBNODE | INDEX_ENTRY_FLAG_LAST_ENTRY)) & pstRtnValue->nFlag))
	{
		pstRtnValue = NULL;
		assert(FALSE);
		goto FINAL;
	}

FINAL:
	return pstRtnValue;
}

// Vcn�� Lcn���� ��ȯ�Ѵ�.
//
// ����,
// ������ ���� RunList�� �Ǿ� ���� ��,
// [0] : LCN = 10, Length = 3
// [1] : LCN = 100, Length = 4
// [2] : LCN = 1000, Length = 5
// �̴� ������ ���� �� 12���� Cluster�� �����Ǿ� ������ �ǹ��Ѵ�.
// 10,11,12,100,101,102,103,1000,1001,1002,1003,1004
// ����, 5(nVcn=4)��° Vcn�� ��û�ߴٸ�,
// 101�� �����Ѵ�.
// ���� ������ ����ų� �� ���� �Է¿� ���ؼ��� FALSE�� �����Ѵ�.
// �׸���, ATTRIB_RECORD�� ���� Vcn(llStartVCN)�� �ִµ�,
// ���� �����ϴ� Vcn���� �Ǹ��Ѵ�.(Ȯ�� �ʿ�)
// �׷���, ����,
// llStartVCN = 3
// nVcn = 4 �̶��,
// ���� nVcn�� 2�� �Ǵ� ���̰�,
// �׷� �� �������� 11�� �ȴ�.
// ����, llStartVCN�� 0�̿��ٸ�, 101�� �����Ѵ�.
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
		// ã�� ���ߴ�.
		// ��, �Ƹ��� nVcn�� �� ������ ����� Ȯ���� �ִٴ� ���̴�.
		assert(FALSE);
		goto FINAL;
	}

	// ������� �Դٸ� ����
	bRtnValue = TRUE;

FINAL:
	return bRtnValue;
}

// �Ϲ������� b-tree�� Ž���� ��,
// ����� ����� ����ȴ�.
// �� �Լ���, �ܺ� ȣ���� ���� ��� ȣ���� ����� ���ɼ��� �ִµ�,
// ���� �Լ� ���� ���ȣ���� �ϸ� Stack overflow ������ �ִ�.
// ����, ����Լ� ����� ���� ������,
//
//		FindFirst -> FindNext -> FindNext -> FindNext -> ... -> FindClose
//
// �� ���� ���� ����Ⱑ ���� �ʴ�.
// ����Լ� ����� �����,
//
//		Start --------------... Long block ...---------------> returned
//              |        |            |           |        |
//              v        v            v           v        v
//           callback  callback    callback    callback callback
//
// �� ���� ���� �� ���ɼ��� �ִ�.
// ����ư, FindFirst / FindNext / FindClose�� ���� ��-����� ���� ����� ���ؼ�,
// ���� Stack�� �ʿ��ϴ�.
// �ش� Stack�� �����Ѵ�.
//
// nGrowthCount : STACK_GROWTH_COUNT
//                �Ϲ����� Stack�� Growth ũ��. �ش� define�� �ּ� ����
//
// [Remark]
//
// Stack�� node�� INDEX_ENTRY array�� ����ִ�.
// ��, Stack�� node�� index block�� �ش�ȴٰ� �� �� �ִ�.
// ����, ������ directory�� 001.txt, 002.txt, ..., 100.txt�� �ִٰ� ��������.
//
// ���� ���,
// �׷� �ش� �κ��� b-tree�� ����ȭ�ϸ�, (�ڽ��� index block�̶�� ��������.)
//
//                  ============================================================
//                 || 030.txt  060.txt  090.txt    091.txt 092.txt ... 100.txt ||    // $INDEX_ROOT (030, 060, 090�� sub-node ����)
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
// �� ���� ���̴�. (040.txt, 050.txt, 070.txt, 080.txt�� ���� ����� subnode�� ǥ������ ����)
// ���ٷ� �̷��� Link�� box�� ���� Stack�� ��Ȳ�� ǥ���� ���̴�.
//
// ù��° FindFirstInode���� $INDEX_ROOT�� ���� ������ ���� �������� Traverse�Ͽ�,
//
//			001.txt
//
// �� ���� �Ѵ�.
// �׶� Stack��,
//
// -------------------------------  
// | 001.txt 002.txt ... 009.txt | ���� ��ġ : 1
// ------------------------------- 
// -------------------- 
// | 010.txt  020.txt | ���� ��ġ : 0
// -------------------- 
// ------------------------------------------------------------
// | 030.txt  060.txt  090.txt    091.txt 092.txt ... 100.txt | : ���� ��ġ : 0
// ------------------------------------------------------------
//
// �� ���� �����ȴ�.
// �״��� FindNextInode ȣ��ÿ���,
//
// 		002.txt
//
// �� ���� �ϰ�, Stack��,
//
// -------------------------------  
// | 001.txt 002.txt ... 009.txt | ���� ��ġ : 2
// -------------------------------
// -------------------- 
// | 010.txt  020.txt | ���� ��ġ : 0
// -------------------- 
// ------------------------------------------------------------
// | 030.txt  060.txt  090.txt    091.txt 092.txt ... 100.txt | : ���� ��ġ : 0
// ------------------------------------------------------------
//
// �� ���� �ȴ�.
//
// ���� 009.txt���� �� ���� ������,
// �� ���� Stacknode�� pop�� �ȴ�.
// 
// -------------------- 
// | 010.txt  020.txt | ���� ��ġ : 1
// -------------------- 
// ------------------------------------------------------------
// | 030.txt  060.txt  090.txt    091.txt 092.txt ... 100.txt | : ���� ��ġ : 0
// ------------------------------------------------------------
//
// �� ���� �ǰ� 010.txt�� �����Ѵ�. [remark.01]
// �׸���, �ٽ� FindNextInode�� �ϰԵǸ�,
// 020.txt�� �ִµ�, �ش� entry�� subnode�� �����Ƿ�, subnode�� �̵��ϰ� �ȴ�.
// subnode�� �̵��ϰ� �ȴٴ� ����,
// Stack�� �ش� subnode�� index block�� push�ϴ� ���̴�.
//
// (����) ���⼭ �ǽ��� ����,
//        [remark.01]���� 010.txt�� subnode�� �������� push���� �ʾҴٴ� ���̴�.
//        ����, 010.txt subnode�� push�ϸ�, 001.txt ~ 009.txt�� �ٽ� list�Ǿ�,
//        ��� ��ȯ�ϰ� �ȴ�.
//        ����, �� index entry array�� node���� ���� subnode�� ���� ��,
//        �ش� subnode�� Stack push�ϴµ�, ��, �ѹ��� �ϴ� �������� �����.
//        ��, �ѹ� push�� ������ subnode�� ��쿡��, push���� �ʵ��� �Ѵ�.
//        �׷��� [remark.01]���� 010.txt�� push���� �ʰ� ���޸� �� ���̴�.
//        �ٽ�, �Ʒ� tree�� ����, �� ��(:)���� �̷��� Link�� �ִµ�
//        �ش� Link�� �ѹ� �ٳణ �κ��̱� ������, �ٽ� Ž������ �ʴ´ٴ� ����
//        ����ȭ�� ���̴�.
//      * �ڵ忡��, ST_INDEX_ENTRY_NODE::bSubnodeTraversed�� ����
//
// �׷��� �Ǹ�,
//
//                  ============================================================
//                 || 030.txt  060.txt  090.txt    091.txt 092.txt ... 100.txt ||    // $INDEX_ROOT (30, 60, 90�� sub-node ����)
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
// �� ���� �Ǹ�, Stack��,
//
// -----------------------
// | 011.txt ... 019.txt | ���� ��ġ : 0
// -----------------------
// -------------------- 
// | 010.txt  020.txt | ���� ��ġ : 1
// -------------------- 
// ------------------------------------------------------------
// | 030.txt  060.txt  090.txt    091.txt 092.txt ... 100.txt | : ���� ��ġ : 0
// ------------------------------------------------------------
// 
// �� ���� �Ǹ�, �������� 011.txt�� ������ �� �ְ� �ȴ�.
//
// �̷� ���� 000.txt ~ 100.txt�� ��� Traverse�� �� �ְ� �ȴ�.
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
		// ����
		if (NULL != stStack.pstNodeArray)
		{
			free(stStack.pstNodeArray);
			stStack.pstNodeArray = NULL;
		}
	}
	return pstRtnValue;
}

// Stack�� �����Ѵ�.
VOID CNTFSHelper::DestroyStack(IN LPST_STACK pstStack)
{
	INT i = 0;

	if (NULL != pstStack)
	{
		if (pstStack->nItemCount > 0)
		{
			// Stack�� �׸��� �ִ� ���¿��� Destroy�� ���Դ�.
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

// Stack�� Pop�Ѵ�.
// pstNode�� pointer ���簡 �ƴ϶�, �޸� ���簡 �̷�����.
// ��, ȣ���ڴ� pstNode�� &stNode�� �����ؾ� �Ѵ�.
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
		// stack �����
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

// Stack�� Push�Ѵ�.
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
		// ���Ҵ� �ؾ� �Ѵ�.
		pstNodeArray = (LPST_STACK_NODE)malloc(sizeof(*pstNodeArray)*(pstStack->nAllocSize + pstStack->nGrowthCount));
		if (NULL == pstNodeArray)
		{
			assert(FALSE);
			goto FINAL;
		}
		memset(pstNodeArray, 0, sizeof(*pstNodeArray)*(pstStack->nAllocSize + pstStack->nGrowthCount));

		// �������� �����ѵ� �޷θ� �����Ѵ�.
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

// Stack�� Pop������ �ʰ�, �� �� �׸��� �����Ѵ�.
// pstNode�� pointer ���簡 �ƴ϶�, �޸� ���簡 �̷�����.
// ��, ȣ���ڴ� pstNode�� &stNode�� �����ؾ� �Ѵ�.
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

	// ������ �ִ� �׸��� �����Ѵ�.
	*pstNode = pstStack->pstNodeArray[pstStack->nItemCount-1];

	// ������� �Դٸ� ����
	bRtnValue = TRUE;

FINAL:
	return bRtnValue;
}

// Stack�� ����°�?
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
// ������ LPST_WIN32_HANDLE �޸� ���� (���� ���� �������� ���ÿ�)
// ���н� INVALID_HANDLE_VALUE_NTFS(=INVALID_HANDLE_VALUE�� ����)�� ����
//
// ����) - FindFirstFile�� Wile-card(*,?)���� msdn�� ��õ� ����ó�� �������� �ʴ´�.
//         ��, *.* Ȥ�� *�� �����Ѵ�. �� �̿��� ���� ������ ERROR_NOT_SUPPORTED�� SetLastError�Ѵ�.
//         ����, *.* Ȥ�� *�� ��û�ϴ� ��� ���� �;� �Ѵ�.
//
//		 - Win32 API������ *.* / *�� ����Ͽ� sub-directory�� �˻��� ������,
//		   .
//		   ..
//		   �� ��ΰ� �켱 Ž���Ǵ°��� ������. (Revision 7 ���� ������)
//
//		 - Win32 API���� "C:"�� �����ϸ�, IDE ���� �����ϴµ�,
//		   �� �Լ��� �������� �ʴ´�.
//       
//       - \\?\ prefix�� ��밡���ϴ�. (���� ���������δ� �����Ѵ�)
//
//       - WIN32_FIND_DATA�� ���� ������ �������� �ʴ´�.
//         cAlternateFileName / dwReserved0 / dwReserved1
//
// [TBD] multi-platform (ex, linux) �غ��Ѵ�.
CNTFSHelper::NTFS_HANDLE WINAPI CNTFSHelper::FindFirstFile(IN LPCWSTR lpFileName, OUT LPWIN32_FIND_DATAW lpFindFileData)
{
	// �ܺ� ���� I/O ���� �����Ѵ�.
	return FindFirstFileExt(lpFileName, lpFindFileData, NULL, NULL);
}

// �ܺ� I/O ���ǵ� FindFirstFile �Լ�
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

	// �� �Լ��� ��ü�� caller�� ���� ����� ȣ���� �߻��� �� �ִ�.
	// ���� �ǵ����̸�, Big size�� ���� ���� ����� �����Ѵ�. (Stack overflow ����)
	// �������� �Ҵ�޾� �������.
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

	// Ntfs�� ����.
	pstNtfs = FindFirstFileOpenNtfs(lpFileName, lpszPath, MAX_PATH_NTFS, pstIoFunction, pstInodeCache, &dwErrorCode, &bFindSubDir);
	if (NULL == pstNtfs)
	{
		assert(FALSE);
		goto FINAL;
	}

	// Path�� Inode�� ã�´�.
	// FindInodeFromPath �Լ��� �ð��� ���� �ɸ��� �Լ��̴�.
	/*
	if (TRUE == bFindSubDir)
	{
		// Inode���� ã�´�.
		dwErrorCode = FindInodeFromPath(pstNtfs, lpszPath, &nInode, pstInodeDataAlloc, NULL);
	}
	else
	{
		// ����, *.*�� ������ �ʾ�,
		// ���� ���� ���� ȹ������� ȣ��� ���,...
		// Inode �Ӹ� �ƴ϶�, Ȯ�� Data�� ���Ѵ�.
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

	// ����, FindFirstFileMain �Լ��� ȣ���Ѵ�.
	/*
	if (TRUE == bFindSubDir)
	{
		// Sub-directory�� ã�� ����,
		// Ȯ�� Data�� �ʿ����.
		pHandle = FindFirstFileMain(pstNtfs, pstInodeDataAlloc, NULL, bFindSubDir, NULL, NULL, lpFindFileData, &dwErrorCode);
	}
	else
	{
		// ���� ���� ���� ȹ������� ȣ��Ǵ� ���,...
		// Inode �Ӹ� �ƴ϶�, Ȯ�� Data�� �����ؾ� �Ѵ�.
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

		// ���� �߻�
		::SetLastError(dwErrorCode);

		if (NULL != lpFindFileData)
		{
			memset(lpFindFileData, 0, sizeof(*lpFindFileData));
		}
	}
	return pHandle;
}

// FindFirstFile �Լ������� ���� lpFileName�� �޾�,
// ntfs�� ��� �����Ѵ�.
// ���� lpFileName ��ΰ� lpszPath�� ���޵ȴ�. (\\?\, *.* ���� ���ŵ�)
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

	// _ ���ڴ� ������ a~z ������ ������ ġȯ�� ����
	StringCchCopyA(szVolume, 7, "\\\\.\\_:");

	// *.*�� \\?\���� ���ŵ� ��θ� ���Ѵ�.
	*pdwErrorCode = GetFindFirstFilePath(lpFileName, lpszPath, dwCchPath, &szVolume[4], &bFindSubDir);
	if (ERROR_SUCCESS != *pdwErrorCode)
	{
		assert(FALSE);
		goto FINAL;
	}

	// Ntfs�� Open�Ѵ�.
	if (NULL == pstIoFunction)
	{
		pstRtnValue = OpenNtfs(szVolume);
	}
	else
	{
		// �ܺ� I/O ����
		pstRtnValue = OpenNtfsIo(szVolume, pstIoFunction);
	}

	if (NULL != pstInodeCache)
	{
		if (FALSE == SetInodeCache(pstRtnValue, pstInodeCache->pfnCreateInodeCache, pstInodeCache->pfnGetCache, pstInodeCache->pfnSetCache, pstInodeCache->pfnDestroyCache, pstInodeCache->pUserContext))
		{
			// Cache�� ���е�
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

	// ����
	*pdwErrorCode = ERROR_SUCCESS;

FINAL:
	return pstRtnValue;
}

// FindFirstFile�� �����ϳ�. Inode ������ �޴´�.
// �׷��� ������, �ӵ��� ���ȴ�.
//
// �ش� Inode ������, FindFirstFile�� ���� ����� NTFS_HANDLE�� ������,
// FindNextFileWithInode(...)�� ȣ���ϸ�, ���� ��ġ�� ���Ͽ� ���� Inode ������
// ������ �� �ִ�.
//
// bFindSubDir�� lpVolumePath�� ��Ȯ�� �ǹ̸� �޾Ƶ����� �ʱ� ������ �߻��Ͽ���.
// ���� lpVolumePath�� C:\*.* �� C:\*�� ���� �Ϻ� Directory�� list�ؾ� �Ѵٸ�,
// TRUE�� �����Ѵ�. �ܸ� File Ȥ�� Directory�� ������ ���Ѵٸ� FALSE�� �����Ѵ�.
// lpFileName�� Ntfs�� Open�� Volume�� ���ϴµ����� ���ȴ�. ��, C:�� D:\�� �̷� ������ �����´�.
// (lpFileName�� \\.\C:�� ���� Naming�� ��쵵 ����Ѵ�.)
// ��, C:\\�� ����Ǿ �����Ѵ�.
// ����,
//
//			FindFirstFileByInode(L"C:\\Data", &stInode, &stInodeEx, TRUE, ...)
//
// �� ���� ȣ���Ͽ��ٰ� ��������.
// stInode�� C:\Windows\system32�� �ش�Ǿ��ٸ�,
//
//			FindFirstFile(L"C:\Windows\system32\*.*", ...)
//
// �� ���� ȿ���� �ִ�. ����, Inode�� ���� ��찡 �ӵ��� ������.
// 
// �ܺ� I/O�� ����Ϸ��� pstIoFunction ���� ä�� �����Ѵ�. ����, �Ϲ����� ��� NULL �����ϴ�.
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

	// Ntfs�� ����.
	pstNtfs = FindFirstFileOpenNtfs(lpVolumePath, lpszPath, MAX_PATH_NTFS, pstIoFunction, pstInodeCache, &dwErrorCode, NULL);
	if (NULL == pstNtfs)
	{
		assert(FALSE);
		goto FINAL;
	}

	// Path�� Inode�� ã�� �ʴ´�.
	// ȣ��� �Ѿ�� inode ������ ������ FindFirstFileMain�� ȣ���Ѵ�.
	// ��, FindInodeFromPath�� ȣ������ �ʾ� �ð��� �����Ų��.

	// ����, FindFirstFileMain �Լ��� ȣ���Ѵ�.
	/*
	if (TRUE == bFindSubDir)
	{
		// Sub-directory�� ã�� ����,
		// Ȯ�� Data�� �ʿ����.
		pHandle = FindFirstFileMain(pstNtfs, pstInodeData, NULL, bFindSubDir, pstSubDirInodeData, lpFindFileData, &dwErrorCode);
	}
	else
	{
		// ���� ���� ���� ȹ������� ȣ��Ǵ� ���,...
		// Inode �Ӹ� �ƴ϶�, Ȯ�� Data�� �����ؾ� �Ѵ�.
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

		// ���� �߻�
		::SetLastError(dwErrorCode);

		if (NULL != lpFindFileData)
		{
			memset(lpFindFileData, 0, sizeof(*lpFindFileData));
		}
	}
	return pHandle;
}

// FindFirstFile�� �����ϰ�,
// ã�� �׸��� Inode ���� �����Ѵ�.
// �ܺ� I/O ���ǰ� �����Ǵ� FindFirstFileWithInode �Լ�
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

	// �� �Լ��� ��ü�� caller�� ���� ����� ȣ���� �߻��� �� �ִ�.
	// ���� �ǵ����̸�, Big size�� ���� ���� ����� �����Ѵ�. (Stack overflow ����)
	// �������� �Ҵ�޾� �������.
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

	// Ntfs�� ����.
	pstNtfs = FindFirstFileOpenNtfs(lpFileName, lpszPath, MAX_PATH_NTFS, pstIoFunction, pstInodeCache, &dwErrorCode, &bFindSubDir);
	if (NULL == pstNtfs)
	{
		assert(FALSE);
		goto FINAL;
	}

	// Path�� Inode�� ã�´�.
	// FindInodeFromPath �Լ��� �ð��� ���� �ɸ��� �Լ��̴�.
	/*
	if (TRUE == bFindSubDir)
	{
		// Inode���� ã�´�.
		dwErrorCode = FindInodeFromPath(pstNtfs, lpszPath, &nInode, pstFileNameInodeDataAlloc, NULL);
	}
	else
	{
		// ����, *.*�� ������ �ʾ�,
		// ���� ���� ���� ȹ������� ȣ��� ���,...
		// Inode �Ӹ� �ƴ϶�, Ȯ�� Data�� ���Ѵ�.
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

	// ����, FindFirstFileMain �Լ��� ȣ���Ѵ�.
	/*
	if (TRUE == bFindSubDir)
	{
		// Sub-directory�� ã�� ����,
		// Ȯ�� Data�� �ʿ����.
		pHandle = FindFirstFileMain(pstNtfs, pstFileNameInodeDataAlloc, NULL, bFindSubDir, pstInodeData, lpFindFileData, &dwErrorCode);
	}
	else
	{
		// ���� ���� ���� ȹ������� ȣ��Ǵ� ���,...
		// Inode �Ӹ� �ƴ϶�, Ȯ�� Data�� �����ؾ� �Ѵ�.
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

		// ���� �߻�
		::SetLastError(dwErrorCode);

		if (NULL != lpFindFileData)
		{
			memset(lpFindFileData, 0, sizeof(*lpFindFileData));
		}
	}
	return pHandle;
}

// FindFirstFile, FindFirstFileByInode, FindFirstFileWithInode
// ���� ȣ���ϴ� ���� Main Body �Լ�
//
// - lpszVolumeA�� \\.\C: �� ���� char ansi string�� �޾Ƶ��δ�.
//
// - Win32 API�� FindFirstFile("C:\*.*", ...)�� ���� *.*�̳� *�� ���ԵǾ�,
//   Sub-directory�� Ž���ؾ� �ϴ� ���, bFindSubDir�� TRUE�� �����Ѵ�.
//
// - ���н� INVALID_HANDLE_VALUE_NTFS�� ���ϵǴµ�,
//   �̶� pdwErrorCode ���� ERROR_SUCCESS�� �ƴ��� ����ȴ�.
//
// - Sub-directory�� Ž���ϴ� ���, ȣ���� ��û�� ����,
//   Sub-directory�� ù �׸� �ش�Ǵ� inode ������
//   pstSubDirInodeData�� �����Ѵ�.
//   ���� NULL �����ϴ�.
//   (bFindSubDir�� FALSE�� ���, pstSubDirInodeData��,
//    Sub-directory�� �ƴ϶� �ڱ� �ڽ��� �ǹ��Ѵ�.)
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

	// FindFirstFile �迭 �Լ�����,
	// ���ο��� ���� ��� ���� �ӽ� ����� ����
	stHandle.stFindFirstFile.lpszNameTmp = (PWCHAR)malloc(sizeof(*stHandle.stFindFirstFile.lpszNameTmp)*MAX_PATH_NTFS);
	if (NULL == stHandle.stFindFirstFile.lpszNameTmp)
	{
		*pdwErrorCode = ERROR_NOT_ENOUGH_MEMORY;
		assert(FALSE);
		goto FINAL;
	}
	memset(stHandle.stFindFirstFile.lpszNameTmp, 0, sizeof(*stHandle.stFindFirstFile.lpszNameTmp)*MAX_PATH_NTFS);

	// Ntfs�� �����´�.
	stHandle.pstNtfs = pstNtfs;
	if (NULL == stHandle.pstNtfs)
	{
		*pdwErrorCode = ERROR_OPEN_FAILED;
		assert(FALSE);
		goto FINAL;
	}

	// ȣ������ ��û�� ���� �÷��׸� �����Ѵ�.
	stHandle.stFindFirstFile.bFindSubDir = bFindSubDir;

	// Path�� Inode�� ã�´�.
	if (TRUE == stHandle.stFindFirstFile.bFindSubDir)
	{
		// Inode ���� ���޹޴´�.
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
		// ����, *.*�� ������ �ʾ�,
		// ���� ���� ���� ȹ������� ȣ��� ���,...
		// do nothing,...
	}

	if ((TRUE			== stHandle.stFindFirstFile.bFindSubDir) &&
		(TYPE_INODE_DOT	!= pstInodeData->nInode))
	{
		// ����, subdirectory �˻�,
		// ��, FindFirstFile("C:\...\*", ...)
		// ������ ȣ��Ǿ��ٸ�,
		// .
		// ..
		// �� �켱 Find�ǵ��� �Ѵ�.
		// ��, C:\*�� .�� ..�� ȣ����� �ʾ������� ���� ó���Ѵ�.
		//
		// ����, FindFirstFile�̳� FindNextFile ȣ���,
		// . Ȥ�� .. ��
		// �ʿ����� �ʴٸ�, �Ʒ� �κ��� �ּ� ó���ϸ� �ȴ�.
		stHandle.stFindFirstFile.bUseFindDot = TRUE;
	}

	if (TRUE == stHandle.stFindFirstFile.bFindSubDir)
	{
		// ���� *.*�� ����Ͽ�, Sub-directory���� �˻��ؾ� �Ѵٸ�,...

		if (TRUE == stHandle.stFindFirstFile.bUseFindDot)
		{
			*pstSubDirInodeDataAlloc = *pstInodeData;

			// ���� �̸��� ������ '.'�� �����Ѵ�.
			stHandle.stFindFirstFile.lpszNameTmp[0] = L'.';
			stHandle.stFindFirstFile.lpszNameTmp[1] = L'\0';

			// . �Ѱ� Find ��
			stHandle.stFindFirstFile.nDotCount = 1;

			// ..�� �ش�Ǵ� parent inode�� �����Ѵ�.
			stHandle.stFindFirstFile.nInodeParent = pstInodeData->llParentDirMftRef &  0x0000FFFFFFFFFFFFUL;

			// ���� inode�� �����Ѵ�.
			stHandle.stFindFirstFile.nInodeStart = pstInodeData->nInode;

			// .. �� ���޵� ���� �����Ѵ�.
			stHandle.stFindFirstFile.dotdot_dwFileAttributes	= pstInodeData->dwFileAttrib;
			stHandle.stFindFirstFile.dotdot_ftCreationTime		= GetLocalFileTime(pstInodeData->llCreationTime);
			stHandle.stFindFirstFile.dotdot_ftLastAccessTime	= GetLocalFileTime(pstInodeData->llLastAccessTime);
			stHandle.stFindFirstFile.dotdot_ftLastWriteTime		= GetLocalFileTime(pstInodeData->llLastDataChangeTime);
		}
		else
		{
			// . ó���� �ƴ� ���,...

			// ù �׸��� ã�´�.
			stHandle.stFindFirstFile.pstFind = FindFirstInode(stHandle.pstNtfs, nInode, pstSubDirInodeDataAlloc, stHandle.stFindFirstFile.lpszNameTmp, MAX_PATH_NTFS);
			if (NULL == stHandle.stFindFirstFile.pstFind)
			{
				// ù �׸��� ã�� ���ߴ�.
				// C:\aaaa
				// �� �ƹ��� �����̳� ���丮�� ������,
				// FindFirstFile�ϸ� ���⿡ �ش�Ǿ���.
				// assert������ �ʴ´�.
				stHandle.stFindFirstFile.bEmptryDirectory = TRUE;
				*pdwErrorCode = ERROR_NO_MORE_FILES;
				goto FINAL;
			}

			for (;;)
			{
				// PROGRA~1(dos path), $MFT(meta-data file)
				// �� ���� ��θ� Filter�Ѵ�.
				if (TRUE == FilterFindFirstFile(pstSubDirInodeDataAlloc))
				{
					// ���ǿ� �����Ѵ�.
					break;
				}

				// ���ǿ� ���� �ʴ� ���, ���ǿ� �´� inode�� ��Ÿ��������
				// Next�Ѵ�.
				if (FALSE == FindNextInode(stHandle.stFindFirstFile.pstFind, pstSubDirInodeDataAlloc, stHandle.stFindFirstFile.lpszNameTmp, MAX_PATH_NTFS))
				{
					// ����...
					// �̷� ��� �����Ѱ�?
					// ��, FindFirstInode�� �����ߴµ�,
					// Filter�� ���ؼ��� �Ѱ��� ������ �ʾ�����...
					*pdwErrorCode = ERROR_NO_MORE_FILES;
					assert(FALSE);
					goto FINAL;
				}
			}
		}
	}

	if (TRUE == stHandle.stFindFirstFile.bFindSubDir)
	{
		// ���� *.*�� ����Ͽ�, Sub-directory���� �˻��ؾ� �Ѵٸ�,...

		if (NULL != pstSubDirInodeData)
		{
			*pstSubDirInodeData = *pstSubDirInodeDataAlloc;
		}
	}
	else
	{
		// ���� �ܸ� ���� ��ȸ�� ���,...
		// ���, Subnode ������ �ƴ϶�, �ڽ��� �����̴�.
		// ���� ���� inode ������ �״�� �����Ѵ�.
		if (NULL != pstSubDirInodeData)
		{
			*pstSubDirInodeData = *pstInodeData;
		}
	}

	// Handle�� ������ ����
	stHandle.nType = TYPE_WIN32_HANDLE_FINDFIRSTFILE;

	// Lock Object ����
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

	// WIN32_FIND_DATA ���� �����Ѵ�.
	if (TRUE == stHandle.stFindFirstFile.bFindSubDir)
	{
		// Sub-directory�� inode ������ �����Ѵ�.
		if (FALSE == GetWin32FindData(pstSubDirInodeDataAlloc, stHandle.stFindFirstFile.lpszNameTmp, lpFindFileData))
		{
			*pdwErrorCode = ERROR_INTERNAL_ERROR;
			assert(FALSE);
			goto FINAL;
		}
	}
	else
	{
		// ���� inode ������ �����Ѵ�.
		if (FALSE == GetWin32FindData(pstInodeData, stHandle.stFindFirstFile.lpszNameTmp, lpFindFileData))
		{
			*pdwErrorCode = ERROR_INTERNAL_ERROR;
			assert(FALSE);
			goto FINAL;
		}
	}

	// Handle �� ����
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

// Win32 API�� FindNextFile�� �����ϰ� �����ȴ�.
BOOL CNTFSHelper::FindNextFile(IN NTFS_HANDLE hFindFile, OUT LPWIN32_FIND_DATA lpFindFileData)
{
	// Inode ���� ��û���� FindNextFileMain �Լ��� �����Ѵ�.
	// �ش� �Լ� ������, LOCK/UNLOCK�� ����ȴ�.
	return FindNextFileMain(hFindFile, lpFindFileData, NULL);
}

// FindNextFile�� �����ϳ�,
// Find�� �׸��� Inode ������ �Բ� �����Ѵ�.
BOOL CNTFSHelper::FindNextFileWithInodeExt(IN NTFS_HANDLE hFindFile, OUT LPWIN32_FIND_DATA lpFindFileData, OUT LPST_INODEDATA pstInodeData)
{
	// Inode ���� ��û�Ͽ���.
	if (NULL == pstInodeData)
	{
		::SetLastError(ERROR_INVALID_PARAMETER);
		assert(FALSE);
		return FALSE;
	}

	// Inode ���� ��û�Ѵ�.
	// �ش� �Լ� ������, LOCK/UNLOCK�� ����ȴ�.
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
		// ���� assert ������ ����.
		// assert(FALSE);
		// Lock���� ���� goto FINAL ��� �ٷ� return �Ѵ�.
		return dwErrorCode;
	}

	pstHandle = (LPST_WIN32_HANDLE)hFindFile;
	if (TYPE_WIN32_HANDLE_FINDFIRSTFILE != pstHandle->nType)
	{
		// �߸��� Handle�� ����
		dwErrorCode = ERROR_INVALID_HANDLE;
		assert(FALSE);
		// goto FINAL;
		// Lock���� ���� goto FINAL ��� �ٷ� return �Ѵ�.
		return dwErrorCode;
	}

	pstInodeDataAlloc = (LPST_INODEDATA)malloc(sizeof(*pstInodeDataAlloc));
	if (NULL == pstInodeDataAlloc)
	{
		dwErrorCode = ERROR_NOT_ENOUGH_MEMORY;
		assert(FALSE);
		// goto FINAL;
		// Lock���� ���� goto FINAL ��� �ٷ� return �Ѵ�.
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
		// ���� FindFirstFile�� *�� *.*�� ������ �ʾ�,
		// �ܸ� ���ϸ� �ϴ� ��쿣, FindNextFile�� �������� �ʴ´�.
		dwErrorCode = ERROR_NO_MORE_FILES;
		goto FINAL;
	}

	if ((TRUE	== pstHandle->stFindFirstFile.bUseFindDot) &&
		(1		== pstHandle->stFindFirstFile.nDotCount))
	{
		// ����, FindFirstFile�� . �� .. �� Ž���Ǵ� ��쿡
		// . ���� Ž���� ��쿡,...
		// ���� ..�� Ž���ǵ��� �ؾ� �Ѵ�.
		lpFindFileData->cFileName[0] = L'.';
		lpFindFileData->cFileName[1] = L'.';
		lpFindFileData->cFileName[2] = L'\0';

		lpFindFileData->dwFileAttributes	= pstHandle->stFindFirstFile.dotdot_dwFileAttributes;
		lpFindFileData->ftCreationTime		= pstHandle->stFindFirstFile.dotdot_ftCreationTime;
		lpFindFileData->ftLastAccessTime	= pstHandle->stFindFirstFile.dotdot_ftLastAccessTime;
		lpFindFileData->ftLastWriteTime		= pstHandle->stFindFirstFile.dotdot_ftLastWriteTime;

		// ���� .. ó���ߴ�.
		pstHandle->stFindFirstFile.nDotCount = 2;

		// ����
		bRtnValue = TRUE;
		goto FINAL;
	}

	if (2 == pstHandle->stFindFirstFile.nDotCount)
	{
		// .. ó�� ������ FindNextFile�� �����Ѵ�.
		if (NULL == pstHandle->stFindFirstFile.pstFind)
		{
			// NULL�� �翬�ϴ�.
			// ����, ���� FindFirstFile, FindNextFile�� �̷����� �ʾұ� �����̴�.
			// good
		}
		else
		{
			// bad
			dwErrorCode = ERROR_INTERNAL_ERROR;
			assert(FALSE);
			goto FINAL;
		}

		// ù �׸��� ã�´�.
		pstHandle->stFindFirstFile.pstFind = FindFirstInode(pstHandle->pstNtfs, pstHandle->stFindFirstFile.nInodeStart, pstInodeDataAlloc, pstHandle->stFindFirstFile.lpszNameTmp, MAX_PATH_NTFS);

		// ���� . �׸��� .. ó���� ������.
		pstHandle->stFindFirstFile.nDotCount = 3;

		// ����
		bRtnValue = TRUE;

		// �׷���...
		if (NULL == pstHandle->stFindFirstFile.pstFind)
		{
			// ù �׸��� ã�� ���ߴ�.
			// ��,
			// .
			// ..
			// �� Ž���� �Ǿ����� �� ���İ� Ž���� �ȵ� ����̴�.
			// ����ִ� directory�� ��쿡 �ش�ȴ�.
			bRtnValue = FALSE;
			dwErrorCode = ERROR_NO_MORE_FILES;
			pstHandle->stFindFirstFile.bEmptryDirectory = TRUE;
			goto FINAL;
		}
	}
	else
	{
		// ���� ���Ϸ� �̵��Ѵ�.
		bRtnValue = FindNextInode(pstHandle->stFindFirstFile.pstFind, pstInodeDataAlloc, pstHandle->stFindFirstFile.lpszNameTmp, MAX_PATH_NTFS);
		if (FALSE == bRtnValue)
		{
			// [TBD]
			// FindNextFileNtfs���� ��-�������� �������� ��κ��̴�.
			// �̶��� ���� �������� ����ȭ�� �ʿ䰡 �ִ�.
			// �׷��� ���ؼ��� pstFind ����� Last Error ���ο� ���� �м��� �ʿ��� ���̴�.
			// ����� FindNextFile �ó��������� ���� ������ �ǹ���,
			// ERROR_NO_MORE_FILES �� �����ϵ��� �Ѵ�.
			dwErrorCode = ERROR_NO_MORE_FILES;
		}
	}

	// ���� Inode�� ã�µ� �����ߴ�.

	// PROGRA~1,
	// $MFT
	// �� ���� ��θ� Filter�Ѵ�.
	for (;;)
	{
		if (TRUE == FilterFindFirstFile(pstInodeDataAlloc))
		{
			// ���ǿ� �����Ѵ�.
			break;
		}

		// ���ǿ� ���� �ʴ� ���, ���ǿ� �´� inode�� ��Ÿ��������
		// Next�Ѵ�.
		bRtnValue = FindNextInode(pstHandle->stFindFirstFile.pstFind, pstInodeDataAlloc, pstHandle->stFindFirstFile.lpszNameTmp, MAX_PATH_NTFS);
		if (FALSE == bRtnValue)
		{
			// ����...
			// �̷� ��� �����Ѱ�?
			// ��, FindFirstInode�� �����ߴµ�,
			// Filter�� ���ؼ��� �Ѱ��� ������ �ʾ�����...
			dwErrorCode = ERROR_NO_MORE_FILES;
			break;
		}
	}

	if (TRUE == bRtnValue)
	{
		// ������ ���޸�,...
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
		// ����...
		// ȣ���ڴ� GetLastError�� �� ���̴�.
		::SetLastError(dwErrorCode);

		if (NULL != pstInodeData)
		{
			memset(pstInodeData, 0, sizeof(*pstInodeData));
		}
	}

	return bRtnValue;
}

// Win32 API�� FindClose�� �����ϰ� �����ȴ�.
BOOL CNTFSHelper::FindClose(IN NTFS_HANDLE hFindFile)
{
	BOOL				bRtnValue	= FALSE;
	DWORD				dwErrorCode	= ERROR_INVALID_PARAMETER;
	LPST_WIN32_HANDLE	pstHandle	= NULL;
	LPVOID				pLock		= NULL;

	if ((INVALID_HANDLE_VALUE_NTFS == hFindFile) || (NULL == hFindFile))
	{
		dwErrorCode = ERROR_INVALID_HANDLE;
		// ���� assert������ ����.
		// assert(FALSE);
		::SetLastError(dwErrorCode);
		return FALSE;
	}

	pstHandle = (LPST_WIN32_HANDLE)hFindFile;
	if (TYPE_WIN32_HANDLE_FINDFIRSTFILE != pstHandle->nType)
	{
		// �߸��� Handle�� ����
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

	// �ݴ´�.
	if (NULL != pstHandle->stFindFirstFile.pstFind)
	{
		bRtnValue = FindCloseInode(pstHandle->stFindFirstFile.pstFind);
	}
	else
	{
		// ���� Handle�� NULL��
		if (FALSE == pstHandle->stFindFirstFile.bFindSubDir)
		{
			// FindFirstFile�� *�� *.*�� ���Ǿ� �ܸ� ���ϸ� Ž���ϴ� ��쿡��,
			// �ش� Handle�� NULL�̴�.
			bRtnValue = TRUE;
		}
		else if ((TRUE == pstHandle->stFindFirstFile.bUseFindDot) &&
				 (TRUE == pstHandle->stFindFirstFile.bEmptryDirectory))
		{
			// . �� .. Ž���� ����ϴ� ���,
			// Directory�� ����,
			// FindFirstFile�� �����Ѵ�.
			// ���� ����ִ� ����� ���, pstFind�� NULL�� �� �ִ�.
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

	// Handle �޸� �����Ѵ�.
	free(pstHandle);
	pstHandle = NULL;

// ������ ����
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
		// ����
		::SetLastError(dwErrorCode);
	}

	return bRtnValue;
}

// Wild-card�� \\?\���� ���ŵ� Path�� �����Ѵ�.
// ��,
// C:\Windows
// �� ���� ��θ� ���Ѵ�.
// ������ \���ڴ� Trim ���� �ش�.
// �ּ� ���̰� 3�̻�(X:\)���� ������� �ش�.
// pchVolumeDrive�� CHAR ���� �ϳ��� �ش�Ǹ�, CHAR�� �� volume�̸��� �����Ѵ�.
// ��, C:\Windows���, 'C'�� �����Ѵ�.
// �׸���, Win32 API����,
// FindFirstFile(L"C:", &data);
// �� �ϸ� IDE��� �����̸����� ���� ������ �޴´�.
// ���� �ش� Handle�� FindNextFile�� ���еȴ�.
// �Ƹ��� Volume�� ���� ������ ������ �����ϴµ� �ѵ�,
// ���⿡���� C:�� ���� ��θ� ������� �ʵ��� �Ѵ�.
// ����, *.* Ȥ�� *�� ����� ���� ���Ǿ��ٸ�, *pbFindSubDir = TRUE�� �����Ѵ�.
//
// [����] CreateFile(lpFileName, ...)�� lpFileName�� volume ��ο� canonical�� ���� ȣ��ȴ�.
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
		// ���� \�� �����Ѵٸ�,
		// \\?\�� ���Ǿ����� Ȯ���Ѵ�.
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
			// 4���� �ڷ� ����.
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
		// ù ���ڴ� �ݵ�� ����
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
		// ?���ڴ� ��� ����
		dwErrorCode = ERROR_NOT_SUPPORTED;
		assert(FALSE);
		goto FINAL;
	}

	// *.*�� Skip�� �� �ֵ��� �Ѵ�.
	lpszFind = wcsstr(lpFileName, L"*.*");
	if (NULL != lpszFind)
	{
		// ���� *.*�� ���� ���,...
		if ((TEXT('\0') == lpszFind[3]) && (TEXT('\\') == lpszFind[-1]))
		{
			// �տ� \�����̰�, ���� *�� ���Ǿ���
			*pbFindSubDir = TRUE;
			StringCchCopyN(lpszPath, dwCchPath, lpFileName, lpszFind-lpFileName);
		}
		else
		{
			// *.* ���ڰ� ���Ǿ�����,
			// ���� ������ �ʾҴ�.
			// �������� �ʴ´�.
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
		// ���� * ���ڰ� ���ԵǾ� �ִ� ���,...
		if ((TEXT('\0') == lpszFind[1]) && (TEXT('\\') == lpszFind[-1]))
		{
			// �׷���, ���� *�� ���Ǿ���
			// �װ��� *.*�� ������ ȿ��
			*pbFindSubDir = TRUE;
			StringCchCopyN(lpszPath, dwCchPath, lpFileName, lpszFind-lpszPath);
		}
		else
		{
			// * ���ڰ� ���Ǿ�����,
			// ���� ������ �ʾҴ�.
			// �������� �ʴ´�.
			memset(lpszPath, 0, sizeof(WCHAR)*dwCchPath);
			dwErrorCode = ERROR_NOT_SUPPORTED;
			assert(FALSE);
			goto FINAL;
		}
	}

	if ((TEXT('a') <= lpszPath[0]) && (lpszPath[0] <= TEXT('z')))
	{
		// ���� �ҹ��� a~z
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

	// ���� \�� Trim�Ѵ�.
	// ��, C:\�� Trim���� �ʴ´�.
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

	// ������� �Դٸ� ����
	dwErrorCode = ERROR_SUCCESS;

FINAL:
	return dwErrorCode;
}

// C:\ABC\DEF\FG
// ���� ���, ��� ȣ���ϸ�,
// ABC(=ERROR_SUCCESS), DEF(=ERROR_SUCCESS), FG(=ERROR_HANDLE_EOF)�� ���� �����Ѵ�.
// ��, �ʱ� pnPos���� 0���� �����ؾ� �Ѵ�.
//
// ERROR_SUCCESS : Component ����
// ERROR_HANDLE_EOF : ������ ����
// �̿� : ����
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
		// ���� \��ġ�� ������ ��
		*pnPos = 2;
	}

	if (TEXT('\0') == lpszPath[*pnPos])
	{
		dwRtnValue = ERROR_HANDLE_EOF;
		goto FINAL;
	}

	// \ ���ڿ� ���� �Ľ��Ѵ�.
	lpszFind = wcsstr(&lpszPath[*pnPos + 1], TEXT("\\"));

	if (NULL == lpszFind)
	{
		// ������ �� �����.
		dwRtnValue = ERROR_HANDLE_EOF;
		StringCchCopyW(lpszComponent, dwCchComponent, &lpszPath[*pnPos + 1]);
		*pnPos += wcslen(lpszComponent) + 1;
		if (wcslen(lpszComponent) > 0)
		{
			// ������ �׸��� �ִٸ�,..
			dwRtnValue = ERROR_SUCCESS;
		}
		goto FINAL;
	}

	// ������ ���� ���� �����
	dwRtnValue = ERROR_SUCCESS;
	StringCchCopyNW(lpszComponent, dwCchComponent, &lpszPath[*pnPos + 1], lpszFind - &lpszPath[*pnPos + 1]);
	*pnPos += (wcslen(lpszComponent) + 1);

FINAL:
	return dwRtnValue;
}

// pstNtfs�� lpszPath�� volume�� �����־�� �Ѵ�.
// ��, C:\windows\system32
// �� ���, pstNtfs�� \\?\C:�� ���� ���̾�� �Ѵ�.
// ����, pstNtfs�� \\?\D:�� �����־��ٸ�,
// D:\windows\system32
// �� �ش�Ǵ� inode�� ���޵� �� �ִ�.
// pstInodeData�� ���ʿ�� NULL �����ϴ�.
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

	// �ʱ� Inode
	nInode = TYPE_INODE_DOT;

	// C:\123\abc\qwe ���
	// DOT inode�� ����,
	// "123"�� �ش�Ǵ� inode�� ã��,
	// �ش� inode�� ���� ���۵� "abc" inode�� ã��,
	// �ش� inode�� ���� ���۵� "qwe" inode�� ã�´�.

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
			// ã�� ������
			dwRtnValue = ERROR_FILE_NOT_FOUND;
			goto FINAL;
		}
	}

	if (3 == nPos)
	{
		// ���� C:\�� ���� ���� ���,...
		// INODE_DOT ��ġ�� data�� �����Ѵ�.
		// INODE_DOT���� ���� �����Ͽ� "."�� ã����,
		// INODE_DOT�� ���ϵȴ�.
		nInode = CNTFSHelper::FindInode(pstNtfs, TYPE_INODE_DOT, L".", &bFound, pstInodeData);
		if ((FALSE == bFound) || (TYPE_INODE_DOT != nInode))
		{
			dwRtnValue = ERROR_FILE_NOT_FOUND;
			assert(FALSE);
			goto FINAL;
		}
	}

	// �� ����
	*pnInode = nInode;

	// ������� �Դٸ� ����
	dwRtnValue = ERROR_SUCCESS;

FINAL:
	if (NULL != lpszComponent)
	{
		free(lpszComponent);
		lpszComponent = NULL;
	}
	return dwRtnValue;
}

// [TBD] Other Platform (��, linux) ����
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
		// ���н�...
		assert(FALSE);
		nRtnValue = nTime;
	}

	return nRtnValue;
}

// INDEX_ENTRY ==> INDEX_ENTRY_NODE�� ��ȯ�Ѵ�.
// INDEX_ENTRY�� ���õ� ������ ����� Cluster �޸𸮰� Free�Ǹ�, ������ �ֹ��ϴµ�,
// INDEX_ENTRY_NODE�� ���� �޸𸮸� ������ �־�, dangling reference�� �߻����� �ʴ´�.
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
		// ������ �׸��̶��,...
		pstIndexEntryNode->bLast = TRUE;
	}

	if (INDEX_ENTRY_FLAG_SUBNODE == (pstIndexEntry->nFlag & INDEX_ENTRY_FLAG_SUBNODE))
	{
		// B-Tree���� subnode�� �ִ� ��쿡,...
		pstIndexEntryNode->bHasSubNode = TRUE;

		// http://inform.pucp.edu.pe/~inf232/Ntfs/ntfs_doc_v0.5/attributes/index_allocation.html
		// �� Index Entry �κ��� ����,
		// ������ -8 byte�� subnode �׸���� �ִ� vcn�� ����Ǿ� �ִ� ���̴�.
		pstIndexEntryNode->nVcnSubnode = *(UBYTE8*)((UBYTE1*)pstIndexEntry + pstIndexEntry->nLengthIndexEntry - 8);
	}

	if (pstIndexEntry->nLengthStream > 0)
	{
		// Data�� �ִٸ�,...
		if (FALSE == GetIndexEntryFileName(pstIndexEntry, lpszName, MAX_PATH_NTFS))
		{
			assert(FALSE);
			goto FINAL;
		}

		// �̸��� �Ҵ��Ѵ�.
		pstIndexEntryNode->lpszFileNameAlloc = _wcsdup(lpszName);
		if (NULL == pstIndexEntryNode->lpszFileNameAlloc)
		{
			assert(FALSE);
			goto FINAL;
		}

		// Inode�� ���Ѵ�.
		pstIndexEntryNode->nInode = GetInode(pstIndexEntry, NULL);

		// ������ �������� ä���.
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

		// Data�� �ִٴ� �������� �������Ѵ�.
		pstIndexEntryNode->bHasData = TRUE;
	}

	// ������� �Դٸ� ����
	bRtnValue = TRUE;

FINAL:

	if (NULL != lpszName)
	{
		free(lpszName);
		lpszName = NULL;
	}

	if (FALSE == bRtnValue)
	{
		// ����
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

// �־��� index entry�� ���� ���۵� index entry�� list��,
// ���� �Ҵ�� �迭�� ����� �����Ѵ�.
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

	// �켱 ������ ���Ѵ�.
	// [TBD] Maximum�� �����ؾ� �ϳ�?
	pstIndexEntryPos = pstIndexEntry;
	for (;;)
	{
		if (INDEX_ENTRY_FLAG_LAST_ENTRY == (pstIndexEntryPos->nFlag & INDEX_ENTRY_FLAG_LAST_ENTRY))
		{
			// ������ �׸�

			if (INDEX_ENTRY_FLAG_SUBNODE == (pstIndexEntryPos->nFlag & INDEX_ENTRY_FLAG_SUBNODE))
			{
				// �׷���, ������ �׸� subnode ������ �ִٸ�,
				// �߰��Ѵ�.
				stIndexEntryArray.nItemCount++;
			}

			break;
		}

		pstIndexEntryPos = GetNextEntry(pstIndexEntryPos);
		if ((INDEX_ENTRY_FLAG_LAST_ENTRY != (pstIndexEntryPos->nFlag & INDEX_ENTRY_FLAG_LAST_ENTRY)) &&
			(0							 == pstIndexEntryPos->nLengthIndexEntry))
		{
			// ����, Index Entry�� ũ��� 0�ε�,
			// Last Entry ���� ���� ���,...
			bForceLastSetting = TRUE;
			break;
		}

		stIndexEntryArray.nItemCount++;
	}

	if (0 == stIndexEntryArray.nItemCount)
	{
		// ����ִ� Directory�� ����̴�.
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
		// �� ��ȯ�Ѵ�.
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
		// ����
		if (NULL != stIndexEntryArray.pstArray)
		{
			free(stIndexEntryArray.pstArray);
			stIndexEntryArray.pstArray = NULL;
		}
	}
	return pstRtnValue;
}

// INDEX_ENTRY�� array�� ���������Ѵ�.
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

// INDEX_ENTRY�� Array�� ���Ѵ�.
// �̴�, ������ INDEX_ENTRY�� subnode�� ���� ��,
// �ش� subnode�� Vcn ��ġ�� �ִ� Index block�� �о�,
// �װ��� INDEX_ENTRY Array�� �����ϴ� ���̴�.
// �ٽ� �����ڸ�, ������ INDEX_ENTRY�� B-Tree ���� subnode��
// �ִ� INDEX_ENTRY�� �迭�� �����Ѵٰ� ���� �ȴ�.
// Input�� �ش�Ǵ� Run list�� $INDEX_ALLOCATION�� Run list�� �����ؾ� �Ѵ�.
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

	// Buffer�� �Ҵ��Ѵ�.
	// Buffer�� Cluster ũ�� ������ �ƴ϶�,
	// Index Block ũ�⸸ŭ �Ҵ��ϰ� �ȴ�.
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
		// ���� IndexBlockSize�� 4K(=4096) byte�̴�.
		// ���� Cluster ũ�Ⱑ 4KB ���϶��,
		// 1 VCN ũ��� Cluster ũ��� ���εȴ�.
		// ���� Cluster ũ��� 4KB�� �����ǹǷ�,
		// �Ϲ����� ���, �̰��� �ش�ȴ�.

		// ����, ��û�� Vcn ��ġ�� 0 �� ��쿡�� �ش�ȴ�.

		// Vcn�� Lcn���� ��ȯ�Ѵ�.
		// ����, ntfs document�� �˷��� ������� Vcn -> Lcn ������ ��ȯ�Ѵ�.
		if (FALSE == GetLcnFromVcn(pstRunList, nCountRunList, nVcn, 0, &nLcn))
		{
			// ����
			assert(FALSE);
			goto FINAL;
		}

		// �ش� Vcn�� ��ġ�� Index Block�� ��ġ��,
		// ���ε� Lcn���� �ش�Ǵ� Cluster ��ġ�̹Ƿ�,
		// �ܼ��� Lcn ���� Cluster ũ�⸦ ���ϸ� �ȴ�.
		nPos = nLcn * pstNtfs->stNtfsInfo.dwClusterSize;
	}
	else
	{
		// ���� ���, $INDEX_ALLOCATION�� runlist�� ���ؼ�, vcn ���� 0~3�� ���Դٰ� ��������.
		// �׷���, ��û�ϴ� vcn���� 0~3 ���̿� �־�� �Ѵ�.
		// �׷���, 64KB�� Cluster�� ���˵� ���,
		// �ش� 0~3 ������ �������. (���� ���, 16)
		// ��, 64KB�� Cluster�� ���, INDEX_ENTRY�� subnode�� �ִ� ���,
		// �ش� subnode�� Vcn���� ������ �Ϲ������� �ʴ´ٴ� ���̴�.

		// �� �κ��� �Ϲ����� �˷��� ntfs document�� �� ��Ÿ�� ���� �ʴ�.

		// document�� ���� Ȯ��)
		// http://inform.pucp.edu.pe/~inf232/Ntfs/ntfs_doc_v0.5/attributes/index_root.html
		// ��
		// __u8 clusters_per_index_block;
		// �ּ��� �����Ѵ�.

		// ntfs-3g�� ���� Ȯ��)
		//
		// ���� Cluster ũ�Ⱑ IndexBlockSize(�Ϲ�������, 4KB)���� ũ�ٸ�,...
		// 1 VCN ũ��� �Ϲ����� Cluster ũ�� ������ �ش���� �ʴ´�.
		// ����)
		// ntfsprogs(ntfs-3g)�� source : http://www.tuxera.com/community/ntfs-3g-download/
		// �� dir.c �κ�
		// Ȥ��
		// http://fossies.org/linux/misc/old/ntfsprogs-1.12.1.tar.gz:a/ntfsprogs-1.12.1/libntfs/dir.c
		//
		// ntfs_inode_lookup_by_name()
		// �Լ� �߰��� ����,
		//
		// if (vol->cluster_size <= index_block_size)
		//		index_vcn_size = vol->cluster_size;
		// else
		//		index_vcn_size = vol->sector_size;
		//
		// ���� ���´�. �ش� �κ��� �����Ѵ�.
		//
		// �ٽ� �������ڸ�, 
		//	ntfs_inode_lookup_by_name(...)
		//		-> ntfs_attr_mst_pread(..., Vcn=16 << index_vcn_size_bits=9 ,...)
		//		   (��, 16�� Vcn�� �������� ����. �׷���, 16�� 512��ŭ ���� ȿ����, 8192�� ����)
		//			-> ntfs_attr_pread_i(..., 8192, ...)
		//			   (���ο��� $INDEX_ALLOCATION�� runlist�� ����.
		//			    ���� runlist�� Vcn�� �Ѿ�� 8192���� >> 16(cluster_size_bits), ��, Cluster ũ��� ����
		//			    �׷���, 
		//				-> ntfs_attr_find_vcn(...)
		// ���� ȣ��Ǵ� callstack�� Ȯ���Ѵ�.

		// �ٽø����ڸ�,
		// $INDEX_ROOT�κ��� Ž���� INDEX_ENTRY list����, subnode�� �ִ� ���,
		// subnode�� ����� INDEX_ENTRY�� vcn�� ����Ǿ� �ִ�.
		// �׷���,
		// 4K�� �ʰ��ϴ� Cluster�� ���˵� �ý����� ���,
		// $INDEX_ALLOCATION�� runlist�� ���� �����Ǵ� vcn ������ ����� ��찡 �ִ�.
		// ��, INDEX_ENTRY�� subnode vcn����, �� ������ vcn�� �ƴ϶�� ���̴�.
		// Ȯ�ΰ��, �ش� ������ Lcn-Vcn �������� �̷��� Cluster ũ�� ������ �ƴ϶�,
		// Sector��������.
		// ��, ����, $INDEX_ALLOCATION�� runlist���� vcn�� ������ 0~3�� ��Ȳ����,
		// subnode�� vcn ���� 16�̶��,
		// ���� �� 16�� Cluster ������ �ƴ϶�, sector �����̴�.
		// �׷���, 16 * 512(sector ũ��) ==> 8192(byte ��ġ)��
		// subnode INDEX_ENTRY�� ����Ǿ� �ִٴ� ���̴�.
		// �׷���, 8192 byte�� ��ġ�� ã�� ���ε�,
		// 8192 / 64K(Cluster ũ��) ==> 0(cluster ��ġ, Lcn)
		// �� ���� 0�� Lcn�� ��ġ�Ѵٴ� ���̴�.
		// �׷�����, ��Ȯ�� 0�� Lcn ��ġ�� ���ϴ°��� �ƴϴ�.
		// �ֳ��ϸ�, 8192�� 64K�� ��Ȯ�� ���� �������� �ʾұ� �����̴�.
		// ��, 0�� Lcn�� �� ������ ��(8192 byte)���� offset ó���� �ؾ� �Ѵ�.
		//
		// �׷��ٸ�, 16�� vcn�� �ش�Ǵ� INDEX_ENTRY�� ã�� ���ؼ���
		// 0�� Lcn ��ġ����, 8192 offset�� ���� ��ġ�� ��Ȯ�� subnode ��ġ�� �ȴ�.

		// �ش� Vcn ���� �ش�Ǵ� 
		nVcnPos = nVcn * pstNtfs->stNtfsInfo.wSectorSize;

		// ��, �ش� Vcn�� �ش�Ǵ� ���� �� nVcnPos ��(byte ����)�� �ش�ȴ�.
		// �׷�, �ش� byte������ $INDEX_ALLOCATION�� runlist�� � Vcn�� ���ԵǴ��� ã�ƺ���.
		// ã�°� �����ϴ�. Cluster ũ��� ������ �ȴ�.
		nVcnCalc = nVcnPos / pstNtfs->stNtfsInfo.dwClusterSize;

		// ����, ���� Vcn������ Lcn���� ��ȯ�Ѵ�.
		if (FALSE == GetLcnFromVcn(pstRunList, nCountRunList, nVcnCalc, 0, &nLcn))
		{
			// ����
			assert(FALSE);
			goto FINAL;
		}

		// �׷���, �̷� ���, Offset���� �����Ѵ�.
		// ��, ��Ȯ�ϰ� ��û�� byte ��ġ(nVcnPos)�� Cluster ��谡 �ƴ� �� �ִٴ� ���̴�.
		// �׷��� ��Ȯ�� Sector ��ġ(byte ����)�� ���Ѵ�.
		nPos = nLcn * pstNtfs->stNtfsInfo.dwClusterSize + nVcnPos % pstNtfs->stNtfsInfo.dwClusterSize;
	}

	// �ش� ��ġ�� ����.
	if (FALSE == SeekByte(pstNtfs, nPos))
	{
		assert(FALSE);
		goto FINAL;
	}

	// ��ġ�� �̵��ߴ�.
	// �׷���, ���� ����.
	// ������ ũ��� Cluster ũ�Ⱑ �ƴ϶�, Index Block Size ��ŭ �д´�.
	if (nIndexBlockSize != ReadIo(pstNtfs->pstIo, pBufCluster, nIndexBlockSize))
	{
		assert(FALSE);
		goto FINAL;
	}

	// $INDEX_ALLOCATION�� STANDARD_INDEX_HEADER�� �����´�.
	pstStdIndexHeader = (LPST_STANDARD_INDEX_HEADER)pBufCluster;

	// INDX ����
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

	// Update Sequence ó���� �Ѵ�.
	if (FALSE == UpdateSequenceIndexAlloc(nIndexBlockSize, pBufCluster, pstNtfs->stNtfsInfo.dwClusterSize, pstNtfs->stNtfsInfo.wSectorSize))
	{
		assert(FALSE);
		goto FINAL;
	}

	// ���� ���� �ʱ� Index Entry�� ���ȴ�.
	pstIndexEntry = (LPST_INDEX_ENTRY)((UBYTE1*)&pstStdIndexHeader->nOffsetEntry + pstStdIndexHeader->nOffsetEntry);

	// �׷� ���� Index Entry ������� Array�� ���Ѵ�.
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

// directory�� �ش�Ǵ� inode�� $INDEX_ROOT�� ������,
// b-tree ���� root�� �ش�Ǵ� index block�� ���� �� �ִ�.
// �ش� index block�� INDEX_ENTRY�� array�� ���Ѵ�.
// index block�� ũ��� $INDEX_ROOT�� ���ǵǱ� ������,
// caller�� �����Ѵ�. �ش� index block�� ũ���,
// $INDEX_ALLOCATION�� ���� ������ ���ȴ�.
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

	// $INDEX_ROOT�� ���Ѵ�.
	pstIndexRootAlloc = FindAttribAlloc(pstNtfs, nInode, TYPE_ATTRIB_INDEX_ROOT);
	if (NULL == pstIndexRootAlloc)
	{
		assert(FALSE);
		goto FINAL;
	}

	// $INDEX_ROOT�� ���� ù��° Entry�� ���Ѵ�.
	pstIndexRoot   = (LPST_INDEX_ROOT)((UBYTE1*)pstIndexRootAlloc->pstAttrib + pstIndexRootAlloc->pstAttrib->stAttr.stResident.wValueOffset);
	pstIndexHeader = (LPST_INDEX_HEADER)((UBYTE1*)pstIndexRoot + sizeof(*pstIndexRoot));
	pstIndexEntry  = (LPST_INDEX_ENTRY)((UBYTE1*)&pstIndexHeader->nOffsetFirstIndexEntry + pstIndexHeader->nOffsetFirstIndexEntry);

	pstRtnValue = GetIndexEntryArrayAlloc(pstIndexEntry);
	if (NULL == pstRtnValue)
	{
		// ����ִ� Directory�� ��찡 ���Եȴ�.
		goto FINAL;
	}

	// Index�� ũ�⸦ �����Ѵ�.
	*pnIndexSize = pstIndexRoot->nSize;

FINAL:
	if (NULL != pstIndexRootAlloc)
	{
		FindAttribFree(pstIndexRootAlloc);
		pstIndexRootAlloc = NULL;
	}
	return pstRtnValue;
}

// Win32 API�� FindFirstFile / FindNextFile�� ���� ���·�, Directory�� ������ �����Ҷ� ó�� ȣ���Ѵ�.
// Win32 API �Լ��� �ٸ���,
// $MFT, $BITMAP�� ���� meta-data file�� PROGRA~1�� ���� dos ���� �̸��� ���´�.
//
// nInode : ������ inode
// pstInodeData : ù��° �׸��� inode ����
//
// Remark : b-tree traverse ���� CNTFSHelper::CreateStack(...) �ּ� ����
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

	// $INDEX_ROOT�� ���Ե� ����� ���Ѵ�.
	pstArray = GetIndexEntryArrayAllocByIndexRoot(pstNtfs, nInode, &stFind.nIndexBlockSize);
	if (NULL == pstArray)
	{
		// ����ִ� Directory�� ��찡 ���Եȴ�.
		goto FINAL;
	}

	// $INDEX_ROOT�� ����� ���������� ���ؿԴٸ�,
	// $INDEX_ALLOCATION���� ���� RunList�� ���ؿ´�.
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

	// Stack�� �����Ѵ�.
	stFind.pstStack = CreateStack(5);
	if (NULL == stFind.pstStack)
	{
		assert(FALSE);
		goto FINAL;
	}

	// Stack�� push �Ѵ�.
	stStackNode.pstIndexEntryArray = pstArray;
	if (FALSE == StackPush(stFind.pstStack, &stStackNode))
	{
		assert(FALSE);
		goto FINAL;
	}

	// FindNextInode�� �̿��Ͽ� ù �׸��� ã�´�.
	if (FALSE == FindNextInode(&stFind, pstInodeData, lpszName, dwCchName))
	{
		assert(FALSE);
		goto FINAL;
	}

	// ���ϰ��� �����Ѵ�.
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
		// ����
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

		// Stack�� ����. [TBD]
	}

	if (NULL != pstIndexAlloc)
	{
		FindAttribFree(pstIndexAlloc);
		pstIndexAlloc = NULL;
	}

	return pstRtnValue;
}

// FindFirstInode ������ File�� ������ �ش�.
// ���� Win32 API�� FindNextFile�� �����ϴ�.
// FindFirstInode�� �ּ� ����.
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
		// Traverse ����
		goto FINAL;
	}

	if (TRUE == pstFind->bErrorOccurred)
	{
		// ������ ���� Traverse ����
		assert(FALSE);
		goto FINAL;
	}

	if (TRUE == IsStackEmptry(pstFind->pstStack))
	{
		// Traverse ����
		goto FINAL;
	}

	// B-Tree��, Array�� ���� ��ġ �κп� Subnode�� ������ �������� �Ѿ��.
	// ��, Leaf node�� ���� B-Tree�� Traverse�ؾ� �Ѵ�.
	for (;;)
	{
		// Stack�� �� �� �׸��� �۾� ����̴�.
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
			// ���� ���� �׸��� ������ ����� ��쿡,...

			// Stack�� Top �κ��� �̹� ��� ����Ǿ���.
			// �׷��� ����� ���� ������.
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
				// Traverse�� ����ȴ�.
				pstFind->bFinished = TRUE;
				break;
			}

			// �ٽ� �ֻ���� �����´�.
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

		// �Ʒ���, ���� Ž���� Index Entry��,
		//		- Subnode�� �ִ�.
		//		- ���� Subnode�� �� �� ����.
		// �� ���,
		// Subnode�� Ž�� ������ ������ Stack �� ��ܿ� Push�Ѵ�.
		if ((TRUE  == stNode.pstIndexEntryArray->pstArray[stNode.pstIndexEntryArray->nCurPos].bHasSubNode) &&
			(FALSE == stNode.pstIndexEntryArray->pstArray[stNode.pstIndexEntryArray->nCurPos].bSubnodeTraversed))
		{
			stNode.pstIndexEntryArray->pstArray[stNode.pstIndexEntryArray->nCurPos].bSubnodeTraversed = TRUE;

			// Subnode�� �ִ�...
			// �׸���, ���� Traverse���� �ʾҴ�.
			// �������� �Ѿ��.
			pstArray = GetIndexEntryArrayAllocByVcn(pstFind->pstNtfs, pstFind->nIndexBlockSize, pstFind->pstRunListAlloc, pstFind->nCountRunList, stNode.pstIndexEntryArray->pstArray[stNode.pstIndexEntryArray->nCurPos].nVcnSubnode);
			if (NULL == pstArray)
			{
				// subnode�� ���µ� ������
				pstFind->bErrorOccurred = TRUE;
				assert(FALSE);
				goto FINAL;
			}

			// Stack�� Push �Ѵ�.
			stNode.pstIndexEntryArray = pstArray;
			if (FALSE == StackPush(pstFind->pstStack, &stNode))
			{
				pstFind->bErrorOccurred = TRUE;
				assert(FALSE);
				goto FINAL;
			}

			// Loop�� �ٽ� ��������.
			continue;
		}
		else
		{
			// do nothing...
		}

		// ����Դٴ� ����, subnode�� �ƴ϶�� ���̴�.
		if (FALSE == stNode.pstIndexEntryArray->pstArray[stNode.pstIndexEntryArray->nCurPos].bHasData)
		{
			// ���� data�� ����...
			// �׷� ���� �׸����� �Ѿ��.
			stNode.pstIndexEntryArray->nCurPos++;
			continue;
		}

		// ����Դٴ� ����, subnode�� �ƴϸ鼭, data�� �ִٴ� ���̴�.
		if (stNode.pstIndexEntryArray->nCurPos >= stNode.pstIndexEntryArray->nItemCount)
		{
			// �Ѿ��.

			// Stack�� Top �κ��� �̹� ��� ����Ǿ���.
			// �׷��� ����� ���� ������.
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
				// Traverse�� ����ȴ�.
				pstFind->bFinished = TRUE;
				break;
			}

			// �ٽ� �ֻ���� �����´�.
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
			// ��, Loop�� �ٽ� �������� �ʴ´�.
			// ��, ���� ��ġ��, Subnode�� �� ����ǰ�,
			// ���� �ٽ� ���� node�� �ö�� ����̴�.
			// �̶���, �Ʒ�, ��, Data�� ������ ������ �����Ѵ�.
		}

		// ����Դٴ� ����, subnode�� �ƴϰ�, data�� ������, array�� �߰��� �ִٴ� ���̴�.
		if (FALSE == GetInodeData(&stNode.pstIndexEntryArray->pstArray[stNode.pstIndexEntryArray->nCurPos], pstInodeData, lpszName, dwCchName))
		{
			pstFind->bErrorOccurred = TRUE;
			assert(FALSE);
			goto FINAL;
		}

		// �׸��� �߰��Ͽ���.
		bRtnValue = TRUE;

		// Loop�� �����.
		break;
	}

	if (TRUE == bRtnValue)
	{
		// ���� �׸����� �̵�����.
		stNode.pstIndexEntryArray->nCurPos++;
	}

FINAL:
	return bRtnValue;
}

// Inode�� ������ �����Ѵ�.
// [TBD] Other platform (ex, linux) ����
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
		// Data�� ����.
		assert(FALSE);
		goto FINAL;
	}

	// Inode
	pstData->nInode = pstIndexEntryNode->nInode;

	// ���� ũ��
	pstData->nFileSize = pstIndexEntryNode->llDataSize;

	// �̸�
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
		// C:\PROGRA~1�� ������...
		pstData->bWin32Namespace = FALSE;
		break;
	default:
		pstData->bWin32Namespace = FALSE;
		assert(FALSE);
	}

	// Parent directory mft
	pstData->llParentDirMftRef = pstIndexEntryNode->llParentDirMftRef;

	// ������ ä���.
	pstData->dwFileAttrib = pstIndexEntryNode->dwFileAttrib;
	pstData->llCreationTime = pstIndexEntryNode->llCreationTime;
	pstData->llLastAccessTime = pstIndexEntryNode->llLastAccessTime;
	pstData->llLastDataChangeTime = pstIndexEntryNode->llLastDataChangeTime;

	// ������� �Դٸ� ����
	bRtnValue = TRUE;

FINAL:

	if (FALSE == bRtnValue)
	{
		// �Լ� ����
		if (NULL != pstData)
		{
			memset(pstData, 0, sizeof(*pstData));
		}
	}

	return bRtnValue;
}

// FindFirstInode�� ������ Object�� �����Ѵ�.
BOOL CNTFSHelper::FindCloseInode(IN LPST_FINDFIRSTINODE pstFind)
{
	BOOL			bRtnValue	= TRUE;	// �⺻�� ����
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
		// Stack�� ����
		for (;;)
		{
			if (TRUE == IsStackEmptry(pstFind->pstStack))
			{
				// Stack�� ������� Exit
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
				// Stack�� Index Entry Array�� �ִٸ�,
				// �����Ѵ�.
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

// Inode Traverse�� �ٸ��� File Traverse��,
//		- $MFT�� ���� metadata file
//		- PROGRA~1�� ���� Dos namespace
// �� ��ŵ�Ѵ�.
// �� �Լ��� TRUE�� ���޵Ǹ�, File Traverse�� �����ϴ� �׸��̰�,
// FALSE�� ���ϵǸ� Skip�ϵ��� �Ѵ�.
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

	// ������� �Դٸ� ����
	bRtnValue = TRUE;

FINAL:
	return bRtnValue;
}

// Inode ������ ������ WIN32_FIND_DATA ����ü ���� ���Ѵ�.
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
	// ��ȯ
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

	// ������� �Դٸ� ����
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

// Win32 API�� CreateFile�� emulation�Ѵ�. (Read-only)
// NOT_USED�� ��õ� argument�� ���ο��� ������� ������ �����ϱ� �ٶ�.
// dwDesiredAccess�� GENERIC_WRITE�� �������� �ʴ´�. (ERROR_NOT_SUPPORTED)
// dwCreationDisposition�� OPEN_EXISTING �̿��� ���� �������� �ʴ´�. (ERROR_NOT_SUPPORTED)
CNTFSHelper::NTFS_HANDLE CNTFSHelper::CreateFile(IN LPCWSTR lpFileName, IN DWORD dwDesiredAccess, NOT_USED IN DWORD dwShareMode, NOT_USED IN LPSECURITY_ATTRIBUTES lpSecurityAttributes, IN DWORD dwCreationDisposition, NOT_USED IN DWORD dwFlagsAndAttributes, NOT_USED IN HANDLE hTemplateFile)
{
	// File�� Open�ϴ� ����
	// $DATA �Ӽ� ���� ����Ѵٴ� ���̴�.
	return CreateFileByAttrib(lpFileName, dwDesiredAccess, dwShareMode, lpSecurityAttributes, dwCreationDisposition, dwFlagsAndAttributes, hTemplateFile, TYPE_ATTRIB_DATA, NULL);
}

CNTFSHelper::NTFS_HANDLE WINAPI CNTFSHelper::CreateFileIo(IN LPCWSTR lpFileName, IN DWORD dwDesiredAccess, NOT_USED IN DWORD dwShareMode, NOT_USED IN LPSECURITY_ATTRIBUTES lpSecurityAttributes, IN DWORD dwCreationDisposition, NOT_USED IN DWORD dwFlagsAndAttributes, NOT_USED IN HANDLE hTemplateFile, OPTIONAL IN LPST_IO_FUNCTION pstIoFunction)
{
	// �ܺ� I/O �Լ� ����
	return CreateFileByAttrib(lpFileName, dwDesiredAccess, dwShareMode, lpSecurityAttributes, dwCreationDisposition, dwFlagsAndAttributes, hTemplateFile, TYPE_ATTRIB_DATA, pstIoFunction);
}

// �������� CreateFile�� Main�Լ��̴�.
// File�� �������� Attribute�� �ִµ�,
// �ش� Attribute�� ���� �дµ� ���ȴ�.
// �Ϲ����� File ������ $DATA�̴�. (TYPE_ATTRIB_DATA)
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
		// ���� ������ �������� �ʴ´�.
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

	// ����, ��θ� ���Ѵ�.
	StringCchCopyA(szVolume, 7, "\\\\.\\_:");	// _ ���ڴ� ������ a~z ������ ������ ġȯ�� ����
	dwErrorCode = GetFindFirstFilePath(lpFileName, lpszPath, MAX_PATH_NTFS, &szVolume[4], &bFindSubDir);
	if (TRUE == bFindSubDir)
	{
		// CreateFile���� FindFirstFileó�� *�� *.*���� ������ �ʴ´�.
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

	// CreateFile ���� Handle�̴�.
	pstHandle->nType = TYPE_WIN32_HANDLE_CREATEFILE;

	// Lock�� ��������.
	pstHandle->pLock = CREATE_LOCK();

	// Ntfs�� Open�Ѵ�.
	if (NULL == pstIoFunction)
	{
		pstHandle->pstNtfs = OpenNtfs(szVolume);
	}
	else
	{
		// �ܺ� I/O ����
		pstHandle->pstNtfs = OpenNtfsIo(szVolume, pstIoFunction);
	}

	if (NULL == pstHandle->pstNtfs)
	{
		dwErrorCode = ERROR_OPEN_FAILED;
		assert(FALSE);
		goto FINAL;
	}

	// Path�� inode�� ã�´�.
	// �ð��� ���� �ɸ���.
	dwErrorCode = FindInodeFromPath(pstHandle->pstNtfs, lpszPath, &pstHandle->stCreateFile.nInode, pstInodeData);
	if (ERROR_SUCCESS != dwErrorCode)
	{
		if (ERROR_FILE_NOT_FOUND != dwErrorCode)
		{
			assert(FALSE);
		}
		goto FINAL;
	}

	// $DATA�� ����.
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

		// ���� ������ �̷����ٸ�,...
		// [TBD]
		// ���� compressed�� �������� �ʴ´�.
		if (0 != pstAttribAlloc->pstAttrib->stAttr.stNonResident.wCompressionSize)
		{
			dwErrorCode = ERROR_NOT_SUPPORTED;
			assert(FALSE);
			goto FINAL;
		}

		// Runlist�� ���� ���� �����´�.
		pstHandle->stCreateFile.bResident = FALSE;

		// Attribute ���� ũ�⸦ �����´�.
		pstHandle->stCreateFile.nFileSize = pstAttribAlloc->pstAttrib->stAttr.stNonResident.llDataSize;
	}
	else
	{
		// Resident.
		pstHandle->stCreateFile.bResident = TRUE;

		// Attribute ���� ũ�⸦ �����´�.
		pstHandle->stCreateFile.nFileSize = pstAttribAlloc->pstAttrib->stAttr.stResident.dwValueLength;
	}

	if (TRUE == pstHandle->stCreateFile.bResident)
	{
		// Resident data�̶��,...
		// �׸� �뷮�� ũ�� ���� ���̱� ������,
		// �׳� ���� ������ ������ ��������.

		// �켱 ���� ũ�⸦ �����Ѵ�.
		pstHandle->stCreateFile.nBufResidentDataSize = pstAttribAlloc->pstAttrib->stAttr.stResident.dwValueLength;

		// [TBD]
		// pstHandle->stCreateFile.nBufResidentDataSize
		// �� ���̰� File record ������ ������� ����

		// ���۸� �Ҵ��Ѵ�.
		pstHandle->stCreateFile.pBufResidentData = (UBYTE1*)malloc(pstHandle->stCreateFile.nBufResidentDataSize);
		if (NULL == pstHandle->stCreateFile.pBufResidentData)
		{
			dwErrorCode = ERROR_FILE_CORRUPT;
			assert(FALSE);
			goto FINAL;
		}
		memset(pstHandle->stCreateFile.pBufResidentData, 0, pstHandle->stCreateFile.nBufResidentDataSize);

		// Resident data�� �����´�.
		memcpy(pstHandle->stCreateFile.pBufResidentData, (UBYTE1*)pstAttribAlloc->pstAttrib + pstAttribAlloc->pstAttrib->stAttr.stResident.wValueOffset, pstHandle->stCreateFile.nBufResidentDataSize);
	}
	else
	{
		// non-Resident data�̶��,...
		// Runlist�� ��������.
		pstHandle->stCreateFile.pstRunList = GetRunListAlloc(GetRunListByte(pstAttribAlloc->pstAttrib), &pstHandle->stCreateFile.nCountRunList);
		if (NULL == pstHandle->stCreateFile.pstRunList)
		{
			dwErrorCode = ERROR_FILE_CORRUPT;
			assert(FALSE);
			goto FINAL;
		}

		// RunList�� ���� Vcn ������ ���Ѵ�.
		for (i=0; i<pstHandle->stCreateFile.nCountRunList; i++)
		{
			pstHandle->stCreateFile.nCountVcn += pstHandle->stCreateFile.pstRunList[i].llLength;
		}

		// Attribute���� ǥ��� Vcn ������ ���Ѵ�.
		if (pstAttribAlloc->pstAttrib->stAttr.stNonResident.llEndVCN - pstAttribAlloc->pstAttrib->stAttr.stNonResident.llStartVCN + 1 != pstHandle->stCreateFile.nCountVcn)
		{
			dwErrorCode = ERROR_FILE_CORRUPT;
			assert(FALSE);
			goto FINAL;
		}
	}

	// ������� �Դٸ� ����
	// ��,
	// $DATA�� resident���,
	//  - buf ������ file data ����
	// $DATA�� non-resident���,
	//  - runlist ���� ����
	// �����̴�.

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
		// ����
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

		// ���� �߻�
		::SetLastError(dwErrorCode);
	}
	return hRtnValue;
}

// WIn32 API Emulation
// lpNumberOfBytesRead�� NULL�� ���޵Ǿ�� �ȵȴ�.
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
		// CreateFile�� ������� handle�� �ƴ� ���,...
		dwErrorCode = ERROR_INVALID_HANDLE;
		assert(FALSE);
		::SetLastError(dwErrorCode);
		return FALSE;
	}

	if (TRUE == pstHandle->stCreateFile.bErrorOccurred)
	{
		// �ѹ��̶� ������ �߻��ߴٸ�,....
		dwErrorCode = ERROR_FILE_INVALID;
		assert(FALSE);
		::SetLastError(dwErrorCode);
		return FALSE;
	}

	//////////////////////////////////////////////////////////////////////////
	// Lock
	LOCK(pstHandle->pLock);

	// ���� File ũ�⸦ �������,...
	if (pstHandle->stCreateFile.nFilePointer >= pstHandle->stCreateFile.nFileSize)
	{
		// ��, ������ ���ٴ� ���ε�,
		// TRUE�� �����ϰ�, 0byte�� �о��ٰ� �����ϴ���.
		bRtnValue = TRUE;
		*lpNumberOfBytesRead = 0;
		goto FINAL;
	}

	if (pstHandle->stCreateFile.nFilePointer + nNumberOfBytesToRead > pstHandle->stCreateFile.nFileSize)
	{
		// ���� ���� ũ�Ⱑ File ũ�⸦ ����� ���,...
		// ���� ũ�⸦ �����Ѵ�.
		nNumberOfBytesToRead = (DWORD)(pstHandle->stCreateFile.nFileSize - pstHandle->stCreateFile.nFilePointer);
	}

	if (TRUE == pstHandle->stCreateFile.bResident)
	{
		// resident�� ���,...

		// resident �޸𸮸� �����Ѵ�.
		memcpy(lpBuffer, &pstHandle->stCreateFile.pBufResidentData[pstHandle->stCreateFile.nFilePointer], nNumberOfBytesToRead);

		// ���� ũ�⸦ �����Ѵ�.
		*lpNumberOfBytesRead = nNumberOfBytesToRead;

		// file pointer�� �ڷ� �ű��.
		pstHandle->stCreateFile.nFilePointer += nNumberOfBytesToRead;

		// ����
		bRtnValue = TRUE;

		goto FINAL;
	}

	// non-Resident�� ���,...

	if (NULL == pstHandle->stCreateFile.pBufCluster)
	{
		// ����, buffer�� �������� �ʾҴ�.
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
		// �ش� file pointer ��ġ�� vcn�� ����ϱ�?
		nVcn = pstHandle->stCreateFile.nFilePointer / pstHandle->pstNtfs->stNtfsInfo.dwClusterSize;

		// vcn ��ġ����, offset�� ���ϱ�?
		nOffset = pstHandle->stCreateFile.nFilePointer % pstHandle->pstNtfs->stNtfsInfo.dwClusterSize;

		// �׷�, nVcn�� ���� Lcn���� ������.
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

		// ����, Lcn���� ��������,
		// Lcn + offset ���� ���� ����.
		if (FALSE == SeekLcn(pstHandle->pstNtfs, nLcn))
		{
			// Lcn ��ġ�� ����.
			dwErrorCode = ERROR_FILE_INVALID;
			assert(FALSE);
			goto FINAL;
		}

		if (pstHandle->pstNtfs->stNtfsInfo.dwClusterSize != ReadIo(pstHandle->pstNtfs->pstIo, pstHandle->stCreateFile.pBufCluster, pstHandle->pstNtfs->stNtfsInfo.dwClusterSize))
		{
			// �дµ� ������
			dwErrorCode = ERROR_FILE_INVALID;
			assert(FALSE);
			goto FINAL;
		}

		// �дµ� �����Ͽ���.
		if (nNumberOfBytesToRead - dwBufferPos > pstHandle->pstNtfs->stNtfsInfo.dwClusterSize - nOffset)
		{
			// ���� ���� Cluster ���� Ŭ��,...
			memcpy((UBYTE1*)lpBuffer + dwBufferPos, &pstHandle->stCreateFile.pBufCluster[nOffset], pstHandle->pstNtfs->stNtfsInfo.dwClusterSize - nOffset);

			// File Pointer�� �ڷ� �ű��.
			pstHandle->stCreateFile.nFilePointer += pstHandle->pstNtfs->stNtfsInfo.dwClusterSize - nOffset;

			// lpBuffer�� ��ġ�� �ڷ� �ű��.
			dwBufferPos += pstHandle->pstNtfs->stNtfsInfo.dwClusterSize - nOffset;

			continue;
		}

		// ���� ���� Cluster ���� ���� ��,...
		// Loop�� �������̴�.
		memcpy((UBYTE1*)lpBuffer + dwBufferPos, &pstHandle->stCreateFile.pBufCluster[nOffset], nNumberOfBytesToRead - dwBufferPos);

		// �о���.
		pstHandle->stCreateFile.nFilePointer += nNumberOfBytesToRead - dwBufferPos;

		break;
	}

	// ���� ũ��
	*lpNumberOfBytesRead = nNumberOfBytesToRead;

	// ������� �Դٸ� ����
	bRtnValue = TRUE;

FINAL:

	UNLOCK(pstHandle->pLock);
	// UnLock
	//////////////////////////////////////////////////////////////////////////

	if (FALSE == bRtnValue)
	{
		// ����
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
		// ���� assert ������ ����.
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

		// Handle �޸� �����Ѵ�.
		free(pstHandle);
		pstHandle = NULL;

		bRtnValue = TRUE;
	}

// ������ ����
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

	// CreateFile �� ����� ũ�⸦ ��������.
	//
	// [TBD]
	// Win32 API������ ��� �����ϴ°�?
	// ��, CreateFile �� ����� ũ���ΰ�?
	// GetFileSize�� ȣ���� ������ ũ���ΰ�?
	nLargeInt.QuadPart = pstHandle->stCreateFile.nFileSize;

	dwRtnValue = nLargeInt.LowPart;

	if (NULL != lpFileSizeHigh)
	{
		*lpFileSizeHigh = nLargeInt.HighPart;
	}

// ������ ����
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
		// ���� Win32 API����,
		// File ũ�⺸�� ū ��ġ�� �䱸�ϴ� ��쿡��
		// ������ �����Ͽ���.
		// do nothing...
		// not alert
	}

	if (TRUE == bBack)
	{
		nLargeInt.QuadPart = -nLargeInt.QuadPart;
	}

	// File Pointer�� �����Ѵ�.
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

	// ������� �Դٸ� ����
	pstHandle->stCreateFile.nFilePointer = nFilePointer;

	// ���ϰ� ����.
	// unsigned�� ��ȯ
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
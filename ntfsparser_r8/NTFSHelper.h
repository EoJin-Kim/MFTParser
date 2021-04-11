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

#pragma once

#include <Windows.h>
#include <stdio.h>

#define NTFS_PARSER_REVISION	8

#define INFOLOG(fmt, ...) fprintf(stdout, fmt, __VA_ARGS__);fprintf(stdout, "\r\n");
#define ERRORLOG(fmt,...) fprintf(stderr, fmt, __VA_ARGS__);fprintf(stderr, "\r\n");

// Win32 API Handle���� Invalid Handle ��
#define INVALID_HANDLE_VALUE_NTFS		(CNTFSHelper::NTFS_HANDLE)INVALID_HANDLE_VALUE

#define NOT_USED

class CNTFSHelper
{
public:
	CNTFSHelper(void);
	~CNTFSHelper(void);

	typedef unsigned char		UBYTE1;
	typedef unsigned __int16	UBYTE2;
	typedef unsigned __int32	UBYTE4;
	typedef unsigned __int64	UBYTE8;
	typedef unsigned __int64	_OFF64_T;
	typedef signed   __int64	SBYTE8;

	// http://www.ceng.metu.edu.tr/~isikligil/ceng334/fsa.pdf
	// �� 205page ����
	typedef UBYTE4	TYPE_ATTRIB;
	#define			TYPE_ATTRIB_UNKNOWN					0
	#define			TYPE_ATTRIB_STANDARD_INFORMATION	16
	#define			TYPE_ATTRIB_ATTRIBUTE_LIST			32
	#define			TYPE_ATTRIB_FILE_NAME				48
	#define			TYPE_ATTRIB_VOLUME_VERSION			64		// ntfs 1.2 only
	#define			TYPE_ATTRIB_OBJECT_ID				64		// ntfs 3.0 over
	#define			TYPE_ATTRIB_SECURITY_DESCRIPTOR		80
	#define			TYPE_ATTRIB_VOLUME_NAME				96
	#define			TYPE_ATTRIB_VOLUME_INFORMATION		112
	#define			TYPE_ATTRIB_DATA					128
	#define			TYPE_ATTRIB_INDEX_ROOT				144
	#define			TYPE_ATTRIB_INDEX_ALLOCATION		160
	#define			TYPE_ATTRIB_BITMAP					176
	#define			TYPE_ATTRIB_SYMBOLIC_LINK			192		// ntfs 1.2 only
	#define			TYPE_ATTRIB_REPARSE_POINT			192		// ntfs 3.0 over
	#define			TYPE_ATTRIB_EA_INFORMATION			208		// os/2
	#define			TYPE_ATTRIB_EA						224		// os/2
	#define			TYPE_LOGGED_UTILITY_STREAM			256		// ntfs 3.0 over

	typedef UBYTE1	TYPE_RUNTYPE;
	#define			TYPE_RUNTYPE_UNKNOWN	0
	#define			TYPE_RUNTYPE_NORMAL		1
	#define			TYPE_RUNTYPE_SPARSE		2

	// http://inform.pucp.edu.pe/~inf232/Ntfs/ntfs_doc_v0.5/files/index.html
	// ����
	typedef UBYTE8	TYPE_INODE;
	#define			TYPE_INODE_MFT		0
	#define			TYPE_INODE_MFTMIRR	1
	#define			TYPE_INODE_LOGFILE	2
	#define			TYPE_INODE_VOLUME	3
	#define			TYPE_INODE_ATTRDEF	4
	#define			TYPE_INODE_DOT		5
	#define			TYPE_INODE_BITMAP	6
	#define			TYPE_INODE_BOOT		7
	#define			TYPE_INODE_BADCLUS	8
	#define			TYPE_INODE_QUOTA	9
	#define			TYPE_INODE_SECURE	9
	#define			TYPE_INODE_UPCASE	10
	#define			TYPE_INODE_EXTEND	11
	#define			TYEP_INODE_DEFAULT	11	// �ǹ� ����. ��������� �⺻ inode��(Metadata file)
	#define			TYPE_INODE_FILE		24	// ���ĺ��� Normal file
	#define			TYPE_INODE_DIR		24

	// MAX_PATH �� ����
	#define MAX_PATH_NTFS				260

	// http://inform.pucp.edu.pe/~inf232/Ntfs/ntfs_doc_v0.5/concepts/filename_namespace.html
	// OR ���� �ʴ´�.
	#define	NAMESPACE_POSIX		0x00
	#define	NAMESPACE_WIN32		0x01
	#define	NAMESPACE_DOS		0x02
	#define NAMESPACE_WIN32DOS	0x03

	// http://inform.pucp.edu.pe/~inf232/Ntfs/ntfs_doc_v0.5/concepts/attribute_header.html
	// �� Flag
	#define ATTRIB_RECORD_FLAG_COMPRESSED	0x0001
	#define ATTRIB_RECORD_FLAG_ENCRYPTED	0x4000
	#define ATTRIB_RECORD_FLAG_SPARSE		0x8000

	// http://inform.pucp.edu.pe/~inf232/Ntfs/ntfs_doc_v0.5/attributes/file_name.html
	// File Attribute
	#define	ATTRIB_FILENAME_FLAG_READONLY	0x00000001
	#define	ATTRIB_FILENAME_FLAG_HIDDEN		0x00000002
	#define	ATTRIB_FILENAME_FLAG_SYSTEM		0x00000004
	#define	ATTRIB_FILENAME_FLAG_ARCHIVE	0x00000020
	#define	ATTRIB_FILENAME_FLAG_DEVICE		0x00000040
	#define	ATTRIB_FILENAME_FLAG_NORMAL		0x00000080
	#define	ATTRIB_FILENAME_FLAG_TEMP		0x00000100
	#define	ATTRIB_FILENAME_FLAG_SPARSE		0x00000200
	#define	ATTRIB_FILENAME_FLAG_REPARSE	0x00000400
	#define	ATTRIB_FILENAME_FLAG_COMPRESSED	0x00000800
	#define	ATTRIB_FILENAME_FLAG_OFFLINE	0x00001000
	#define	ATTRIB_FILENAME_FLAG_NCI		0x00002000
	#define	ATTRIB_FILENAME_FLAG_ENCRYPTED	0x00004000
	#define	ATTRIB_FILENAME_FLAG_DIRECTORY	0x10000000
	#define	ATTRIB_FILENAME_FLAG_INDEXVIEW	0x20000000

	#pragma pack(push, 1)	// for _WIN32. ����ü alignment ����

	// NTFS Boot Sector ������ ������ ����ü
	// BIOS Parameter Block
	typedef struct tagST_NTFS_BPB
	{
		UBYTE2	wBytePerSector;			// Bytes per sector
		UBYTE1	cSectorPerCluster;		// Sector per cluster
		UBYTE1	zero1[7];				// Reserved sector, fats, ... 
		UBYTE1	cMedia;					// Media descriptor
		UBYTE2	zero2;
		UBYTE2	wSectorPerTrack;		// Sectors per tack (for boot)
		UBYTE2	wHeadNum;				// Head count
		UBYTE4	dwHiddenSector;			// Hidden Sector (for boot)
		UBYTE4	zero3;
		UBYTE1	cPhysicalDrive;			// Drive. 0x00 : Floopy / 0x80 : HDD
		UBYTE1	zero4;
		UBYTE1	cExtBootSig;			// Extended boot signature : 0x80
		UBYTE1	zero5;
		UBYTE8	llSectorNum;			// Total sector count
		UBYTE8	llMftLcn;				// MFT location
		UBYTE8	llMftMirrLcn;			// Mirror MFT location
		UBYTE1	cClusterPerMFT;			// Cluster per MFT Record
		UBYTE1	zero6[3];
		UBYTE1	cClusterPerIndex;		// Cluster per File Record (INDX)
		UBYTE1	zero7[3];
		UBYTE8	llVolumeSerial;			// Volume serial number
		UBYTE4	dwChecksum;				// Boot sector checksum
	} ST_NTFS_BPB, *LPST_NTFS_BPB;

	typedef struct tagST_BOOT_SECTOR
	{
		UBYTE1		szJumpCode[3];		// NTFS JumpCode
		UBYTE1		szOemId[8];			// OEM ID
		ST_NTFS_BPB	stBpb;				// BIOS Parameter Block
		UBYTE1		cBootStrapCode[426];// Boot Strap code
		UBYTE2		wMagic;				// Magic Code (0xaa55, little endian)
	} ST_BOOT_SECTOR, *LPST_BOOT_SECTOR;

	typedef struct tagST_MFT_HEADER
	{
		UBYTE1		szFileSignature[4];
		UBYTE2		wFixupArrayOffset;
		UBYTE2		wFixupArrayCount;
		UBYTE8		llLsn;
		UBYTE2		wSeqNum;
		UBYTE2		wLinkCount;
		UBYTE2		wAttribOffset;
		UBYTE2		wFlag;
		UBYTE4		dwUsedSize;
		UBYTE4		dwAllocatedSize;
		UBYTE8		llBaseMftRecord;
		UBYTE2		wNextAttribId;
		UBYTE2		wReserved;
		UBYTE4		dwMftRecordNumber;
	} ST_MFT_HEADER, *LPST_MFT_HEADER;

	// MFT Attribute Record. (MFT Entry ��� �Ҹ��⵵ ��)
	// (http://msdn.microsoft.com/en-us/library/bb470039(v=vs.85).aspx ����)
	typedef struct tagST_ATTRIB_RECORD
	{
		TYPE_ATTRIB	dwType;
		UBYTE4		dwLength;
		UBYTE1		cNonResident;
		UBYTE1		cNameLength;
		UBYTE2		wNameOffset;
		UBYTE2		wFlag;	// ATTRIB_RECORD_FLAG_*
		UBYTE2		wId;
		union
		{
				struct
				{
					UBYTE4	dwValueLength;
					UBYTE2	wValueOffset;
					UBYTE1	cResidentFlag;
					UBYTE1	cReserved;
				} stResident;

				struct  
				{
					UBYTE8	llStartVCN;
					UBYTE8	llEndVCN;
					UBYTE2	wOffsetDataRun;
					UBYTE2	wCompressionSize;
					UBYTE1	szReserved[4];
					UBYTE8	llAllocSize;
					UBYTE8	llDataSize;
					UBYTE8	llInitSize;
				} stNonResident;
		} stAttr;
	} ST_ATTRIB_RECORD, *LPST_ATTRIB_RECORD;

	// $ATTRIBUTE_LIST�� ����
	// http://www.alex-ionescu.com/NTFS.pdf �� 4.4.2 Structure ����
	// http://inform.pucp.edu.pe/~inf232/Ntfs/ntfs_doc_v0.5/attributes/attribute_list.html ����
	typedef struct tagST_ATTRIBUTE_LIST
	{
		UBYTE4 nType;
		UBYTE2 nRecordLength;
		UBYTE1 nNameLength;
		UBYTE1 nNameOffset;
		UBYTE8 nStartVcn;
		UBYTE8 nReference;
		UBYTE2 nAttributeId;
	} ST_ATTRIBUTE_LIST, *LPST_ATTRIBUTE_LIST;

	typedef struct tagST_ATTRIB_STANDARD_INFORMATION
	{
		UBYTE8	llCreationTime;
		UBYTE8	llLastDataChangeTime;
		UBYTE8	llLastMftChangeTime;
		UBYTE8	llLastAccessTime;
		UBYTE4	dwFileAttrib;
		UBYTE4	dwMaxVersion;
		UBYTE4	dwVersion;
		UBYTE4	dwClassId;
		UBYTE4	dwOwnerId;
		UBYTE4	dwSecurityId;
		UBYTE8	llQuotaCharged;
		UBYTE8	llUSN;	// Update Sequence Number
	} ST_ATTRIB_STANDARD_INFORMATION, *LPST_ATTRIB_STANDARD_INFORMATION;

	#pragma warning(push)
	#pragma warning(disable:4200)

	typedef struct tagST_ATTRIB_FILE_NAME
	{
		UBYTE8	llParentDirMftRef;
		UBYTE8	llCreationTime;
		UBYTE8	llLastDataChangeTime;
		UBYTE8	llLastMftChangeTime;
		UBYTE8	llLastAccessTime;
		UBYTE8	llAllocSize;
		UBYTE8	llDataSize;
		UBYTE4	dwFileAttrib;
		union
		{
			struct
			{
				UBYTE2	wPackExtAttribSize;
				UBYTE2	wReserved;
			} stExtAttrib;
			UBYTE4 dwReparsePoint;
		};
		UBYTE1	cFileNameLength;
		UBYTE1	cFileNameSpace;
		WCHAR	lpszFileNameW[0];	// in Visual C++, warning 4200
	} ST_ATTRIB_FILE_NAME, *LPST_ATTRIB_FILE_NAME;

	typedef struct tagST_BYTE8
	{
		UBYTE1	val[8];
	} ST_BYTE8, *LPST_BYTE8;

	#pragma warning(pop)

	#pragma pack(pop)

	// RunList ���� Node
	typedef struct tagST_RUNLIST
	{
		TYPE_RUNTYPE	cType;
		UBYTE8			llLCN;
		UBYTE8			llLength;
	} ST_RUNLIST, *LPST_RUNLIST;

	// NTFS meta ���� (NTFSHelper���� Context ���������� IN/OUT���� ����ϴ� ��)
	typedef struct tagST_NTFS_INFO
	{
		UBYTE4		dwClusterSize;
		UBYTE4		dwMftSize;
		UBYTE4		dwMftSectorCount;	// Mft Header�� �����ϴ� Sector ����
		UBYTE8		llMftSectorPos;
		UBYTE2		wSectorSize;
		UBYTE4		dwReadBufSizeForMft;
		UBYTE8		nTotalClusterCount;
	} ST_NTFS_INFO, *LPST_NTFS_INFO;

	typedef struct tagST_FILENAME
	{
		WCHAR szFileName[MAX_PATH + 1];		// Ȥ�ó� �� Buffer overrun ������ ���� +1
	} ST_FILENAME, *LPST_FILENAME;

	// Find Attrib Info
	typedef struct tagST_FIND_ATTRIB
	{
		LPST_ATTRIB_RECORD	pstAttrib;		// Attrib record ��. (���� Element)
		LPST_MFT_HEADER		pstMftHeader;	// �ش� Mft Header ��. (ū ����)
		UBYTE1*				pBuf;			// pstAttrib�� �����ϱ� ���� ����.
	} ST_FIND_ATTRIB, *LPST_FIND_ATTRIB;

	// Bitmap ����
	typedef struct tagST_BITMAP_INFO
	{
		UBYTE1*		pBitmap;
		_OFF64_T	nBitmapBufSize;
		BOOL		bAlloc;
	} ST_BITMAP_INFO, *LPST_BITMAP_INFO;

	// Inode ����
	typedef struct tagST_INODEDATA
	{
		TYPE_INODE	nInode;					// Inode ��
 		BOOL		bWin32Namespace;		// Win32 Namespace ����. ex) C:\PROGRA~1 ���� ��� Dos namespace�̹Ƿ�, Win32 Namespace�� �ƴϴ�.

		UBYTE4		dwFileAttrib;			// ATTRIB_FILENAME_FLAG_* �Ӽ���. Directory�� ���� ����, ��Ÿ �Ӽ����� �� �� �ְ� �Ѵ�.

		UBYTE8		llCreationTime;			// �ð� ��
		UBYTE8		llLastAccessTime;
		UBYTE8		llLastDataChangeTime;

		UBYTE8		nFileSize;
		UBYTE8		llParentDirMftRef;
	} ST_INODEDATA, *LPST_INODEDATA;

	//////////////////////////////////////////////////////////////////////////
	// I/O ����
	typedef	INT	TYPE_IO;
	#define		TYPE_IO_UNKNOWN			0
	#define		TYPE_IO_LOCALDISK		1
	#define		TYPE_IO_USERDEFINE		(0xFFFFFFFF)

	// User define I/O �Լ����� ���� calling convention.
	// Windows������ �ش�.
	// Linux������???
	#ifdef _WIN32
		#define IO_CDECL	__cdecl
	#else
		#define IO_CDECL
	#endif

	// �ܺο��� ����� I/O �Լ����� ���� File Descriptor�� HANDLE ���� �뵵�� Ÿ��
	// LPFN_OpenIo���� new�� malloc�� ���� heap ������ ������ �����ϵ��� �Ѵ�.
	// LPFN_CloseIo���� delete�� free�� ���� clean-up�Ѵ�.
	typedef LPVOID IO_LPVOID;

	// �ܺο��� ����� I/O �Լ��� Prototype
	// ����� �Լ��� __cdecl ������� ������
	// pUserIoContext�� OpenNtfs���� ������ ��(ST_IO_FUNCTION::pUserIoContext)�� ���´�.
	typedef IO_LPVOID	(IO_CDECL *LPFN_OpenIo) (IN LPCSTR lpszPath, OPTIONAL IN LPVOID pUserIoContext);
	typedef BOOL		(IO_CDECL *LPFN_CloseIo)(IN IO_LPVOID pIoHandle);
	typedef BOOL		(IO_CDECL *LPFN_SeekIo) (IN IO_LPVOID pIoHandle, IN UBYTE8 nPos, IN INT nOrigin);
	typedef INT			(IO_CDECL *LPFN_ReadIo) (IN IO_LPVOID pIoHandle, OUT LPVOID pBuffer, IN unsigned int count);

	// I/O �Լ� ����
	typedef struct tagST_IO_FUNCTION
	{
		DWORD			dwCbSize;
		LPFN_OpenIo		pfnOpenIo;
		LPFN_CloseIo	pfnCloseIo;
		LPFN_SeekIo		pfnSeekIo;
		LPFN_ReadIo		pfnReadIo;
		LPVOID			pUserIoContext;
	} ST_IO_FUNCTION, *LPST_IO_FUNCTION;

	/*
		�Ʒ� ������ �������� ���ߵ� �Լ��� �����ϸ�,
		I/O �Լ��� ���� ������ �� �ִ�.

		�Լ� ���� : CNTFSHelper::IO_LPVOID __cdecl MyOpenIo(IN LPCSTR lpszPath, OPTIONAL IN LPVOID pUserIoContext)
			; ���� : ���ǵ� �Լ����� new �� malloc�� ���� heap ������ ������
			         NULL�� ��� ���з� �����Ѵ�.

			; lpszPath : \\.\C:�� ���� ��ΰ� ����.
			; pUserIoContext : OpenNtfsIo(...)���� ������ ��(ST_IO_FUNCTION::pUserIoContext)�� ���´�.
			                   �ٸ� I/O �Լ��� (LPFN_CloseIo, LPFN_SeekIo, ...)�� �����ؾ� �Ѵٸ�,
							   �� �Լ��� ���ϰ��� �ش� ���� �����Ͽ� �����Ͻÿ�.
							   ��, ���� ���,
							   typedef struct tagST_MY_HANDLE
							   {
									INT nFd;
									...
									LPVOID pUserIoContext;
							   } ST_MY_HANDLE;
							   �� ���� ���Խ�Ų��. �׸���, malloc(sizeof(ST_MY_HANDLE) ���� �����Ѵ�.

		�Լ� ���� : BOOL __cdecl MyCloseIo(IN CNTFSHelper::IO_LPVOID pIoHandle)
			; ���� : ���� TRUE, ���� FALSE

			; pIoHandle : LPFN_OpenIo ���ϰ�. delete�� free������ �޸� ������ �ʿ��� ���̴�.

		�Լ� ���� : BOOL __cdecl MySeekIo(IN CNTFSHelper::IO_LPVOID pIoHandle, IN CNTFSHelper::UBYTE8 nPos, IN INT nOrigin)
			; ���� : ���� TRUE, ���� FALSE (�Ϲ����� C�� seek�� ���ϰ� ü�谡 �ٸ��� ����)

			; nPos : Seek�� ��ġ. Byte ����
			; nOrigin : SEEK_SET / SEEK_CUR / SEEK_END

		�Լ� ���� : INT __cdecl MyReadIo(IN CNTFSHelper::IO_LPVOID pIoHandle, OUT LPVOID pBuffer, IN unsigned int count)
			; ���� : ���� ����Ʈ ��. ���н� -1�� ����

			; ���� argument�� msdn�� _read �Լ� ����

		�� �Լ����� �����ѵ�,

		OpenNtfsIoDefine("\\.\C:", MyOpenIo, MyCloseIo, MySeekIo, MyReadIo, this)

		�� ���� ȣ���� �� �ִ�.
	*/

	// I/O ����
	typedef struct tagST_IO
	{
		TYPE_IO			nType;
		INT				nFd;	// disk file descriptor

		// �ܺο��� ����� I/O �Լ�
		ST_IO_FUNCTION	stIoFunction;

		// �ܺο��� ����� I/O �Լ����� ���� context��
		IO_LPVOID		pUserIoHandle;
	} ST_IO, *LPST_IO;

	//////////////////////////////////////////////////////////////////////////
	// Inode Cache

	// Cache callback �Լ����� ���� calling convention.
#ifdef _WIN32
#define INODE_CACHE_CDECL	__cdecl
#else
#define INODE_CACHE_CDECL
#endif

	typedef LPVOID HANDLE_INODE_CACHE;

	typedef HANDLE_INODE_CACHE (INODE_CACHE_CDECL *LPFN_CreateInodeCache) (OPTIONAL IN LPVOID pUserContext);
	typedef BOOL (INODE_CACHE_CDECL *LPFN_GetCache)(IN HANDLE_INODE_CACHE hHandle, IN TYPE_INODE nStartInode, IN LPCWSTR lpszNameUpperCase, OUT PBOOL pbFound, OUT LPST_INODEDATA pstInodeData, OPTIONAL IN LPVOID pUserContext);
	typedef BOOL (INODE_CACHE_CDECL *LPFN_SetCache)(IN HANDLE_INODE_CACHE hHandle, IN TYPE_INODE nStartInode, IN LPCWSTR lpszNameUpperCase, IN LPST_INODEDATA pstInodeData, OPTIONAL IN LPVOID pUserContext);
	typedef VOID (INODE_CACHE_CDECL *LPFN_DestroyCache)(IN HANDLE_INODE_CACHE hHandle, OPTIONAL IN LPVOID pUserContext);

	/*
	�Ʒ� 4���� �Լ��� �����Ͽ�, SetInodeCache�� �����ϸ�,
	Inode���� ������ �ʿ��Ҷ� ������ �Լ��� ȣ���Ѵ�.

	HANDLE_INODE_CACHE MyCreateInodeCache(OPTIONAL IN LPVOID pUserContext);
	BOOL MyGetCache(IN HANDLE_INODE_CACHE hHandle, IN TYPE_INODE nStartInode, IN LPCWSTR lpszNameUpperCase, OUT PBOOL pbFound, OUT LPST_INODEDATA pstInodeData, OPTIONAL IN LPVOID pUserContext);
	BOOL MySetCache(IN HANDLE_INODE_CACHE hHandle, IN TYPE_INODE nStartInode, IN LPCWSTR lpszNameUpperCase, IN LPST_INODEDATA pstInodeData, OPTIONAL IN LPVOID pUserContext);
	VOID MyDestroyCache(IN HANDLE_INODE_CACHE hHandle, OPTIONAL IN LPVOID pUserContext);

	nStartInode���� lpszNameUpperCase���� ������, pstInodeData ���� cache�Ͽ� �����ϰ�, ������ �ָ� �ȴ�.
	(���� : lpszNameUpperCase�� C:\component\component\...�� ���� �ϳ��� component ������ �빮�ڷ� �̷��� ���ڿ� ���̸�,
	        nStartInode���� �θ� ���, C:\a\b\c\d.exe�� ���, C:\a\b\c �� inode�� �ǹ��Ѵ�.)

	HANDLE_INODE_CACHE�� ���ο��� ���� ������ handle ��(LPVOID)�̴�.
	�ش� ���� ������, ������ GetCache/SetCache�� ������ ���´�.

	pUserContext�� SetInodeCache�� ��Ͻ��� ST_INODE_CACHE::pUserContext ���� ȣ��Ǿ� ���´�. �ʿ���ٸ� NULL �����ϴ�.

	CreateInodeCache ȣ�� ������ SetInodeCache ȣ�� �����̴�.
	
	����, �Ʒ��� Win32 API Emulation���� ����Ѵٸ�,
	FindFirstFile�� HANDLE �������� ȣ��ȴ�.
	��, ���� �ڵ忡�� �� 3���� FindFirstFile�� ȣ��Ǿ��ٸ�,
	CreateInodeCache�� �� 3��, ȣ���� �޴´�.
	*/

	typedef struct tagST_INODE_CACHE
	{
		LPFN_CreateInodeCache	pfnCreateInodeCache;
		LPFN_GetCache			pfnGetCache;
		LPFN_SetCache			pfnSetCache;
		LPFN_DestroyCache		pfnDestroyCache;
		LPVOID					pUserContext;
	} ST_INODE_CACHE, *LPST_INODE_CACHE;

	//////////////////////////////////////////////////////////////////////////
	// NTFS ����
	typedef struct tagST_NTFS
	{
		LPST_IO				pstIo;			// I/O
		ST_NTFS_BPB			stBpb;			// NTFS�� BPB ������ ���� ������.
		ST_NTFS_INFO		stNtfsInfo;		// CNTFSHelper���� ���Ǵ� IN/OUT ���� ������.
		LPST_BITMAP_INFO	pstBitmapInfo;	// Bitmap ������ ����.
		LPST_INODE_CACHE	pstInodeCache;	// Inode Cache callback ����
		HANDLE_INODE_CACHE	hInodeCache;	// Inode Cache handle ����
	} ST_NTFS, *LPST_NTFS;
	//
	//////////////////////////////////////////////////////////////////////////

	//////////////////////////////////////////////////////////////////////////
	//////////////////////////////////////////////////////////////////////////
	//// I. ntfs operation
	//////////////////////////////////////////////////////////////////////////
	//////////////////////////////////////////////////////////////////////////

public:
	//////////////////////////////////////////////////////////////////////////
	// basic operation

	// ntfs object open / close
	static LPST_NTFS		OpenNtfs(IN LPCSTR lpszPath);
	static LPST_NTFS		OpenNtfsIo(IN LPCSTR lpszPath, IN LPST_IO_FUNCTION pstIoFunction);
	static VOID				CloseNtfs(IN LPST_NTFS pstNtfs);

public:
	// seek operation
	static BOOL				SeekLcn(IN LPST_NTFS pstNtfs, IN UBYTE8 llLcn);
	static BOOL				SeekSector(IN LPST_NTFS pstNtfs, IN UBYTE8 llSectorNum);
	static BOOL				SeekByte(IN LPST_NTFS pstNtfs, IN UBYTE8 nByte);

	// runlist operation
	static UBYTE1*			GetRunListByte(IN LPST_ATTRIB_RECORD pstAttrib);
	static LPST_RUNLIST		GetRunListAlloc(IN	UBYTE1* pRunList, OUT PINT pnCount);
	static VOID				GetRunListFree(IN LPST_RUNLIST pRunList);

	// cache operation
	static BOOL				SetInodeCache(IN LPST_NTFS pstNtfs, IN LPFN_CreateInodeCache pfnCreateInodeCache, IN LPFN_GetCache pfnGetCache, IN LPFN_SetCache pfnSetCAche, IN LPFN_DestroyCache pfnDestroyCache, OPTIONAL IN LPVOID pUserContext);

	// etc
	static LPCSTR			GetRevision(OPTIONAL OUT LPINT pnRevision);

	//////////////////////////////////////////////////////////////////////////
	// basic i/o operation
protected:
	static LPST_IO			OpenIo(IN LPCSTR lpszPath);
	static LPST_IO			OpenUserIo(IN LPCSTR lpszPath, IN LPST_IO_FUNCTION pstIoFunction);
	static BOOL				CloseIo(IN LPST_IO pstIo);
	static BOOL				SeekIo(IN LPST_IO pstIo, IN UBYTE8 nPos, IN INT nOrigin);
	static INT				ReadIo(IN LPST_IO pstIo, OUT LPVOID pBuffer, IN unsigned int count);

	//////////////////////////////////////////////////////////////////////////
	// ntfs attribute Operation
public:
	static LPST_FIND_ATTRIB			FindAttribAlloc(IN LPST_NTFS pstNtfs, IN TYPE_INODE nTypeInode, IN TYPE_ATTRIB nTypeAttrib);
	static VOID						FindAttribFree(IN LPST_FIND_ATTRIB pstFindAttrib);

protected:
	static LPST_ATTRIB_RECORD		GetAttrib(IN LPST_NTFS pstNtfs, IN LPST_MFT_HEADER pstMftHeader, IN UBYTE1* pReadBuf, IN TYPE_ATTRIB nType, OUT PBOOL pbAttribList, OUT TYPE_INODE* pnInodeAttribList);
	static LPST_ATTRIB_RECORD		FindAttrib(IN LPST_NTFS pstNtfs, IN LPST_FIND_ATTRIB pstFindAttrib);
	static LPST_ATTRIB_FILE_NAME	GetAttrib_FileName(IN LPST_ATTRIB_RECORD pstAttrib);

	//////////////////////////////////////////////////////////////////////////
	// Internal Utility & Caclulation
protected:
	static ST_FILENAME		GetFileName(IN LPST_ATTRIB_FILE_NAME pstFileName);
	static SBYTE8			GetOffsetSBYTE8(IN UBYTE8 n8Byte, IN INT nOffset);
	static LPCWSTR			GetDefaultFileName(IN TYPE_INODE nInode);
	static _OFF64_T			GetDiskSize(IN LPST_NTFS pstNtfs);
	static BOOL				IsNormalFile(IN TYPE_INODE nInode);
	static BOOL				IsValidMftHeader(IN LPST_NTFS pstNtfs, IN UBYTE1* pReadBuf, IN LPST_MFT_HEADER pstMftHeader);
	static BOOL				RunFixup(IN LPST_NTFS pstNtfs, IN LPST_MFT_HEADER pstMftHeader, IN OUT UBYTE1* pBuf);
	static BOOL				GetBootSectorBPB(IN LPST_NTFS pstNtfs, OUT LPST_NTFS_BPB pstBpb);
	static inline UBYTE8	GetBytes(IN UBYTE8 n8Byte, IN INT nByte);
	static inline UBYTE8	GetUBYTE8(IN UBYTE1* pBuf);

	// Vcn --> Lcn Operation
protected:
	static BOOL				GetLcnFromMftVcn(IN LPST_NTFS pstNtfs, IN TYPE_INODE nInode, OUT UBYTE8* pnLcn, OUT PINT pnOffsetBytes);
	static BOOL				GetLcnFromVcn(IN LPST_RUNLIST pstRunList, IN INT nCountRunList, IN UBYTE8 nVcn, OPTIONAL IN UBYTE8 llStartVCN, OUT UBYTE8* pnLcn);

	//////////////////////////////////////////////////////////////////////////
	// Bitmap / Used or non-used Cluster Operation
public:
	static LPST_BITMAP_INFO	BitmapAlloc(IN LPST_NTFS pstNtfs);
	static VOID				BitmapFree(IN LPST_BITMAP_INFO pstBitmapInfo);
	static BOOL				IsClusterUsed(IN UBYTE8 nLcn, IN LPST_BITMAP_INFO pstBitmapInfo);
	static UBYTE8			GetUsedClusterCount(IN LPST_NTFS pstNtfs, IN LPST_BITMAP_INFO pstBitmapInfo, OPTIONAL OUT UBYTE8* pnMaxUsedClusterNum);
protected:
	static BOOL				SetBitmapInfo(IN UBYTE1* pBitmap, IN _OFF64_T nBitmapBufSize, OUT LPST_BITMAP_INFO pstBitmapInfo);
	static BOOL				CheckUsedCluster(IN UBYTE1* pBitmap, IN _OFF64_T nBufSize, IN UBYTE8 nLcn);

	//////////////////////////////////////////////////////////////////////////
	//////////////////////////////////////////////////////////////////////////
	//// II. inode Traverse Operation
	//////////////////////////////////////////////////////////////////////////
	//////////////////////////////////////////////////////////////////////////

protected:
	// http://inform.pucp.edu.pe/~inf232/Ntfs/ntfs_doc_v0.5/attributes/index_root.html
	#pragma pack(push, 1)
	typedef struct tagST_INDEX_ROOT
	{
		UBYTE4 nAttributeType;
		UBYTE4 nCollationRule;
		UBYTE4 nSize;
		UBYTE1 nClusterPerIndexRecord;
		UBYTE1 padding[3];
	} ST_INDEX_ROOT, *LPST_INDEX_ROOT;
	#pragma pack(pop)

	// http://inform.pucp.edu.pe/~inf232/Ntfs/ntfs_doc_v0.5/attributes/index_root.html
	#pragma pack(push, 1)
	typedef struct tagST_INDEX_HEADER
	{
		UBYTE4 nOffsetFirstIndexEntry;
		UBYTE4 nTotalSize;
		UBYTE4 nAllocatedSize;
		UBYTE1 nFlags;
		UBYTE1 padding[3];
	} ST_INDEX_HEADER, *LPST_INDEX_HEADER;
	#pragma pack(pop)

	// http://inform.pucp.edu.pe/~inf232/Ntfs/ntfs_doc_v0.5/attributes/index_allocation.html
	#pragma pack(push, 1)
	typedef struct tagST_INDEX_ENTRY
	{
		UBYTE8 nFileReference;
		UBYTE2 nLengthIndexEntry;
		UBYTE2 nLengthStream;
		UBYTE1 nFlag;		// INDEX_ENTRY_FLAG_*
		UBYTE1 padding[3];
		UBYTE1 stream[1];

		// nLengthIndexEntry ��ŭ �ڷ� ���� -8 byte ��ġ��
		// sub-node�� index allocation attribute�� ����.
	} ST_INDEX_ENTRY, *LPST_INDEX_ENTRY;
	#pragma pack(pop)

	// http://inform.pucp.edu.pe/~inf232/Ntfs/ntfs_doc_v0.5/concepts/index_header.html
	#pragma pack(push, 1)
	typedef struct tagST_STANDARD_INDEX_HEADER
	{
		UBYTE1 szMagic[4];	// INDX
		UBYTE2 nOffsetUpdateSequence;
		UBYTE2 nSizeUpdateSequence;
		UBYTE8 nLogFileSequenceNumber;
		UBYTE8 nVcn;
		UBYTE4 nOffsetEntry;
		UBYTE4 nSizeEntry;
		UBYTE4 nAllocSizeEntry;
		UBYTE1 bNotLeafNode;
		UBYTE1 padding[3];
		UBYTE2 nUpdateSequence;
		UBYTE2 arrUpdateSequence[1];
	} ST_STANDARD_INDEX_HEADER, *LPST_STANDARD_INDEX_HEADER;
	#pragma pack(pop)

	// ST_INDEX_ENTRY::nFlag
	// ���⿡�� Subnode��, Ntfs�� Directory�� �ǹ��ϴ� ���� �ƴ϶�,
	// b-tree ���� sub-node�� �ǹ��Ѵ�.
	#define INDEX_ENTRY_FLAG_SUBNODE	(0x1)
	#define INDEX_ENTRY_FLAG_LAST_ENTRY	(0x2)

public:
	// Index Entry�� Traverse�� �� Array�� ����� Node
	// ST_INDEX_ENTRY�� Disk�� ���� ���� ���� Cluster �޸𸮸�
	// Free�ϸ� �ֹ��� ������.
	// �׷���, �׿� �����ϰ�, ���� ���Ǵ� ������ ���� INDEX_ENTRY��
	// �Ʒ��� ���� �����.
	// Traverse�� �޸� ��뷮�� ��������
	typedef struct tagST_INDEX_ENTRY_NODE
	{
		BOOL		bHasSubNode;			// Subnode�� �ִ°�?
		BOOL		bLast;					// ������ �׸��ΰ�?
		BOOL		bHasData;				// Data�� �ִ°�?
		BOOL		bSubnodeTraversed;		// �ѹ� Traverse�Ǿ��°�?
		UBYTE8		nVcnSubnode;			// Subnode�� Vcn ��
		TYPE_INODE	nInode;					// Inode ��. �̸��� �ִ� ���(lpszFileNameAlloc != NULL) ��ȿ
		UBYTE8		llParentDirMftRef;		// ST_ATTRIB_FILE_NAME::llParentDirMftRef
		UBYTE8		llCreationTime;			// ST_ATTRIB_FILE_NAME::llCreationTime
		UBYTE8		llLastDataChangeTime;	// ST_ATTRIB_FILE_NAME::llLastDataChangeTime
		UBYTE8		llLastAccessTime;		// ST_ATTRIB_FILE_NAME::lLastAccessTime
		UBYTE8		llDataSize;				// ST_ATTRIB_FILE_NAME::llDataSize
		UBYTE1		cFileNameSpace;			// ST_ATTRIB_FILE_NAME::cFileNameSpace
		UBYTE4		dwFileAttrib;			// ST_ATTRIB_FILE_NAME::dwFileAttrib
		PWCHAR		lpszFileNameAlloc;		// ST_ATTRIB_FILE_NAME::lpszFileNameW ���� �Ҵ� (\0���� ����). NULL �̶�� �̸��� ���ٴ� ����.
	} ST_INDEX_ENTRY_NODE, *LPST_INDEX_ENTRY_NODE;

	typedef struct tagST_INDEX_ENTRY_ARRAY
	{
		INT						nItemCount;
		LPST_INDEX_ENTRY_NODE	pstArray;
		INT						nCurPos;
	} ST_INDEX_ENTRY_ARRAY, *LPST_INDEX_ENTRY_ARRAY;

	typedef struct tagST_STACK_NODE
	{
		// ����,
		// Stack�� Index Entry Array �̿ܿ� �ٸ� ���� �� �� �ִ�.
		LPST_INDEX_ENTRY_ARRAY	pstIndexEntryArray;
	} ST_STACK_NODE, *LPST_STACK_NODE;

	typedef struct tagST_STACK
	{
		LPST_STACK_NODE	pstNodeArray;
		INT				nAllocSize;
		INT				nItemCount;
		INT				nGrowthCount;
	} ST_STACK, *LPST_STACK;

public:
	typedef struct tagST_FINDFIRSTINODE
	{
		LPST_NTFS		pstNtfs;
		UBYTE4			nIndexBlockSize;
		LPST_RUNLIST	pstRunListAlloc;
		INT				nCountRunList;
		LPST_STACK		pstStack;
		BOOL			bFinished;
		BOOL			bErrorOccurred;
	} ST_FINDFIRSTINODE, *LPST_FINDFIRSTINODE;

public:
	//////////////////////////////////////////////////////////////////////////
	// inode operation
	static TYPE_INODE FindInode(IN LPST_NTFS pstNtfs, IN TYPE_INODE nStartInode, IN LPCWSTR lpszName, OUT PBOOL pbFound, OPTIONAL OUT LPST_INODEDATA pstInodeData);
	static LPST_FINDFIRSTINODE FindFirstInode(IN LPST_NTFS pstNtfs, IN TYPE_INODE nInode, OUT LPST_INODEDATA pstInodeData, OUT LPWSTR lpszName, IN DWORD dwCchName);
	static BOOL FindNextInode(IN LPST_FINDFIRSTINODE pstFind, OUT LPST_INODEDATA pstInodeData, OUT LPWSTR lpszName, IN DWORD dwCchName);
	static BOOL FindCloseInode(IN LPST_FINDFIRSTINODE pstFind);

protected:
	// INDEX_ENTRY operation
	static LPST_INDEX_ENTRY	GetNextEntry(IN LPST_INDEX_ENTRY pstCurIndexEntry);
	static LPST_ATTRIB_FILE_NAME GetFileNameAttrib(IN LPST_INDEX_ENTRY pstIndexEntry);
	static BOOL GetIndexEntryFileName(IN LPST_INDEX_ENTRY pstEntry, OUT LPWSTR lpszName, IN DWORD dwCchLength);
	static BOOL UpdateSequenceIndexAlloc(IN UBYTE4 nIndexSize, IN UBYTE1* pBufCluster, IN INT nClusterSize, IN INT nSectorSize);
	static TYPE_INODE GetInode(IN LPST_INDEX_ENTRY pstIndexEntry, OPTIONAL OUT UBYTE2* pnSequenceNumber);
	static BOOL IndexEntryToIndexEntryNode(IN LPST_INDEX_ENTRY pstIndexEntry, OUT LPST_INDEX_ENTRY_NODE pstIndexEntryNode);
	static BOOL GetInodeData(IN LPST_INDEX_ENTRY_NODE pstIndexEntryNode, OUT LPST_INODEDATA pstData, OPTIONAL OUT LPTSTR lpszName, OPTIONAL IN DWORD dwCchName);

	// INDEX_ENTRY_ARRAY Operation
	static LPST_INDEX_ENTRY_ARRAY GetIndexEntryArrayAllocByVcn(IN LPST_NTFS pstNtfs, IN UBYTE4 nIndexBlockSize, IN LPST_RUNLIST pstRunList, IN INT nCountRunList, IN UBYTE8 nVcn);
	static LPST_INDEX_ENTRY_ARRAY GetIndexEntryArrayAllocByIndexRoot(IN LPST_NTFS pstNtfs, IN TYPE_INODE nInode, OUT UBYTE4* pnIndexSize);
	static VOID GetIndexEntryArrayFree(IN LPST_INDEX_ENTRY_ARRAY pstArrayAlloc);
	static LPST_INDEX_ENTRY_ARRAY GetIndexEntryArrayAlloc(IN LPST_INDEX_ENTRY pstIndexEntry);

	// Stack Operation
	static LPST_STACK CreateStack(IN INT nGrowthCount);
	static VOID DestroyStack(IN LPST_STACK pstStack);
	static BOOL StackPop(IN LPST_STACK pstStack, OUT LPST_STACK_NODE pstNode);
	static BOOL StackPush(IN LPST_STACK pstStack, IN LPST_STACK_NODE pstNode);
	static BOOL StackGetTop(IN LPST_STACK pstStack, OUT LPST_STACK_NODE pstNode);
	static BOOL IsStackEmptry(IN LPST_STACK pstStack);

	// Stack�� ������ ũ��
	// �̴� Ntfs�� �ִ� B-tree�� depth�� ������ �ִ�.
	// ���� B-tree�� depth�� 3�ε�,
	// �̰ͺ��� ���� ũ�� �����Ѵ�.
	// �׷���, Stack�� Ŀ�� �� re-allocation�� �پ�� ���̴�.
#define STACK_GROWTH_COUNT	(5)
	// Stack�� �ִ� ũ��
	// STACK_GROWTH_COUNT ��ŭ �����ϸ�,
	// �ִ밪�� �����Ѵ�.
	// ntfs index ó���� b-tree�� �ִ� depth�� �����ϸ� �ȴ�.
#define STACK_MAXIMUM_COUNT	(512)

protected:
	// Other Utility
	static INT wcscmpUppercase(IN LPCWSTR lpszFindNameUpper, IN LPCWSTR lpszEntry);
	static FILETIME GetLocalFileTime(IN UBYTE8 nUtc);

	//////////////////////////////////////////////////////////////////////////
	//////////////////////////////////////////////////////////////////////////
	//// III. Win32 API Emulation
	//////////////////////////////////////////////////////////////////////////
	//////////////////////////////////////////////////////////////////////////

protected:
	// Handle ���� 
	typedef INT TYPE_WIN32_HANDLE;
	#define		TYPE_WIN32_HANDLE_UNKNOWN		(0)
	#define		TYPE_WIN32_HANDLE_FINDFIRSTFILE	(1)
	#define		TYPE_WIN32_HANDLE_CREATEFILE	(2)

	// Handle ����
	typedef struct tagST_WIN32_HANDLE
	{
		LPST_NTFS			pstNtfs;
		TYPE_WIN32_HANDLE	nType;
		LPVOID				pLock;

		// nType�� ���� HANDLE ������ ����.
		union
		{
			// FindFirstFile �Լ��� ���� ����
			struct
			{
				LPST_FINDFIRSTINODE	pstFind;
				BOOL				bFindSubDir;
				BOOL				bUseFindDot;
				BOOL				bEmptryDirectory;
				INT					nDotCount;
				TYPE_INODE			nInodeStart;
				TYPE_INODE			nInodeParent;
				PWCHAR				lpszNameTmp;

				// �Ʒ� �κ��� ..�� FindNextFile �� ��,
				// WIN32_FIND_DATA ������ �� �κ��̴�.
				// ����)
				// ..�� . ��, FindFirstFile�� ��ο� ���� ���� ���޵Ǿ���.
				// ��������δ�, ���� ����� ������ �� �� ������ �ƴϾ���.
				// ����, FindFirstFile�� .�� ���޵� ��,
				// �Ʒ� ���� �����Ͽ�,
				// ..�� ������ ���� �����ϵ��� �Ѵ�.
				// WIN32_FIND_DATA ��ü�� �����ص� �ǳ�,
				// sizeof(WIN32_FIND_DATA)�� ũ�Ⱑ ũ�� ������,
				// �� �Ϻθ� �����ؼ� ����Ѵ�.
				DWORD				dotdot_dwFileAttributes;
				FILETIME			dotdot_ftCreationTime;
				FILETIME			dotdot_ftLastAccessTime;
				FILETIME			dotdot_ftLastWriteTime;
			} stFindFirstFile;

			// CreateFile �Լ��� ���� ����
			struct 
			{
				TYPE_INODE		nInode;
				UBYTE8			nFileSize;
				BOOL			bResident;
				LPST_RUNLIST	pstRunList;
				INT				nCountRunList;
				UBYTE8			nCountVcn;
				UBYTE1*			pBufResidentData;
				INT				nBufResidentDataSize;
				UBYTE8			nFilePointer;
				UBYTE1*			pBufCluster;
				BOOL			bErrorOccurred;
			} stCreateFile;
		};
	} ST_WIN32_HANDLE, *LPST_WIN32_HANDLE;

public:
	// Main Handle
	typedef LPST_WIN32_HANDLE NTFS_HANDLE;

	//////////////////////////////////////////////////////////////////////////
	// FindFirstFile �Լ���
public:
	static NTFS_HANDLE WINAPI FindFirstFile(IN LPCWSTR lpFileName, OUT LPWIN32_FIND_DATAW lpFindFileData);
	static BOOL WINAPI FindNextFile(IN NTFS_HANDLE hFindFile, OUT LPWIN32_FIND_DATA lpFindFileData);
	static BOOL WINAPI FindClose(IN NTFS_HANDLE hFindFile);

public:

	// Win32 ���� �Լ��� �ƴ�����,
	// FindFirstFile �Լ��� ���� ����̳�, �ܺ� I/O ����� �����Ѵ�.
	// ~ByInode�� ~WithInode���� �̿��ϸ�, ������ ����ų �� �ִ�.

	// pstIoFunction�� NULL �����ѵ�, ���� ������ ä���� �����ϸ�,
	// �ܺ� I/O�� ����Ͽ� ����� �� �ִ�.
	// �ܺ� I/O�� ����Ͽ� ���޹��� NTFS_HANDLE�� ������,
	// FindNext~ �Լ����� ȣ���ϸ�, �ڵ����� �ܺ� I/O�� �����ȴ�.

	// pstInodeCache�� NULL �����ѵ�, ���� ������ ä���� �����ϸ�,
	// Inode���� ���� cache ����� ����� �� �ִ�.
	// ����, cache �ڵ�� caller���� ������ �Ѵ�.
	static NTFS_HANDLE FindFirstFileExt(IN LPCWSTR lpFileName, OUT LPWIN32_FIND_DATAW lpFindFileData, OPTIONAL IN LPST_IO_FUNCTION pstIoFunction, OPTIONAL IN LPST_INODE_CACHE pstInodeCache);
	static NTFS_HANDLE FindFirstFileByInodeExt(IN LPCWSTR lpVolumePath, IN LPST_INODEDATA pstInodeData, IN BOOL bFindSubDir, OPTIONAL OUT LPST_INODEDATA pstSubDirInodeData, OUT LPWIN32_FIND_DATAW lpFindFileData, OPTIONAL IN LPST_IO_FUNCTION pstIoFunction, OPTIONAL IN LPST_INODE_CACHE pstInodeCache);
	static NTFS_HANDLE FindFirstFileWithInodeExt(IN LPCWSTR lpFileName, OUT LPWIN32_FIND_DATAW lpFindFileData, OUT LPST_INODEDATA pstInodeData, OPTIONAL IN LPST_IO_FUNCTION pstIoFunction, OPTIONAL IN LPST_INODE_CACHE pstInodeCache);
	static BOOL FindNextFileWithInodeExt(IN NTFS_HANDLE hFindFile, OUT LPWIN32_FIND_DATA lpFindFileData, OUT LPST_INODEDATA pstInodeData);

protected:
	// FindFirstFile, FindFirstFileByInode���� ȣ��Ǵ� ���� Main �Լ�
	// FindNextFile, FindNextFileWithInode���� ȣ��Ǵ� ���� Main �Լ�
	static NTFS_HANDLE FindFirstFileMain(IN LPST_NTFS pstNtfs, IN LPST_INODEDATA pstInodeData, IN BOOL bFindSubDir, OPTIONAL OUT LPST_INODEDATA pstSubDirInodeData, OUT LPWIN32_FIND_DATAW lpFindFileData, OUT LPDWORD pdwErrorCode);
	static BOOL FindNextFileMain(IN NTFS_HANDLE hFindFile, OUT LPWIN32_FIND_DATA lpFindFileData, OPTIONAL OUT LPST_INODEDATA pstInodeData);

protected:
	// Utility
	static LPST_NTFS FindFirstFileOpenNtfs(IN LPCWSTR lpFileName, OUT LPWSTR lpszPath, IN DWORD dwCchPath, OPTIONAL IN LPST_IO_FUNCTION pstIoFunction, OPTIONAL IN LPST_INODE_CACHE pstInodeCache, OUT LPDWORD pdwErrorCode, OPTIONAL OUT PBOOL pbFindSubDir);
	static DWORD GetFindFirstFilePath(IN LPCWSTR lpFileName, OUT LPWSTR lpszPath, IN DWORD dwCchPath, OUT LPSTR pchVolumeDrive, OUT PBOOL pbFindSubDir);
	static DWORD GetPathComponent(IN LPCWSTR lpszPath, OUT LPWSTR lpszComponent, IN DWORD dwCchComponent, IN OUT PINT pnPos);
	static DWORD FindInodeFromPath(IN LPST_NTFS pstNtfs, IN LPCWSTR lpszPath, OUT TYPE_INODE* pnInode, OPTIONAL OUT LPST_INODEDATA pstInodeData);
	static BOOL  FilterFindFirstFile(IN LPST_INODEDATA pstData);
	static BOOL GetWin32FindData(IN LPST_INODEDATA pstData, IN LPCWSTR lpszName, OUT PWIN32_FIND_DATAW pstFD);

	//////////////////////////////////////////////////////////////////////////
	// CreateFile / ReadFile �Լ���
public:
	static NTFS_HANDLE WINAPI CreateFile(IN LPCWSTR lpFileName, IN DWORD dwDesiredAccess, NOT_USED IN DWORD dwShareMode, NOT_USED IN LPSECURITY_ATTRIBUTES lpSecurityAttributes, IN DWORD dwCreationDisposition, NOT_USED IN DWORD dwFlagsAndAttributes, NOT_USED IN HANDLE hTemplateFile);
	static BOOL WINAPI ReadFile(IN NTFS_HANDLE hFile, OUT LPVOID lpBuffer, IN DWORD nNumberOfBytesToRead, OUT LPDWORD lpNumberOfBytesRead, NOT_USED OPTIONAL IN OUT LPOVERLAPPED lpOverlapped);
	static BOOL WINAPI CloseHandle(IN NTFS_HANDLE hObject);
	static DWORD WINAPI GetFileSize(IN NTFS_HANDLE hFile, OPTIONAL OUT LPDWORD lpFileSizeHigh);
	static DWORD WINAPI SetFilePointer(IN NTFS_HANDLE hFile, IN LONG lDistanceToMove, OPTIONAL IN OUT PLONG lpDistanceToMoveHigh, IN DWORD dwMoveMethod);

public:
	// �ܺ� I/O ���� �Լ�
	// (����) CreateFileIo�� ������ Handle�� ������, ReadFile, CloseHandle, GetFileSize, SetFilePointer�� ȣ���ϸ�,
	// �ش� �Լ����� �ڵ����� �ܺ� I/O�� ����ȴ�.
	static NTFS_HANDLE WINAPI CreateFileIo(IN LPCWSTR lpFileName, IN DWORD dwDesiredAccess, NOT_USED IN DWORD dwShareMode, NOT_USED IN LPSECURITY_ATTRIBUTES lpSecurityAttributes, IN DWORD dwCreationDisposition, NOT_USED IN DWORD dwFlagsAndAttributes, NOT_USED IN HANDLE hTemplateFile, OPTIONAL IN LPST_IO_FUNCTION pstIoFunction);

public:
	// Win32 ���� �Լ��� �ƴ�����,
	// Ȯ�� API
	// File�� Attribute data�� �����ϴµ� ���ȴ�.
	// ������ CNTFSHelper::CreateFile�� Main �Լ��̴�.
	// �ܺ� I/O ������� �ʴ� �Ϲ����� ��Ȳ�̶��, pstIoFunction�� NULL ���� �����ϴ�.
	static NTFS_HANDLE CreateFileByAttrib(IN LPCWSTR lpFileName, IN DWORD dwDesiredAccess, NOT_USED IN DWORD dwShareMode, NOT_USED IN LPSECURITY_ATTRIBUTES lpSecurityAttributes, IN DWORD dwCreationDisposition, NOT_USED IN DWORD dwFlagsAndAttributes, NOT_USED IN HANDLE hTemplateFile, IN TYPE_ATTRIB nTypeAttrib, OPTIONAL IN LPST_IO_FUNCTION pstIoFunction);

protected:
	// Lock �Լ�
	// �Ʒ� �Լ��� ����,
	// III. Win32 API Emulation�� Public �Լ�������,
	// Thread-safe�ϴ�.
	// [TBD] other platform (��, linux)
	static LPVOID CREATE_LOCK(VOID);
	static VOID DESTROY_LOCK(IN LPVOID pLock);
	static VOID LOCK(IN LPVOID pLock);
	static VOID UNLOCK(IN LPVOID pLock);
};
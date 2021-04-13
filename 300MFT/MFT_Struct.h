#pragma once
#include "stdafx.h"
/*
Attribute 종류
0x10 $STANDARD_INFORMATION 최근 접근 / 생성 시간, 소유자, 보안 아이디와 같은 일반적 정보
0x20 $ATTRIBUTE_LIST 속성 리스트 정보
0x30 $FILE_NAME 파일 이름(유니코드 형식)
0x40 $VOLUME_VERSION 볼륨 정보, 오직 1.2버전에만 존재
0x40 $OBJECT_ID 16 bytes로 이루어진 파일이나 디렉토리 고유 값
0x50 $SECURITY_DESCRIPTOR 파일의 접근 제어와 보안 속성
0x60 $VOLUME_NAME 볼륨 이름과 관련 정보를 가지고 있음
0x70 $VOLUME_INFORMATION 파일시스템의 버전과 여러 Flags
0x80 $DATA 파일의 내용을 담고 있다.
0x90 $INDEX_ROOT 인덱스 트리의 루트 노드
0xA0 $INDEX_ALLOCATION 인덱스 트리와 연결된 노드들을 담고 있다.
0xB0 $BITMAP 할당 정보를 관리하는 속성
0xC0 $SYMBOLIC_LINK Soft Link 정보 (버전 1.2에만 존재)
0xC0 $REPARSE_POINT Reparse 위치 정보
0xD0 $EA_INFORMATION Os/2 응용프로그램과의 호환성을 위해 사용
0xE0 $EA Os/2 응용프로그램과의 호환성을 위해 사용
0x0100 $LOGGED_UTILITY_STREAM 암호화된 속성의 정보와 Key를 가지고 있다
*/

/*MFT 메타데이터 엔트리
File Name
설명
0 : $MFT #MFT를 담고 있는 파일
1 : $MFTMirr #MFT 백업본
2 : $LogFile #트랜잭션 저널을 기록
3 : $Volume #볼륨의 레이블, 버전 등 볼륨에 대한 정보
4 : $AttrDef #인자 값, 이름, 크기 등 여러 속성 값
5 : . #파일 시스템의 루트 디렉토리
6 : $Bitmap #파일 시스템의 클러스터 할당 관리 정보
7 : $Boot #부트 레코드 영역의 정보
8 : $BadClus #배드 클러스터 영역 정보
9 : $Secure #보안과 접근 권한에 대한 정보
10 : $Upcase #모든 유니코드 문자의 대문자
11 : $Extend #추가적 확장을 담는 디렉토리. Windows는 일바적으로 이 디렉토리 안에 어떠한 정보도 담지 않음
12~15 : 사용 안 함 #사용 중 이라 설정은 되어 있지만 비어 있음
16~23 : 사용 안 함 #미래를 위해 비어 있음
상관 없음 : $Objld #파일 고유의 ID 정보를 담음(Windows 2000 이상 버전부터 존재)
상관 없음 : $Quota #사용량 정보(Windows 2000 이상부터 존재)
상관 없음 : $Reparse #Reparse Point에 대한 정보(Windows 2000 이상부터 존재)
상관 없음 : $UsnJrnl #파일이나 디렉토리 변경이 있을 경우 그 기록을 담아 놓은 파일(Windows 2000 이상부터 존재)
24~ : 일반 파일 #일반적 파일이나 디렉토리들은 이 이후에 저장됨.
*/
#define $FILENAMESIZE	66
#define $AttributeHeaderSize	16
#define $ResidentSize		8
#define $NoneResidentSize	48

#pragma pack(push, 1)
enum MFT_Attr
{
	_MFT_STANDARD_INFORMATION= 0x10,
	_MFT_ATTRIBUTE_LIST= 0x20,
	_MFT_FILE_NAME = 0x30,
	_MFT_VOLUME_VERSION = 0x40,
	_MFT_OBJECT_ID = 0x40,
	_MFT_SECURITY_DESCRIPTOR = 0x50,
	_MFT_VOLUME_NAME= 0x60,
	_MFT_VOLUME_INFORMATION= 0x70,
	_MFT_DATA= 0x80,
	_MFT_INDEX_ROOT= 0x90 ,
	_MFT_INDEX_ALLOCATION= 0xA0,
	_MFT_BITMAP= 0xB0,
	_MFT_SYMBOLIC_LINK= 0xC0,
	_MFT_REPARSE_POINT= 0xC0,
	_MFT_EA_INFORMATION= 0xD0,
	_MFT_EA= 0xE0,
	_MFT_LOGGED_UTILITY_STREAM= 0x0100
};


typedef struct _MFT_Entry_Header
{
	char Signature[4];
	WORD OffsetOfFixupArray;
	WORD OffsetOfFixupValue;
	ULONGLONG LogFileSequenceNumber;
	WORD SequenceValue;
	WORD HardLinkCount;
	WORD OffsetToFirstAttribute;
	WORD Flags;
	DWORD UsedSizeOfMFTEntry;
	DWORD AllocatedSizeOfMFTEntry;
	ULONGLONG FileReferenceToBaseMFTEntry;
	WORD NextAttributeID;


}MFT_Entry_Header;


typedef struct _MFT_Attr_Header
{
	DWORD AttributeTypeID;
	DWORD LengthOfAttribute;
	UCHAR NonResidentFlag; //0이면 레지던트 1이면 Non레지던츠
	UCHAR LengthOfName;
	WORD OffsetToName;
	WORD Flags;
	WORD AttributeIdentifier;

}MFT_Attr_Header;


typedef struct _MFT_Resident
{
	DWORD SizeOfContent;
	WORD OffsetToContent;
	UCHAR IndexedFlag;
	UCHAR Padding; 

}MFT_Resident;


typedef struct _MFT_NonResident
{
	ULONGLONG StartingVcnRunlist;
	ULONGLONG EndingVcnRunlist;
	WORD OffsetRunlist; //0이면 레지던트 1이면 Non레지던츠
	WORD CompressionUnitSize;
	DWORD Padding;
	ULONGLONG AllocatedSizeAttributeContent;
	ULONGLONG RealSizeAttributeContent;
	ULONGLONG InitializedSizeAttributeContent;

}MFT_NonResident;

typedef struct _MFT_Standard_Information
{
	DWORD64 CreationTime;
	DWORD64 ModifiedTime;
	DWORD64 MFTModifiedTime;
	DWORD64 AccessedTime;
	DWORD32 Flags;
	DWORD32 MNV; //Maximum number of versions
	DWORD32 VersionNumber;
	DWORD32 ClassID;
	DWORD32 OwnerID;
	DWORD32 SecurityID;
	DWORD64 QuotaCharged;
	DWORD64 USN;

}MFT_Standard_Information;

typedef struct _MFT_Attr_List
{
	DWORD AttrType;
	WORD EntryLength;
	UCHAR NameLength;
	UCHAR StartLengthOffset;
	ULONGLONG AttrStartVCN;
	UCHAR FileReferenceAddr[8];
	UCHAR AttrID;
}MFT_Attr_List;

typedef struct _MFT_FileName
{
	//UCHAR MFTEntryAddress[6];
	//WORD SequenceNumber;
	ULONGLONG FileReferenceAddress;
	ULONGLONG CreationTime;
	ULONGLONG ModifiedTime;
	ULONGLONG MFTModifiedTime;
	ULONGLONG AccessedTime;
	ULONGLONG AllocatedSizeFile;
	ULONGLONG RealSizeFile;
	DWORD Flags;
	DWORD ReparseValue;
	UCHAR LengthName;
	UCHAR TypeName;


}MFT_FileName;



typedef struct _Entry_Info
{
	std::string FileName;
	ULONGLONG ParentEntyrNum;
	ULONGLONG LSN;
	ULONGLONG USN;
	WORD EntryFlags;
	
}Entry_Info;

typedef struct _MFT_Collumn
{
	std::string FileName;
	ULONGLONG LSN;
	ULONGLONG USN;

}MFT_Collumn;
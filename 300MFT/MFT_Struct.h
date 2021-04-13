#pragma once
#include "stdafx.h"
/*
Attribute ����
0x10 $STANDARD_INFORMATION �ֱ� ���� / ���� �ð�, ������, ���� ���̵�� ���� �Ϲ��� ����
0x20 $ATTRIBUTE_LIST �Ӽ� ����Ʈ ����
0x30 $FILE_NAME ���� �̸�(�����ڵ� ����)
0x40 $VOLUME_VERSION ���� ����, ���� 1.2�������� ����
0x40 $OBJECT_ID 16 bytes�� �̷���� �����̳� ���丮 ���� ��
0x50 $SECURITY_DESCRIPTOR ������ ���� ����� ���� �Ӽ�
0x60 $VOLUME_NAME ���� �̸��� ���� ������ ������ ����
0x70 $VOLUME_INFORMATION ���Ͻý����� ������ ���� Flags
0x80 $DATA ������ ������ ��� �ִ�.
0x90 $INDEX_ROOT �ε��� Ʈ���� ��Ʈ ���
0xA0 $INDEX_ALLOCATION �ε��� Ʈ���� ����� ������ ��� �ִ�.
0xB0 $BITMAP �Ҵ� ������ �����ϴ� �Ӽ�
0xC0 $SYMBOLIC_LINK Soft Link ���� (���� 1.2���� ����)
0xC0 $REPARSE_POINT Reparse ��ġ ����
0xD0 $EA_INFORMATION Os/2 �������α׷����� ȣȯ���� ���� ���
0xE0 $EA Os/2 �������α׷����� ȣȯ���� ���� ���
0x0100 $LOGGED_UTILITY_STREAM ��ȣȭ�� �Ӽ��� ������ Key�� ������ �ִ�
*/

/*MFT ��Ÿ������ ��Ʈ��
File Name
����
0 : $MFT #MFT�� ��� �ִ� ����
1 : $MFTMirr #MFT �����
2 : $LogFile #Ʈ����� ������ ���
3 : $Volume #������ ���̺�, ���� �� ������ ���� ����
4 : $AttrDef #���� ��, �̸�, ũ�� �� ���� �Ӽ� ��
5 : . #���� �ý����� ��Ʈ ���丮
6 : $Bitmap #���� �ý����� Ŭ������ �Ҵ� ���� ����
7 : $Boot #��Ʈ ���ڵ� ������ ����
8 : $BadClus #��� Ŭ������ ���� ����
9 : $Secure #���Ȱ� ���� ���ѿ� ���� ����
10 : $Upcase #��� �����ڵ� ������ �빮��
11 : $Extend #�߰��� Ȯ���� ��� ���丮. Windows�� �Ϲ������� �� ���丮 �ȿ� ��� ������ ���� ����
12~15 : ��� �� �� #��� �� �̶� ������ �Ǿ� ������ ��� ����
16~23 : ��� �� �� #�̷��� ���� ��� ����
��� ���� : $Objld #���� ������ ID ������ ����(Windows 2000 �̻� �������� ����)
��� ���� : $Quota #��뷮 ����(Windows 2000 �̻���� ����)
��� ���� : $Reparse #Reparse Point�� ���� ����(Windows 2000 �̻���� ����)
��� ���� : $UsnJrnl #�����̳� ���丮 ������ ���� ��� �� ����� ��� ���� ����(Windows 2000 �̻���� ����)
24~ : �Ϲ� ���� #�Ϲ��� �����̳� ���丮���� �� ���Ŀ� �����.
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
	UCHAR NonResidentFlag; //0�̸� ������Ʈ 1�̸� Non��������
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
	WORD OffsetRunlist; //0�̸� ������Ʈ 1�̸� Non��������
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
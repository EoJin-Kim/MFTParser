#pragma once

#include "stdafx.h"
#include "300MFT.h"
//#define MFTAttrHeaderSize	16
#define NTFSFILENAMEMAX		256
#define MFTStartEntryNum	27
class MFT_Parser : public NTFS_Parser
{
private:

protected:

public:
	MFT_Parser(void);
	MFT_Parser(char* _drivename);
	~MFT_Parser(void);

	//std::map<ULONGLONG, ULONGLONG> MFTRunlist;

	std::vector<ULONGLONG> MFTRunlistSizeVector;
	std::vector<ULONGLONG> MFTRunlistOffsetVector;
	

	ULONGLONG MFTVcnSize;
	
	static UCHAR* MFTbuf;
	static UCHAR* MFTstartBufOffset;
	static ULONGLONG MFTEntryCount;
	static std::string tableColumn;
	static std::map<ULONGLONG, Entry_Info> MFTdb;
	static std::map<ULONGLONG, std::string> dirFullPath;
	static std::map<ULONGLONG, Entry_Info> FileInfo;
	UCHAR* MFTrecentBufEntryOffset;
	UCHAR* MFTEntry;

	wchar_t fileName[255];

	ULONGLONG FileCount;
	/// <summary>
	/// 여기 까지 사용 밑에는 수정 예정
	/// </summary>
	MFT_Entry_Header* myMFT;
	MFT_Attr_Header* MFTattrAddr;
	//MFT_Attr_Header* MFTattr;

	MFT_Resident* MFTRegi;
	MFT_Resident* MFTRegiAddr;

	MFT_NonResident* MFTNonRegi;
	MFT_NonResident* MFTNonRegiAddr;

	std::stack<char*> FilePath;

	int MFTFileList(HANDLE hdrive);
	int ReadMFT(HANDLE hdrive);
	int Go2MFT(HANDLE hdrive, UCHAR* buf);
	int CheckSumCheck(UCHAR* buf);
	int ReadMFTAttr(const UCHAR* buf);
	int ResidentFlag(const UCHAR* buf, DWORD attrLen);
	int AttributeType(const IN UCHAR* buf);
	inline int MFTStadardInformation(IN UCHAR* recentOffset, ULONGLONG recentEntry,DWORD64 &usn);
	inline int MFTAttributeList(IN UCHAR* recentOffset,ULONGLONG recentEntry);
	inline int MFTFileName(const IN UCHAR* buf, ULONGLONG* EntryAddress, ULONGLONG *parentEntryAddress);

	int MFTObjectID(const IN UCHAR* buf);
	int MFTSecurityDescriptor(const IN UCHAR* buf);
	int MFTVolumeName(const IN UCHAR* buf);
	int MFTVolumeInformation(const IN UCHAR* buf);
	int MFTData(const IN UCHAR* buf, ULONGLONG* VCNSize);
	int MFTIndexRoot(const IN UCHAR* buf);
	int MFTIndexAllocation(const IN UCHAR* buf);
	int MFTBitmap(const IN UCHAR* buf);
	int MFTReparsePoint(const IN UCHAR* buf);
	int MFTEa(const IN UCHAR* buf);
	int MFTLoggedUtilityStream(const IN UCHAR* buf);

	int RunListCalc(const IN UCHAR* buf, int runlistOffset, std::vector<ULONGLONG> *runlistSizeVector, std::vector<ULONGLONG> *runlistOffsetVector);
	int FixUpArrayRecover();
	//int FindRootDir(ULONGLONG mftEntryAddress, std::stack<std::wstring> *filePath);
	inline int FindEntryName(ULONGLONG mftEntryAddress,bool attrListCheck);
	ULONGLONG FindMFTEntryOffset(ULONGLONG MFTEntry);
	int WriteSqlite(std::wstring querys);
	int MFTTraversal(ULONGLONG start, ULONGLONG end);
	int ThreadStart(int threadCount);
	int DirFullPath(void);
	int FileFullPath(void);
	int FindRootDir(ULONGLONG taskEntryNum, ULONGLONG parentEntryNum);
	int MakeQuery(std::string &query,std::string tableName,ULONGLONG entryNum);
	
};



#include "stdafx.h"
#include "MFT_Parser.h"
#include "300MFT.h"


//MFT_Parser::MFT_Parser(TCHAR* _drivename) : NTFS_Parser(_drivename)
MFT_Parser::MFT_Parser() : NTFS_Parser()
{
}
MFT_Parser::~MFT_Parser(void)
{
}
// 해야 할 것
// 1. MFT Entry 값 받아오면 offset 계산하여 리턴해주는 함수 만들기
// 2. MFT파일에 대한 For문 만들기
int MFT_Parser::MFTFileList(HANDLE hdrive)
{
	this->FileCount = 0;

	// txt에 저장하는 코드
	/*
	DWORD dwWrite, dwRead;
	
	std::wstring wDatas = L"";
	wchar_t lineFeed = L'\n';
	
	HANDLE wFile = CreateFileW(L"write.txt", GENERIC_READ | GENERIC_WRITE, 0, NULL, CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL, NULL);

	if (wFile == INVALID_HANDLE_VALUE)
	{
		_tprintf(TEXT("File Not Open\n"));
		return 0;

	}
	//WriteFile(wFile, wdata.c_str(), wdata.size() * sizeof(wchar_t), &dwWrite, NULL);
	// txt에 저장하는 코드 끝
	*/
	
	//this->MFTEntryCount = 1000000;
	//this->ThreadStart(1);
	
	//CloseHandle(wFile);
	//sqlite3_close(db);
	// buf 주소 초기화 후 삭제
	this->MFTbuf = this->MFTstartBufOffset;
	//delete[] this->MFTbuf;
	//delete[] this->fileName;
	//delete filePath;
	_tprintf(TEXT("MFT Count : %llu\n"), MFTEntryCount);
	
	return 1;
}

ULONGLONG MFT_Parser::FindMFTEntryOffset(ULONGLONG MFTEntry)
{
	ULONGLONG recentEndEntry = 0;
	for (int i = 0; i < this->MFTRunlistSizeVector.size(); i++)
	{
		_tprintf(TEXT("Vector Index : %d \n"), i);
		recentEndEntry += MFTRunlistSizeVector[i] / this->MFTEntrySize;
		if (recentEndEntry > MFTEntry)
		{
			recentEndEntry -= MFTRunlistSizeVector[i] / this->MFTEntrySize;
			ULONGLONG Offest = MFTRunlistOffsetVector[i] +(MFTEntry - recentEndEntry)*this->MFTEntrySize;
			return Offest;

		}
		else
		{
			continue;
		}

	}
	return -1;
}

int MFT_Parser::ReadMFT(HANDLE hdrive)
{
	this->ReadVBR(hdrive);
	UCHAR* buf = new UCHAR[this->MFTEntrySize];
	UCHAR* bufAddress;
	bufAddress = buf;
	MFT_Attr_Header *MFTattr;
	//this->MFTattrAddr = this->MFTattr;

	this->MFTRegi = new MFT_Resident;
	this->MFTRegiAddr = MFTRegi;

	this->MFTNonRegi = new MFT_NonResident;
	this->MFTNonRegiAddr = MFTNonRegi;

	this->Go2MFT(hdrive, buf);
	MFT_Entry_Header *entryHeader =(MFT_Entry_Header*)buf;
	//_tprintf(TEXT("MFTEntryHeaderSize Size : %d \n"), sizeof(MFT_Entry_Header));
	

	// 첫 속성 오프셋 만큼 buf 위치 이동
	buf += entryHeader->OffsetToFirstAttribute;

	// 속성이 존재하는지 안하는지 체크하는 변수
	UINT attrCheck;
	memcpy(&attrCheck, buf, 4);

	//std::stack<std::wstring> filePath;
	while (attrCheck != 0xffffffff)
	{
		MFTattr = (_MFT_Attr_Header*)buf;
		if (attrCheck == 0x80)
		{
			this->MFTData(buf, &this->MFTVcnSize);
		}
		//_tprintf(TEXT("MFT Attr %d \n"), attrCheck);
		buf += MFTattr->LengthOfAttribute;
		memcpy(&attrCheck, buf, 4);
	}

	buf = bufAddress;
	

	// MFT 파일의 크기만큼 버퍼 생성
	ULONGLONG MFTSize = this->MFTVcnSize * this->BytesPerCluster;
	this->MFTbuf = new UCHAR[MFTSize];

	// 한 버퍼에 모두 담는 초기 주소
	this->MFTstartBufOffset = this->MFTbuf;


	// Runlist계산해서 버퍼에 MFT파일 전체 복사
	for (int i = 0; i < this->MFTRunlistSizeVector.size(); i++)
	{

		this->ReadByte(this->hdrive, this->MFTbuf, this->MFTRunlistSizeVector[i], FILE_BEGIN, this->MFTRunlistOffsetVector[i]);
		this->MFTbuf += this->MFTRunlistSizeVector[i];

	}
	// 복사 후 buf초기 주소로 복귀
	this->MFTbuf = this->MFTstartBufOffset;
	this->MFTEntryCount = this->MFTVcnSize * this->BytesPerCluster / this->MFTEntrySize;
	this->MFTrecentBufEntryOffset = this->MFTbuf;
	delete[] buf;
	//delete this->MFTattr;
	
	return 0;
}

int MFT_Parser::Go2MFT(HANDLE hdrive, UCHAR* buf)
{
	this->ReadByte(hdrive,buf, this->MFTEntrySize, FILE_BEGIN,this->StartMFT);
	return 0;
}

int MFT_Parser::CheckSumCheck(UCHAR* buf)
{
	//buf=buf + (myMFT->OffsetOfFixupArray);
	USHORT chksum;
	memcpy(&chksum,(buf + this->myMFT->OffsetOfFixupArray), 2);
	int SectorsPerMFT = this->MFTEntrySize / this->BytesPerSector;
	for (int i = 0; i < SectorsPerMFT; i++)
	{
		USHORT tmp=0;
		memcpy(&tmp, (buf + this->BytesPerSector-2), 2);
		if (tmp != chksum)
		{
			_tprintf(TEXT("CheckSum Break \n"));
			break;
			return 0;
		}

	}
	
	return 1;	
}
int MFT_Parser::ReadMFTAttr(const UCHAR* buf)
{
	//UCHAR* tmpBuf = new UCHAR[this->MFTEntrySize];
	MFT_Attr_Header* MFTattr;
	buf += this->myMFT->OffsetToFirstAttribute;

	UINT attrCheck;
	//attrCheck = (UINT*)buf;
	

	//UINT attrSize=0;
	//_MFT_Attr_Header* recentAttr = new _MFT_Attr_Header;
	memcpy(&attrCheck, buf, 4);
	if (attrCheck != 0xffffffff)
	{
		MFTattr = (_MFT_Attr_Header*)buf;
	}
	while (attrCheck != 0xffffffff)
	{
		//Attr에 따른 구분
		
		
		this->AttributeType(buf);
		// 0 이면 레지던트
		//ResidentFlag(buf, this->MFTattr->LengthOfAttribute);

		buf += MFTattr->LengthOfAttribute;
		memcpy(&attrCheck, buf, 4);
		if (attrCheck != 0xffffffff)
		{
			MFTattr = (_MFT_Attr_Header*)buf;
			
		}
	}
	
	
	return 1;
}

int MFT_Parser::AttributeType(const UCHAR* buf)
{
	MFT_Attr_Header* MFTattr;
	MFTattr = (MFT_Attr_Header*)buf;
	//buf += sizeof(MFT_Attr_Header);
	switch (MFTattr->AttributeTypeID)
	{
	case _MFT_STANDARD_INFORMATION:

		_tprintf(TEXT("Attr 01 \n"));
		break;

	case _MFT_ATTRIBUTE_LIST:

		_tprintf(TEXT("Attr 02 \n"));
		break;
	case _MFT_FILE_NAME:
		//this->MFTFileName(buf);
		_tprintf(TEXT("Attr 03 \n"));
		break;
	case _MFT_OBJECT_ID:

		_tprintf(TEXT("Attr 04 \n"));
		break;
	case _MFT_SECURITY_DESCRIPTOR:

		_tprintf(TEXT("Attr 05 \n"));
		break;
	case _MFT_VOLUME_NAME:

		_tprintf(TEXT("Attr 06 \n"));
		break;
	case _MFT_VOLUME_INFORMATION:

		_tprintf(TEXT("Attr 07 \n"));
		
		break;
	case _MFT_DATA:

		_tprintf(TEXT("MFT DATA \n"));
		//this->MFTData(buf);

		break;
	case _MFT_INDEX_ROOT:

		_tprintf(TEXT("Attr 09 \n"));
		break;
	case _MFT_INDEX_ALLOCATION:

		_tprintf(TEXT("Attr 0A \n"));
		break;
	case _MFT_BITMAP:

		_tprintf(TEXT("Attr 0B \n"));
		break;
	case _MFT_REPARSE_POINT:

		_tprintf(TEXT("Attr 0C \n"));
		break;
	case _MFT_EA_INFORMATION:

		_tprintf(TEXT("Attr 0D \n"));
		break;
	case _MFT_EA:

		_tprintf(TEXT("Attr 0E \n"));
		break;
	case _MFT_LOGGED_UTILITY_STREAM:

		_tprintf(TEXT("Attr 100 \n"));
		break;
	}
	return 1;
}
inline int MFT_Parser::MFTFileName(const IN UCHAR* buf, ULONGLONG* EntryAddress, ULONGLONG* parentEntryAddress)
{
	MFT_Attr_Header* recentMFTattr = (MFT_Attr_Header*)buf;
	if (recentMFTattr->NonResidentFlag == 0)
	{
		buf += $AttributeHeaderSize;

		MFT_Resident *recentMFTRegi= (MFT_Resident*)buf;

		WORD SizeContent = recentMFTRegi->SizeOfContent;
		WORD OffsetContent = recentMFTRegi->OffsetToContent;
		//buf -= MFTAttrHeaderSize;
		buf += OffsetContent- $AttributeHeaderSize;
		MFT_FileName* fileNameInfo;
		fileNameInfo = (MFT_FileName*)buf;
		
		if (fileNameInfo->TypeName == 2)
		{
			return -1;
		}
		UCHAR nameSize = fileNameInfo->LengthName;
		//부모 MFT Entry Number 변수
		ULONGLONG MFTReferenceEntryAddress = 0;

		// 해당 MFT 얼마나 재사용 수 변수
		//WORD SequenceNumber = 0;

		memcpy(&MFTReferenceEntryAddress, buf, 6);
		buf += $FILENAMESIZE;
		
		*parentEntryAddress = MFTReferenceEntryAddress;
		memcpy(this->fileName, buf, nameSize*2);
		this->fileName[nameSize] = 0;
		std::string filename_str = StrConvert::WstrToUtf8(this->fileName);
		//Entry_Info entryInfo = { filename_str ,*parentEntryAddress, };
		this->MFTdb[*EntryAddress].FileName = filename_str;
		this->MFTdb[*EntryAddress].ParentEntyrNum = *parentEntryAddress;

	}
	return 1;
}
inline int MFT_Parser::FindEntryName(ULONGLONG mftEntryAddress,bool attrListCheck = false)
{
	
	UCHAR* tmpOffset;
	//_tprintf(TEXT("%d ->"), mftEntryAddress);
	ULONGLONG mftOffset = mftEntryAddress * this->MFTEntrySize;
	tmpOffset = this->MFTstartBufOffset + mftOffset;
	// MFT Enter Header 파싱
	MFT_Entry_Header *recentMFTEntryHeader = (MFT_Entry_Header*)tmpOffset;

	// Flags가 0이면 삭제된 파일 및 폴더 리턴
	if (recentMFTEntryHeader->Flags == 0)
	{
		return 0;
	}
	// flags 값이 0이 아니면 flag 값 저장
	DWORD flags= recentMFTEntryHeader->Flags;
	ULONGLONG LSN = recentMFTEntryHeader->LogFileSequenceNumber;

	MFT_Parser::MFTdb[mftEntryAddress].EntryFlags = flags;
	MFT_Parser::MFTdb[mftEntryAddress].LSN = LSN;

	ULONGLONG parentEntryAddress=-1;

	bool nameCheck = false;

	// 픽스업 어레이 복구
	//memcpy(this->MFTbuf + 510, this->MFTbuf + recentMFTEntryHeader->OffsetOfFixupArray + 2, 2);
	tmpOffset += recentMFTEntryHeader->OffsetToFirstAttribute;
	

	// 다음 속성이 존재하는지 확인
	//memcpy(&attrCheck, this->MFTbuf, 4);
	UINT *attrCheck = (UINT*)tmpOffset;
	DWORD64 USN = -1;
	while (*attrCheck != 0xffffffff)
	{
		//Attr에 따른 구분
		MFT_Attr_Header* recentMFTAttr = (MFT_Attr_Header*)tmpOffset;
		if (recentMFTAttr->LengthOfAttribute > this->MFTEntrySize)
		{
			return 3;
		}
		if (*attrCheck == 0x10)
		{
			
			if (this->MFTStadardInformation(tmpOffset, mftEntryAddress, USN) == 1)
			{
				MFT_Parser::MFTdb[mftEntryAddress].USN = USN;
			}
		}
		else if (*attrCheck == 0x20  && attrListCheck==false)
		{
			if (this->MFTAttributeList(tmpOffset, mftEntryAddress) != -1)
			{

			}
		}
		else if (*attrCheck == 0x30)
		{
			if (this->MFTFileName(tmpOffset, &mftEntryAddress, &parentEntryAddress) != -1)
			{

				nameCheck = true;


				
				break;
			}

		}
		tmpOffset += recentMFTAttr->LengthOfAttribute;
		attrCheck = (UINT*)tmpOffset;

	}
	if (!nameCheck)
	{
		//(*filePath).push(L"Noname");
		return 2;
	}
	if (mftEntryAddress == 5)
	{
		return 0;
	}
	else
	{
		//_tprintf(TEXT("%llu->"), parentEntryAddress);
		//FindRootDir(parentEntryAddress, filePath);
	}
	return 1;
}
int MFT_Parser::MFTData(const IN UCHAR* buf, ULONGLONG* VCNSize)
{
	//this->MFTattr = (MFT_Attr_Header*)buf;
	//int ResiResult = ResidentFlag(buf);
	MFT_Attr_Header* MFTattr = (MFT_Attr_Header*)buf;

	if (MFTattr->NonResidentFlag == 0)
	{
		buf += sizeof(MFTattr);
		this->MFTRegi = (MFT_Resident*)buf;
		_tprintf(TEXT("MFT Resident Flag\n"));
		//return 0;
	}
	// 1이면 넌 레지던트
	else if (MFTattr->NonResidentFlag == 1)
	{
		//Attr Header 관련 코드
		//buf += $AttributeHeaderSize;
		MFT_NonResident* MFTNonRegi = (MFT_NonResident*)(buf+ $AttributeHeaderSize);
		*VCNSize = MFTNonRegi->EndingVcnRunlist - MFTNonRegi->StartingVcnRunlist +1;
		
		ULONGLONG FileSize = (MFTNonRegi->EndingVcnRunlist - MFTNonRegi->StartingVcnRunlist+1)*this->BytesPerCluster;
		WORD runlistOffset = MFTNonRegi->OffsetRunlist;

		// Runlist 관련 코드
		//buf -= MFTAttrHeaderSize;
		this->RunListCalc(buf, runlistOffset,&this->MFTRunlistSizeVector,&this->MFTRunlistOffsetVector);

		//_tprintf(TEXT("MFT NonResident Flag\n"));
	}

	else
	{
		_tprintf(TEXT("MFT NonResident Flag Error \n"));
		//return -1;
	}

	return 1;
}
int MFT_Parser::RunListCalc(const IN UCHAR* buf,int runlistOffset, std::vector<ULONGLONG> *runlistSizeVector, std::vector<ULONGLONG> *runlistOffsetVector)
{
	UCHAR startRunlist = buf[runlistOffset];

	// Runlist 정보 1바이트 파싱
	UCHAR runlistFileSizeInfo;
	UCHAR runlistFileOffsetInfo;

	// Runlist 계산 변수 선언
	//std::map<ULONGLONG, ULONGLONG> SizeAndOffset;
	LONGLONG ClusterSizeTmp = 0;
	LONGLONG ClusterOffsetTmp = 0;
	ULONGLONG runlistClusterSize = 0;
	ULONGLONG runlistClusterOffset = 0;

	// Runlist 계산
	// size 파싱
	buf += runlistOffset;
	while (startRunlist != 0)
	{

		runlistFileSizeInfo = startRunlist & 0b1111;
		runlistFileOffsetInfo = (startRunlist & 0b11110000) >> 4;
		memcpy(&ClusterSizeTmp, &buf[1], runlistFileSizeInfo);
		ClusterSizeTmp *= this->BytesPerCluster;
		runlistClusterSize += ClusterSizeTmp;
		buf += runlistFileSizeInfo;
		// NTFS VBR에서부터의 위치
		// offset 파싱

		memcpy(&ClusterOffsetTmp, &buf[1], runlistFileOffsetInfo);

		UCHAR negativeCheck = buf[runlistFileOffsetInfo];
		
		if (negativeCheck & 0b10000000)
		{
			ULONGLONG negativeRemove = std::pow(2, 8 * runlistFileOffsetInfo)-1;
			ClusterOffsetTmp = negativeRemove- ClusterOffsetTmp +1;
			runlistClusterOffset -= ClusterOffsetTmp * this->BytesPerCluster;
		}
		else
		{
			//_tprintf(TEXT("Positive %lu\n"), ClusterOffsetTmp);
			runlistClusterOffset += ClusterOffsetTmp * this->BytesPerCluster;
		}
		
		buf += runlistFileOffsetInfo + 1;
		startRunlist = buf[0];
		runlistSizeVector->push_back(ClusterSizeTmp);
		runlistOffsetVector->push_back(runlistClusterOffset);
		ClusterOffsetTmp = 0;
		ClusterSizeTmp = 0;
		negativeCheck = 0;
	}
	return 1;
}
int MFT_Parser::FixUpArrayRecover()
{
	ULONGLONG MFTEntryCount = this->MFTVcnSize * this->BytesPerCluster / this->MFTEntrySize;
	UCHAR* EntryOffset = this->MFTbuf;
	MFT_Entry_Header* recentMFTEntryHeader;
	//recentMFTEntryHeader = (MFT_Entry_Header*)EntryOffset;
	for (int i = 27; i < MFTEntryCount; i++)
	{
		recentMFTEntryHeader = (MFT_Entry_Header*)EntryOffset;
		if (recentMFTEntryHeader->Flags == 0)
		{
			EntryOffset += this->MFTEntrySize;
			continue;
		}
		memcpy(EntryOffset + 510, EntryOffset + recentMFTEntryHeader->OffsetOfFixupArray + 2, 2);
		EntryOffset += this->MFTEntrySize;

	}
	return 1;
}
int MFT_Parser::ResidentFlag(const UCHAR* buf, DWORD AttrLen = 0)
{
	MFT_Attr_Header* MFTattr;

	MFTattr = (MFT_Attr_Header*)buf;
	//buf += sizeof(MFT_Attr_Header);

	if (MFTattr->NonResidentFlag == 0)
	{
		this->MFTRegi = (MFT_Resident*)buf;
		return 0;
	}
	// 1이면 넌 레지던트
	else if (MFTattr->NonResidentFlag == 1)
	{
		this->MFTNonRegi = (MFT_NonResident*)buf;
		

		return 1;
	}

	else
	{
		_tprintf(TEXT("MFT NonResident Flag Error \n"));
		return -1;
	}
}
int MFT_Parser::ThreadStart(const int threadCount)
{
	DWORD dwThreadId = 1;
	

	HANDLE *hThread = new HANDLE[threadCount];
	
	ULONGLONG splitCount = this->MFTEntryCount / threadCount;
	ULONGLONG startCount = 0;
	ThreadParam *param = new ThreadParam[threadCount];

	for (int i = 0; i < threadCount; i++)
	{
		if (i + 1 >= threadCount)
		{
			splitCount += this->MFTEntryCount % threadCount;
		}

		param[i] = { startCount,startCount + splitCount ,i };
		hThread[i] = CreateThread(NULL, 0, MyThreadMFTList, &param[i], 0, &dwThreadId);
		
		startCount += splitCount;


	}
	
	WaitForMultipleObjects(threadCount, hThread, TRUE, INFINITE);\

	if (hThread != NULL)
		delete[] hThread;
	if (param != NULL)
		delete[] param;

	return 1;
}

int MFT_Parser::MFTTraversal(ULONGLONG start, ULONGLONG end)
{
	for (int i = start; i < end; i++)
	{
		//_tprintf(TEXT("Entry : %llX\n"), i);
		/*
		if (i == 7986)
		{
			int a = 1;
		}
		*/
		
		FindEntryName(i);
	}
	return 1;
}

int MFT_Parser::DirFullPath(void)
{
	
	for (auto it : MFT_Parser::MFTdb) {
		//_tprintf(TEXT("Entry : %llu\t %s\n"), it.first, it.second.FileName.c_str());
		
		/*
		if (it.second.ParentEntyrNum == 0)
		{
			int a = 0;
		}
		*/

		//if (MFT_Parser::dirFullPath.find(it.second.ParentEntyrNum) == MFT_Parser::dirFullPath.end() && it.second.EntryFlags & 0x02)
		if (MFT_Parser::dirFullPath.find(it.second.ParentEntyrNum) == MFT_Parser::dirFullPath.end() && it.second.ParentEntyrNum!=0)
		{
			// not found
			// 0x2(폴더 옵션)과 or해서 참 and dirFullPath에 해당 디렉토리 없으면 조건문 실행
			FindRootDir(it.second.ParentEntyrNum,it.second.ParentEntyrNum);
			
				
		}
	}
	
	return 1;
}
int MFT_Parser::FileFullPath(void)
{
	for (auto it : MFT_Parser::MFTdb) {
		//int a = it.second.EntryFlags & 0x02;
		
		// 폴더가 아닐 경우만
		if (!(it.second.EntryFlags & 0x02))
		{
			// MFTdb 의 parentEntry가 driFullPath에 없는 경우
			/*
			if (MFT_Parser::dirFullPath.find(it.second.ParentEntyrNum) == MFT_Parser::dirFullPath.end())
			{
				int a = 0;;
			}
			*/
			if (!it.second.FileName.empty())
			{
				FileInfo[it.first].FileName.append(dirFullPath[it.second.ParentEntyrNum]).append("/").append(it.second.FileName);
				FileInfo[it.first].EntryFlags = it.second.EntryFlags;
				FileInfo[it.first].LSN = it.second.LSN;
				FileInfo[it.first].USN = it.second.USN;
				FileInfo[it.first].ParentEntyrNum = it.second.ParentEntyrNum;
			}
		}
	}
	return 1;
}

inline int MFT_Parser::FindRootDir(ULONGLONG taskEntryNum,ULONGLONG parentEntryNum)
{
	if (parentEntryNum != 5)
	{
		

		if (MFT_Parser::MFTdb.find(parentEntryNum) != MFT_Parser::MFTdb.end())
		{
			FindRootDir(taskEntryNum, MFT_Parser::MFTdb[parentEntryNum].ParentEntyrNum);
			MFT_Parser::dirFullPath[taskEntryNum].append("/").append(MFT_Parser::MFTdb[parentEntryNum].FileName.c_str());
		}
		else
		{

			//_tprintf(TEXT("Entry : %llu\tName %s\t Parent %llu\n"), taskEntryNum, MFT_Parser::MFTdb[taskEntryNum].FileName.c_str(), MFTdb[taskEntryNum].ParentEntyrNum);

		}
		
	}
	else
	{

		MFT_Parser::dirFullPath[taskEntryNum].append(driveName);
	}
	return 1;
}
inline int MFT_Parser::MFTStadardInformation(IN UCHAR* recentOffset, ULONGLONG recentEntry, DWORD64 &usn)
{
	MFT_Attr_Header* attrListHeader = (MFT_Attr_Header*)recentOffset;
	if (attrListHeader->NonResidentFlag == 0)
	{
		MFT_Resident* recentResident = (MFT_Resident*)(recentOffset + $AttributeHeaderSize);
		int contentLength = recentResident->SizeOfContent;
		int recentContentOffset = 0;
		recentOffset += recentResident->OffsetToContent;
		MFT_Standard_Information* stdInformaion = (MFT_Standard_Information*)recentOffset;
		usn = stdInformaion->USN;
		return 1;
	}
	// Non 레지던트 경우
	else
	{
		return 0;
	}
}

inline int MFT_Parser::MFTAttributeList(IN UCHAR* recentOffset,ULONGLONG recentEntry)
{
	MFT_Attr_Header* attrListHeader = (MFT_Attr_Header *)recentOffset;
	// 레지던트 경우
	if (attrListHeader->NonResidentFlag == 0)
	{
		//recentOffset += $AttributeHeaderSize;
		MFT_Resident* recentResident = (MFT_Resident*)(recentOffset + $AttributeHeaderSize);
		int contentLength = recentResident->SizeOfContent;
		int recentContentOffset = 0;
		recentOffset += recentResident->OffsetToContent;
		MFT_Attr_List* attrList = (MFT_Attr_List*)recentOffset;
		while (contentLength> recentContentOffset && attrList->AttrType != 0x0)
		{
			
			
			if (attrList->AttrType ==0x30)
			{
				ULONGLONG MFTReferenceEntryAddress = 0;
				memcpy(&MFTReferenceEntryAddress, attrList->FileReferenceAddr, 6);
				//해당 $ATTRIBUTE_LIST에 Reference Entry 값이 MFT_Parser::MFTdb에 없으면
				if (MFT_Parser::MFTdb.find(MFTReferenceEntryAddress) == MFT_Parser::MFTdb.end())
				{
					// 언급한 엔트리가 엔트리가 지금 엔트리가 아닐 경우
					if (MFTReferenceEntryAddress != recentEntry)
					{
						if (this->FindEntryName(MFTReferenceEntryAddress,true) == 1)
						{
							MFT_Parser::MFTdb[recentEntry].FileName = MFT_Parser::MFTdb[MFTReferenceEntryAddress].FileName;
							MFT_Parser::MFTdb[recentEntry].ParentEntyrNum = MFT_Parser::MFTdb[MFTReferenceEntryAddress].ParentEntyrNum;
							break;
						}
					}
				}
				//해당 $ATTRIBUTE_LIST에 Reference Entry 값이 MFT_Parser::MFTdb에 있으면
				else
				{
					if (MFTReferenceEntryAddress != recentEntry)
					{
						if (!MFT_Parser::MFTdb[MFTReferenceEntryAddress].FileName.empty())
						{
							MFT_Parser::MFTdb[recentEntry].FileName = MFT_Parser::MFTdb[MFTReferenceEntryAddress].FileName;
							MFT_Parser::MFTdb[recentEntry].ParentEntyrNum = MFT_Parser::MFTdb[MFTReferenceEntryAddress].ParentEntyrNum;
							break;
						}

					}
					
				}
				
			}
			recentContentOffset += attrList->EntryLength;
			recentOffset += attrList->EntryLength;
			attrList = (MFT_Attr_List*)recentOffset;
		}
	}
	// Non 레지던트 경우
	else
	{
		//Attr Header 관련 코드
		MFT_NonResident* recentNonResident = (MFT_NonResident*)(recentOffset + $AttributeHeaderSize);
		ULONGLONG VCNSize = recentNonResident->EndingVcnRunlist - recentNonResident->StartingVcnRunlist + 1;

		ULONGLONG FileSize = VCNSize * this->BytesPerCluster;
		WORD runlistOffset = recentNonResident->OffsetRunlist;

		// Runlist 관련 코드
		//recentOffset -= MFTAttrHeaderSize;
		std::vector<ULONGLONG> RunlistSizeVector;
		std::vector<ULONGLONG> RunlistOffsetVector;
		UCHAR* attrBuf = new UCHAR[FileSize];
		UCHAR* attrBufAddress = attrBuf;
		this->RunListCalc(recentOffset, runlistOffset, &RunlistSizeVector, &RunlistOffsetVector);

		// Attrbute List 읽어들여서 attrBuf 포인터 변수에 저장
		for (int i = 0; i < RunlistSizeVector.size(); i++)
		{
			//_tprintf(TEXT("RunlistSizeVector : %llu\n"), RunlistSizeVector[i]);
			//_tprintf(TEXT("RunlistOffsetVector : %llu\n"), RunlistOffsetVector[i]);
			this->ReadByte(this->hdrive, attrBuf, RunlistSizeVector[i], FILE_BEGIN, RunlistOffsetVector[i]);
			attrBuf += RunlistSizeVector[i];
		}
		attrBuf = attrBufAddress;
		MFT_Attr_List* attrList;
		int recentContentOffset = 0;
		attrList = (MFT_Attr_List*)attrBuf;
		while (FileSize > recentContentOffset && attrList->AttrType<=0x30)
		{
			

			if (attrList->AttrType == 0x30)
			{
				ULONGLONG MFTReferenceEntryAddress = 0;
				memcpy(&MFTReferenceEntryAddress, attrList->FileReferenceAddr, 6);
				//해당 키에 대한 값이 없으면
				if (MFT_Parser::MFTdb.find(MFTReferenceEntryAddress) == MFT_Parser::MFTdb.end())
				{
					// 언급한 엔트리가 엔트리가 지금 엔트리가 아닐 경우
					
					if (this->FindEntryName(MFTReferenceEntryAddress, true) == 1)
					{
						if (recentEntry == MFTReferenceEntryAddress)
						{
							break;
						}
						MFT_Parser::MFTdb[recentEntry].FileName = MFT_Parser::MFTdb[MFTReferenceEntryAddress].FileName;
						MFT_Parser::MFTdb[recentEntry].ParentEntyrNum = MFT_Parser::MFTdb[MFTReferenceEntryAddress].ParentEntyrNum;
						break;
					}
				}
			}
			recentContentOffset += attrList->EntryLength;
			attrBuf += attrList->EntryLength;
			attrList = (MFT_Attr_List*)attrBuf;
		}


		
		attrBuf = attrBufAddress;
		delete[] attrBuf;
	}
	return 1;
}
int MFT_Parser::MakeQuery(std::string &query, std::string tableName, ULONGLONG entryNum)
{
	if (query.empty() == true)
	{
		char startQuery[1024] = "";
		std::string tableSql = "INSERT INTO '%s' (%s) VALUES ";
		sprintf(startQuery, tableSql.c_str(), tableName.c_str(), MFT_Parser::tableColumn.c_str());
		query.append(startQuery);
	}
	char dataQuery[1024] = "";
	sprintf(dataQuery, "('%llu','%llu','%llu','%s'),", entryNum, MFT_Parser::FileInfo[entryNum].LSN, MFT_Parser::FileInfo[entryNum].USN, StrConvert::ReplaceAll(MFT_Parser::FileInfo[entryNum].FileName,"'","''").c_str());
	query.append(dataQuery);
	return 1;

}
std::string MFT_Parser::tableColumn="'MFTentry','LSN','USN','Path'";
std::map<ULONGLONG, Entry_Info> MFT_Parser::MFTdb;
std::map<ULONGLONG, Entry_Info> MFT_Parser::FileInfo;
UCHAR* MFT_Parser::MFTbuf;
ULONGLONG MFT_Parser::MFTEntryCount;
UCHAR* MFT_Parser::MFTstartBufOffset;
std::map<ULONGLONG, std::string> MFT_Parser::dirFullPath;
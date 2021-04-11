#pragma once

#include "stdafx.h"
#include "200NTFS.h"

#define VBR_SIZE 512
class NTFS_Parser : public MBR_Parser
{
private:

	
protected:

public:
	
	//NTFS_Parser(TCHAR* _drivename);
	NTFS_Parser();
	~NTFS_Parser(void);
	char drive[10];
	std::string driveName;


	NTFS_VBR* myNTFSVBR;
	NTFS_VBR* myNTFSVBRAddr;

	static USHORT BytesPerSector;
	static USHORT SectorsPerCluster;
	static ULONGLONG BytesPerCluster;
	static ULONGLONG TotalSector;
	static ULONGLONG StartMFT;
	static UINT MFTEntrySize;


	static HANDLE hdrive;
	void Test(void);
	int OpenDrive();
	void CloseDrive(void);
	
	int ReadVBR(HANDLE hdrive);
	void SetDrive(char _drive[]);

	// 001·Î »¬ ÇÔ¼öµé

};

#include "stdafx.h"
#include "200NTFS.h"



//NTFS_Parser::NTFS_Parser(TCHAR* _drivename)
NTFS_Parser::NTFS_Parser()
{
	
	
	//std::cout << "Constructor" << std::endl;
	//_tprintf(TEXT("Constructor2\n"));
	//std::cout << TEXT("Constructor3") << std::endl;
}
NTFS_Parser::~NTFS_Parser()
{
}
void NTFS_Parser::SetDrive(char _drive[])
{
	strcpy_s(this->drive, 10, _drive);
	//char tempDrive[4];
	//strcpy_s(tempDrive,10, &_drive[4]);
	this->driveName.append(&_drive[4]);
	this->OpenDrive();
}

void NTFS_Parser::Test(void)
{
	TCHAR driveName[] = TEXT("C:\\Users\\hfhsh\\Desktop\\aa.txt");
	HANDLE hdrive = CreateFile(
		driveName,
		GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_FLAG_NO_BUFFERING, NULL);
	if (hdrive == INVALID_HANDLE_VALUE)
	{
		std::cout << "error " << GetLastError() << std::endl;
		return;
	}
	else
	{
		this->hdrive = hdrive;
		UCHAR buf[512] = { 0 };
		DWORD readn;
		BOOL brtv;
		//brtv=ReadFile(this->hdrive, buf,SECTOR_SIZE, &readn,NULL);
		brtv = ReadFile(this->hdrive, buf, 5, &readn, NULL);
		//ReadFile(hdrive, buf, 5, &readn, NULL);
		std::cout << "driveOpen " << std::endl;
		return;
	}
	return;
}

int NTFS_Parser::OpenDrive()
{

	HANDLE hdrive = CreateFileA(
		(LPCSTR)(this->drive),
		//L"\\\\.\\C:",
		//TEXT("C:\\Users\\hfhsh\\Desktop\\aa.txt"),
		GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);

	if (hdrive == INVALID_HANDLE_VALUE)
	{
		std::cout << "error " << GetLastError() << std::endl;
		return 0;
	}
	else
	{
		this->hdrive = hdrive;
		return 1;
	}
}

void NTFS_Parser::CloseDrive(void)
{
	CloseHandle(this->hdrive);
}

int NTFS_Parser::ReadVBR(HANDLE hdrive)
{
	UCHAR buf[VBR_SIZE] = { 0, };
	//this->OpenDrive();
	this->Read512Byte(hdrive,buf);
	this->myNTFSVBR = new NTFS_VBR;
	NTFS_VBR* myNTFSVBRAddr = myNTFSVBR;
	this->myNTFSVBR = (NTFS_VBR*)buf;
	int len = sizeof(this->myNTFSVBR->BytesPerSector);
	this->BytesPerSector = (USHORT)this->LittleE2Dec(this->myNTFSVBR->BytesPerSector, sizeof(this->myNTFSVBR->BytesPerSector));
	this->SectorsPerCluster = (USHORT)this->LittleE2Dec(this->myNTFSVBR->SectorsPerCluster, sizeof(this->myNTFSVBR->SectorsPerCluster));
	this->TotalSector = this->LittleE2Dec(this->myNTFSVBR->TotalSectors, sizeof(this->myNTFSVBR->TotalSectors));
	this->BytesPerCluster = this->SectorsPerCluster * this->BytesPerSector;
	this->StartMFT = this->LittleE2Dec(this->myNTFSVBR->LCN_MFT, sizeof(this->myNTFSVBR->LCN_MFT)) * this->BytesPerCluster;
	//this->StartMFT = this->StartMFT * this->BytesPerCluster;
	this->MFTEntrySize = pow(2, (~((char)this->LittleE2Dec(this->myNTFSVBR->MFTEntrySize, sizeof(this->myNTFSVBR->MFTEntrySize)))) + 1);
	//_tprintf(TEXT("BytesPerCluster : %llu \n"), this->BytesPerCluster);
	myNTFSVBR = myNTFSVBRAddr;
	delete myNTFSVBR;
	return 0;

}
USHORT NTFS_Parser::BytesPerSector;
USHORT NTFS_Parser::SectorsPerCluster;
ULONGLONG NTFS_Parser::BytesPerCluster;
ULONGLONG NTFS_Parser::TotalSector;
ULONGLONG NTFS_Parser::StartMFT;
UINT NTFS_Parser::MFTEntrySize;
HANDLE NTFS_Parser::hdrive;
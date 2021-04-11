#pragma once
#include "stdafx.h"

#pragma pack(push, 1)
typedef struct _NTFS_VBR
{
	UCHAR JumpCommand[3];
	UCHAR OEM_ID[8];
	UCHAR BytesPerSector[2];
	UCHAR SectorsPerCluster[1];
	UCHAR Reserved_1[2];
	UCHAR Always0_1[3];
	UCHAR Unused_1[2];
	UCHAR MediaDescriptor[1];
	UCHAR Always0_2[2];
	UCHAR SectorPerTrack[2];
	UCHAR NumberOfHeads[2];
	UCHAR HiddenSectors[4];
	UCHAR Unused_2[8];
	UCHAR TotalSectors[8];
	UCHAR LCN_MFT[8];
	UCHAR LCN_MFTMir[8];
	UCHAR MFTEntrySize[1];
	UCHAR Reserved_3[3];
	UCHAR ClustersPerIndexBlock[4];
	UCHAR VolumeSerialNumber[8];
	UCHAR CheckSum[4];
	UCHAR BootCodeAndErrorMessage[426];
	UCHAR Signature[2];
}NTFS_VBR;


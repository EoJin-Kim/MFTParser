
#include "stdafx.h"


// 현재 바이트 읽을때 offset 이도시 현재 위치기준으로 이동 후 읽는다. 
int _tmain()
{


    clock_t start, end;
    double totalTime;
    //TCHAR driveName[] = TEXT("\\\\.\\PhysicalDrive0");
    char driveName[] = "\\\\.\\C:";

    MFT_Parser* myDrive = new MFT_Parser();
    myDrive->SetDrive(driveName);
    myDrive->ReadMFT(myDrive->hdrive);
    myDrive->FixUpArrayRecover();

    start = clock();
    myDrive->MFTFileList(myDrive->hdrive);

    
    /*
    int recent = 849332;
    int parent = 0;
    while (true)
    {
        if (recent == 5)
        {
            _tprintf(TEXT("Root\n"));
            break;
        }
        _tprintf(TEXT("Directory Name : %s Parent : 0x%llX\n"), MFT_Parser::MFTdb[recent].FileName.c_str(), MFT_Parser::MFTdb[recent].ParentEntyrNum);
        recent = MFT_Parser::MFTdb[recent].ParentEntyrNum;
    }
    */


    /*
    
    HANDLE wFile;
    char wfName[20] = "error.txt";
    wFile = CreateFileA(wfName, GENERIC_READ | GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);

    if (wFile == INVALID_HANDLE_VALUE)
    {
        
        return 0;
    }
    */
    MFT_Parser* MFTtmp = new MFT_Parser();
    MFTtmp->MFTTraversal(0, MFT_Parser::MFTEntryCount);
    //MFTtmp->MFTTraversal(51594, MFT_Parser::MFTEntryCount, wFile);

    /*
    for (auto it : MFT_Parser::MFTdb) {
        _tprintf(TEXT("Entry : %llu\t %s\n"), it.first, it.second.FileName.c_str());

    }
    */
    
    myDrive->DirFullPath();
    myDrive->FileFullPath();

    // Sqlite에 데이터 넣기\

    
    MySqlite* mySqlite = new MySqlite();

    std::string testdb = "file.db";
    mySqlite->CreateDB(testdb);

    std::string tableName = "FileInfo";
    
    mySqlite->CreaetTable(tableName);
    
    int count = 0;
    std::string query ;
    for (auto it : MFT_Parser::FileInfo) {
        count += 1;
        myDrive->MakeQuery(query, tableName,it.first);
        if (count == 100)
        {
            //StrConvert::ReplaceAll(query, "'", "''");
            mySqlite->StringExec(tableName, query);
            count = 0;
            query.clear();
        }
    }
    if (!query.empty())
    {
        //StrConvert::ReplaceAll(query, "'", "''");
        mySqlite->StringExec(tableName, query);
        query.clear();
    }
    //_tprintf(TEXT("FileName : %s\n"), MFT_Parser::MFTdb[1].FileName.c_str());
    mySqlite->CloseDB();
     myDrive->CloseDrive();
    
    end = clock(); //시간 측정 끝
    totalTime = (double)(end - start)/CLOCKS_PER_SEC;
    printf("%f", totalTime);
}

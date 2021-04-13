
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
    _tprintf(TEXT("MFT Loading\n"));
    myDrive->ReadMFT(myDrive->hdrive);
    myDrive->FixUpArrayRecover();

    start = clock();
    myDrive->MFTFileList(myDrive->hdrive);

    



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
    _tprintf(TEXT("MFT Traversal\n"));
    MFTtmp->MFTTraversal(0, MFT_Parser::MFTEntryCount);
    //MFTtmp->MFTTraversal(51594, MFT_Parser::MFTEntryCount, wFile);

    /*
    for (auto it : MFT_Parser::MFTdb) {
        _tprintf(TEXT("Entry : %llu\t %s\n"), it.first, it.second.FileName.c_str());

    }
    */
    delete[] MFT_Parser::MFTbuf;
    _tprintf(TEXT("MFT Dir Full Path\n"));
    myDrive->DirFullPath();
    _tprintf(TEXT("MFT File Full Path\n"));
    myDrive->FileFullPath();
    
    // Sqlite에 데이터 넣기
    /*
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
    */

    // sqlite에 저장된 MFT 정보 갖고오기
    MySqlite * sqlTest = new MySqlite();
    std::string testdb2 = "file.db";
    std::string tableName2 = "FileInfo";
    sqlTest->CreateDB(testdb2);
    // 테이블이 존재하는 경우
    if (sqlTest->CheckTable(tableName2))
    {
        _tprintf(TEXT("SQLITE Loading\n"));
        sqlTest->SelectTable(tableName2, MFT_Parser::SelectCallback);
        _tprintf(TEXT("Modify And Create File Find\n"));
        myDrive->ModifyAndCreateFileFind();
    }
    // 테이블이 존재하지 않는 경우
    else
    {
        _tprintf(TEXT("No Exist\n"));
    }


     myDrive->CloseDrive();
    
    end = clock(); //시간 측정 끝
    totalTime = (double)(end - start)/CLOCKS_PER_SEC;
    printf("%f", totalTime);
}

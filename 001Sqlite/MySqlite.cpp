#include "stdafx.h"
#include "MySqlite.h"
/*
int main()
{
	std::cout << "Hello World!\n";
	sqlite3* db;
	char* err_msg = 0;
	wchar_t* err_wmsg = 0;
	int rc;
	const char* tableSql;
	//std::string query;
	wchar_t query[1024] = L"";
	TCHAR a;

	swprintf(query, L"�� ���� %d���Դϴ�.", 1000);
	// Open database
	

	tableSql = "DROP TABLE IF EXISTS MFTpath;"
		"CREATE TABLE MFTpath(MFTentry INT, Path TEXT);";
	rc = sqlite3_exec(db, tableSql, 0, 0, &err_msg);
	if (rc != SQLITE_OK)
	{
		fprintf(stderr, "SQL error: %s\n", err_msg);

		sqlite3_free(err_msg);
		sqlite3_close(db);

		return 1;
	}
	std::wstring filePath = L"C:/Users/hfhsh/Downloads/sqlite_unicode (2)";
	swprintf(query, L"INSERT INTO MFTpath VALUES(%d, '%s');", 1, filePath.c_str());
	rc = sqlite3_exec16(db, query, 0, 0, &err_wmsg);
	if (rc != SQLITE_OK)
	{
		fprintf(stderr, "SQL error: %s\n", err_wmsg);

		sqlite3_free(err_wmsg);
		sqlite3_close(db);

		return 1;
	}
}
*/
int MySqlite::CreateDB(std::string dbName)
{
	int rc;
	const char* tableSql;
	rc = sqlite3_open(dbName.c_str(), &this->db);

	if (rc) {
		fprintf(stderr, "Can't open database: %s\n", sqlite3_errmsg(this->db));
		sqlite3_close(this->db);
		return 0;
	}
	else {
		fprintf(stderr, "Opened database successfully\n");
		return 1;
	}
	
}

int MySqlite::CloseDB()
{
	sqlite3_close(db);
	return 1;
}
int MySqlite::CreaetTable(std::string tableName)
{
	int rc;
	char* err_msg = 0;
	char query[1024] = "";
	std::string tableSql ="DROP TABLE IF EXISTS %s;CREATE TABLE %s(%s);";
	sprintf(query, tableSql.c_str(), tableName.c_str(), tableName.c_str(), MySqlite::MFTColumn.c_str());
	rc = sqlite3_exec(this->db, query, 0, 0, &err_msg);
	if (rc != SQLITE_OK)
	{

		fprintf(stderr, "SQL error: %s\n", err_msg);

		sqlite3_free(err_msg);
		sqlite3_close(db);

		return 0;
	}
}

int MySqlite::StringExec(std::string tableName, std::string &queryString)
{
	//
	int rc;
	char* err_msg = 0;
	//std::string testQuery = "INSERT INTO 'testTable12' ('MFTentry','LSN','Path') VALUES ('0','3108233476580','$MFT'),('1','33560283','$MFTMirr')";

	queryString = queryString.substr(0, queryString.length() - 1);
	
	rc = sqlite3_exec(db, queryString.c_str(), 0, 0, &err_msg);
	if (rc != SQLITE_OK)
	{
		HANDLE File;
		DWORD dwWrite;
		char fName[20] = "error.txt";
		File = CreateFileA(fName, GENERIC_READ | GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);

		if (File == INVALID_HANDLE_VALUE)
		{

			return 0;
		}
		WriteFile(File, queryString.c_str(), queryString.length(), &dwWrite, NULL);
		CloseHandle(File);
		fprintf(stderr, "SQL error: %s\n", err_msg);

		sqlite3_free(err_msg);
		sqlite3_close(db);

		return 0;
	}

	return 1;
}



std::string MySqlite::MFTColumn = "\
	MFTentry INT,\
	LSN INT,\
	USN INT,\
	Path TEXT\
	";
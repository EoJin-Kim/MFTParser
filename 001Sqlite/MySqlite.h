#pragma once
#include "stdafx.h"
class MySqlite
{
private:

public:
	static std::string MFTColumn;

	sqlite3* db;
	int CreateDB(std::string dbName);
	int CloseDB();

	int CreaetTable(std::string tableName);
	int WstringExec(std::wstring queryString);
	int StringExec(std::string tableName, std::string &queryString);
	

protected:
};
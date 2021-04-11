#include "stdafx.h"


class StrConvert
{
private:

protected:

public:
	static std::string WstrToUtf8(LPCWSTR wszWideFilename);
	static std::string ReplaceAll(std::string& str, const std::string& from, const std::string& to);
};
#include "stdafx.h"
#include "StrConvert.h"

std::string StrConvert::WstrToUtf8(LPCWSTR wszWideFilename)
{
    int nByte;
    char* szFilename;
    std::string strOutput;



    nByte = WideCharToMultiByte(CP_UTF8, 0, wszWideFilename, -1, 0, 0, 0, 0);
    if (nByte == 0) {
        return 0;
    }
    szFilename = (char*)malloc(nByte);
    if (szFilename == 0) {
        return 0;
    }



    nByte = WideCharToMultiByte(CP_UTF8, 0, wszWideFilename, -1, szFilename, nByte, 0, 0);
    if (nByte == 0) {
        free(szFilename);
        szFilename = 0;
    }
    if (NULL != szFilename)
    {
        strOutput = szFilename;
        free(szFilename);
        szFilename = 0;
    }



    return strOutput;
}

std::string StrConvert::ReplaceAll(std::string& str, const std::string& from, const std::string& to) 
{
    size_t start_pos = 0; //string처음부터 검사
    while ((start_pos = str.find(from, start_pos)) != std::string::npos)  //from을 찾을 수 없을 때까지
    {
        str.replace(start_pos, from.length(), to);
        start_pos += to.length(); // 중복검사를 피하고 from.length() > to.length()인 경우를 위해서
    }
    return str;
}
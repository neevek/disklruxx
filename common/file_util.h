/*******************************************************************************
**          File: file_util.h
**        Author: neevek <i@neevek.net>.
** Creation Time: 2014-10-24 Fri 01:31 PM
**   Description: some utility methods to handle files
*******************************************************************************/
#ifndef FILE_UTIL_H_
#define FILE_UTIL_H_
#include <string>

class FileUtil {
 public:
   static std::string ReadFileAsString(const std::string &path);
   static void WriteStringToFile(const std::string &content, 
       const std::string &path);

   static bool FileExists(const std::string &path);
   static bool DirExists(const std::string &path);
   static bool MakeDir(const std::string &path);
   static bool MakeDirs(const std::string path);
   static bool DeleteFile(const std::string &path);
   static long GetFileSize(const std::string &path);
};

#endif /* end of include guard: FILE_UTIL_H_ */


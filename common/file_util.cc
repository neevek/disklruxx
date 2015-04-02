/*******************************************************************************
**          File: file_util.cc
**        Author: neevek <i@neevek.net>.
** Creation Time: 2014-10-24 Fri 01:31 PM
**   Description: some utility methods to handle files
*******************************************************************************/
#include "file_util.h"
#include <fstream>
#include <sys/stat.h> /* for lstat */
#include <cerrno>
#include <cstdio>
#include "log/log.h"

std::string FileUtil::ReadFileAsString(const std::string &path) {
  std::string data;
  std::ifstream fin(path, std::ios::binary);

  int buf_size = 1024;
  char buf[buf_size + 1];

  while(fin.good()) {
    fin.read(buf, buf_size);
    int count = fin.gcount();
    if (count > 0) {
      data.append(buf, count);
    } else {
      break;
    }
  }

  fin.close();

  return data;
}

void FileUtil::WriteStringToFile(const std::string &content, 
    const std::string &path) {

  std::ofstream fout(path, std::ios::binary);

  if (fout.good()) {
    fout << content;
  }

  fout.close();
}


bool FileUtil::FileExists(const std::string &path) {
  struct stat stat_buf;
  if (lstat(path.c_str(), &stat_buf) == 0) {
    return (stat_buf.st_mode & S_IFMT) == S_IFREG;
  //} else {
    //LOG_E("FileUtil", "failed to read file status with lstat");
  }

  return false;
}

bool FileUtil::DirExists(const std::string &path) {
  struct stat stat_buf;
  if (::lstat(path.c_str(), &stat_buf) == 0) {
    return (stat_buf.st_mode & S_IFMT) == S_IFDIR;
  //} else {
    //LOG_E("FileUtil", "failed to read file status with lstat");
  }

  return false;
}

bool FileUtil::MakeDir(const std::string &path) {
  if (!FileUtil::DirExists(path)) {
    int ret = ::mkdir(path.c_str(), S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
    if (ret != 0 && errno != EEXIST) {
      LOG_E("FileUtil", "failed to create path: %s", path.c_str());
      return false;
    }   

    LOG_D("FileUtil", "created path: %s", path.c_str());
    return true;
  }

  return false;
}

bool FileUtil::MakeDirs(const std::string path) {
  if (path.size() <= 0) {
    return false;
  }

  const char *dir = path.c_str();

  char *ch = (char *)dir;
  char *last_ch = ch;

  while (*ch == '/') {
    ++ch;
  }   

  while ((ch = strchr(ch, '/'))) {
    while (*(ch + 1) == '/') {
      ++ch;
    }
    *ch = '\0';
    int ret = ::mkdir(dir, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
    if (ret != 0 && errno != EEXIST) {
      LOG_E("FileUtil", "failed to create dir '%s': %s", 
          dir, strerror(errno));

      *ch = '/';
      return false;
    }   

    *ch = '/';
    ++ch;
    last_ch = ch; 
  }   

  int ret = ::mkdir(dir, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
  if (*last_ch != '\0' && ret != 0 && errno != EEXIST) {
    LOG_E("FileUtil", "failed to create dir '%s': %s", dir, strerror(errno));
    return false;
  }   

  return true;
}

bool FileUtil::DeleteFile(const std::string &path) {
  if (FileExists(path) && std::remove(path.c_str()) != 0) {
    LOG_E("FileUtil", "failed to delete file: %s", path.c_str());
    return false;
  }

  return true;
}

long FileUtil::GetFileSize(const std::string &path) {
  struct stat stat_buf;
  int rc = stat(path.c_str(), &stat_buf);
 return rc == 0 ? stat_buf.st_size : -1;
}


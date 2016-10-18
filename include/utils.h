#ifndef UTILS_H
#define UTILS_H
#include "base.h"

std::string readFile(const std::string &fileName);

bool isFileExist(const char *fileName);

bool setupLogger();

#endif

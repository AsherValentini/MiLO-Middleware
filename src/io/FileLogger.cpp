#include "io/FileLogger.hpp"
#include <cstdio>

using namespace milo::io;

FileLogger::~FileLogger() { close(); }

bool FileLogger::open(const std::string&) { return true; }
void FileLogger::write(const std::string&) {}
bool FileLogger::flush() { return true; }
void FileLogger::close() { fp_ = nullptr; }


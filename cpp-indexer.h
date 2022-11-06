#pragma once
#include <clang-c/CXString.h>
#include <clang-c/Index.h>
#include <string>

using String = std::string;

String GetCursorSourcefile(CXCursor Cursor);
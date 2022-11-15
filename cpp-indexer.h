#pragma once
#include <clang-c/CXString.h>
#include <clang-c/Index.h>
#include <string>

using String = std::string;

struct SourceLocation {
    String file;
    unsigned line;
    unsigned column;
    unsigned offset;
    mutable String temp;
    mutable String temp2;
    const char* c_str() const {
        temp = file + ":" + std::to_string(line) + ":" + std::to_string(column) + ":" + std::to_string(offset);
        return temp.c_str();
    }
    const char* line_col_offset_c_str() const {
        temp2 = std::to_string(line) + ":" + std::to_string(column) + ":" + std::to_string(offset);
        return temp2.c_str();
    }
};

SourceLocation GetCursorSourceLocation(CXCursor Cursor);
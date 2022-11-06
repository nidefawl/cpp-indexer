#include "cpp-indexer.h"

String GetCursorSourcefile(CXCursor Cursor) {
  CXSourceLocation Loc = clang_getCursorLocation(Cursor);
  CXString source;
  CXFile file;
  clang_getExpansionLocation(Loc, &file, 0, 0, 0);
  source = clang_getFileName(file);
  if (!clang_getCString(source)) {
    clang_disposeString(source);
    return "<invalid loc>";
  }
  else {
    String s = clang_getCString(source);
    clang_disposeString(source);
    return s;
  }
}
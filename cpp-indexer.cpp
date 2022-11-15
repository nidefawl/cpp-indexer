#include "cpp-indexer.h"

SourceLocation GetCursorSourceLocation(CXCursor Cursor) {
  SourceLocation srcLoc{};
  CXFile file_cx{};
  auto loc_cx = clang_getCursorLocation(Cursor);
  clang_getExpansionLocation(loc_cx, &file_cx, &srcLoc.line, &srcLoc.column, &srcLoc.offset);
  CXString source_cx = clang_getFileName(file_cx);
  auto szSource = clang_getCString(source_cx);
  if (!szSource) {
    srcLoc.file = "<invalid file>";
  } else {
    srcLoc.file = szSource;
  }
  clang_disposeString(source_cx);
  return srcLoc;
}
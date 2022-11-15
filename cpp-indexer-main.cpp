/* c-index-test.c */
#include "cpp-indexer.h"
#include <cassert>
#include <clang-c/BuildSystem.h>
#include <clang-c/CXCompilationDatabase.h>
#include <clang-c/CXString.h>
#include <clang-c/Documentation.h>
#include <clang-c/Index.h>
#include <clang/Config/config.h>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cctype>
#include <map>
#include <string>
#include <vector>
#ifdef _WIN32
#include <direct.h>
#else
#include <unistd.h>
#endif

String fully_qualified(CXCursor Cursor) {
    if (clang_Cursor_isNull(Cursor)) {
        return "";
    }
    auto kind = clang_getCursorKind(Cursor);
    if (kind == CXCursor_TranslationUnit) {
        return "";
    }
    String res = fully_qualified(clang_getCursorSemanticParent(Cursor));
    if (res != "") {
        res += "::";
    }
    if (kind == CXCursor_Namespace && clang_Cursor_isAnonymous(Cursor)) {
        res += "<anonymous namespace>";
    }
    else {
        res += clang_getCString(clang_getCursorSpelling(Cursor));
    }
    return res;
}

struct ClassDef {
    String strName;
    String strNamespace;
    SourceLocation srcLoc;
    int64_t nSize;
    std::vector<String> vecBaseClasses;
};
struct FunctionDef {
    String strName;
    String strNamespace;
    SourceLocation srcLoc;
    int64_t nStatic;
    String strSig;
    String strReturnType;
    std::vector<String> vecParams;
};

int ProcessClassName(CXCursor Cursor, ClassDef& classDef) {
    CXCursor td_def = clang_getCursorDefinition(Cursor);
    if (clang_Cursor_isNull(td_def)) {
        return 1;
    }
    auto typeName_cx = clang_getCursorSpelling(Cursor);
    String strName = clang_getCString(typeName_cx);
    classDef.strNamespace = fully_qualified(clang_getCursorSemanticParent(Cursor));
    auto fqn = strName;
    if (classDef.strNamespace != "") {
        fqn = classDef.strNamespace + "::" + fqn;
    }
    classDef.strName = fqn;
    classDef.srcLoc = GetCursorSourceLocation(Cursor);
    clang_disposeString(typeName_cx);
    return 0;
}

int ProcessClassDef(CXCursor Cursor, ClassDef& classDef) {
    classDef.nSize = clang_Type_getSizeOf(clang_getCursorType(Cursor));
    std::vector <String> vecBaseClasses;
    CXCursorVisitor visitor = [](CXCursor cursor, CXCursor parent, CXClientData client_data) -> CXChildVisitResult {
        auto kind = clang_getCursorKind(cursor);
        if (kind == CXCursor_CXXBaseSpecifier) {
            auto base = clang_getCursorReferenced(cursor);
            auto baseName_cx = clang_getCursorSpelling(base);
            String baseName = clang_getCString(baseName_cx);
            auto baseNamespace = fully_qualified(clang_getCursorSemanticParent(base));
            auto fqn = baseName;
            if (baseNamespace != "") {
                fqn = baseNamespace + "::" + fqn;
            }
            auto vecBaseClasses = static_cast<std::vector <String> *>(client_data);
            vecBaseClasses->push_back(fqn);
            clang_disposeString(baseName_cx);
        }
        return CXChildVisit_Continue;
    };
    clang_visitChildren(Cursor, visitor, &classDef.vecBaseClasses);
    return 0;
}

void PrintClassDef(const ClassDef& classDef) {
    // if (classDef.vecBaseClasses.empty())
    //     return;
    printf("name: '%s', namespace: '%s', location: '%s', size: %zd\n",
        classDef.strName.c_str(), classDef.strNamespace.c_str(), classDef.srcLoc.c_str(), classDef.nSize);
    printf("name: '%s' has %zu base classes:", classDef.strName.c_str(), classDef.vecBaseClasses.size());
    for (auto& base : classDef.vecBaseClasses) {
        printf(" '%s'", base.c_str());
    }
    printf("\n----\n");
}

enum CXChildVisitResult FunctionVisitor(CXCursor Cursor,
                                        CXCursor Parent,
                                        CXClientData ClientData) {
    auto pFuncDef = static_cast<FunctionDef*>(ClientData);
    if (clang_getCursorKind(Cursor) == CXCursor_ParmDecl) {
        auto paramType = clang_getCursorType(Cursor);
        auto paramTypename_cx = clang_getTypeSpelling(paramType);
        // append variable name
        auto paramName_cx = clang_getCursorSpelling(Cursor);
        pFuncDef->vecParams.emplace_back(clang_getCString(paramTypename_cx) + String(" ") + clang_getCString(paramName_cx));
        clang_disposeString(paramTypename_cx);
    }
    return CXChildVisit_Continue;
}
int ProcessFunction(CXCursor Cursor, FunctionDef& classDef) {
    CXCursor td_def = clang_getCursorDefinition(Cursor);
    if (clang_Cursor_isNull(td_def)) {
        return 1;
    }
    auto typeName_cx = clang_getCursorSpelling(Cursor);
    String strName = clang_getCString(typeName_cx);
    classDef.strNamespace = fully_qualified(clang_getCursorSemanticParent(Cursor));
    auto fqn = strName;
    if (classDef.strNamespace != "") {
        fqn = classDef.strNamespace + "::" + fqn;
    }
    classDef.strName = fqn;
    classDef.srcLoc = GetCursorSourceLocation(Cursor);
    classDef.nStatic = clang_CXXMethod_isStatic(Cursor);
    auto resultType = clang_getCursorResultType(Cursor);
    auto resultTypename_cx = clang_getTypeSpelling(resultType);
    classDef.strReturnType = clang_getCString(resultTypename_cx);
    clang_disposeString(resultTypename_cx);
    clang_disposeString(typeName_cx);
    // iterate over children
    clang_visitChildren(Cursor, FunctionVisitor, &classDef);
    String funcSig = classDef.strReturnType + " " + fqn + "(";
    for (auto& param : classDef.vecParams) {
        funcSig += param;
        if (&param != &classDef.vecParams.back()) {
            funcSig += ", ";
        }
    }
    funcSig += ")";
    classDef.strSig = funcSig;
    return 0;
}
void PrintFunction(const FunctionDef& funcDef) {
    printf("name: '%s', namespace: '%s', location: '%s', static: %zd, sig: '%s', return_type: '%s', params: %zu\n",
        funcDef.strName.c_str(), funcDef.strNamespace.c_str(), funcDef.srcLoc.c_str(), funcDef.nStatic, funcDef.strSig.c_str(), funcDef.strReturnType.c_str(), funcDef.vecParams.size());
}
/* Data used by the visitor */
using VisitorData = struct {
    const char* szSrcPath;
    CXTranslationUnit TU;
    std::map<String, ClassDef> VecClassDefs;
    std::map<String, FunctionDef> VecFuncDefs;
    std::vector<String> ExcludeFileMatch;
};
void WriteCsvFile(const VisitorData& UserData, const String& PathFileOut) {
    std::array fNames = { "class", "function" };

    String fileOutFunc = PathFileOut + ".func.csv";
    String fileOutClass = PathFileOut + ".class.csv";
    // sort by name first
    std::vector<std::pair<String, FunctionDef>> vecFuncDefs(UserData.VecFuncDefs.begin(), UserData.VecFuncDefs.end());
    std::vector<std::pair<String, ClassDef>> vecClassDefs(UserData.VecClassDefs.begin(), UserData.VecClassDefs.end());
    {
        FILE* fp = fopen(fileOutFunc.c_str(), "w");
        if (fp == nullptr) {
            fprintf(stderr, "Failed to open file '%s' for writing\n", fileOutFunc.c_str());
            return;
        }
        fprintf(fp, "name,namespace,file,location,static,sig,return_type,params\n");
        std::sort(vecFuncDefs.begin(), vecFuncDefs.end(), [](const auto& a, const auto& b) {
            return a.first < b.first;
        });
        for (auto& [name, funcDef] : vecFuncDefs) {
            fprintf(fp, "\"%s\",\"%s\",\"%s\",\"%s\",%zd,\"%s\",\"%s\",%zu\n",
                funcDef.strName.c_str(), funcDef.strNamespace.c_str(), funcDef.srcLoc.file.c_str(), funcDef.srcLoc.line_col_offset_c_str(), funcDef.nStatic, funcDef.strSig.c_str(), funcDef.strReturnType.c_str(), funcDef.vecParams.size());
        }
        fclose(fp);
    }
    {
        FILE* fp = fopen(fileOutClass.c_str(), "w");
        if (fp == nullptr) {
            fprintf(stderr, "Failed to open file '%s' for writing\n", fileOutClass.c_str());
            return;
        }
        fprintf(fp, "name,namespace,baseclasses,file,location,size\n");
        std::sort(vecClassDefs.begin(), vecClassDefs.end(), [](const auto& a, const auto& b) {
            return a.first < b.first;
        });
        
        for (auto& [name, classDef] : vecClassDefs) {
            String baseClasses = "";
            for (auto& baseClass : classDef.vecBaseClasses) {
                baseClasses += baseClass;
                if (&baseClass != &classDef.vecBaseClasses.back()) {
                    baseClasses += ";";
                }
            }
            fprintf(fp, "\"%s\",\"%s\",\"%s\",\"%s\",\"%s\",%zu\n",
                classDef.strName.c_str(), classDef.strNamespace.c_str(), baseClasses.c_str(), classDef.srcLoc.file.c_str(), classDef.srcLoc.line_col_offset_c_str(), classDef.nSize);
        }
        fclose(fp);
    }
}
enum CXChildVisitResult ClassAndFunctionVisitor(CXCursor Cursor,
                                                CXCursor Parent,
                                                CXClientData ClientData) {
    // check if we deal with a forward declaration
    if (clang_isCursorDefinition(Cursor) == 0) {
        return CXChildVisit_Continue;
    }
    CXSourceLocation Loc = clang_getCursorLocation(Cursor);
    CXFile file{};
    clang_getExpansionLocation(Loc, &file, nullptr, nullptr, nullptr);
    if (!file) {
      return CXChildVisit_Continue;
    }
    auto UserData = reinterpret_cast<VisitorData*>(ClientData);
    auto fname = clang_getFileName(file);
    String fname_str = !clang_getCString(fname) ? "<invalid loc>" : clang_getCString(fname);
    clang_disposeString(fname);
    if (fname_str.empty() || !UserData->szSrcPath || std::strncmp(fname_str.c_str(), UserData->szSrcPath, std::strlen(UserData->szSrcPath)) != 0) {
        return CXChildVisit_Continue;
    }
    for (auto& excludeDir : UserData->ExcludeFileMatch) {
        if (fname_str.find(excludeDir) != String::npos) {
            return CXChildVisit_Continue;
        }
    }
    if (Cursor.kind == CXCursor_ClassDecl ||
        Cursor.kind == CXCursor_StructDecl ||
        Cursor.kind == CXCursor_ClassTemplate/* ||
        Cursor.kind == CXCursor_EnumDecl   ||
        Cursor.kind == CXCursor_UnionDecl ||
        Cursor.kind == CXCursor_TypedefDecl */) {
        ClassDef c{};
        if (0 == ProcessClassName(Cursor, c)) {
            // remove srcpath from c.strFile if strFile starts with srcpath
            if (UserData->szSrcPath && !std::strncmp(c.srcLoc.file.c_str(), UserData->szSrcPath, std::strlen(UserData->szSrcPath))) {
                c.srcLoc.file = c.srcLoc.file.substr(std::strlen(UserData->szSrcPath));
                if (c.srcLoc.file[0] == '/') {
                    c.srcLoc.file = c.srcLoc.file.substr(1);
                }
            }
            ProcessClassDef(Cursor, c);
            auto it = UserData->VecClassDefs.find(c.strName);
            if (it == UserData->VecClassDefs.end() || it->second.nSize <= 0) {
                if (c.vecBaseClasses.size()) PrintClassDef(c);
                UserData->VecClassDefs[c.strName] = c;
            }
        }
        return CXChildVisit_Recurse;
    }
    if (Cursor.kind == CXCursor_FunctionDecl ||
        Cursor.kind == CXCursor_FunctionTemplate) {
        FunctionDef f{};
        if (0 == ProcessFunction(Cursor, f)) {
            // PrintFunction(f);
            // remove srcpath from c.strFile if strFile starts with srcpath
            if (UserData->szSrcPath && !std::strncmp(f.srcLoc.file.c_str(), UserData->szSrcPath, std::strlen(UserData->szSrcPath))) {
                f.srcLoc.file = f.srcLoc.file.substr(std::strlen(UserData->szSrcPath));
                if (f.srcLoc.file[0] == '/') {
                    f.srcLoc.file = f.srcLoc.file.substr(1);
                }
            }
            UserData->VecFuncDefs[f.strName] = f;
        }
        return CXChildVisit_Recurse;
    }
    return CXChildVisit_Recurse;

    // PrintCursor(Cursor, Data->CommentSchemaFile);
    // PrintCursorExtent(Cursor);
    // if (clang_isDeclaration(Cursor.kind)) {
    //     enum CX_CXXAccessSpecifier access = clang_getCXXAccessSpecifier(Cursor);
    //     const char* accessStr = nullptr;
    //     switch (access) {
    //         case CX_CXXInvalidAccessSpecifier:
    //             break;
    //         case CX_CXXPublic:
    //             accessStr = "public";
    //             break;
    //         case CX_CXXProtected:
    //             accessStr = "protected";
    //             break;
    //         case CX_CXXPrivate:
    //             accessStr = "private";
    //             break;
    //     }

    //     if (accessStr)
    //         printf(" [access=%s]", accessStr);
    // }
}
enum {
    MAX_COMPILE_ARGS = 512
};

static unsigned CreateTranslationUnit(CXIndex Idx, const char *file,
                                      CXTranslationUnit *TU) {
  enum CXErrorCode Err = clang_createTranslationUnit2(Idx, file, TU);
  if (Err != CXError_Success) {
    fprintf(stderr, "Unable to load translation unit from '%s'!\n", file);
    *TU = nullptr;
    return 0;
  }
  return 1;
}
int main(int argc, char* argv[]) {
    CXString cxargs[MAX_COMPILE_ARGS];
    const char* args[MAX_COMPILE_ARGS];
    // if (argc < 2)
    //     printf("Usage: %s <builddir>\n", argv[0]);

    const char* szBuildDir = argc > 1 ? argv[1] : "/data/dev/daw/build";
    const char* szSrcPath = argc > 2 ? argv[2] : "/data/dev/daw/src";
    if (!szBuildDir || !szSrcPath) {
        printf("Usage: %s <builddir> <srcpath>\n", argv[0]);
        return 1;
    }
    fprintf(stdout, "Build dir: %s\n", szBuildDir);
    fprintf(stdout, "Src path: %s\n", szSrcPath);
    String outFilePath = String(getcwd(nullptr, 0)) + "/cpp-index";
    const bool bFullScan = true;
    CXIndex Idx = clang_createIndex(/* excludeDeclsFromPCH */ 1,
                                    /* displayDiagnostics=*/1);
    if (!Idx) {
        fprintf(stderr, "Could not create Index\n");
        return 1;
    }
    CXCompilationDatabase_Error ec{};
    auto db = clang_CompilationDatabase_fromDirectory(szBuildDir, &ec);
    if (!db || ec != CXCompilationDatabase_NoError) {
        fprintf(stderr, "Could not load compilation database from %s\n",
                szBuildDir);
        return 1;
    }
    auto idxAction = clang_IndexAction_create(Idx);

    if (chdir(szBuildDir) != 0) {
        fprintf(stderr, "Could not chdir to %s\n", szBuildDir);
        return 1;
    }
    fprintf(stdout, "Current dir: %s\n", getcwd(nullptr, 0));
    unsigned numCommands = 0;
    auto CCmds           = clang_CompilationDatabase_getAllCompileCommands(db);
    if (!CCmds || (numCommands = clang_CompileCommands_getSize(CCmds)) == 0) {
        fprintf(stderr, "compilation db is empty %s\n", szBuildDir);
        return 1;
    }

    VisitorData Data{};
    Data.ExcludeFileMatch.emplace_back("/thirdparty/");
    Data.ExcludeFileMatch.emplace_back("/tests/");
    Data.ExcludeFileMatch.emplace_back("/nanovg/");

    int errorCode = 0;
    for (unsigned i = 0; i < numCommands && errorCode == 0; ++i) {
        auto CCmd = clang_CompileCommands_getCommand(CCmds, i);

        auto wd = clang_CompileCommand_getDirectory(CCmd);
        if (chdir(clang_getCString(wd)) != 0) {
            printf("Could not chdir to %s\n", clang_getCString(wd));
            return 1;
        }
        clang_disposeString(wd);

        int numArgs = (int)clang_CompileCommand_getNumArgs(CCmd);
        if (numArgs > MAX_COMPILE_ARGS) {
            fprintf(stderr, "got more compile arguments than maximum\n");
            return 1;
        }
        CXString cxString = clang_CompileCommand_getFilename(CCmd);
        auto cStringFilename = clang_getCString(cxString);
        // if (!std::strstr(cStringFilename, "host.cpp")) {
        //     clang_disposeString(cxString);
        //     continue;
        // }
        printf("Indexing file %d of %d (%s)\n", i + 1, numCommands, cStringFilename);
        // printf("Args: ");
        for (unsigned a = 0; a < numArgs; ++a) {
            cxargs[a] = clang_CompileCommand_getArg(CCmd, a);
            args[a]   = clang_getCString(cxargs[a]);
            //   printf("%s ", args[a]);
        }
        // printf("\n");
        CXTranslationUnit TU{};
        // if (!CreateTranslationUnit(Idx, cStringFilename, &TU)) {
        //     fprintf(stderr, "Could not create translation unit for %s\n",
        //             cStringFilename);
        //     continue;
        // }
        static IndexerCallbacks IndexCBEmpty{};
        errorCode = clang_indexSourceFileFullArgv(
                            idxAction,
                            nullptr,    /* client_data */
                            &IndexCBEmpty, sizeof(IndexCBEmpty), /*  &IndexCB,sizeof(IndexCB) ,*/
                            0,          /* index_opts */
                            nullptr,
                            args,
                            numArgs,
                            nullptr, 0,
                            &TU,        /* out_TU */
                            0);         /* CXTranslationUnit_DetailedPreprocessingRecord */
        // ImportedASTFilesData *importedASTs = bFullScan ?
        // importedASTs_create() : nullptr;
        if (errorCode != 0) {
            fprintf(stderr, "Error indexing %s\n", cStringFilename);
            // if (importedASTs)
            //     importedASTs_dispose(importedASTs);
        } else {
            Data.TU = TU;
            Data.szSrcPath = szSrcPath;
            auto sizeBefore = Data.VecClassDefs.size();
            auto sizeBeforeFunc = Data.VecFuncDefs.size();
            clang_visitChildren(clang_getTranslationUnitCursor(TU), ClassAndFunctionVisitor, &Data);
            auto sizeAfter = Data.VecClassDefs.size();
            auto sizeAfterFunc = Data.VecFuncDefs.size();
            printf("Found %zu classes in %s. Total %zu\n", sizeAfter - sizeBefore, cStringFilename, sizeAfter);
            printf("Found %zu functions in %s. Total %zu\n", sizeAfterFunc - sizeBeforeFunc, cStringFilename, sizeAfterFunc);
            WriteCsvFile(Data, outFilePath);
            // if (bFullScan && importedASTs) {
            //     unsigned i;
            //     for (i = 0; i < importedASTs->num_files && errorCode == 0; ++i) {
            //         errorCode = index_ast_file(importedASTs->filenames[i], Idx,
            //         idxAction,
            //                                 importedASTs);
            //     }
            //     importedASTs_dispose(importedASTs);
            // }
        }
        clang_disposeTranslationUnit(TU);
        for (unsigned a = 0; a < numArgs; ++a)
            clang_disposeString(cxargs[a]);
        clang_disposeString(cxString);
    }
    WriteCsvFile(Data, outFilePath);
    clang_CompileCommands_dispose(CCmds);
    clang_CompilationDatabase_dispose(db);
    clang_IndexAction_dispose(idxAction);
    clang_disposeIndex(Idx);

    return 0;
}

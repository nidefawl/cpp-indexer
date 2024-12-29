#include "cpp-indexer.h"
#include <cstdio>
#include <string>
#include <unistd.h>
#include <limits.h>

int main(int argc, char** argv) {
    if (argc < 2) {
        std::fprintf(stderr, "Usage: %s <builddir> <srcpath> [csvpath]\n", argv[0]);
        // return 1;
    }
    std::string buildDir = argc > 1 ? argv[1] : "./build";
    std::string srcPath = argc > 2 ? argv[2] : "./src";
    std::string outFilePath = argc > 3 ? argv[3] : "";
    auto cwd = getcwd(nullptr, 0);
    if (outFilePath.empty() && cwd) {
        outFilePath = cwd;
    }
    char actualpath [PATH_MAX+1];
    char* p = realpath(buildDir.c_str(), actualpath);
    if (p) buildDir = p;
    p = realpath(srcPath.c_str(), actualpath);
    if (p) srcPath = p;
    p = realpath(outFilePath.c_str(), actualpath);
    if (p) outFilePath = p;
    outFilePath += "/cpp-index";
    std::fprintf(stdout, "build directory: %s\n", buildDir.c_str());
    std::fprintf(stdout, "src path: %s\n", srcPath.c_str());
    std::fprintf(stdout, "csv path: %s.class.csv %s.func.csv\n", outFilePath.c_str(), outFilePath.c_str());
    return runIndexer(buildDir.c_str(), srcPath.c_str(), outFilePath.c_str());
}
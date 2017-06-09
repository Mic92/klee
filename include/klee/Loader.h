//===-- Linker.cpp --------------------------------------------------------===//
//
//                     The KLEE Symbolic Virtual Machine
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#ifndef KLEE_LINKER_H
#define KLEE_LINKER_H

#include <string>
#include <vector>

namespace llvm {
    class Module;
}

namespace klee {
    namespace loader {
        enum LibcType {
            NoLibc, KleeLibc, UcLibc
        };

        struct LoadOptions {
            // whether posix stubs should be linked
            bool enablePOSIXRuntime;
            // which libc should be linked
            LibcType libc;
            // list of library paths to link
            std::vector<std::string> linkLibraries;
        };

        /// Load an llvm bitcode file  and link against klee's runtime libraries
        /// @inputFile the file to load
        /// @entryPoint function name which should be considered as entrypoint
        /// @libraryPath path were klee runtime libraries are located
        llvm::Module* loadInputFile(std::string inputFile,
                                    std::string entryPoint,
                                    std::string libraryPath,
                                    LoadOptions &options);
    };
}


#endif //KLEE_LINKER_H

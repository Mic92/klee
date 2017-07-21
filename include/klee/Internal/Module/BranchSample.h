//
// Created by joerg on 7/20/17.
//

#ifndef KLEE_BRANCHSAMPLE_H
#define KLEE_BRANCHSAMPLE_H

#include "klee/Internal/Module/KInstruction.h"

namespace klee {

  class BranchSample {
  public:
    static std::vector<BranchSample>* readFromFile(std::string path, std::string *errorMessage);
    KInstruction* source;
    KInstruction* target;
  };
}



#endif //KLEE_BRANCHSAMPLE_H

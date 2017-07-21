//
// Created by joerg on 7/20/17.
//

#include "klee/Internal/Module/BranchSample.h"
#include <iostream>
#include <fstream>

using namespace klee;


#define BRANCH_SAMPLES_FILE_MAGIC "BranchSamples"

struct BranchSampleFileHeader {
  char fileMagic[sizeof(BRANCH_SAMPLES_FILE_MAGIC)];
  uint64_t numberOfRecords;
};

std::vector<BranchSample> *BranchSample::readFromFile(std::string path, std::string *errorMessage) {
  std::ifstream fh(path.c_str(), std::ios::binary);
  if (!fh.good()) {
    *errorMessage = strerror(errno);
    return nullptr;
  }
  BranchSampleFileHeader header;
  fh.read((char*)&header, sizeof(header));
  if (!fh) {
    *errorMessage = "Cannot read file header: " + std::string(strerror(errno));
  }
  if (!fh || header.fileMagic != BRANCH_SAMPLES_FILE_MAGIC) {
    *errorMessage = "Invalid file magic";
    return nullptr;
  }

  auto samples = new std::vector<BranchSample>;
  samples->reserve(header.numberOfRecords);
  for (uint64_t i = 0; i < header.numberOfRecords; i++) {
    BranchSample sample;
    fh.read((char*)&sample, sizeof(sample));
    if (!fh) {
      *errorMessage = "Cannot read all samples from file: " + std::string(strerror(errno));
      return nullptr;
    }
    samples->push_back(sample);
  }

  *errorMessage = "";
  return samples;
}

//===-- Linker.cpp --------------------------------------------------------===//
//
//                     The KLEE Symbolic Virtual Machine
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#include "klee/Loader.h"
#include "klee/Config/Version.h"
#include "klee/Internal/Support/ErrorHandling.h"
#include "klee/Internal/Support/ModuleUtil.h"

#include <llvm/ADT/SmallString.h>

#if LLVM_VERSION_CODE >= LLVM_VERSION(4, 0)
#include <llvm/Bitcode/BitcodeReader.h>
#else
#include <llvm/Bitcode/ReaderWriter.h>
#endif

#if LLVM_VERSION_CODE >= LLVM_VERSION(3, 2)
#include <llvm/IR/Module.h>
#include <llvm/IR/Constants.h>
#include <llvm/IR/LLVMContext.h>
#else
#include <llvm/Module.h>
#include <llvm/Constants.h>
#include "llvm/LLVMContext.h"
#endif

#if LLVM_VERSION_CODE < LLVM_VERSION(3, 5)
#include "llvm/Support/CallSite.h"
#else
#include "llvm/IR/CallSite.h"
#endif

#include <llvm/Support/MemoryBuffer.h>
#include <llvm/Support/Path.h>
#include <llvm/Support/FileSystem.h>

using namespace llvm;
using namespace klee;

static void initEnv(Module *mainModule, std::string entryPoint) {

    /*
      nArgcP = alloc oldArgc->getType()
      nArgvV = alloc oldArgv->getType()
      store oldArgc nArgcP
      store oldArgv nArgvP
      klee_init_environment(nArgcP, nArgvP)
      nArgc = load nArgcP
      nArgv = load nArgvP
      oldArgc->replaceAllUsesWith(nArgc)
      oldArgv->replaceAllUsesWith(nArgv)
    */

    Function *mainFn = mainModule->getFunction(entryPoint);
    if (!mainFn) {
        klee_error("'%s' function not found in module.", entryPoint.c_str());
    }

    if (mainFn->arg_size() < 2) {
        klee_error("Cannot handle ""--posix-runtime"" when main() has less than two arguments.\n");
    }

    Instruction *firstInst = &*mainFn->begin()->begin();

    Value *oldArgc = &*mainFn->arg_begin();
    Value *oldArgv = &*(++mainFn->arg_begin());

    AllocaInst* argcPtr =
            new AllocaInst(oldArgc->getType(), "argcPtr", firstInst);
    AllocaInst* argvPtr =
            new AllocaInst(oldArgv->getType(), "argvPtr", firstInst);

    /* Insert void klee_init_env(int* argc, char*** argv) */
    std::vector<const Type*> params;
    LLVMContext &ctx = mainModule->getContext();
    params.push_back(Type::getInt32Ty(ctx));
    params.push_back(Type::getInt32Ty(ctx));
    Function* initEnvFn =
            cast<Function>(mainModule->getOrInsertFunction("klee_init_env",
                                                           Type::getVoidTy(ctx),
                                                           argcPtr->getType(),
                                                           argvPtr->getType(),
                                                           NULL));
    assert(initEnvFn);
    std::vector<Value*> args;
    args.push_back(argcPtr);
    args.push_back(argvPtr);
#if LLVM_VERSION_CODE >= LLVM_VERSION(3, 0)
    Instruction* initEnvCall = CallInst::Create(initEnvFn, args,
                                                "", firstInst);
#else
    Instruction* initEnvCall = CallInst::Create(initEnvFn, args.begin(), args.end(),
					      "", firstInst);
#endif
    Value *argc = new LoadInst(argcPtr, "newArgc", firstInst);
    Value *argv = new LoadInst(argvPtr, "newArgv", firstInst);

    oldArgc->replaceAllUsesWith(argc);
    oldArgv->replaceAllUsesWith(argv);

    new StoreInst(oldArgc, argcPtr, initEnvCall);
    new StoreInst(oldArgv, argvPtr, initEnvCall);
}


#ifndef SUPPORT_KLEE_UCLIBC
static llvm::Module *linkWithUclibc(llvm::Module *mainModule,
                                    std::string libraryPath,
                                    std::string entryPoint,
                                    loader::LoadOptions options) {
    klee_error("invalid libc, no uclibc support!\n");
}
#else
static void replaceOrRenameFunction(llvm::Module *module,
		const char *old_name, const char *new_name)
{
  Function *f, *f2;
  f = module->getFunction(new_name);
  f2 = module->getFunction(old_name);
  if (f2) {
    if (f) {
      f2->replaceAllUsesWith(f);
      f2->eraseFromParent();
    } else {
      f2->setName(new_name);
      assert(f2->getName() == new_name);
    }
  }
}
static llvm::Module *linkWithUclibc(llvm::Module *mainModule,
                                    std::string libraryPath,
                                    std::string entryPoint,
                                    loader::LoadOptions options) {
  LLVMContext &ctx = mainModule->getContext();
  // Ensure that klee-uclibc exists
  SmallString<128> uclibcBCA(libraryPath);
  llvm::sys::path::append(uclibcBCA, KLEE_UCLIBC_BCA_NAME);

#if LLVM_VERSION_CODE >= LLVM_VERSION(3, 6)
  Twine uclibcBCA_twine(uclibcBCA.c_str());
  if (!llvm::sys::fs::exists(uclibcBCA_twine))
#else
  bool uclibcExists=false;
  llvm::sys::fs::exists(uclibcBCA.c_str(), uclibcExists);
  if (!uclibcExists)
#endif
    klee_error("Cannot find klee-uclibc : %s", uclibcBCA.c_str());

  Function *f;
  // force import of __uClibc_main
  mainModule->getOrInsertFunction("__uClibc_main",
                                  FunctionType::get(Type::getVoidTy(ctx),
                                               std::vector<LLVM_TYPE_Q Type*>(),
                                                    true));

  // force various imports
  if (options.enablePOSIXRuntime) {
    LLVM_TYPE_Q llvm::Type *i8Ty = Type::getInt8Ty(ctx);
    mainModule->getOrInsertFunction("realpath",
                                    PointerType::getUnqual(i8Ty),
                                    PointerType::getUnqual(i8Ty),
                                    PointerType::getUnqual(i8Ty),
                                    NULL);
    mainModule->getOrInsertFunction("getutent",
                                    PointerType::getUnqual(i8Ty),
                                    NULL);
    mainModule->getOrInsertFunction("__fgetc_unlocked",
                                    Type::getInt32Ty(ctx),
                                    PointerType::getUnqual(i8Ty),
                                    NULL);
    mainModule->getOrInsertFunction("__fputc_unlocked",
                                    Type::getInt32Ty(ctx),
                                    Type::getInt32Ty(ctx),
                                    PointerType::getUnqual(i8Ty),
                                    NULL);
  }

  f = mainModule->getFunction("__ctype_get_mb_cur_max");
  if (f) f->setName("_stdlib_mb_cur_max");

  // Strip of asm prefixes for 64 bit versions because they are not
  // present in uclibc and we want to make sure stuff will get
  // linked. In the off chance that both prefixed and unprefixed
  // versions are present in the module, make sure we don't create a
  // naming conflict.
  for (Module::iterator fi = mainModule->begin(), fe = mainModule->end();
       fi != fe; ++fi) {
    Function *f = &*fi;
    const std::string &name = f->getName();
    if (name[0]=='\01') {
      unsigned size = name.size();
      if (name[size-2]=='6' && name[size-1]=='4') {
        std::string unprefixed = name.substr(1);

        // See if the unprefixed version exists.
        if (Function *f2 = mainModule->getFunction(unprefixed)) {
          f->replaceAllUsesWith(f2);
          f->eraseFromParent();
        } else {
          f->setName(unprefixed);
        }
      }
    }
  }

  mainModule = klee::linkWithLibrary(mainModule, uclibcBCA.c_str());
  assert(mainModule && "unable to link with uclibc");


  replaceOrRenameFunction(mainModule, "__libc_open", "open");
  replaceOrRenameFunction(mainModule, "__libc_fcntl", "fcntl");

  // Take care of fortified functions
  replaceOrRenameFunction(mainModule, "__fprintf_chk", "fprintf");

  // XXX we need to rearchitect so this can also be used with
  // programs externally linked with uclibc.

  // We now need to swap things so that __uClibc_main is the entry
  // point, in such a way that the arguments are passed to
  // __uClibc_main correctly. We do this by renaming the user main
  // and generating a stub function to call __uClibc_main. There is
  // also an implicit cooperation in that runFunctionAsMain sets up
  // the environment arguments to what uclibc expects (following
  // argv), since it does not explicitly take an envp argument.
  Function *userMainFn = mainModule->getFunction(entryPoint);
  assert(userMainFn && "unable to get user main");
  Function *uclibcMainFn = mainModule->getFunction("__uClibc_main");
  assert(uclibcMainFn && "unable to get uclibc main");
  userMainFn->setName("__user_main");

  const FunctionType *ft = uclibcMainFn->getFunctionType();
  assert(ft->getNumParams() == 7);

  std::vector<LLVM_TYPE_Q Type*> fArgs;
  fArgs.push_back(ft->getParamType(1)); // argc
  fArgs.push_back(ft->getParamType(2)); // argv
  Function *stub = Function::Create(FunctionType::get(Type::getInt32Ty(ctx), fArgs, false),
                                    GlobalVariable::ExternalLinkage,
                                    entryPoint,
                                    mainModule);
  BasicBlock *bb = BasicBlock::Create(ctx, "entry", stub);

  std::vector<llvm::Value*> args;
  args.push_back(llvm::ConstantExpr::getBitCast(userMainFn,
                                                ft->getParamType(0)));
  args.push_back(&*stub->arg_begin()); // argc
  args.push_back(&*(++stub->arg_begin())); // argv
  args.push_back(Constant::getNullValue(ft->getParamType(3))); // app_init
  args.push_back(Constant::getNullValue(ft->getParamType(4))); // app_fini
  args.push_back(Constant::getNullValue(ft->getParamType(5))); // rtld_fini
  args.push_back(Constant::getNullValue(ft->getParamType(6))); // stack_end
#if LLVM_VERSION_CODE >= LLVM_VERSION(3, 0)
  CallInst::Create(uclibcMainFn, args, "", bb);
#else
  CallInst::Create(uclibcMainFn, args.begin(), args.end(), "", bb);
#endif

  new UnreachableInst(ctx, bb);

  klee_message("NOTE: Using klee-uclibc : %s", uclibcBCA.c_str());
  return mainModule;
}
#endif

Module* klee::loader::loadInputFile(std::string inputFile,
                                    std::string entryPoint,
                                    std::string libraryPath,
                                    LoadOptions &options) {
  llvm::LLVMContext ctx;
  Module *mainModule = 0;

#if LLVM_VERSION_CODE < LLVM_VERSION(3, 5)
  OwningPtr<MemoryBuffer> bufferPtr;
  error_code ec=MemoryBuffer::getFileOrSTDIN(inputFile.c_str(), bufferPtr);
  if (ec) {
    klee_error("error loading program '%s': %s", inputFile.c_str(),
               ec.message().c_str());
  }

  std::string errorMsg;
  mainModule = getLazyBitcodeModule(bufferPtr.get(), ctx, &errorMsg);

  if (mainModule) {
    if (mainModule->MaterializeAllPermanently(&errorMsg)) {
      delete mainModule;
      mainModule = 0;
    }
  }
  if (!mainModule)
    klee_error("error loading program '%s': %s", inputFile.c_str(),
               errorMsg.c_str());
#else
    auto Buffer = MemoryBuffer::getFileOrSTDIN(inputFile.c_str());
    if (!Buffer)
        klee_error("error loading program '%s': %s", inputFile.c_str(),
                   Buffer.getError().message().c_str());

    std::error_code ec;
#if LLVM_VERSION_CODE >= LLVM_VERSION(4, 0)
    Expected<std::unique_ptr<Module>> mainModuleOrError =
    getOwningLazyBitcodeModule(std::move(Buffer.get()), ctx);
    ec = mainModuleOrError ? std::error_code() :
    errorToErrorCode(mainModuleOrError.takeError());
#elif LLVM_VERSION_CODE >= LLVM_VERSION(3, 6)
    auto mainModuleOrError = getLazyBitcodeModule(std::move(Buffer.get()), ctx);
    ec = mainModuleOrError.getError();
#else
    auto mainModuleOrError = getLazyBitcodeModule(Buffer->get(), ctx);
    ec = mainModuleOrError.getError();
#endif

    if (ec) {
        klee_error("error loading program '%s': %s", inputFile.c_str(),
                   ec.message().c_str());
    }
    else {
        // The module has taken ownership of the MemoryBuffer so release it
        // from the std::unique_ptr
        Buffer->release();
    }
#if LLVM_VERSION_CODE >= LLVM_VERSION(3, 7)
    mainModule = mainModuleOrError->release();
#else
    mainModule = *mainModuleOrError;
#endif
#if LLVM_VERSION_CODE >= LLVM_VERSION(4, 0)
    if (llvm::Error err = mainModule->materializeAll()) {
    std::error_code ec = errorToErrorCode(std::move(err));
#elif LLVM_VERSION_CODE >= LLVM_VERSION(3, 8)
    if (auto ec = mainModule->materializeAll()) {
#else
    if (auto ec = mainModule->materializeAllPermanently()) {
#endif
        klee_error("error loading program '%s': %s", inputFile.c_str(),
                   ec.message().c_str());
    }
#endif

    if (options.enablePOSIXRuntime) {
        initEnv(mainModule, entryPoint);
    }

    switch (options.libc) {
        case NoLibc: /* silence compiler warning */
            break;

        case KleeLibc: {
            // FIXME: Find a reasonable solution for this.
            SmallString<128> Path(libraryPath);
#if LLVM_VERSION_CODE >= LLVM_VERSION(3,3)
            llvm::sys::path::append(Path, "klee-libc.bc");
#else
            llvm::sys::path::append(Path, "libklee-libc.bca");
#endif
            mainModule = klee::linkWithLibrary(mainModule, Path.c_str());
            assert(mainModule && "unable to link with klee-libc");
            break;
        }

        case UcLibc:
            mainModule = linkWithUclibc(mainModule, libraryPath, entryPoint, options);
            break;
    }

    if (options.enablePOSIXRuntime) {
        SmallString<128> Path(libraryPath);
        llvm::sys::path::append(Path, "libkleeRuntimePOSIX.bca");
        klee_message("NOTE: Using model: %s", Path.c_str());
        mainModule = klee::linkWithLibrary(mainModule, Path.c_str());
        assert(mainModule && "unable to link with simple model");
    }

    std::vector<std::string>::iterator libs_it;
    std::vector<std::string>::iterator libs_ie;
    for (libs_it = options.linkLibraries.begin(), libs_ie = options.linkLibraries.end();
         libs_it != libs_ie; ++libs_it) {
        const char * libFilename = libs_it->c_str();
        klee_message("Linking in library: %s.\n", libFilename);
        mainModule = klee::linkWithLibrary(mainModule, libFilename);
    }

    return mainModule;
}

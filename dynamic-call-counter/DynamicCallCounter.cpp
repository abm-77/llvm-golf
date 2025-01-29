
#include "llvm/Analysis/ValueTracking.h"
#include "llvm/Pass.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"
#include "llvm/Support/raw_ostream.h"
#include <cstdint>

using namespace llvm;

namespace {

struct DynamicCallCounter : PassInfoMixin<DynamicCallCounter> {
public:
  void insertCallCounter(LLVMContext &ctx, IRBuilder<> &builder, Module &M,
                         Function &F) {
    Constant *zero = ConstantInt::get(Type::getInt32Ty(ctx), 0);
    Constant *one = ConstantInt::get(Type::getInt32Ty(ctx), 1);
    GlobalVariable *fnCounter = new GlobalVariable(
        M, zero->getType(), false, GlobalValue::ExternalLinkage, zero,
        F.getName() + "_counter");
    counters[F.getName()] = fnCounter;
    for (auto &BB : F) {
      Instruction *insertPos = &*BB.getFirstInsertionPt();
      builder.SetInsertPoint(insertPos);
      Value *count = builder.CreateLoad(fnCounter->getValueType(), fnCounter,
                                        "load_" + F.getName() + "_counter");
      Value *increment = builder.CreateAdd(count, one);
      builder.CreateStore(increment, fnCounter);
      break;
    }
  }

  void insertSummary(LLVMContext &ctx, IRBuilder<> &builder, Module &M) {
    FunctionType *printfType =
        FunctionType::get(IntegerType::getInt32Ty(ctx),
                          PointerType::getUnqual(Type::getInt8Ty(ctx)), true);
    Function *printfFunc =
        Function::Create(printfType, Function::ExternalLinkage, "printf", M);
    Constant *formatStr = ConstantDataArray::getString(
        ctx, "function %s had %d dyanmic calls.\n");
    GlobalVariable *formatVar = new GlobalVariable(
        M, formatStr->getType(), true, GlobalVariable::PrivateLinkage,
        formatStr, ".fmt_str");
    Constant *zero = ConstantInt::get(Type::getInt32Ty(ctx), 0);
    Value *formatPtr =
        builder.CreateGEP(formatStr->getType(), formatVar, {zero, zero});

    Function *mainFn = M.getFunction("main");
    if (!mainFn) {
      errs() << "main function not found.\n";
      return;
    }

    BasicBlock &lastBB = mainFn->back();
    Instruction &last = lastBB.back();
    builder.SetInsertPoint(&last);
    for (auto &F : M) {
      if (!counters.contains(F.getName()))
        continue;
      Constant *funcNameStr = ConstantDataArray::getString(ctx, F.getName());
      GlobalVariable *funcNameVar = new GlobalVariable(
          M, funcNameStr->getType(), true, GlobalValue::PrivateLinkage,
          funcNameStr, F.getName() + "_str");
      Value *funcNamePtr =
          builder.CreateGEP(funcNameStr->getType(), funcNameVar, {zero, zero});
      Value *count =
          builder.CreateLoad(Type::getInt32Ty(ctx), counters[F.getName()],
                             "load_" + F.getName() + "_counter");
      builder.CreateCall(printfFunc, {formatPtr, funcNamePtr, count});
    }
  }

  PreservedAnalyses run(Module &M, ModuleAnalysisManager &MAM) {
    LLVMContext &ctx = M.getContext();
    IRBuilder<> builder(ctx);
    for (auto &F : M)
      insertCallCounter(ctx, builder, M, F);
    insertSummary(ctx, builder, M);
    return PreservedAnalyses::none();
  }

private:
  StringMap<GlobalVariable *> counters;
};

} // namespace
  //
extern "C" LLVM_ATTRIBUTE_WEAK ::llvm::PassPluginLibraryInfo
llvmGetPassPluginInfo() {
  return {.APIVersion = LLVM_PLUGIN_API_VERSION,
          .PluginName = "dynamic count pass",
          .PluginVersion = "v0.1",
          .RegisterPassBuilderCallbacks = [](PassBuilder &PB) {
            PB.registerPipelineParsingCallback(
                [](StringRef Name, ModulePassManager &MPM, ...) {
                  if (Name == "dynamic-count-pass") {
                    MPM.addPass(DynamicCallCounter());
                    return true;
                  } else {
                    return false;
                  }
                });
          }};
}

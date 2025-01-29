#include "llvm/Analysis/ValueTracking.h"
#include "llvm/Pass.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"

using namespace llvm;

namespace {
struct InjectFuncCallPass : PassInfoMixin<InjectFuncCallPass> {

  PreservedAnalyses run(Function &F, FunctionAnalysisManager &FAM) {
    LLVMContext &ctx = F.getContext();
    Module *M = F.getParent();
    IRBuilder<> Builder(ctx);

    Function *printfFunc = M->getFunction("printf");
    if (!printfFunc) {
      FunctionType *printfType =
          FunctionType::get(IntegerType::getInt32Ty(ctx),
                            PointerType::getUnqual(Type::getInt8Ty(ctx)), true);
      printfFunc =
          Function::Create(printfType, Function::ExternalLinkage, "printf", M);
    }

    Constant *formatStr =
        ConstantDataArray::getString(ctx, "hello from %s w/ %d args!\n");
    GlobalVariable *formatVar = new GlobalVariable(
        *M, formatStr->getType(), true, GlobalValue::PrivateLinkage, formatStr,
        ".print_fmt");

    for (BasicBlock &BB : F) {
      Constant *funcNameStr = ConstantDataArray::getString(ctx, F.getName());
      GlobalVariable *funcNameVar = new GlobalVariable(
          *M, funcNameStr->getType(), true, GlobalValue::PrivateLinkage,
          funcNameStr, F.getName() + "_str");

      Instruction *insertPos = &*BB.getFirstInsertionPt();
      Builder.SetInsertPoint(insertPos);

      Constant *zero = ConstantInt::get(Type::getInt32Ty(ctx), 0);
      Value *formatPtr =
          Builder.CreateGEP(formatStr->getType(), formatVar, {zero, zero});
      Value *funcNamePtr =
          Builder.CreateGEP(funcNameStr->getType(), funcNameVar, {zero, zero});
      Constant *argCount =
          ConstantInt::get(Type::getInt32Ty(ctx), F.arg_size());

      Builder.CreateCall(printfFunc, {formatPtr, funcNamePtr, argCount});
      break;
    }

    return PreservedAnalyses::none();
  }

  static bool isRequired() { return true; }
};

} // namespace

extern "C" LLVM_ATTRIBUTE_WEAK ::llvm::PassPluginLibraryInfo
llvmGetPassPluginInfo() {
  return {.APIVersion = LLVM_PLUGIN_API_VERSION,
          .PluginName = "inject pass",
          .PluginVersion = "v0.1",
          .RegisterPassBuilderCallbacks = [](PassBuilder &PB) {
            PB.registerPipelineParsingCallback(
                [](StringRef Name, FunctionPassManager &FPM, ...) {
                  if (Name == "inject-pass") {
                    FPM.addPass(InjectFuncCallPass());
                    return true;
                  } else {
                    return false;
                  }
                });
          }};
}

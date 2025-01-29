#include "llvm/Analysis/ValueTracking.h"
#include "llvm/Pass.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"
#include "llvm/Support/raw_ostream.h"
#include <cstdint>

using namespace llvm;

namespace {

struct StaticCallCounter : public AnalysisInfoMixin<StaticCallCounter> {
public:
  using Result = StringMap<uint32_t>;
  Result run(Module &M, ModuleAnalysisManager &MAM) {
    Result res;
    for (auto &F : M) {
      for (auto &BB : F) {
        for (auto &I : BB) {
          if (auto *call = dyn_cast<CallInst>(&I)) {
            auto callee = call->getCalledFunction()->getName();
            if (res.contains(callee)) {
              res[callee]++;
            } else {
              res[callee] = 1;
            }
          }
        }
      }
    }
    return res;
  }

  static bool isRequired() { return true; }

private:
  static AnalysisKey Key;
  friend struct AnalysisInfoMixin<StaticCallCounter>;
};

struct StaticCallCounterPrinter
    : public PassInfoMixin<StaticCallCounterPrinter> {
public:
  explicit StaticCallCounterPrinter(raw_ostream &OutS) : OS(OutS) {}
  PreservedAnalyses run(Module &M, ModuleAnalysisManager &MAM) {
    auto freqs = MAM.getResult<StaticCallCounter>(M);
    for (auto &[name, freq] : freqs) {
      errs() << name << " has " << freq << " static calls.\n";
    }
    return PreservedAnalyses::all();
  }

private:
  raw_ostream &OS;
};

AnalysisKey StaticCallCounter::Key;

} // namespace

extern "C" LLVM_ATTRIBUTE_WEAK ::llvm::PassPluginLibraryInfo
llvmGetPassPluginInfo() {
  return {.APIVersion = LLVM_PLUGIN_API_VERSION,
          .PluginName = "opcode pass",
          .PluginVersion = "v0.1",
          .RegisterPassBuilderCallbacks = [](PassBuilder &PB) {
            PB.registerPipelineParsingCallback(
                [](StringRef Name, ModulePassManager &MPM, ...) {
                  if (Name == "print<StaticCallCounter>") {
                    MPM.addPass(StaticCallCounterPrinter(errs()));
                    return true;
                  } else {
                    return false;
                  }
                });

            PB.registerPipelineStartEPCallback(
                [](ModulePassManager &MPM, OptimizationLevel Level) {
                  MPM.addPass(StaticCallCounterPrinter(errs()));
                });

            PB.registerAnalysisRegistrationCallback(
                [](ModuleAnalysisManager &MAM) {
                  MAM.registerPass([&] { return StaticCallCounter(); });
                });
          }};
}

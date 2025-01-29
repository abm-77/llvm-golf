#include "llvm/Analysis/ValueTracking.h"
#include "llvm/Pass.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"
#include "llvm/Support/raw_ostream.h"
#include <cstdint>

using namespace llvm;

namespace {
struct OpcodeCounter : public AnalysisInfoMixin<OpcodeCounter> {
public:
  using Result = StringMap<uint32_t>;

  Result run(Function &F, FunctionAnalysisManager &FAM) {
    Result result;

    for (BasicBlock &BB : F) {
      for (Instruction &I : BB) {
        auto op = I.getOpcodeName();
        if (result.contains(op)) {
          result[op]++;
        } else {
          result[op] = 1;
        }
      }
    }

    return result;
  }

  static bool isRequired() { return true; }

private:
  static AnalysisKey Key;
  friend struct AnalysisInfoMixin<OpcodeCounter>;
};

AnalysisKey OpcodeCounter::Key;

struct OpcodeCounterPrinter : public PassInfoMixin<OpcodeCounterPrinter> {
public:
  explicit OpcodeCounterPrinter(raw_ostream &OutS) : OS(OutS) {}
  PreservedAnalyses run(Function &F, FunctionAnalysisManager &FAM) {
    auto opcodeFreqs = FAM.getResult<OpcodeCounter>(F);
    for (auto &[op, freq] : opcodeFreqs) {
      errs() << "op " << op << ": " << freq << "\n";
    }
    return PreservedAnalyses::all();
  }

private:
  raw_ostream &OS;
};
} // namespace

extern "C" LLVM_ATTRIBUTE_WEAK ::llvm::PassPluginLibraryInfo
llvmGetPassPluginInfo() {
  return {.APIVersion = LLVM_PLUGIN_API_VERSION,
          .PluginName = "opcode pass",
          .PluginVersion = "v0.1",
          .RegisterPassBuilderCallbacks = [](PassBuilder &PB) {
            // specify for opt
            PB.registerPipelineParsingCallback(
                [](StringRef Name, FunctionPassManager &FPM,
                   ArrayRef<PassBuilder::PipelineElement>) {
                  if (Name == "print<opcode-counter>") {
                    FPM.addPass(OpcodeCounterPrinter(errs()));
                    return true;
                  }
                  return false;
                });

            // register printer pass as part of existing pipeline
            PB.registerVectorizerStartEPCallback(
                [](FunctionPassManager &FPM, OptimizationLevel Level) {
                  FPM.addPass(OpcodeCounterPrinter(errs()));
                });

            // register for FAM.getResult
            PB.registerAnalysisRegistrationCallback(
                [](FunctionAnalysisManager &FAM) {
                  FAM.registerPass([&] { return OpcodeCounter(); });
                });
          }};
}

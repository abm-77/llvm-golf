#include "llvm/Analysis/ValueTracking.h"
#include "llvm/IR/Dominators.h"
#include "llvm/Pass.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"
#include "llvm/Support/raw_ostream.h"

using namespace llvm;

namespace {

struct RIV : public AnalysisInfoMixin<RIV> {
  using Result = MapVector<BasicBlock const *, SmallPtrSet<Value *, 8>>;

  Result run(Function &F, FunctionAnalysisManager &FAM) {
    Result defs, map;
    DominatorTree *DT = &FAM.getResult<DominatorTreeAnalysis>(F);
    for (auto &BB : F) {
      auto &defined_values = defs[&BB];
      for (auto &I : BB) {
        if (I.getType()->isIntegerTy()) {
          defined_values.insert(&I);
        }
      }
    }

    auto &entry_vals = map[&F.getEntryBlock()];
    for (GlobalVariable &global : F.getParent()->globals()) {
      if (global.getType()->isIntegerTy())
        entry_vals.insert(&global);
    }
    for (Argument &arg : F.args()) {
      if (arg.getType()->isIntegerTy())
        entry_vals.insert(&arg);
    }

    std::vector<DomTreeNodeBase<BasicBlock> *> worklist{DT->getRootNode()};
    while (!worklist.empty()) {
      auto curr = worklist.back();
      worklist.pop_back();
      auto &curr_defs = defs[curr->getBlock()];
      auto curr_rivs = map[curr->getBlock()];
      for (auto sub : *curr) {
        worklist.push_back(sub);
        auto sub_bb = sub->getBlock();
        map[sub_bb].insert(curr_defs.begin(), curr_defs.end());
        map[sub_bb].insert(curr_rivs.begin(), curr_rivs.end());
      }
    }
    return map;
  }

private:
  static AnalysisKey Key;
  friend struct AnalysisInfoMixin<RIV>;
};

AnalysisKey RIV::Key;

struct RIVPrinter : public PassInfoMixin<RIVPrinter> {
  explicit RIVPrinter(raw_ostream &OutS) : OS(OutS) {}

  PreservedAnalyses run(Function &F, FunctionAnalysisManager &FAM) {
    RIV::Result res = FAM.getResult<RIV>(F);
    for (auto &[bb, rivs] : res) {
      bb->printAsOperand(OS);
      OS << ": ";
      for (auto riv : rivs) {
        riv->print(OS);
        OS << ", ";
      }
      OS << "\n";
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
            PB.registerPipelineParsingCallback(
                [](StringRef Name, FunctionPassManager &FPM, ...) {
                  if (Name == "print<RIV>") {
                    FPM.addPass(RIVPrinter(errs()));
                    return true;
                  } else {
                    return false;
                  }
                });

            PB.registerVectorizerStartEPCallback(
                [](FunctionPassManager &FPM, OptimizationLevel Level) {
                  FPM.addPass(RIVPrinter(errs()));
                });

            PB.registerAnalysisRegistrationCallback(
                [](FunctionAnalysisManager &FAM) {
                  FAM.registerPass([&] { return RIV(); });
                });
          }};
}

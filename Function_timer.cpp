#include "llvm/IR/PassManager.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Constants.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Passes/PassPlugin.h"
#include "llvm/Passes/PassBuilder.h"

using namespace llvm;

class FunctionTimerPass
    : public PassInfoMixin<FunctionTimerPass> {

public:
    PreservedAnalyses run(Module &M, ModuleAnalysisManager &AM) {

        LLVMContext &Context = M.getContext();

        Type *VoidType = Type::getVoidTy(Context);
        Type *PointerType = PointerType::getUnqual(Context);

        // Function-level timers.
        FunctionType *TimerFunctionType =
            FunctionType::get(VoidType, {PointerType}, false);

        FunctionCallee TimerStart =
            M.getOrInsertFunction("timer_start", TimerFunctionType);

        FunctionCallee TimerStop =
            M.getOrInsertFunction("timer_stop", TimerFunctionType);

        // Call-level loggers.
        FunctionCallee LoggerBefore =
            M.getOrInsertFunction("logger_before", TimerFunctionType);

        FunctionCallee LoggerAfter =
            M.getOrInsertFunction("logger_after", TimerFunctionType);

        // Collect original calls before modifying the IR.
        // This prevents instrumenting the logger calls we insert.
        SmallVector<CallInst *, 64> OriginalCalls;

        for (Function &F : M) {

            // Skip declarations and runtime instrumentation functions.
            if (F.isDeclaration() ||
                F.getName().starts_with("timer_") ||
                F.getName().starts_with("logger_")) {
                continue;
            }

            errs() << "Function: " << F.getName() << "\n";

            for (BasicBlock &BB : F) {
                for (Instruction &I : BB) {

                    if (auto *Call = dyn_cast<CallInst>(&I)) {
                        OriginalCalls.push_back(Call);
                    }
                }
            }
        }

        // Instrument every original direct call.
        for (CallInst *Call : OriginalCalls) {

            Function *Caller = Call->getFunction();
            Function *Callee = Call->getCalledFunction();

            // Skip indirect calls for now.
            if (Callee == nullptr) {
                continue;
            }

            // Avoid instrumenting calls to our runtime functions.
            StringRef CalleeName = Callee->getName();

            if (CalleeName.starts_with("timer_") ||
                CalleeName.starts_with("logger_")) {
                continue;
            }

            // Avoid instrumenting calls to LLVM intrinsics.
            if (Callee->isIntrinsic()) {
                continue;
            }

            // Get the callee's name as a global string.
            IRBuilder<> BeforeBuilder(Call);

            Value *CalleeNamePtr =
                BeforeBuilder.CreateGlobalStringPtr(CalleeName);

            // Insert logger_before() immediately before the call.
            BeforeBuilder.CreateCall(
                LoggerBefore,
                {CalleeNamePtr}
            );

            // Insert logger_after() immediately after the call.
            Instruction *NextInstruction = Call->getNextNode();

            if (NextInstruction != nullptr) {

                IRBuilder<> AfterBuilder(NextInstruction);

                Value *AfterNamePtr =
                    AfterBuilder.CreateGlobalStringPtr(CalleeName);

                AfterBuilder.CreateCall(
                    LoggerAfter,
                    {AfterNamePtr}
                );
            }

            errs() << "  Instrumented call in "
                   << Caller->getName()
                   << " -> "
                   << CalleeName
                   << "\n";
        }

        // Preserve your existing function-entry and function-exit timers.
        for (Function &F : M) {

            if (F.isDeclaration() ||
                F.getName().starts_with("timer_") ||
                F.getName().starts_with("logger_")) {
                continue;
            }

            // Insert timer_start() at function entry.
            IRBuilder<> EntryBuilder(
                &*F.getEntryBlock().getFirstInsertionPt()
            );

            Value *FunctionName =
                EntryBuilder.CreateGlobalStringPtr(F.getName());

            EntryBuilder.CreateCall(
                TimerStart,
                {FunctionName}
            );

            // Insert timer_stop() before normal returns.
            for (BasicBlock &BB : F) {

                Instruction *Terminator = BB.getTerminator();

                if (isa<ReturnInst>(Terminator)) {

                    IRBuilder<> ReturnBuilder(Terminator);

                    Value *ReturnFunctionName =
                        ReturnBuilder.CreateGlobalStringPtr(F.getName());

                    ReturnBuilder.CreateCall(
                        TimerStop,
                        {ReturnFunctionName}
                    );
                }
            }
        }

        return PreservedAnalyses::none();
    }
};


// Register the pass with the New Pass Manager.
void registerFunctionTimer(PassBuilder &PB) {

    PB.registerPipelineParsingCallback(
        [](StringRef Name,
           ModulePassManager &MPM,
           ArrayRef<PassBuilder::PipelineElement>) {

            if (Name == "function-timer") {
                MPM.addPass(FunctionTimerPass());
                return true;
            }

            return false;
        }
    );
}


extern "C" LLVM_ATTRIBUTE_WEAK
PassPluginLibraryInfo llvmGetPassPluginInfo() {

    return {
        LLVM_PLUGIN_API_VERSION,
        "FunctionTimer",
        LLVM_VERSION_STRING,
        registerFunctionTimer
    };
}
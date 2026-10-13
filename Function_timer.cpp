#include "llvm/IR/PassManager.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Function.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Passes/PassPlugin.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Instructions.h"
using namespace llvm;


class FunctionTimerPass
    : public PassInfoMixin<FunctionTimerPass> {
public:
    PreservedAnalyses run(Module &M, ModuleAnalysisManager &AM){
        LLVMContext &Context  = M.getContext() ;
        Type *TimerReturnType = Type::getVoidTy(Context);
        Type *PointerType = llvm::PointerType::getUnqual(Context);
        FunctionType *TimerFunctionType =  FunctionType::get(TimerReturnType, {PointerType}, false);
        FunctionCallee TimerStart =   M.getOrInsertFunction("timer_start", TimerFunctionType);
        FunctionCallee TimerStop =   M.getOrInsertFunction("timer_stop", TimerFunctionType);
        for (Function &F : M) {
    if (F.isDeclaration() || F.getName().starts_with("timer_")) {
        continue;
    }

    errs() << "Function: " << F.getName() << "\n";

    // 1. Insert timer_start at the function entry.
    IRBuilder<> EntryBuilder(
        &*F.getEntryBlock().getFirstInsertionPt()
    );

    Value *FunctionName =  EntryBuilder.CreateGlobalStringPtr(F.getName());
    EntryBuilder.CreateCall(TimerStart, {FunctionName});
    // 2. Insert timer_stop before every return instruction.
    for (BasicBlock &BB : F) {
        Instruction *Terminator = BB.getTerminator();
        if (isa<ReturnInst>(Terminator)) {
            IRBuilder<> ReturnBuilder(Terminator);
            Value *ReturnFunctionName =
                ReturnBuilder.CreateGlobalStringPtr(F.getName());

            ReturnBuilder.CreateCall(TimerStop, {ReturnFunctionName} );
        }
    }
}

        return PreservedAnalyses::none();
    }
}  ;

void registerFunctionTimer(PassBuilder &PB) {
    PB.registerPipelineParsingCallback(
        [](StringRef Name,
           ModulePassManager &MPM,
           ArrayRef<PassBuilder::PipelineElement>){
            if (Name == "function-timer") {
                MPM.addPass(FunctionTimerPass());
                return true;
            }
            return false;
        }
    );
}


extern "C" LLVM_ATTRIBUTE_WEAK PassPluginLibraryInfo llvmGetPassPluginInfo(){
        return {
            LLVM_PLUGIN_API_VERSION,
            "FunctionTimer",
            LLVM_VERSION_STRING 
            ,registerFunctionTimer 
    } ;} 

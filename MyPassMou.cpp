#include "llvm/ADT/SetVector.h"
#include "llvm/Pass.h"
#include "llvm/ADT/Statistic.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Function.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/Transforms/IPO/PassManagerBuilder.h"
#include "llvm/Support/Format.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/Constants.h"
#include <set>

using namespace llvm;

namespace {
    struct MyPassMou : public ModulePass {
        static char ID;
        MyPassMou() : ModulePass(ID) {}
        
        bool runOnModule(Module &M) override {
            errs() << "MyPassMou: ";
            errs().write_escaped(M.getName()) << '\n';
            
            for (Module::global_iterator mo = M.global_begin(); mo != M.global_end(); ++mo) {
                mo->dump();
                mo->getType()->dump();
                errs() << "isVectorTy: " << mo->getType()->isVectorTy() << '\n';
                errs() << "isPointerTy: " << mo->getType()->isPointerTy() << '\n';
                errs() << "isSized: " << mo->getType()->isSized() << '\n';
                errs() << "isHalfTy: " << mo->getType()->isHalfTy() << '\n';
                errs() << "isVoidTy: " << mo->getType()->isVoidTy() << '\n';
                errs() << "isArrayTy: " << mo->getType()->isArrayTy() << '\n';
                errs() << "isEmptyTy: " << mo->getType()->isEmptyTy() << '\n';
                errs() << "isTokenTy: " << mo->getType()->isTokenTy() << '\n';
                errs() << "isMetadataTy: " << mo->getType()->isMetadataTy() << '\n';
            }

//
//            std::vector<Type *> typeList;
//            typeList.push_back(M.getIdentifiedStructTypes().at(0)->getElementType(1));
//            typeList.push_back(M.getIdentifiedStructTypes().at(0)->getElementType(1));
//            typeList.push_back(M.getIdentifiedStructTypes().at(0)->getElementType(1));
//            llvm::ArrayRef<llvm::Type *> StructTypelist(typeList);
//            StructType * newST = StructType::create(M.getContext(), StructTypelist, "DoublePointer");
//            M.getIdentifiedStructTypes().push_back(newST);
            return false;
        }
    }; // end of struct Hello
}  // end of anonymous namespace

char MyPassMou::ID = 0;
static RegisterPass<MyPassMou> X("MyPassMou", "MyPassMou Pass",
                             false /* Only looks at CFG */,
                             false /* Analysis Pass */);

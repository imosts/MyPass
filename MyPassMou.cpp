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
#include <iostream>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>
#include <fstream>

using namespace llvm;

namespace {
    struct MyPassMou : public ModulePass {
        static char ID;
        MyPassMou() : ModulePass(ID) {}
        
        bool runOnModule(Module &M) override {
            errs() << "MyPassMou: ";
            errs().write_escaped(M.getName()) << '\n';
            
            std::vector<std::string> lists;
            std::vector<std::string> listsAdd;
//            std::vector<std::string>::iterator listsIter;
            
            std::ifstream open_file("localFunName.txt"); // 读取
            while (open_file) {
                std::string line;
                std::getline(open_file, line);
                auto index=std::find(lists.begin(), lists.end(), line);
                if(!line.empty() && index == lists.end()){
                    lists.push_back(line);
                    errs() << "Save it!" << line << '\n';
                    errs() << line.size() << '\n';
                }
            }
            
            
            
            for (Module::iterator mo = M.begin(); mo != M.end(); ++mo) {
//                errs() << "isDeclarationForLinker: " << mo->isDeclarationForLinker() << '\n';
//                errs() << "isDeclaration: " << mo->isDeclaration() << '\n';
                errs() << mo->getName().str() << '\n';
                
                
                if (!(mo->isDeclaration())) {
                    auto index=std::find(lists.begin(),lists.end(),mo->getName().str());
                    auto indexAdd=std::find(listsAdd.begin(),listsAdd.end(),mo->getName().str());
                    if(index == lists.end() && indexAdd == listsAdd.end()){
                        listsAdd.push_back(mo->getName().str());
                        errs() << "Put it:" << mo->getName().str() << '\n';
                    }else{
                        errs() << "Find it!" << '\n';
//
//
//                        FunctionType *FT = mo->getFunctionType();
//                        std::vector<llvm::Type *> tyList;
//                        for (unsigned i = 0; i < FT->getNumParams(); i++) {
//                            if (FT->getParamType(i)->isPointerTy()) {
//                                tyList.push_back(llvm::PointerType::getUnqual(FT->getParamType(i)));
//                            }else{
//                                tyList.push_back(FT->getParamType(i));
//                            }
//
//                            FT->getParamType(i)->dump();
//
//                        }
                    
//                        llvm::ArrayRef<llvm::Type *> ARtyList(tyList);
//                        FunctionType *newFT = FunctionType::get(FT->getReturnType(), ARtyList, 0);
//                        newFT->dump();
//
//
//                        FT->dump();

//                        mo->mutateType(llvm::PointerType::getUnqual(newFT));
                    
                    }
                    
                }
                
                errs() << '\n';
               
            }
            
            // 写入
            std::ofstream file;
            file.open("localFunName.txt", std::ios::out | std::ios::app | std::ios::binary);
            std::ostream_iterator<std::string> output_iterator(file, "\n");
            std::copy(listsAdd.begin(), listsAdd.end(), output_iterator);


            return false;
        }
    }; // end of struct Hello
}  // end of anonymous namespace

char MyPassMou::ID = 0;
static RegisterPass<MyPassMou> X("MyPassMou", "MyPassMou Pass",
                             false /* Only looks at CFG */,
                             false /* Analysis Pass */);

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
#include "llvm/Transforms/Utils/BasicBlockUtils.h"
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
        
        std::vector<std::string> lists;
        std::vector<std::string> structNameList;
        std::vector<std::string> varNameList;
        std::vector<std::string> listsAdd;
        
        bool runOnModule(Module &M) override {
            errs() << "MyPassMou: ";
            errs().write_escaped(M.getName()) << '\n';
            
            
            //读取文件中已有的函数名，并将不在list的函数名添加到list
            std::ifstream open_file("localFunName.txt");
            while (open_file) {
                std::string line;
                std::getline(open_file, line);
                auto index=std::find(lists.begin(), lists.end(), line);
                if(!line.empty() && index == lists.end()){
                    lists.push_back(line);
                    errs() << "Function Name Save it： " << line << '\n';
                }
            }
            
            //遍历Module，如存在函数的声明，且不在list和listAdd中，将其添加到listAdd中
            for (Module::iterator mo = M.begin(); mo != M.end(); ++mo) {
                errs() << mo->getName().str() << '\n';

                if (!(mo->isDeclaration())) {
                    auto index=std::find(lists.begin(),lists.end(),mo->getName().str());
                    auto indexAdd=std::find(listsAdd.begin(),listsAdd.end(),mo->getName().str());
                    if(index == lists.end() && indexAdd == listsAdd.end()){
                        listsAdd.push_back(mo->getName().str());
                        errs() << "Function Name Put it:" << mo->getName().str() << '\n';
                    }
                }
                errs() << '\n';
            }
            
            //将listAdd的内容写入localFunName.txt
            std::ofstream file;
            file.open("localFunName.txt", std::ios::out | std::ios::app | std::ios::binary);
            std::ostream_iterator<std::string> output_iterator(file, "\n");
            std::copy(listsAdd.begin(), listsAdd.end(), output_iterator);
            
            //读取结构体名，并将不在structNameList的结构体名添加到structNameList（除去重复的名称）
            std::ifstream open_file_struct("StructName.txt");
            while (open_file_struct) {
                std::string line_struct;
                std::getline(open_file_struct, line_struct);
                auto index=std::find(structNameList.begin(), structNameList.end(), line_struct);
                if(!line_struct.empty() && index == structNameList.end()){
                    structNameList.push_back(line_struct);
                    errs() << "Struct Name Save it: " << line_struct << '\n';
                }
            }
            
            //读取变量名，并将不在varNameList的变量名添加到varNameList（除去重复的名称）
            std::ifstream open_file_var("VarName.txt");
            while (open_file_var) {
                std::string line_var;
                std::getline(open_file_var, line_var);
                auto index=std::find(varNameList.begin(), varNameList.end(), line_var);
                if(!line_var.empty() && index == varNameList.end()){
                    varNameList.push_back(line_var);
                    errs() << "Var Name Save it: " << line_var << '\n';
                }
            }
            
            errs() << '\n';
            
            for(auto *S : M.getIdentifiedStructTypes())
            {
                S->dump();
            }
            
            Module::global_iterator Ea;
            for (Module::global_iterator gi = M.global_begin(); gi != M.global_end(); ++gi) {
                gi->dump();
                errs() << "Name:" << gi->getName() << '\n';
                auto index=std::find(varNameList.begin(), varNameList.end(), gi->getName().str());
                if(index != varNameList.end()){
                    errs() << "Global:" << gi->getName() << '\n';
                    gi->dump();
                    gi->getType()->getContainedType(0)->dump();
                    errs() << gi->getType()->getContainedType(0)->getContainedType(0)->getStructName() << '\n';
                    gi->getType()->getContainedType(0)->getContainedType(0)->dump();
                    
                    
                    
//                    if (StructType * ST = dyn_cast<StructType>(gi->getType()->getContainedType(0)->getContainedType(0))) {
//                        std::vector<Type *> typeList;
//                        for (unsigned i = 0; i < ST->getNumElements(); i++) {
//                            ST->getElementType(i)->dump();
//                            if (ST->getElementType(i)->isPointerTy()) {
//                                if (StructType *STIn = dyn_cast<StructType>(ST->getElementType(i)->getContainedType(0))) {
//                                    typeList.push_back(llvm::PointerType::getUnqual(ST->getElementType(i)));
//                                }
//
//                            }else{
//                                typeList.push_back(ST->getElementType(i));
//                            }
//                        }
//
//                        llvm::ArrayRef<llvm::Type *> StructTypelist(typeList);
//                        std::string name = ST->getName().str() + ".DoublePointer";
//                        StructType * newST = StructType::create(M.getContext(), StructTypelist, name);
//                        ArrayType *AT = ArrayType::get(newST, 16);
                    
                    gi->getType()->getContainedType(0)->getContainedType(0)->dump();
                    errs() << "changeStructTypeToDP:" << '\n';
                    Type *T = changeStructTypeToDP(M, gi->getType()->getContainedType(0)->getContainedType(0));
                    errs() << "*T: " << *T << '\n';
                    T->dump();
                    GlobalVariable *GV;
                    if (ArrayType *AT = dyn_cast<ArrayType>(gi->getValueType())) {
                        unsigned num = AT->getNumElements();
                        ArrayType *newAT = ArrayType::get(T, num);
                        GV = new GlobalVariable(M, newAT, gi->isConstant(), gi->getLinkage(), Constant::getNullValue(newAT), gi->getName()+"DP", &(*gi));
                        gi->mutateType(llvm::PointerType::getUnqual(newAT));
                    }else{
                        GV = new GlobalVariable(M, T, gi->isConstant(), gi->getLinkage(), Constant::getNullValue(T), gi->getName()+"DP", &(*gi));
                        gi->mutateType(llvm::PointerType::getUnqual(T));
                    }

                    GV->copyAttributesFrom(&(*gi));
                    GV->dump();
                    gi->dump();
                    GV->getValueType()->dump();
                    gi->getValueType()->dump();
                    GV->getType()->dump();
                    gi->getType()->dump();
                    gi->replaceAllUsesWith(GV);
                    
                    errs() << '\n';
                    
                    
//                        if (GlobalObject *GO = dyn_cast<GlobalObject>(gi)) {
//                            GO->mutateType(llvm::PointerType::getUnqual(AT));
//
//                            errs() << "DEBUG:" << '\n';
//
//                            GlobalVariable *GV = new GlobalVariable(M, AT, gi->isConstant(), gi->getLinkage(), Constant::getNullValue(AT), "Strings", &(*gi));
//
//                            GV->copyAttributesFrom(&(*gi));
//
//                            gi->replaceAllUsesWith(GV);
//
//                        }
                    
//                    }
                }
                
                errs() << '\n';
            }
            
            for(auto *S : M.getIdentifiedStructTypes())
            {
                S->dump();
            }


            return false;
        }
        
        Type * changeStructTypeToDP(Module &M, Type * T){
            if (StructType *ST = dyn_cast<StructType>(T)) {
                std::vector<Type *> typeList;
                Type *tmp;
                bool isFind = false;
                std::string name = "";
                if (ST->hasName()) {
                    name = ST->getStructName().str();
                    errs() << name << '\n';
                }

                std::string StrName = name + ".DoublePointer";

                for(auto *S : M.getIdentifiedStructTypes()){
                    if (StrName == S->getStructName().str()) {
                        isFind = true;
                        errs() << "Find it!:" << '\n';
                        return S;
                    }
                }
                
                for (unsigned i = 0; i < ST->getNumElements(); i++) {
                    errs() << "CT:";
                    ST->getElementType(i)->dump();
                    
                    if (ST->getElementType(i)->isPointerTy() && !(ST->getElementType(i)->getContainedType(0)->isFunctionTy())) {
                        errs() << "CT1:" << '\n';
                        if (StructType * tST = dyn_cast<StructType>(ST->getElementType(i)->getContainedType(0))) {
                            if (!(tST->hasName())) {
                                tmp = ST->getElementType(i);
                            }else{
                                tmp = llvm::PointerType::getUnqual(llvm::PointerType::getUnqual(changeStructTypeToDP(M, ST->getElementType(i)->getContainedType(0))));
                            }
                        }else{
                            tmp = llvm::PointerType::getUnqual(llvm::PointerType::getUnqual(changeStructTypeToDP(M, ST->getElementType(i)->getContainedType(0))));
                        }
                        
                    }else if(ST->getElementType(i)->isStructTy()){
                        errs() << "CT2:" << '\n';
                        name = ST->getElementType(i)->getStructName().str();
                        auto index = std::find(structNameList.begin(), structNameList.end(), name);
                        errs() << name << '\n';
                        if (index != structNameList.end() || (name.find("struct.anon") != name.npos) || (name.find("union.anon") != name.npos)) {
                            errs() << "CT21:" << '\n';
                            tmp = changeStructTypeToDP(M, ST->getElementType(i));
                        }else{
                            errs() << "CT22:" << '\n';
                            tmp = ST->getElementType(i);
                        }
                    }else{
                        errs() << "CT3:" << '\n';
                        tmp = ST->getElementType(i);
                    }
                    typeList.push_back(tmp);
                }
                
                llvm::ArrayRef<llvm::Type *> StructTypelist(typeList);
                
                name = ST->getName().str() + ".DoublePointer";
                StructType * newST = StructType::create(M.getContext(), StructTypelist, name);
                
                return newST;
            }else{
                if (T->isPointerTy()) {
                    return llvm::PointerType::getUnqual(T);
                }else{
                    return T;
                }
            }
        }
        
    }; // end of struct Hello
}  // end of anonymous namespace

char MyPassMou::ID = 0;
static RegisterPass<MyPassMou> X("MyPassMou", "MyPassMou Pass",
                             false /* Only looks at CFG */,
                             false /* Analysis Pass */);

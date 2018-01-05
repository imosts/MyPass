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
        std::vector<Type *> typeList;
        
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
            
            //添加free函数
            listsAdd.push_back("free");
            //将listAdd的内容写入localFunName.txt
            std::ofstream file;
            file.open("localFunName.txt", std::ios::out | std::ios::app | std::ios::binary);
            std::ostream_iterator<std::string> output_iterator(file, "\n");
            std::copy(listsAdd.begin(), listsAdd.end(), output_iterator);
            
            
            
            typeList.push_back(Type::getInt64Ty(M.getContext()));
            typeList.push_back(PointerType::getUnqual(PointerType::getUnqual(Type::getInt8Ty(M.getContext()))));
            ArrayRef<Type *> mallocListAR(typeList);
            FunctionType *mallocFT = FunctionType::get(Type::getVoidTy(M.getContext()), mallocListAR, false);
            typeList.pop_back();
            typeList.pop_back();
            Value* mallocFunc = Function::Create(mallocFT, Function::ExternalLinkage, "safeMalloc" , &M);
            
            typeList.push_back(PointerType::getUnqual(PointerType::getUnqual(Type::getInt8Ty(M.getContext()))));
            ArrayRef<Type *> freeListAR(typeList);
            FunctionType *freeFT = FunctionType::get(Type::getVoidTy(M.getContext()), freeListAR, false);
            typeList.pop_back();
            Value* freeFunc = Function::Create(freeFT, Function::ExternalLinkage, "safeFree" , &M);
            
            typeList.push_back(PointerType::getUnqual(PointerType::getUnqual(Type::getInt8Ty(M.getContext()))));
            typeList.push_back(PointerType::getUnqual(PointerType::getUnqual(PointerType::getUnqual(Type::getInt8Ty(M.getContext())))));
            ArrayRef<Type *> traceDPListAR(typeList);
            FunctionType *traceDPFT = FunctionType::get(Type::getVoidTy(M.getContext()), traceDPListAR, false);
            typeList.pop_back();
            typeList.pop_back();
            Value* traceDPFunc = Function::Create(traceDPFT, Function::ExternalLinkage, "tracePoint" , &M);
            
            typeList.push_back(PointerType::getUnqual(PointerType::getUnqual(Type::getInt8Ty(M.getContext()))));
            typeList.push_back(PointerType::getUnqual(PointerType::getUnqual(Type::getInt8Ty(M.getContext()))));
            typeList.push_back(PointerType::getUnqual(Type::getInt8Ty(M.getContext())));
            ArrayRef<Type *> getPtrListAR(typeList);
            FunctionType *getPtrFT = FunctionType::get(Type::getVoidTy(M.getContext()), getPtrListAR, false);
            typeList.pop_back();
            typeList.pop_back();
            typeList.pop_back();
            Value* getPtrFunc = Function::Create(getPtrFT, Function::ExternalLinkage, "getPtr" , &M);
            
//            typeList.push_back(PointerType::getUnqual(PointerType::getUnqual(Type::getInt8Ty(M.getContext()))));
//            ArrayRef<Type *> LoadListAR(typeList);
//            FunctionType *LoadFT = FunctionType::get(PointerType::getUnqual(Type::getInt8Ty(M.getContext())), LoadListAR, false);
//            typeList.pop_back();
//            Value* LoadFunc = Function::Create(LoadFT, Function::LinkOnceAnyLinkage , "Load" , &M);
            
            return true;
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
                    ST->getElementType(i)->dump();
                    
                    if (ST->getElementType(i)->isPointerTy() && !(ST->getElementType(i)->getContainedType(0)->isFunctionTy())) {
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
                        name = ST->getElementType(i)->getStructName().str();
                        
                        if (name.find("struct.") != name.npos) {
                            name = name.substr(7, name.length() - 7);
                        }else if (name.find("union.") != name.npos){
                            name = name.substr(6, name.length() - 6);
                        }
                        
                        auto index = std::find(structNameList.begin(), structNameList.end(), name);
                        errs() << name << '\n';
                        if (index != structNameList.end() || (name.find("anon") != name.npos) || (name.find("anon") != name.npos)) {
                            tmp = changeStructTypeToDP(M, ST->getElementType(i));
                        }else{
                            tmp = ST->getElementType(i);
                        }
                    }else{
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
        
        int pointerLevel(Type *T){
            int i = 0;
            if (T->isPointerTy()) {
                while (T->getContainedType(0)->isPointerTy()) {
                    T = T->getContainedType(0);
                    i++;
                }
                i++;
            }
            return i;
        }
        
        Type* getSouType(Type *T, int level){
            if (level < 1) {
                return T;
            }else{
                for (int i = 0; i < level; i++) {
                    T = T->getContainedType(0);
                }
                return T;
            }
        }
        
        Type* getPtrType(Type *T, int level){
            if (level < 1) {
                return T;
            }else{
                for (int i = 0; i < level; i++) {
                    T = llvm::PointerType::getUnqual(T);
                }
                return T;
            }
        }
        
    }; // end of struct
}  // end of anonymous namespace

char MyPassMou::ID = 0;
static RegisterPass<MyPassMou> X("MyPassMou", "MyPassMou Pass",
                             false /* Only looks at CFG */,
                             false /* Analysis Pass */);

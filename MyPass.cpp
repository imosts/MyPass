//===- MyPass.cpp - Example code from "Writing an LLVM Pass" ---------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This file implements two versions of the LLVM "MyPass" pass described
// in docs/WritingAnLLVMPass.html
//
//===----------------------------------------------------------------------===//

#include "llvm/ADT/Statistic.h"
#include "llvm/IR/Function.h"
#include "llvm/Pass.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/Transforms/IPO/PassManagerBuilder.h"
#include "llvm/Support/Format.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/Constants.h"
#include <set>

using namespace llvm;

#define DEBUG_TYPE "MyPass"
#define INI_TYPE_MEMSET 1
#define INI_TYPE_MEMCPY 2

//STATISTIC(MyPassCounter, "Counts number of functions greeted");

namespace {
    struct MyPass : public FunctionPass {
        static char ID; // Pass identification, replacement for typeid
        bool flag;
        
        unsigned ini_type = 0;
        
        MyPass() : FunctionPass(ID) {}
        MyPass(bool flag) : FunctionPass(ID) {
            this->flag = flag;
        }
        
        std::set<StringRef> ValueName;
        
        bool runOnFunction(Function &F) override {
//            errs() <<"Debug0\n";
//            if(this->flag){
                Function *tmp = &F;
                // 遍历函数中的所有基本块
//                errs() <<"Debug1\n";
                for (Function::iterator bb = tmp->begin(); bb != tmp->end(); ++bb) {
                    // 遍历基本块中的每条指令
                    for (BasicBlock::iterator inst = bb->begin(); inst != bb->end(); ++inst) {
                        //遍历指令的操作数
                        inst->dump();
                        
                        
                        
//                        //对于call指令，获取函数名称 和 参数
//                        if (inst->getOpcode() == Instruction::Call) {
//                            if (CallInst *CI = dyn_cast<CallInst>(inst)) {
//                                Function *f = CI->getCalledFunction();
//                                if (f) {
//                                    errs() << "Function Argument:" << '\n';
//                                    for (Function::arg_iterator ai = f->arg_begin(), ae = f->arg_end(); ai != ae; ++ai) {
//                                        ai->dump();
//                                        errs() << " ";
//                                    }
//                                }
//                            }
//                        }
//
                        /*对于alloca指令，若创建类型为指针，指令名不变，拓展一级
                         *对于一级指针，拓展之后，新建一个一级指针使拓展的指针指向改一级指针
                         *TODO 新建的指针使用放置在堆上
                         *TODO 是否可能是指针向量？？？
                         *TODO 对于函数参数的alloca 不能处理为多级指针
                         */
                        if (inst->getOpcode() == Instruction::Alloca) {
                            if (AllocaInst * AI = dyn_cast<AllocaInst>(inst)) {
//                                AI->getAllocatedType()->dump();
//                                AI->getArraySize()->dump();
                                
                                if (AI->getAllocatedType()->isPointerTy()) {
                                    if (AI->getAllocatedType()->getContainedType(0)->isPointerTy()) {
                                        AI->setAllocatedType(AI->getType());
                                        AI->setName("n" + AI->getName());
                                        ((Value *)AI)->mutateType((llvm::PointerType::getUnqual(AI->getAllocatedType())));
                                    }else{
                                        AllocaInst *addAlloca = new AllocaInst(AI->getAllocatedType(), "al", &(*inst));
                                        AI->setAllocatedType(AI->getType());
                                        AI->setName("n" + AI->getName());
                                        ((Value *)AI)->mutateType((llvm::PointerType::getUnqual(AI->getAllocatedType())));
                                        
                                        if (Instruction *insert = &(*(inst->getNextNode()))) {
                                            StoreInst *addStore = new StoreInst((Value *)addAlloca, (Value *)AI, insert);
                                        }else{
                                            StoreInst *addStore = new StoreInst((Value *)addAlloca, (Value *)AI, &(*bb));
                                        }
                                        ValueName.insert(addAlloca->getName());
                                        for (BasicBlock::iterator in = bb->begin(); in != bb->end(); ++in) {
                                            if (in->getOpcode() == Instruction::Store) {
                                                if (in->getOperand(1) == AI && isa<ConstantPointerNull>(in->getOperand(0))) {
                                                    in->eraseFromParent();
                                                    break;
                                                }
                                            }
                                        }

                                    }
                                    //在改变变量的集合中添加该变量名
                                    ValueName.insert(AI->getName());
                                }else if(AI->getAllocatedType()->isArrayTy()){
                                    //处理指针数组
                                    if (AI->getAllocatedType()->getArrayElementType()->isPointerTy()) {
                                        ArrayType *AT = dyn_cast<ArrayType>(AI->getAllocatedType());
                                        int eleNum = AT->getNumElements();
                                        Type *eleType = AT->getElementType();
                                        Type *mulPtrTy = llvm::PointerType::getUnqual(eleType);
                                        ArrayType *newArrTy = ArrayType::get(mulPtrTy, eleNum);
                                        //变为高一级的指针数组
                                        AI->setAllocatedType(newArrTy);
                                        AI->mutateType(llvm::PointerType::getUnqual(newArrTy));
                                        AI->setName("na" + AI->getName());
                                        
                                        if (!(eleType->getContainedType(0)->isPointerTy())) {
                                            //若为一级以上的指针数组
                                            //则变为二级指针后还需，创建同样数量的一级指针，并让二级指针指向一级指针
                                            Type * destType;
                                            
                                            //新增的一级级指针
                                            AllocaInst *addArrAlloca = new AllocaInst(AT, "aal", &(*inst));
                                            
                                            BasicBlock::iterator in;
                                            Function *f;
                                            CallInst *cIn;
                                            
                                            for (in = bb->begin(); in != bb->end(); ++in) {
                                                if (in->getOpcode() == Instruction::BitCast && (&(*in))->getOperand(0) == AI) {
                                                    
                                                    BitCastInst *oldBC = dyn_cast<BitCastInst>(in);
                                                    destType = oldBC->getDestTy();
                                                    if (cIn = dyn_cast<CallInst>(&(*(in->getNextNode())))) {
                                                        f = cIn->getCalledFunction();
                                                        if (f->getName().contains("llvm.memset.")) {
                                                            ini_type = INI_TYPE_MEMSET;
                                                            break;
                                                        }
                                                        if (f->getName().contains("llvm.memcpy.")) {
                                                            ini_type = INI_TYPE_MEMCPY;
                                                            break;
                                                        }
                                                    }
                                                }
                                            }
                                            
                                            //后续修改写法，TODO 注意后面没有指令
                                            BasicBlock::iterator in2;
                                            
                                            
                                            //为新增的指针数组初始化
                                            if (in != bb->end() && ini_type == INI_TYPE_MEMSET) {
                                                Type * destTy = in->getType();
                                                
                                                destTy->dump();
                                                
                                                //为新增的指针数组创建bitcast指令
                                                BitCastInst *insetCast = (BitCastInst *) CastInst::Create(Instruction::BitCast, addArrAlloca, destTy, "nc", &(*in));
                                                
                                                insetCast->dump();
                                                
                                                //若初始化为空指针
                                                ArrayRef< OperandBundleDef >  memsetArg;
                                                Function::ArgumentListType &argList = f->getArgumentList();
                                                
                                                CallInst *newIn = CallInst::Create(cIn, memsetArg, &(*in));
                                                
                                                newIn->setArgOperand(0, insetCast);
                                                
                                                in2 = in;
                                                ++in2;
                                                ++in2;
                                            }else if(in != bb->end() && ini_type == INI_TYPE_MEMCPY){
                                                in2 = in;
                                                ++in2;
                                                ++in2;

                                            }else{
                                                //若初始化为非空指针
                                                in2 = inst;
                                                ++in2;
                                            }
                                            
                                            
                                            //建立二级指针与一级指针的关系
                                            
                                            std::vector<Value *> indexList;
                                            indexList.push_back(ConstantInt::get(Type::getInt64Ty(bb->getContext()), 0, false));
                                            
                                            
                                            
                                            for (int i = 0; i < eleNum; i++) {
                                                indexList.push_back(ConstantInt::get(Type::getInt64Ty(bb->getContext()), i, false));
                                                
                                                llvm::ArrayRef<llvm::Value *> GETidexList(indexList);
                                                GetElementPtrInst *iniPtrArrNew = GetElementPtrInst::CreateInBounds(addArrAlloca, GETidexList, "ign", &(*(in2)));
                                                GetElementPtrInst *iniPtrArrOld = GetElementPtrInst::CreateInBounds(&(*AI), GETidexList, "igo", &(*(in2)));
                                                StoreInst *instStore = new StoreInst::StoreInst(iniPtrArrNew, iniPtrArrOld, &(*(in2)));
                                                indexList.pop_back();
                                            }
                                            
                                            if (in != bb->end() && ini_type == INI_TYPE_MEMCPY) {
                                                BitCastInst *oldBC = dyn_cast<BitCastInst>(in);
                                                if (oldBC->getSrcTy()->getContainedType(0) != oldBC->getType()) {
                                                    oldBC->mutateType(llvm::PointerType::getUnqual(oldBC->getType()));
                                                    BitCastInst *oldBC = NULL;
                                                    //获取memcpy中源地址的参数 和 信息 TODO：通过函数 参数 一步一步获取
                                                    if (CallInst *CI = dyn_cast<CallInst>(in->getNextNode())) {
                                                        Function *f = CI->getCalledFunction();
                                                        Value *csIV = NULL;
                                                        User *csSV = NULL;
                                                        
                                                        csIV = CI->getOperand(1);
                                                        csSV = dyn_cast<User>(csIV);
                                                        errs() << '\n';
                                                        
                                                        
                                                        
                                                        ArrayType *cssAType = NULL;
                                                        Type *cssType = NULL;
                                                        int num = 1;
                                                        
                                                        if (csSV) {
                                                            if (csSV->getOperandUse(0)->getType()->getContainedType(0)->isArrayTy()) {
                                                                cssAType = (ArrayType *)csSV->getOperandUse(0)->getType()->getContainedType(0);
                                                                errs() << "cssAType->dump():";
                                                                cssAType->dump();
                                                            }else{
                                                                cssType = csSV->getType();
                                                                errs() << "cssType->dump():";
                                                                cssType->dump();
                                                            }
                                                            
                                                            
                                                            
                                                            if (cssAType) {
                                                                num = cssAType->getNumElements();
                                                                errs() << cssAType->getNumElements();
                                                            }else{
                                                                num = 1;
                                                            }
                                                            
                                                            errs() << "num:" << num;
                                                            
                                                            std::vector<Value *> cssIndexList;
                                                            cssIndexList.push_back(ConstantInt::get(Type::getInt64Ty(bb->getContext()), 0, false));
                                                            
                                                            
                                                            
                                                            for (int i = 0; i < num; i++) {
                                                                cssIndexList.push_back(ConstantInt::get(Type::getInt64Ty(bb->getContext()), i, false));
                                                                
                                                                llvm::ArrayRef<llvm::Value *> GETcssIdexList(cssIndexList);
                                                                GetElementPtrInst *iniPtrArrNew = GetElementPtrInst::CreateInBounds(csSV->getOperand(0), GETcssIdexList, "ign", &(*(in2)));
                                                                LoadInst *iniLoad = new LoadInst::LoadInst(iniPtrArrNew, "ni", &(*(in2)));
                                                                GetElementPtrInst *iniPtrArrOld = GetElementPtrInst::CreateInBounds(&(*AI), GETcssIdexList, "igo", &(*(in2)));
                                                                LoadInst *iniLoadOld = new LoadInst::LoadInst(iniPtrArrOld, "ni", &(*(in2)));
                                                                StoreInst *instStore = new StoreInst::StoreInst(iniLoad, iniLoadOld, &(*(in2)));
                                                                cssIndexList.pop_back();
                                                            }
                                                            errs() << "XXXXXXXXXXXXXXXXXXXXX" << '\n';
                                                            //删除二级指针原有的初始化
                                                            BasicBlock::iterator in3 = in;
                                                            ++in;
                                                            in->eraseFromParent();
                                                            in3->eraseFromParent();
                                                        }
 
                                                    }
                                                    
                                                }
                                            }
                                            
                                        }
                                    }
                                }

                                
                            }
                        }
                        
                        //对于Store指令，判断第二个操作数，若为二级指针
                        if (inst->getOpcode() == Instruction::Store) {
                            if ((ValueName.find(inst->getOperand(1)->getName()) != ValueName.end()) && (ValueName.find(inst->getOperand(0)->getName()) == ValueName.end())) {
                                if (inst->getOperand(0)->getType()->isPointerTy()) {
                                    if (isa<ConstantPointerNull>(inst->getOperand(0))) {
                                        //新建一个一级指针数组
                                        Value *newVal = dyn_cast<Value>(inst->getOperand(0));
                                        newVal->mutateType((llvm::PointerType::getUnqual(newVal->getType())));
                                        newVal->setName("n" + newVal->getName());
                                        //通过原有的一级指针初始化方式，初始化该一级指针数组
                                        
                                    }
                                }
                            }
                        }
                        
                        
//                        for (Instruction::op_iterator oi = inst->op_begin(), oe = inst->op_end(); oi != oe; ++oi) {
//                            if (Value * V = dyn_cast<Value>(oi)) {
//                                if (V->getType()->isPointerTy()) {
//                                    Value * newVal = dyn_cast<Value>(oi);
//                                    newVal->dump();
//                                    if (ValueName.find(newVal->getName()) == ValueName.end()) {
////                                        errs() << "isNameEmpty:" << newVal->getName().empty() << '\n';
////                                        errs() << "Value: " << newVal->getName().str() << '\n';
//                                        
////                                        Argument * newArg = new Argument(llvm::PointerType::getUnqual(inst->getOperand(0)->getType()));
////                                        newArg->setName("n" + inst->getOperand(0)->getName());
////                                        errs() << "Arg: " << newArg->getName().str() << '\n';
////                                        inst->setOperand(oi->getOperandNo(), newArg);
//                                        
//                                        newVal->mutateType((llvm::PointerType::getUnqual(newVal->getType())));
//                                        
//                                        newVal->setName("n" + newVal->getName());
//                                        errs() << "NewValue: " << oi->get()->getName() << '\n';
//                                        
//                                        if (!newVal->getName().empty()) {
//                                            ValueName.insert(newVal->getName());
//                                        }
//                                        
//                                    }else{
//                                        errs() << "stay Type!" << '\n';
//                                        //newVal->mutateType(newVal->getType());
//                                    }
//                                }
//                            }
//                        }
                        
                        inst->dump();
                        errs() << '\n';
                        
                        
                        if (inst->getOpcode() == Instruction::Load) {
                            LoadInst *newLoad = dyn_cast<LoadInst>(inst);
                            Value *Val = newLoad->getOperand(0);
                            if (inst->getType() != Val->getType()->getContainedType(0)) {
                                if (newLoad->getType()->isPointerTy()) {
//                                    Val->getType()->dump();
                                    Val->getType()->getContainedType(0);
//                                    inst->getType()->dump();
                                    inst->mutateType(Val->getType()->getContainedType(0));
                                    inst->setName("nl" + inst->getName());
                                }else{
                                    errs() << "InsertLoad:" << '\n';
//                                    Val->getType()->dump();
                                    Val->getType()->getContainedType(0)->dump();
//                                    inst->getType()->dump();
                                    LoadInst *insertLoad = new LoadInst(Val, "il", &(*inst));
                                    inst->setOperand(0, insertLoad);
                                }
                            }
                        }
                        
                        //TODO:ContainedType is ArraryPointer
                        //TODO:考虑结构体的获取内部类型
                        if (inst->getOpcode() == Instruction::GetElementPtr) {
                            GetElementPtrInst *newGEP = dyn_cast<GetElementPtrInst>(inst);
                            Value *Val = newGEP->getOperand(0);
                            
                            //TODO:ContainedType is ArraryPointer
                            
                            Type *SouContainType = Val->getType();

                            for (unsigned i = 1; i < newGEP->getNumOperands(); i++) {
                                ConstantInt *a = dyn_cast<ConstantInt>(newGEP->getOperand(i));
                                SouContainType = SouContainType->getContainedType(0);
                            }
                            
//                            newGEP->getSourceElementType()->dump();
//                            newGEP->getResultElementType()->dump();
//                            newGEP->getType()->dump();
//                            Val->getType()->getContainedType(0)->dump();
//                            SouContainType->dump();
                            
                            if (newGEP->getResultElementType() != SouContainType) {
                                if (Val->getType()->getContainedType(0)->isArrayTy()) {
                                    
                                    newGEP->mutateType(llvm::PointerType::getUnqual(SouContainType));
                                    newGEP->setName("n" + newGEP->getName());
                                    newGEP->setSourceElementType(Val->getType()->getContainedType(0));
                                    newGEP->setResultElementType(SouContainType);
                                }else{
                                    //TODO:还需考虑有无问题
                                    Value *Val = newGEP->getOperand(0);
                                    LoadInst *insertLoad = new LoadInst(Val, "gl", &(*inst));
                                    inst->setOperand(0, insertLoad);
                                }
                                
                            }
                            
//                            errs() << "!!!begin:\n";
//                            errs() << "inst type:\n";
//                            newGEP->getType()->dump();
//                            errs() << "getResultElementType:\n";
//                            newGEP->getResultElementType()->dump();
//                            errs() << "getSourceElementType:\n";
//                            newGEP->getSourceElementType()->dump();
//                            errs() << "getContainedType:\n";
//                            Val->getType()->getContainedType(0)->dump();
//                            errs() << "over!!!\n";
                        }
                        
                        
                        //指针级数更加，当Store指令操作数不符合，添加一个load指令（即读取指针内容，变为原级指针）
                        if (inst->getOpcode() == Instruction::Store) {
                            if (StoreInst *SI = dyn_cast<StoreInst>(inst)) {
                                Value * SPtr = SI->getPointerOperand();
                                Value * SVal = SI->getValueOperand();
                                if ((llvm::PointerType::getUnqual(SVal->getType())) != SPtr->getType()) {
                                    errs() << "!!!Store Ptr not match Val:" << '\n';
                                    SI->dump();
                                    if ((llvm::PointerType::getUnqual(llvm::PointerType::getUnqual(SVal->getType()))) == SPtr->getType()) {
                                        const Twine *name = new Twine::Twine("l");
                                        LoadInst * insert = new LoadInst(SPtr, *name, &(*inst));
                                        SI->setOperand(1, insert);
                                    }
                                }
                            }
                        }
                        
                        //对于Call指令
                        if (inst->getOpcode() == Instruction::Call) {
                            CallInst * test = dyn_cast<CallInst>(inst);
                            errs() << "isBuiltin:" << test->isNoBuiltin();
                        }
                        
                        
//                            }
//                        }
                        
//                        if (inst->getOpcode() == Instruction::Alloca) {
//                            errs() << "Debug1" << '\n';
//                            
//                            if (inst->getOperand(0)->getType()->getTypeID() == llvm::Type::PointerTyID) {
//                                errs() << "Debug2" << '\n';
//                                llvm::Type *ty = llvm::PointerType::getUnqual(inst->getOperand(0)->getType());
//                                AllocaInst *tem = new AllocaInst(llvm::Type::getInt32Ty(inst->getContext()), inst->getName(), &(*bb));
//    
//                                AllocaInst *tem = new AllocaInst(llvm::Type::getInt32Ty(inst->getContext()), (const Twine)inst->getName());
//                                ReplaceInstWithInst(inst->getParent()->getInstList(), inst, tem);
//                                errs() << "Repalced!" << '\n';
//                                return false;
//                            }
//                        }
                        
//                        if (inst->getOpcode() == Instruction::Store) {
//                            StoreInst *store = dyn_cast<StoreInst>(inst);
//                            Value * ptr_addr = store->getPointerOperand();
//                            Value * obj_addr = store->getValueOperand();
//                            
//                            if (GetElementPtrInst *GEP = dyn_cast<GetElementPtrInst>(obj_addr)) {
//                                Value * GEPgp = GEP->getPointerOperand();
//                                if (LoadInst * LI = dyn_cast<LoadInst>(GEPgp)) {
//                                    errs() << "gep_addr:" << LI << '\n';
//                                    errs() << "*gep_addr:" << *LI << '\n';
//                                }else{
//                                    errs() << "Not LI" << '\n';
//                                }
//                            }else{
//                                errs() << "Not GEP" << '\n';
//                            }
//                            
//                            
//                            
//                            errs() << "ptr_addr:" << ptr_addr << '\n';
//                            errs() << "obj_addr:" << obj_addr << '\n';
//                            errs() << "*ptr_addr:" << *ptr_addr << '\n';
//                            errs() << "*obj_addr:" << *obj_addr << '\n';
//                        }
                        
                    }
                    
                    for (BasicBlock::iterator inst = bb->begin(); inst != bb->end(); ++inst) {
                        inst->dump();
                        if (inst->getOpcode() == Instruction::Store) {
                            if (StoreInst *SI = dyn_cast<StoreInst>(inst)) {
                                Value * SPtr = SI->getPointerOperand();
                                Value * SVal = SI->getValueOperand();
                                if ((llvm::PointerType::getUnqual(SVal->getType())) != SPtr->getType()) {
                                    errs() << "@@@Store Ptr not match Val@@@" << '\n';
                                }
                            }
                            
                        }

                    }
                }
            return true;
    };
        
    };
}

char MyPass::ID = 0;
static RegisterPass<MyPass> X("MyPass", "MyPass Pass");

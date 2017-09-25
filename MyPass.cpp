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

//STATISTIC(MyPassCounter, "Counts number of functions greeted");

namespace {
    struct MyPass : public FunctionPass {
        static char ID; // Pass identification, replacement for typeid
        bool flag;
        
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
                        
                        /*对于alloca指令，若创建类型为指针，指令名不变，拓展一级
                         *对于一级指针，拓展之后，新建一个一级指针使拓展的指针指向改一级指针
                         *TODO 新建的指针使用放置在堆上
                         *TODO 是否可能是指针向量？？？
                         */
                        if (inst->getOpcode() == Instruction::Alloca) {
                            if (AllocaInst * AI = dyn_cast<AllocaInst>(inst)) {
                                AI->getAllocatedType()->dump();
                                AI->getArraySize()->dump();
                                
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
                                    errs() << "AI->getName() :" << AI->getName();
                                    ValueName.insert(AI->getName());
                                }else if(AI->getAllocatedType()->isArrayTy()){
                                    if (AI->getAllocatedType()->getArrayElementType()->isPointerTy()) {
                                        ArrayType *AT = dyn_cast<ArrayType>(AI->getAllocatedType());
                                        int eleNum = AT->getNumElements();
                                        Type *eleType = AT->getElementType();
                                        Type *mulPtrTy = llvm::PointerType::getUnqual(eleType);
                                        ArrayType *newArrTy = ArrayType::get(eleType, eleNum);
                                        if (eleType->getContainedType(0)->isPointerTy()) {
                                            //若为二级以上的指针数组
                                            
                                        }else{
                                            //若为一级以上的指针数组
                                            //则变为二级指针后还需，创建同样数量的一级指针，并让二级指针指向一级指针
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
                                        Value *newVal = dyn_cast<Value>(inst->getOperand(0));
                                        newVal->mutateType((llvm::PointerType::getUnqual(newVal->getType())));
                                        newVal->setName("n" + newVal->getName());
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
                                    Val->getType()->dump();
                                    Val->getType()->getContainedType(0);
                                    inst->getType()->dump();
                                    inst->mutateType(Val->getType()->getContainedType(0));
                                    inst->setName("nl" + inst->getName());
                                }else{
                                    errs() << "InsertLoad:" << '\n';
                                    Val->getType()->dump();
                                    Val->getType()->getContainedType(0)->dump();
                                    inst->getType()->dump();
                                    LoadInst *insertLoad = new LoadInst(Val, "il", &(*inst));
                                    inst->setOperand(0, insertLoad);
                                }
                            }
                        }
                        
                        //TODO:ContainedType is ArraryPointer
                        if (inst->getOpcode() == Instruction::GetElementPtr) {
                            GetElementPtrInst *newGEP = dyn_cast<GetElementPtrInst>(inst);
                            Value *Val = newGEP->getOperand(0);
                            
                            if (newGEP->getResultElementType() != Val->getType()->getContainedType(0)) {
//                                Type *newS = llvm::PointerType::getUnqual(newGEP->getSourceElementType());
//                                Type *newR = llvm::PointerType::getUnqual(newGEP->getSourceElementType());
//                                newGEP->setSourceElementType(newS);
//                                newGEP->mutateType(llvm::PointerType::getUnqual(newR));
//                                newGEP->setResultElementType(newR);
                                Value *Val = newGEP->getOperand(0);
                                LoadInst *insertLoad = new LoadInst(Val, "gl", &(*inst));
                                inst->setOperand(0, insertLoad);
                            }
                            
                            errs() << "!!!begin:\n";
                            errs() << "inst type:\n";
                            newGEP->getType()->dump();
                            errs() << "getResultElementType:\n";
                            newGEP->getResultElementType()->dump();
                            errs() << "getSourceElementType:\n";
                            newGEP->getSourceElementType()->dump();
                            errs() << "getContainedType:\n";
                            Val->getType()->getContainedType(0)->dump();
                            errs() << "over!!!\n";
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
                        /* 获取更高级指针 设置参数名称
                                errs().write_escaped(inst->getOpcodeName()) << '\n';
                                errs() << "isPointerType:" << inst->getOperand(0)->getType()->isPointerTy() << '\n';
                        if (inst->getOperand(0)->getType()->isPointerTy()) {
                            Value *V = inst->getOperand(0);
                            const Type * T = V->getType();
                            errs() << "inst: " << *inst << '\n';
                            errs() << "Value: " ;
                            V->dump();
                            errs() << "Double Pointer? " << T->getContainedType(0)->isPointerTy() << '\n' ;
                        }
                        
                        inst->dump();
                        
                        Type *test = llvm::PointerType::getUnqual(inst->getOperand(0)->getType());
                        inst->getOperand(0)->getType()->dump();
                        
                        inst->getOperand(0)->setName("new" + inst->getOperand(0)->getName());
                        test->dump();
                        
                        inst->dump();
                        
                        Argument * newArg = new Argument(llvm::PointerType::getUnqual(inst->getOperand(0)->getType()));
                        newArg->setName(inst->getOperand(0)->getName());
                        
                        newArg->dump();
                        
                        errs() << '\n';
                         
                         */
                        
//                        for (Instruction::op_iterator oi = inst->op_begin(), oe = inst->op_end(); oi != oe; ++oi) {
//                            errs() << "getOP:" << ((Value *) oi) << '\n';
//                        }
                        
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
                        
//                        for (unsigned i = 0; i < inst->getNumOperands(); i++) {
//                            if (isa<PointerType>(inst->getOperand(i)->getType())){
//                                errs() << "change OP:" << '\n';
//                                PointerType *temp = llvm::PointerType::getUnqual(inst->getOperand(i)->getType());
//                                inst->set
//                            }
//                        }
                        
                        
//                        for (Instruction::op_iterator OI = inst->op_begin(), OE = inst->op_end(); OI != OE; ++OI)
//                        {
//                            if ((OI->getUser())){
//                                /*
//                                 PointerType *temp = llvm::PointerType::getUnqual(inst->getOperand(OI)->getType());
//                                 
//                                 Value *val = *OI;
//                                 iter = mapClonedAndOrg.find(val);
//                                 
//                                 if( iter != mapClonedAndOrg.end())
//                                 {
//                                 *OI = (Value*)iter->second.PN;
//                                 }
//                                 */
//                                errs() << "OPisPointerType:YES" << OI->getUser() << '\n';
//                            }
//                            
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
    
        
        // a+b === a-(-b)
        void ob_add(BinaryOperator *bo) {
            BinaryOperator *op = NULL;
            
            if (bo->getOpcode() == Instruction::Add) {
                // 生成 (－b)
                op = BinaryOperator::CreateNeg(bo->getOperand(1), "", bo);
                // 生成 a-(-b)
                op = BinaryOperator::Create(Instruction::Sub, bo->getOperand(0), op, "", bo);
                
                op->setHasNoSignedWrap(bo->hasNoSignedWrap());
                op->setHasNoUnsignedWrap(bo->hasNoUnsignedWrap());
            }
            
            // 替换所有出现该指令的地方
            bo->replaceAllUsesWith(op);
        }
        
        void ReplaceInstWithValue(BasicBlock::InstListType &BIL,
                                        BasicBlock::iterator &BI, Value *V) {
            Instruction &I = *BI;
            // Replaces all of the uses of the instruction with uses of the value
            I.replaceAllUsesWith(V);
            
            // Make sure to propagate a name if there is one already.
            if (I.hasName() && !V->hasName())
                V->takeName(&I);
            
            // Delete the unnecessary instruction now...
            BI = BIL.erase(BI);
        }
        
        void ReplaceInstWithInst(BasicBlock::InstListType &BIL,
                                       BasicBlock::iterator &BI, Instruction *I) {
            assert(I->getParent() == nullptr &&
                   "ReplaceInstWithInst: Instruction already inserted into basic block!");
            
            // Copy debug location to newly added instruction, if it wasn't already set
            // by the caller.
            if (!I->getDebugLoc())
                I->setDebugLoc(BI->getDebugLoc());
            
            // Insert the new instruction into the basic block...
            BasicBlock::iterator New = BIL.insert(BI, I);
            
            // Replace all uses of the old instruction, and delete it.
            ReplaceInstWithValue(BIL, BI, I);
            
            // Move BI back to point to the newly inserted instruction
            BI = New;
        }
        
    };
}

char MyPass::ID = 0;
static RegisterPass<MyPass> X("MyPass", "MyPass Pass");

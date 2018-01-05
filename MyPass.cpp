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
#include "llvm/IR/Module.h"
#include "llvm/IR/Function.h"
#include "llvm/Pass.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/Transforms/IPO/PassManagerBuilder.h"
#include "llvm/Support/Format.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/Constants.h"
#include "llvm/Transforms/Utils/BasicBlockUtils.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/Operator.h"
#include "llvm/IR/CallSite.h"

#include <set>
#include <fstream>

using namespace llvm;

#define DEBUG_TYPE "MyPass"
#define INI_TYPE_MEMSET 1
#define INI_TYPE_MEMCPY 2
#define MULTIPTR_OR_NODEPTR 0xC000000000000000
#define MULTIPTR 0x8000000000000000
#define NODEPTR 0x4000000000000000
#define AND_PTR_VALUE 0x00007FFFFFFFFFFF


namespace {
    struct MyPass : public FunctionPass {
        static char ID; // Pass identification, replacement for typeid
        bool flag;
        bool need_bb_iter_begin = false;
        bool isIniGloVar = false;
        bool isFunHasNullPtr = false;
        AllocaInst *FunNullPtr = NULL;
        GlobalVariable *GNPtr = NULL;
        
        unsigned ini_type = 0;
        
        MyPass() : FunctionPass(ID) {}
        MyPass(bool flag) : FunctionPass(ID) {
            this->flag = flag;
        }
        
        std::set<StringRef> ValueName;
        std::vector<std::string> varNameList;
        std::vector<std::string> structNameList;
        std::vector<std::string> localFunName;
        std::vector<Value *> structIndexList;
        
        
        bool runOnFunction(Function &F) override {
            Function *tmpF = &F;
            unsigned FAnum = 0;
            isFunHasNullPtr = false;

            std::ifstream open_file("localFunName.txt"); // 读取
            while (open_file) {
                std::string line;
                std::getline(open_file, line);
                auto index=std::find(localFunName.begin(), localFunName.end(), line);
                if(!line.empty() && index == localFunName.end()){
                    localFunName.push_back(line);
                }
            }
            
            for (Function::iterator bb = tmpF->begin(); bb != tmpF->end(); ++bb) {
                for (BasicBlock::iterator inst = bb->begin(); inst != bb->end(); ++inst) {
                    errs() << "  Inst Before" <<'\n';
                    inst->dump();
                    
                    if (LoadInst *LI = dyn_cast<LoadInst>(inst)) {
                        if (pointerLevel(LI->getOperand(0)->getType()) >= 2) {
                            errs() << "LoadInst Deubg:" << '\n';
                            LI->dump();
                            int useNum = LI->getNumUses();
                            int LIUseNum = 0;
                            int FunArgUseNum = 0;
                            if(useNum >= 1){
                                //BUG:Bitcast转换以后要继续考虑是否为不需要转换的！！！
                                for (BasicBlock::iterator inst2 = inst; inst2 != bb->end(); ++inst2) {
                                    if (useNum <= 0) {
                                        break;
                                    }
                                    if (StoreInst *SI = dyn_cast<StoreInst>(inst2)) {
                                        if (isComeFormLI(SI->getValueOperand(), LI) && SI->getValueOperand()->getType()->isPointerTy()) {
                                            LIUseNum++;
                                            useNum--;
                                            continue;
                                        }
                                    }else if (PtrToIntInst *PTI = dyn_cast<PtrToIntInst>(inst2)) {
                                        if (isComeFormLI(PTI->getOperand(0), LI)) {
                                            LIUseNum++;
                                            useNum--;
                                            continue;
                                        }
                                    }else if(CallInst *CI = dyn_cast<CallInst>(inst2)){
                                        if (Function *fTemp = CI->getCalledFunction()) {
                                            auto index = std::find(localFunName.begin(), localFunName.end(), fTemp->getName().str());
                                            
                                            if (index != localFunName.end()) {
                                                errs() << "find LocalFun!" << '\n';
                                                for (Instruction::op_iterator oi = CI->op_begin(); oi != CI->op_end(); ++oi) {
                                                    if (Value *V = dyn_cast<Value>(oi)) {
                                                        if (isComeFormLI(V, LI)) {
                                                            errs() << "find LocalFun use!" << '\n';
                                                            LIUseNum++;
                                                            FunArgUseNum++;
                                                            useNum--;
                                                            continue;
                                                        }
                                                    }
                                                    
                                                }
                                            }
                                            
                                        }else{
                                            for (Instruction::op_iterator oi = CI->op_begin(); oi != CI->op_end(); ++oi) {
                                                if (Value *V = dyn_cast<Value>(oi)) {
                                                    if (isComeFormLI(V, LI)) {
                                                        errs() << "find Function Pointer LI use!" << '\n';
                                                        LIUseNum++;
                                                        FunArgUseNum++;
                                                        useNum--;
                                                        continue;
                                                    }
                                                }
                                            }
                                        }
                                        
                                    }
                                }
                            }
                            
                            if (useNum > 0) {
                                LoadInst *sameLI = new LoadInst(LI->getPointerOperand(), "sameLI", &(*inst));
                                inst++;
                                Value *phi = insertLoadCheckInBasicBlock(tmpF, bb, inst, sameLI);
                                inst--;
                                LI->replaceAllUsesWith(phi);
                                if (LIUseNum > 0) {
                                    errs() << "FunArgUseNum:" << FunArgUseNum << '\n';
                                    for (BasicBlock::iterator inst2 = inst; inst2 != bb->end(); ++inst2) {
                                        if (LIUseNum <= 0) {
                                            break;
                                        }
                                        if (StoreInst *SI = dyn_cast<StoreInst>(inst2)) {
                                            if (SI->getValueOperand() == phi && SI->getValueOperand()->getType()->isPointerTy()) {
                                                LIUseNum--;
                                                SI->setOperand(0, LI);
                                            }
                                        }else if (PtrToIntInst *PTI = dyn_cast<PtrToIntInst>(inst2)) {
                                            if (PTI->getOperand(0) == phi) {
                                                LIUseNum--;
                                                PTI->setOperand(0, LI);
                                            }
                                        }else if (FunArgUseNum > 0 && dyn_cast<CallInst>(inst2)) {
                                            errs() << "find CallInst!" << '\n';
                                            if (CallInst *CI = dyn_cast<CallInst>(inst2)) {
                                                if (Function *fTemp = CI->getCalledFunction()) {
                                                    auto index = std::find(localFunName.begin(), localFunName.end(), fTemp->getName().str());
                                                    errs() << "chang FunArg:" << '\n';
                                                    if (index != localFunName.end()) {
                                                        for (unsigned i = 0; i < CI->getNumOperands(); ++i) {
                                                            if (Value *V = dyn_cast<Value>(CI->llvm::User::getOperand(i))) {
                                                                if (isComeFormSouce(V, phi)) {
                                                                    errs() << "chang FunArg:" << '\n';
                                                                    CI->dump();
                                                                    LIUseNum--;
                                                                    FunArgUseNum--;
                                                                    SI->setOperand(i, LI);
                                                                }
                                                            }
                                                        }

                                                    }
                                                }else{
                                                    for (unsigned i = 0; i < CI->getNumOperands(); ++i) {
                                                        if (Value *V = dyn_cast<Value>(CI->llvm::User::getOperand(i))) {
                                                            if (isComeFormSouce(V, phi)) {
                                                                errs() << "chang FunArg:" << '\n';
                                                                CI->dump();
                                                                LIUseNum--;
                                                                FunArgUseNum--;
                                                                SI->setOperand(i, LI);
                                                            }
                                                        }
                                                    }
                                                }
                                            }
                                            
                                        }
                                    }
                                }else{
                                    LI->eraseFromParent();
                                }
                                
                            }
                        }
                        
                    }
                    
                    if (CallInst *CI = dyn_cast<CallInst>(inst)) {
                        if (Function *fTemp = CI->getCalledFunction()) {
                            if (CI && CI->getCalledFunction()->getName().equals("free")) {
                                errs() << "free debug!" << '\n';
                                CI->setCalledFunction(tmpF->getParent()->getFunction("safeFree"));
                                LoadInst *LI = (LoadInst *)getFirstLoadPtrOP(CI->getOperand(0));
                                if (LI) {
                                    if (LI->getPointerOperand()->getType() == PointerType::getUnqual(PointerType::getUnqual(Type::getInt8Ty(bb->getContext())))) {
                                        CI->setOperand(0, LI->getPointerOperand());
                                    }else{
                                        LI->dump();
                                        BitCastInst *BCI = new BitCastInst(LI->getPointerOperand(), PointerType::getUnqual(PointerType::getUnqual(Type::getInt8Ty(bb->getContext()))), "", &(*inst));
                                        CI->setOperand(0, BCI);
                                    }

                                }
                            }
                            
                            errs() << CI->getCalledFunction()->getName() << '\n';
                        } else if (BitCastOperator *BCO = dyn_cast<BitCastOperator>(CI->getCalledValue())) {
                            if (BCO->getOperand(0)->getName().equals("free")) {
                                errs() << "free Function!\n";
                                LoadInst *LI = dyn_cast<LoadInst>(CI->getOperand(0));
                                BitCastInst *BCI = NULL;
                                Value *FreeArg = LI->getPointerOperand();
                                if (LI) {
                                    LI->dump();
                                    if (LI->getPointerOperand()->getType() != PointerType::getUnqual(PointerType::getUnqual(Type::getInt8Ty(bb->getContext())))) {
                                        BCI = new BitCastInst(LI->getPointerOperand(), PointerType::getUnqual(PointerType::getUnqual(Type::getInt8Ty(bb->getContext()))), "", &(*inst));
                                    }
                                }
                                
                                if (BCI) {
                                    FreeArg = BCI;
                                }
                                
                                std::vector<Value *> freeArg;
                                freeArg.push_back(FreeArg);
                                ArrayRef<Value *> funcArg(freeArg);
                                Value *func = tmpF->getParent()->getFunction("safeFree");
                                CallInst *nCI = CallInst::Create(func, funcArg, "", &(*inst));
                                inst--;
                                CI->eraseFromParent();
                                inst++;
                                
                            }
                        }
                        
                    }
                    
                    
                    if (StoreInst *SI = dyn_cast<StoreInst>(inst)) {
                        if (isComeFromGEPAndChange(SI->getValueOperand())) {
                            errs() << "SI Value Come from GEP and Ptr Change!\n";
                            if (getComeFromGEPAndChangeOrigin(SI->getValueOperand())) {
                                Value *V = getComeFromGEPAndChangeOrigin(SI->getValueOperand());
                                if (PHINode *phi = dyn_cast<PHINode>(V)) {
                                    if (LoadInst *LI = dyn_cast<LoadInst>(phi->getOperand(0))) {
                                        
                                        std::vector<Value *> getPtrArg;
                                        
                                        BitCastInst *originBCI = new BitCastInst(LI->getPointerOperand(), PointerType::getUnqual(PointerType::getUnqual(Type::getInt8Ty(tmpF->getContext()))), "oriBCI", &(*inst));
                                        getPtrArg.push_back(originBCI);
                                        BitCastInst *oldBCI = new BitCastInst(SI->getPointerOperand(), PointerType::getUnqual(PointerType::getUnqual(Type::getInt8Ty(tmpF->getContext()))), "oldBCI", &(*inst));
                                        getPtrArg.push_back(oldBCI);
                                        BitCastInst *ptrBCI = new BitCastInst(SI->getValueOperand(), PointerType::getUnqual(Type::getInt8Ty(tmpF->getContext())), "ptrBCI", &(*inst));
                                        getPtrArg.push_back(ptrBCI);
                                        
                                        ArrayRef<Value *> funcArg(getPtrArg);
                                        Value *func = tmpF->getParent()->getFunction("getPtr");
                                        CallInst *nCI = CallInst::Create(func, funcArg, "", &(*inst));
                                        inst++;
                                        SI->eraseFromParent();
                                        inst--;
                                    }
                                }else if (GlobalValue *GV = dyn_cast<GlobalValue>(V)) {
                                    errs() << "GlobalValue!\n";
                                    std::vector<Value *> getPtrArg;
                                    
                                    BitCastInst *originBCI = new BitCastInst(V, PointerType::getUnqual(PointerType::getUnqual(Type::getInt8Ty(tmpF->getContext()))), "oriBCI", &(*inst));
                                    getPtrArg.push_back(originBCI);
                                    BitCastInst *oldBCI = new BitCastInst(SI->getPointerOperand(), PointerType::getUnqual(PointerType::getUnqual(Type::getInt8Ty(tmpF->getContext()))), "oldBCI", &(*inst));
                                    getPtrArg.push_back(oldBCI);
                                    BitCastInst *ptrBCI = new BitCastInst(SI->getValueOperand(), PointerType::getUnqual(Type::getInt8Ty(tmpF->getContext())), "ptrBCI", &(*inst));
                                    getPtrArg.push_back(ptrBCI);
                                    
                                    ArrayRef<Value *> funcArg(getPtrArg);
                                    Value *func = tmpF->getParent()->getFunction("getPtr");
                                    CallInst *nCI = CallInst::Create(func, funcArg, "", &(*inst));
                                    errs() << "StoreInst: ";
                                    inst++;
                                    SI->eraseFromParent();
                                    inst--;
                                }
                            }
                        }else if (isSIValueComeFromMalloc(SI)) {
                            CallInst *CI = isSIValueComeFromMalloc(SI);
                            BasicBlock::iterator tmp = inst;
                            bool isBBhead = true;
                            errs() << "malloc debug!" << '\n';
                            tmp++;
                            tmp->dump();
                            changTOSafeMalloc(tmpF, CI, SI, tmp);
                            inst = tmp;
                            inst--;
                        }else if ((pointerLevel(SI->getValueOperand()->getType()) >= 1) && (pointerLevel(SI->getPointerOperand()->getType()) >= 2)) {
                            std::vector<Value *> traceDP;
                            BitCastInst *BCIV = new BitCastInst(SI->getValueOperand(), PointerType::getUnqual(PointerType::getUnqual(Type::getInt8Ty(tmpF->getContext()))), "tBCIV", &(*inst));
                            BitCastInst *BCIP = new BitCastInst(SI->getPointerOperand(), PointerType::getUnqual(PointerType::getUnqual(PointerType::getUnqual(Type::getInt8Ty(tmpF->getContext())))), "tBCIP", &(*inst));
                            traceDP.push_back(BCIV);
                            traceDP.push_back(BCIP);
                            ArrayRef<Value *> funcArg(traceDP);
                            Value *func = tmpF->getParent()->getFunction("tracePoint");
                            CallInst *nCI = CallInst::Create(func, funcArg, "", &(*inst));
                            inst++;
                            SI->eraseFromParent();
                            inst--;
                        }
                    }

                    inst->dump();
                    errs() << "  Inst After" <<'\n' <<'\n';
                }
            }
            return true;

            
    };
        
        Value * insertLoadCheckInBasicBlock(Function* F, Function::iterator &originBB, BasicBlock::iterator &insetPoint, Value *address){
            PtrToIntInst *PTI = new PtrToIntInst(address, Type::getInt64Ty(originBB->getContext()), "", &(*insetPoint));
            BinaryOperator *BO = BinaryOperator::Create(Instruction::BinaryOps::And, PTI, ConstantInt::get(Type::getInt64Ty(originBB->getContext()), MULTIPTR_OR_NODEPTR, false), "", &(*insetPoint));
            ICmpInst *ICM = new ICmpInst(&(*insetPoint), llvm::CmpInst::ICMP_NE, BO, ConstantInt::get(Type::getInt64Ty(originBB->getContext()), 0, false));
            
            BasicBlock *newBB = llvm::SplitBlock(&(*originBB), &(*insetPoint), nullptr, nullptr);
            BasicBlock *oldBB = &(*originBB);
            BasicBlock::iterator inst = originBB->begin();
            newBB->setName("newBasicBlock");
            originBB->setName("oldBasicBlock");
            
            BasicBlock *nBAND = BasicBlock::Create(originBB->getContext(), "ANDBB", F, newBB);
            BranchInst *nBIAND = BranchInst::Create(newBB, nBAND);
            
            BasicBlock *nBmulti = BasicBlock::Create(originBB->getContext(), "multiBB", F, newBB);
            BranchInst *nBINode = BranchInst::Create(newBB, nBmulti);
            
            BranchInst *oldBR = BranchInst::Create(nBAND, newBB, ICM, &(*originBB));
            inst = originBB->end();
            inst--;
            inst--;
            inst->eraseFromParent();
            
            originBB++;
            inst = originBB->begin();
            BinaryOperator *BOAnd = BinaryOperator::Create(Instruction::BinaryOps::And, PTI, ConstantInt::get(Type::getInt64Ty(originBB->getContext()), AND_PTR_VALUE, false), "", &(*inst));
            IntToPtrInst *ITPAnd = new IntToPtrInst(BOAnd, address->getType(), "", &(*inst));
            BinaryOperator *BOmul = BinaryOperator::Create(Instruction::BinaryOps::And, PTI, ConstantInt::get(Type::getInt64Ty(originBB->getContext()), MULTIPTR, false), "", &(*inst));
            ICmpInst *ICMelse = new ICmpInst(&(*inst), llvm::CmpInst::ICMP_NE, BOmul, ConstantInt::get(Type::getInt64Ty(originBB->getContext()), 0, false));
            BranchInst *oldBRelse = BranchInst::Create(nBmulti, newBB, ICMelse, &(*originBB));
            inst->eraseFromParent();
            
            originBB++;
            inst = originBB->begin();
            BitCastInst *multiBCI = new BitCastInst(ITPAnd, PointerType::getUnqual(ITPAnd->getType()), "", &(*inst));
            LoadInst *multiLI = new LoadInst(multiBCI, "", &(*inst));
            PtrToIntInst *PTImulti = new PtrToIntInst(multiLI, Type::getInt64Ty(originBB->getContext()), "", &(*inst));
            BinaryOperator *BOmulte = BinaryOperator::Create(Instruction::BinaryOps::And, PTImulti, ConstantInt::get(Type::getInt64Ty(originBB->getContext()), AND_PTR_VALUE, false), "", &(*inst));
            IntToPtrInst *ITPmulti = new IntToPtrInst(BOmulte, multiLI->getType(), "", &(*inst));
            
            originBB++;
            inst = originBB->begin();
            PHINode *PhiNode = PHINode::Create(address->getType(), 3, "", &(*inst));
            PhiNode->addIncoming(address, oldBB);
            PhiNode->addIncoming(ITPAnd, nBAND);
            PhiNode->addIncoming(ITPmulti, nBmulti);
            return PhiNode;
        }
        
        //判断ICMP的操作数是否来源于Value V
        bool instComeFromVal(Instruction *I, Value *V){
            bool result = false;
            for (unsigned i = 0; i < I->getNumOperands(); i++) {
                if (Value *OPV = dyn_cast<Value>(I->getOperand(i))) {
                    if (OPV == V) {
                        //若Value等于操作数，即ICMP操作数来自于该Value，则返回真
                        return true;
                    }else if (Instruction *OPI = dyn_cast<Instruction>(OPV)){
                        if (GetElementPtrInst *GEP = dyn_cast<GetElementPtrInst>(OPI)) {
                            //若GEP指令操作数超过2个，则读取Value内部数据结构，这种情况不符合情况，返回假
                            if (GEP->getNumOperands() > 2) {
                                return false;
                            }
                        }
                        //若来自函数调用，这种情况不符合情况，返回假
                        if (CallInst *C = dyn_cast<CallInst>(OPV)) {
                            return false;
                        }
                        //函数中排出了一些不符合情况的特例，后面出现BUG，可能需要进一步添加特例
                        //除此之外，递归操作数是指令的的来源
                        result = instComeFromVal(OPI, V);
                    }
                }
            }
            return result;
        };
        
        
        
        //判断源类型是否为结构体（若干级指针或者直接为结构体）
        bool isSouceStructType(Type *T){
            if (T->isPointerTy() || T->isArrayTy()) {
                while (T->getContainedType(0)->isPointerTy()) {
                    T = T->getContainedType(0);
                }
                T = T->getContainedType(0);
            }
            return T->isStructTy();
        }
        
        //返回T的指针级数
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
        
        
        //获取level级指针的源类型
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
        
        //获取指向T类型的level级指针
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
        
        bool isStackPointer(Value *V) {
            if (isa<AllocaInst>(V)) {
                return true;
            }
            if (BitCastInst *BC = dyn_cast<BitCastInst>(V)) {
                return isStackPointer(BC->getOperand(0));
            } else if (PtrToIntInst *PI = dyn_cast<PtrToIntInst>(V)) {
                return isStackPointer(PI->getOperand(0));
            } else if (GetElementPtrInst *GEP = dyn_cast<GetElementPtrInst>(V)) {
                return isStackPointer(GEP->getPointerOperand());
            }
            
            return false;
        }
        
        bool isComeFormSouce(Value *V, Value*S) {
            if (V == S) {
                return true;
            }
            if (BitCastInst *BC = dyn_cast<BitCastInst>(V)) {
                return isComeFormSouce(BC->getOperand(0), S);
            } else if (PtrToIntInst *PI = dyn_cast<PtrToIntInst>(V)) {
                return isComeFormSouce(PI->getOperand(0), S);
            } else if (LoadInst *LI = dyn_cast<LoadInst>(V)) {
                return isComeFormSouce(LI->getPointerOperand(), S);
            }

            return false;
        }
        
        bool isComeFormLI(Value *V, LoadInst *LI) {
            if (V == LI) {
                return true;
            }
            if (BitCastInst *BC = dyn_cast<BitCastInst>(V)) {
                return isComeFormLI(BC->getOperand(0), LI);
            } else if (PtrToIntInst *PI = dyn_cast<PtrToIntInst>(V)) {
                return isComeFormLI(PI->getOperand(0), LI);
            }
            return false;
        }
        
        Value* getFirstLoadPtrOP(Value *V) {
            if (BitCastInst *BC = dyn_cast<BitCastInst>(V)) {
                return getFirstLoadPtrOP(BC->getOperand(0));
            } else if (PtrToIntInst *PI = dyn_cast<PtrToIntInst>(V)) {
                return getFirstLoadPtrOP(PI->getOperand(0));
            } else if (LoadInst *LI = dyn_cast<LoadInst>(V)) {
                return LI;
            }
            return NULL;
        }
        
        bool  isSIComeFromMalloc(StoreInst *SI, Value *M){
            if (SI->getValueOperand() == M) {
                return true;
            }else if (BitCastInst *BCI = dyn_cast<BitCastInst>(SI->getValueOperand())) {
                return isSIComeFromMalloc(SI, BCI);
            }
            return false;
        }
        
        bool  isComeFromGEPAndChange(Value *V){
            if (GetElementPtrInst *GEP = dyn_cast<GetElementPtrInst>(V)) {
                if ((!dyn_cast<ConstantInt>(GEP->getOperand(1))->equalsInt(0)) || (GEP->getNumIndices() > 1)) {
                    return true;
                } else {
                    if (GetElementPtrInst *nGEP = dyn_cast<GetElementPtrInst>(GEP->getPointerOperand())) {
                        return isComeFromGEPAndChange(GEP->getPointerOperand());
                    } else if (BitCastInst *BCI = dyn_cast<BitCastInst>(V)) {
                        return isComeFromGEPAndChange(BCI->getOperand(0));
                    }
                }
            }else if (BitCastInst *BCI = dyn_cast<BitCastInst>(V)) {
                return isComeFromGEPAndChange(BCI->getOperand(0));
            }else if (BitCastOperator *BCO = dyn_cast<BitCastOperator>(V)) {
                return isComeFromGEPAndChange(BCO->getOperand(0));
            }else if (ConstantExpr *CE = dyn_cast<llvm::ConstantExpr>(V)) {
                if (CE->getOpcode() == Instruction::GetElementPtr) {
                    if (CE->getNumOperands() > 1) {
                        return true;
                    }
                }
            }
            return false;
        }
        
        Value *  getComeFromGEPAndChangeOrigin(Value *V){
            if (GetElementPtrInst *GEP = dyn_cast<GetElementPtrInst>(V)) {
                if ((!dyn_cast<ConstantInt>(GEP->getOperand(1))->equalsInt(0)) || (GEP->getNumIndices() > 1)) {
                    return GEP->getPointerOperand();
                } else {
                    if (GetElementPtrInst *nGEP = dyn_cast<GetElementPtrInst>(GEP->getPointerOperand())) {
                        return getComeFromGEPAndChangeOrigin(GEP->getPointerOperand());
                    } else if (BitCastInst *BCI = dyn_cast<BitCastInst>(V)) {
                        return getComeFromGEPAndChangeOrigin(BCI->getOperand(0));
                    }
                }
            }else if (BitCastInst *BCI = dyn_cast<BitCastInst>(V)) {
                return getComeFromGEPAndChangeOrigin(BCI->getOperand(0));
            }else if (BitCastOperator *BCO = dyn_cast<BitCastOperator>(V)) {
                return getComeFromGEPAndChangeOrigin(BCO->getOperand(0));
            }else if (ConstantExpr *CE = dyn_cast<llvm::ConstantExpr>(V)) {
                if (CE->getOpcode() == Instruction::GetElementPtr) {
                    if (CE->getNumOperands() > 1) {
                        return CE->getOperand(0);
                    }
                }
            }
            return NULL;
        }
        
        CallInst* isSIValueComeFromMalloc(Value *V){
            if (CallInst *CI = dyn_cast<CallInst>(V)) {
                if (Function *fTemp = CI->getCalledFunction()) {
                    if (CI->getCalledFunction()->getName().equals("malloc")) {
                        return CI;
                    }
                }
            }else if (StoreInst *SI = dyn_cast<StoreInst>(V)) {
                return isSIValueComeFromMalloc(SI->getValueOperand());
            }else if (BitCastInst *BCI = dyn_cast<BitCastInst>(V)) {
                return isSIValueComeFromMalloc(BCI->getOperand(0));
            }
            return NULL;
        }
        
        void changTOSafeMalloc(Function *tmpF, CallInst *CI, StoreInst *SI, BasicBlock::iterator &nextInst) {
            std::vector<Value *> mallocArg;
            mallocArg.push_back(CI->getArgOperand(0));
            BitCastInst *nBCI = new BitCastInst(SI->getPointerOperand(), PointerType::getUnqual(PointerType::getUnqual(Type::getInt8Ty(tmpF->getContext()))), "mBCI", &(*nextInst));
            mallocArg.push_back(nBCI);
            ArrayRef<Value *> funcArg(mallocArg);
            Value *func = tmpF->getParent()->getFunction("safeMalloc");
            CallInst *nCI = CallInst::Create(func, funcArg, "", &(*nextInst));
            errs() << "changTOSafeMalloc debug13!" << '\n';
            
            if (BitCastInst *BCI = dyn_cast<BitCastInst>(SI->getValueOperand())) {
                errs() << "changTOSafeMalloc debug21!" << '\n';
                errs() << "changTOSafeMalloc debug22!" << '\n';
                if (BCI->getNumUses() > 1) {
                    LoadInst *LI = new LoadInst(SI->getPointerOperand(), "mLI", &(*nextInst));
                    BCI->mutateType(LI->getType());
                    BCI->replaceAllUsesWith(LI);
                    errs() << "changTOSafeMalloc debug11!" << '\n';
                }
                errs() << "changTOSafeMalloc debug23!" << '\n';
                SI->dump();
                BCI->dump();
                CI->dump();
                SI->eraseFromParent();
                errs() << "changTOSafeMalloc debug24!" << '\n';
                BCI->eraseFromParent();
                errs() << "changTOSafeMalloc debug24!" << '\n';
                CI->eraseFromParent();
                errs() << "changTOSafeMalloc debug24!" << '\n';
            }else {
                SI->eraseFromParent();
                CI->eraseFromParent();
            }
        
            
            
            
        }
        
    };

}

char MyPass::ID = 0;
static RegisterPass<MyPass> X("MyPass", "MyPass Pass");

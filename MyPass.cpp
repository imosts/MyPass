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
                                for (BasicBlock::iterator inst2 = inst; inst2 != bb->end(); ++inst2) {
                                    if (useNum <= 0) {
                                        break;
                                    }
                                    if (StoreInst *SI = dyn_cast<StoreInst>(inst2)) {
                                        if (SI->getValueOperand() == LI && SI->getValueOperand()->getType()->isPointerTy()) {
                                            LIUseNum++;
                                            useNum--;
                                            continue;
                                        }
                                    }else if (PtrToIntInst *PTI = dyn_cast<PtrToIntInst>(inst2)) {
                                        if (PTI->getOperand(0) == LI) {
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
                                                        if (isComeFormSouce(V, LI)) {
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
                                                    if (isComeFormSouce(V, LI)) {
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
//                            if (CI->getCalledFunction()->getName().equals("malloc")) {
////                                errs() << "malloc debug!" << '\n';
////                                std::vector<Value *> mallocArg;
////                                mallocArg.push_back(CI->getArgOperand(0));
////                                BasicBlock::iterator nextInst = inst;
////                                nextInst++;
////                                if (BitCastInst *BCI = dyn_cast<BitCastInst>(nextInst)) {
////                                    if (BCI->getOperandUse(0) == CI) {
////                                        nextInst++;
////                                        if (StoreInst *SI = dyn_cast<StoreInst>(nextInst)) {
////                                            if (SI->getOperand(0) == BCI) {
////                                                inst++;
////                                                inst++;
////                                                inst++;
////                                                BitCastInst *nBCI = new BitCastInst(SI->getPointerOperand(), PointerType::getUnqual(PointerType::getUnqual(Type::getInt8Ty(tmpF->getContext()))), "mBCI", &(*inst));
////                                                mallocArg.push_back(nBCI);
////                                                ArrayRef<Value *> funcArg(mallocArg);
////                                                Value *func = tmpF->getParent()->getFunction("safeMalloc");
////                                                CallInst *nCI = CallInst::Create(func, funcArg, "", &(*inst));
////
////                                                if (BCI->getNumUses() > 1) {
////                                                    LoadInst *LI = new LoadInst(SI->getPointerOperand(), "mLI", &(*inst));
////                                                    BCI->mutateType(LI->getType());
////                                                    BCI->replaceAllUsesWith(LI);
////                                                }
////                                                errs() << "malloc debug11!" << '\n';
////                                                SI->eraseFromParent();
////                                                BCI->eraseFromParent();
////                                                CI->eraseFromParent();
////                                                errs() << "malloc debug12!" << '\n';
////                                            }
////                                        }
////
////                                        inst--;
////                                        inst->dump();
////                                        errs() << "malloc debug2!" << '\n';
////                                    }
////                                }
//                                BasicBlock::iterator tmp = inst;
//                                bool isBBhead = true;
//                                errs() << "malloc debug!" << '\n';
//                                if (tmp != bb->begin()) {
//                                    tmp--;
//                                    isBBhead = false;
//                                }
//                                tmp->dump();
//                                for (BasicBlock::iterator inst2 = inst; inst2 != bb->end(); ++inst2) {
//                                    if (StoreInst *SI = dyn_cast<StoreInst>(inst2)) {
//                                        if (isSIComeFromMalloc(SI, CI)) {
//                                            inst2++;
//                                            changTOSafeMalloc(tmpF, CI, SI, inst2);
//
//                                            if (!isBBhead) {
//                                                inst = tmp;
//                                                inst++;
//                                            }else{
//                                                inst = bb->begin();
//                                                inst++;
//                                            }
//                                            inst->dump();
//                                            break;
////                                            inst = inst2;
////                                            inst->dump();
////                                            continue;
//                                        }
//                                    }
//                                }
//                                bb->dump();
//
//                            }else
                            if (CI && CI->getCalledFunction()->getName().equals("free")) {
                                errs() << "free debug!" << '\n';
                                CI->setCalledFunction(tmpF->getParent()->getFunction("safeFree"));
                                BasicBlock::iterator preInst = inst;
                                preInst--;
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
                        }
                    }
                    
                    
                    if (StoreInst *SI = dyn_cast<StoreInst>(inst)) {
                        if (isComeFromGEPAndChange(SI->getValueOperand())) {
                            errs() << "SI Value Come from GEP and Ptr Change!\n";
                            SI->dump();
                        }
                        
                        if (isSIValueComeFromMalloc(SI)) {
                            CallInst *CI = isSIValueComeFromMalloc(SI);
                            BasicBlock::iterator tmp = inst;
                            bool isBBhead = true;
                            errs() << "malloc debug!" << '\n';
//                            if (tmp != bb->begin()) {
//                                tmp--;
//                                isBBhead = false;
//                            }
                            tmp++;
                            tmp->dump();
                            changTOSafeMalloc(tmpF, CI, SI, tmp);
                            inst = tmp;
                            inst--;
//                            for (BasicBlock::iterator inst2 = inst; inst2 != bb->end(); ++inst2) {
//                                if (StoreInst *SI = dyn_cast<StoreInst>(inst2)) {
//                                    if (isSIComeFromMalloc(SI, CI)) {
//                                        inst2++;
//                                        changTOSafeMalloc(tmpF, CI, SI, inst2);
//
//                                        if (!isBBhead) {
//                                            inst = tmp;
//                                            inst++;
//                                        }else{
//                                            inst = bb->begin();
//                                            inst++;
//                                        }
//                                        inst->dump();
//                                        break;
//                                    }
//                                }
//                            }
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
                            errs() <<  "tracePoint debug1:" << '\n';
                            inst->dump();
                            SI->eraseFromParent();
                            inst--;
                            errs() <<  "tracePoint debug2:" << '\n';
                            inst->dump();
                        }else if (pointerLevel(SI->getPointerOperand()->getType()) == 1) {
                            PtrToIntInst *PTI = new PtrToIntInst(SI->getPointerOperand(), Type::getInt64Ty(bb->getContext()), "", &(*inst));
                            BinaryOperator *BOAnd = BinaryOperator::Create(Instruction::BinaryOps::And, PTI, ConstantInt::get(Type::getInt64Ty(bb->getContext()), AND_PTR_VALUE, false), "", &(*inst));
                            IntToPtrInst *ITPAnd = new IntToPtrInst(BOAnd, SI->getPointerOperand()->getType(), "", &(*inst));
                            SI->setOperand(1, ITPAnd);
                        }
                        
                        
                    }

                    inst->dump();
                    errs() << "  Inst After" <<'\n' <<'\n';
                }
            }
            return true;

            
    };
        
        
        
//        Value * insertLoadCheckInBasicBlock(Function* F, Function::iterator &originBB, BasicBlock::iterator &insetPoint, Value *address){
//            PtrToIntInst *PTI = new PtrToIntInst(address, Type::getInt64Ty(originBB->getContext()), "", &(*insetPoint));
//            BinaryOperator *BO = BinaryOperator::Create(Instruction::BinaryOps::And, PTI, ConstantInt::get(Type::getInt64Ty(originBB->getContext()), MULTIPTR_OR_NODEPTR, false), "", &(*insetPoint));
//            ICmpInst *ICM = new ICmpInst(&(*insetPoint), llvm::CmpInst::ICMP_NE, BO, ConstantInt::get(Type::getInt64Ty(originBB->getContext()), 0, false));
//
//            BasicBlock *newBB = llvm::SplitBlock(&(*originBB), &(*insetPoint), nullptr, nullptr);
//            BasicBlock *oldBB = &(*originBB);
//            BasicBlock::iterator inst = originBB->begin();
//            newBB->setName("newBasicBlock");
//            originBB->setName("oldBasicBlock");
//
//            BasicBlock *nBMulti = BasicBlock::Create(originBB->getContext(), "multiBB", F, newBB);
//            BranchInst *nBIMulti = BranchInst::Create(newBB, nBMulti);
//
//            BasicBlock *nBNode = BasicBlock::Create(originBB->getContext(), "nodeBB", F, newBB);
//            BranchInst *nBINode = BranchInst::Create(newBB, nBNode);
//
//            BasicBlock *nBElse = BasicBlock::Create(originBB->getContext(), "elseIfBB", F, nBMulti);
//            BranchInst *nBIElse = BranchInst::Create(newBB, nBElse);
//
//            BranchInst *oldBR = BranchInst::Create(nBElse, newBB, ICM, &(*originBB));
//            inst = originBB->end();
//            inst--;
//            inst--;
//            inst->eraseFromParent();
//
//            originBB++;
//            inst = originBB->begin();
//            PtrToIntInst *PTIelse = new PtrToIntInst(address, Type::getInt64Ty(originBB->getContext()), "", &(*inst));
//            BinaryOperator *BOelse = BinaryOperator::Create(Instruction::BinaryOps::And, PTIelse, ConstantInt::get(Type::getInt64Ty(originBB->getContext()), MULTIPTR, false), "", &(*inst));
//            ICmpInst *ICMelse = new ICmpInst(&(*inst), llvm::CmpInst::ICMP_NE, BOelse, ConstantInt::get(Type::getInt64Ty(originBB->getContext()), 0, false));
//            BranchInst *oldBRelse = BranchInst::Create(nBMulti, nBNode, ICMelse, &(*originBB));
//            inst->eraseFromParent();
//
//            originBB++;
//            inst = originBB->begin();
//            PtrToIntInst *PTImulti = new PtrToIntInst(address, Type::getInt64Ty(originBB->getContext()), "", &(*inst));
//            BinaryOperator *BOmulte = BinaryOperator::Create(Instruction::BinaryOps::And, PTImulti, ConstantInt::get(Type::getInt64Ty(originBB->getContext()), AND_PTR_VALUE, false), "", &(*inst));
//            IntToPtrInst *ITPmulti = new IntToPtrInst(BOmulte, PointerType::getUnqual(address->getType()), "", &(*inst));
//            LoadInst *multiLI = new LoadInst(ITPmulti, "", &(*inst));
//
//
//            originBB++;
//            inst = originBB->begin();
//            PtrToIntInst *PTInode = new PtrToIntInst(address, Type::getInt64Ty(originBB->getContext()), "", &(*inst));
//            BinaryOperator *BOnode = BinaryOperator::Create(Instruction::BinaryOps::And, PTInode, ConstantInt::get(Type::getInt64Ty(originBB->getContext()), AND_PTR_VALUE, false), "", &(*inst));
//            IntToPtrInst *ITPnode = new IntToPtrInst(BOnode, address->getType(), "", &(*inst));
//
//            originBB++;
//            inst = originBB->begin();
//            PHINode *PhiNode = PHINode::Create(address->getType(), 3, "", &(*inst));
//            PhiNode->addIncoming(address, oldBB);
//            PhiNode->addIncoming(multiLI, nBMulti);
//            PhiNode->addIncoming(ITPnode, nBNode);
//
//            return PhiNode;
//        }
        
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
//            inst = originBB->begin();
//            inst--;
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
        
        //插入FOR循环
        //F：为插入基本块的函数
        //originBB:插入原基本块
        //insetPoint:插入的基本点（指令之前）
        //loopNum:for循环的次数
        BasicBlock * insertForLoopInBasicBlock(Function* F, BasicBlock *originBB, Instruction *insetPoint, int loopNum){
            BasicBlock *newBB = llvm::SplitBlock(originBB, insetPoint, nullptr, nullptr);
            newBB->setName("newBasicBlock");
            originBB->setName("oldBasicBlock");
            AllocaInst *AI;
            for (BasicBlock::iterator in = originBB->begin(); in != originBB->end(); ++in) {
                if (in->getOpcode() == Instruction::Br) {
                    AI = new AllocaInst(Type::getInt32Ty(originBB->getContext()), "i", &(*in));
                    StoreInst *SI = new StoreInst(ConstantInt::get(Type::getInt32Ty(originBB->getContext()), 0, true), AI, "ini", &(*in));
                    
                    
                }
            }
            
            BasicBlock *nBinc = BasicBlock::Create(originBB->getContext(), "for.inc", F, newBB);
            BranchInst *nBIinc = BranchInst::Create(newBB, nBinc);
            
            BasicBlock *nBbody = BasicBlock::Create(originBB->getContext(), "for.body", F, nBinc);
            BranchInst *nBIbody = BranchInst::Create(nBinc, nBbody);
            
            BasicBlock *nBcon = BasicBlock::Create(originBB->getContext(), "for.con", F, nBbody);
            BranchInst *nBIcon = BranchInst::Create(nBbody, nBcon);
            
            BasicBlock::iterator brIn = originBB->end();
            --brIn;
            if (BranchInst *oBI = dyn_cast<BranchInst>(brIn)) {
                oBI->llvm::User::setOperand(0, nBcon);
            }
            
            BasicBlock::iterator nBconIter = nBcon->end();
            --nBconIter;
            if (BranchInst *oBI = dyn_cast<BranchInst>(nBconIter)) {
                LoadInst *LI = new LoadInst(AI, "nli", &(*nBconIter));
                ICmpInst *ICM = new ICmpInst(&(*nBconIter), llvm::CmpInst::ICMP_SLT, LI, ConstantInt::get(Type::getInt32Ty(originBB->getContext()), loopNum, true));
                BranchInst *nnBIcon = BranchInst::Create(nBbody, newBB, ICM, nBcon);
                nBconIter->eraseFromParent();
            }
            
            BasicBlock::iterator nBIncIter = nBinc->end();
            --nBIncIter;
            if (BranchInst *oBI = dyn_cast<BranchInst>(nBIncIter)) {
                LoadInst *LI = new LoadInst(AI, "nli", &(*nBIncIter));
                BinaryOperator *BO = BinaryOperator::Create(llvm::Instruction::BinaryOps::Add, LI, ConstantInt::get(Type::getInt32Ty(originBB->getContext()), 1, true), "add", &(*nBIncIter));
                StoreInst *SI = new StoreInst(BO, AI, &(*nBIncIter));
                oBI->setOperand(0, nBcon);
            }
            return nBbody;
        }
        
        
        
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
        
        //获取当前结构体类型ST的DP结构体类型，若不存在则返回NULL
        StructType* getChangedStructType(Module *M, StructType *ST){
            std::string StrName = ST->getStructName().str() + ".DoublePointer";
            for(auto *S : M->getIdentifiedStructTypes()){
                if (StrName == S->getStructName().str()) {
                    return S;
                }
            }
            return NULL;
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
        
        //获取GEP源操作数和参数对应的正确类型
        Type* getRightGEPSouType(GetElementPtrInst *newGEP){
            Value *Val = newGEP->getOperand(0);
            Type *SouContainType = Val->getType();
            
            for (unsigned i = 1; i < newGEP->getNumOperands(); i++) {
                if (SouContainType->isStructTy()) {
                    if (ConstantInt *a = dyn_cast<ConstantInt>(newGEP->getOperand(i))) {
                        SouContainType = SouContainType->getStructElementType(a->getZExtValue());
                    }else{
                        SouContainType = SouContainType->getContainedType(0);
                    }
                }else{
                    SouContainType = SouContainType->getContainedType(0);
                }
                
            }
            return SouContainType;
        }
        
        //初始化全局变量，如果已拓展的二级指针，初始化一级指针
        //若为
        bool iniGlobalVarDP(Function::iterator &B, BasicBlock::iterator &I){
            Module *M = B->getParent()->getParent();
            //分配
            AllocaInst *AI = new AllocaInst(PointerType::getUnqual(Type::getInt32Ty(B->getContext())), "GloBasePtr", &(*I));
            StoreInst *AISI = new StoreInst::StoreInst(ConstantPointerNull::get((PointerType *)(AI->getAllocatedType())), AI, &(*I));
            
            for (Module::global_iterator gi = M->global_begin(); gi != M->global_end(); ++gi){
                std::string name = gi->getName().str();
                auto S = std::find(varNameList.begin(), varNameList.end(), name);
                if (S != varNameList.end()) {
                    gi->getValueType()->dump();
                    iniTypeRel(B, gi->getValueType(), I, &(*gi), AI);
                }
            }
            return false;
        }
        
        void iniTypeRel(Function::iterator &B, Type *T, BasicBlock::iterator &I, Value *souValue, Value *nullValue){
            BasicBlock::iterator tmpin = I;
            if (dyn_cast<AllocaInst>(I)) {
                tmpin++;
            }
            
            if (T->isStructTy()) {
                if (StructType *ST = dyn_cast<StructType>(T)) {
                    std::string name = ST->getStructName().str();
                    auto S = name.find(".DoublePointer");
                    if (S != name.npos) {
                        for (unsigned i = 0; i < ST->getNumElements(); ++i) {
                            errs() << "structIndexList.push_back: " << i <<'\n';
                            structIndexList.push_back(ConstantInt::get(Type::getInt32Ty(B->getContext()), i, false));
                            iniTypeRel(B, T->getStructElementType(i), I, souValue, nullValue);
                            structIndexList.pop_back();
                            errs() << "structIndexList.pop_back: " << i <<'\n';
                        }
                    }
                }
            }else if (pointerLevel(T) == 2 && !(getSouType(T, 2)->isFunctionTy())){
                errs() << "iniTypeRel pointerLevel Debug1: " <<'\n';
                souValue->dump();
                T->dump();
                llvm::ArrayRef<llvm::Value *> GETidexList(structIndexList);
                for (ArrayRef<llvm::Value *>::iterator S = GETidexList.begin(); S != GETidexList.end(); ++S) {
                    errs() << **S << '\n';
                }
//                GetElementPtrInst *GEP = GetElementPtrInst::CreateInBounds(souValue, GETidexList, "iniGVGEP", &(*tmpin));
//                BitCastInst *BCI = new BitCastInst(nullValue, GEP->getType()->getContainedType(0), "iniGVBCI", &(*tmpin));
//                StoreInst *SI = new StoreInst(BCI, GEP, &(*tmpin));
//
//                AllocaInst *baseAI = new AllocaInst(T->getContainedType(0), "iniBasePtr", &(*I));


                if (GNPtr) {
                    BitCastInst *BCI = new BitCastInst(GNPtr, T, "iniBPBCI", &(*tmpin));
                    GetElementPtrInst *GEP = GetElementPtrInst::CreateInBounds(souValue, GETidexList, "iniGVGEP", &(*tmpin));
                    StoreInst *SI = new StoreInst(BCI, GEP, &(*tmpin));
                }
//                StoreInst *baseSI = new StoreInst(BCI, baseAI, &(*tmpin));
               
            }else if (ArrayType *AT = dyn_cast<ArrayType>(T)){
                if ((pointerLevel(T->getContainedType(0)) == 2) || (T->getContainedType(0)->isStructTy() && iniTypeRelIsChange(B, T->getContainedType(0)))) {
                    errs() << "D1" <<'\n';
                    I->dump();
                    BasicBlock *bb = I->getParent();
                    errs() << "D2" <<'\n';
                    BasicBlock *newBB = insertForLoopInBasicBlock(B->getParent(), &(*bb), &(*tmpin), AT->getNumElements());
                    errs() << "D3" <<'\n';
                    need_bb_iter_begin = true;
                    BasicBlock::iterator ii = bb->end();
                    //找到for循环的i的值
                    ii--;
                    ii--;
                    ii--;
                    
                    ii->dump();
                    if (Value *test = dyn_cast<Value>(ii)) {
                        BasicBlock::iterator bdi = newBB->end();
                        bdi--;
                        LoadInst *insertLoad = new LoadInst(test, "iniGVarr", &(*bdi));
                        SExtInst *SEI = new SExtInst(insertLoad, Type::getInt64Ty(bb->getContext()), "iniGVsei", &(*bdi));
                        
                        errs() << "structIndexList.push_back: ";
                        SEI->dump();
                        structIndexList.push_back(SEI);
                        llvm::ArrayRef<llvm::Value *> GETidexList(structIndexList);
                        errs() << "iniTypeRel Debug1: "<<'\n';
                        for (ArrayRef<llvm::Value *>::iterator S = GETidexList.begin(); S != GETidexList.end(); ++S) {
                            errs() << **S << '\n';
                        }
                        
                        errs() << "iniTypeRel Debug2: "<<'\n';
                        if (pointerLevel(T->getContainedType(0)) == 2) {
                            GetElementPtrInst *GEP = GetElementPtrInst::CreateInBounds(souValue, GETidexList, "iniGVign", &(*bdi));
                            BitCastInst *BCI = new BitCastInst(nullValue, GEP->getType()->getContainedType(0), "iniGVBCI", &(*bdi));
                            StoreInst *instStore = new StoreInst::StoreInst(BCI, GEP, &(*bdi));
                        }else if (T->getContainedType(0)->isStructTy()){
                            if (StructType *ST = dyn_cast<StructType>(T->getContainedType(0))) {
                                //TODO: 做优化，先判断结构体内是否有需要建立关系的地方，如不需要，则不必插入FOR循环
                                iniTypeRel(B, ST, bdi, souValue, nullValue);
                            }
                        }
                        errs() << "iniTypeRel Debug3: "<<'\n';
                        structIndexList.pop_back();
                        errs() << "structIndexList.pop_back: ";
                        SEI->dump();
                    }
                    
                    //使现在的基本块跳转到新的基本块
                    for (int i = 0; i < 4; ++i) {
                        B++;
                    }
                    //使迭代的指令到达新的基本块的第一条指令，即为逻辑上未插入for循环的下一条指令
                    I = B->begin();
                    
                }
            }
        }
        
        bool iniTypeRelIsChange(Function::iterator &B, Type *T){
            bool isChange = false;
            
            if (T->isStructTy()) {
                if (StructType *ST = dyn_cast<StructType>(T)) {
                    std::string name = ST->getStructName().str();
                    auto S = name.find(".DoublePointer");
                    if (S != name.npos) {
                        for (unsigned i = 0; i < ST->getNumElements(); ++i) {
                            structIndexList.push_back(ConstantInt::get(Type::getInt32Ty(B->getContext()), i, false));
                            isChange = iniTypeRelIsChange(B, T->getStructElementType(i));
                            structIndexList.pop_back();
                        }
                    }
                }
            }else if (pointerLevel(T) == 2 && !(getSouType(T, 2)->isFunctionTy())){
                isChange = true;
            }else if (ArrayType *AT = dyn_cast<ArrayType>(T)){
                if ((pointerLevel(T->getContainedType(0)) == 2)){
                    isChange = true;
                }else if (T->getContainedType(0)->isStructTy()){
                    StructType *ST = dyn_cast<StructType>(T->getContainedType(0));
                    isChange = iniTypeRelIsChange(B, ST);
                }
            }
            return isChange;
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
        
        void setAllocaStructType(Function *tmpF, Instruction *inst){
            if (AllocaInst *AI = dyn_cast<AllocaInst>(inst)) {
                Type *AllocT = AI->getAllocatedType();
                Type *newTy = AllocT;
                if (StructType *ST = dyn_cast<StructType>(getSouType(AllocT, pointerLevel(AllocT)))) {
                    std::string name = ST->getStructName().str();
                    
                    if (name.find("struct.") != name.npos) {
                        name = name.substr(7, name.length() - 7);
                    }else if (name.find("union.") != name.npos){
                        name = name.substr(6, name.length() - 6);
                    }
                    
                    auto S = std::find(structNameList.begin(), structNameList.end(), name);
                    if (S != structNameList.end() || (name.find("anon") != name.npos) || (name.find("anon") != name.npos)) {
                        newTy = changeStructTypeToDP(*(tmpF->getParent()), getSouType(AllocT, pointerLevel(AllocT)));
                        if (AllocT->isPointerTy()) {
                            newTy = getPtrType(newTy, pointerLevel(AllocT) + 1);
                        }
                    }else if(AllocT->isPointerTy()) {
                        newTy = PointerType::getUnqual(AllocT);
                    }
                }else if (pointerLevel(AllocT) > 0 && !(getSouType(AllocT, pointerLevel(AllocT))->isFunctionTy())){
                    newTy = PointerType::getUnqual(AllocT);
                }else if (ArrayType *AT = dyn_cast<ArrayType>(AllocT)){
                    getSouType(AT->getContainedType(0), pointerLevel(AT->getContainedType(0)))->dump();
                    if (StructType *ST = dyn_cast<StructType>(getSouType(AT->getContainedType(0), pointerLevel(AT->getContainedType(0))))) {
                        std::string name = ST->getStructName().str();
                        
                        if (name.find("struct.") != name.npos) {
                            name = name.substr(7, name.length() - 7);
                        }else if (name.find("union.") != name.npos){
                            name = name.substr(6, name.length() - 6);
                        }
                        
                        auto S = std::find(structNameList.begin(), structNameList.end(), name);
                        if (S != structNameList.end() || (name.find("anon") != name.npos) || (name.find("anon") != name.npos)) {
                            newTy = changeStructTypeToDP(*(tmpF->getParent()), getSouType(AT->getContainedType(0), pointerLevel(AT->getContainedType(0))));
                            errs() << "setAllocaStructType debug1:" << '\n';
//                            newTy = getPtrType(newTy, pointerLevel(AT->getContainedType(0)));
                            
                            if (AT->getContainedType(0)->isPointerTy()) {
                                newTy = getPtrType(newTy, pointerLevel(AT->getContainedType(0)) + 1);
                            }
                            newTy->dump();
                        }else if(AllocT->isPointerTy()) {
                            newTy = PointerType::getUnqual(AllocT);
                        }
                        newTy = ArrayType::get(newTy, AT->getNumElements());
                    }else if(pointerLevel(AT) > 0 && !(getSouType(AT, pointerLevel(AT))->isFunctionTy())){
                        newTy = PointerType::getUnqual(AllocT);
                        newTy = ArrayType::get(newTy, AT->getNumElements());
                    }
                }
                
                AI->setAllocatedType(newTy);
                AI->setName("n" + AI->getName());
                ((Value *)AI)->mutateType((llvm::PointerType::getUnqual(newTy)));
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
            }
            return false;
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
//                nextInst++;
//                nextInst++;
//                nextInst++;
            }else {
                SI->eraseFromParent();
                CI->eraseFromParent();
//                nextInst++;
//                nextInst++;
            }
            
            
//            errs() << "changTOSafeMalloc debug15!" << '\n';
//
//            errs() << "changTOSafeMalloc debug!" << '\n';
//            M->dump();
//            CI->dump();
//            if (StoreInst *SI = dyn_cast<StoreInst>(M)) {
//                errs() << "changTOSafeMalloc debug11!" << '\n';
//                if (CallInst *CI = dyn_cast<CallInst>(M)) {
//                    errs() << "changTOSafeMalloc debug12!" << '\n';
//
//
//                }
//            }else
            
            
            
        }
        
    };

}

char MyPass::ID = 0;
static RegisterPass<MyPass> X("MyPass", "MyPass Pass");

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
#include "llvm/Transforms/Utils/BasicBlockUtils.h"
#include <set>
#include <fstream>

using namespace llvm;

#define DEBUG_TYPE "MyPass"
#define INI_TYPE_MEMSET 1
#define INI_TYPE_MEMCPY 2


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
            Function *tmpF = &F;
            unsigned FAnum = 0;
            
            std::vector<std::string> localFunName;
            std::ifstream open_file("localFunName.txt"); // 读取
            while (open_file) {
                std::string line;
                std::getline(open_file, line);
                auto index=std::find(localFunName.begin(), localFunName.end(), line);
                if(!line.empty() && index == localFunName.end()){
                    localFunName.push_back(line);
                }
            }
            
            // 遍历函数中的所有基本块
            for (Function::iterator bb = tmpF->begin(); bb != tmpF->end(); ++bb) {
                // 遍历基本块中的每条指令
                
                //统计函数的传递的参数，并在后面的分配指令（Alloca）忽略相应数目的语句
                FAnum = tmpF->getFunctionType()->getNumParams();
                if (tmpF->getName().contains("main")) {
                    FAnum++;
                }
                
                for (BasicBlock::iterator inst = bb->begin(); inst != bb->end(); ++inst) {
                    //遍历指令的操作数
                    
                    inst->dump();
                    
                    
                    /*对于alloca指令，若创建类型为指针，指令名不变，拓展一级
                     *对于一级指针，拓展之后，新建一个一级指针使拓展的指针指向改一级指针
                     *TODO 新建的指针使用放置在堆上
                     *TODO 是否可能是指针向量？？？
                     *TODO 对于函数参数的alloca 不能处理为多级指针
                     */
                    if (inst->getOpcode() == Instruction::Alloca && FAnum == 0) {//只有参数计数为0时，才能继续执行拓展，否则FAnum--
                        errs() << "XXXX" << FAnum << '\n';
                        if (AllocaInst * AI = dyn_cast<AllocaInst>(inst)) {
                            if (AI->getAllocatedType()->isPointerTy() && !(AI->getAllocatedType()->getContainedType(0)->isFunctionTy())) {
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
                                //此处有BUG，详情 10.18-19工作进度 TODO3
                                if (1) {
                                    ArrayType *AT = dyn_cast<ArrayType>(AI->getAllocatedType());
                                    int eleNum = AT->getNumElements();
                                    Type *eleType = AT->getElementType();
                                    Type *mulPtrTy = llvm::PointerType::getUnqual(eleType);
                                    ArrayType *newArrTy = ArrayType::get(mulPtrTy, eleNum);
                                    //变为高一级的指针数组
                                    AI->setAllocatedType(newArrTy);
                                    AI->mutateType(llvm::PointerType::getUnqual(newArrTy));
                                    AI->setName("na" + AI->getName());
                                    //                                        //在改变变量的集合中添加该变量名
                                    //                                        ValueName.insert(AI->getName());

                                    
                                    if (!(eleType->isPointerTy()) || (!(eleType->getContainedType(0)->isPointerTy()))) {
                                        //若为一级以上的指针数组
                                        //则变为二级指针后还需，创建同样数量的一级指针，并让二级指针指向一级指针
                                        Type * destType;
                                        
                                        errs() << "TEST2!" << '\n';
                                        
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
                                                        //                                                            errs() << "XXXXXXXXXXXXXXXXXXXXX" << '\n';
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
                            
                            //通过在分配结构体对象时，获取结构体类型信息，创建一个拓展指针的结构体类型（StructType）；然后通过分配一个该新结构体的对象（AllocaInst ），将该结构体声明加入module，再改变原分配指令的分配类型为新结构体，最后删除新结构体分配语句
                            //TODO:目前没考虑引用第三方库结构体，之后考虑对于第三方的结构体过滤？（大体思路 与函数调用一样 遍历完以后 通过名称排除第三方库）
                            if (AI->getAllocatedType()->isStructTy()) {
//                                errs() << "@@@@@@@@@@@@@@@@@@@" << '\n';
                                AI->getAllocatedType()->dump();
                                if (StructType * ST = dyn_cast<StructType>(AI->getAllocatedType())) {
                                    std::vector<Type *> typeList;
                                    for (unsigned i = 0; i < ST->getNumElements(); i++) {
                                        if (ST->getElementType(i)->isPointerTy()) {
                                            typeList.push_back(llvm::PointerType::getUnqual(ST->getElementType(i)));
                                        }else{
                                            typeList.push_back(ST->getElementType(i));
                                        }
                                    }
                                    
                                    llvm::ArrayRef<llvm::Type *> StructTypelist(typeList);
                                    std::string name = ST->getName().str() + ".DoublePointer";
                                    StructType * newST = StructType::create(bb->getContext(), StructTypelist, name);
                                    //新建一个结构体类型，并分配一个该类型（使得该类型在结构声明中），再修改原分配类型
                                    AllocaInst * temp = new AllocaInst(newST, "test", &(*inst));
                                    AI->setAllocatedType(newST);
                                    AI->mutateType(llvm::PointerType::getUnqual(newST));
                                    //删除为声明儿分配的无用新结构分配语句
                                    temp->eraseFromParent();
                                    
                                }
                            }
                            
                            
                            //                                AI->getType()->getContainedType(0)->dump();
                            //                                errs() << "isStructType:" << AI->getType()->getContainedType(0)->isStructTy() << '\n';
                            //                                errs() << "isVectorTy: " << AI->getType()->getContainedType(0)->isVectorTy() << '\n';
                            //                                errs() << "isPointerTy: " << AI->getType()->getContainedType(0)->isPointerTy() << '\n';
                            //                                errs() << "isSized: " << AI->getType()->getContainedType(0)->isSized() << '\n';
                            //                                errs() << "isHalfTy: " << AI->getType()->getContainedType(0)->isHalfTy() << '\n';
                            //                                errs() << "isVoidTy: " << AI->getType()->getContainedType(0)->isVoidTy() << '\n';
                            //                                errs() << "isArrayTy: " << AI->getType()->getContainedType(0)->isArrayTy() << '\n';
                            //                                errs() << "isEmptyTy: " << AI->getType()->getContainedType(0)->isEmptyTy() << '\n';
                            //                                errs() << "isTokenTy: " << AI->getType()->getContainedType(0)->isTokenTy() << '\n';
                            //                                errs() << "isMetadataTy: " << AI->getType()->getContainedType(0)->isMetadataTy() << '\n';
                        }
                        
                        
                    }else if(inst->getOpcode() == Instruction::Alloca){
                        //去掉函数传递的参数分配
                        //确保FAnum不会溢出
                        if (FAnum > 0) {
                            FAnum--;
                        }
                        
                        //对于函数参数分配的变量，将其拓展为多级指针，在通过bitcast指令，将所有形参（指针对指针类型）转换为多级指针
                        if (tmpF->getName().str() != "main") {
                            errs() << tmpF->getName() << '\n';
                            AllocaInst *argAlloca = dyn_cast<AllocaInst>(inst);
                            if (argAlloca->getAllocatedType()->isPointerTy() && !(argAlloca->getAllocatedType()->getContainedType(0)->isFunctionTy())) {
                                errs() << "XXXXXXXXXXXXX" << '\n';
                                argAlloca->setAllocatedType(llvm::PointerType::getUnqual(argAlloca->getAllocatedType()));
                                argAlloca->mutateType(llvm::PointerType::getUnqual(argAlloca->getAllocatedType()));
                                argAlloca->dump();
                                argAlloca->getType()->dump();
                                for (BasicBlock::iterator tmpInst = inst; tmpInst != bb->end(); ++tmpInst) {
                                    if (StoreInst *SI = dyn_cast<StoreInst>(tmpInst)) {
                                        if (SI->getPointerOperand() == argAlloca) {
                                            errs() << "Store Inst!" << '\n';
                                            BitCastInst *BCI = new BitCastInst(SI->getValueOperand(), SI->getPointerOperand()->getType()->getContainedType(0), "nBCI", &(*tmpInst));
                                            SI->setOperand(0, BCI);
                                            break;
                                        }
                                    }
                                }
                                
                            }
                        }
                    }
                    
                    //对于union（非全局union），若碰到bitcast指令，则转换为拓展指针
                    //TODO: 对于全局的第三方库的union处理的问题
                    if (inst->getOpcode() == Instruction::BitCast) {
//                        errs() << "@@@@@@######$$$" << '\n';
                        if (BitCastInst *BCI = dyn_cast<BitCastInst>(inst)) {
                            if (StructType *ST = dyn_cast<StructType>(BCI->getSrcTy()->getContainedType(0))) {
                                if (ST->getStructName().contains("union.") && BCI->getDestTy()->getContainedType(0)->isPointerTy()) {
                                    inst->mutateType(llvm::PointerType::getUnqual(ST->getElementType(0)));
                                    inst->dump();
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
                                //                                    errs() << "InsertLoad:" << '\n';
                                //                                    Val->getType()->dump();
                                //                                    Val->getType()->getContainedType(0)->dump();∫
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
                        
                        //此处单独处理结构体有关的GEP，若GEP返回的指针非二级，则非指针，只是把源类型修改一下，后面不做操作
                        //否则，继续修改其他参数，使得返回值为二级指针
                        //TODO:目前没考虑引用第三方库结构体，之后考虑对于第三方的结构体过滤？（大体思路 与函数调用一样 遍历完以后 通过名称排除第三方库）
                        if(Val->getType()->getContainedType(0)->isStructTy()){
                            newGEP->setSourceElementType(Val->getType()->getContainedType(0));
                            if (newGEP->getType()->getContainedType(0)->isPointerTy()) {
                                newGEP->mutateType(llvm::PointerType::getUnqual(newGEP->getType()));
                                newGEP->setName("n" + newGEP->getName());
                                newGEP->setResultElementType(newGEP->getType()->getContainedType(0));
                            }
                        }
                        
                        //GEP若源、目的类型不匹配
                        if (newGEP->getResultElementType() != SouContainType) {
                            
                            if (Val->getType()->getContainedType(0)->isArrayTy()) {
                                
                                newGEP->mutateType(llvm::PointerType::getUnqual(SouContainType));
                                newGEP->setName("n" + newGEP->getName());
                                newGEP->setSourceElementType(Val->getType()->getContainedType(0));
                                newGEP->setResultElementType(SouContainType);
                            }else if(!(Val->getType()->getContainedType(0)->isStructTy())){//此处要去掉结构体的情况，因为结构体在之前已经处理完了
                                //TODO:还需考虑有无问题
                                Value *Val = newGEP->getOperand(0);
                                errs() << "XXXXXXX!Val->getType()->getContainedType(0)->isStructTy()XXXXXXXX" << '\n';
                                newGEP->dump();
                                Val->getType()->dump();
                                Val->getType()->getContainedType(0)->dump();
                                Val->getType()->getContainedType(0)->getContainedType(0)->dump();
                                
                                if (Val->getType()->isPointerTy() && Val->getType()->getContainedType(0)->isPointerTy() && Val->getType()->getContainedType(0)->getContainedType(0)->isStructTy()) {
                                    errs() << "XXXXXXXThis Bug Area!XXXXXXXX" << '\n';
                                    newGEP->getType()->dump();
                                    newGEP->getSourceElementType()->dump();
                                    errs() << "newGEP->getType()->isPointerTy()" << newGEP->getType()->isPointerTy() << '\n';
                                    errs() << "newGEP->getType() == newGEP->getSourceElementType()" << newGEP->getType() << '\n';
                                    errs() << "newGEP->getType() == newGEP->getSourceElementType()" << newGEP->getNumOperands() << '\n';
                                    if (newGEP->getNumOperands() == 2) {
                                        errs() << "SetType" << '\n';
                                        newGEP->mutateType(llvm::PointerType::getUnqual(SouContainType));
                                        newGEP->setName("n" + newGEP->getName());
                                        newGEP->setSourceElementType(Val->getType()->getContainedType(0));
                                        newGEP->setResultElementType(SouContainType);
                                    }else{
                                        errs() << "NewLoad" << '\n';
                                        LoadInst *insertLoad = new LoadInst(Val, "gl", &(*inst));
                                        inst->setOperand(0, insertLoad);
                                    }
                                    Val->getType()->getContainedType(0)->dump();
                                    SouContainType->dump();
                                }else if(Val->getType()->isPointerTy() && Val->getType()->getContainedType(0)->isPointerTy() && Val->getType()->getContainedType(0)->getContainedType(0)->isPointerTy()){
                                    newGEP->mutateType(llvm::PointerType::getUnqual(SouContainType));
                                    newGEP->setName("n" + newGEP->getName());
                                    newGEP->setSourceElementType(Val->getType()->getContainedType(0));
                                    newGEP->setResultElementType(SouContainType);
                                }else{
                                    LoadInst *insertLoad = new LoadInst(Val, "gl", &(*inst));
                                    inst->setOperand(0, insertLoad);
                                }
                                
                            }
                            
                        }
                        
                    }
                    
                    if (inst->getOpcode() == Instruction::Add) {
                        BasicBlock *loopBody = insertForLoopInBasicBlock(tmpF, &(*bb), &(*inst), 10);
                        BasicBlock::iterator i = bb->end();
                        i--;
                        i--;
                        i--;
                        errs() << "test i:";
                        i->dump();
                        
                        for (int i = 0; i < 4; ++i) {
                            bb++;
                        }
                    }
                    
                    //store指令源操作数、目标操作数类型不匹配，若为指针运算且为二级以上指针，则新建一个指针指向该地址
                    //TODO:超过二级的指针，理论上要建立一级指针 然后逐一确定各级的关系
                    if (inst->getOpcode() == Instruction::Store) {
                        if (StoreInst *SI = dyn_cast<StoreInst>(inst)) {
                            Value * SPtr = SI->getPointerOperand();
                            Value * SVal = SI->getValueOperand();
                            if ((llvm::PointerType::getUnqual(SVal->getType())) != SPtr->getType()) {
                                //                                    errs() << "!!!Store Ptr not match Val:" << '\n';
                                //                                    SI->dump();
                                //若第一个操作数由GEP指令的来，且为一级指针
                                if (dyn_cast<GetElementPtrInst>(inst->getOperand(0)) && !(inst->getOperand(0)->getType()->getContainedType(0)->isPointerTy())){
                                    //若源操作数与目标操作数只差一级指针
                                    if ((llvm::PointerType::getUnqual(llvm::PointerType::getUnqual(SVal->getType()))) == SPtr->getType()) {
                                        //新建一个指针，并用该指针连接源、目标指针
                                        AllocaInst *nAllInst = new AllocaInst(SVal->getType(), "base", &(*inst));
                                        StoreInst *nStoInst = new StoreInst(SVal, nAllInst, &(*inst));
                                        inst->setOperand(0, nAllInst);
                                    }
                                }else{
                                    if ((llvm::PointerType::getUnqual(llvm::PointerType::getUnqual(SVal->getType()))) == SPtr->getType()) {
                                        const Twine *name = new Twine::Twine("l");
                                        LoadInst * insert = new LoadInst(SPtr, *name, &(*inst));
                                        SI->setOperand(1, insert);
                                    }
                                }
                            }
                        }
                    }
                    
                    //对于Call指令
                    if (inst->getOpcode() == Instruction::Call) {
                        CallInst * test = dyn_cast<CallInst>(inst);
                        errs() << "AAAAAAAAAAAAAAAAAAA" << '\n';
                        Function *fTemp = test->getCalledFunction();
                        if (Function *fTemp = test->getCalledFunction()) {
                            fTemp->dump();
                            //                        errs() << "Call Function Name: " << fTemp->getName() << '\n';
                            errs() << "debug1" << '\n';
                            auto index = std::find(localFunName.begin(), localFunName.end(), fTemp->getName().str());
                            
                            if (index == localFunName.end()) {
                                for (Instruction::op_iterator oi = test->op_begin(); oi != test->op_end(); ++oi) {
                                    if (Value * op = dyn_cast<Value>(oi)) {
                                        if (op->getType()->isPointerTy() && op->getType()->getContainedType(0)->isPointerTy()) {
                                            LoadInst *callLoad = new LoadInst(op, "ncl", &(*inst));
                                            test->setOperand(oi->getOperandNo(), callLoad);
                                        }
                                    }
                                }
                            }else{
                                //TODO:
                                errs() << "In localFunName!" << '\n';
                                FunctionType *FT = test->getFunctionType();
                                
                                for (unsigned i = 0; i < test->getNumOperands() - 1; i++) {
                                    if (test->getOperand(i)->getType()->isPointerTy()) {
                                        errs() << "getContainedType";
                                        test->getOperand(i)->getType()->getContainedType(0)->dump();
                                        BitCastInst *BCI = new BitCastInst(test->getOperand(i), FT->getParamType(i), "nBCI", &(*inst));
                                        test->setOperand(i, BCI);
                                        
                                    }
                                }
                                
                                //                            fTemp->getFunctionType()->dump();
                                //
                            }
                        }else{
                            errs() << "test->getCalledValue: " << '\n';
                            if (test->getCalledValue()->getType()->isPointerTy() && test->getCalledValue()->getType()->getContainedType(0)->isPointerTy()) {
                                LoadInst *LI = new LoadInst(test->getCalledValue(), "test", &(*inst));
                                test->setCalledFunction(LI);
                            }
                            
                            for (unsigned i = 0; i < test->getNumOperands() - 1; i++) {
                                if (test->getOperand(i)->getType()->isPointerTy() && !(test->getOperand(i)->getType()->getContainedType(0)->isFunctionTy())) {
                                    errs() << "getContainedType: ";
//                                    test->getCalledValue()->getType()->getContainedType(0)->dump();
                                    if (FunctionType *FT = dyn_cast<FunctionType>(test->getCalledValue()->getType()->getContainedType(0))) {
                                        BitCastInst *BCI = new BitCastInst(test->getOperand(i), FT->getParamType(i), "nBCI", &(*inst));
                                        test->setOperand(i, BCI);

                                    }
//                                    test->getOperand(i)->getType()->getContainedType(0)->dump();
                                    
                                }
                            }
                        }
                        
                        
                        
                        test->dump();
//                        errs() << "XXXXXXXXXXXXXXXXXXXXXX" << '\n';
                    }
                    
                    //简单的使返回的指针类型对应，即取出多级指针的内容 返回
                    if (inst->getOpcode() == Instruction::Ret) {
                        if (inst->getNumOperands() > 0) {
                            if (inst->getOperand(0)->getType() != tmpF->getReturnType()) {
                                errs() << "XXXXX RET type is not match return type !XXXXX" << '\n';
                                if (inst->getOperand(0)->getType()->getContainedType(0) == tmpF->getReturnType()) {
                                    LoadInst *LI = new LoadInst(inst->getOperand(0), "nNI", &(*inst));
                                    inst->setOperand(0, LI);
                                }
                            }
                        }
                        
                    }
                    
                    //针对ptrtoint指令，若为有name且二级以上的指针（说明已经被拓展），通过load获取其内容
                    //TODO:考虑以后可能会有PtrToInt逻辑上需要直接将高级指针转换的情况
                    if (inst->getOpcode() == Instruction::PtrToInt) {
                        if (inst->getOperand(0)->hasName() && inst->getOperand(0)->getType()->getContainedType(0)->isPointerTy()) {
                            //                                errs() << "XXXXXXXXXXXXXXXXXXXXXX" << '\n';
                            Value *tempValue;
                            if (tempValue = dyn_cast<Value>(inst->getOperand(0))) {
                                LoadInst *newLoadInst = new LoadInst(tempValue, "nlPTI", &(*inst));
                                inst->setOperand(0, newLoadInst);
                            }
                        }
                    }
                    
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
                ICmpInst *ICM = new ICmpInst(&(*nBconIter), llvm::CmpInst::ICMP_SLT, LI, ConstantInt::get(Type::getInt32Ty(originBB->getContext()), loopNum - 1, true));
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
        
    };
    
    
    
    

}

char MyPass::ID = 0;
static RegisterPass<MyPass> X("MyPass", "MyPass Pass");

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
        bool need_bb_iter_begin = false;
        
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
                    
                    //判断是否插入了基本块，并且在新的基本块上，需要第一条语句开始迭代
                    if (need_bb_iter_begin) {
                        inst = bb->begin();
                        need_bb_iter_begin = false;
                    }
                    
                    errs() << "  Inst Begin:" << '\n';
                    inst->dump();
                    
                    
                    /*对于alloca指令，若创建类型为指针，指令名不变，拓展一级
                     *对于一级指针，拓展之后，新建一个一级指针使拓展的指针指向改一级指针
                     *TODO 新建的指针使用放置在堆上
                     *TODO 是否可能是指针向量？？？
                     *TODO 对于函数参数的alloca 不能处理为多级指针
                     */
                    if (inst->getOpcode() == Instruction::Alloca && FAnum == 0) {//只有参数计数为0时，才能继续执行拓展，否则FAnum--
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
                                    //在改变变量的集合中添加该变量名
                                    
                                    if (!(eleType->isPointerTy()) || (!(eleType->getContainedType(0)->isPointerTy()))) {
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
                                            
                                            //为新增的指针数组创建bitcast指令
                                            BitCastInst *insetCast = (BitCastInst *) CastInst::Create(Instruction::BitCast, addArrAlloca, destTy, "nc", &(*in));
                                            
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
                                        
                                        
                                        //***建立二级指针与一级指针的关系
                                        std::vector<Value *> indexList;
                                        indexList.push_back(ConstantInt::get(Type::getInt64Ty(bb->getContext()), 0, false));
                                        
                                        
                                        //原来添加eleNum*3条指令建立对应关系
                                        //现在改为用for完成
                                        BasicBlock::iterator testi = inst;
                                        testi++;
                                        
                                        //插入一个for循环，并且在循环体中完成对二级指针和一级指针建立关系
                                        BasicBlock *loopBody = insertForLoopInBasicBlock(tmpF, &(*bb), &(*testi), eleNum);
                                        BasicBlock::iterator ii = bb->end();
                                        //找到for循环的i的值
                                        ii--;
                                        ii--;
                                        ii--;

                                        if (Value *test = dyn_cast<Value>(ii)) {
                                            BasicBlock::iterator bdi = loopBody->end();
                                            bdi--;
                                            LoadInst *insertLoad = new LoadInst(test, "test", &(*bdi));
                                            SExtInst *SEI = new SExtInst(insertLoad, Type::getInt64Ty(bb->getContext()), "sei", &(*bdi));
                                            indexList.push_back(SEI);
                                            llvm::ArrayRef<llvm::Value *> GETidexList(indexList);
                                            
                                            GetElementPtrInst *iniPtrArrNew = GetElementPtrInst::CreateInBounds(addArrAlloca, GETidexList, "ign", &(*bdi));
                                            GetElementPtrInst *iniPtrArrOld = GetElementPtrInst::CreateInBounds(&(*AI), GETidexList, "igo", &(*bdi));
                                            StoreInst *instStore = new StoreInst::StoreInst(iniPtrArrNew, iniPtrArrOld, &(*bdi));
                                        }
                                        
                                        //使现在的基本块跳转到新的基本块
                                        for (int i = 0; i < 4; ++i) {
                                            bb++;
                                        }
                                        //使迭代的指令到达新的基本块的第一条指令，即为逻辑上未插入for循环的下一条指令
                                        inst = bb->begin();
                                        
                                        //***建立二级指针与一级指针的关系
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
                                                    
                                                    ArrayType *cssAType = NULL;
                                                    Type *cssType = NULL;
                                                    int num = 1;
                                                    
                                                    if (csSV) {
                                                        if (csSV->getOperandUse(0)->getType()->getContainedType(0)->isArrayTy()) {
                                                            cssAType = (ArrayType *)csSV->getOperandUse(0)->getType()->getContainedType(0);
                                                        }else{
                                                            cssType = csSV->getType();
                                                        }
                                                        
                                                        if (cssAType) {
                                                            num = cssAType->getNumElements();
                                                        }else{
                                                            num = 1;
                                                        }
                                                        
                                                        std::vector<Value *> cssIndexList;
                                                        cssIndexList.push_back(ConstantInt::get(Type::getInt64Ty(bb->getContext()), 0, false));

                                                        //***插入一个for循环，并且在循环体中完成赋值
                                                        BasicBlock *loopBody = insertForLoopInBasicBlock(tmpF, &(*bb), &(*inst), num);
                                                        BasicBlock::iterator ii = bb->end();
                                                        //找到for循环的i的值
                                                        ii--;
                                                        ii--;
                                                        ii--;

                                                        if (Value *test = dyn_cast<Value>(ii)) {
                                                            BasicBlock::iterator bdi = loopBody->end();
                                                            bdi--;
                                                            LoadInst *insertLoad = new LoadInst(test, "test", &(*bdi));
                                                            SExtInst *SEI = new SExtInst(insertLoad, Type::getInt64Ty(bb->getContext()), "sei", &(*bdi));
                                                            cssIndexList.push_back(SEI);

                                                            llvm::ArrayRef<llvm::Value *> GETcssIdexList(cssIndexList);
                                                            GetElementPtrInst *iniPtrArrNew = GetElementPtrInst::CreateInBounds(csSV->getOperand(0), GETcssIdexList, "ign", &(*(bdi)));
                                                            LoadInst *iniLoad = new LoadInst::LoadInst(iniPtrArrNew, "ni", &(*(bdi)));
                                                            GetElementPtrInst *iniPtrArrOld = GetElementPtrInst::CreateInBounds(&(*AI), GETcssIdexList, "igo", &(*(bdi)));
                                                            LoadInst *iniLoadOld = new LoadInst::LoadInst(iniPtrArrOld, "ni", &(*(bdi)));
                                                            StoreInst *instStore = new StoreInst::StoreInst(iniLoad, iniLoadOld, &(*(bdi)));
                                                        }

                                                        //使现在的基本块跳转到新的基本块
                                                        for (int i = 0; i < 4; ++i) {
                                                            bb++;
                                                        }
                                                        //使迭代的指令到达新的基本块的第一条指令，即为逻辑上未插入for循环的下一条指令
                                                        inst = bb->begin();
                                                        //***完成赋值
                                                        
                                                        //删除二级指针原有的初始化
                                                        BasicBlock::iterator in3 = in;
                                                        ++in;
                                                        in->eraseFromParent();
                                                        in3->eraseFromParent();
                                                    }
                                                    
                                                }
                                            }
                                        }
                                        need_bb_iter_begin = true;
                                    }
                                }
                            }
                            
                            //通过在分配结构体对象时，获取结构体类型信息，创建一个拓展指针的结构体类型（StructType）；然后通过分配一个该新结构体的对象（AllocaInst ），将该结构体声明加入module，再改变原分配指令的分配类型为新结构体，最后删除新结构体分配语句
                            //TODO:目前没考虑引用第三方库结构体，之后考虑对于第三方的结构体过滤？（大体思路 与函数调用一样 遍历完以后 通过名称排除第三方库）
                            if (AI->getAllocatedType()->isStructTy()) {
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
                            
                        }
                        
                        
                    }else if(inst->getOpcode() == Instruction::Alloca){
                        //去掉函数传递的参数分配
                        //确保FAnum不会溢出
                        if (FAnum > 0) {
                            FAnum--;
                        }
                        
                        //对于函数参数分配的变量，将其拓展为多级指针，在通过bitcast指令，将所有形参（指针对指针类型）转换为多级指针
                        if (tmpF->getName().str() != "main") {
                            AllocaInst *argAlloca = dyn_cast<AllocaInst>(inst);
                            if (argAlloca->getAllocatedType()->isPointerTy() && !(argAlloca->getAllocatedType()->getContainedType(0)->isFunctionTy())) {
                                argAlloca->setAllocatedType(llvm::PointerType::getUnqual(argAlloca->getAllocatedType()));
                                argAlloca->mutateType(llvm::PointerType::getUnqual(argAlloca->getAllocatedType()));
                                for (BasicBlock::iterator tmpInst = inst; tmpInst != bb->end(); ++tmpInst) {
                                    if (StoreInst *SI = dyn_cast<StoreInst>(tmpInst)) {
                                        if (SI->getPointerOperand() == argAlloca) {

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
                    
                    
                    
                    
                    
                    if (inst->getOpcode() == Instruction::Load) {
                        LoadInst *newLoad = dyn_cast<LoadInst>(inst);
                        Value *Val = newLoad->getOperand(0);
                        if (inst->getType() != Val->getType()->getContainedType(0)) {
                            if (newLoad->getType()->isPointerTy()) {
                                //此处寻找以后的ICMP指令，并判断ICMP指令的操作数是否来源于该Load指令
                                BasicBlock::iterator in2;
                                ICmpInst *I;
                                bool isIcmpLoad = false;
                                for (in2 = inst; in2 != bb->end(); ++in2) {
                                    if (in2->getOpcode() == Instruction::ICmp) {
                                        if (I = dyn_cast<ICmpInst>(in2)) {
                                            //instComeFromVal函数为判断I的操作数是否来源于newLoad
                                            //函数中排出了一些不符合情况的特例，后面出现BUG，可能需要进一步添加特例
                                            isIcmpLoad = instComeFromVal(I, newLoad);
                                            break;
                                        }
                                    }
                                }
                                
                                //若是load指令后的值用于ICMP对比，则此处获取低一级的内容
                                if (isIcmpLoad) {
                                    LoadInst *insertLoad = new LoadInst(Val, "icmpl", &(*inst));
                                    inst->setOperand(0, insertLoad);
                                }else{
                                    //否则修改类型，获取高一级的内容
                                    Val->getType()->getContainedType(0);
                                    inst->mutateType(Val->getType()->getContainedType(0));
                                    inst->setName("nl" + inst->getName());
                                }

                            }else{
                                
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
                                
                                if (Val->getType()->isPointerTy() && Val->getType()->getContainedType(0)->isPointerTy() && Val->getType()->getContainedType(0)->getContainedType(0)->isStructTy()) {
                                    LoadInst *insertLoad = new LoadInst(Val, "gl", &(*inst));
                                    inst->setOperand(0, insertLoad);
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
                        Function *fTemp = test->getCalledFunction();
                        if (Function *fTemp = test->getCalledFunction()) {
                            fTemp->dump();
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

                                FunctionType *FT = test->getFunctionType();
                                test->dump();
                                for (unsigned i = 0; i < test->getNumOperands() - 1; i++) {
                                    //对于函数的每个参数，判断它是否为指针
                                    if (test->getOperand(i)->getType()->isPointerTy()) {
                                        GetElementPtrInst *GEP;
                                        BitCastInst *BCI;
                                        //判断参数是否由GEP指令得来（数组地址一般由GEP的来）
                                        if (GEP = dyn_cast<GetElementPtrInst>(test->getOperand(i))) {
                                            //若GEP的源操作数依然是GEP，则让其源GEP指令变为近一步判断的GEP指令
                                            if (GetElementPtrInst *GEP2 = dyn_cast<GetElementPtrInst>(GEP->getOperand(0))) {
                                                GEP = GEP2;
                                            }
                                            //判断GEP指令的源操作数是否为数组，且数组内的元素不是指针
                                            //若是的话，建立指针等于
                                            if (GEP->getOperand(0)->getType()->getContainedType(0)->isArrayTy() && !(GEP->getOperand(0)->getType()->getContainedType(0)->getContainedType(0)->isPointerTy())) {
                                                AllocaInst *arrPtr = new AllocaInst(test->getOperand(i)->getType(), "arrP", &(*inst));
                                                StoreInst *SI1 = new StoreInst(test->getOperand(i), arrPtr, "apst", &(*inst));
                                                BCI = new BitCastInst(arrPtr, FT->getParamType(i), "nBCI", &(*inst));
                                            }
                                        }else{
                                            //若不是数组的地址，则认为是指针，直接降级
                                            BCI = new BitCastInst(test->getOperand(i), FT->getParamType(i), "nBCI", &(*inst));
                                        }
                                        //
                                        test->setOperand(i, BCI);
                                    }
                                }
                                
                            }
                        }else{
                            //通过函数指针调用函数
                            //没有判断是否为本地函数
                            //此处函数指针若指向第三方库函数，有BUG！！！！！！！！！
                            //可否新建一个函数外壳，中间
                            
                            for (unsigned i = 0; i < test->getNumOperands() - 1; i++) {
                                if (test->getOperand(i)->getType()->isPointerTy() && !(test->getOperand(i)->getType()->getContainedType(0)->isFunctionTy())) {

                                    if (FunctionType *FT = dyn_cast<FunctionType>(test->getCalledValue()->getType()->getContainedType(0))) {
                                        GetElementPtrInst *GEP;
                                        BitCastInst *BCI;
                                        //判断参数是否由GEP指令得来（数组地址一般由GEP的来）
                                        if (GEP = dyn_cast<GetElementPtrInst>(test->getOperand(i))) {
                                            //若GEP的源操作数依然是GEP，则让其源GEP指令变为近一步判断的GEP指令
                                            if (GetElementPtrInst *GEP2 = dyn_cast<GetElementPtrInst>(GEP->getOperand(0))) {
                                                GEP = GEP2;
                                            }
                                            //判断GEP指令的源操作数是否为数组，且数组内的元素不是指针
                                            //若是的话，建立指针等于
                                            if (GEP->getOperand(0)->getType()->getContainedType(0)->isArrayTy() && !(GEP->getOperand(0)->getType()->getContainedType(0)->getContainedType(0)->isPointerTy())) {
                                                AllocaInst *arrPtr = new AllocaInst(test->getOperand(i)->getType(), "arrP", &(*inst));
                                                StoreInst *SI1 = new StoreInst(test->getOperand(i), arrPtr, "apst", &(*inst));
                                                BCI = new BitCastInst(arrPtr, FT->getParamType(i), "nBCI", &(*inst));
                                            }
                                        }else{
                                            //若不是数组的地址，则认为是指针，直接降级
                                            BCI = new BitCastInst(test->getOperand(i), FT->getParamType(i), "nBCI", &(*inst));
                                        }
                                        test->setOperand(i, BCI);

                                    }
                                    
                                }
                            }
                        }

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
                            Value *tempValue;
                            if (tempValue = dyn_cast<Value>(inst->getOperand(0))) {
                                LoadInst *newLoadInst = new LoadInst(tempValue, "nlPTI", &(*inst));
                                inst->setOperand(0, newLoadInst);
                            }
                        }
                    }
                    
                    if (inst->getOpcode() == Instruction::PHI) {
                        if (PHINode *PHI = dyn_cast<PHINode>(inst)) {
                            PHI->mutateType(PHI->getIncomingValue(0)->getType());
                        }
                    }
                    
                    inst->dump();
                    errs() << "  Inst After" <<'\n' <<'\n';
                }

                
                //最后检验一遍是否存在Store的Ptr和Value不匹配的情况
                errs() << "  ------------------------" << '\n';
                errs() << bb->getName() << '\n';
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
                errs() << "  ------------------------" << '\n' << '\n';
            }
            return true;

            
    };
        
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
        
    };

}

char MyPass::ID = 0;
static RegisterPass<MyPass> X("MyPass", "MyPass Pass");

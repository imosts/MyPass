#include "llvm/Transforms/MyPassMou/MyPassMou.h"

using namespace llvm;

namespace {
    struct MyPassMou : public ModulePass {
        static char ID;
        bool flag;
        
        MyPassMou() : ModulePass(ID) {}
        MyPassMou(bool flag) : ModulePass(ID) {
            this->flag = flag;
        }
        
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
            
            return true;
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
Pass *createMyPassMou() { return new MyPassMou(); }

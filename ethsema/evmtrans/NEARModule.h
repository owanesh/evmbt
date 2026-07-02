#pragma once

#include "Type.h"
#include "BasicBlock.h"
#include "CompilerHelper.h"

#include "preprocessor/llvm_includes_start.h"
#include <llvm/IR/GlobalVariable.h>
#include "preprocessor/llvm_includes_end.h"

namespace dev
{
namespace eth
{
namespace trans
{

 
class NEARModule {
public:
    NEARModule(llvm::Module &m);

    llvm::LLVMContext &VMCtx;
    llvm::Module &VMMod;

    llvm::Function* fn_read_register();
    llvm::Function* fn_input();
    llvm::Function* fn_value_return();

    // near environment    
    llvm::Constant *ATOMIC_OP_REGISTER = 
        llvm::ConstantInt::get(Type::Int64Ty, 0xffffffffffffffff - 2);

private:
    void add_attr(llvm::Function* func, const std::string name);
};

}
}
}
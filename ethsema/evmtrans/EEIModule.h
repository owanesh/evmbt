#pragma once

// #include "Type.h"
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

struct EEIData
{
	enum Index
	{
		CallData,
		CallDataSize
	};

	byte const* callData = nullptr;
	uint64_t 	callDataSize = 0;
};

class EEIModule {

    llvm::Module* TheModule = nullptr;
    IRBuilder& m_builder;
    llvm::LLVMContext& VMContext;

    void initEEIDeclaration();
    void initMemorySection();
    void initUpdateMemorySize();
    void initPrebuiltContract();
    void initBswapI256();
    void initMemcpy();

public:
    llvm::Function *Func_call = nullptr;
    llvm::Function *Func_callCode = nullptr;
    llvm::Function *Func_callDataCopy = nullptr;
    llvm::Function *Func_callStatic = nullptr;
    llvm::Function *Func_callDelegate = nullptr;
    llvm::Function *Func_create = nullptr;
    llvm::Function *Func_create2 = nullptr;
    llvm::Function *Func_finish = nullptr;
    llvm::Function *Func_getCallDataSize = nullptr;
    llvm::Function *Func_getCallValue = nullptr;
    llvm::Function *Func_getCaller = nullptr;
    llvm::Function *Func_codeCopy = nullptr;
    llvm::Function *Func_externalCodeCopy = nullptr;
    llvm::Function *Func_getGasLeft = nullptr;
    llvm::Function *Func_log = nullptr;
    // llvm::Function *Func_log0 = nullptr;
    // llvm::Function *Func_log1 = nullptr;
    // llvm::Function *Func_log2 = nullptr;
    // llvm::Function *Func_log3 = nullptr;
    // llvm::Function *Func_log4 = nullptr;
    llvm::Function *Func_returnDataSize = nullptr;
    llvm::Function *Func_returnDataCopy = nullptr;
    llvm::Function *Func_revert = nullptr;
    llvm::Function *Func_selfDestruct = nullptr;
    llvm::Function *Func_storageLoad = nullptr;
    llvm::Function *Func_storageStore = nullptr;
    llvm::Function *Func_getTxGasPrice = nullptr;
    llvm::Function *Func_getTxOrigin = nullptr;
    llvm::Function *Func_getBlockCoinbase = nullptr;
    llvm::Function *Func_getBlockDifficulty = nullptr;
    llvm::Function *Func_getBlockGasLimit = nullptr;
    llvm::Function *Func_getBlockNumber = nullptr;
    llvm::Function *Func_getBlockTimestamp = nullptr;
    llvm::Function *Func_getBlockHash = nullptr;
    llvm::Function *Func_getExternalBalance = nullptr;
    llvm::Function *Func_getAddress = nullptr;
    llvm::Function *Func_getCodeSize = nullptr;
    llvm::Function *Func_getExternalCodeSize = nullptr;
    llvm::Function *Func_getReturnDataSize = nullptr;
    // support up to London hardfork
    llvm::Function *Func_getBasefee = nullptr;
    llvm::Function *Func_getChainID = nullptr;
    llvm::Function *Func_getSelfBalance = nullptr;
    llvm::Function *Func_getExternalCodeHash = nullptr;

    // llvm::Function *Func_print32 = nullptr;

    llvm::Function *Func_keccak256 = nullptr;
    llvm::Function *Func_sha256 = nullptr;
    llvm::Function *Func_sha3 = nullptr;
    llvm::Function *Func_ripemd160 = nullptr;
    llvm::Function *Func_ecrecover = nullptr;

    llvm::Function *Func_exp256 = nullptr;
    llvm::Function *Func_bswap256 = nullptr;
    llvm::Function *Func_memcpy = nullptr;
    llvm::Function *Func_updateMemorySize = nullptr;
    

    llvm::GlobalVariable *MemorySize;
    llvm::Value *StackBase = nullptr;
	llvm::Value *StackSize = nullptr;
    llvm::Value *JmpBuf = nullptr;

    EEIModule(IRBuilder &_builder, llvm::Module *_module);
    void init();

    llvm::Value *prepareData();
    void initRuntime();

    llvm::Value* getStackBase() { return StackBase; }
	llvm::Value* getStackSize() { return StackSize; }
    llvm::Value* getJmpBuf() { return JmpBuf; }

    llvm::Value *emitEndianConvert(llvm::Value *Val);

    // void emitABIStore(llvm::StringRef Name, llvm::Value *Result);
    void registerReturnData(llvm::Value* _offset, llvm::Value* _size);

};

}
}
}
// following     https://github.com/ewasm/design/commit/8946232935822723a4c80bec03eb1b8ecd237d5f
#pragma once

#include "EEIModule.h"
#include "Type.h"
// #include "Endianness.h"

#include "preprocessor/llvm_includes_start.h"
#include <llvm/IR/CFG.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/IntrinsicInst.h>
#include <llvm/IR/GlobalVariable.h>
#include <llvm/IR/Constants.h>
#include <llvm/Support/Alignment.h>
#include "preprocessor/llvm_includes_end.h"

namespace dev
{
namespace eth
{
namespace trans
{

EEIModule::EEIModule(IRBuilder &_builder, llvm::Module *_module) :
    m_builder(_builder),
    TheModule(_module),
    VMContext(_builder.getContext()) {}

void EEIModule::init() {
    initMemorySection();
    initEEIDeclaration();
    // initHelperDeclaration();
    initPrebuiltContract();
    initBswapI256();
    initMemcpy();
}

void EEIModule::initMemcpy() {
    Func_memcpy = llvm::Function::Create(
        llvm::FunctionType::get(Type::Void, {Type::Int8PtrTy, Type::Int8PtrTy, Type::Int32Ty}, false),
        llvm::Function::InternalLinkage, "solidity.memcpy", TheModule);
    Func_memcpy->addFnAttr(llvm::Attribute::NoUnwind);

    llvm::Argument *const Dst = Func_memcpy->arg_begin();
    llvm::Argument *const Src = Dst + 1;
    llvm::Argument *const Length = Src + 1;
    llvm::ConstantInt *const One = m_builder.getInt32(1);
    Dst->setName("dst");
    Src->setName("src");
    Length->setName("length");

    llvm::BasicBlock *Entry =
        llvm::BasicBlock::Create(VMContext, "entry", Func_memcpy);
    llvm::BasicBlock *Loop =
        llvm::BasicBlock::Create(VMContext, "loop", Func_memcpy);
    llvm::BasicBlock *Return =
        llvm::BasicBlock::Create(VMContext, "return", Func_memcpy);

    m_builder.SetInsertPoint(Entry);
    llvm::Value *Cmp = m_builder.CreateICmpNE(Length, m_builder.getInt32(0));
    m_builder.CreateCondBr(Cmp, Loop, Return);

    m_builder.SetInsertPoint(Loop);
    llvm::PHINode *SrcPHI = m_builder.CreatePHI(Type::Int8PtrTy, 2);
    llvm::PHINode *DstPHI = m_builder.CreatePHI(Type::Int8PtrTy, 2);
    llvm::PHINode *LengthPHI = m_builder.CreatePHI(Type::Int32Ty, 2);

    llvm::Value *Value = m_builder.CreateLoad(SrcPHI);
    m_builder.CreateStore(Value, DstPHI);
    llvm::Value *Src2 = m_builder.CreateInBoundsGEP(SrcPHI, {One});
    llvm::Value *Dst2 = m_builder.CreateInBoundsGEP(DstPHI, {One});
    llvm::Value *Length2 = m_builder.CreateSub(LengthPHI, One);
    llvm::Value *Cmp2 = m_builder.CreateICmpNE(Length2, m_builder.getInt32(0));
    m_builder.CreateCondBr(Cmp2, Loop, Return);

    m_builder.SetInsertPoint(Return);
    m_builder.CreateRetVoid();

    SrcPHI->addIncoming(Src, Entry);
    SrcPHI->addIncoming(Src2, Loop);
    DstPHI->addIncoming(Dst, Entry);
    DstPHI->addIncoming(Dst2, Loop);
    LengthPHI->addIncoming(Length, Entry);
    LengthPHI->addIncoming(Length2, Loop);
}


llvm::Value *EEIModule::emitEndianConvert(llvm::Value *Val) {

    if (auto constant = llvm::dyn_cast<llvm::ConstantInt>(Val))
        return m_builder.getInt(constant->getValue().byteSwap());

    llvm::StringRef Name = Val->getName();
    llvm::Type *Ty = Val->getType();
    llvm::Value *Ext = m_builder.CreateZExtOrTrunc(Val, Type::Int256Ty, "extend_256");
    if (const unsigned BitWidth = Ty->getIntegerBitWidth(); BitWidth != 256) {
        Ext = m_builder.CreateShl(Ext, 256 - BitWidth, "shift_left");
    }
    llvm::Value *Reverse = m_builder.CreateCall(Func_bswap256, {Ext}, Name + ".reverse");
    return m_builder.CreateTrunc(Reverse, Ty, Name + ".trunc");
}

void EEIModule::initBswapI256() {
    Func_bswap256 = llvm::Function::Create(
        llvm::FunctionType::get(Type::Int256Ty, {Type::Int256Ty}, false), llvm::Function::InternalLinkage, "solidity.bswapi256", TheModule);
    Func_bswap256->addFnAttr(llvm::Attribute::NoUnwind);
    Func_bswap256->addFnAttr(llvm::Attribute::ReadNone);

    auto *const Arg = Func_bswap256->arg_begin();
    Arg->setName("data");

    llvm::BasicBlock *Entry =
        llvm::BasicBlock::Create(VMContext, "bswapi256.entry", Func_bswap256);
    m_builder.SetInsertPoint(Entry);

    llvm::Value *Data[32];
    for (size_t I = 0; I < 32; ++I) {
        if (I < 16) {
        Data[I] = m_builder.CreateShl(Arg, 248 - I * 16);
        } else {
        Data[I] = m_builder.CreateLShr(Arg, I * 16 - 248);
        }
        if (I != 0 && I != 31) {
        Data[I] = m_builder.CreateAnd(Data[I], llvm::APInt(256, 0xFF, false)
                                                << ((31 - I) * 8));
        }
    }
    llvm::Value *Result = m_builder.CreateOr(Data[0], Data[1]);
    for (size_t I = 2; I < 32; ++I) {
        Result = m_builder.CreateOr(Result, Data[I]);
    }
    m_builder.CreateRet(Result);
}

void EEIModule::initMemorySection() {
    MemorySize = new llvm::GlobalVariable(*TheModule, Type::Int256Ty, false, llvm::GlobalVariable::ExternalLinkage, m_builder.getIntN(256, 0), "memory.size");
    MemorySize->setUnnamedAddr(llvm::GlobalVariable::UnnamedAddr::Global);
    MemorySize->setAlignment(llvm::MaybeAlign(256));

    // HeapBase = new llvm::GlobalVariable(*TheModule, Type::Int8Ty, false, llvm::GlobalVariable::ExternalLinkage, nullptr, "__heap_base");
    // HeapBase->setAlignment(llvm::MaybeAlign(1));

    // StackBase = m_builder.CreateCall(mallocFunc, m_builder.getInt64(Type::Word->getPrimitiveSizeInBits() / 8 * stackSizeLimit), "stack.base"); // TODO: Use Type::SizeT type
	// StackSize = m_builder.CreateAlloca(Type::Size, nullptr, "stack.size");
	// m_builder.CreateStore(m_builder.getInt64(0), m_stackSize);

    // Func_updateMemorySize = llvm::Function::Create(llvm::FunctionType::get(Type::Void, {Type::Int256Ty, Type::Int256Ty}, false), llvm::Function::InternalLinkage, "solidity.updateMemorySize", TheModule);
    // Func_updateMemorySize->addFnAttr(llvm::Attribute::NoUnwind);
    // initUpdateMemorySize();
}

void EEIModule::initUpdateMemorySize() {
    llvm::Argument *Pos = Func_updateMemorySize->arg_begin();
    Pos->setName("memory.pos");
    llvm::Argument *Range = Pos + 1;
    Range->setName("memory.range");
    llvm::BasicBlock *Entry =
        llvm::BasicBlock::Create(VMContext, "UpdateMemory.entry", Func_updateMemorySize);
    llvm::BasicBlock *Update =
        llvm::BasicBlock::Create(VMContext, "UpdateMemory.update", Func_updateMemorySize);
    llvm::BasicBlock *Done =
        llvm::BasicBlock::Create(VMContext, "UpdateMemory.done", Func_updateMemorySize);
    m_builder.SetInsertPoint(Entry);
    llvm::Value *OrigSize = m_builder.CreateLoad(MemorySize, "memory.size");
    llvm::Value *EndPos = m_builder.CreateAdd(Pos, Range);
    llvm::Value *Condition = m_builder.CreateICmpUGT(EndPos, OrigSize);
    m_builder.CreateCondBr(Condition, Update, Done);
    m_builder.SetInsertPoint(Update);
    llvm::ConstantInt *Mask =
        m_builder.getInt(llvm::APInt::getHighBitsSet(256, 251));
    llvm::Value *Base = m_builder.CreateAnd(EndPos, Mask);
    llvm::Value *NewSize =
        m_builder.CreateAdd(Base, m_builder.getIntN(256, 32), "memory.new_size");
    m_builder.CreateStore(NewSize, MemorySize);
    m_builder.CreateBr(Done);
    m_builder.SetInsertPoint(Done);
    m_builder.CreateRetVoid();
}

void EEIModule::initEEIDeclaration() {
    //see https://github.com/ewasm/design/commit/8946232935822723a4c80bec03eb1b8ecd237d5f

    // llvm::LLVMContext& VMContext = m_builder.getContext();
    // llvm::Module* TheModule = getModule();

    llvm::FunctionType *FT = nullptr;
    llvm::Attribute Ethereum =
        llvm::Attribute::get(VMContext, "wasm-import-module", "ethereum");
    llvm::Attribute Debug =
        llvm::Attribute::get(VMContext, "wasm-import-module", "debug");

    // callDataCopy
    FT = llvm::FunctionType::get(Type::Void, {Type::Int8PtrTy, Type::Int32Ty, Type::Int32Ty}, false);
    Func_callDataCopy = llvm::Function::Create(
        FT, llvm::Function::ExternalLinkage, "ethereum.callDataCopy", TheModule);
    Func_callDataCopy->addFnAttr(Ethereum);
    Func_callDataCopy->addFnAttr(
        llvm::Attribute::get(VMContext, "wasm-import-name", "callDataCopy"));
    Func_callDataCopy->addFnAttr(llvm::Attribute::WriteOnly);
    Func_callDataCopy->addFnAttr(llvm::Attribute::NoUnwind);
    Func_callDataCopy->addParamAttr(0, llvm::Attribute::WriteOnly);

    // call
    FT = llvm::FunctionType::get(
        Type::Int32Ty, {Type::Int64Ty, Type::AddressPtrTy, Type::Int32PtrTy, Type::Int8PtrTy, Type::Int32Ty}, false);
    Func_call = llvm::Function::Create(FT, llvm::Function::ExternalLinkage,
                                      "ethereum.call", TheModule);
    Func_call->addFnAttr(Ethereum);
    Func_call->addFnAttr(
        llvm::Attribute::get(VMContext, "wasm-import-name", "call"));
    Func_call->addFnAttr(llvm::Attribute::NoUnwind);
    Func_call->addParamAttr(1, llvm::Attribute::ReadOnly);
    Func_call->addParamAttr(2, llvm::Attribute::ReadOnly);
    Func_call->addParamAttr(3, llvm::Attribute::ReadOnly);

    // callCode
    FT = llvm::FunctionType::get(
        Type::Int32Ty, {Type::Int64Ty, Type::AddressPtrTy, Type::Int32PtrTy, Type::Int8PtrTy, Type::Int32Ty}, false);
    Func_callCode = llvm::Function::Create(FT, llvm::Function::ExternalLinkage,
                                      "ethereum.callCode", TheModule);
    Func_callCode->addFnAttr(Ethereum);
    Func_callCode->addFnAttr(
        llvm::Attribute::get(VMContext, "wasm-import-name", "callCode"));
    Func_callCode->addFnAttr(llvm::Attribute::NoUnwind);
    Func_callCode->addParamAttr(1, llvm::Attribute::ReadOnly);
    Func_callCode->addParamAttr(2, llvm::Attribute::ReadOnly);
    Func_callCode->addParamAttr(3, llvm::Attribute::ReadOnly);

    // callStatic
    FT = llvm::FunctionType::get(
        Type::Int32Ty, {Type::Int64Ty, Type::AddressPtrTy, Type::Int8PtrTy, Type::Int32Ty}, false);
    Func_callStatic = llvm::Function::Create(FT, llvm::Function::ExternalLinkage,
                                            "ethereum.callStatic", TheModule);
    Func_callStatic->addFnAttr(Ethereum);
    Func_callStatic->addFnAttr(
        llvm::Attribute::get(VMContext, "wasm-import-name", "callStatic"));
    Func_callStatic->addFnAttr(llvm::Attribute::NoUnwind);
    Func_callStatic->addParamAttr(1, llvm::Attribute::ReadOnly);
    Func_callStatic->addParamAttr(2, llvm::Attribute::ReadOnly);

    // callDelegate
    FT = llvm::FunctionType::get(
        Type::Int32Ty, {Type::Int64Ty, Type::AddressPtrTy, Type::Int8PtrTy, Type::Int32Ty}, false);
    Func_callDelegate = llvm::Function::Create(
        FT, llvm::Function::ExternalLinkage, "ethereum.callDelegate", TheModule);
    Func_callDelegate->addFnAttr(Ethereum);
    Func_callDelegate->addFnAttr(
        llvm::Attribute::get(VMContext, "wasm-import-name", "callDelegate"));
    Func_callDelegate->addFnAttr(llvm::Attribute::NoUnwind);
    Func_callDelegate->addParamAttr(1, llvm::Attribute::ReadOnly);
    Func_callDelegate->addParamAttr(2, llvm::Attribute::ReadOnly);

    // create
    FT = llvm::FunctionType::get(
        Type::Int32Ty, {Type::Int32PtrTy, Type::Int32PtrTy, Type::Int32Ty, Type::Int32PtrTy,}, false);
    Func_create = llvm::Function::Create(
        FT, llvm::Function::ExternalLinkage, "ethereum.create", TheModule);
    Func_create->addFnAttr(Ethereum);
    Func_create->addFnAttr(
        llvm::Attribute::get(VMContext, "wasm-import-name", "create"));
    Func_create->addFnAttr(llvm::Attribute::NoUnwind);
    Func_create->addParamAttr(0, llvm::Attribute::ReadOnly);
    Func_create->addParamAttr(1, llvm::Attribute::ReadOnly);


    // FT = llvm::FunctionType::get(
    //     Type::Int32Ty, {Type::Int128PtrTy, Type::Int8PtrTy, Type::Int32Ty, Type::Int256PtrTy, Type::AddressPtrTy},
    //     false);
    FT = llvm::FunctionType::get(
        Type::Int32Ty, {Type::Int32PtrTy, Type::Int32PtrTy, Type::Int32Ty, Type::Int32PtrTy, Type::Int32PtrTy}, false);
    Func_create2 = llvm::Function::Create(FT, llvm::Function::ExternalLinkage,
                                            "ethereum.create2", TheModule);
    Func_create2->addFnAttr(Ethereum);
    Func_create2->addFnAttr(
        llvm::Attribute::get(VMContext, "wasm-import-name", "create2"));
    Func_create->addFnAttr(llvm::Attribute::NoUnwind);
    Func_create->addParamAttr(0, llvm::Attribute::ReadOnly);
    Func_create->addParamAttr(1, llvm::Attribute::ReadOnly);  

    // finish
    FT = llvm::FunctionType::get(Type::Void, {Type::Int8PtrTy, Type::Int32Ty}, false);
    Func_finish = llvm::Function::Create(FT, llvm::Function::ExternalLinkage,
                                        "ethereum.finish", TheModule);
    Func_finish->addFnAttr(Ethereum);
    Func_finish->addFnAttr(
        llvm::Attribute::get(VMContext, "wasm-import-name", "finish"));
    Func_finish->addFnAttr(llvm::Attribute::WriteOnly);
    Func_finish->addFnAttr(llvm::Attribute::NoUnwind);
    Func_finish->addParamAttr(0, llvm::Attribute::ReadOnly);

    // getCallDataSize
    FT = llvm::FunctionType::get(Type::Int32Ty, {}, false);
    Func_getCallDataSize =
        llvm::Function::Create(FT, llvm::Function::ExternalLinkage,
                              "ethereum.getCallDataSize", TheModule);
    Func_getCallDataSize->addFnAttr(Ethereum);
    Func_getCallDataSize->addFnAttr(
        llvm::Attribute::get(VMContext, "wasm-import-name", "getCallDataSize"));
    Func_getCallDataSize->addFnAttr(llvm::Attribute::ReadOnly);
    Func_getCallDataSize->addFnAttr(llvm::Attribute::NoUnwind);

    // getCallValue
    FT = llvm::FunctionType::get(Type::Void, {Type::Int128PtrTy}, false);
    Func_getCallValue = llvm::Function::Create(
        FT, llvm::Function::ExternalLinkage, "ethereum.getCallValue", TheModule);
    Func_getCallValue->addFnAttr(Ethereum);
    Func_getCallValue->addFnAttr(
        llvm::Attribute::get(VMContext, "wasm-import-name", "getCallValue"));

    // getCaller
    FT = llvm::FunctionType::get(Type::Void, {Type::Int32PtrTy}, false);
    Func_getCaller = llvm::Function::Create(FT, llvm::Function::ExternalLinkage,
                                            "ethereum.getCaller", TheModule);
    Func_getCaller->addFnAttr(Ethereum);
    Func_getCaller->addFnAttr(
        llvm::Attribute::get(VMContext, "wasm-import-name", "getCaller"));
    Func_getCaller->addFnAttr(llvm::Attribute::ArgMemOnly);
    Func_getCaller->addFnAttr(llvm::Attribute::NoUnwind);
    Func_getCaller->addParamAttr(0, llvm::Attribute::WriteOnly);

    // codeCopy
    FT = llvm::FunctionType::get(Type::Void, {Type::Int8PtrTy, Type::Int32Ty, Type::Int32Ty}, false);
    Func_codeCopy = llvm::Function::Create(FT, llvm::Function::ExternalLinkage,
                                          "ethereum.codeCopy", TheModule);
    Func_codeCopy->addFnAttr(Ethereum);
    Func_codeCopy->addFnAttr(
        llvm::Attribute::get(VMContext, "wasm-import-name", "codeCopy"));
    Func_codeCopy->addFnAttr(llvm::Attribute::ArgMemOnly);
    Func_codeCopy->addFnAttr(llvm::Attribute::NoUnwind);
    Func_codeCopy->addParamAttr(0, llvm::Attribute::WriteOnly);

    // externalCodeCopy
    FT = llvm::FunctionType::get(
        Type::Void, {Type::Int32PtrTy, Type::Int32PtrTy, Type::Int32Ty, Type::Int32Ty}, false);
    Func_externalCodeCopy =
        llvm::Function::Create(FT, llvm::Function::ExternalLinkage,
                              "ethereum.externalCodeCopy", TheModule);
    Func_externalCodeCopy->addFnAttr(Ethereum);
    Func_externalCodeCopy->addFnAttr(
        llvm::Attribute::get(VMContext, "wasm-import-name", "externalCodeCopy"));
    Func_externalCodeCopy->addFnAttr(llvm::Attribute::ArgMemOnly);
    Func_externalCodeCopy->addFnAttr(llvm::Attribute::NoUnwind);

    // getGasLeft
    FT = llvm::FunctionType::get(Type::Int64Ty, {}, false);
    Func_getGasLeft = llvm::Function::Create(FT, llvm::Function::ExternalLinkage,
                                            "ethereum.getGasLeft", TheModule);
    Func_getGasLeft->addFnAttr(Ethereum);
    Func_getGasLeft->addFnAttr(
        llvm::Attribute::get(VMContext, "wasm-import-name", "getGasLeft"));
    Func_getGasLeft->addFnAttr(llvm::Attribute::NoUnwind);

    // log
    FT = llvm::FunctionType::get(Type::Void,
                                {Type::Int32PtrTy, Type::Int32Ty, Type::Int32Ty, Type::Int32PtrTy, Type::Int32PtrTy, Type::Int32PtrTy, Type::Int32PtrTy},
                                false);
    Func_log = llvm::Function::Create(FT, llvm::Function::ExternalLinkage,
                                      "ethereum.log", TheModule);
    Func_log->addFnAttr(Ethereum);
    Func_log->addFnAttr(
        llvm::Attribute::get(VMContext, "wasm-import-name", "log"));
    Func_log->addFnAttr(llvm::Attribute::NoUnwind);
    Func_log->addParamAttr(0, llvm::Attribute::ReadOnly);
    Func_log->addParamAttr(3, llvm::Attribute::ReadOnly);
    Func_log->addParamAttr(4, llvm::Attribute::ReadOnly);
    Func_log->addParamAttr(5, llvm::Attribute::ReadOnly);
    Func_log->addParamAttr(6, llvm::Attribute::ReadOnly);

    // returnDataSize
    FT = llvm::FunctionType::get(Type::Int32Ty, {}, false);
    Func_returnDataSize =
        llvm::Function::Create(FT, llvm::Function::ExternalLinkage,
                              "ethereum.returnDataSize", TheModule);
    Func_returnDataSize->addFnAttr(Ethereum);
    Func_returnDataSize->addFnAttr(
        llvm::Attribute::get(VMContext, "wasm-import-name", "returnDataSize"));

    // returnDataCopy
    FT = llvm::FunctionType::get(Type::Void, {Type::Int8PtrTy, Type::Int32Ty, Type::Int32Ty}, false);
    Func_returnDataCopy =
        llvm::Function::Create(FT, llvm::Function::ExternalLinkage,
                              "ethereum.returnDataCopy", TheModule);
    Func_returnDataCopy->addFnAttr(Ethereum);
    Func_returnDataCopy->addFnAttr(
        llvm::Attribute::get(VMContext, "wasm-import-name", "returnDataCopy"));
    Func_returnDataCopy->addFnAttr(llvm::Attribute::NoUnwind);
    Func_returnDataCopy->addParamAttr(0, llvm::Attribute::WriteOnly);

    // revert
    FT = llvm::FunctionType::get(Type::Void, {Type::Int8PtrTy, Type::Int32Ty}, false);
    Func_revert = llvm::Function::Create(FT, llvm::Function::ExternalLinkage,
                                        "ethereum.revert", TheModule);
    Func_revert->addFnAttr(Ethereum);
    Func_revert->addFnAttr(
        llvm::Attribute::get(VMContext, "wasm-import-name", "revert"));
    Func_revert->addFnAttr(llvm::Attribute::WriteOnly);
    Func_revert->addFnAttr(llvm::Attribute::NoUnwind);
    Func_revert->addParamAttr(0, llvm::Attribute::ReadOnly);

    // selfDestruct
    FT = llvm::FunctionType::get(Type::Void, {Type::Int32PtrTy}, false);
    Func_selfDestruct = llvm::Function::Create(FT, llvm::Function::ExternalLinkage, "ethereum.selfDestruct", TheModule);
    Func_selfDestruct->addFnAttr(Ethereum);
    Func_selfDestruct->addFnAttr(
        llvm::Attribute::get(VMContext, "wasm-import-name", "selfDestruct"));
    Func_selfDestruct->addFnAttr(llvm::Attribute::NoUnwind);
    Func_selfDestruct->addParamAttr(0, llvm::Attribute::ReadOnly);

    // storageLoad
    FT = llvm::FunctionType::get(Type::Void, {Type::Int32PtrTy, Type::Int32PtrTy}, false);
    Func_storageLoad = llvm::Function::Create(FT, llvm::Function::ExternalLinkage,
                                              "ethereum.storageLoad", TheModule);
    Func_storageLoad->addFnAttr(Ethereum);
    Func_storageLoad->addFnAttr(
        llvm::Attribute::get(VMContext, "wasm-import-name", "storageLoad"));
    Func_storageLoad->addFnAttr(llvm::Attribute::NoUnwind);
    Func_storageLoad->addParamAttr(0, llvm::Attribute::ReadOnly);
    Func_storageLoad->addParamAttr(1, llvm::Attribute::WriteOnly);

    // storageStore
    FT = llvm::FunctionType::get(Type::Void, {Type::Int32PtrTy, Type::Int32PtrTy}, false);
    Func_storageStore = llvm::Function::Create(
        FT, llvm::Function::ExternalLinkage, "ethereum.storageStore", TheModule);
    Func_storageStore->addFnAttr(Ethereum);
    Func_storageStore->addFnAttr(
        llvm::Attribute::get(VMContext, "wasm-import-name", "storageStore"));
    Func_storageStore->addFnAttr(llvm::Attribute::NoUnwind);
    Func_storageStore->addParamAttr(0, llvm::Attribute::ReadOnly);
    Func_storageStore->addParamAttr(1, llvm::Attribute::ReadOnly);

    // getTxGasPrice
    FT = llvm::FunctionType::get(Type::Void, {Type::Int32PtrTy}, false);
    Func_getTxGasPrice = llvm::Function::Create(
        FT, llvm::Function::ExternalLinkage, "ethereum.getTxGasPrice", TheModule);
    Func_getTxGasPrice->addFnAttr(Ethereum);
    Func_getTxGasPrice->addFnAttr(
        llvm::Attribute::get(VMContext, "wasm-import-name", "getTxGasPrice"));

    // getTxOrigin
    FT = llvm::FunctionType::get(Type::Void, {Type::Int32PtrTy}, false);
    Func_getTxOrigin = llvm::Function::Create(FT, llvm::Function::ExternalLinkage,
                                              "ethereum.getTxOrigin", TheModule);
    Func_getTxOrigin->addFnAttr(Ethereum);
    Func_getTxOrigin->addFnAttr(
        llvm::Attribute::get(VMContext, "wasm-import-name", "getTxOrigin"));

    // getBlockCoinbase
    FT = llvm::FunctionType::get(Type::Void, {Type::Int32PtrTy}, false);
    Func_getBlockCoinbase =
        llvm::Function::Create(FT, llvm::Function::ExternalLinkage,
                              "ethereum.getBlockCoinbase", TheModule);
    Func_getBlockCoinbase->addFnAttr(Ethereum);
    Func_getBlockCoinbase->addFnAttr(
        llvm::Attribute::get(VMContext, "wasm-import-name", "getBlockCoinbase"));

    // getBlockDifficulty
    FT = llvm::FunctionType::get(Type::Void, {Type::Int32PtrTy}, false);
    Func_getBlockDifficulty =
        llvm::Function::Create(FT, llvm::Function::ExternalLinkage,
                              "ethereum.getBlockDifficulty", TheModule);
    Func_getBlockDifficulty->addFnAttr(Ethereum);
    Func_getBlockDifficulty->addFnAttr(llvm::Attribute::get(
        VMContext, "wasm-import-name", "getBlockDifficulty"));

    // getBlockGasLimit
    FT = llvm::FunctionType::get(Type::Int64Ty, {}, false);
    Func_getBlockGasLimit =
        llvm::Function::Create(FT, llvm::Function::ExternalLinkage,
                              "ethereum.getBlockGasLimit", TheModule);
    Func_getBlockGasLimit->addFnAttr(Ethereum);
    Func_getBlockGasLimit->addFnAttr(
        llvm::Attribute::get(VMContext, "wasm-import-name", "getBlockGasLimit"));

    // getBlockNumber
    FT = llvm::FunctionType::get(Type::Int64Ty, {}, false);
    Func_getBlockNumber =
        llvm::Function::Create(FT, llvm::Function::ExternalLinkage,
                              "ethereum.getBlockNumber", TheModule);
    Func_getBlockNumber->addFnAttr(Ethereum);
    Func_getBlockNumber->addFnAttr(
        llvm::Attribute::get(VMContext, "wasm-import-name", "getBlockNumber"));

    // getBlockTimestamp
    FT = llvm::FunctionType::get(Type::Int64Ty, {}, false);
    Func_getBlockTimestamp =
        llvm::Function::Create(FT, llvm::Function::ExternalLinkage,
                              "ethereum.getBlockTimestamp", TheModule);
    Func_getBlockTimestamp->addFnAttr(Ethereum);
    Func_getBlockTimestamp->addFnAttr(
        llvm::Attribute::get(VMContext, "wasm-import-name", "getBlockTimestamp"));

    // getBlockHash
    FT = llvm::FunctionType::get(Type::Int32Ty, {Type::Int64Ty, Type::Int32PtrTy}, false);
    Func_getBlockHash = llvm::Function::Create(
        FT, llvm::Function::ExternalLinkage, "ethereum.getBlockHash", TheModule);
    Func_getBlockHash->addFnAttr(Ethereum);
    Func_getBlockHash->addFnAttr(
        llvm::Attribute::get(VMContext, "wasm-import-name", "getBlockHash"));

    // getExternalBalance
    FT = llvm::FunctionType::get(Type::Void, {Type::Int32PtrTy, Type::Int32PtrTy}, false);
    Func_getExternalBalance =
        llvm::Function::Create(FT, llvm::Function::ExternalLinkage,
                              "ethereum.getExternalBalance", TheModule);
    Func_getExternalBalance->addFnAttr(Ethereum);
    Func_getExternalBalance->addFnAttr(llvm::Attribute::get(
        VMContext, "wasm-import-name", "getExternalBalance"));

    // debug.print32
    // FT = llvm::FunctionType::get(Type::Void, {Type::Int32Ty}, false);
    // Func_print32 = llvm::Function::Create(FT, llvm::Function::ExternalLinkage,
    //                                       "debug.print32", TheModule);
    // Func_print32->addFnAttr(Debug);
    // Func_print32->addFnAttr(
    //     llvm::Attribute::get(VMContext, "wasm-import-name", "print32"));
    // Func_print32->addFnAttr(llvm::Attribute::NoUnwind);

    // getAddress
    FT = llvm::FunctionType::get(Type::Void, {Type::Int32PtrTy}, false);
    Func_getAddress = llvm::Function::Create(FT, llvm::Function::ExternalLinkage,
                                            "ethereum.getAddress", TheModule);
    Func_getAddress->addFnAttr(Ethereum);
    Func_getAddress->addFnAttr(
        llvm::Attribute::get(VMContext, "wasm-import-name", "getAddress"));

    // getCodeSize
    FT = llvm::FunctionType::get(Type::Int32Ty, {}, false);
    Func_getCodeSize = llvm::Function::Create(FT, llvm::Function::ExternalLinkage,
                                              "ethereum.getCodeSize", TheModule);
    Func_getCodeSize->addFnAttr(Ethereum);
    Func_getCodeSize->addFnAttr(
        llvm::Attribute::get(VMContext, "wasm-import-name", "getCodeSize"));

    // getExternalCodeSize
    FT = llvm::FunctionType::get(Type::Int32Ty, {Type::Int32PtrTy}, false);
    Func_getExternalCodeSize =
        llvm::Function::Create(FT, llvm::Function::ExternalLinkage,
                              "ethereum.getExternalCodeSize", TheModule);
    Func_getExternalCodeSize->addFnAttr(Ethereum);
    Func_getExternalCodeSize->addFnAttr(llvm::Attribute::get(
        VMContext, "wasm-import-name", "getExternalCodeSize"));

    // getReturnDataSize
    FT = llvm::FunctionType::get(Type::Int32Ty, {}, false);
    Func_getReturnDataSize =
        llvm::Function::Create(FT, llvm::Function::ExternalLinkage,
                              "ethereum.getReturnDataSize", TheModule);
    Func_getReturnDataSize->addFnAttr(Ethereum);
    Func_getReturnDataSize->addFnAttr(
        llvm::Attribute::get(VMContext, "wasm-import-name", "getReturnDataSize"));

    // getBasefee
    FT = llvm::FunctionType::get(Type::Int64Ty, {}, false);
    Func_getBasefee =
        llvm::Function::Create(FT, llvm::Function::ExternalLinkage,
                              "ethereum.getBasefee", TheModule);
    Func_getBasefee->addFnAttr(Ethereum);
    Func_getBasefee->addFnAttr(
        llvm::Attribute::get(VMContext, "wasm-import-name", "getBasefee"));
    
    // getChainID
    FT = llvm::FunctionType::get(Type::Int64Ty, {}, false);
    Func_getChainID =
        llvm::Function::Create(FT, llvm::Function::ExternalLinkage,
                              "ethereum.getChainID", TheModule);
    Func_getChainID->addFnAttr(Ethereum);
    Func_getChainID->addFnAttr(
        llvm::Attribute::get(VMContext, "wasm-import-name", "getChainID"));
    
    // getSelfBalance
    FT = llvm::FunctionType::get(Type::Void, {Type::Int32PtrTy}, false);
    Func_getSelfBalance =
        llvm::Function::Create(FT, llvm::Function::ExternalLinkage,
                              "ethereum.getSelfBalance", TheModule);
    Func_getSelfBalance->addFnAttr(Ethereum);
    Func_getSelfBalance->addFnAttr(llvm::Attribute::get(
        VMContext, "wasm-import-name", "getSelfBalance"));

    // getExternalCodeHash
    FT = llvm::FunctionType::get(Type::Void, {Type::Int32PtrTy, Type::Int32PtrTy}, false);
    Func_getExternalCodeHash = llvm::Function::Create(
        FT, llvm::Function::ExternalLinkage, "ethereum.getExternalCodeHash", TheModule);
    Func_getExternalCodeHash->addFnAttr(Ethereum);
    Func_getExternalCodeHash->addFnAttr(
        llvm::Attribute::get(VMContext, "wasm-import-name", "getExternalCodeHash"));
}

void EEIModule::initPrebuiltContract() {
    // llvm::FunctionType *FT = llvm::FunctionType::get(Type::Int256Ty, {Type::BytesTy}, false);
    // Func_keccak256 = llvm::Function::Create(FT, llvm::Function::InternalLinkage, "solidity.keccak256", TheModule);
    // Func_keccak256->addFnAttr(llvm::Attribute::NoUnwind);


    // llvm::Argument *Memory = Func_keccak256->arg_begin();
    // Memory->setName("memory");
    // // llvm::Argument *Length = Ptr + 1;
    // // Length->setName("length");

    // llvm::BasicBlock *Entry =
    //     llvm::BasicBlock::Create(VMContext, "sha3.entry", Func_keccak256);
    // m_builder.SetInsertPoint(Entry);

    // llvm::Value *Length = m_builder.CreateTrunc(
    //     m_builder.CreateExtractValue(Memory, {0}), Type::Int32Ty, "length");
    // llvm::Value *Ptr = m_builder.CreateExtractValue(Memory, {1}, "ptr");

    // llvm::Value *AddressPtr =
    //     m_builder.CreateAlloca(Type::AddressTy, nullptr, "address.ptr");
    // llvm::APInt Address = llvm::APInt(160, 9).byteSwap();
    // m_builder.CreateStore(m_builder.getInt(Address), AddressPtr);


    // llvm::Value *Fee = m_builder.CreateCall(Func_getGasLeft, {});
    // m_builder.CreateCall(Func_callStatic, {Fee, AddressPtr, Ptr, Length});
    // llvm::Value *ResultPtr =
    //     m_builder.CreateAlloca(Type::Int256Ty, nullptr, "result.ptr");
    // llvm::Value *ResultVPtr =
    //     m_builder.CreateBitCast(ResultPtr, Type::Int8PtrTy, "result.vptr");
    // m_builder.CreateCall(Func_returnDataCopy,
    //                    {ResultVPtr, m_builder.getInt32(0), m_builder.getInt32(32)});
    // llvm::Value *Result = m_builder.CreateLoad(ResultPtr);

    // m_builder.CreateRet(Result);
    
 
    // keccak256
    llvm::FunctionType *FT = llvm::FunctionType::get(Type::Int256Ty, {Type::Int8PtrTy, Type::Int32Ty}, false);
    Func_keccak256 = llvm::Function::Create(FT, llvm::Function::InternalLinkage, "solidity.keccak256", TheModule);
    Func_keccak256->addFnAttr(llvm::Attribute::NoUnwind);
    // llvm::Argument *Memory = Func_keccak256->arg_begin();
    // Memory->setName("memory");
    llvm::Argument *Ptr = Func_keccak256->arg_begin();
    Ptr->setName("ptr");
    llvm::Argument *Length = Ptr + 1;
    Length->setName("length");

    llvm::BasicBlock *Entry = llvm::BasicBlock::Create(VMContext, "sha3.entry", Func_keccak256);
    m_builder.SetInsertPoint(Entry);

    // Ptr = m_builder.CreateTrunc(Ptr, Type::Int32Ty);


    // llvm::Value *Length = m_builder.CreateTrunc(m_builder.CreateExtractValue(Memory, {0}), Int32Ty, "length");
    // llvm::Value *Ptr = m_builder.CreateExtractValue(Memory, {1}, "ptr");

    llvm::Value *AddressPtr = m_builder.CreateAlloca(Type::AddressTy, nullptr, "address.ptr");
    llvm::APInt Address = llvm::APInt(160, 9).byteSwap();
    m_builder.CreateStore(m_builder.getInt(Address), AddressPtr);

    llvm::Value *Fee = m_builder.CreateCall(Func_getGasLeft, {});
    m_builder.CreateCall(Func_callStatic, {Fee, AddressPtr, Ptr, Length});

    // get result
    llvm::Value *ResultPtr = m_builder.CreateAlloca(Type::Int256Ty, nullptr, "result.ptr");
    m_builder.CreateCall(Func_returnDataCopy, {m_builder.CreateBitCast(ResultPtr, Type::Int8PtrTy), m_builder.getInt32(0), m_builder.getInt32(32)});
    llvm::Value *Result = m_builder.CreateLoad(Type::Int256Ty, ResultPtr);

    m_builder.CreateRet(Result);
}


void EEIModule::initRuntime() {
    // auto checkBB = llvm::BasicBlock::Create(m_loadTxCtxFn->getContext(), "Check", m_loadTxCtxFn);
	// auto loadBB = llvm::BasicBlock::Create(m_loadTxCtxFn->getContext(), "Load", m_loadTxCtxFn);
	// auto exitBB = llvm::BasicBlock::Create(m_loadTxCtxFn->getContext(), "Exit", m_loadTxCtxFn);
}

llvm::Value *EEIModule::prepareData() {
    llvm::Value *CallDataSize = m_builder.CreateCall(Func_getCallDataSize, {});
    llvm::Value *ValPtr = m_builder.CreateAlloca(Type::Int8Ty, CallDataSize);
    
    m_builder.CreateCall(Func_callDataCopy, {ValPtr, m_builder.getInt32(0), CallDataSize});

    llvm::Value *Bytes = llvm::ConstantAggregateZero::get(Type::BytesTy);
    Bytes = m_builder.CreateInsertValue(Bytes, m_builder.CreateZExt(CallDataSize, Type::Int256Ty), {0});
    Bytes = m_builder.CreateInsertValue(Bytes, ValPtr, {1});
}

void EEIModule::registerReturnData(llvm::Value* _offset, llvm::Value* _size) {
    m_builder.CreateCall(Func_finish, {_offset, _size});
}

}
}
}
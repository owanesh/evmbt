#pragma once

#include "preprocessor/llvm_includes_start.h"
#include <llvm/IR/Type.h>
#include <llvm/IR/Constants.h>
#include <llvm/IR/Metadata.h>
#include "preprocessor/llvm_includes_end.h"

// #include "BinaryTrans.h"


namespace dev
{
namespace eth
{
namespace trans
{

// struct StkExecInfoType
// {
// 	uint32_t in;
// 	uint32_t out;
// };

struct Type
{
	static llvm::IntegerType* Word;
	static llvm::PointerType* WordPtr;

	static llvm::IntegerType* Bool;
	static llvm::IntegerType* Size;
	static llvm::IntegerType* Gas;
	static llvm::PointerType* GasPtr;

	static llvm::IntegerType* Byte;
	static llvm::PointerType* BytePtr;

	static llvm::Type* Void;

	/// Main function return type
	static llvm::IntegerType* MainReturn;

	static llvm::PointerType* EnvPtr;
	static llvm::PointerType* RuntimeDataPtr;
	static llvm::PointerType* RuntimePtr;

	// TODO: Redesign static LLVM objects
	static llvm::MDNode* expectTrue;

	// WASM Types
	/// i8, i16, i32, i64, i128, i256
	static llvm::IntegerType *Int8Ty, *Int32Ty, *Int64Ty, *Int128Ty, *Int256Ty;
	/// i160
	static llvm::IntegerType *AddressTy, *Int160Ty;

	/// i8*, i32*, i64*, i128*, i256*
	static llvm::PointerType *Int8PtrTy, *Int32PtrTy, *Int64PtrTy, *Int128PtrTy,
		*Int256PtrTy;
	/// i160*
	static llvm::PointerType *AddressPtrTy, *Int160PtrTy;

	/// args types
	static llvm::IntegerType *ArgsElemTy;
	static llvm::PointerType *ArgsElemPtrTy;
	/// return types
	static llvm::IntegerType *ReturnElemTy;
	static llvm::PointerType *ReturnElemPtrTy;
	/// bytes
	static llvm::StructType *BytesTy;
	static llvm::IntegerType *BytesElemTy;
	static llvm::PointerType *BytesElemPtrTy;
	/// String
	static llvm::StructType *StringTy;

	static void init(llvm::LLVMContext& _context);
};

struct Constant
{
	static llvm::ConstantInt* gasMax;

	/// Returns word-size constant
	static llvm::ConstantInt* get(int64_t _n);
	static llvm::ConstantInt* get(llvm::APInt const& _n);

};

}
}
}


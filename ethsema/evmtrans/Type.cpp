#include "Type.h"

#include <llvm/IR/MDBuilder.h>

namespace dev
{
namespace eth
{
namespace trans
{



llvm::IntegerType* Type::Word;
llvm::PointerType* Type::WordPtr;
llvm::IntegerType* Type::Bool;
llvm::IntegerType* Type::Size;
llvm::IntegerType* Type::Gas;
llvm::PointerType* Type::GasPtr;
llvm::IntegerType* Type::Byte;
llvm::PointerType* Type::BytePtr;
llvm::Type* Type::Void;
llvm::IntegerType* Type::MainReturn;
llvm::PointerType* Type::EnvPtr;
llvm::PointerType* Type::RuntimeDataPtr;
llvm::PointerType* Type::RuntimePtr;
llvm::ConstantInt* Constant::gasMax;
llvm::MDNode* Type::expectTrue;
llvm::IntegerType *Type::Int8Ty, *Type::Int32Ty, *Type::Int64Ty, *Type::Int128Ty, *Type::Int256Ty;
llvm::IntegerType *Type::AddressTy, *Type::Int160Ty;
llvm::PointerType *Type::Int8PtrTy, *Type::Int32PtrTy, *Type::Int64PtrTy, *Type::Int128PtrTy, *Type::Int256PtrTy;
llvm::PointerType *Type::AddressPtrTy, *Type::Int160PtrTy;
llvm::IntegerType *Type::ArgsElemTy;
llvm::PointerType *Type::ArgsElemPtrTy;
llvm::IntegerType *Type::ReturnElemTy;
llvm::PointerType *Type::ReturnElemPtrTy;
llvm::StructType *Type::BytesTy;
llvm::IntegerType *Type::BytesElemTy;
llvm::PointerType *Type::BytesElemPtrTy;
llvm::StructType *Type::StringTy;

void Type::init(llvm::LLVMContext& _context)
{
	if (!Word)	// Do init only once
	{
		Word = llvm::Type::getIntNTy(_context, 256);
		WordPtr = Word->getPointerTo();
		Bool = llvm::Type::getInt1Ty(_context);
		Size = llvm::Type::getInt64Ty(_context);
		Gas = Size;
		GasPtr = Gas->getPointerTo();
		Byte = llvm::Type::getInt8Ty(_context);
		BytePtr = Byte->getPointerTo();
		Void = llvm::Type::getVoidTy(_context);
		MainReturn = llvm::Type::getInt32Ty(_context);

		Constant::gasMax = llvm::ConstantInt::getSigned(Type::Gas, std::numeric_limits<int64_t>::max());

		expectTrue = llvm::MDBuilder{_context}.createBranchWeights(1, 0);

		Int8Ty = llvm::Type::getInt8Ty(_context);
		Int32Ty = llvm::Type::getInt32Ty(_context);
		Int64Ty = llvm::Type::getInt64Ty(_context);
		Int128Ty = llvm::Type::getInt128Ty(_context);
		AddressTy = llvm::Type::getIntNTy(_context, 160);
		Int160Ty = AddressTy;
		Int256Ty = llvm::Type::getIntNTy(_context, 256);

		Int8PtrTy = llvm::Type::getInt8PtrTy(_context);
		Int32PtrTy = llvm::Type::getInt32PtrTy(_context);
		Int64PtrTy = llvm::Type::getInt64PtrTy(_context);
		Int128PtrTy = llvm::Type::getIntNPtrTy(_context, 128);
		AddressPtrTy = llvm::Type::getIntNPtrTy(_context, 160);
		Int160PtrTy = AddressPtrTy;
		Int256PtrTy = llvm::Type::getIntNPtrTy(_context, 256);

		ArgsElemTy = Int8Ty;
		ArgsElemPtrTy = Int8PtrTy;
		BytesElemTy = Int8Ty;
		BytesElemPtrTy = Int8PtrTy;
		ReturnElemTy = Int8Ty;
		ReturnElemPtrTy = Int8PtrTy;

		BytesTy = llvm::StructType::create(_context, {Int256Ty, BytesElemPtrTy}, "bytes");
  		StringTy = llvm::StructType::create(_context, {Int256Ty, BytesElemPtrTy}, "string", false);
		
	}
}

llvm::ConstantInt* Constant::get(int64_t _n)
{
	return llvm::ConstantInt::getSigned(Type::Word, _n);
}

llvm::ConstantInt* Constant::get(llvm::APInt const& _n)
{
	return llvm::ConstantInt::get(Type::Word->getContext(), _n);
}

}
}
}


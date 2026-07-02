#include "Arith256.h"

#include <iostream>
#include <iomanip>

#include "preprocessor/llvm_includes_start.h"
#include <llvm/IR/Module.h>
#include <llvm/IR/IntrinsicInst.h>
#include "preprocessor/llvm_includes_end.h"

#include "Type.h"
// #include "Endianness.h"
// #include "Utils.h"

namespace dev
{
namespace eth
{
namespace trans
{

Arith256::Arith256(IRBuilder& _builder, llvm::Module *_module) :
	m_builder(_builder),
	m_module(_module)
{
	getExpFunc();
}

void Arith256::debug(llvm::Value* _value, char _c, llvm::Module& _module, IRBuilder& _builder)
{
	static const auto funcName = "debug";
	auto func = _module.getFunction(funcName);
	if (!func)
		func = llvm::Function::Create(llvm::FunctionType::get(Type::Void, {Type::Word, _builder.getInt8Ty()}, false), llvm::Function::ExternalLinkage, funcName, &_module);

	_builder.CreateCall(func, {_builder.CreateZExtOrTrunc(_value, Type::Word), _builder.getInt8(_c)});
}

namespace
{
llvm::Function* createCTLZFunc(llvm::Type* _type, llvm::Module& _module) {
	unsigned bit = _type == Type::Word ? 256 : 512;
	std::string bitStr = _type == Type::Word ? "256" : "512";

	auto func = llvm::Function::Create(llvm::FunctionType::get(_type, {_type}, false), llvm::Function::InternalLinkage, "wasm.ctlz.i"+bitStr, &_module);
	func->setDoesNotThrow();

	auto zero = llvm::ConstantInt::get(_type, 0);
	auto one = llvm::ConstantInt::get(_type, 1);

	llvm::Argument* x = &(*func->arg_begin());
	x->setName("x");

	//  tmp = x;
	//  cnt = 0;
	//	while (tmp != 0) {
	//		tmp = tmp >> 1; (lshr)
	//      cnt ++;
	//	}
	//  tlz = bit - cnt;

	auto entryBB = llvm::BasicBlock::Create(_module.getContext(), "ctlz.Entry", func);
	auto headerBB = llvm::BasicBlock::Create(_module.getContext(), "ctlz.LoopHeader", func);
	auto bodyBB = llvm::BasicBlock::Create(_module.getContext(), "ctlz.LoopBody", func);
	auto returnBB = llvm::BasicBlock::Create(_module.getContext(), "ctlz.Return", func);

	auto builder = IRBuilder{entryBB};
	builder.SetInsertPoint(entryBB);
	builder.CreateBr(headerBB);

	builder.SetInsertPoint(headerBB);
	auto t = builder.CreatePHI(_type, 2, "tmp");
	auto c = builder.CreatePHI(_type, 2, "cnt");
	auto tNonZero = builder.CreateICmpNE(t, zero, "tmp.nonzero");
	builder.CreateCondBr(tNonZero, bodyBB, returnBB);

	builder.SetInsertPoint(bodyBB);
	auto t1 = builder.CreateLShr(t, one, "t1");
	auto c1 = builder.CreateAdd(c, one, "c1");
	builder.CreateBr(headerBB);

	t->addIncoming(x, entryBB);
	t->addIncoming(t1, bodyBB);
	c->addIncoming(zero, entryBB);
	c->addIncoming(c1, bodyBB);

	builder.SetInsertPoint(returnBB);
	auto r = builder.CreateSub(llvm::ConstantInt::get(_type, bit), c);
	builder.CreateRet(r);

	return func;
}

llvm::Function* createUDivRemFunc(llvm::Type* _type, llvm::Module& _module, char const* _funcName)
{
	// Based of "Improved shift divisor algorithm" from "Software Integer Division" by Microsoft Research
	// The following algorithm also handles divisor of value 0 returning 0 for both quotient and remainder

	auto retType = llvm::VectorType::get(_type, 2);
	auto func = llvm::Function::Create(llvm::FunctionType::get(retType, {_type, _type}, false), llvm::Function::InternalLinkage, _funcName, &_module);
	func->setDoesNotThrow();
	func->setDoesNotAccessMemory();

	auto zero = llvm::ConstantInt::get(_type, 0);
	auto one = llvm::ConstantInt::get(_type, 1);

	auto iter = func->arg_begin();
	llvm::Argument* x = &(*iter++);
	x->setName("x");
	llvm::Argument* y = &(*iter);
	y->setName("y");

	auto entryBB = llvm::BasicBlock::Create(_module.getContext(), "Entry", func);
	auto mainBB = llvm::BasicBlock::Create(_module.getContext(), "Main", func);
	auto beforeloopYBB = llvm::BasicBlock::Create(_module.getContext(), "beforeloopY", func);
	auto loopYBB = llvm::BasicBlock::Create(_module.getContext(), "LoopY", func);
	auto loopBB = llvm::BasicBlock::Create(_module.getContext(), "Loop", func);
	auto continueBB = llvm::BasicBlock::Create(_module.getContext(), "Continue", func);
	auto returnBB = llvm::BasicBlock::Create(_module.getContext(), "Return", func);

	auto builder = IRBuilder{entryBB};
	auto yLEx = builder.CreateICmpULE(y, x);
	auto r0 = x;
	builder.CreateCondBr(yLEx, mainBB, returnBB);

	builder.SetInsertPoint(mainBB);
	// auto ctlzIntr = llvm::Intrinsic::getDeclaration(&_module, llvm::Intrinsic::ctlz, _type);
	auto ctlzIntr = createCTLZFunc(_type, _module);
	// both y and r are non-zero
	// auto yLz = builder.CreateCall(ctlzIntr, {y, builder.getInt1(true)}, "y.lz");
	// auto rLz = builder.CreateCall(ctlzIntr, {r0, builder.getInt1(true)}, "r.lz");
	auto yLz = builder.CreateCall(ctlzIntr, {y}, "y.lz");
	auto rLz = builder.CreateCall(ctlzIntr, {r0}, "r.lz");
	auto i0 = builder.CreateNUWSub(yLz, rLz, "i0");
	// auto y0 = builder.CreateShl(y, i0);
	// builder.CreateBr(loopBB);
	builder.CreateBr(beforeloopYBB);

	builder.SetInsertPoint(beforeloopYBB);
	auto i0Phi = builder.CreatePHI(_type, 2, "i0.phi");
	auto y0 = builder.CreatePHI(_type, 2, "y0");
	auto i0NonZero = builder.CreateICmpNE(i0Phi, zero, "i0.nonzero");
	builder.CreateCondBr(i0NonZero, loopYBB, loopBB);

	builder.SetInsertPoint(loopYBB);
	auto y1 = builder.CreateShl(y0, one);
	auto i1 = builder.CreateSub(i0Phi, one);
	builder.CreateBr(beforeloopYBB);

	y0->addIncoming(y, mainBB);
	y0->addIncoming(y1, loopYBB);
	i0Phi->addIncoming(i0, mainBB);
	i0Phi->addIncoming(i1, loopYBB);

	builder.SetInsertPoint(loopBB);
	auto yPhi = builder.CreatePHI(_type, 2, "y.phi");
	auto rPhi = builder.CreatePHI(_type, 2, "r.phi");
	auto iPhi = builder.CreatePHI(_type, 2, "i.phi");
	auto qPhi = builder.CreatePHI(_type, 2, "q.phi");
	auto rUpdate = builder.CreateNUWSub(rPhi, yPhi);
	auto qUpdate = builder.CreateOr(qPhi, one);	// q += 1, q lowest bit is 0
	auto rGEy = builder.CreateICmpUGE(rPhi, yPhi);
	auto r1 = builder.CreateSelect(rGEy, rUpdate, rPhi, "r1");
	auto q1 = builder.CreateSelect(rGEy, qUpdate, qPhi, "q");
	auto iZero = builder.CreateICmpEQ(iPhi, zero);
	builder.CreateCondBr(iZero, returnBB, continueBB);

	builder.SetInsertPoint(continueBB);
	auto i2 = builder.CreateNUWSub(iPhi, one);
	auto q2 = builder.CreateShl(q1, one);
	auto y2 = builder.CreateLShr(yPhi, one);
	builder.CreateBr(loopBB);

	yPhi->addIncoming(y0, beforeloopYBB);
	yPhi->addIncoming(y2, continueBB);
	rPhi->addIncoming(r0, beforeloopYBB);
	rPhi->addIncoming(r1, continueBB);
	iPhi->addIncoming(i0, beforeloopYBB);
	iPhi->addIncoming(i2, continueBB);
	qPhi->addIncoming(zero, beforeloopYBB);
	qPhi->addIncoming(q2, continueBB);

	builder.SetInsertPoint(returnBB);
	auto qRet = builder.CreatePHI(_type, 2, "q.ret");
	qRet->addIncoming(zero, entryBB);
	qRet->addIncoming(q1, loopBB);
	auto rRet = builder.CreatePHI(_type, 2, "r.ret");
	rRet->addIncoming(r0, entryBB);
	rRet->addIncoming(r1, loopBB);
	auto ret = builder.CreateInsertElement(llvm::UndefValue::get(retType), qRet, uint64_t(0), "ret0");
	ret = builder.CreateInsertElement(ret, rRet, 1, "ret");
	builder.CreateRet(ret);

	return func;
}
}

llvm::Function* Arith256::getUDivRem256Func(llvm::Module& _module)
{
	static const auto funcName = "evm.udivrem.i256";
	if (auto func = _module.getFunction(funcName))
		return func;

	return createUDivRemFunc(Type::Word, _module, funcName);
}

llvm::Function* Arith256::getUDivRem512Func(llvm::Module& _module)
{
	static const auto funcName = "evm.udivrem.i512";
	if (auto func = _module.getFunction(funcName))
		return func;

	return createUDivRemFunc(llvm::IntegerType::get(_module.getContext(), 512), _module, funcName);
}

llvm::Function* Arith256::getUDiv256Func(llvm::Module& _module)
{
	static const auto funcName = "evm.udiv.i256";
	if (auto func = _module.getFunction(funcName))
		return func;

	auto udivremFunc = getUDivRem256Func(_module);

	auto func = llvm::Function::Create(llvm::FunctionType::get(Type::Word, {Type::Word, Type::Word}, false), llvm::Function::InternalLinkage, funcName, &_module);
	func->setDoesNotThrow();
	func->setDoesNotAccessMemory();

	auto iter = func->arg_begin();
	llvm::Argument* x = &(*iter++);
	x->setName("x");
	llvm::Argument* y = &(*iter);
	y->setName("y");

	auto bb = llvm::BasicBlock::Create(_module.getContext(), {}, func);
	auto builder = IRBuilder{bb};
	auto udivrem = builder.CreateCall(udivremFunc, {x, y});
	auto udiv = builder.CreateExtractElement(udivrem, uint64_t(0));
	builder.CreateRet(udiv);

	return func;
}

llvm::Function* Arith256::getMulFunc(llvm::Module& _module, uint64_t BitWidth, const std::string funcName) {
	
	llvm::Type *IntHiTy = BitWidth == 256 ? Type::Word : Type::Int128Ty;
  	llvm::Type *IntLoTy = BitWidth == 256 ? Type::Int128Ty : Type::Int64Ty;

	auto func = llvm::Function::Create(llvm::FunctionType::get(IntHiTy, {IntHiTy, IntHiTy}, false), llvm::Function::InternalLinkage, funcName, &_module);
	func->setDoesNotThrow();
	func->setDoesNotAccessMemory();

	llvm::Argument *LHS = func->arg_begin();
	LHS->setName("lhs");
	llvm::Argument *RHS = LHS + 1;
	RHS->setName("rhs");

	auto entryBB = llvm::BasicBlock::Create(_module.getContext(), "mul.Entry", func);
	auto Builder = IRBuilder{entryBB};

	const uint64_t Shift = BitWidth / 2;
	const llvm::APInt HalfMask = llvm::APInt::getLowBitsSet(BitWidth / 2, BitWidth / 4);
	const uint64_t HalfShift = BitWidth / 4;

	llvm::Value *LHS_L = Builder.CreateTrunc(LHS, IntLoTy, "lhs_l");
	llvm::Value *LHS_H =
		Builder.CreateTrunc(Builder.CreateLShr(LHS, Shift), IntLoTy, "lhs_h");
	llvm::Value *RHS_L = Builder.CreateTrunc(RHS, IntLoTy, "rhs_l");
	llvm::Value *RHS_H =
		Builder.CreateTrunc(Builder.CreateLShr(RHS, Shift), IntLoTy, "rhs_h");

	llvm::Value *LHS_LL = Builder.CreateAnd(LHS_L, HalfMask, "lhs_ll");
	llvm::Value *RHS_LL = Builder.CreateAnd(RHS_L, HalfMask, "rhs_ll");
	llvm::Value *T = Builder.CreateMul(LHS_LL, RHS_LL, "t");

	llvm::Value *T_L = Builder.CreateAnd(T, HalfMask, "t_l");
	llvm::Value *T_H = Builder.CreateLShr(T, HalfShift, "t_h");

	llvm::Value *LHS_LH = Builder.CreateLShr(LHS_L, HalfShift, "lhs_lh");
	llvm::Value *RHS_LH = Builder.CreateLShr(RHS_L, HalfShift, "rhs_lh");

	llvm::Value *U =
		Builder.CreateAdd(Builder.CreateMul(LHS_LH, RHS_LL), T_H, "u");
	llvm::Value *U_L = Builder.CreateAnd(U, HalfMask, "u_l");
	llvm::Value *U_H = Builder.CreateLShr(U, HalfShift, "u_h");

	llvm::Value *V =
		Builder.CreateAdd(Builder.CreateMul(LHS_LL, RHS_LH), U_L, "v");
	llvm::Value *V_H = Builder.CreateLShr(V, HalfShift, "v_h");

	llvm::Value *W = Builder.CreateAdd(Builder.CreateMul(LHS_LH, RHS_LH),
										Builder.CreateAdd(U_H, V_H), "w");

	llvm::Value *O_L = Builder.CreateZExt(
		Builder.CreateAdd(T_L, Builder.CreateShl(V, HalfShift)), IntHiTy, "o_l");
	llvm::Value *O_H = Builder.CreateZExt(
		Builder.CreateAdd(W, Builder.CreateAdd(Builder.CreateMul(RHS_H, LHS_L),
												Builder.CreateMul(RHS_L, LHS_H))),
		IntHiTy, "o_h");

	llvm::Value *O = Builder.CreateAdd(O_L, Builder.CreateShl(O_H, Shift), "o");

	Builder.CreateRet(O);

	return func;
}

llvm::Function* Arith256::getMul256Func(llvm::Module& _module)
{
	static const auto funcName = "evm.mul.i256";
	if (auto func = _module.getFunction(funcName))
		return func;
	return getMulFunc(_module, 256, funcName);
}

llvm::Function* Arith256::getMul128Func(llvm::Module& _module)
{
	static const auto funcName = "evm.mul.i128";
	if (auto func = _module.getFunction(funcName))
		return func;
	return getMulFunc(_module, 128, funcName);
}

namespace
{
llvm::Function* createURemFunc(llvm::Type* _type, llvm::Module& _module, char const* _funcName)
{
	auto udivremFunc = _type == Type::Word ? Arith256::getUDivRem256Func(_module) : Arith256::getUDivRem512Func(_module);
	// auto udivremFunc = Arith256::getUDivRem256Func(_module);
	auto func = llvm::Function::Create(llvm::FunctionType::get(_type, {_type, _type}, false), llvm::Function::InternalLinkage, _funcName, &_module);
	func->setDoesNotThrow();
	func->setDoesNotAccessMemory();

	auto iter = func->arg_begin();
	llvm::Argument* x = &(*iter++);
	x->setName("x");
	llvm::Argument* y = &(*iter);
	y->setName("y");

	auto bb = llvm::BasicBlock::Create(_module.getContext(), {}, func);
	auto builder = IRBuilder{bb};
	auto udivrem = builder.CreateCall(udivremFunc, {x, y});
	auto r = builder.CreateExtractElement(udivrem, uint64_t(1));
	builder.CreateRet(r);

	return func;
}
}

llvm::Function* Arith256::getURem256Func(llvm::Module& _module)
{
	static const auto funcName = "evm.urem.i256";
	if (auto func = _module.getFunction(funcName))
		return func;
	return createURemFunc(Type::Word, _module, funcName);
}

llvm::Function* Arith256::getURem512Func(llvm::Module& _module)
{
	static const auto funcName = "evm.urem.i512";
	if (auto func = _module.getFunction(funcName))
		return func;
	return createURemFunc(llvm::IntegerType::get(_module.getContext(), 512), _module, funcName);
}

llvm::Function* Arith256::getSDivRem256Func(llvm::Module& _module)
{
	static const auto funcName = "evm.sdivrem.i256";
	if (auto func = _module.getFunction(funcName))
		return func;

	auto udivremFunc = getUDivRem256Func(_module);

	auto retType = llvm::VectorType::get(Type::Word, 2);
	auto func = llvm::Function::Create(llvm::FunctionType::get(retType, {Type::Word, Type::Word}, false), llvm::Function::InternalLinkage, funcName, &_module);
	func->setDoesNotThrow();
	func->setDoesNotAccessMemory();

	auto iter = func->arg_begin();
	llvm::Argument* x = &(*iter++);
	x->setName("x");
	llvm::Argument* y = &(*iter);
	y->setName("y");

	auto bb = llvm::BasicBlock::Create(_module.getContext(), "", func);
	auto builder = IRBuilder{bb};
	auto xIsNeg = builder.CreateICmpSLT(x, Constant::get(0));
	auto xNeg = builder.CreateSub(Constant::get(0), x);
	auto xAbs = builder.CreateSelect(xIsNeg, xNeg, x);

	auto yIsNeg = builder.CreateICmpSLT(y, Constant::get(0));
	auto yNeg = builder.CreateSub(Constant::get(0), y);
	auto yAbs = builder.CreateSelect(yIsNeg, yNeg, y);

	auto res = builder.CreateCall(udivremFunc, {xAbs, yAbs});
	auto qAbs = builder.CreateExtractElement(res, uint64_t(0));
	auto rAbs = builder.CreateExtractElement(res, 1);

	// the remainder has the same sign as dividend
	auto rNeg = builder.CreateSub(Constant::get(0), rAbs);
	auto r = builder.CreateSelect(xIsNeg, rNeg, rAbs);

	auto qNeg = builder.CreateSub(Constant::get(0), qAbs);
	auto xyOpposite = builder.CreateXor(xIsNeg, yIsNeg);
	auto q = builder.CreateSelect(xyOpposite, qNeg, qAbs);

	auto ret = builder.CreateInsertElement(llvm::UndefValue::get(retType), q, uint64_t(0));
	ret = builder.CreateInsertElement(ret, r, 1);
	builder.CreateRet(ret);

	return func;
}

llvm::Function* Arith256::getSDiv256Func(llvm::Module& _module)
{
	static const auto funcName = "evm.sdiv.i256";
	if (auto func = _module.getFunction(funcName))
		return func;

	auto sdivremFunc = getSDivRem256Func(_module);

	auto func = llvm::Function::Create(llvm::FunctionType::get(Type::Word, {Type::Word, Type::Word}, false), llvm::Function::InternalLinkage, funcName, &_module);
	func->setDoesNotThrow();
	func->setDoesNotAccessMemory();

	auto iter = func->arg_begin();
	llvm::Argument* x = &(*iter++);
	x->setName("x");
	llvm::Argument* y = &(*iter);
	y->setName("y");

	auto bb = llvm::BasicBlock::Create(_module.getContext(), {}, func);
	auto builder = IRBuilder{bb};
	auto sdivrem = builder.CreateCall(sdivremFunc, {x, y});
	auto q = builder.CreateExtractElement(sdivrem, uint64_t(0));
	builder.CreateRet(q);

	return func;
}

llvm::Function* Arith256::getSRem256Func(llvm::Module& _module)
{
	static const auto funcName = "evm.srem.i256";
	if (auto func = _module.getFunction(funcName))
		return func;

	auto sdivremFunc = getSDivRem256Func(_module);

	auto func = llvm::Function::Create(llvm::FunctionType::get(Type::Word, {Type::Word, Type::Word}, false), llvm::Function::InternalLinkage, funcName, &_module);
	func->setDoesNotThrow();
	func->setDoesNotAccessMemory();

	auto iter = func->arg_begin();
	llvm::Argument* x = &(*iter++);
	x->setName("x");
	llvm::Argument* y = &(*iter);
	y->setName("y");

	auto bb = llvm::BasicBlock::Create(_module.getContext(), {}, func);
	auto builder = IRBuilder{bb};
	auto sdivrem = builder.CreateCall(sdivremFunc, {x, y});
	auto r = builder.CreateExtractElement(sdivrem, uint64_t(1));
	builder.CreateRet(r);

	return func;
}

llvm::Function* Arith256::getExpFunc()
{
	if (!m_exp)
	{
		llvm::Type* argTypes[] = {Type::Word, Type::Word};
		m_exp = llvm::Function::Create(llvm::FunctionType::get(Type::Word, argTypes, false), llvm::Function::InternalLinkage, "evm.exp", m_module);
		m_exp->setDoesNotThrow();
		m_exp->setDoesNotAccessMemory();

		auto iter = m_exp->arg_begin();
		llvm::Argument* base = &(*iter++);
		base->setName("base");
		llvm::Argument* exponent = &(*iter);
		exponent->setName("exponent");

		InsertPointGuard guard{m_builder};

		//	while (e != 0) {
		//		if (e % 2 == 1)
		//			r *= b;
		//		b *= b;
		//		e /= 2;
		//	}

		auto entryBB = llvm::BasicBlock::Create(m_builder.getContext(), "exp.Entry", m_exp);
		auto headerBB = llvm::BasicBlock::Create(m_builder.getContext(), "exp.LoopHeader", m_exp);
		auto bodyBB = llvm::BasicBlock::Create(m_builder.getContext(), "exp.LoopBody", m_exp);
		auto updateBB = llvm::BasicBlock::Create(m_builder.getContext(), "exp.ResultUpdate", m_exp);
		auto continueBB = llvm::BasicBlock::Create(m_builder.getContext(), "exp.Continue", m_exp);
		auto returnBB = llvm::BasicBlock::Create(m_builder.getContext(), "exp.Return", m_exp);

		m_builder.SetInsertPoint(entryBB);
		m_builder.CreateBr(headerBB);

		m_builder.SetInsertPoint(headerBB);
		auto r = m_builder.CreatePHI(Type::Word, 2, "r");
		auto b = m_builder.CreatePHI(Type::Word, 2, "b");
		auto e = m_builder.CreatePHI(Type::Word, 2, "e");
		auto eNonZero = m_builder.CreateICmpNE(e, Constant::get(0), "e.nonzero");
		m_builder.CreateCondBr(eNonZero, bodyBB, returnBB);

		m_builder.SetInsertPoint(bodyBB);
		auto eOdd = m_builder.CreateICmpNE(m_builder.CreateAnd(e, Constant::get(1)), Constant::get(0), "e.isodd");
		m_builder.CreateCondBr(eOdd, updateBB, continueBB);

		m_builder.SetInsertPoint(updateBB);
		auto r0 = m_builder.CreateMul(r, b);
		m_builder.CreateBr(continueBB);

		m_builder.SetInsertPoint(continueBB);
		auto r1 = m_builder.CreatePHI(Type::Word, 2, "r1");
		r1->addIncoming(r, bodyBB);
		r1->addIncoming(r0, updateBB);
		auto b1 = m_builder.CreateMul(b, b);
		auto e1 = m_builder.CreateLShr(e, Constant::get(1), "e1");
		m_builder.CreateBr(headerBB);

		r->addIncoming(Constant::get(1), entryBB);
		r->addIncoming(r1, continueBB);
		b->addIncoming(base, entryBB);
		b->addIncoming(b1, continueBB);
		e->addIncoming(exponent, entryBB);
		e->addIncoming(e1, continueBB);

		m_builder.SetInsertPoint(returnBB);
		m_builder.CreateRet(r);
	}
	return m_exp;
}

llvm::Value* Arith256::exp(llvm::Value* _arg1, llvm::Value* _arg2)
{
	//	while (e != 0) {
	//		if (e % 2 == 1)
	//			r *= b;
	//		b *= b;
	//		e /= 2;
	//	}

	if (auto c1 = llvm::dyn_cast<llvm::ConstantInt>(_arg1))
	{
		if (auto c2 = llvm::dyn_cast<llvm::ConstantInt>(_arg2))
		{
			auto b = c1->getValue();
			auto e = c2->getValue();
			auto r = llvm::APInt{256, 1};
			while (e != 0)
			{
				if (e[0])
					r *= b;
				b *= b;
				e = e.lshr(1);
			}
			return Constant::get(r);
		}
	}

	return m_builder.CreateCall(getExpFunc(), {_arg1, _arg2});
}

llvm::Function* Arith256::getSafeMathAddFunc(llvm::Module& _module)
{
	static const auto funcName = "safeAdd";
	if (auto func = _module.getFunction(funcName))
		return func;
	/* @ Solidity
	  func():
	  	c = a + b;
        require(c >= a);
		return c;
	*/
	auto func = llvm::Function::Create(llvm::FunctionType::get(Type::Int256Ty, {Type::Int256Ty, Type::Int256Ty}, false), 
										llvm::Function::InternalLinkage, funcName, &_module);
	func->setDoesNotThrow();
	func->setDoesNotAccessMemory();

	llvm::Argument *LHS = func->arg_begin();
	LHS->setName("lhs");
	llvm::Argument *RHS = LHS + 1;
	RHS->setName("rhs");

	auto entryBB = llvm::BasicBlock::Create(_module.getContext(), "safeAdd.entry", func);
	auto unreachableBB = llvm::BasicBlock::Create(_module.getContext(), "safeAdd.unreachable", func);
	auto passBB = llvm::BasicBlock::Create(_module.getContext(), "safeAdd.pass", func);
	
	auto Builder = IRBuilder{entryBB};
	auto RES = Builder.CreateAdd(LHS, RHS);
	Builder.CreateCondBr(Builder.CreateICmpULT(RES, LHS, "RES < LHS"), unreachableBB, passBB); 

	Builder.SetInsertPoint(unreachableBB);
	Builder.CreateCall(
		_module.getFunction("ethereum.revert"), 
		{Builder.CreateAlloca(Type::Int8Ty, nullptr), Builder.getInt32(0)});
	// f_builder.CreateUnreachable();
	Builder.CreateRet(Constant::get(0));
	/*
	%.entry:
		c = a + b
		br (c < a), unreachableBB, passBB
	%.passBB:
		return c
	%.unreachableBB:
		unreachable
	*/
	Builder.SetInsertPoint(passBB);
	Builder.CreateRet(RES);
	return func;
}

llvm::Function* Arith256::getSafeMathSubFunc(llvm::Module& _module)
{
	static const auto funcName = "safeSub";
	if (auto func = _module.getFunction(funcName))
		return func;
	/* @ Solidity
	  func():
		if (a == 0) return 0;
		uint256 c = a * b;
		if (c / a != b) return 0;
		return (true, c);
	*/
	auto func = llvm::Function::Create(llvm::FunctionType::get(Type::Int256Ty, {Type::Int256Ty, Type::Int256Ty}, false), 
										llvm::Function::InternalLinkage, funcName, &_module);
	func->setDoesNotThrow();
	func->setDoesNotAccessMemory();

	llvm::Argument *LHS = func->arg_begin();
	LHS->setName("lhs");
	llvm::Argument *RHS = LHS + 1;
	RHS->setName("rhs");

	auto entryBB = llvm::BasicBlock::Create(_module.getContext(), "safeSub.entry", func);

	auto unreachableBB = llvm::BasicBlock::Create(_module.getContext(), "safeSub.unreachable", func);
	auto passBB = llvm::BasicBlock::Create(_module.getContext(), "safeSub.pass", func);

	// llvm::IRBuilder<>& f_builder;
	auto f_builder = IRBuilder{entryBB};
	// auto f_builder = llvm::IRBuilder<>;//llvm::IRBuilder<llvm::NoFolder>;
	f_builder.SetInsertPoint(entryBB);
	// f_builder.CreateRet(Constant::get(12));
	// auto destIdx = llvm::cast<llvm::ValueAsMetadata>(jump->getMetadata(c_destIdxLabel)->getOperand(0))->getValue();
	f_builder.CreateCondBr(f_builder.CreateICmpUGT(RHS, LHS, "LHS < RHS"), unreachableBB, passBB); 

	/*
	%.entry:
		br (a < b), unreachableBB, passBB
	%.passBB:
		return a - b
	%.unreachableBB:
		revert
	*/
	f_builder.SetInsertPoint(passBB);
	f_builder.CreateRet(f_builder.CreateSub(LHS, RHS));

	f_builder.SetInsertPoint(unreachableBB);
	f_builder.CreateCall(
		_module.getFunction("ethereum.revert"),  
		{f_builder.CreateAlloca(Type::Int8Ty, nullptr), f_builder.getInt32(0)});
	// f_builder.CreateUnreachable();
	f_builder.CreateRet(Constant::get(0));

	return func;
}

llvm::Function* Arith256::getSafeMathMulFunc(llvm::Module& _module)
{
	static const auto funcName = "safeMul";
	if (auto func = _module.getFunction(funcName))
		return func;
	/* @ Solidity
	  func():
		if (a == 0) return 0;
		uint256 c = a * b;
		if (c / a != b) return 0;
		return (true, c);
	*/
	auto func = llvm::Function::Create(llvm::FunctionType::get(Type::Int256Ty, {Type::Int256Ty, Type::Int256Ty}, false), 
										llvm::Function::InternalLinkage, funcName, &_module);
	func->setDoesNotThrow();
	func->setDoesNotAccessMemory();

	llvm::Argument *LHS = func->arg_begin();
	LHS->setName("lhs");
	llvm::Argument *RHS = LHS + 1;
	RHS->setName("rhs");

	auto entryBB = llvm::BasicBlock::Create(_module.getContext(), "safeMul.entry", func);
	auto return0BB = llvm::BasicBlock::Create(_module.getContext(), "safeMul.return0", func);
	auto overflowBB = llvm::BasicBlock::Create(_module.getContext(), "safeMul.overflow", func);
	auto unreachableBB = llvm::BasicBlock::Create(_module.getContext(), "safeMul.unreachable", func);
	auto passBB = llvm::BasicBlock::Create(_module.getContext(), "safeMul.pass", func);
	
	auto Builder = IRBuilder{entryBB};
	Builder.CreateCondBr(Builder.CreateICmpEQ(LHS, Constant::get(0), "LHS == 0"), return0BB, overflowBB); 

	/*
	%.entry:
		br (a == 0 || b == 0), checkaBB, overflowBB
	%.checkaBB:
		return 0
	%.coverflowBB:
		br (c / a != b), unreachableBB, successBB
	*/
	Builder.SetInsertPoint(return0BB);
	Builder.CreateRet(Constant::get(0));
	
	Builder.SetInsertPoint(overflowBB);
	auto res = Builder.CreateMul(LHS, RHS);
	Builder.CreateCondBr(Builder.CreateICmpNE( Builder.CreateUDiv(res, LHS), RHS, "RES / LHS != RHS"), unreachableBB, passBB); 

	Builder.SetInsertPoint(unreachableBB);
	Builder.CreateCall(
		_module.getFunction("ethereum.revert"), 
		{Builder.CreateAlloca(Type::Int8Ty, nullptr), Builder.getInt32(0)});
	// f_builder.CreateUnreachable();
	Builder.CreateRet(Constant::get(0));

	Builder.SetInsertPoint(passBB);
	Builder.CreateRet(res);
	return func;
}

// llvm::Function* Arith256::etSafeMathSDivFunc(llvm::Module& _module)
// {
// }

llvm::Function* Arith256::getSafeMathUDivFunc(llvm::Module& _module)
{
	static const auto funcName = "safeUDiv";
	if (auto func = _module.getFunction(funcName))
		return func;
	/* @ Solidity
	  func():
		require(b!=0);
		return a/b;
	*/
	auto func = llvm::Function::Create(llvm::FunctionType::get(Type::Int256Ty, {Type::Int256Ty, Type::Int256Ty}, false), 
										llvm::Function::InternalLinkage, funcName, &_module);
	func->setDoesNotThrow();
	func->setDoesNotAccessMemory();

	llvm::Argument *LHS = func->arg_begin();
	LHS->setName("lhs");
	llvm::Argument *RHS = LHS + 1;
	RHS->setName("rhs");

	auto entryBB = llvm::BasicBlock::Create(_module.getContext(), "safeUDiv.entry", func);
	auto unreachableBB = llvm::BasicBlock::Create(_module.getContext(), "safeUDiv.unreachable", func);
	auto passBB = llvm::BasicBlock::Create(_module.getContext(), "safeUDiv.pass", func);
	
	auto Builder = IRBuilder{entryBB};
	Builder.CreateCondBr(Builder.CreateICmpEQ(RHS, Constant::get(0), "RHS == 0"), unreachableBB, passBB); 

	Builder.SetInsertPoint(unreachableBB);
	Builder.CreateCall(
		_module.getFunction("ethereum.revert"), 
		{Builder.CreateAlloca(Type::Int8Ty, nullptr), Builder.getInt32(0)});
	// f_builder.CreateUnreachable();
	Builder.CreateRet(Constant::get(0));

	/*
	%.entry:
		br (b == 0), unreachableBB, passBB
	%.passBB:
		return a / b
	%.unreachableBB:
		unreachable
	*/
	Builder.SetInsertPoint(passBB);
	Builder.CreateRet(Builder.CreateUDiv(LHS, RHS));
	return func;
}

}
}
}

// extern "C"
// {
// 	EXPORT void debug(uint64_t a, uint64_t b, uint64_t c, uint64_t d, char z)
// 	{
// 		DLOG(JIT) << "DEBUG " << std::dec << z << ": " //<< d << c << b << a
// 				<< " ["	<< std::hex << std::setfill('0') << std::setw(16) << d << std::setw(16) << c << std::setw(16) << b << std::setw(16) << a << "]\n";
// 	}
// }

#include "BasicBlock.h"

#include <iostream>

#include "preprocessor/llvm_includes_start.h"
#include <llvm/IR/CFG.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/IntrinsicInst.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/Support/raw_os_ostream.h>
#include "preprocessor/llvm_includes_end.h"

#include "Type.h"
#include "Instruction.h"
// #include "Utils.h"

namespace dev
{
namespace eth
{
namespace trans
{

BasicBlock::BasicBlock(instr_idx _firstInstrIdx, code_iterator _begin, code_iterator _end, llvm::Function* _mainFunc):
	m_firstInstrIdx{_firstInstrIdx},
	m_begin(_begin),
	m_end(_end),
	m_llvmBB(llvm::BasicBlock::Create(_mainFunc->getContext(), {".", std::to_string(_firstInstrIdx)}, _mainFunc))
{
	BasicBlock::ctxDep();
	// stk_in = res.in;
	// stk_out = res.out;
}
void BasicBlock::ctxDep()
{	
	uint32_t prev_s = 0;
	uint32_t curr_s = 0;
	for (code_iterator it = m_begin; it != m_end; it = skipPushDataAndGetNext(it, m_end))
	{
		uint8_t opcode = static_cast<uint8_t>(Instruction(*it));
		uint8_t _out = std::get<1>(InstrMap[opcode]); // stack out , v
		uint8_t _in  = std::get<2>(InstrMap[opcode]); // stack in , \mu
		
		if (curr_s >= _out)
		{
			curr_s -= _out;
		}
		else
		{
			prev_s += _out - curr_s;
			curr_s = 0;
		}
		curr_s += _in;
	}
	m_stk_in = prev_s;
	m_stk_out = curr_s;
	// StkExecInfoType res = {prev_s, curr_s};
	// return res;
}



MiniLocalStack::MiniLocalStack(IRBuilder& _builder):
	m_builder(_builder) {}

void MiniLocalStack::push(llvm::Value* _value)
{
	auto ptr = storeItem(_value);
	m_local.push_back(ptr);
	m_local.push_back(_value);
	m_maxSize = std::max(m_maxSize, size());
}

llvm::Value* MiniLocalStack::pop()
{
	auto item = get(0);
	assert(!m_local.empty() || !m_input.empty());

	if (m_local.size() > 0)
		m_local.pop_back();
	else
		++m_globalPops;

	m_minSize = std::min(m_minSize, size());
	return item;
}

/// Copies the _index-th element of the local stack and pushes it back on the top.
void MiniLocalStack::dup(size_t _index)
{
	auto val = get(_index);
	push(val);
}

/// Swaps the top element with the _index-th element of the local stack.
void MiniLocalStack::swap(size_t _index)
{
	assert(_index > 0); ///< _index must not be 0.
	auto val = get(_index);
	auto tos = get(0);
	set(_index, tos);
	set(0, val);
}

llvm::Value* MiniLocalStack::get(size_t _index)
{
	if(_index < m_local.size())
		return *(m_local.rbegin() + _index); // count from back

	auto idx = _index - m_local.size() + m_globalPops;
	if (idx >= m_input.size())
		m_input.resize(idx + 1);
	auto& item = m_input[idx];
	if (!item) {
		ssize_t globalIdx = -static_cast<ssize_t>(idx) - m_globalPops - 1;
		auto ptr = s_global[s_global.size() + globalIdx];
		item = m_builder.CreateLoad(Type::Word, ptr);
		m_minSize = std::min(m_minSize, globalIdx); 	// remember required stack size
	}
	return item;
}

void MiniLocalStack::set(size_t _index, llvm::Value* _word)
{
	if (_index < m_local.size())
	{
		*(m_local.rbegin() + _index) = _word;
		return;
	}

	auto idx = _index - m_local.size() + m_globalPops;
	assert(idx < m_input.size());
	m_input[idx] = _word;
}

llvm::Value* MiniLocalStack::storeItem(llvm::Value* item) {
	if (item->getType() != Type::Int256Ty)
		item = m_builder.CreateZExt(item, Type::Int256Ty);
	auto addr = m_builder.CreateAlloca(Type::Int256Ty, nullptr);
	m_builder.CreateStore(item, addr);
	return addr;
}

llvm::Value* MiniLocalStack::loadItem(llvm::Value* addr) {
	if (addr->getType() != Type::Int256PtrTy)
		addr = m_builder.CreateIntToPtr(addr, Type::Int256PtrTy);
	return m_builder.CreateLoad(Type::Int256Ty, addr);;
}

// void MiniLocalStack::finalize()
// {
// 	if (auto term = m_builder.GetInsertBlock()->getTerminator())
// 		m_builder.SetInsertPoint(term); // Insert before terminator

// 	auto inputIt = m_input.rbegin();
// 	auto localIt = m_local.begin();
// 	while (m_globalPops > 0)
// 		s_global.pop_back();
// 	for (auto globalIdx = -static_cast<ssize_t>(m_input.size()); globalIdx < size(); ++globalIdx)
// 	{
// 		llvm::Value* item = nullptr;
// 		if (globalIdx < -m_globalPops)
// 		{
// 			item = *inputIt++;	// update input items (might contain original value)
// 			if (!item)			// some items are skipped
// 				continue;

// 			ssize_t idx = -globalIdx - 1;
// 			s_global[s_global.size() + idx] = storeItem(item);
// 		}
// 		else {
// 			item = *localIt++;	// store new items
// 			s_global.push_back(storeItem(item));
// 		}

// 		// auto slot = m_builder.CreateConstGEP1_64(m_sp, globalIdx);
// 		// m_builder.CreateAlignedStore(item, slot, 16); // TODO: Handle malloc alignment. Also for 32-bit systems.
// 	}

// 	m_local.empty();
// 	m_input.empty();

// }


GlobalStack::GlobalStack(IRBuilder& _builder, llvm::Constant *_stk, llvm::Value* _spPtr):
	m_builder(_builder), stk(_stk), m_sp(_spPtr) {
	
	first_block = m_builder.GetInsertBlock();
	topIdx = 0;

	// auto now_point = m_builder.saveIP();
	// if (auto term = first_block->getTerminator())
		// m_builder.SetInsertPoint(term); // Insert before terminator

	// std::vector<llvm::Value*> indices(2, llvm::ConstantInt::get(Type::Int256Ty, 0));
	// llvm::ArrayRef<llvm::Value *> indicesRef(indices);
	// m_sp = llvm::cast<llvm::GlobalVariable>(_builder. getModule().getOrInsertGlobal("spPtr", Type::Int256Ty)); // init to zero

	// m_builder.CreateGEP(stk, indicesRef, "spPtr"); // address to sp

	// m_builder.CreateStore(llvm::ConstantInt::get(Type::Int256Ty, 0), m_sp); 

	// m_builder.restoreIP(now_point);
}


GlobalStack::GlobalStack(const GlobalStack &rhs):
	m_builder(rhs.m_builder) {
	// deep copy

	// std::vector<llvm::Value*> _local = rhs.m_local;
	// for (int i = 0; i < _local.size(); i++) {
	// 	llvm::Value _v = *_local[i];
	// 	m_local.push_back(&_v);
	// }
	// std::vector<llvm::Value*> m_local(rhs.m_local);

	// for (auto v: rhs.m_local)
	// 	m_local.push_back(v);

	// for (auto v: rhs.m_address)
	// 	m_address.push_back(v);
	
	stk = rhs.stk;
	m_sp = rhs.m_sp;
	first_block = rhs.first_block;

	m_minSize = rhs.m_minSize;
	m_maxSize = rhs.m_maxSize;
	topIdx 	  = rhs.topIdx;
	stackSize = rhs.stackSize;
}


llvm::Value* GlobalStack::getPtr(size_t idx){

	llvm::Value* index = m_builder.CreateLoad(m_sp);
	if (idx > 0)
	{
		index = m_builder.CreateSub(index, llvm::ConstantInt::get(Type::Int256Ty, idx));
	}

	std::vector<llvm::Value*> indices { llvm::ConstantInt::get(Type::Int256Ty, 0), index };
	llvm::ArrayRef<llvm::Value *> indicesRef(indices);

	// return *ptr = stk[sp]
	return m_builder.CreateGEP(stk, indicesRef); 
}	


void GlobalStack::push(llvm::Value* _value)
{	
	// stk[0] ++
	
	// m_builder.CreateStore(m_builder.CreateAdd(tsp, Constant::get(1)), m_sp); 


	// auto now_point = m_builder.saveIP();
	// if (auto term = first_block->getTerminator())
	// 	m_builder.SetInsertPoint(term); // Insert before terminator
	
	if (enableRegs)
	{
		m_local.push_back(_value);
	}
	else
	{
		auto tsp = m_builder.CreateLoad(m_sp);	
		m_builder.CreateStore(m_builder.CreateAdd(tsp, llvm::ConstantInt::get(Type::Int256Ty, 1)), m_sp); //sp++
		// m_sp = m_builder.CreateAdd(m_sp, llvm::ConstantInt::get(Type::Int256Ty, 1));
		
		if (_value->getType() != Type::Int256Ty)
		{	
			auto _value256 = m_builder.CreateZExt(_value, Type::Int256Ty);
			m_builder.CreateStore(_value256, getPtr(0)); // get stk[sp-0]
		}
		else
		{
			m_builder.CreateStore(_value, getPtr(0)); // get stk[sp-0]
		}
	}
		

	// m_builder.restoreIP(now_point);
	

	// stk[sp] = _value
	// m_builder.CreateStore(_value, getPtr(-1)); 

	// m_local.push_back(_value);
	// llvm::Value* addr = nullptr;
	// if (topIdx < m_maxSize) {
	// 	addr = m_address[topIdx++];
	// 	storeItem(_value, addr);
	// 	//m_builder.CreateStore(_value, addr);
	// } else {
	// 	addr = storeItem(_value);
	// 	m_address.push_back(addr);
	// 	topIdx ++;
	// }
	/* create a register, with value of _value */
	// llvm::Value* addr = nullptr;
	// addr = storeItem(_value);
	// m_address.push_back(addr);
	// topIdx ++;
	// m_maxSize = std::max(m_maxSize, size());
}


// llvm::Value* GlobalStack::popLocal()
// {	
// 	llvm::Value* item = *(m_local.rbegin());
// 	m_local.pop_back();
// 	return item;
// }

// void GlobalStack::pushLocal(llvm::Value* _value)
// {	
// 	m_local.push_back(_value);
// }

llvm::Value* GlobalStack::pop()
{	
	llvm::Value* item;
	if (enableRegs)
	{
		if (m_local.empty())
			return llvm::ConstantInt::get(Type::Int256Ty, 0);
		item = *(m_local.rbegin());
		m_local.pop_back();
	}
	else{
		item = m_builder.CreateLoad(getPtr(0));//get stk[sp-0]
		// stk[0] --
		auto tsp = m_builder.CreateLoad(m_sp);	
		m_builder.CreateStore(m_builder.CreateSub(tsp, llvm::ConstantInt::get(Type::Int256Ty, 1)), m_sp); 
		// m_sp = m_builder.CreateSub(m_sp, llvm::ConstantInt::get(Type::Int256Ty, 1)); 
	}
	return item;
}

// llvm::Value* GlobalStack::pop(bool getValue) {
// 	auto item = get(0, getValue);
// 	assert(!m_local.empty());

// 	m_local.pop_back();
// 	// m_address.pop_back();
// 	topIdx--;

// 	m_minSize = std::min(m_minSize, size());
// 	return item;
// }


void GlobalStack::dup(size_t _index)
{	
	if (enableRegs)
	{
		if (_index >= m_local.size())
		{
			m_local.push_back(llvm::ConstantInt::get(Type::Int256Ty, 0));
			return;
		}
		llvm::Value* tmp = *(m_local.rbegin() + _index); // count from back
		m_local.push_back(tmp);
	}
	else{
		auto ptr = getPtr(_index); // src
		push(m_builder.CreateLoad(ptr));
	}
}

void GlobalStack::swap(size_t _index)
{
	assert(_index >= 0); // at least two elements

	if (enableRegs)
	{	
		if (m_local.size() <= _index + 1)
		{
			while (m_local.size() <= _index + 1)
				m_local.push_back(llvm::ConstantInt::get(Type::Int256Ty, 0));
			return;
		}
		llvm::Value* srcVal = *(m_local.rbegin() + _index + 1); // count from back
		// std::cerr << "@srcVal = " 
					// << llvm::dyn_cast<llvm::ConstantInt>(srcVal)->getSExtValue() << "\n";
		llvm::Value* dstVal = *m_local.rbegin();
		// std::cerr << "@dstVal = " 
					// << llvm::dyn_cast<llvm::ConstantInt>(dstVal)->getSExtValue() << "\n";
		auto lastIdx = m_local.size() - 1;
		m_local[lastIdx - (_index + 1)] = dstVal;
		m_local[lastIdx] = srcVal;
	}
	else
	{
		auto srcVal = m_builder.CreateLoad(getPtr(0));
		auto dstPtr = getPtr(_index + 1);
		m_builder.CreateStore(m_builder.CreateLoad(dstPtr), getPtr(0));
		m_builder.CreateStore(srcVal, dstPtr);
	}


}
/*
llvm::Value* GlobalStack::get(size_t _index, bool getValue)
{
	// assert(_index < m_local.size());
	// if (getValue)
	//	return *(m_local.rbegin() + _index); // count from back

	return loadItem(m_address[topIdx - _index - 1]);
}

// void GlobalStack::set(size_t _index, llvm::Value* _word)
// {
// 	assert(_index < m_local.size());

// 	m_local[size() - _index - 1] = _word;
// 	auto addr = m_address[size() - _index - 1];
// 	m_builder.CreateStore(_word, addr);
// }

llvm::Value* GlobalStack::storeItem(llvm::Value* item, llvm::Value* _addr) {
	if (item->getType() != Type::Int256Ty)
		item = m_builder.CreateZExt(item, Type::Int256Ty);
	// auto addr = m_builder.CreateGEP(m_sp, _index);
	// auto addr = _addr;
	// if (!_addr) {
	// 	auto now_point = m_builder.saveIP();
	// 	if (auto term = first_block->getTerminator())
	// 		m_builder.SetInsertPoint(term); // Insert before terminator
	// 	addr = m_builder.CreateAlloca(Type::Int256Ty, nullptr);
	// 	m_builder.restoreIP(now_point);
	// }
	auto addr = m_builder.CreateAlloca(Type::Int256Ty, nullptr);
	m_builder.CreateStore(item, addr);
	return addr;
}

llvm::Value* GlobalStack::loadItem(llvm::Value* addr) {
	if (addr->getType() != Type::Int256PtrTy)
		addr = m_builder.CreateIntToPtr(addr, Type::Int256PtrTy);
	return m_builder.CreateLoad(Type::Int256Ty, addr);
}
*/
void GlobalStack::finalize() {
	// for (int i = 0; i < size(); i++) {
	// 	auto slot = m_builder.CreateGEP(Type::Word, m_sp, m_builder.getInt32(i));
	// 	// m_builder.CreateAlignedStore(m_address, slot, llvm::MaybeAlign(32));
	// }
}

}
}
}

#pragma once

#include <vector>

#include "CompilerHelper.h"
#include "Common.h"


namespace dev
{
namespace eth
{
namespace trans
{
using namespace evmtrans;
using instr_idx = uint64_t;

class EEIModule;




class GlobalStack
{
public:
	bool enableRegs = false;

	GlobalStack( IRBuilder& _builder, llvm::Constant *_stk, llvm::Value *_spPtr);
	GlobalStack( const GlobalStack &rhs );

	void push(llvm::Value* _value);

	llvm::Value* pop();
	void dup(size_t _index);
	void swap(size_t _index);
	llvm::Value* get_sp() { return m_sp;};
	// llvm::Value* GlobalStack::popLocal();
	// void GlobalStack::pushLocal(llvm::Value* _value);
	
	std::vector<llvm::Value*> view(){return m_local;}
	std::vector<llvm::Value*>* get_locals_ptr(){return &m_local;}
	
	void clearLocals() {m_local.clear();}

	ssize_t minSize() const { return m_minSize; }
	ssize_t maxSize() const { return m_maxSize; }

	void finalize();

private:
	// llvm::Value* get(size_t _index, bool getValue = false);
	// void set(size_t _index, llvm::Value* _value);
	
	std::vector<llvm::Value*> m_local;
	std::vector<llvm::Value*> m_address;



	llvm::Constant *stk;

	IRBuilder &m_builder;
	llvm::Value* m_sp;  // address to the stack pointer
	llvm::BasicBlock* first_block;

	ssize_t m_minSize = 0;
	ssize_t m_maxSize = 0;
	ssize_t topIdx = 0;
	ssize_t stackSize = 32;

	// helper function
	// llvm::Value* storeItem(llvm::Value* item, llvm::Value* _addr = nullptr);
	// llvm::Value* loadItem(llvm::Value* addr);
	llvm::Value* getPtr(size_t _index);
};


class MiniLocalStack
{
public:
	MiniLocalStack(IRBuilder& _builder);

	/// Pushes value on stack
	void push(llvm::Value* _value);

	/// Pops and returns top value
	llvm::Value* pop();

	/// Duplicates _index'th value on stack
	void dup(size_t _index);

	/// Swaps _index'th value on stack with a value on stack top.
	/// @param _index Index of value to be swaped. Must be > 0.
	void swap(size_t _index);

	std::vector<llvm::Value*> view(){return m_local;}

	ssize_t size() const { return static_cast<ssize_t>(m_local.size()) - m_globalPops; }
	ssize_t minSize() const { return m_minSize; }
	ssize_t maxSize() const { return m_maxSize; }

	/// Finalize local stack: check the requirements and update of the global stack.
	void finalize();

private:
	/// Gets _index'th value from top (counting from 0)
	llvm::Value* get(size_t _index);

	/// Sets _index'th value from top (counting from 0)
	void set(size_t _index, llvm::Value* _value);



	/// Local stack items that has not been pushed to global stack. First item is just above global stack.
	std::vector<llvm::Value*> m_local;

	/// Items fetched from global stack. First element matches the top of the global stack.
	/// Can contain nulls if some items has been skipped.
	std::vector<llvm::Value*> m_input;

	std::vector<llvm::Value*> s_global;

	IRBuilder &m_builder;

	ssize_t m_globalPops = 0; 	///< Number of items poped from global stack. In other words: global - local stack overlap.
	ssize_t m_minSize = 0;		///< Minimum reached local stack size. Can be negative.
	ssize_t m_maxSize = 0;		///< Maximum reached local stack size.

	/// helper function
	llvm::Value* storeItem(llvm::Value* item);
	llvm::Value* loadItem(llvm::Value* addr);
};

class BasicBlock
{
public:
	explicit BasicBlock(instr_idx _firstInstrIdx, code_iterator _begin, code_iterator _end, llvm::Function* _mainFunc);

	llvm::BasicBlock* llvm() { return m_llvmBB; }

	instr_idx firstInstrIdx() const { return m_firstInstrIdx; }

	code_iterator begin() const { return m_begin; }
	code_iterator end() const { return m_end; }
	code_iterator terminator() const { return m_end - 1; }
	
	instr_idx terminatorIdx() const { return m_firstInstrIdx + m_end - m_begin - 1; }

	// for cfg
	std::vector<std::pair<instr_idx, std::vector<instr_idx>> > succs() {return m_succs; }
	void add_succ(std::pair<instr_idx, std::vector<instr_idx>> _succ){ m_succs.push_back(_succ);}
	void pop_succ(){ return m_succs.pop_back();}
	// std::vector<instr_idx> dsuccs() {return dm_succs;}
	// void add_dsucc(instr_idx _k){ dm_succs.push_back(_k);}

	std::vector<BasicBlock*> bbs_from;
	std::vector<BasicBlock*> bbs_to;

	uint32_t get_stk_in() const {return m_stk_in; };
	uint32_t get_stk_out() const {return m_stk_out; };

	void add_phi(llvm::PHINode* node) {m_phi_nodes.push_back(node); };
	std::vector<llvm::PHINode*>* get_phi_nodes_ptr() { return &m_phi_nodes; };

private:
	instr_idx const m_firstInstrIdx = 0; 	///< Code index of first instruction in the block
	code_iterator const m_begin = {};		///< Iterator pointing code beginning of the block
	code_iterator const m_end = {};			///< Iterator pointing code end of the block

	llvm::BasicBlock* const m_llvmBB;		///< Reference to the LLVM BasicBlock
	
	std::vector<std::pair<instr_idx, std::vector<instr_idx>> > m_succs;	///< next edges
	
	uint32_t m_stk_in;
	uint32_t m_stk_out;

	void ctxDep();

	std::vector<llvm::PHINode*> m_phi_nodes;

	// std::vector<instr_idx> dm_succs{std::vector<instr_idx>(0,0)};
};

}
}
}

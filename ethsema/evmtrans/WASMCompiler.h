#pragma once

// #include "BinaryTrans.h"
#include "BasicBlock.h"
#include "NEARModule.h"

namespace dev
{
namespace eth
{
namespace trans
{

int64_t splitCode(code_iterator _begin, code_iterator _end, uint64_t &RETURNPC);

class WASMCompiler
{
public:
	struct Options
	{
		/// Rewrite switch instructions to sequences of branches
		bool rewriteSwitchToBranches = true;

		/// Dump CFG as a .dot file for graphviz
		bool dumpCFG = false;
	};

	WASMCompiler(llvm::Module &M, std::vector<uint8_t> &B, uint64_t PC);
  WASMCompiler(llvm::Module &M, std::vector<uint8_t> &B, uint64_t PC, llvm::StringRef RtWasmCode);

	void compileMain(std::vector<uint8_t> &ExeCode, const std::string& Name);

  void FixEwasmDeployer(llvm::StringRef RtWasmCode) {
    assert(EwasmRetCall && "Fail to identify the RETURN used to deploy Ewasm code");
    auto Zero = llvm::ConstantInt::get(Type::Int32Ty, 0);

    llvm::IRBuilder<> IRB(EwasmRetCall);
    IRB.CreateGlobalStringPtr(RtWasmCode, "runtimeWasmCode");
    llvm::Value *pStr = IRB.CreateGEP(
      Module.getNamedGlobal("runtimeWasmCode"), {Zero, Zero});

    // EwasmRetCall->setOperand(2, pStr);
    // EwasmRetCall->setOperand(2, IRB.getInt32(RtWasmCode.size()));
  }

private:
  uint64_t RetPC = 0;
  llvm::StringRef RtWasmCode;
  llvm::LLVMContext &Context;
	std::vector<uint8_t> Bytecode;
	std::vector<uint8_t> ExeCode;

	llvm::Module& Module;
  llvm::CallInst *EwasmRetCall = nullptr;

	bool env_ewasm() { return true; }
	bool env_near() { return false; }
	
  NEARModule *NearEnv; // near env
  // EEIModule *EwasmEnv; // ewasm env
  // Arith256 *Arith; // arithmetic 256 library

	void dry_run_bb(BasicBlock const &bb, std::vector<uint64_t> &stack);
	void dry_run(std::vector<BasicBlock>& blocks);
	std::vector<BasicBlock> createBasicBlocks(code_iterator _begin, code_iterator _end);

	void  cfg_to_dot(std::string &s, std::vector<BasicBlock> const& blocks, bool minimal);

	// void compileAllBlocks(std::vector<BasicBlock>& blocks, class EEIModule& _eei, class Arith256& _arith, llvm::Module& _module, uint8_t* rtCodeItr=NULL, size_t rtCodeSize=0);

	// void compileBasicBlock(BasicBlock& _basicBlock, class EEIModule& _eei, class Arith256& _arith, MiniLocalStack& stack);
	std::vector<llvm::ConstantInt*> compileBasicBlock(BasicBlock& _basicBlock, class EEIModule& _eei, class Arith256& _arith, GlobalStack& _stack);

	void resolveJumps(std::vector<BasicBlock>& blocks);
	void resolve_phi(std::vector<BasicBlock> &blocks, std::map<BasicBlock*, std::vector<llvm::Value*>* > &stkMap);

	// void compile(code_iterator _begin, code_iterator _end, std::string const& _id, llvm::Module* module, class EEIModule& eei);

	/// Compiler options
	// Options const& m_options;

	/// Helper class for generating IR
	// IRBuilder m_builder;                                                

	/// Block with a jump table.
	llvm::BasicBlock* m_jumpTableBB = nullptr;

	/// Block with exit
	llvm::BasicBlock* m_exitBB = nullptr;

	/// Block with abort
	// llvm::BasicBlock* m_abortBB = nullptr;

	/// Main program function
	llvm::Function* m_mainFunc = nullptr;

	inline llvm::Constant *createGlobalStringPtr(llvm::LLVMContext &Context, llvm::Module &Module, llvm::StringRef Str, const llvm::Twine &Name = "") {
		llvm::GlobalVariable *GV = createGlobalString(Context, Module, Str, Name);
		llvm::Constant *Zero = llvm::ConstantInt::get(llvm::Type::getInt32Ty(Context), 0);
		llvm::Constant *Indices[] = {Zero, Zero};
		return llvm::ConstantExpr::getInBoundsGetElementPtr(GV->getValueType(), GV, Indices);
	}
	inline llvm::GlobalVariable *createGlobalString(llvm::LLVMContext &Context,  llvm::Module &Module, llvm::StringRef Str, const llvm::Twine &Name = "") {
		llvm::Constant *StrConstant = llvm::ConstantDataArray::getString(Context, Str, false);
		auto *GV = new llvm::GlobalVariable(Module, StrConstant->getType(), true, llvm::GlobalValue::ExternalLinkage, StrConstant, Name);
		GV->setUnnamedAddr(llvm::GlobalValue::UnnamedAddr::Global);
		GV->setAlignment(llvm::MaybeAlign(1));
		return GV;
	}
};

}
}
}

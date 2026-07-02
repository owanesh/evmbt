#include <stack> 
#include "Optimizer.h"

#include "preprocessor/llvm_includes_start.h"
#include <llvm/IR/BasicBlock.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/LegacyPassManager.h>
#include <llvm/Transforms/Scalar.h>
#include <llvm/Transforms/Scalar/GVN.h>

#include <llvm/ADT/Statistic.h>
#include "llvm/IR/CallSite.h"
#include <llvm/Support/raw_ostream.h>
#include <llvm/Analysis/CallGraph.h>
#include <llvm/IR/Instruction.h>
#include <llvm/IR/Instructions.h>


#include <llvm/Transforms/InstCombine/InstCombine.h>

#include <llvm/Transforms/IPO.h>
#include <llvm/Transforms/Utils.h>
#include <llvm/Transforms/Utils/BasicBlockUtils.h>
#include <llvm/Transforms/Utils/Mem2Reg.h>
#include <llvm/Pass.h>
// #include <llvm/IR/SymbolTableListTraits.h>
#include "preprocessor/llvm_includes_end.h"

#include "Arith256.h"
// #include "SECore.h" //SE project
#include "Type.h"
// #include "Tainter.h"


#include "EEIModule.h"

// #include"z3++.h"

namespace dev
{
namespace eth
{
namespace trans
{

namespace
{

static const auto c_bytecodePC = "pc";

class LongJmpEliminationPass: public llvm::FunctionPass
{
	static char ID;

public:
	LongJmpEliminationPass():
		llvm::FunctionPass(ID)
	{}

	virtual bool runOnFunction(llvm::Function& _func) override;
};

char LongJmpEliminationPass::ID = 0;

bool LongJmpEliminationPass::runOnFunction(llvm::Function& _func)
{
	auto iter = _func.getParent()->begin();
	if (&_func != &(*iter))
		return false;

	auto& mainFunc = _func;
	auto& ctx = _func.getContext();
	auto abortCode = llvm::ConstantInt::get(llvm::Type::getInt32Ty(ctx), -1);

	auto& exitBB = mainFunc.back();
	assert(exitBB.getName() == "Exit");
	auto retPhi = llvm::cast<llvm::PHINode>(&exitBB.front());

	auto modified = false;
	for (auto bbIt = mainFunc.begin(); bbIt != mainFunc.end(); ++bbIt)
	{
		if (auto term = llvm::dyn_cast<llvm::UnreachableInst>(bbIt->getTerminator()))
		{
			auto longjmp = term->getPrevNode();
			assert(llvm::isa<llvm::CallInst>(longjmp));
			auto bbPtr = &(*bbIt);
			retPhi->addIncoming(abortCode, bbPtr);
			llvm::ReplaceInstWithInst(term, llvm::BranchInst::Create(&exitBB));
			longjmp->eraseFromParent();
			modified = true;
		}
	}

	return modified;
}

}

bool optimize(llvm::Module& _module)
{
	auto pm = llvm::legacy::PassManager{};
	pm.add(llvm::createFunctionInliningPass(2, 2, false));
	// pm.add(new LongJmpEliminationPass{}); 				// TODO: Takes a lot of time with little effect
	pm.add(llvm::createCFGSimplificationPass());


    // Provide basic AliasAnalysis support for GVN.
    // pm.add(llvm::createBasicAliasAnalysisPass());

    // Promote allocas to registers.
    // pm.add(llvm::createPromoteMemoryToRegisterPass());
    
	// Do simple "peephole" optimizations and bit-twiddling optzns.
    pm.add(llvm::createInstructionCombiningPass());

    // Reassociate expressions.
    // pm.add(llvm::createReassociatePass());
    
	// Eliminate Common SubExpressions.
    // pm.add(llvm::createNewGVNPass());
    // Simplify the control flow graph (deleting unreachable blocks, etc).
    // pm.add(llvm::createCFGSimplificationPass());


	// pm.add(llvm::createInstructionCombiningPass());
	pm.add(llvm::createAggressiveDCEPass());
	pm.add(llvm::createLowerSwitchPass());
	return pm.run(_module);
}

namespace
{

class LowerEVMPass: public llvm::FunctionPass
{
	static char ID;

public:
	LowerEVMPass():
		llvm::FunctionPass(ID)
	{}

	// virtual bool runOnBasicBlock(llvm::BasicBlock& _bb) override;
	virtual bool runOnFunction(llvm::Function& _func) override;

	using llvm::FunctionPass::doFinalization;
	virtual bool doFinalization(llvm::Module& _module) override;
};

char LowerEVMPass::ID = 0;

bool LowerEVMPass::runOnFunction(llvm::Function& _func)
{	
	// llvm::errs()<< "OPTIMIZATIONing---------------\n";
	auto modified = false;
	auto module = _func.getParent();
	auto i512Ty = llvm::IntegerType::get(_func.getContext(), 512);
	for (llvm::BasicBlock &_bb : _func) {
		for (auto it = _bb.begin(); it != _bb.end(); ++it)
		{
			auto& inst = *it;
			llvm::Function* func = nullptr;
			if (inst.getType() == Type::Word)
			{
				switch (inst.getOpcode())
				{
				case llvm::Instruction::UDiv: {
					// if (auto constant = llvm::dyn_cast<llvm::ConstantInt>(inst.getOperand(0)))
					// 	break;
					// if (auto constant = llvm::dyn_cast<llvm::ConstantInt>(inst.getOperand(1)))
					// 	break;
					func = Arith256::getUDiv256Func(*module);
					break;
				}

				case llvm::Instruction::URem: {
					// if (auto constant = llvm::dyn_cast<llvm::ConstantInt>(inst.getOperand(0)))
					// 	break;
					// if (auto constant = llvm::dyn_cast<llvm::ConstantInt>(inst.getOperand(1)))
					// 	break;
					func = Arith256::getURem256Func(*module);
					break;
				}

				case llvm::Instruction::SDiv: {
					// if (auto constant = llvm::dyn_cast<llvm::ConstantInt>(inst.getOperand(0)))
					// 	break;
					// if (auto constant = llvm::dyn_cast<llvm::ConstantInt>(inst.getOperand(1)))
					// 	break;
					func = Arith256::getSDiv256Func(*module);
					break;
				}

				case llvm::Instruction::SRem: {
					// if (auto constant = llvm::dyn_cast<llvm::ConstantInt>(inst.getOperand(0)))
					// 	break;
					// if (auto constant = llvm::dyn_cast<llvm::ConstantInt>(inst.getOperand(1)))
					// 	break;
					func = Arith256::getSRem256Func(*module);
					break;
				}

				case llvm::Instruction::Mul: {
					// if (auto constant = llvm::dyn_cast<llvm::ConstantInt>(inst.getOperand(0)))
					// 	break;
					// if (auto constant = llvm::dyn_cast<llvm::ConstantInt>(inst.getOperand(1)))
					// 	break;
					func = Arith256::getMul256Func(*module);
					break;
				}
				}
			}
			else if (inst.getType() == Type::Int128Ty) {
				switch (inst.getOpcode())
				{
				case llvm::Instruction::Mul: {
					// if (auto constant = llvm::dyn_cast<llvm::ConstantInt>(inst.getOperand(0)))
					// 	break;
					// if (auto constant = llvm::dyn_cast<llvm::ConstantInt>(inst.getOperand(1)))
					// 	break;
					func = Arith256::getMul128Func(*module);
					break;
				}
				}
			}
			else if (inst.getType() == i512Ty)
			{
				switch (inst.getOpcode())
				{
				case llvm::Instruction::URem:
					func = Arith256::getURem512Func(*module);
					break;
				}
			}

			if (func)
			{
				auto call = llvm::CallInst::Create(func, {inst.getOperand(0), inst.getOperand(1)});
				llvm::ReplaceInstWithInst(_bb.getInstList(), it, call);
				modified = true;
				// llvm::errs()<< "[Optimize] " << inst.getOpcode() << " -> modified\n";
			}
		}
	}
	return modified;
}

bool LowerEVMPass::doFinalization(llvm::Module&)
{
	return false;
}


/* A New PASS */
class CodecopyPass: public llvm::FunctionPass
{
	static char ID;

public:
	CodecopyPass():
		llvm::FunctionPass(ID)
	{}
	virtual bool runOnFunction(llvm::Function& _func) override;
	using llvm::FunctionPass::doFinalization;
	virtual bool doFinalization(llvm::Module& _module) override;
};

char CodecopyPass::ID = 1;

bool CodecopyPass::runOnFunction(llvm::Function& _func) 
{	
	llvm::errs()<< "======================= my optimize::estimate dead storage ===========================\n";
	
	auto modified = false;
	auto module = _func.getParent();
	for (llvm::BasicBlock &_bb : _func) {
		for (auto it = _bb.begin(); it != _bb.end(); ++it)
		{
			auto& inst = *it;
			auto operandsNum  = inst.getNumOperands();
			if (!(inst.getOpcode() == llvm::Instruction::Call && operandsNum == 3)) continue;
		
			if (inst.getOperand(2)->getName() == "ethereum.storageStore") {
				llvm::errs() <<  inst.getOpcodeName() << "\n";
				for (auto i = 0; i < inst.getNumOperands(); ++i)  {
					llvm::errs() << "[" << i << "] " << inst.getOperand(i)->getName() << "\n";
				}
				auto mslotPtr = inst.getOperand(0);
				auto mvalue   = inst.getOperand(1);
			}
			else if (inst.getOperand(1)->getName() == "ethereum.storageSload") {
				for (auto i = 0; i < inst.getNumOperands(); ++i)  {
					llvm::errs() << "[" << i << "] " << inst.getOperand(i)->getName() << "\n";
				}
			}
			//  	inst.getOperand(0) == eei.Func_storageStore) {
			// // inst.getOperand(0) == ''
			// // for (auto i
			// return false;
		}
	}
	return modified;
}

bool CodecopyPass::doFinalization(llvm::Module&){return false;}




/* A Reentrancy PASS */
class EstimateReenrancePass: public llvm::FunctionPass
{
	static char ID;

public:
	EstimateReenrancePass():
		llvm::FunctionPass(ID)
	{}
	bool detectBug(llvm::Function& _func);
	virtual bool runOnFunction(llvm::Function& _func) override;
	using llvm::FunctionPass::doFinalization;
	virtual bool doFinalization(llvm::Module& _module) override;
};

char EstimateReenrancePass::ID = 2;



bool EstimateReenrancePass::detectBug(llvm::Function& _func) 
{	
	llvm::errs() << "Detect Reentrancy\n"; 
	std::vector<llvm::Instruction*> extCallList;
	for (auto _it = _func.begin(); _it != _func.end(); ++_it) {
		auto _bbPtr = &(*_it);
		bool foundExtCall = false;
		for (auto it = _bbPtr->begin(); it != _bbPtr->end(); ++it)
		{
			auto& inst = *it;
			unsigned operandsNum = inst.getNumOperands();
			if (operandsNum == 6 && inst.getOperand(5)->getName() == "ethereum.call")
			{
				extCallList.push_back(&(*it));
				llvm::errs() << "Find an EXTCALL "; 
				if (inst.hasMetadata(c_bytecodePC))
				{
					auto _meta = llvm::cast<llvm::ValueAsMetadata>(inst.getMetadata(c_bytecodePC)->getOperand(0))->getValue();
					auto pc = llvm::dyn_cast<llvm::ConstantInt>(_meta)->getSExtValue();
					llvm::errs() << "PC = " << pc << "\n"; 
				}
				foundExtCall = true;
			}
			else if (foundExtCall && operandsNum == 3 && inst.getOperand(2)->getName() == "ethereum.storageStore")
			{
				llvm::errs() << "Find an call-store"; 
				return true;
			}
		}
	}
	// traverse the CFG to find a Storage Writting after the external call, DFS
	std::vector<llvm::BasicBlock*> visited;
	for (auto extCall : extCallList) {
		std::stack<llvm::BasicBlock*> fifo;
		fifo.push(extCall->getParent());
		while (fifo.size() > 0)
		{
			llvm::BasicBlock* dnode = fifo.top();
			fifo.pop();
			// find Store
			for (auto it = dnode->begin(); it != dnode->end(); ++it)
			{
				auto& inst = *it;
				if (inst.getNumOperands() == 3 && inst.getOperand(2)->getName() == "ethereum.storageStore")
				{
					// llvm::errs() << "Find an storageStore "; 
					// if (inst.hasMetadata(c_bytecodePC))
					// {
					// auto _meta = llvm::cast<llvm::ValueAsMetadata>(inst.getMetadata(c_bytecodePC)->getOperand(0))->getValue();
					// auto pc = llvm::dyn_cast<llvm::ConstantInt>(_meta)->getSExtValue();
					// llvm::errs() << "PC = " << pc << "\n"; 
					// }
					return true; // one extCall depends on Storage
				}
				
			}
			llvm::Instruction* TInst = dnode->getTerminator();
			for (unsigned I = 0, NSucc = TInst->getNumSuccessors(); I < NSucc; ++I)
			{	
				llvm::BasicBlock* successor = TInst->getSuccessor(I);
				if (std::find(visited.begin(), visited.end(), successor) == visited.end())
				{
					fifo.push(successor); // never visited before
					visited.push_back(successor);
				}
			}
		}
	}
	return false;
}

bool EstimateReenrancePass::runOnFunction(llvm::Function& _func) 
{	
	if (_func.getName() != "main")	return false;
	llvm::errs()<< "=========== [" << _func.getName() << "]optimize::estimate reentrancy ===========================\n";

	bool modified = false;
	// detect reentrancy
	if (!detectBug(_func)) return false;
	/*****************************
	 * mutex = SLOAD()
	 * if (mutex != 0)
	 * 		unreachable
	 * SSTORE(mutex, 1) //lock
	 * Entry ...
	 *    .... code ....
	 * ------- terminator ------
	 * SSTORE(mutex, 0) // unlock
	 * ...
	******************************/

	auto module = _func.getParent();
    // storageLoad
    llvm::Function * Func_storageLoad = module->getFunction("ethereum.storageLoad");
	assert (Func_storageLoad != nullptr);
    // storageStore
	llvm::Function * Func_storageStore = module->getFunction("ethereum.storageStore");
	assert (Func_storageStore != nullptr);		

	std::vector<llvm::Instruction*> terminatorInstrList;
	for (auto _it = _func.begin(); _it != _func.end(); ++_it) {
		auto _bbPtr = &(*_it);
		for (auto it = _bbPtr->begin(); it != _bbPtr->end(); ++it)
		{
			auto& inst = *it;
			auto operandsNum = inst.getNumOperands();
			// llvm::errs() << "operands number= " << inst.getNumOperands() << "\n";
			// llvm::errs() <<  inst.getOperand(operandsNum-1)->getName() << "\n";
			// if (!(inst.getOpcode() == llvm::Instruction::Call && operandsNum == 6)) continue;
			// llvm::errs() <<  inst.getOpcodeName() << "\n";
			if (operandsNum == 3 
				&& (inst.getOperand(operandsNum-1)->getName() == "ethereum.finish" 
					|| inst.getOperand(operandsNum-1)->getName() == "ethereum.revert")) {
				terminatorInstrList.push_back(&(*it));
			}
		}
	}

	// entry, guarding all [public] functions
	auto entryInst = &(*_func.begin()->begin());
	auto m_builder = IRBuilder{entryInst};

	// FIXME. we set mutex at Storage[0xffffffff]
	auto AddressPtr = m_builder.CreateAlloca(Type::Int256Ty, nullptr, "mutex.slot.ptr");
	m_builder.CreateStore(llvm::ConstantInt::get(Type::Int256Ty, 0xfffffff), AddressPtr);
	auto ValPtr = m_builder.CreateAlloca(Type::Int256Ty, nullptr, "mutex.value.ptr");

	llvm::errs() << "init \n";
	m_builder.CreateCall(Func_storageLoad, {m_builder.CreateBitCast(AddressPtr, Type::Int32PtrTy), m_builder.CreateBitCast(ValPtr, Type::Int32PtrTy)});
	auto mutex = m_builder.CreateLoad(ValPtr);
	
	llvm::errs() << "locked in the entry" << "\n";
	// Check the mutex before the CALL
	auto _cond = m_builder.CreateICmpNE(mutex, Constant::get(0), "reentrancy.check");
	llvm::SplitBlockAndInsertIfThen(_cond, entryInst, true);

	IRBuilder before_builder(entryInst);
	//lock
	before_builder.CreateStore(Constant::get(1), ValPtr);
	before_builder.CreateCall(Func_storageStore, {before_builder.CreateBitCast(AddressPtr, Type::Int32PtrTy), before_builder.CreateBitCast(ValPtr, Type::Int32PtrTy)}); 


	for (auto terminator : terminatorInstrList) {
		// unlock
		IRBuilder end_builder(terminator);
		// IRBuilder end_builder(instr->getNextNonDebugInstruction());
		end_builder.CreateStore(Constant::get(0), ValPtr);
		llvm::errs() << "unlock before an Terminator\n";
		end_builder.CreateCall(Func_storageStore, {end_builder.CreateBitCast(AddressPtr, Type::Int32PtrTy), end_builder.CreateBitCast(ValPtr, Type::Int32PtrTy)});
		modified = true;
	}
	return modified;
}
bool EstimateReenrancePass::doFinalization(llvm::Module&){return false;}


/* A New PASS */
class DetectSuicidePass: public llvm::FunctionPass
{
	static char ID;

public:
	DetectSuicidePass():
		llvm::FunctionPass(ID)
	{}
	virtual bool runOnFunction(llvm::Function& _func) override;
	using llvm::FunctionPass::doFinalization;
	virtual bool doFinalization(llvm::Module& _module) override;
};

char DetectSuicidePass::ID = 3;

bool DetectSuicidePass::runOnFunction(llvm::Function& _func) 
{	
	if (_func.getName() != "main") return false;
	llvm::errs()<< "=========== [" << _func.getName() << "]optimize::detect Suicide ===========================\n";
	/**
	 Demonstration of how Z3 can be used to prove validity of
	De Morgan's Duality Law: {e not(x and y) <-> (not x) or ( not y) }
	**/
	/*
    z3::context c;
	z3::expr arr = c.constant("arr", c.array_sort(c.bv_sort(256), c.bv_sort(8))); 
    
	z3::expr x = c.bv_const("x", 8);	
	arr = z3::store(arr, 0, x);
    z3::expr conjecture = (z3::select(arr, 0) == c.bv_val(12, 8));
    z3::solver s(c);
    s.add(conjecture);
    std::cout << s << "\n";
	std::cout << s.check() << "\n";

	z3::model m = s.get_model();
	// std::cout << "Model::" << m << "\n";
	for (unsigned i = 0; i < m.size(); i++) {
		z3::func_decl v = m[i];
		// assert(v.arity() == 0); 
		std::cout << v.name() << " = " << m.get_const_interp(v) << "\n";
	}
	// std::cout << "x + y + 1 = " << m.eval(x + y + 1) << "\n";
	*/

	
	/* SE project
	se::SEngine instance(_func.getParent());
	instance.run(&_func);
	
    for (auto _p: instance.m_Nodes)
		llvm::errs() << "Node #" << _p->uid << "\n";

	exit(-1);
	// analysis
	for (auto nodePtr : instance.m_Nodes)
	{	
		// if (nodePtr->uid == 0) continue;
		for (auto it = nodePtr->bbPtr->begin(); it != nodePtr->bbPtr->end(); ++it)
		{	
			if (it->getOpcode() != llvm::Instruction::Call)	continue;

            const llvm::CallBase& cs = llvm::cast<llvm::CallBase>(*it);
            llvm::Function* F = llvm::dyn_cast<llvm::Function>(cs.getCalledOperand());
            std::string fName = F->getName().str();
			if (fName == "ethereum.selfDestruct")
			{
				llvm::errs() << "SelfDestruct Detected\n";

				llvm::errs() << "----------------- constraints -----------------------\n";
				for (auto _cst : nodePtr->constraints)
					llvm::errs() << _cst.to_string() << "\n";
				llvm::errs() << "-------------------------------------------------------\n";

				z3::solver s(instance.ExprCtx);
				for (auto _cst : nodePtr->constraints)
					s.add(_cst);
				z3::set_param("timeout", 100000);
				if (s.check() != z3::sat) {
					llvm::errs() << "unsat:\n";
					llvm::errs() << "Constrains#" << nodePtr->constraints.size() << "\n";
					// return false;
				}
				else
				{
					llvm::errs() << "Solved !!\n";

					z3::model m = s.get_model();
					for (unsigned i = 0; i < m.size(); i++) {
						z3::func_decl v = m[i];
						std::cout << v.name() << " = " << m.get_const_interp(v) << "\n";
					}
				}
			}
		}
		*/

		/******************** branch analysis ********************** 
		auto terminator = nodePtr->bbPtr->getTerminator();
		if (auto branch = llvm::dyn_cast<llvm::BranchInst>(terminator)) 
		{
			if (branch->isConditional())
			{
				llvm::errs() << "Found -> " << terminator->getOpcodeName() << "\n";
				z3::solver s(instance.ExprCtx);
				s.add(nodePtr->constraints[0]);
				std::cout << s << "\n";
			
				std::cout << s << "\n";
				std::cout << s.check() << "\n";
				if (s.check() != z3::sat) return false;
				
				z3::model m = s.get_model();
				for (unsigned i = 0; i < m.size(); i++) {
					z3::func_decl v = m[i];
					std::cout << v.name() << " = " << m.get_const_interp(v) << "\n";
				}
			}
		}
		**************************************************************/
	// }
	
	
	// for (auto edge : instance.m_Edges)
	// {
	// 	llvm::errs() << "#" << edge.node_from << "-> #" << edge.node_to << "\n	";
	// }	
	
	// llvm::errs() << "asdasd@Run\n";
	return false;
}
bool DetectSuicidePass::doFinalization(llvm::Module&){ return false; }



/* A Rollback PASS */
class EstimateRollbackPass: public llvm::FunctionPass
{
	static char ID;


public:
	EstimateRollbackPass():
		llvm::FunctionPass(ID)
	{}
	bool detectBug(llvm::Function& _func);
	virtual bool runOnFunction(llvm::Function& _func) override;
	using llvm::FunctionPass::doFinalization;
	virtual bool doFinalization(llvm::Module& _module) override;
};

char EstimateRollbackPass::ID = 4;


bool EstimateRollbackPass::detectBug(llvm::Function& _func)
{
	std::vector<llvm::Instruction*> icmpInstList;
	std::vector<llvm::Value*> originValList, senderValList;

	for (auto _it = _func.begin(); _it != _func.end(); ++_it) {
		auto _bbPtr = &(*_it);
		for (auto it = _bbPtr->begin(); it != _bbPtr->end(); ++it)
		{
			auto& inst = *it;
			if (inst.getOpcode() == llvm::Instruction::ICmp && inst.getNumOperands() == 2)
				icmpInstList.push_back(&(*it));

			else if (inst.getNumOperands() == 3
				&& inst.getOperand(2)->getName() == "ethereum.getTxOrigin")
				originValList.push_back(&(*std::next(it, 7)));

			else if (inst.getNumOperands() == 3
				&& inst.getOperand(2)->getName() == "ethereum.getTxOrigin")
				senderValList.push_back(&(*std::next(it, 7)));
		}
	}
	if (senderValList.size() == 0 || originValList.size() == 0) return true;
	
	for (auto icmpInst : icmpInstList)
	{
		llvm::Value* lhs = icmpInst->getOperand(0);
		llvm::Value* rhs = icmpInst->getOperand(1);
		if ((std::find(originValList.begin(), originValList.end(), lhs) != originValList.end() && 
			 std::find(senderValList.begin(), senderValList.end(), rhs) != senderValList.end())
			|| (std::find(originValList.begin(), originValList.end(), rhs) != originValList.end() && 
			    std::find(senderValList.begin(), senderValList.end(), lhs) != senderValList.end()))
		{
			return false;
		}
	}
	return true;
}

bool EstimateRollbackPass::runOnFunction(llvm::Function& _func) 
{	
	if (_func.getName() != "main")	return false;
	llvm::errs()<< "=========== [" << _func.getName() << "]optimize::estimate rollback ===========================\n";

	auto module = _func.getParent();
	bool modified = false;

	if (!detectBug(_func)) return false;

	llvm::Function * Func_getCaller = module->getFunction("ethereum.getCaller");
	llvm::Function * Func_getTxOrigin = module->getFunction("ethereum.getTxOrigin");
	assert (Func_getCaller != nullptr && Func_getTxOrigin != nullptr);		

	// side effects function -> dispatcher
	std::vector<llvm::Instruction*> callInstrList;
	callInstrList.push_back(&(*_func.begin()->begin()));

	for (auto instr : callInstrList) {
		IRBuilder m_builder(instr);

		auto ValPtr = m_builder.CreateAlloca(Type::AddressTy, nullptr);

		m_builder.CreateCall(Func_getCaller, {m_builder.CreateBitCast(ValPtr, Type::Int32PtrTy)});
		auto m_sender = m_builder.CreateLoad(ValPtr);

		m_builder.CreateCall(Func_getTxOrigin, {m_builder.CreateBitCast(ValPtr, Type::Int32PtrTy)});
		auto m_origin = m_builder.CreateLoad(ValPtr);

		auto _cond = m_builder.CreateICmpNE(m_sender, m_origin);
		llvm::SplitBlockAndInsertIfThen(_cond, instr, true);	
		
		/***************
		 if (tx.origin != msg.sender)
		 	unreachable
		 else
		 	[instr]
		 * **************/
		modified = true;
	}
	return modified;
}
bool EstimateRollbackPass::doFinalization(llvm::Module&){return false;}


/* Modify from https://github.com/fadyosman/LLVMTaintAnalysis/blob/master/taintpass.cpp
 * author: fadyosman
*/
// class TainterPass: public llvm::ModulePass
// {
// 	static char ID;

// public:
// 	TainterPass():
// 		llvm::ModulePass(ID)
// 	{}

// 	// static char ID=4; // Pass identification, replacement for typeid
// 	std::vector<TaintEdge> taintList;
// 	//Entries for adding an entry for "main" and sink for "strcpy".
// 	//Those are hardcoded for demonestration purposes but later they will be read from a file (probably protocol buffers file).
// 	//Vector of the functions to search for (entry, source, sink) later this can be loaded from file, probably I will use protocol buffers.
// 	std::vector<ApiFunc> apiFuncs;
// 	//Structs for "strcpy" and "main" functions.
// 	ApiFunc strcpySink;
// 	ApiFunc mainEntry;

// 	virtual bool runOnModule(llvm::Module &m) override;
// 	using llvm::ModulePass::doFinalization;
// 	virtual bool doFinalization(llvm::Module& _module) override;

// private:
// 	TaintNode getCallee(TaintNode node);	
// };

// char TainterPass::ID = 5;


// //Helper function to get the callee of a certain node.
// TaintNode TainterPass::getCallee(TaintNode node) {
// 	for(auto t : taintList) {
// 		if(t.src == node) {
// 			return t.dst;
// 		}
// 	}
// 	return TaintNode();
// }

// bool TainterPass::runOnModule(llvm::Module &m) 
// {	
//    //strcpy sink.
//         strcpySink.interestingArg = 1; //1 is the second argument, so we are interested only if user input reaches the second argument.
//         strcpySink.funcName = "strcpy";
//         strcpySink.type = API_SINK;
//         apiFuncs.push_back(strcpySink);
//         //Main function entry.
//         mainEntry.interestingArg = 1; //1 is argv, so we are interested in argv.
//         mainEntry.funcName = "main";
//         mainEntry.type = API_ENTRY;
//         apiFuncs.push_back(mainEntry);

//         //Looping through functions to find all sources and sinks.
//         for (llvm::Function &f: m) {
//             //Looping through each function argument to analyze the taint.
//             for(auto arg = f.arg_begin(); arg != f.arg_end(); ++arg) {
//                 llvm::StringRef argName = arg->getName();
//                 unsigned argNo = arg->getArgNo();
//                 //List of local tainted variables.
//                 std::vector<std::string> taintedVars;
//                 //The first taint is the function argument.
//                 taintedVars.push_back(argName);
//                 //Follow each argument and see if it reaches a sink.
//                 for (llvm::BasicBlock &bb : f) {
//                     for (llvm::Instruction &i : bb) {
//                         unsigned opcode = i.getOpcode();
//                         switch (opcode) {
//                         case llvm::Instruction::Store:
//                         {
//                             llvm::StoreInst *storeinst = llvm::dyn_cast<llvm::StoreInst>(&i);
//                             //First operand of store.
//                             llvm::StringRef operand1 = storeinst->getOperand(0)->getName();
//                             //Second operand of store.
//                             llvm::StringRef operand2 = storeinst->getOperand(1)->getName();
//                             //Checking if the taintedVars vector contains this input.
//                             if (std::find(taintedVars.begin(), taintedVars.end(), operand1) != taintedVars.end())
//                             {
//                                 // Element in vector.
//                                 // The instruction is loading operand1 into operand2.
//                                 // So let's add operand2 into our taint list.
//                                 taintedVars.push_back(operand2);
//                             }
//                             break;
//                         }
//                         case llvm::Instruction::Load:
//                         {
//                             llvm::LoadInst *loadinst = llvm::dyn_cast<llvm::LoadInst>(&i);
//                             //Getting the variable that's going to be loaded.
//                             llvm::StringRef operand = loadinst->getOperand(0)->getName();
//                             //Getting the variable that's going to be set.
//                             llvm::StringRef result = loadinst->getName();
//                             if (std::find(taintedVars.begin(), taintedVars.end(), operand) != taintedVars.end())
//                             {
//                                 //The instruction is loading operand into result.
//                                 //So let's as result into taint list.
//                                 taintedVars.push_back(result);
//                             }
//                             break;
//                         }
//                         case llvm::Instruction::GetElementPtr:
//                         {
//                             //Same as load and store, adding taint.
//                             llvm::GetElementPtrInst *gepinst = llvm::dyn_cast<llvm::GetElementPtrInst>(&i);
//                             llvm::StringRef operand = gepinst->getOperand(0)->getName();
//                             llvm::StringRef result = gepinst->getName();
//                             if (std::find(taintedVars.begin(), taintedVars.end(), operand) != taintedVars.end())
//                             {
//                                 //The instruction is loading operand into result.
//                                 //So let's as result into taint list.
//                                 taintedVars.push_back(result);
//                             }
//                             break;
//                         }
//                         //TODO: Write code with if statements to test this module.
//                         //TODO: PHI instruction handling for taint analysis, also test against vulnserver and detect all the bugs.
//                         case llvm::Instruction::PHI:
//                         {
//                             llvm::PHINode *phi = llvm::dyn_cast<llvm::PHINode>(&i);
//                             break;
//                         }
//                         case llvm::Instruction::Call:
//                         {
//                             //TODO: Take functions that reads our tainted input and pass into consideration.

//                             //Classifying functions.
//                             llvm::CallInst *callinst = llvm::dyn_cast<llvm::CallInst>(&i);
//                             llvm::Function* calledFunction = callinst->getCalledFunction();
//                             //Populating the taints vector contains caller-callee data.

//                             TaintEdge taint;
//                             taint.src.name = f.getName();
//                             taint.dst.name = calledFunction->getName();
//                             taint.src.arg = argNo;
//                             //Setting the type intially to user defined function.
//                             taint.src.type = API_USERFUNC;
//                             taint.dst.type = API_USERFUNC;

//                             //Checking if the src or the destination is defined as source, entry or sink and change the type.
//                             for(ApiFunc s:apiFuncs){
//                                 if(f.getName() == s.funcName) {
//                                     taint.src.type = s.type;
//                                 }
//                                 if(calledFunction->getName() == s.funcName) {
//                                     taint.dst.type = s.type;
//                                 }
//                             }

//                             //Loop through the arguments to find tainted arguments, adding the tainted arguments to the taints vector.
//                             int operandIndex = 0;
//                             for(auto calledArg = callinst->arg_begin(); calledArg != callinst->arg_end(); ++calledArg) {
//                                 if (std::find(taintedVars.begin(), taintedVars.end(), calledArg->get()->getName()) != taintedVars.end()) {
//                                     taint.dst.arg = operandIndex;
//                                     taintList.push_back(taint);
//                                 }
//                                 operandIndex++;
//                             }
//                             break;
//                         }
//                         default:
//                             break;
//                         }
//                     }
//                 }

//             }
//         }
// 		llvm::errs()<<"==============================" << taintList.size() << "==============================\n";
//         //Performing the analysis and printing analysis trace for each vulnerability.
//         for(auto &t: taintList) {
//            if(t.src.type == API_ENTRY) {
//                 std::vector<TaintNode> trace;
//                 TaintNode caller = t.src;
//                 while(caller.name != "") {
//                     TaintNode callee = getCallee(caller);
//                     trace.push_back(caller);
//                     if(callee.type == API_SINK) {
//                         for(auto s: apiFuncs) {
//                             if(s.funcName == callee.name && s.interestingArg != callee.arg) {
//                                 //We found a sink but the data is not passed to the interesting argument so ignore it.
//                                 break;
//                             }
//                         }
//                         //Print the stack trace with emojies to show entry, sink and normal functions.
//                         llvm::errs()<<"==============================------------==============================\n";
//                         llvm::errs()<<"\U0001F525 Vulnerability detected: dangerous use of '"<<callee.name<<"' \U0001F525\n";
//                         llvm::errs()<<"\U0001F500 Analysis Trace (argument index is zero based):\n";
//                         trace.push_back(callee);
//                         for(auto k :trace) {
//                             llvm::errs()<<"\t";
//                             switch (k.type) {
//                             case API_SINK:
//                                 llvm::errs()<<"\U0000274E ";
//                                 break;
//                             case API_ENTRY:
//                             case API_SOURCE:
//                                 llvm::errs()<<"\U0001F4E5 ";
//                                 break;
//                             default:
//                                 llvm::errs()<<"\U0001F504 ";
//                                 break;
//                             }
//                             llvm::errs()<<"Function : "<<k.name<<" argument "<<k.arg<<"\n";
//                         }
//                         llvm::errs()<<"==============================------------==============================\n";
//                         break;
//                     }
//                     caller = callee;
//                 }
//             }
//         }
//         //We are not modifying the code, so return false.
//         return false;
// }

// bool TainterPass::doFinalization(llvm::Module&){ return false; }


/* A PASS enabling to upgrade on-chain smart contract in one identical address*/
class ProxyWrapperPass: public llvm::FunctionPass
{
	static char ID;

public:
	ProxyWrapperPass():
		llvm::FunctionPass(ID)
	{}
	virtual bool runOnFunction(llvm::Function& _func) override;
	using llvm::FunctionPass::doFinalization;
	virtual bool doFinalization(llvm::Module& _module) override;
};

char ProxyWrapperPass::ID = 6;

bool ProxyWrapperPass::runOnFunction(llvm::Function& _func) 
{	
	auto findRtReturn = [](llvm::Function&  _func) -> llvm::Instruction* 
	{ 
		for (auto _bit = _func.begin(); _bit != _func.end(); ++_bit) 
		{
			for (auto it = _bit->begin(); it != _bit->end(); ++it)
			{
				auto& inst = *it;
				if (inst.getNumOperands() == 3 && inst.getOperand(2)->getName() == "ethereum.finish")
				{
					llvm::errs() << " find a finish\n";
					auto offset = inst.getOperand(0);
					llvm::errs() << llvm::dyn_cast<llvm::ConstantInt>(inst.getOperand(1))->getSExtValue();
					// if (offset->hasName())
					// 	llvm::errs() << offset->getName() << "\n"; 
					// else 
					// 	llvm::errs() << "ptr " << offset << "\n";

					if(llvm::cast<llvm::Operator>(offset)->getOpcode() == llvm::Instruction::GetElementPtr)
					{	
						auto GEPInst = llvm::cast<llvm::Operator>(offset);
						if(GEPInst->getOperand(0)->getName() == "runtimeCode")
						{
							// llvm::errs() << "Found the instruction.\n" << "offset = " << llvm::dyn_cast<llvm::ConstantInt>(GEPInst->getOperand(1))->getSExtValue();
							// exit(-1);
							return &(*it);
						}
					}
				}
			}
		}
		return nullptr;
	};

	if (_func.getName() != "main")	return false;
	llvm::errs()<< "=========== [" << _func.getName() << "]optimize::ProxyWrapper ===========================\n";

	auto module = _func.getParent();
	llvm::Function * Func_getCaller = module->getFunction("ethereum.getCaller");
	llvm::Function * Func_call = module->getFunction("ethereum.call");
	llvm::Function * Func_returnDataCopy = module->getFunction("ethereum.returnDataCopy");
	llvm::Function * Func_finish = module->getFunction("ethereum.finish");
	llvm::Function * Func_getGasLeft = module->getFunction("ethereum.getGasLeft");
	assert (Func_getCaller != nullptr && Func_call != nullptr && Func_returnDataCopy != nullptr && Func_finish != nullptr && Func_getGasLeft != nullptr);		

	// side effects function -> dispatcher
	auto RtReturnInst = findRtReturn(_func);
	if (RtReturnInst == nullptr)	return false;

	// replace
	/*
	----------------- before -------------------
	call(Func_finish, getElementPtr @runtimeCode, length)
	--------------------------------------------
	|||||||||||||||||||||||||||||||||||||||||||
	------------------ after -------------------
	call (Func_getcaller, %ptr, 160, ...)
	call (Func_call, gas, value, load(ptr), inputOffset, inputSize, outputOffset, outputSize)
	call(Func_finish, outputOffset, outputSize)
	*/
	
	int64_t runtimeCodeSize = llvm::dyn_cast<llvm::ConstantInt>(RtReturnInst->getOperand(1))->getSExtValue();
	
	IRBuilder m_builder(RtReturnInst);
	// llvm::ReplaceInstWithInst(RtReturnInst, m_builder.CreateCall)

	// callee address
	auto calleeAddrPtr = m_builder.CreateAlloca(Type::AddressTy, nullptr);
	m_builder.CreateCall(Func_getCaller, {m_builder.CreateBitCast(calleeAddrPtr, Type::Int32PtrTy)}); // auto m_sender = m_builder.CreateLoad(ValPtr);

	// value
	llvm::Value *ValuePtr = m_builder.CreateAlloca(Type::Int128Ty, nullptr);
	m_builder.CreateStore(llvm::ConstantInt::get(Type::Int128Ty, 0), ValuePtr);

	// inOff
	llvm::Value *inOffPtr = m_builder.CreateAlloca(Type::Int32Ty, nullptr);
	m_builder.CreateStore(m_builder.getInt32(0x31d19166), inOffPtr);	// bytes4(keccak256("deployBytecode()"))


	// auto gas = m_builder.CreateCall(Func_getGasLeft, {});

	// Depolyer.deployBytecode() // view
	m_builder.CreateCall(Func_call, 
		{m_builder.getInt64(2300), calleeAddrPtr, 
		m_builder.CreateBitCast(ValuePtr, Type::Int32PtrTy), m_builder.CreateBitCast(inOffPtr, Type::Int8PtrTy), m_builder.getInt32(32)}); // gas > 800 for SLOAD at least
	

	auto outOff  = m_builder.CreateAlloca(Type::Int8Ty, nullptr);
	auto outSize = m_builder.getInt32(runtimeCodeSize);
	
	m_builder.CreateCall(Func_returnDataCopy, {outOff, m_builder.getInt32(0), outSize});

	// m_builder.CreateGEP(conArray, indicesRef, "spPtr")
	m_builder.CreateCall(Func_finish, {m_builder.CreateGEP(outOff, m_builder.getInt8(32)), outSize});
	RtReturnInst->eraseFromParent();
	module->getNamedGlobal("runtimeCode")->eraseFromParent();
	return true;
}
bool ProxyWrapperPass::doFinalization(llvm::Module&){return false;}


 

/* prevent from [Unchecked send] */
class EstimateVulSendPass: public llvm::FunctionPass
{
	static char ID;
	int vulPC;

public:
	EstimateVulSendPass(int const _vulPC):
		llvm::FunctionPass(ID),
		vulPC(_vulPC)
	{}
	// bool detect_unused_return_values(llvm::Function& _func, );
	virtual bool runOnFunction(llvm::Function& _func) override;
	using llvm::FunctionPass::doFinalization;
	virtual bool doFinalization(llvm::Module& _module) override;
};

char EstimateVulSendPass::ID = 6;

// bool EstimateVulSendPass::detect_unused_return_values(llvm::Function& _func, ) 
// {
	// return true;
// }

bool EstimateVulSendPass::runOnFunction(llvm::Function& _func) 
{	
	if (_func.getName() != "main")	return false;
	llvm::errs()<< "=========== [" << _func.getName() << "]optimize::estimate unsafe send =======================\n";
	bool modified = false;
	
	// side effects function -> dispatcher
	std::vector<llvm::Instruction*> vulSendRetList;

	for (auto _it = _func.begin(); _it != _func.end(); ++_it) {
		auto _bbPtr = &(*_it);
		for (auto it = _bbPtr->begin(); it != _bbPtr->end(); ++it)
		{
			auto& inst = *it;

			// if (!detect_unused_return_values(_func, it))  continue;
			if (!inst.hasMetadata(c_bytecodePC)) continue;
			auto _meta = llvm::cast<llvm::ValueAsMetadata>(inst.getMetadata(c_bytecodePC)->getOperand(0))->getValue();
			auto pc = llvm::dyn_cast<llvm::ConstantInt>(_meta)->getSExtValue() ;
			if (pc != vulPC)	continue;
			
			// llvm::errs() << "Found " << inst.getOperand(5)->getName();

			auto operandsNum = inst.getNumOperands();
			if (operandsNum == 6 
				&& inst.getOperand(operandsNum-1)->getName() == "ethereum.call") {
				vulSendRetList.push_back(&(*it));
				llvm::errs() << "vulSended. " << "\n";
			}
		}
	}

	for (auto retReg : vulSendRetList) {
		IRBuilder m_builder(retReg->getNextNonDebugInstruction());
		llvm::Value *_cond = m_builder.CreateICmpNE(retReg, m_builder.getInt32(0), "new_ret_check");
		llvm::SplitBlockAndInsertIfThen(_cond, retReg->getNextNonDebugInstruction()->getNextNonDebugInstruction(), true);
		llvm::errs()<< "Protect One CAll\n";
		modified = true;
	}
	return modified;
}
bool EstimateVulSendPass::doFinalization(llvm::Module&){return false;}

/* prevent from [vulnerable origin] */
class EstimateOriginPass: public llvm::FunctionPass
{
	static char ID;
	int vulPC;

public:
	EstimateOriginPass(int const _vulPC):
		llvm::FunctionPass(ID),
		vulPC(_vulPC)
	{}
	virtual bool runOnFunction(llvm::Function& _func) override;
	using llvm::FunctionPass::doFinalization;
	virtual bool doFinalization(llvm::Module& _module) override;
};

char EstimateOriginPass::ID = 7;

bool EstimateOriginPass::runOnFunction(llvm::Function& _func) 
{	
	if (_func.getName() != "main")	return false;
	bool modified = false;

	// collect all tx.origin value
	std::vector<llvm::Instruction*> icmpInstList, sloadInstList;
	std::vector<code_iterator*> patchPos;
	
	for (auto _it = _func.begin(); _it != _func.end(); ++_it) {
		auto _bbPtr = &(*_it);
		for (auto it = _bbPtr->begin(); it != _bbPtr->end(); ++it)
		{
			auto& inst = *it;

			if (inst.getOpcode() == llvm::Instruction::ICmp && inst.getNumOperands() == 2)
				icmpInstList.push_back(&(*it));
			
			else if (inst.getNumOperands() == 3
				&& inst.getOperand(2)->getName() == "ethereum.storageLoad") {
				sloadInstList.push_back(&(*std::next(it, 5)));

				auto& instt = *std::next(it, 5);
				if (instt.hasMetadata(c_bytecodePC))
				{
					auto _meta = llvm::cast<llvm::ValueAsMetadata>(instt.getMetadata(c_bytecodePC)->getOperand(0))->getValue();
					auto pc = llvm::dyn_cast<llvm::ConstantInt>(_meta)->getSExtValue() ;
					llvm::errs() << "SLOAD, PC=" << pc << "\n";
				}
			}
		}
	}
	if (sloadInstList.size() == 0) return false;

	auto module = _func.getParent();
	llvm::Function * Func_getCaller = module->getFunction("ethereum.getCaller");

	int _idx = 0;
	for (auto _it = _func.begin(); _it != _func.end(); ++_it, ++_idx) {
		auto _bbPtr = &(*_it);
		for (auto it = _bbPtr->begin(); it != _bbPtr->end(); ++it)
		{
			auto& inst = *it;
			if (inst.getNumOperands() == 2
				&& inst.getOperand(1)->getName() == "ethereum.getTxOrigin") {
				for (auto icmpInst : icmpInstList) {
					llvm::Value* lhs = icmpInst->getOperand(0);
					llvm::Value* rhs = icmpInst->getOperand(1);
					llvm::Value* originVal = llvm::cast<llvm::Value>(std::next(it, 7));
					if (lhs == originVal || rhs == originVal)
					{
						if (inst.hasMetadata(c_bytecodePC))
						{
							auto _meta = llvm::cast<llvm::ValueAsMetadata>(inst.getMetadata(c_bytecodePC)->getOperand(0))->getValue();
							auto pc = llvm::dyn_cast<llvm::ConstantInt>(_meta)->getSExtValue();
							llvm::errs() << "tx.origin, PC=" << pc << "\n";
						}
						// replace msg.origin with tx.origin
						auto call = llvm::CallInst::Create(Func_getCaller, {inst.getOperand(0)});
						llvm::ReplaceInstWithInst(_bbPtr->getInstList(), it, call);
						modified = true;
						llvm::errs() << "replace one tx.origin => msg.sender\n";
					}
				}
			}
		}
	}


	// for (auto _it = _func.begin(); _it != _func.end(); ++_it) {
	// 	auto _bbPtr = &(*_it);
	// 	for (auto it = _bbPtr->begin(); it != _bbPtr->end(); ++it)
	// 	{
	// 		auto& inst = *it;

	// 		// if (!inst.hasMetadata(c_bytecodePC)) continue;
	// 		// auto _meta = llvm::cast<llvm::ValueAsMetadata>(inst.getMetadata(c_bytecodePC)->getOperand(0))->getValue();
	// 		// auto pc = llvm::dyn_cast<llvm::ConstantInt>(_meta)->getSExtValue() ;
	// 		// if (pc != vulPC)	continue;

	// 		auto operandsNum = inst.getNumOperands();
	// 		if (operandsNum == 2
	// 			&& inst.getOperand(1)->getName() == "ethereum.getTxOrigin") {
				
	// 			// replace msg.origin with tx.origin
	// 			auto call = llvm::CallInst::Create(Func_getCaller, {inst.getOperand(0)});
	// 			llvm::ReplaceInstWithInst(_bbPtr->getInstList(), it, call);
	// 			modified = true;
	// 			llvm::errs() << "replace one tx.origin => msg.sender\n";
	// 		}
	// 	}
	// }
	return modified;
}
bool EstimateOriginPass::doFinalization(llvm::Module&){return false;}

/* prevent from [use SafeMath] */
class UseSafeMathPass: public llvm::FunctionPass
{
	static char ID;
	int vulPC;


public:
	UseSafeMathPass(int const _vulPC):
		llvm::FunctionPass(ID),
		vulPC(_vulPC)
	{}
	bool detectBug(llvm::Function& _func, llvm::BasicBlock* _parent);
	virtual bool runOnFunction(llvm::Function& _func) override;
	using llvm::FunctionPass::doFinalization;
	virtual bool doFinalization(llvm::Module& _module) override;
};

char UseSafeMathPass::ID = 8;

bool UseSafeMathPass::detectBug(llvm::Function& _func, llvm::BasicBlock* _parent)
{
	// traverse the CFG
	std::vector<llvm::BasicBlock*> visited;
	std::stack<llvm::BasicBlock*> fifo;
	fifo.push(_parent);
	while (fifo.size() > 0)
	{
		llvm::BasicBlock* dnode = fifo.top();
		fifo.pop();
		for (auto it = dnode->begin(); it != dnode->end(); ++it)
		{
			auto& inst = *it;
			if ((inst.getNumOperands() == 3 && inst.getOperand(2)->getName() == "ethereum.storageStore")
				|| (inst.getNumOperands() == 6 && inst.getOperand(5)->getName() == "ethereum.call"))
				return true; // one extCall depends on Storage
		}
		llvm::Instruction* TInst = dnode->getTerminator();
		for (unsigned I = 0, NSucc = TInst->getNumSuccessors(); I < NSucc; ++I)
		{	
			llvm::BasicBlock* successor = TInst->getSuccessor(I);
			if (std::find(visited.begin(), visited.end(), successor) == visited.end())
			{
				fifo.push(successor); // never visited before
				visited.push_back(successor);
			}
		}
	}
}

bool UseSafeMathPass::runOnFunction(llvm::Function& _func) 
{	
	if (_func.getName() != "main")	return false;
	llvm::errs()<< "=========== [" << _func.getName() << "]optimize:: SafeMath ===========================\n";
	bool modified = false;
	auto module = _func.getParent();

	for (llvm::BasicBlock &_bb : _func) {
		for (auto it = _bb.begin(); it != _bb.end(); ++it)
		{
			auto& inst = *it;
			
			// if (!inst.hasMetadata(c_bytecodePC)) continue;
			// auto _meta = llvm::cast<llvm::ValueAsMetadata>(inst.getMetadata(c_bytecodePC)->getOperand(0))->getValue();
			// auto pc = llvm::dyn_cast<llvm::ConstantInt>(_meta)->getSExtValue() ;
			// if (pc != vulPC)	continue;
			
			if (!detectBug(_func, inst.getParent())) continue;
			
			std::string opcodeName = inst.getName();
			if (opcodeName.rfind("unsafe", 0) == 0) 
			{	
				llvm::Function* func = nullptr;	
				switch (inst.getOpcode())
				{
					case llvm::Instruction::Add: {
						func = Arith256::getSafeMathAddFunc(*module);
						llvm::errs() << "detect add## " << opcodeName << "\n";
						break;
					}				
					case llvm::Instruction::Sub: {
						func = Arith256::getSafeMathSubFunc(*module);
						llvm::errs() << "detect sub## " << opcodeName << "\n";
						break;
					}
					case llvm::Instruction::Mul: {
						func = Arith256::getSafeMathMulFunc(*module);
						llvm::errs() << "detect mul## " << opcodeName << "\n";
						break;
					}	
					case llvm::Instruction::UDiv: {
						func = Arith256::getSafeMathUDivFunc(*module);
						break;
					}
					// case llvm::Instruction::SDiv: {
					// 	func = Arith256::getSafeMathSDivFunc(*module);
					// 	break;
					// }				
				}
				if (func != nullptr)
				{
					auto call = llvm::CallInst::Create(func, {inst.getOperand(0), inst.getOperand(1)});
					// llvm::ReplaceInstWithInst(&inst, call);
					llvm::ReplaceInstWithInst(_bb.getInstList(), it, call);
					modified = true;
				}
			}
		}
	}
	return modified;
}

bool UseSafeMathPass::doFinalization(llvm::Module&){return false;}

} // end of passes


/* replace calldata to Klee size */
class KleePass: public llvm::FunctionPass
{
	static char ID;

public:
	KleePass():
		llvm::FunctionPass(ID)
	{}
	virtual bool runOnFunction(llvm::Function& _func) override;
	using llvm::FunctionPass::doFinalization;
	virtual bool doFinalization(llvm::Module& _module) override;
};

char KleePass::ID = 9;

bool KleePass::runOnFunction(llvm::Function& _func) 
{	
	llvm::errs()<< "======================= my optimize::estimate dead storage ===========================\n";
	
	auto modified = false;
	auto module = _func.getParent();
	for (llvm::BasicBlock &_bb : _func) {
		for (auto it = _bb.begin(); it != _bb.end(); ++it)
		{
			auto& inst = *it;
			auto operandsNum  = inst.getNumOperands();
			if (!(inst.getOpcode() == llvm::Instruction::Call && operandsNum == 4)) continue;
		
			if (inst.getOperand(3)->getName() == "ethereum.callDataCopy") {
				llvm::Value* dst_offset = inst.getOperand(0);
				llvm::Value* src_offset = inst.getOperand(1);
				llvm::Value* size   = inst.getOperand(2);

				// llvm::Instruction* klee_sym = llvm::AllocaInst::Create();
				// llvm::ReplaceInstWithInst(_bb.getInstList(), it, klee_sym);
			}
		}
	}
	return modified;
}

bool KleePass::doFinalization(llvm::Module&){return false;}


/* A New PASS */
class calldataCopyPass: public llvm::FunctionPass
{
	static char ID;

public:
	calldataCopyPass():
		llvm::FunctionPass(ID)
	{}
	virtual bool runOnFunction(llvm::Function& _func) override;
	using llvm::FunctionPass::doFinalization;
	virtual bool doFinalization(llvm::Module& _module) override;
};

char calldataCopyPass::ID = 10;

bool calldataCopyPass::runOnFunction(llvm::Function& _func) 
{	
	if (_func.getName() != "main")	return false;
	llvm::errs()<< "======================= simplify the calldatacopy ===========================\n";
	auto modified = false;
	llvm::Module *module = _func.getParent();
	llvm::Function *Func_callDataCopy = module->getFunction("ethereum.callDataCopy");

	// auto module = _func.getParent();
	for (llvm::BasicBlock &_bb : _func) {
		for (llvm::BasicBlock::iterator it = _bb.begin(); it != _bb.end(); ++it)
		{
			auto& inst = *it;
			auto operandsNum  = inst.getNumOperands();
			if (!
			    (inst.getOpcode() == llvm::Instruction::Call 
				&& operandsNum == 4
				&& inst.getOperand(3)->getName() == "ethereum.callDataCopy")) 
				continue;

			// llvm::errs() << "calldata copy\n";
			llvm::Value* dst = inst.getOperand(0);
			llvm::Value* src_offset  = inst.getOperand(1);
			llvm::Value* _  = inst.getOperand(2);

			llvm::Instruction* reverse = inst.getNextNonDebugInstruction()->getNextNonDebugInstruction();
			if (reverse->getName() != ".reverse")	continue;


			llvm::Instruction* next_ins = reverse->getNextNonDebugInstruction();
			llvm::LShrOperator* lshr = nullptr;
			if ((lshr = llvm::dyn_cast<llvm::LShrOperator>(next_ins))
				&& llvm::dyn_cast<llvm::ConstantInt>(lshr->getOperand(1))->getSExtValue() == 224)
			{
				llvm::errs() << "found one\n";
				// auto call = llvm::CallInst::Create(Func_callDataCopy, {dst, src_offset, llvm::ConstantInt::get(Type::Int32Ty, 4)});
				inst.setOperand(2, llvm::ConstantInt::get(Type::Int32Ty, 4));
				// it ++;
				// it ++;
				// it ++;
				// auto m_builder = IRBuilder{&inst};
				// llvm::Value *sig = m_builder.CreateAlloca(Type::Int32Ty);
				// m_builder.CreateBitCast(sig, Type::)
				// llvm::Value *Data[4];
				// for (size_t I = 0; I < 4; ++I) {
				// 	Data[I] = m_builder.CreateShl(, 248 - I * 16);
				// }
				// llvm::Value *Result = m_builder.CreateOr(Data[0], Data[1]);
				// for (size_t I = 2; I < 32; ++I) {
				// 	Result = m_builder.CreateOr(Result, Data[I]);
				// }
				// llvm::ReplaceInstWithValue(_bb.getInstList(), it, reverse);
				return modified;
			}
			else if (auto udiv = llvm::dyn_cast<llvm::UDivOperator>(next_ins))
			{	
				llvm::Instruction *And = next_ins->getNextNonDebugInstruction();
				if (And->getOpcode() == llvm::Instruction::And 
					&& llvm::dyn_cast<llvm::ConstantInt>(And->getOperand(0))->getZExtValue() == 0xffffffff)
				{
					// llvm::errs() << "deteted\n";
					/*
						call void @ethereum.callDataCopy(i8* %7, i32 %5, i32 4) ;; inst
						%8 = load i256, i256* %6
						%.reverse = call i256 @solidity.bswapi256(i256 %8)
						%unsafe = call i256 @evm.udiv.i256(i256 %.reverse, i256 26959946667150639794667015087019630673637144422540572481103610249216)
						%9 = and i256 4294967295, %unsafe
						====>

						call void @ethereum.callDataCopy(i8* %7, i32 %5, i32 4)
						%8 = load i256, i256* %6
						%.reverse = call i256 @solidity.bswapi256(i256 %8)
						%9 = lshr i256 %.reverse, 224
					*/
					inst.setOperand(2, llvm::ConstantInt::get(Type::Int32Ty, 4));
					llvm::BinaryOperator *lshr = llvm::BinaryOperator::Create(llvm::Instruction::LShr, reverse, llvm::ConstantInt::get(Type::Int256Ty, 224));
					
					// auto m_builder = IRBuilder{reverse};
					// llvm::Value* sig = m_builder.CreateLShr(reverse, 224, "sig");
					it ++;
					it ++;
					it ++;
					it ++;
					llvm::ReplaceInstWithInst(_bb.getInstList(), it, lshr);
					// llvm::ReplaceInstWithValue(_bb.getInstList(), it, sig);
					return modified;
				}

			}
		}
	}
	return modified;
}
bool calldataCopyPass::doFinalization(llvm::Module&){return false;}



/* remove unused functions */
class freeFuncPass: public llvm::FunctionPass
{
	static char ID;

public:
	freeFuncPass():
		llvm::FunctionPass(ID)
	{}
	virtual bool runOnFunction(llvm::Function& _func) override;
	using llvm::FunctionPass::doFinalization;
	virtual bool doFinalization(llvm::Module& _module) override;
};

char freeFuncPass::ID = 11;

bool freeFuncPass::runOnFunction(llvm::Function& _func) 
{	
	llvm::errs() << _func.getName();
	// if (_func.getName() != "main") return false;	
	bool modified = false;

	return modified;
}

bool freeFuncPass::doFinalization(llvm::Module&){return false;}


bool prepare(llvm::Module& _module, Options options)
{
	auto pm = llvm::legacy::PassManager{};
	// pm.add(new CodecopyPass{});
	
	if (options.isRtcode && options.enableKlee)		        pm.add(new KleePass{});  // build symbolic input
	
	if (options.isRtcode && options.enableCheckReentrancy)	pm.add(new EstimateReenrancePass{}); //sGuard
	if (options.isRtcode && options.enableDetectSuicide)	pm.add(new DetectSuicidePass{}); 
	if (options.isRtcode && options.enableEOAonly )			pm.add(new EstimateRollbackPass{});
	if (options.isRtcode && options.enableCheckSend)		pm.add(new EstimateVulSendPass{options.vulpc});
	if (options.isRtcode && options.enableRmOrigin)		    pm.add(new EstimateOriginPass{options.vulpc}); // sGuard
	if (options.isRtcode && options.enableSafeMath)		    pm.add(new UseSafeMathPass{options.vulpc});	  // sGuard
	
	if ((!options.isRtcode) && options.enableUpgrade)		pm.add(new ProxyWrapperPass{});
	if (options.isRtcode) pm.add(new calldataCopyPass{});

	pm.add(new LowerEVMPass{}); //1

	pm.add(llvm::createReassociatePass());

	pm.add(llvm::createCFGSimplificationPass());//1

	pm.add(llvm::createDeadCodeEliminationPass());
	
    pm.add(llvm::createReassociatePass());
    
	pm.add(llvm::createFunctionInliningPass(2, 2, false));

	pm.add(llvm::createAggressiveDCEPass());
	
	pm.add(llvm::createPromoteMemoryToRegisterPass());//2

	return pm.run(_module);
}

// bool prepare(llvm::Module& _module, Options options)
// {
// 	auto pm = llvm::legacy::PassManager{};
// 	// pm.add(new CodecopyPass{});
	
// 	if (options.isRtcode && options.enableKlee)		        pm.add(new KleePass{});  // build symbolic input
	
// 	if (options.isRtcode && options.enableCheckReentrancy)	pm.add(new EstimateReenrancePass{}); //sGuard
// 	if (options.isRtcode && options.enableDetectSuicide)	pm.add(new DetectSuicidePass{}); 
// 	if (options.isRtcode && options.enableEOAonly )			pm.add(new EstimateRollbackPass{});
// 	if (options.isRtcode && options.enableCheckSend)		pm.add(new EstimateVulSendPass{options.vulpc});
// 	if (options.isRtcode && options.enableRmOrigin)		    pm.add(new EstimateOriginPass{options.vulpc}); // sGuard
// 	if (options.isRtcode && options.enableSafeMath)		    pm.add(new UseSafeMathPass{options.vulpc});	  // sGuard
	
// 	if ((!options.isRtcode) && options.enableUpgrade)		pm.add(new ProxyWrapperPass{});
// 	// if (options.isRtcode) pm.add(new calldataCopyPass{});

// 	pm.add(new LowerEVMPass{}); //1

// 	// pm.add(llvm::createReassociatePass());

// 	// pm.add(llvm::createCFGSimplificationPass());//1

// 	// pm.add(llvm::createDeadCodeEliminationPass());
	
//     // pm.add(llvm::createReassociatePass());
    
// 	// pm.add(llvm::createFunctionInliningPass(2, 2, false));

// 	// pm.add(llvm::createAggressiveDCEPass());
	
// 	// pm.add(llvm::createPromoteMemoryToRegisterPass());//2

// 	return pm.run(_module);
// }

}
}
}

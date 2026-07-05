#include "WASMCompiler.h"

#include <fstream>
#include <chrono>
#include <sstream>
#include <map>
#include <set>
#include <queue>

#include "preprocessor/llvm_includes_start.h"
#include <llvm/IR/CFG.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/IntrinsicInst.h>
#include <llvm/Support/Alignment.h>
#include <llvm/Support/raw_os_ostream.h>
#include <llvm/ADT/APInt.h>
// #include <llvm/Transforms/Utils/BasicBlockUtils.h>

#include "preprocessor/llvm_includes_end.h"

// #include "BinaryTrans.h"
#include "Instruction.h"
#include "Type.h"
// #include "Memory.h"
// #include "Ext.h"
// #include "GasMeter.h"
#include "Utils.h" // for std:cerr
// #include "Endianness.h"
#include "Arith256.h"
#include "EEIModule.h"
#include "NEARModule.h"

#ifndef __has_cpp_attribute
#define __has_cpp_attribute(x) 0
#endif

#if __has_cpp_attribute(fallthrough)
#define FALLTHROUGH [[fallthrough]]
#elif __has_cpp_attribute(clang::fallthrough)
#define FALLTHROUGH [[clang::fallthrough]]
#elif __has_cpp_attribute(gnu::fallthrough)
#define FALLTHROUGH [[gnu::fallthrough]]
#else
#define FALLTHROUGH
#endif

namespace dev
{
	namespace eth
	{
		namespace trans
		{
			static const auto c_signed = "signed";
			static const auto c_destIdxLabel = "destIdx";
			static const auto c_bytecodePC = "pc";


			int64_t splitCode(code_iterator _begin, code_iterator _end, uint64_t &RETURNPC) {
				bool existCodeCopy = false;
				bool existReturn = false;

				code_iterator begin = _begin; // begin of current block
				// std::cerr << "------------------------------\n";
				for (code_iterator curr = begin, next = begin; curr != _end; curr = next)
				{
					next = skipPushDataAndGetNext(curr, _end);
					// std::cerr << std::hex << (int)(*curr) << " ";
					switch (Instruction(*curr)) {
					case Instruction::CODECOPY:
						existCodeCopy = true;
						// std::cerr << "CodeCopy\n";
						break;

					case Instruction::RETURN:
						if (existCodeCopy) {
							existReturn = true;
							// std::cerr << "RETURN\n";
							RETURNPC = curr - begin;
						}
						break;

					case Instruction::PUSH1:
						if (existReturn) 
							return curr - begin; // success split the runtime code=code[curr:]; deploy code=code[:curr]
						break;
					default:
						break;
					}
				}
				return -1;
			}

      WASMCompiler::WASMCompiler(llvm::Module &M, std::vector<uint8_t> &B, uint64_t PC) : 
        Module(M), Bytecode(B), Context(M.getContext()), RetPC(PC) {
          Type::init(Module.getContext());
          NearEnv = new NEARModule(M);
          // EwasmEnv = new EEIModule(Module);
          // Arith = new Arith256(Module);
      }

      WASMCompiler::WASMCompiler(llvm::Module &M, std::vector<uint8_t> &B, uint64_t PC, llvm::StringRef RtWasmCode) : 
        Module(M), Bytecode(B), Context(M.getContext()), RetPC(PC), RtWasmCode(RtWasmCode) {
          Type::init(Module.getContext());
          NearEnv = new NEARModule(M);
          // EwasmEnv = new EEIModule(Module);
          // Arith = new Arith256(Module);
        }

			std::vector<BasicBlock> WASMCompiler::createBasicBlocks(
        code_iterator _codeBegin, code_iterator _codeEnd) {
				/// Helper function that skips push data and finds next iterator (can be the end)
				auto skipPushDataAndGetNext = [](code_iterator _curr, code_iterator _end)
				{
					static const auto push1 = static_cast<size_t>(Instruction::PUSH1);
					static const auto push32 = static_cast<size_t>(Instruction::PUSH32);
					size_t offset = 1;
					if (*_curr >= push1 && *_curr <= push32)
						offset += std::min<size_t>(*_curr - push1 + 1, (_end - _curr) - 1);
					return _curr + offset;
				};

				std::vector<BasicBlock> blocks;

				bool isDead = false;
				auto begin = _codeBegin; // begin of current block

				for (auto curr = begin, next = begin; curr != _codeEnd; curr = next) {

					next = skipPushDataAndGetNext(curr, _codeEnd);

					if (isDead) {
						if (Instruction(*curr) == Instruction::JUMPDEST)
						{
							isDead = false;
							begin = curr;
						}
						else
							continue;
					}

					bool isEnd = false;
					switch (Instruction(*curr)) {
					case Instruction::JUMP:
					case Instruction::RETURN:
					case Instruction::REVERT:
					case Instruction::STOP:
					case Instruction::SUICIDE:
						isDead = true;
						FALLTHROUGH;
					case Instruction::JUMPI:
						isEnd = true;
						break;

					default:
						break;
					}

					assert(next <= _codeEnd);
					if (next == _codeEnd || Instruction(*next) == Instruction::JUMPDEST)
						isEnd = true;

					if (isEnd) {
						auto beginIdx = begin - _codeBegin;
						blocks.emplace_back(beginIdx, begin, next, m_mainFunc);
						begin = next;
					}
				}

				return blocks;
			}

			void WASMCompiler::resolveJumps(std::vector<BasicBlock> &blocks)
			{

				std::map<uint64_t, BasicBlock*> idxMap;
				for (size_t i = 0; i < blocks.size(); i++) {
					idxMap[blocks[i].firstInstrIdx()] = &blocks[i];
				}

				auto jumpTable = llvm::cast<llvm::SwitchInst>(m_jumpTableBB->getTerminator());
				// for (auto it = m_mainFunc->begin()), end = m_mainFunc->end(); it != end; ++it)
				for (auto it = std::next(m_mainFunc->begin()), end = std::prev(m_mainFunc->end(), 3); it != end; ++it)
				{
					auto nextBlockIter = it;
					++nextBlockIter; // If the last code block, that will be "stop" block.
					auto currentBlockPtr = &(*it);
					auto nextBlockPtr = &(*nextBlockIter);

					llvm::Instruction *term = it->getTerminator();
					llvm::BranchInst *jump = nullptr;

					if (!term)
					{
						// Block may have no terminator if the next instruction is a JUMPDEST
						IRBuilder{currentBlockPtr}.CreateBr(nextBlockPtr);
					}
					else if ((jump = llvm::dyn_cast<llvm::BranchInst>(term)) && jump->getSuccessor(0) == m_jumpTableBB)
					{
						auto destIdx = llvm::cast<llvm::ValueAsMetadata>(jump->getMetadata(c_destIdxLabel)->getOperand(0))->getValue();
						// std::cerr << "1 end\n";
						if (auto constant = llvm::dyn_cast<llvm::ConstantInt>(destIdx))
						{
							// If destination index is a constant do direct jump to the destination block.
							// std::cerr << "[resolveJumps] Static Jump target. at" <<  llvm::dyn_cast<llvm::ConstantInt>(constant)->getSExtValue() << "\n";
							auto bb = jumpTable->findCaseValue(constant)->getCaseSuccessor();
							jump->setSuccessor(0, bb);

							llvm::Instruction *prev_ins = jump->getPrevNonDebugInstruction();
							while (prev_ins != nullptr)
							{
								if (auto store = llvm::dyn_cast<llvm::StoreInst>(prev_ins))
								{
									if (store->getOperand(1)->getName() == "target")
									{
										store->eraseFromParent();
										break;
									}
								}
								prev_ins = prev_ins->getPrevNonDebugInstruction();
							}

							// std::cerr << "[resolveJumps] Static Jump End " << bb << "\n";
						}
						if (jump->isConditional())
							jump->setSuccessor(1, &(*nextBlockIter)); // Set next block for conditional jumps
					}
				}
			}

			void WASMCompiler::resolve_phi(std::vector<BasicBlock> &blocks, std::map<BasicBlock *, std::vector<llvm::Value *> *> &stkMap)
			{
				// std::map<uint64_t, BasicBlock *> idxMap;
				// for (int i = 0; i < blocks.size(); i++)
				// {
				// 	idxMap[blocks[i].firstInstrIdx()] = &blocks[i];
				// }

				std::vector<BasicBlock *> dfs = {&blocks[0]};

				std::set<BasicBlock *> solved;
				for (auto itr = blocks.begin(); itr != blocks.end(); ++itr)
				{
					BasicBlock bb = *itr;
					if (bb.get_stk_in() == 0)
						solved.insert(&bb);
				}

				std::cerr << "begin->\n";
				for (size_t idx = 0; idx < blocks.size(); ++idx)
				{
					BasicBlock *bb_ptr = &blocks[idx];
					std::cerr << "BB " << std::hex << " " << bb_ptr->firstInstrIdx() << "; #phi=" << bb_ptr->get_phi_nodes_ptr()->size() << "\n";
					for (auto _itr = bb_ptr->bbs_from.begin(); _itr != bb_ptr->bbs_from.end(); ++_itr)
					{
						BasicBlock *bb_from_ptr = *_itr;
						std::cerr << "    <--- BB " << std::hex << " " << bb_from_ptr->firstInstrIdx() << " out=" << bb_from_ptr->get_stk_out() << "\n";
						// std::cerr << " #" << bb_ptr->get_phi_nodes_ptr()->size() << "; phis[" << bb_ptr->get_stk_in() << "]" << "\n";
						for (uint16_t i = 1; i <= bb_ptr->get_stk_in(); ++i)
						{
							llvm::PHINode *unsolved_phi = *(bb_ptr->get_phi_nodes_ptr()->end() - i);

							llvm::Value *incoming_val = *(stkMap[bb_from_ptr]->end() - i);
							unsolved_phi->addIncoming(incoming_val, bb_from_ptr->llvm());
						}
					}
					std::cerr << "-----------\n";
				}
			}

			void WASMCompiler::compileMain(
        std::vector<uint8_t> &ExeCode, 
        const std::string& Name) {
        
        IRBuilder IRB(Context);
				EEIModule eei(IRB, &Module);
				eei.init();

				Arith256 arith(IRB, &Module);
        
				// std::cerr << "Create Main Function\n";
				auto *FuncTy = llvm::FunctionType::get(Type::Void, false);
				m_mainFunc = llvm::Function::Create(FuncTy, llvm::Function::ExternalLinkage, Name, &Module);
				m_mainFunc->addFnAttr(llvm::Attribute::get(Context, "wasm-export-name", Name));

				auto mainBB = llvm::BasicBlock::Create(Context, "main_Entry", m_mainFunc);

				// std::cerr << "Create Basic Blocks " << " rtCodeSize= " << rtCodeSize << "\n";
				std::vector<BasicBlock> blocks = createBasicBlocks(ExeCode.data(), ExeCode.data() + ExeCode.size());

				// std::map<uint64_t, BasicBlock*> idxMap;
				// for (int i = 0; i < blocks.size(); i++) {
				// 	idxMap[blocks[i].firstInstrIdx()] = &blocks[i];
				// 	std::cerr << blocks[i].firstInstrIdx() << " ";
				// }
				// std::cerr << "\n";

				// Module.getOrInsertGlobal("mstk", llvm::ArrayType::get(Type::Int256Ty, 1024));
				// llvm::GlobalVariable *conArray = Module.getNamedGlobal("mstk");

				// llvm::ConstantAggregateZero* _constinitt = llvm::ConstantAggregateZero::get(Type::Int256Ty);
				// llvm::GlobalVariable *cconArray = new llvm::GlobalVariable(*module, llvm::ArrayType::get(Type::Int256Ty, 128), false,
				// 										llvm::GlobalVariable::PrivateLinkage,
				// 										_constinitt, "mstkkkk");
				// cconArray->setUnnamedAddr(llvm::GlobalVariable::UnnamedAddr::Global);

				llvm::ConstantAggregateZero *_constinit = llvm::ConstantAggregateZero::get(llvm::ArrayType::get(Type::Int256Ty, 512));
				llvm::GlobalVariable *conArray = new llvm::GlobalVariable(Module, llvm::ArrayType::get(Type::Int256Ty, 512), false,
																		  llvm::GlobalVariable::PrivateLinkage,
																		  _constinit, "mstk");
				conArray->setUnnamedAddr(llvm::GlobalVariable::UnnamedAddr::Global);

				llvm::GlobalVariable *targetPtr = new llvm::GlobalVariable(Module, Type::Int256Ty, false,
																		   llvm::GlobalVariable::PrivateLinkage,
																		   llvm::ConstantInt::get(Type::Word, 0), "target");
				targetPtr->setUnnamedAddr(llvm::GlobalVariable::UnnamedAddr::Global);
				targetPtr->setAlignment(llvm::MaybeAlign(256));

				llvm::GlobalVariable *_spPtr = new llvm::GlobalVariable(Module, Type::Int256Ty, false,
																		llvm::GlobalVariable::PrivateLinkage,
																		llvm::ConstantInt::get(Type::Word, 0), "spPtr");
				_spPtr->setUnnamedAddr(llvm::GlobalVariable::UnnamedAddr::Global);
				_spPtr->setAlignment(llvm::MaybeAlign(256));

				// Module.getOrInsertGlobal("target", Type::Int256Ty);
				// llvm::GlobalVariable *targetPtr = Module.getNamedGlobal("target");
				// targetPtr->setLinkage(llvm::GlobalValue::PrivateLinkage);
				// targetPtr->setAlignment(32);

				auto m_testBB = llvm::BasicBlock::Create(Context, "Test", m_mainFunc);

				m_jumpTableBB = llvm::BasicBlock::Create(Context, "JumpTable", m_mainFunc);
				auto m_abortBB = llvm::BasicBlock::Create(Context, "Abort", m_mainFunc);

        IRB.SetInsertPoint(m_jumpTableBB);
				// auto target = IRB.CreatePHI(Type::Word, blocks.size(), "target");

				// IRB.CreateSwitch(target, m_abortBB);
				IRB.CreateSwitch(IRB.CreateLoad(targetPtr), m_abortBB);

				// std::cerr << "SetInsertPoint mainBB\n";
				IRB.SetInsertPoint(mainBB);
				IRB.CreateBr(mainBB->getNextNode());

				m_exitBB = llvm::BasicBlock::Create(Context, "Exit", m_mainFunc);

				// int cnt = 0;
				bool not_slim = true;

				if (not_slim)
				{
					/*
					heavy mode
					recovery stack dependency via simulation.
					*/

					bool no_ctx = false;
					if (no_ctx) {	
						GlobalStack _stack(IRB, Module.getNamedGlobal("mstk"), Module.getNamedGlobal("spPtr"));
						for (auto itr = blocks.begin(); itr != blocks.end(); ++itr) {
							auto bb = *itr;
							// _stack.enableRegs = false;
							// compileBasicBlock(bb, indirectDsts, eei, arith, *module, _stack, rtCodeItr, rtCodeSize, EVMCode, RETURNPC);

							std::vector<llvm::Value *> depStk;
							_stack.enableRegs = false;

							IRB.SetInsertPoint(bb.llvm());
							for (uint32_t _ = 0; _ < bb.get_stk_in(); ++_)
							{
								depStk.push_back(_stack.pop()); // get item from GEP
							}

							_stack.enableRegs = false;
							for (auto itr = depStk.rbegin(); itr != depStk.rend(); itr++)
							{
								_stack.push(*itr); // push into the local stack
							}

							_stack.enableRegs = false; // use regs only for optimization
							compileBasicBlock(bb, eei, arith,  _stack);
							_stack.enableRegs = false;

							auto now_point = IRB.saveIP();
							if (auto term = bb.llvm()->getTerminator())
								IRB.SetInsertPoint(term); // Insert before terminator

							for (auto value : _stack.view())
								_stack.push(value); // store with GEP
							IRB.restoreIP(now_point);

							_stack.clearLocals();
						}
					}
					else {
						dry_run(blocks); // build cfg inside each basic block

						// deploy code
						GlobalStack _stack(IRB, Module.getNamedGlobal("mstk"), Module.getNamedGlobal("spPtr"));

						std::set<BasicBlock *> visited;
						std::queue<std::pair<BasicBlock *, std::vector<llvm::Value *> *>> waitingBlocks;
						std::vector<llvm::Value *> *root = new std::vector<llvm::Value *>;
						waitingBlocks.push(std::make_pair(&blocks[0], root));

						while (!waitingBlocks.empty())
						{
							// std::cerr << "going to analyze one block\n";
							BasicBlock *pbb = waitingBlocks.front().first;
							std::vector<llvm::Value *> *pstack = waitingBlocks.front().second;
							waitingBlocks.pop();

							if (visited.find(pbb) != visited.end()) {
								delete pstack;
								continue;

							} else
								visited.insert(pbb);

							if (pbb == nullptr) {
								// jump to an invalid block, such as INVALID
								continue;
							}

							// std::cerr << "set fallout edges\n";
							std::vector<llvm::Value *> depStk;
							// std::cerr << "Debug2:  pstack->size=" <<  pstack->size() << "; ptr= "<<pbb << "\n";
							IRB.SetInsertPoint(pbb->llvm());

							// std::cerr << "Debug:  pstack->size=" <<  pstack->size() << "\n";
							// if (Instruction(*pbb->begin()) != Instruction::JUMPDEST && pstack->size() > 0)
							if ((Instruction(*pbb->begin()) != Instruction::JUMPDEST || (pbb->bbs_from.size() == 1 && Instruction(*pbb->bbs_from[0]->begin()) == Instruction::JUMPI )) && pstack->size() > 0)
							{
								// std::cerr << "Starting with non-JUMPDEST\n";
								// fallout block
								for (auto itr = pstack->rbegin(); itr != pstack->rend() && depStk.size() < pbb->get_stk_in(); ++itr)
								{
									depStk.push_back(*itr);
								}
								// std::cerr << "seting fallout edges. depStk.size=" << depStk.size() << "in="<< pbb->get_stk_in() << "\n";
								// set up GEP index
								llvm::Value *tsp = IRB.CreateLoad(_stack.get_sp());
								IRB.CreateStore(IRB.CreateSub(tsp, llvm::ConstantInt::get(Type::Int256Ty, depStk.size())),
														_stack.get_sp());
							}
							// std::cerr << "ending set fallout edges\n";

							_stack.enableRegs = false;
							// std::cerr << "Step1. stk_in=" << pbb->get_stk_in() << "; depStk#="<< depStk.size() << "\n";
							if (depStk.size() < pbb->get_stk_in())
							{
								// not enough stack elements
								uint32_t threshold = pbb->get_stk_in() - (uint32_t)depStk.size();
								for (uint32_t _ = 0; _ < threshold; ++_)
								{
									depStk.push_back(_stack.pop()); // get item from GEP
								}
							}
							// std::cerr << "Step2. stk_in=" << pbb->get_stk_in() << "; depStk#="<< depStk.size() << "\n";

							// std::cerr << "generate local stack\n";
							_stack.enableRegs = true;
							for (auto itr = depStk.rbegin(); itr != depStk.rend(); itr++)
							{
								_stack.push(*itr); // push into the local stack
							}
							// std::cerr << "compiling... local stack elements#" << _stack.view().size() << "; stk_in=" << pbb->get_stk_in() << "; depStk#="<< depStk.size() << "\n";
							_stack.enableRegs = true;
							compileBasicBlock(*pbb, eei, arith, _stack);
							_stack.enableRegs = false;

							// maintain the LLVM registers produced from this basic block
							// std::vector<llvm::Value*> *vs = new std::vector<llvm::Value*>;
							// for (auto value : _stack.view())
							// 	vs->push_back(value);

							// std::cerr << "pbb->bbs_to.size() = " << pbb->bbs_to.size() << "\n";
							for (auto _pbb : pbb->bbs_to)
							{
								// analyze.
								std::vector<llvm::Value *> *vs = new std::vector<llvm::Value *>;
								for (auto value : _stack.view())
									vs->push_back(value);
								waitingBlocks.push(std::make_pair(_pbb, vs));
							}
							// std::cerr << "copied the _stack\n";

							_stack.enableRegs = false;
							auto now_point = IRB.saveIP();
							if (auto term = pbb->llvm()->getTerminator())
								IRB.SetInsertPoint(term); // Insert before terminator

							for (auto value : _stack.view())
								_stack.push(value); // store with GEP
							IRB.restoreIP(now_point);

							_stack.clearLocals();
							delete pstack;
						}

						for (uint32_t idx = 0; idx < blocks.size(); ++idx) {
							BasicBlock bb = blocks[idx];
							if (visited.find(&blocks[idx]) != visited.end())
								continue;
							visited.insert(&bb);

							std::vector<llvm::Value *> depStk;
							_stack.enableRegs = false;

							IRB.SetInsertPoint(bb.llvm());
							for (uint32_t _ = 0; _ < bb.get_stk_in(); ++_)
							{
								depStk.push_back(_stack.pop()); // get item from GEP
							}

							_stack.enableRegs = true;
							for (auto itr = depStk.rbegin(); itr != depStk.rend(); itr++)
							{
								_stack.push(*itr); // push into the local stack
							}

							_stack.enableRegs = true; // use regs only for optimization
							compileBasicBlock(bb, eei, arith, _stack);
							_stack.enableRegs = false;

							auto now_point = IRB.saveIP();
							if (auto term = bb.llvm()->getTerminator())
								IRB.SetInsertPoint(term); // Insert before terminator

							for (auto value : _stack.view())
								_stack.push(value); // store with GEP
							IRB.restoreIP(now_point);

							_stack.clearLocals();
						}
					}

					
					// std::cerr << "after compileBasicBlock\n";

					// std::cerr << "Total BB cnt = " << std::hex << blocks.size() << "  regsBB cnt = " << cnt << "\n";

					// auto outfile = "/home/toor/evmTrans/blockchain/tmpllvm.ll";
					// std::error_code ec;
					// llvm::raw_fd_ostream fout(llvm::StringRef(outfile), ec);
					// if (ec) {
					// 	llvm::raw_os_ostream cerr{std::cerr};
					// 	Module.print(cerr, nullptr);
					// } else {
					// 	Module.print(fout, nullptr);
					// }
					// std::cerr << "tmp LLVM IR dump success.\n";

					// 	stop execution, identical to return(0,0)
					IRB.SetInsertPoint(m_exitBB);

					// auto _t = IRB.CreateGEP(conArray, indicesRef, "spPtr"); // address to a[0]
					// auto sp = IRB.CreateLoad(_t); // stk[0]

					// auto _a = IRB.CreateAlloca(Type::Int256Ty, nullptr);
					// IRB.CreateStore(sp, _a);
					// targetPtr->setLinkage(llvm::GlobalValue::ExternalLinkage);
					// targetPtr->setLinkage(llvm::GlobalValue::CommonLinkage);

					// IRB.CreateStore(Constant::get(13), targetPtr);

					// GlobalStack sstack(IRB, conArray);
					// sstack.push(Constant::get(12));
					// sstack.push(Constant::get(13));

					// sstack.swap(0);
					// // sstack.dup(0);
					// // sstack.pop();
					// auto a = sstack.pop();
					// IRB.CreateStore(a, targetPtr);

					auto _adr = IRB.CreateAlloca(Type::Int256Ty, nullptr);
					IRB.CreateCall(eei.Func_finish, {IRB.CreateBitCast(_adr, Type::Int8PtrTy), IRB.getInt32(0)});

					/************************************************************************************/
					// llvm::Constant *conArray = Module.getOrInsertGlobal("array", llvm::ArrayType::get(Type::Int256Ty, 1024));
					// auto sp = IRB.CreateAlloca(Type::Int256Ty, nullptr);
					// IRB.CreateStore(Constant::get(1), sp);

					// IRB.CreateStore(v, gep);

					// auto br_con = IRB.CreateAlloca(Type::Int256Ty, nullptr);
					// auto br_adr = IRB.CreateSelect(IRB.CreateICmpEQ(IRB.CreateLoad(br_con), Constant::get(0)), llvm::BlockAddress::get(m_abortBB), llvm::BlockAddress::get(m_exitBB));

					// auto indirectBr = IRB.CreateIndirectBr(br_adr, 2);
					// indirectBr->addDestination(m_abortBB);
					// indirectBr->addDestination(m_exitBB);

					// auto jumpTable = llvm::cast<llvm::SwitchInst>(m_jumpTableBB->getTerminator());

					// auto dyPtr = IRB.CreateAlloca(Type::Int256Ty, nullptr);
					// auto _tab = IRB.CreateSwitch(IRB.CreateLoad(dyPtr), m_abortBB);
					// _tab->addCase(Constant::get(12), m_exitBB);

					// auto jumpTable = llvm::cast<llvm::SwitchInst>(m_jumpTableBB->getTerminator());
					// jumpTable->addCase(Constant::get(13), m_exitBB);

					/************************************************************************************/
					IRB.CreateUnreachable();

					IRB.SetInsertPoint(m_abortBB);
					auto addr = IRB.CreateAlloca(Type::Int256Ty, nullptr);
					IRB.CreateCall(
						eei.Func_revert,
						{IRB.CreateBitCast(addr, Type::Int8PtrTy),
						 IRB.getInt32(0)});
					IRB.CreateBr(m_exitBB);

					// std::cerr << "before resolveJumps\n";
					resolveJumps(blocks);
					// std::cerr << "after resolveJumps\n";
				}
				else
				{
					/*
					slim mode.
					analyze local stack and assign PHI nodes.
					*/

					dry_run(blocks); // build cfg inside each basic block

					// std::map<BasicBlock*> idxMap;
					// int a[10];
					// a[0] = 100;

					std::map<BasicBlock *, std::vector<llvm::Value *> *> stkMap;
					std::vector<std::pair<BasicBlock *, std::vector<llvm::Value *> *>> processed_bbs;

					// for (uint32_t i = 0;i < blocks.size(); ++i)
					// {
					// 	std::vector<llvm::Value*> *vs = new std::vector<llvm::Value*>;
					// 	stkMap[&blocks[i]] = vs;
					// }

					// insert necessary PHI nodes to generate IR
					GlobalStack _stack(IRB, Module.getNamedGlobal("mstk"), Module.getNamedGlobal("spPtr"));
					_stack.enableRegs = true;

					for (uint32_t idx = 0; idx < (uint32_t)blocks.size(); ++idx)
					// for (auto itr = blocks.begin(); itr != blocks.end(); ++itr)
					{
						BasicBlock *pbb = &blocks[idx];
						IRB.SetInsertPoint(pbb->llvm());

						std::cerr << "BB " << std::hex << pbb->firstInstrIdx() << " <- ctx#" << pbb->bbs_from.size() << "\n";
						for (uint32_t _ = 0; _ < pbb->get_stk_in(); ++_)
						{
							llvm::PHINode *PN = IRB.CreatePHI(Type::Int256Ty, (uint32_t)pbb->bbs_from.size(), "prev");
							pbb->add_phi(PN);
							std::cerr << "bb phi count = " << pbb->get_phi_nodes_ptr()->size() << "\n";

							_stack.push(llvm::cast<llvm::Value>(PN)); // push into the local stack
						}

						compileBasicBlock(*pbb, eei, arith, _stack);

						// std::cerr << "before\n";

						std::vector<llvm::Value *> *vs = new std::vector<llvm::Value *>;
						vs->assign(_stack.get_locals_ptr()->begin(), _stack.get_locals_ptr()->end());
						stkMap[pbb] = vs; // record the allocated LLVM registers

						// std::cerr << "end\n";

						_stack.clearLocals();
					}

					IRB.SetInsertPoint(m_exitBB);
					auto _adr = IRB.CreateAlloca(Type::Int256Ty, nullptr);
					IRB.CreateCall(eei.Func_finish, {IRB.CreateBitCast(_adr, Type::Int8PtrTy), IRB.getInt32(0)});
					IRB.CreateUnreachable();

					IRB.SetInsertPoint(m_abortBB);
					auto addr = IRB.CreateAlloca(Type::Int256Ty, nullptr);
					IRB.CreateCall(
						eei.Func_revert,
						{IRB.CreateBitCast(addr, Type::Int8PtrTy),
						 IRB.getInt32(0)});
					IRB.CreateBr(m_exitBB);

					std::cerr << "[-] Basic IR generated.\n";
					// resolve_phi

					resolve_phi(blocks, stkMap);
					std::cerr << "[-] Phi nodes solved.\n";

					// free stkMap
					for (auto iter = stkMap.begin(); iter != stkMap.end(); ++iter) {
						// cout << iter->first << " : " << iter->second << endl;
						delete iter->second;
					}
				}
				std::cerr << "IR Generated \n";
			}

			void WASMCompiler::cfg_to_dot(std::string &s, std::vector<BasicBlock> const &blocks, bool minimal = false)
			{
				return;
			}

			void WASMCompiler::dry_run_bb(BasicBlock const &bb, std::vector<uint64_t> &stack)
			{
				auto balanced_stk = [](std::vector<uint64_t> *stk, uint64_t out, uint64_t in)
				{
					uint8_t _;
					for (_ = 0; _ < out; ++_)
					{
						if (stk->empty())
							stk->push_back(0);
						stk->pop_back();
					}

					for (_ = 0; _ < in; ++_)
						stk->push_back(0);
				};

				// for each instruction
				for (code_iterator it = bb.begin(); it != bb.end(); ++it)
				{
					Instruction ins = Instruction(*it);
					uint32_t opcode = static_cast<uint32_t>(ins);

					uint8_t stk_out = std::get<1>(InstrMap[opcode]);
					uint8_t stk_in = std::get<2>(InstrMap[opcode]);
					// std::cerr << "Opcode@ 0x" << std::hex << (uint64_t)opcode << "\n";

					// Pushes first because they are very frequent
					if (0x60 <= opcode && opcode <= 0x7f)
					{
						// std::cerr << "PUSHn\n";
						llvm::APInt value = readPushData(it, bb.end());
						if (value.getActiveBits() <= 64)
							stack.push_back(value.getZExtValue());
						else
							stack.push_back(0);
						// std::cerr << stack[stack.size() - 1] << "\n";
					}
					else if (opcode < 0x10)
					{
						// Arithmetic
						// std::cerr << "arithmetic\n";
						if (ins == Instruction::STOP)
							return;
						else
							balanced_stk(&stack, stk_out, stk_in);
					}

					else if (opcode < 0x20)
					{
						// Comparisons
						// std::cerr << "compare\n";
						balanced_stk(&stack, stk_out, stk_in);
					}
					else if (opcode < 0x40)
					{
						// SHA3 and environment info
						// std::cerr << "sha3&environment\n";
						balanced_stk(&stack, stk_out, stk_in);
					}
					else if (opcode < 0x50)
					{
						// Block info
						// std::cerr << "blockinfo\n";
						balanced_stk(&stack, stk_out, stk_in);
					}
					else if (opcode < 0x60)
					{
						// VM state manipulations
						// std::cerr << " activated \n";
						if (ins == Instruction::POP)
						{
							if (!stack.empty())
								stack.pop_back();
							// std::cerr << " pop \n";
						}
						else if (ins == Instruction::JUMP || ins == Instruction::JUMPI)
						{
							return;
						}
						else
						{
							// std::cerr << "-> " << stack[0] << " " << stack[1] << " " << "\n";
							balanced_stk(&stack, stk_out, stk_in);
							// std::cerr << stack.size() << "\n";
						}
					}
					// 0x60-0x7f => PUSH_ANY

					// DUPn (eg. DUP1: a b c -> a b c c, DUP3: a b c -> a b c a)
					else if (Instruction::DUP1 <= ins && ins <= Instruction::DUP16)
					{
						auto depth = static_cast<size_t>(opcode - 0x7f);
						stack.push_back(stack.size() >= depth ? stack[stack.size() - depth] : 0);
					}
					else if (Instruction::SWAP1 <= ins && ins <= Instruction::SWAP16)
					{
						// SWAPn (eg. SWAP1: a b c d -> a b d c, SWAP3: a b c d -> d b c a)
						// 0x8e - opcode is a negative number, -2 for 0x90 ... -17 for 0x9f
						auto depth = static_cast<size_t>(opcode - 0x8e);
						if (stack.size() > depth)
						{
							uint64_t tmp = stack[stack.size() - 1];
							stack[stack.size() - 1] = stack[stack.size() - depth - 1];
							stack[stack.size() - depth - 1] = tmp;
						}
					}
					else if (Instruction::LOG0 <= ins && ins <= Instruction::LOG4)
					{
						/*
						0xa0 ... 0xa4, 32/64/96/128/160 + len(data) gas
						a. Opcodes LOG0...LOG4 are added, takes 2-6 stack arguments
								MEMSTART MEMSZ (TOPIC1) (TOPIC2) (TOPIC3) (TOPIC4)
						b. Logs are kept track of during tx execution exactly the same way as selfdestructs
						(except as an ordered list, not a set).
						Each log is in the form [address, [topic1, ... ], data] where:
						* address is what the ADDRESS opcode would output
						* data is mem[MEMSTART: MEMSTART + MEMSZ]
						* topics are as provided by the opcode
						c. The ordered list of logs in the transaction are expressed as [log0, log1, ..., logN].
						*/
						balanced_stk(&stack, stk_out, stk_in);
					}
					// CREATE
					else if (ins == Instruction::CREATE || ins == Instruction::CALL || ins == Instruction::CALLCODE || ins == Instruction::DELEGATECALL || ins == Instruction::CREATE2 || ins == Instruction::STATICCALL)
					{
						balanced_stk(&stack, stk_out, stk_in);
					}

					else if (ins == Instruction::RETURN || ins == Instruction::REVERT)
					{
						balanced_stk(&stack, stk_out, stk_in);
						return;
					}

					else if (ins == Instruction::SUICIDE)
					{
						balanced_stk(&stack, stk_out, stk_in);
						return; // SELFDESTRUCT opcode (also called SELFDESTRUCT)
					}
				}
			}

			void WASMCompiler::dry_run(std::vector<BasicBlock> &blocks)
			{
				std::map<uint64_t, BasicBlock *> idxMap;
				for (int i = 0; i < blocks.size(); i++)
				{
					idxMap[blocks[i].firstInstrIdx()] = &blocks[i];
				}

				// assign register pointers for each basic block individually.
				std::vector<std::pair<BasicBlock *, std::vector<uint64_t> *>> dfs;

				std::vector<uint64_t> *root = new std::vector<uint64_t>;
				dfs.push_back(std::make_pair(&blocks[0], root));

				std::set<BasicBlock *> visited;
				while (!dfs.empty())
				{
					// std::cerr << "\ndfs size = " << dfs.size() << "\n";
					BasicBlock *bb_ptr = dfs.back().first;
					std::vector<uint64_t> *stk_ptr = dfs.back().second;
					dfs.pop_back();

					if (bb_ptr == nullptr)
					{
						delete stk_ptr;
						continue;
					}

					// std::cerr << "BB# " << std::hex << " " << bb_ptr->firstInstrIdx() << "-";

					// edges found
					if (visited.find(bb_ptr) != visited.end())
						continue;
					else
						visited.insert(bb_ptr);

					uint64_t pc = (uint64_t)(bb_ptr->terminatorIdx());
					Instruction ternimator = Instruction(*bb_ptr->terminator());
					// std::cerr << " " << *bb_ptr->end() << "@@";
					// std::cerr << "<---->" << pc << ":" << static_cast<uint64_t>(ternimator) << "\n";

					dry_run_bb(*bb_ptr, *stk_ptr);

					if (ternimator == Instruction::JUMP)
					{
						uint64_t jump_pc = stk_ptr->empty() ? 0 : stk_ptr->back();
						if (!stk_ptr->empty())
							stk_ptr->pop_back();
						auto nextIt = idxMap.find(jump_pc);
						BasicBlock *next_bb = nextIt == idxMap.end() ? nullptr : nextIt->second;
						if (next_bb && Instruction(*next_bb->begin()) == Instruction::JUMPDEST)
						{
							bb_ptr->bbs_to.push_back(next_bb);
							next_bb->bbs_from.push_back(bb_ptr);

							std::vector<uint64_t> *new_stk_ptr = new std::vector<uint64_t>;
							new_stk_ptr->assign(stk_ptr->begin(), stk_ptr->end());
							dfs.push_back(std::make_pair(next_bb, new_stk_ptr));
							// std::cerr << "JUMP " << pc << " -> " << jump_pc << "\n";
							// std::cerr << "=>" << std::hex << next_bb->firstInstrIdx() << "--------" << (uint64_t)next_bb->terminatorIdx()  << " : " << (uint32_t)std::get<1>(InstrMap[static_cast<uint64_t>(Instruction(*next_bb->terminator()))]) <<  "\n";
						}
						else
						{
							bb_ptr->bbs_to.push_back(nullptr); // INVALID
							continue;
						}
					}

					else if (ternimator == Instruction::JUMPI)
					{
						uint64_t jump_pc = stk_ptr->empty() ? 0 : stk_ptr->back();
						if (!stk_ptr->empty())
							stk_ptr->pop_back();
						if (!stk_ptr->empty())
							stk_ptr->pop_back();

						// fallout
						uint64_t fallout_pc = pc + 1;
						auto falloutIt = idxMap.find(fallout_pc);
						BasicBlock *fallout_bb = falloutIt == idxMap.end() ? nullptr : falloutIt->second;
						if (fallout_bb && static_cast<uint32_t>(*fallout_bb->begin()) != 0xfe)
						{
							bb_ptr->bbs_to.push_back(fallout_bb);
							fallout_bb->bbs_from.push_back(bb_ptr);

							std::vector<uint64_t> *fall_stk_ptr = new std::vector<uint64_t>;
							fall_stk_ptr->assign(stk_ptr->begin(), stk_ptr->end());

							dfs.push_back(std::make_pair(fallout_bb, fall_stk_ptr));
							// std::cerr << "JUMPI-fall " << pc << " -> " << fallout_pc << "\n";
							// std::cerr << "=====>" << std::hex << fallout_bb->firstInstrIdx() << "--------" << (uint64_t)fallout_bb->terminatorIdx() << " : " <<  (uint32_t)std::get<1>(InstrMap[static_cast<uint64_t>(Instruction(*fallout_bb->terminator()))]) << "\n";
						}
						else
						{
							// INVALID
							bb_ptr->bbs_to.push_back(nullptr); // INVALID
						}
						
						std::cerr << "JUMPI-next\n";
						// jump
						auto jumpIt = idxMap.find(jump_pc);
						BasicBlock *jump_bb = jumpIt == idxMap.end() ? nullptr : jumpIt->second;
						if (jump_bb && Instruction(*jump_bb->begin()) == Instruction::JUMPDEST)
						{

							bb_ptr->bbs_to.push_back(jump_bb);
							jump_bb->bbs_from.push_back(bb_ptr);

							std::vector<uint64_t> *next_stk_ptr = new std::vector<uint64_t>;
							next_stk_ptr->assign(stk_ptr->begin(), stk_ptr->end());
							dfs.push_back(std::make_pair(jump_bb, next_stk_ptr));
							// std::cerr << "JUMPI-next " << pc << " -> " << jump_pc << "\n";
							// std::cerr << "=>" << std::hex << jump_bb->firstInstrIdx() << "--------" << (uint64_t)jump_bb->terminatorIdx()  << " : " <<  (uint32_t)std::get<1>(InstrMap[static_cast<uint64_t>(Instruction(*jump_bb->terminator()))]) << "\n";
						}
						else
						{	
							// std::cerr << "invalid\n";
							bb_ptr->bbs_to.push_back(nullptr); // INVALID
						}
						// std::cerr << "JUMPI-next-set\n";
					}

					else if (ternimator == Instruction::STOP || ternimator == Instruction::RETURN || ternimator == Instruction::REVERT || ternimator == Instruction::SUICIDE)
					{
						// stk_ptr->clear();
						delete stk_ptr;
						continue;
					}
					// std::cerr << "Go to next block\n";
				}
				// std::cerr << "\n[+] CFG Generated!\n";
			}

			std::vector<llvm::ConstantInt *> WASMCompiler::compileBasicBlock(
        BasicBlock &_basicBlock, 
        EEIModule &_eei, 
        Arith256 &_arith, 
        GlobalStack &stack) {

        llvm::IRBuilder<> IRB(_basicBlock.llvm());
				std::vector<llvm::ConstantInt *> nextBlockIdxs;
				// std::cerr << "--------------- Current Process bb#" << std::hex <<  _basicBlock.firstInstrIdx() << "-------------- \n";

				for (auto it = _basicBlock.begin(); it != _basicBlock.end(); ++it)
				{
					auto inst = Instruction(*it);

					int pc = (int)(_basicBlock.firstInstrIdx() + it - _basicBlock.begin());
					// std::cerr << "[compileBasicBlock]@ " << std::hex << pc << " :" << (int)*it << "\n";

					switch (inst)
					{

					case Instruction::ADD:
					{
						auto lhs = stack.pop();
						auto rhs = stack.pop();
						if (lhs->getType() != rhs->getType())
						{
							lhs = IRB.CreateZExt(lhs, Type::Word);
							rhs = IRB.CreateZExt(rhs, Type::Word);
						}
						auto result = IRB.CreateAdd(lhs, rhs, "unsafe");
						stack.push(result);

						llvm::Instruction *Inst = llvm::cast<llvm::Instruction>(result);
						Inst->setMetadata(c_bytecodePC, llvm::MDNode::get(Context, llvm::ValueAsMetadata::get(Constant::get(pc))));
						break;
					}

					case Instruction::SUB:
					{
						auto lhs = stack.pop();
						auto rhs = stack.pop();
						if (lhs->getType() != rhs->getType())
						{
							lhs = IRB.CreateZExt(lhs, Type::Word);
							rhs = IRB.CreateZExt(rhs, Type::Word);
						}
						auto result = IRB.CreateSub(lhs, rhs, "unsafe");
						// llvm::errs() << "unsafe Sub " << result->getName() << "\n";
						stack.push(result);
						llvm::Instruction *Inst = llvm::cast<llvm::Instruction>(result);
						Inst->setMetadata(c_bytecodePC, llvm::MDNode::get(Context, llvm::ValueAsMetadata::get(Constant::get(pc))));
						break;
					}

					case Instruction::MUL:
					{
						auto lhs = stack.pop();
						auto rhs = stack.pop();

						// llvm::Type* resTy =  lhs->getType()->getBitWidth()  > rhs->getType()->getBitWidth() ? lhs->getType() : rhs->getType();
						llvm::Type *resTy = Type::Word;
						if (lhs->getType() != rhs->getType())
						{
							lhs = IRB.CreateZExt(lhs, resTy);
							rhs = IRB.CreateZExt(lhs, resTy);
						}

						llvm::Value *result;
						// if (resTy->getType()->getBitWidth() <= 64)
						// {
						result = IRB.CreateMul(lhs, rhs, "unsafe");
						// } else {
						// auto mul_func = Arith256::getMul256Func(_module);
						// r = IRB.CreateCall(mul_func, {lhs, rhs}, "unsafe");
						// }
						stack.push(result);
						llvm::Instruction *Inst = llvm::cast<llvm::Instruction>(result);
						Inst->setMetadata(c_bytecodePC, llvm::MDNode::get(Context, llvm::ValueAsMetadata::get(Constant::get(pc))));
						break;

						// if (auto constant = llvm::dyn_cast<llvm::ConstantInt>(lhs)){
						// 	if (constant->getSExtValue() == 0)
						// 	{
						// 		stack.push(llvm::ConstantInt::get(lhs->getType(), 0));
						// 		break;
						// 	}
						// 	else if (constant->getSExtValue() == 1)
						// 	{
						// 		stack.push(rhs);
						// 		break;
						// 	}
						// }

						// if (auto constant = llvm::dyn_cast<llvm::ConstantInt>(rhs)){
						// 	if (constant->getSExtValue() == 0)
						// 	{
						// 		stack.push(llvm::ConstantInt::get(rhs->getType(), 0));
						// 		break;
						// 	}
						// 	else if (constant->getSExtValue() == 1)
						// 	{
						// 		stack.push(lhs);
						// 		break;
						// 	}
						// }
					}

					case Instruction::DIV:
					{
						auto d = stack.pop();
						auto n = stack.pop();
						if (d->getType() != n->getType())
						{
							d = IRB.CreateZExt(d, Type::Int256Ty);
							n = IRB.CreateZExt(n, Type::Int256Ty);
						}

						// auto divByZero = IRB.CreateICmpEQ(n, Constant::get(0));
						// n = IRB.CreateSelect(divByZero, Constant::get(1), n); // protect against hardware signal
						llvm::Value *result = IRB.CreateUDiv(d, n, "unsafe");
						// r = IRB.CreateSelect(divByZero, Constant::get(0), r);

						// auto div_func = Arith256::getUDiv256Func(_module);
						// auto r = IRB.CreateCall(div_func, {d, n});
						stack.push(result);
						llvm::Instruction *Inst = llvm::cast<llvm::Instruction>(result);
						Inst->setMetadata(c_bytecodePC, llvm::MDNode::get(Context, llvm::ValueAsMetadata::get(Constant::get(pc))));
						break;
					}

					case Instruction::SDIV:
					{
						auto d = stack.pop();
						auto n = stack.pop();
						if (d->getType() != n->getType())
						{
							d = IRB.CreateZExt(d, Type::Int256Ty);
							n = IRB.CreateZExt(n, Type::Int256Ty);
						}
						// auto divByZero = IRB.CreateICmpEQ(n, Constant::get(0));
						// auto divByMinusOne = IRB.CreateICmpEQ(n, Constant::get(-1));
						// n = IRB.CreateSelect(divByZero, Constant::get(1), n); // protect against hardware signal
						auto result = IRB.CreateSDiv(d, n, "unsafe");
						// r = IRB.CreateSelect(divByZero, Constant::get(0), r);
						// auto dNeg = IRB.CreateSub(Constant::get(0), d);
						// r = IRB.CreateSelect(divByMinusOne, dNeg, r); // protect against undef i256.min / -1
						// auto sdiv_func = Arith256::getSDiv256Func(_module);
						// auto r = IRB.CreateCall(sdiv_func, {d, n});
						stack.push(result);

						llvm::Instruction *Inst = llvm::cast<llvm::Instruction>(result);
						Inst->setMetadata(c_bytecodePC, llvm::MDNode::get(Context, llvm::ValueAsMetadata::get(Constant::get(pc))));
						break;
					}

					case Instruction::MOD:
					{
						auto d = stack.pop();
						auto n = stack.pop();
						// auto divByZero = IRB.CreateICmpEQ(n, Constant::get(0));
						// n = IRB.CreateSelect(divByZero, Constant::get(1), n); // protect against hardware signal
						if (d->getType() != n->getType())
						{
							d = IRB.CreateZExt(d, Type::Int256Ty);
							n = IRB.CreateZExt(n, Type::Int256Ty);
						}
						auto result = IRB.CreateURem(d, n);
						// result = IRB.CreateSelect(divByZero, Constant::get(0), r);
						stack.push(result);
						llvm::Instruction *Inst = llvm::cast<llvm::Instruction>(result);
						Inst->setMetadata(c_bytecodePC, llvm::MDNode::get(Context, llvm::ValueAsMetadata::get(Constant::get(pc))));
						break;
					}

					case Instruction::SMOD:
					{
						auto d = stack.pop();
						auto n = stack.pop();
						if (d->getType() != n->getType())
						{
							d = IRB.CreateZExt(d, Type::Int256Ty);
							n = IRB.CreateZExt(n, Type::Int256Ty);
						}
						// auto divByZero = IRB.CreateICmpEQ(n, Constant::get(0));
						// auto divByMinusOne = IRB.CreateICmpEQ(n, Constant::get(-1));
						// n = IRB.CreateSelect(divByZero, Constant::get(1), n); // protect against hardware signal
						auto result = IRB.CreateSRem(d, n);
						// r = IRB.CreateSelect(divByZero, Constant::get(0), r);
						// r = IRB.CreateSelect(divByMinusOne, Constant::get(0), r); // protect against undef i256.min / -1
						stack.push(result);
						llvm::Instruction *Inst = llvm::cast<llvm::Instruction>(result);
						Inst->setMetadata(c_bytecodePC, llvm::MDNode::get(Context, llvm::ValueAsMetadata::get(Constant::get(pc))));
						break;
					}

					case Instruction::ADDMOD:
					{
						auto i512Ty = IRB.getIntNTy(512);
						auto a = stack.pop();
						auto b = stack.pop();
						auto m = stack.pop();
						auto divByZero = IRB.CreateICmpEQ(m, Constant::get(0));
						a = IRB.CreateZExt(a, i512Ty);
						b = IRB.CreateZExt(b, i512Ty);
						m = IRB.CreateZExt(m, i512Ty);
						auto result = IRB.CreateNUWAdd(a, b);
						result = IRB.CreateURem(result, m);
						result = IRB.CreateSelect(divByZero, Constant::get(0), IRB.CreateTrunc(result, Type::Word));
						stack.push(result);

						llvm::Instruction *Inst = llvm::cast<llvm::Instruction>(result);
						Inst->setMetadata(c_bytecodePC, llvm::MDNode::get(Context, llvm::ValueAsMetadata::get(Constant::get(pc))));
						break;
					}

					case Instruction::MULMOD:
					{
						auto i512Ty = IRB.getIntNTy(512);
						auto a = stack.pop();
						auto b = stack.pop();
						auto m = stack.pop();
						auto divByZero = IRB.CreateICmpEQ(m, Constant::get(0));
						a = IRB.CreateZExt(a, i512Ty);
						b = IRB.CreateZExt(b, i512Ty);
						m = IRB.CreateZExt(m, i512Ty);
						auto p = IRB.CreateNUWMul(a, b);
						p = IRB.CreateURem(p, m);
						p = IRB.CreateTrunc(p, Type::Word);
						p = IRB.CreateSelect(divByZero, Constant::get(0), p);
						stack.push(p);

						llvm::Instruction *Inst = llvm::cast<llvm::Instruction>(p);
						Inst->setMetadata(c_bytecodePC, llvm::MDNode::get(Context, llvm::ValueAsMetadata::get(Constant::get(pc))));
						break;
					}

					case Instruction::EXP:
					{
						auto base = stack.pop();
						auto exponent = stack.pop();
						auto ret = _arith.exp(base, exponent);
						stack.push(ret);

						// llvm::Instruction *Inst = llvm::cast<llvm::Instruction>(ret);
						// Inst->setMetadata(c_bytecodePC, llvm::MDNode::get(Context, llvm::ValueAsMetadata::get(Constant::get(pc))));
						break;
					}

					case Instruction::NOT:
					{
						auto value = stack.pop();
						auto result = IRB.CreateNot(value, "bnot");
						stack.push(result);
						llvm::Instruction *Inst = llvm::cast<llvm::Instruction>(result);
						Inst->setMetadata(c_bytecodePC, llvm::MDNode::get(Context, llvm::ValueAsMetadata::get(Constant::get(pc))));
						break;
					}

					case Instruction::LT:
					{
						auto lhs = stack.pop();
						auto rhs = stack.pop();
						if (lhs->getType() != rhs->getType())
						{
							lhs = IRB.CreateZExt(lhs, Type::Word);
							rhs = IRB.CreateZExt(rhs, Type::Word);
						}
						auto res1 = IRB.CreateICmpULT(lhs, rhs);
						auto res256 = IRB.CreateZExt(res1, Type::Word);
						stack.push(res256);

						llvm::Instruction *Inst = llvm::cast<llvm::Instruction>(res256);
						Inst->setMetadata(c_bytecodePC, llvm::MDNode::get(Context, llvm::ValueAsMetadata::get(Constant::get(pc))));
						break;
					}

					case Instruction::GT:
					{
						auto lhs = stack.pop();
						auto rhs = stack.pop();
						if (lhs->getType() != rhs->getType())
						{
							lhs = IRB.CreateZExt(lhs, Type::Word);
							rhs = IRB.CreateZExt(rhs, Type::Word);
						}
						auto res1 = IRB.CreateICmpUGT(lhs, rhs);
						auto res256 = IRB.CreateZExt(res1, Type::Word);
						stack.push(res256);

						llvm::Instruction *Inst = llvm::cast<llvm::Instruction>(res256);
						Inst->setMetadata(c_bytecodePC, llvm::MDNode::get(Context, llvm::ValueAsMetadata::get(Constant::get(pc))));
						break;
					}

					case Instruction::SLT:
					{
						auto lhs = stack.pop();
						auto rhs = stack.pop();
						if (lhs->getType() != rhs->getType())
						{
							lhs = IRB.CreateZExt(lhs, Type::Word);
							rhs = IRB.CreateZExt(rhs, Type::Word);
						}
						auto res1 = IRB.CreateICmpSLT(lhs, rhs);
						auto res256 = IRB.CreateZExt(res1, Type::Word);
						stack.push(res256);

						llvm::Instruction *Inst = llvm::cast<llvm::Instruction>(res256);
						Inst->setMetadata(c_bytecodePC, llvm::MDNode::get(Context, llvm::ValueAsMetadata::get(Constant::get(pc))));
						break;
					}

					case Instruction::SGT:
					{
						auto lhs = stack.pop();
						auto rhs = stack.pop();
						if (lhs->getType() != rhs->getType())
						{
							lhs = IRB.CreateZExt(lhs, Type::Word);
							rhs = IRB.CreateZExt(rhs, Type::Word);
						}
						auto res1 = IRB.CreateICmpSGT(lhs, rhs);
						auto res256 = IRB.CreateZExt(res1, Type::Word);
						stack.push(res256);

						llvm::Instruction *Inst = llvm::cast<llvm::Instruction>(res256);
						Inst->setMetadata(c_bytecodePC, llvm::MDNode::get(Context, llvm::ValueAsMetadata::get(Constant::get(pc))));
						break;
					}

					case Instruction::EQ:
					{
						auto lhs = stack.pop();
						auto rhs = stack.pop();
						if (lhs->getType() != rhs->getType())
						{
							lhs = IRB.CreateZExt(lhs, Type::Word);
							rhs = IRB.CreateZExt(rhs, Type::Word);
						}
						auto res1 = IRB.CreateICmpEQ(lhs, rhs);
						auto res256 = IRB.CreateZExt(res1, Type::Word);
						stack.push(res256);

						llvm::Instruction *Inst = llvm::cast<llvm::Instruction>(res256);
						Inst->setMetadata(c_bytecodePC, llvm::MDNode::get(Context, llvm::ValueAsMetadata::get(Constant::get(pc))));
						break;
					}

					case Instruction::ISZERO:
					{
						auto top = stack.pop();
						// if (top->getType() != Type::Word) {
						// 	top = IRB.CreateZExt(top, Type::Word);
						// }
						auto iszero = IRB.CreateICmpEQ(top, llvm::ConstantInt::get(top->getType(), 0), "iszero");
						auto result = IRB.CreateZExt(iszero, Type::Word);
						stack.push(result);

						llvm::Instruction *Inst = llvm::cast<llvm::Instruction>(result);
						Inst->setMetadata(c_bytecodePC, llvm::MDNode::get(Context, llvm::ValueAsMetadata::get(Constant::get(pc))));
						break;
					}

					case Instruction::AND:
					{

						auto lhs = stack.pop();
						auto rhs = stack.pop();
						if (lhs->getType() != rhs->getType())
						{
							lhs = IRB.CreateZExt(lhs, Type::Word);
							rhs = IRB.CreateZExt(rhs, Type::Word);
						}

						auto res = IRB.CreateAnd(lhs, rhs);
						stack.push(res);

						llvm::Instruction *Inst = llvm::cast<llvm::Instruction>(res);
						Inst->setMetadata(c_bytecodePC, llvm::MDNode::get(Context, llvm::ValueAsMetadata::get(Constant::get(pc))));
						break;
					}

					case Instruction::OR:
					{
						auto lhs = stack.pop();
						auto rhs = stack.pop();
						if (lhs->getType() != rhs->getType())
						{
							lhs = IRB.CreateZExt(lhs, Type::Word);
							rhs = IRB.CreateZExt(rhs, Type::Word);
						}

						auto res = IRB.CreateOr(lhs, rhs);
						stack.push(res);

						llvm::Instruction *Inst = llvm::cast<llvm::Instruction>(res);
						Inst->setMetadata(c_bytecodePC, llvm::MDNode::get(Context, llvm::ValueAsMetadata::get(Constant::get(pc))));
						break;
					}

					case Instruction::XOR:
					{
						auto lhs = stack.pop();
						auto rhs = stack.pop();
						if (lhs->getType() != rhs->getType())
						{
							lhs = IRB.CreateZExt(lhs, Type::Word);
							rhs = IRB.CreateZExt(rhs, Type::Word);
						}
						auto res = IRB.CreateXor(lhs, rhs);
						stack.push(res);

						llvm::Instruction *Inst = llvm::cast<llvm::Instruction>(res);
						Inst->setMetadata(c_bytecodePC, llvm::MDNode::get(Context, llvm::ValueAsMetadata::get(Constant::get(pc))));
						break;
					}

					case Instruction::BYTE:
					{
						const auto idx = stack.pop();
						// auto value = Endianness::toBE(IRB, stack.pop());
						auto value = _eei.emitEndianConvert(stack.pop());

						auto idxValid = IRB.CreateICmpULT(idx, Constant::get(32), "idxValid");
						auto bytes = IRB.CreateBitCast(value, llvm::VectorType::get(Type::Byte, 32), "bytes");
						// TODO: Workaround for LLVM bug. Using big value of index causes invalid memory access.
						auto safeIdx = IRB.CreateTrunc(idx, IRB.getIntNTy(5));
						// TODO: Workaround for LLVM bug. DAG Builder used sext on index instead of zext
						safeIdx = IRB.CreateZExt(safeIdx, Type::Size);
						auto byte = IRB.CreateExtractElement(bytes, safeIdx, "byte");
						value = IRB.CreateZExt(byte, Type::Word);
						value = IRB.CreateSelect(idxValid, value, Constant::get(0));
						stack.push(value);

						llvm::Instruction *Inst = llvm::cast<llvm::Instruction>(value);
						Inst->setMetadata(c_bytecodePC, llvm::MDNode::get(Context, llvm::ValueAsMetadata::get(Constant::get(pc))));
						break;
					}

					case Instruction::SIGNEXTEND:
					{
						auto idx = stack.pop();
						auto word = stack.pop();

						auto k32_ = IRB.CreateTrunc(idx, IRB.getIntNTy(5), "k_32");
						auto k32 = IRB.CreateZExt(k32_, Type::Size);
						auto k32x8 = IRB.CreateMul(k32, IRB.getInt64(8), "kx8");

						// test for word >> (k * 8 + 7)
						auto bitpos = IRB.CreateAdd(k32x8, IRB.getInt64(7), "bitpos");
						auto bitposEx = IRB.CreateZExt(bitpos, Type::Word);
						auto bitval = IRB.CreateLShr(word, bitposEx, "bitval");
						auto bittest = IRB.CreateTrunc(bitval, Type::Bool, "bittest");

						auto mask_ = IRB.CreateShl(Constant::get(1), bitposEx);
						auto mask = IRB.CreateSub(mask_, Constant::get(1), "mask");

						auto negmask = IRB.CreateXor(mask, llvm::ConstantInt::getAllOnesValue(Type::Word), "negmask");
						auto val1 = IRB.CreateOr(word, negmask);
						auto val0 = IRB.CreateAnd(word, mask);

						auto kInRange = IRB.CreateICmpULE(idx, llvm::ConstantInt::get(Type::Word, 30));
						auto result = IRB.CreateSelect(kInRange,
															 IRB.CreateSelect(bittest, val1, val0),
															 word, "signed");

						// llvm::MDNode* N = llvm::MDNode::get(Context, llvm::MDString::get(Context, "signed"));

						// llvm::SelectInst *Inst = llvm::cast<llvm::SelectInst>(result);
						// Inst->setMetadata("stats.signed", llvm::MDNode::get(Context, llvm::ValueAsMetadata::get(Constant::get(0))));

						stack.push(result);

						llvm::Instruction *Inst = llvm::cast<llvm::Instruction>(result);
						Inst->setMetadata(c_bytecodePC, llvm::MDNode::get(Context, llvm::ValueAsMetadata::get(Constant::get(pc))));
						break;
					}

					case Instruction::SHL:
					{
						auto shift = stack.pop();
						auto value = stack.pop();
						auto res = IRB.CreateShl(value, shift, "ShlAssign");
						stack.push(res);

						llvm::Instruction *Inst = llvm::cast<llvm::Instruction>(res);
						Inst->setMetadata(c_bytecodePC, llvm::MDNode::get(Context, llvm::ValueAsMetadata::get(Constant::get(pc))));
						break;
					}

					case Instruction::SHR:
					{
						auto shift = stack.pop();
						auto value = stack.pop();
						auto res = IRB.CreateLShr(value, shift, "ShrAssign");
						stack.push(res);

						llvm::Instruction *Inst = llvm::cast<llvm::Instruction>(res);
						Inst->setMetadata(c_bytecodePC, llvm::MDNode::get(Context, llvm::ValueAsMetadata::get(Constant::get(pc))));
						break;
					}

					case Instruction::SAR:
					{
						// signed
						auto shift = stack.pop();
						auto value = stack.pop();
						auto res = IRB.CreateAShr(value, shift, "SarAssign");
						stack.push(res);

						llvm::Instruction *Inst = llvm::cast<llvm::Instruction>(res);
						Inst->setMetadata(c_bytecodePC, llvm::MDNode::get(Context, llvm::ValueAsMetadata::get(Constant::get(pc))));
						break;
					}

					case Instruction::SHA3:
					{
						auto inOff = stack.pop();
						auto inSize = stack.pop();
						// inSize = IRB.CreateAdd(inSize, Constant::get(0x20));
						if (inOff->getType() != Type::Int8PtrTy)
							inOff = IRB.CreateIntToPtr(inOff, Type::Int8PtrTy);
						if (inSize->getType() != Type::Int32Ty)
							inSize = IRB.CreateTruncOrBitCast(inSize, Type::Int32Ty);

						llvm::Value *hash = IRB.CreateCall(_eei.Func_keccak256, {inOff, inSize});
						auto result = _eei.emitEndianConvert(hash);
						stack.push(result);

						llvm::Instruction *Inst = llvm::cast<llvm::Instruction>(result);
						Inst->setMetadata(c_bytecodePC, llvm::MDNode::get(Context, llvm::ValueAsMetadata::get(Constant::get(pc))));
						break;

						// auto inOff = stack.pop();
						// auto inSize = stack.pop();
						// if (inOff->getType() != Type::Int8PtrTy)
						// 	inOff = IRB.CreateIntToPtr(inOff, Type::Int8PtrTy);
						// if (inSize->getType() != Type::Int32Ty)
						// 	inSize = IRB.CreateTruncOrBitCast(inSize, Type::Int32Ty);

						// llvm::Value *Bytes = llvm::ConstantAggregateZero::get(Type::BytesTy);
						// Bytes = IRB.CreateInsertValue(
						// 	Bytes, IRB.CreateZExtOrTrunc(inSize, Type::Int256Ty), {0});
						// Bytes = IRB.CreateInsertValue(Bytes, inOff, {1});
						// llvm::Value *Result = IRB.CreateCall(_eei.Func_keccak256, {Bytes});
						// break;
					}

					case Instruction::POP:
					{
						stack.pop();
						break;
					}

					case Instruction::ANY_PUSH:
					{
						auto value = readPushData(it, _basicBlock.end());
						// std::cerr << "* Push data = " << value.toString(10, false) << "\n";
						stack.push(llvm::ConstantInt::get(Type::Int256Ty, value));
						break;
					}

					case Instruction::ANY_DUP:
					{

						auto index = static_cast<size_t>(inst) - static_cast<size_t>(Instruction::DUP1);
						// std::cerr << "[----------] DUP["<< index << "]" << static_cast<size_t>(inst) << "\n";
						// std::cerr << "Before DUP; stack size= " << stack.size() << "\n";
						// for (auto mtp = stack.view().begin(); mtp != stack.view().end() ;++mtp)
						// {
						// 	auto val = *mtp;
						// 	if (auto constant = llvm::dyn_cast<llvm::ConstantInt>(val))
						// 	{
						// 		std::cerr << "@Constant="
						// 			<< llvm::dyn_cast<llvm::ConstantInt>(val)->getSExtValue() << "\n";
						// 	}
						// 	else
						// 	{
						// 		std::cerr << "XX\n";
						// 	}
						// }
						stack.dup(index);

						// std::cerr << "After stack size= " << stack.size() << "\n";
						// for (auto mtp = stack.view().begin(); mtp != stack.view().end() ;++mtp)
						// {
						// 	auto val = *mtp;
						// 	if (auto constant = llvm::dyn_cast<llvm::ConstantInt>(val))
						// 	{
						// 		std::cerr << "@Constant="
						// 			<< llvm::dyn_cast<llvm::ConstantInt>(val)->getSExtValue() << "\n";
						// 	}
						// 	else
						// 	{
						// 		std::cerr << "XX" << val <<"\n";
						// 	}
						// }
						// std::cerr << "----------------------\n";

						break;
					}

					case Instruction::ANY_SWAP:
					{
						auto index = static_cast<size_t>(inst) - static_cast<size_t>(Instruction::SWAP1);
						stack.swap(index);
						break;
					}

					case Instruction::MLOAD:
					{
						auto addr = stack.pop();
						if (addr->getType() != Type::Int256PtrTy)
							addr = IRB.CreateIntToPtr(addr, Type::Int256PtrTy);
						// auto word = _memory.loadWord(addr);
						auto word = IRB.CreateLoad(Type::Word, addr);
						auto result = _eei.emitEndianConvert(word);
						stack.push(result);

						// llvm::Instruction *Inst = llvm::cast<llvm::Instruction>(result);
						// Inst->setMetadata(c_bytecodePC, llvm::MDNode::get(Context, llvm::ValueAsMetadata::get(Constant::get(pc))));
						break;
					}

					case Instruction::MSTORE:
					{
						auto addr = stack.pop();
						auto word = stack.pop();
						if (addr->getType() != Type::Int256PtrTy)
							addr = IRB.CreateIntToPtr(addr, Type::Int256PtrTy);
						if (addr->getType() != Type::Int256Ty)
							word = IRB.CreateZExt(word, Type::Int256Ty);
						IRB.CreateStore(_eei.emitEndianConvert(word), addr);
						// IRB.CreateStore(word, addr);
						// _memory.storeWord(addr, word);

						// llvm::Instruction *Inst = llvm::cast<llvm::Instruction>(result);
						// Inst->setMetadata(c_bytecodePC, llvm::MDNode::get(Context, llvm::ValueAsMetadata::get(Constant::get(pc))));
						break;
					}

					case Instruction::MSTORE8:
					{
						auto addr = stack.pop();
						auto word = stack.pop();

						if (addr->getType() != Type::Int8PtrTy)
							addr = IRB.CreateIntToPtr(addr, Type::Int8PtrTy);
						if (word->getType() != Type::Int8Ty)
							word = IRB.CreateTrunc(word, Type::Int8Ty);

						// IRB.CreateStore(_eei.emitEndianConvert(IRB.CreateAnd(word, Constant::get(0xff))), addr);
						// IRB.CreateStore(word, addr);
						IRB.CreateStore(_eei.emitEndianConvert(word), addr);
						// _memory.storeByte(addr, word);
						break;
					}

					case Instruction::MSIZE:
					{
						stack.push(IRB.CreateLoad(_eei.MemorySize));
						break;
					}

					case Instruction::SLOAD:
					{
						auto index = stack.pop();

						// auto value = _ext.sload(index);
						auto AddressPtr = IRB.CreateAlloca(Type::Int256Ty, nullptr, "lslot.ptr");
						auto ValPtr = IRB.CreateAlloca(Type::Int256Ty, nullptr, "lvalue.ptr");
						IRB.CreateStore(_eei.emitEndianConvert(index), AddressPtr);
						IRB.CreateCall(_eei.Func_storageLoad, {IRB.CreateBitCast(AddressPtr, Type::Int32PtrTy, "lslot.mptr"), IRB.CreateBitCast(ValPtr, Type::Int32PtrTy, "lvalue.mptr")});
						// IRB.CreateCall(_eei.Func_storageLoad, {IRB.CreateBitCast(AddressPtr, Type::Int256PtrTy), IRB.CreateBitCast(ValPtr, Type::Int256PtrTy)});
						auto value = IRB.CreateLoad(ValPtr);
						auto result = _eei.emitEndianConvert(value);
						stack.push(result);
						// stack.push(value);

						llvm::Instruction *Inst = llvm::cast<llvm::Instruction>(result);
						Inst->setMetadata(c_bytecodePC, llvm::MDNode::get(Context, llvm::ValueAsMetadata::get(Constant::get(pc))));
						break;
					}

					case Instruction::SSTORE:
					{
						// if (m_staticCall)
						// 	goto invalidInstruction;

						auto index = stack.pop();
						auto value = stack.pop();

						if (index->getType() != Type::Word)
						{
							index = IRB.CreateZExt(index, Type::Word);
						}
						if (value->getType() != Type::Word)
						{
							value = IRB.CreateZExt(value, Type::Word);
						}

						// _gasMeter.countSStore(_ext, index, value);
						// _ext.sstore(index, value);
						auto AddressPtr = IRB.CreateAlloca(Type::Int256Ty, nullptr, "sslot.ptr"); // allocate an <Int256Ty>, referenced with $AddressPtr
						auto ValPtr = IRB.CreateAlloca(Type::Int256Ty, nullptr, "svalue.ptr");
						IRB.CreateStore(_eei.emitEndianConvert(index), AddressPtr); // store $index into Mem[$AddressPtr]
						// IRB.CreateStore(value, ValPtr);
						IRB.CreateStore(_eei.emitEndianConvert(value), ValPtr);
						auto callInst = IRB.CreateCall(_eei.Func_storageStore, {IRB.CreateBitCast(AddressPtr, Type::Int32PtrTy, "sslot.mptr"), IRB.CreateBitCast(ValPtr, Type::Int32PtrTy, "svalue.mptr")});
						// IRB.CreateCall(_eei.Func_storageStore, {IRB.CreateBitCast(AddressPtr, Type::Int256PtrTy), IRB.CreateBitCast(ValPtr, Type::Int256PtrTy)});

						llvm::Instruction *Inst = llvm::cast<llvm::Instruction>(callInst);
						Inst->setMetadata(c_bytecodePC, llvm::MDNode::get(Context, llvm::ValueAsMetadata::get(Constant::get(pc))));
						break;
					}

					case Instruction::JUMP:
					case Instruction::JUMPI:
					{
						auto idx = stack.pop();
						auto destIdx = llvm::MDNode::get(Context, llvm::ValueAsMetadata::get(idx));
						// std::cerr << "destIdx = " << destIdx << "@@@" << idx << "\n";
						// std::cerr << "Going to JUMPDEST: " << llvm::ValueAsMetadata::get(idx)<< "\n";
						// Create branch instruction, initially to jump table.
						// Destination will be optimized with direct jump during jump resolving if destination index is a constant.

						// if (inst == Instruction::JUMP)
						// {
						// 	// auto br_con = IRB.CreateAlloca(Type::Int256Ty, nullptr);
						// 	// auto br_adr = IRB.CreateSelect(IRB.CreateICmpEQ(IRB.CreateLoad(br_con), Constant::get(0)), llvm::BlockAddress::get(m_abortBB), llvm::BlockAddress::get(m_exitBB));
						// 	auto jumpTable = llvm::cast<llvm::SwitchInst>(m_jumpTableBB->getTerminator());
						// 	auto bb = jumpTable->findCaseValue(Constant::get(12))->getCaseSuccessor();

						// 	auto indirectBr = IRB.CreateIndirectBr(bb, indirectDsts.size());
						// 	for (auto bbPtr : indirectDsts)
						// 	{
						// 		indirectBr->addDestination(bbPtr->llvm());
						// 	}

						// 	// indirectBr->addDestination(m_abortBB);
						// 	// indirectBr->addDestination(m_exitBB);
						// }
						// else{
						// 	auto jumpInst = IRB.CreateCondBr(IRB.CreateICmpNE(stack.pop(), Constant::get(0), "jump.check"), m_jumpTableBB, _basicBlock.llvm()->getNextNode());
						// 	// Attach medatada to branch instruction with information about destination index.
						// 	jumpInst->setMetadata(c_destIdxLabel, destIdx);
						// }

						// llvm::BranchInst * jumpInst;
						// if (inst == Instruction::JUMP)
						// {
						// 	auto targetPtr = llvm::cast<llvm::GlobalVariable>(_module.getOrInsertGlobal("target", Type::Int256Ty));
						// 	IRB.CreateStore(idx, targetPtr);
						// 	jumpInst = IRB.CreateBr(m_jumpTableBB);
						// }
						// else
						// {
						// 	// JUMPI
						// 	auto cond = IRB.CreateICmpNE(stack.pop(), Constant::get(0), "jump.check");
						// 	IRB.CreateStore(idx, targetPtr);
						// 	jumpInst = IRB.CreateCondBr(cond, m_jumpTableBB, _basicBlock.llvm()->getNextNode());
						// }
						// jumpInst->setMetadata(c_destIdxLabel, destIdx);

						// if (auto constant = llvm::dyn_cast<llvm::ConstantInt>(idx))
						// {
						// 	std::cerr << "[ResolveBlocks] @@@@@@ #" << _basicBlock.firstInstrIdx()
						// 	<< ((inst == Instruction::JUMP) ? " JUMP @ Constant=" : " JUMPI @ Constant=")
						// 	<< llvm::dyn_cast<llvm::ConstantInt>(idx)->getSExtValue() << "\n";
						// }
						// else
						// {
						// 	std::cerr << "[ResolveBlocks] @@@@@@ #" << _basicBlock.firstInstrIdx() << ((inst == Instruction::JUMP) ? " JUMP @ Unknown" : " JUMPI @ unKnown") << "\n";
						// }

						llvm::GlobalVariable *targetPtr = llvm::cast<llvm::GlobalVariable>(Module.getOrInsertGlobal("target", Type::Int256Ty));

						llvm::BranchInst *jumpInst = nullptr;

						if (inst == Instruction::JUMP) {
							IRB.CreateStore(idx, targetPtr);
							jumpInst = IRB.CreateBr(m_jumpTableBB);

						} else {
							// auto rawCond =  IRB.CreateTruncOrBitCast(stack.pop(), Type::Bool);
							llvm::Value *rawCond = stack.pop();
							llvm::Value *cond = IRB.CreateICmpNE(rawCond, llvm::ConstantInt::get(rawCond->getType(), 0), "jump.check");
							IRB.CreateStore(idx, targetPtr);
							jumpInst = IRB.CreateCondBr(cond, m_jumpTableBB, _basicBlock.llvm()->getNextNode());
						}
						// Attach medatada to branch instruction with information about destination index.
						jumpInst->setMetadata(c_destIdxLabel, destIdx);
						break;
					}

					case Instruction::JUMPDEST:
					{
						// Add the basic block to the jump table.
						assert(it == _basicBlock.begin() && "JUMPDEST must be the first instruction of a basic block");
						auto jumpTable = llvm::cast<llvm::SwitchInst>(m_jumpTableBB->getTerminator());
						jumpTable->addCase(Constant::get(_basicBlock.firstInstrIdx()), _basicBlock.llvm());
						break;
					}

					case Instruction::PC:
					{
						auto value = Constant::get(it - _basicBlock.begin() + _basicBlock.firstInstrIdx());
						stack.push(value);

						llvm::Instruction *Inst = llvm::cast<llvm::Instruction>(value);
						Inst->setMetadata(c_bytecodePC, llvm::MDNode::get(Context, llvm::ValueAsMetadata::get(Constant::get(pc))));
						break;
					}

					case Instruction::GAS:
					{
						auto value = IRB.CreateCall(_eei.Func_getGasLeft, {});
						auto result = IRB.CreateZExt(value, Type::Word);
						stack.push(result);

						llvm::Instruction *Inst = llvm::cast<llvm::Instruction>(result);
						Inst->setMetadata(c_bytecodePC, llvm::MDNode::get(Context, llvm::ValueAsMetadata::get(Constant::get(pc))));
						break;
					}

					case Instruction::ADDRESS:
					{
						auto ValPtr = IRB.CreateAlloca(Type::AddressTy, nullptr);
						auto destMemIdx = IRB.CreateBitCast(ValPtr, Type::Int32PtrTy);
						IRB.CreateCall(_eei.Func_getAddress, {destMemIdx});
						auto value = IRB.CreateLoad(ValPtr);
						auto result = IRB.CreateZExt(value, Type::Word);
						stack.push(result);

						llvm::Instruction *Inst = llvm::cast<llvm::Instruction>(result);
						Inst->setMetadata(c_bytecodePC, llvm::MDNode::get(Context, llvm::ValueAsMetadata::get(Constant::get(pc))));
						break;
					}
					case Instruction::CALLER:
					{
						auto ValPtr = IRB.CreateAlloca(Type::AddressTy, nullptr);
						auto destMemIdx = IRB.CreateBitCast(ValPtr, Type::Int32PtrTy);
						IRB.CreateCall(_eei.Func_getCaller, {destMemIdx});
						auto m_value = IRB.CreateLoad(ValPtr);
						auto value = _eei.emitEndianConvert(m_value);
						auto result = IRB.CreateZExt(value, Type::Word);
						stack.push(result);

						llvm::Instruction *Inst = llvm::cast<llvm::Instruction>(result);
						Inst->setMetadata(c_bytecodePC, llvm::MDNode::get(Context, llvm::ValueAsMetadata::get(Constant::get(pc))));
						break;
					}
					case Instruction::ORIGIN:
					{
						auto ValPtr = IRB.CreateAlloca(Type::AddressTy, nullptr);
						auto destMemIdx = IRB.CreateBitCast(ValPtr, Type::Int32PtrTy);
						auto callInst = IRB.CreateCall(_eei.Func_getTxOrigin, {destMemIdx});
						auto m_value = IRB.CreateLoad(ValPtr);
						auto value = _eei.emitEndianConvert(m_value);
						auto result = IRB.CreateZExt(value, Type::Word);
						stack.push(result);

						llvm::Instruction *Inst = llvm::cast<llvm::Instruction>(callInst);
						Inst->setMetadata(c_bytecodePC, llvm::MDNode::get(Context, llvm::ValueAsMetadata::get(Constant::get(pc))));
						break;
					}
					case Instruction::COINBASE:
					{
						// stack.push(IRB.CreateZExt(Endianness::toNative(IRB, _runtimeManager.getTxContextItem(2)), Type::Word));
						auto ValPtr = IRB.CreateAlloca(Type::AddressTy, nullptr);
						auto destMemIdx = IRB.CreateBitCast(ValPtr, Type::Int32PtrTy);
						IRB.CreateCall(_eei.Func_getBlockCoinbase, {destMemIdx});
						auto value = IRB.CreateLoad(ValPtr);
						auto result = IRB.CreateZExt(value, Type::Word);
						stack.push(result);

						llvm::Instruction *Inst = llvm::cast<llvm::Instruction>(result);
						Inst->setMetadata(c_bytecodePC, llvm::MDNode::get(Context, llvm::ValueAsMetadata::get(Constant::get(pc))));
						break;
					}
					case Instruction::GASPRICE:
					{
						// stack.push(Endianness::toNative(IRB, _runtimeManager.getTxContextItem(0)));
						auto ValPtr = IRB.CreateAlloca(Type::Int128Ty, nullptr);
						auto destMemIdx = IRB.CreateBitCast(ValPtr, Type::Int32PtrTy);
						IRB.CreateCall(_eei.Func_getTxGasPrice, {destMemIdx});
						auto value = IRB.CreateLoad(ValPtr);
						auto result = IRB.CreateZExt(value, Type::Word);
						stack.push(result);

						llvm::Instruction *Inst = llvm::cast<llvm::Instruction>(result);
						Inst->setMetadata(c_bytecodePC, llvm::MDNode::get(Context, llvm::ValueAsMetadata::get(Constant::get(pc))));
						break;
					}
					case Instruction::DIFFICULTY:
					{
						// stack.push(Endianness::toNative(IRB, _runtimeManager.getTxContextItem(6)));
						auto ValPtr = IRB.CreateAlloca(Type::Int256Ty, nullptr);
						auto destMemIdx = IRB.CreateBitCast(ValPtr, Type::Int32PtrTy);
						IRB.CreateCall(_eei.Func_getBlockDifficulty, {destMemIdx});
						auto value = IRB.CreateLoad(ValPtr);
						stack.push(value);

						llvm::Instruction *Inst = llvm::cast<llvm::Instruction>(value);
						Inst->setMetadata(c_bytecodePC, llvm::MDNode::get(Context, llvm::ValueAsMetadata::get(Constant::get(pc))));
						break;
					}

					case Instruction::GASLIMIT:
					{
						// stack.push(IRB.CreateZExt(_runtimeManager.getTxContextItem(5), Type::Word));
						auto value = IRB.CreateCall(_eei.Func_getBlockGasLimit, {});
						auto result = IRB.CreateZExt(value, Type::Word);
						stack.push(result);

						llvm::Instruction *Inst = llvm::cast<llvm::Instruction>(result);
						Inst->setMetadata(c_bytecodePC, llvm::MDNode::get(Context, llvm::ValueAsMetadata::get(Constant::get(pc))));
						break;
					}

					case Instruction::CHAINID:
					{
						auto chainID = IRB.CreateCall(_eei.Func_getChainID, {});
						auto result = IRB.CreateZExt(chainID, Type::Word);
						stack.push(result);

						llvm::Instruction *Inst = llvm::cast<llvm::Instruction>(result);
						Inst->setMetadata(c_bytecodePC, llvm::MDNode::get(Context, llvm::ValueAsMetadata::get(Constant::get(pc))));
						break;
					}

					case Instruction::SELFBALANCE:
					{
						auto ValPtr = IRB.CreateAlloca(Type::Int128Ty, nullptr);
						IRB.CreateCall(_eei.Func_getSelfBalance, {IRB.CreateBitCast(ValPtr, Type::Int32PtrTy)});
						auto value = IRB.CreateLoad(ValPtr);
						stack.push(value);
						break;
					}

					case Instruction::BASEFEE:
					{
						auto value = IRB.CreateCall(_eei.Func_getBasefee, {});
						auto result = IRB.CreateZExt(value, Type::Word);
						stack.push(result);

						llvm::Instruction *Inst = llvm::cast<llvm::Instruction>(result);
						Inst->setMetadata(c_bytecodePC, llvm::MDNode::get(Context, llvm::ValueAsMetadata::get(Constant::get(pc))));
						break;
					}

					case Instruction::NUMBER:
					{
						// stack.push(IRB.CreateZExt(_runtimeManager.getTxContextItem(3), Type::Word));
						auto value = IRB.CreateCall(_eei.Func_getBlockNumber, {});
						auto result = IRB.CreateZExt(value, Type::Word);
						stack.push(result);

						llvm::Instruction *Inst = llvm::cast<llvm::Instruction>(result);
						Inst->setMetadata(c_bytecodePC, llvm::MDNode::get(Context, llvm::ValueAsMetadata::get(Constant::get(pc))));
						break;
					}
					case Instruction::TIMESTAMP:
					{
						// stack.push(IRB.CreateZExt(_runtimeManager.getTxContextItem(4), Type::Word));
						auto value = IRB.CreateCall(_eei.Func_getBlockTimestamp, {});
						auto result = IRB.CreateZExt(value, Type::Word);
						stack.push(result);

						llvm::Instruction *Inst = llvm::cast<llvm::Instruction>(result);
						Inst->setMetadata(c_bytecodePC, llvm::MDNode::get(Context, llvm::ValueAsMetadata::get(Constant::get(pc))));
						break;
					}
					case Instruction::CALLVALUE:
					{
						// auto beValue = _runtimeManager.getValue();
						// stack.push(Endianness::toNative(IRB, beValue));
						auto ValPtr = IRB.CreateAlloca(Type::Int128Ty, nullptr);
						IRB.CreateCall(_eei.Func_getCallValue, {ValPtr});
						auto value = IRB.CreateLoad(ValPtr);
						stack.push(value);

						llvm::Instruction *Inst = llvm::cast<llvm::Instruction>(value);
						Inst->setMetadata(c_bytecodePC, llvm::MDNode::get(Context, llvm::ValueAsMetadata::get(Constant::get(pc))));
						break;
					}

					case Instruction::CODESIZE:
					{
						// stack.push(_runtimeManager.getCodeSize());
						// auto value = IRB.CreateCall(_eei.Func_getCodeSize, {});
						stack.push(IRB.getInt32((uint32_t)Bytecode.size()));

						break;
					}
					case Instruction::CALLDATASIZE:
					{
						// stack.push(_runtimeManager.getCallDataSize());
						auto value = IRB.CreateCall(_eei.Func_getCallDataSize, {});
						stack.push(value);
						break;
					}
					case Instruction::RETURNDATASIZE:
					{
						// if (m_rev < EVMC_BYZANTIUM)
						// 	goto invalidInstruction;

						// auto returnBufSizePtr = _runtimeManager.getReturnBufSizePtr();
						// auto returnBufSize = IRB.CreateLoad(returnBufSizePtr);
						// stack.push(IRB.CreateZExt(returnBufSize, Type::Word));
						auto value = IRB.CreateCall(_eei.Func_getReturnDataSize, {});
						stack.push(value);
						break;
					}

					case Instruction::BLOCKHASH:
					{
						auto number = stack.pop();
						auto limitC = IRB.getInt64(std::numeric_limits<int64_t>::max());
						auto limit = IRB.CreateZExt(limitC, Type::Word);
						auto isBigNumber = IRB.CreateICmpUGT(number, limit);
						if (number->getType() != Type::Int64Ty)
							number = IRB.CreateTruncOrBitCast(number, Type::Int64Ty);
						auto ValPtr = IRB.CreateAlloca(Type::Int256Ty, nullptr);
						auto destMemIdx = IRB.CreateBitCast(ValPtr, Type::Int32PtrTy);
						IRB.CreateCall(_eei.Func_getBlockHash, {number, destMemIdx});
						auto hash = IRB.CreateSelect(isBigNumber, Constant::get(0), IRB.CreateLoad(ValPtr));
						hash = _eei.emitEndianConvert(hash);
						stack.push(hash);

						llvm::Instruction *Inst = llvm::cast<llvm::Instruction>(hash);
						Inst->setMetadata(c_bytecodePC, llvm::MDNode::get(Context, llvm::ValueAsMetadata::get(Constant::get(pc))));
						break;
					}

					case Instruction::BALANCE:
					{
						auto address = stack.pop();
						if (address->getType() != Type::AddressTy)
							address = IRB.CreateTruncOrBitCast(address, Type::AddressTy);
						auto AddressPtr = IRB.CreateAlloca(Type::AddressTy, nullptr);
						auto ValPtr = IRB.CreateAlloca(Type::Int128Ty, nullptr);
						IRB.CreateStore(address, AddressPtr);
						IRB.CreateCall(_eei.Func_getExternalBalance, {IRB.CreateBitCast(AddressPtr, Type::Int32PtrTy), IRB.CreateBitCast(ValPtr, Type::Int32PtrTy)});
						auto value = IRB.CreateLoad(ValPtr);
						stack.push(value);

						llvm::Instruction *Inst = llvm::cast<llvm::Instruction>(value);
						Inst->setMetadata(c_bytecodePC, llvm::MDNode::get(Context, llvm::ValueAsMetadata::get(Constant::get(pc))));
						break;
					}

					case Instruction::EXTCODESIZE:
					{
						auto address = stack.pop();

						if (address->getType() != Type::AddressTy)
							address = IRB.CreateTruncOrBitCast(address, Type::AddressTy);

						auto AddressPtr = IRB.CreateAlloca(Type::AddressTy, nullptr);
						if (address->getType() != Type::AddressTy)
							address = IRB.CreateTruncOrBitCast(address, Type::AddressTy);
						IRB.CreateStore(_eei.emitEndianConvert(address), AddressPtr);

						auto value = IRB.CreateCall(_eei.Func_getExternalCodeSize, {IRB.CreateBitCast(AddressPtr, Type::Int32PtrTy)});
						stack.push(value);
						break;
					}

					case Instruction::EXTCODEHASH:
					{
						auto address = stack.pop();
						if (address->getType() != Type::AddressTy)
							address = IRB.CreateTruncOrBitCast(address, Type::AddressTy);
						auto AddressPtr = IRB.CreateAlloca(Type::AddressTy, nullptr);
						IRB.CreateStore(address, AddressPtr);

						auto resultPtr = IRB.CreateAlloca(Type::Int32Ty, nullptr);

						IRB.CreateCall(_eei.Func_getExternalCodeHash, {IRB.CreateBitCast(AddressPtr, Type::Int32PtrTy), resultPtr});
						auto value = IRB.CreateLoad(resultPtr);
						stack.push(value);
						break;
					}

					case Instruction::CALLDATACOPY:
					{
						// std::cerr << "---------- CALLDATACOPY ---------\n";
						auto destMemIdx = stack.pop();
						auto srcIdx = stack.pop();
						auto reqBytes = stack.pop();
						if (destMemIdx->getType() != Type::Int8PtrTy)
							destMemIdx = IRB.CreateIntToPtr(destMemIdx, Type::Int8PtrTy);
						if (srcIdx->getType() != Type::Int32Ty)
							srcIdx = IRB.CreateTruncOrBitCast(srcIdx, Type::Int32Ty);
						if (reqBytes->getType() != Type::Int32Ty)
							reqBytes = IRB.CreateTruncOrBitCast(reqBytes, Type::Int32Ty);

						auto callInst = IRB.CreateCall(_eei.Func_callDataCopy, {destMemIdx, srcIdx, reqBytes});
						llvm::Instruction *Inst = llvm::cast<llvm::Instruction>(callInst);
						Inst->setMetadata(c_bytecodePC, llvm::MDNode::get(Context, llvm::ValueAsMetadata::get(Constant::get(pc))));
						break;
					}

					case Instruction::RETURNDATACOPY:
					{
						// if (m_rev < EVMC_BYZANTIUM)
						// 	goto invalidInstruction;

						auto destMemIdx = stack.pop();
						auto srcIdx = stack.pop();
						auto reqBytes = stack.pop();

						if (destMemIdx->getType() != Type::Int8PtrTy)
							destMemIdx = IRB.CreateIntToPtr(destMemIdx, Type::Int8PtrTy);
						if (srcIdx->getType() != Type::Int32Ty)
							srcIdx = IRB.CreateTruncOrBitCast(srcIdx, Type::Int32Ty);
						if (reqBytes->getType() != Type::Int32Ty)
							reqBytes = IRB.CreateTruncOrBitCast(reqBytes, Type::Int32Ty);

						IRB.CreateCall(_eei.Func_returnDataCopy, {destMemIdx, srcIdx, reqBytes});
						// auto srcPtr = IRB.CreateLoad(_runtimeManager.getReturnBufDataPtr());
						// auto srcSize = IRB.CreateLoad(_runtimeManager.getReturnBufSizePtr());

						// _memory.copyBytesNoPadding(srcPtr, srcSize, srcIdx, destMemIdx, reqBytes);
						break;
					}

					case Instruction::CODECOPY:
					{
						auto destMemIdx = stack.pop();
						auto srcIdx = stack.pop();
						auto reqBytes = stack.pop();

						if (destMemIdx->getType() != Type::Int8PtrTy)
							destMemIdx = IRB.CreateIntToPtr(destMemIdx, Type::Int8PtrTy);
						if (srcIdx->getType() != Type::Int32Ty)
							srcIdx = IRB.CreateTruncOrBitCast(srcIdx, Type::Int32Ty);
						if (reqBytes->getType() != Type::Int32Ty)
							reqBytes = IRB.CreateTruncOrBitCast(reqBytes, Type::Int32Ty);

            if (!Module.getNamedGlobal("evmCode")) {
              std::string str(Bytecode.begin(), Bytecode.end());
              IRB.CreateGlobalStringPtr(llvm::StringRef(str), "evmCode");
            }

            llvm::Value *pStr = IRB.CreateGEP(
                Module.getNamedGlobal("evmCode"), 
                {llvm::ConstantInt::get(Type::Int32Ty, 0), srcIdx});

            IRB.CreateCall(_eei.Func_memcpy, {destMemIdx, pStr, reqBytes});
            // llvm::CallInst *memcpy_call = IRB.CreateMemCpy(destMemIdx, 8, pStr, 8, reqBytes, true);
            break;
					}

					case Instruction::EXTCODECOPY:
					{
						auto addr = stack.pop();
						auto destMemIdx = stack.pop();
						auto srcIdx = stack.pop();
						auto reqBytes = stack.pop();

						if (addr->getType() != Type::AddressTy)
							addr = IRB.CreateTruncOrBitCast(addr, Type::AddressTy);

						auto AddressPtr = IRB.CreateAlloca(Type::AddressTy, nullptr);
						IRB.CreateStore(addr, AddressPtr);
						auto AddressPtr32 = IRB.CreateBitCast(AddressPtr, Type::Int32PtrTy);
						if (destMemIdx->getType() != Type::Int32PtrTy)
							destMemIdx = IRB.CreateIntToPtr(destMemIdx, Type::Int32PtrTy);
						if (srcIdx->getType() != Type::Int32Ty)
							srcIdx = IRB.CreateTruncOrBitCast(srcIdx, Type::Int32Ty);
						if (reqBytes->getType() != Type::Int32Ty)
							reqBytes = IRB.CreateTruncOrBitCast(reqBytes, Type::Int32Ty);

						IRB.CreateCall(_eei.Func_externalCodeCopy, {AddressPtr32, destMemIdx, srcIdx, reqBytes});
						// auto codeRef = _ext.extcode(addr);
						// _memory.copyBytes(codeRef.ptr, codeRef.size, srcIdx, destMemIdx, reqBytes);
						break;
					}

					case Instruction::CALLDATALOAD:
					{
						// std::cerr << "---------- CALLDATALOAD \n";
						auto idx = stack.pop();

						if (env_ewasm()) {
							if (idx->getType() != Type::Int32Ty)
								idx = IRB.CreateTruncOrBitCast(idx, Type::Int32Ty);
							auto dest = IRB.CreateAlloca(Type::Word, nullptr);
							auto destMemIdx = IRB.CreateBitCast(dest, Type::BytePtr);
							IRB.CreateCall(_eei.Func_callDataCopy, {destMemIdx, idx, IRB.getInt32(32)});
							auto value = IRB.CreateLoad(Type::Word, dest);
							auto result = _eei.emitEndianConvert(value);
							stack.push(result);

						} else if (env_near()) {
							// IRB.CreateCall(Env.fn_input(), {Env.ATOMIC_OP_REGISTER});
							// auto dest = IRB.CreateAlloca(Type::Word, nullptr);

							// IRB.CreateCall(Env.fn_read_register(),
							// 	{Env.ATOMIC_OP_REGISTER, IRB.CreateBitCast(dest, Type::Int64PtrTy)});
							// auto value = IRB.CreateLoad(Type::Word, dest);
							// stack.push(value);
						}
						break;

						// std::cerr << "[-] I'am CALLDATALOAD \n";
						// auto idx = stack.pop();
						// if (idx->getType() != Type::Int32Ty)
						// 	idx = IRB.CreateTruncOrBitCast(idx, Type::Int32Ty);

						// auto dest = IRB.CreateAlloca(Type::Int256Ty, nullptr);
						// // auto destMemIdx = IRB.CreateBitCast(dest, Type::BytePtr);

						// auto destMemIdx = IRB.CreateIntToPtr(dest, Type::Int32PtrTy);

						// // auto size = IRB.CreateCall(_eei.Func_getCallDataSize, {});
						// // IRB.CreateCall(_eei.Func_callDataCopy, {destMemIdx, idx, size});
						// IRB.CreateCall(_eei.Func_callDataCopy, {destMemIdx, idx, IRB.getInt32(32)});
						// auto value = IRB.CreateLoad(Type::Int256Ty, destMemIdx);
						// // stack.push(value);
						// auto result = _eei.emitEndianConvert(value);
						// stack.push(result);
						// break;
					}

					case Instruction::CREATE:
					{
						// if (m_staticCall)
						// 	goto invalidInstruction;

						auto endowment = stack.pop();
						auto initOff = stack.pop();
						auto initSize = stack.pop();

						auto ValPtr = IRB.CreateAlloca(Type::Int128Ty, nullptr);
						IRB.CreateStore(IRB.CreateTruncOrBitCast(endowment, Type::Int128Ty), ValPtr);
						auto ValPtr32 = IRB.CreateBitCast(ValPtr, Type::Int32PtrTy);

						if (initOff->getType() != Type::Int32PtrTy)
							initOff = IRB.CreateIntToPtr(initOff, Type::Int32PtrTy);
						if (initSize->getType() != Type::Int32Ty)
							initSize = IRB.CreateTruncOrBitCast(initSize, Type::Int32Ty);

						auto AddressPtr = IRB.CreateAlloca(Type::AddressTy, nullptr);
						auto addr = IRB.CreateBitCast(AddressPtr, Type::Int32PtrTy);

						IRB.CreateCall(_eei.Func_create, {ValPtr32, initOff, initSize, addr});
						auto result = IRB.CreateLoad(AddressPtr);
						stack.push(IRB.CreateZExt(result, Type::Int256Ty));
						break;
					}

					case Instruction::CREATE2:
					{
						// if (m_staticCall)
						// 	goto invalidInstruction;
						// llvm::errs() << "Create222222";
						// exit(-1);
						auto endowment = stack.pop();
						auto initOff = stack.pop();
						auto initSize = stack.pop();
						auto salt = stack.pop();

						auto ValPtr = IRB.CreateAlloca(Type::Int128Ty, nullptr);
						IRB.CreateStore(IRB.CreateTruncOrBitCast(endowment, Type::Int128Ty), ValPtr);
						auto ValPtr32 = IRB.CreateBitCast(ValPtr, Type::Int32PtrTy);

						if (initOff->getType() != Type::Int32PtrTy)
							initOff = IRB.CreateIntToPtr(initOff, Type::Int32PtrTy);
						if (initSize->getType() != Type::Int32Ty)
							initSize = IRB.CreateTruncOrBitCast(initSize, Type::Int32Ty);

						auto AddressPtr = IRB.CreateAlloca(Type::AddressTy, nullptr);
						auto addr = IRB.CreateBitCast(AddressPtr, Type::Int32PtrTy);

						auto saltPtr = IRB.CreateAlloca(Type::Int128Ty, nullptr);
						IRB.CreateStore(IRB.CreateTruncOrBitCast(salt, Type::Int128Ty), saltPtr);
						auto saltPtr32 = IRB.CreateBitCast(saltPtr, Type::Int32PtrTy);

						IRB.CreateCall(_eei.Func_create2, {ValPtr32, initOff, initSize, saltPtr32, addr});
						auto result = IRB.CreateLoad(AddressPtr);
						stack.push(IRB.CreateZExt(result, Type::Int256Ty));

						break;
					}

					case Instruction::CALL:
					case Instruction::CALLCODE:
					{
						auto callGas = stack.pop();
						auto address = stack.pop();
						auto value = stack.pop();
						auto inOff = stack.pop();
						auto inSize = stack.pop();
						auto outOff = stack.pop();
						auto outSize = stack.pop();

						if (callGas->getType() != Type::Int64Ty)
							callGas = IRB.CreateTruncOrBitCast(callGas, Type::Int64Ty, "gasI64");

						llvm::Value *AddressPtr = IRB.CreateAlloca(Type::AddressTy, nullptr, "address.ptr");
						if (address->getType() != Type::AddressTy)
							address = IRB.CreateTruncOrBitCast(address, Type::AddressTy);

						IRB.CreateStore(_eei.emitEndianConvert(address), AddressPtr);

						llvm::Value *ValuePtr = IRB.CreateAlloca(Type::Int128Ty, nullptr, "value.ptr");
						if (value->getType() != Type::Int128Ty)
							value = IRB.CreateTruncOrBitCast(value, Type::Int128Ty);
						IRB.CreateStore(value, ValuePtr);

						if (ValuePtr->getType() != Type::Int32PtrTy)
							ValuePtr = IRB.CreateTruncOrBitCast(ValuePtr, Type::Int32PtrTy);

						if (inOff->getType() != Type::Int8PtrTy)
							inOff = IRB.CreateIntToPtr(inOff, Type::Int8PtrTy);
						if (inSize->getType() != Type::Int32Ty)
							inSize = IRB.CreateTruncOrBitCast(inSize, Type::Int32Ty);
						auto func = (inst == Instruction::CALL) ? _eei.Func_call : _eei.Func_callCode;
						auto r = IRB.CreateCall(func, {callGas, AddressPtr, ValuePtr, inOff, inSize}, "call_ret");
						auto _valided = IRB.CreateICmpEQ(r, llvm::ConstantInt::get(r->getType(), 0), "valided");
						stack.push(IRB.CreateSelect(_valided, Constant::get(1), Constant::get(0)));

						if (outOff->getType() != Type::Int8PtrTy)
							outOff = IRB.CreateIntToPtr(outOff, Type::Int8PtrTy);
						if (outSize->getType() != Type::Int32Ty)
							outSize = IRB.CreateTruncOrBitCast(outSize, Type::Int32Ty);
						IRB.CreateCall(_eei.Func_returnDataCopy, {outOff, IRB.getInt32(0), outSize});

						llvm::Instruction *Inst = llvm::cast<llvm::Instruction>(r);
						Inst->setMetadata(c_bytecodePC, llvm::MDNode::get(Context, llvm::ValueAsMetadata::get(Constant::get(pc))));
						break;
					}
					case Instruction::DELEGATECALL:
					case Instruction::STATICCALL:
					{
						auto callGas = stack.pop();
						auto address = stack.pop();
						auto inOff = stack.pop();
						auto inSize = stack.pop();
						auto outOff = stack.pop();
						auto outSize = stack.pop();

						if (callGas->getType() != Type::Int64Ty)
							callGas = IRB.CreateTruncOrBitCast(callGas, Type::Int64Ty);

						if (address->getType() != Type::AddressTy)
							address = IRB.CreateTruncOrBitCast(address, Type::AddressTy);
						llvm::Value *AddressPtr = IRB.CreateAlloca(Type::AddressTy, nullptr, "address.ptr");

						IRB.CreateStore(_eei.emitEndianConvert(address), AddressPtr);

						if (inOff->getType() != Type::Int8PtrTy)
							inOff = IRB.CreateIntToPtr(inOff, Type::Int8PtrTy);
						if (inSize->getType() != Type::Int32Ty)
							inSize = IRB.CreateTruncOrBitCast(inSize, Type::Int32Ty);

						auto func = (inst == Instruction::DELEGATECALL) ? _eei.Func_callDelegate : _eei.Func_callStatic;
						auto r = IRB.CreateCall(func, {callGas, AddressPtr, inOff, inSize});

						auto _valided = IRB.CreateICmpEQ(r, llvm::ConstantInt::get(r->getType(), 0), "valided");
						stack.push(IRB.CreateSelect(_valided, Constant::get(1), Constant::get(0)));

						if (outOff->getType() != Type::Int8PtrTy)
							outOff = IRB.CreateIntToPtr(outOff, Type::Int8PtrTy);
						if (outSize->getType() != Type::Int32Ty)
							outSize = IRB.CreateTruncOrBitCast(outSize, Type::Int32Ty);
						IRB.CreateCall(_eei.Func_returnDataCopy, {outOff, IRB.getInt32(0), outSize});

						llvm::Instruction *Inst = llvm::cast<llvm::Instruction>(r);
						Inst->setMetadata(c_bytecodePC, llvm::MDNode::get(Context, llvm::ValueAsMetadata::get(Constant::get(pc))));
						break;
					}

					case Instruction::RETURN:
          {
						auto index = stack.pop();
						auto size = stack.pop();

            if (env_ewasm()) {
              index = IRB.CreateIntToPtr(index, Type::Int8PtrTy);
              size = IRB.CreateTruncOrBitCast(size, Type::Int32Ty);
              
              if (RetPC && !RtWasmCode.empty() && _basicBlock.firstInstrIdx() + it - _basicBlock.begin() == RetPC) {
                auto Zero = llvm::ConstantInt::get(Type::Int32Ty, 0);
                IRB.CreateGlobalStringPtr(RtWasmCode, "runtimeCode");
                llvm::Value *pStr = IRB.CreateGEP(
                  Module.getNamedGlobal("runtimeCode"), {Zero, Zero});
                IRB.CreateCall(_eei.Func_finish, {pStr, IRB.getInt32(static_cast<uint32_t>(RtWasmCode.size()))});
              } else {
                IRB.CreateCall(_eei.Func_finish, {index, size});
              }

              IRB.CreateBr(m_exitBB);
              
            } else if (env_near()) {
                index = IRB.CreateIntToPtr(index, Type::Int64PtrTy);
                size = IRB.CreateTruncOrBitCast(size, Type::Int64Ty);
                // IRB.CreateCall(Env, {size, index});
                IRB.CreateBr(m_exitBB);
            }

						break;
          }

					case Instruction::REVERT:
					{
						auto index = stack.pop();
						auto size = stack.pop();
            index = IRB.CreateIntToPtr(index, Type::Int8PtrTy);
            size = IRB.CreateTruncOrBitCast(size, Type::Int32Ty);
            IRB.CreateCall(_eei.Func_revert, {index, size});
						IRB.CreateBr(m_exitBB);
						break;
					}

					case Instruction::SUICIDE:
					{
						auto dest = stack.pop();
						auto callInst = IRB.CreateCall(_eei.Func_selfDestruct, {IRB.CreateIntToPtr(dest, Type::Int32PtrTy)});

						IRB.CreateBr(m_exitBB);
						// nextBlockIdxs.push_back(Constant::get(-2));

						llvm::Instruction *Inst = llvm::cast<llvm::Instruction>(callInst);
						Inst->setMetadata(c_bytecodePC, llvm::MDNode::get(Context, llvm::ValueAsMetadata::get(Constant::get(pc))));
						break;
					}
					case Instruction::STOP:
					{
						IRB.CreateBr(m_exitBB);
						// nextBlockIdxs.push_back(Constant::get(-2));
						break;
					}

					case Instruction::LOG0:
					case Instruction::LOG1:
					case Instruction::LOG2:
					case Instruction::LOG3:
					case Instruction::LOG4:
					{
						// if (m_staticCall)
						// 	goto invalidInstruction;

						auto beginIdx = stack.pop();
						auto numBytes = stack.pop();

						auto beginPtr = IRB.CreateIntToPtr(beginIdx, Type::Int32PtrTy);
						if (numBytes->getType() != Type::Int32Ty)
							numBytes = IRB.CreateTruncOrBitCast(numBytes, Type::Int32Ty);

						llvm::SmallVector<llvm::Value *, 4> topics;
						uint32_t numTopics = static_cast<uint32_t>(inst) - static_cast<uint32_t>(Instruction::LOG0);

						auto number = IRB.getInt32(numTopics);

						for (size_t i = 0; i < numTopics; ++i)
						{
							llvm::Value *topic = stack.pop();
							auto _adr = IRB.CreateAlloca(topic->getType(), nullptr);
							IRB.CreateStore(topic, _adr);
							topics.emplace_back(IRB.CreateBitCast(_adr, Type::Int32PtrTy));
						}
						for (size_t i = numTopics; i < 4; ++i)
							topics.emplace_back(llvm::ConstantPointerNull::get(Type::Int32PtrTy));

						IRB.CreateCall(_eei.Func_log, {beginPtr, numBytes, number, topics[0], topics[1], topics[2], topics[3]});
						// _ext.log(beginIdx, numBytes, topics);
						break;
					}

					default: // Invalid instruction - abort
						std::cerr << "INVALID Opcode: " << std::hex << static_cast<size_t>(inst) << "; lift to `unreachable`.\n";
						IRB.CreateUnreachable();
						// IRB.CreateBr(m_exitBB);
						// nextBlockIdxs.push_back(Constant::get(-2));
						it = _basicBlock.end() - 1; // finish block compilation
					}
				}

				// _gasMeter.commitCostBlock();

				// stack.finalize();
				// std::cerr << "[-] single bb-end-----------------";
				return nextBlockIdxs;
			}

		}
	}
}

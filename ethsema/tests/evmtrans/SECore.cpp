#include <iomanip>
#include <iostream>

#include "preprocessor/llvm_includes_end.h"
#include "preprocessor/llvm_includes_start.h"
#include "z3++.h"
#include <llvm/IR/IntrinsicInst.h>
#include <llvm/IR/Module.h>

#include "SECore.h"
#include "Type.h"
// #include "Endianness.h"
// #include "Utils.h"

// using namespace z3;
using namespace llvm;

namespace se
{
// struct Cell;

// Node::Node(llvm::BasicBlock* _bbPtr):
// 	bbPtr{_bbPtr},
// 	uid(0)
// {}

ExecutionState::ExecutionState(unsigned _regsNum, z3::expr _x)
{
    gas = 0;
    regsNum = _regsNum;

    for (uint i = 0; i < _regsNum; ++i)
    {
        locals.push_back(_x);
    }
}

ExecutionState::ExecutionState(const ExecutionState& state) : gas(state.gas), regsNum(state.regsNum)
{
    // errs() << "\n[ExecutionState()] " << "regsNum = " << regsNum << "; state.locals.size() = " <<
    // state.locals.size() << "\n";


    for (unsigned i = 0; i < regsNum; i++)
    {
        // state.locals.pop_back();
        locals.push_back(state.locals[i]);
        // errs() << "copy [" << i << "]\n";
        // locals.push_back(state.locals[i]);
        // state.locals[i] = expr;
    }
}

KInstruction::~KInstruction()
{
    // delete[] operands;
}


SEngine::~SEngine()
{
    for (uint i = 0; i < m_numInstructions; ++i)
    {
        // delete[] m_instructions[i];
        delete m_instructions[i];
    }
    delete m_instructions;

    for (unsigned i = 0; i < m_Nodes.size(); ++i)
    {
        // llvm::errs() << "killed Node\n";
        for (unsigned j = 0; j < m_Nodes[i]->states.size(); ++j)
            delete m_Nodes[i]->states[j];
    }
}

const z3::expr& SEngine::eval(KInstruction* ki, unsigned index, ExecutionState& state) const
{
    assert(index < ki->inst->getNumOperands());
    int vnumber = ki->operands[index];

    assert(vnumber != -1 && "Invalid operand to eval(), not a value or constant!");
    errs() << "[eval] index =" << index << "; vnumber = " << vnumber << " \n";

    if (vnumber == -2)
    {
        // unused global variable
        llvm::Value* gvar = ki->inst->getOperand(index);
        auto it = m_gZ3Map.find(gvar->getName());
        assert(it != m_gZ3Map.end() && "GlobalVariable does not exit.\n");
        return it->second;

        // AllocaInst* ai = cast<AllocaInst>(i);
        // unsigned elementBytesSize = m_targetData->getTypeStoreSize(ai->getAllocatedType());
        // // z3::expr size = Expr::createPointer(elementSize);
        // // if (ai->isArrayAllocation())
        // // {
        // //     z3::expr count = eval(ki, 0, state);
        // //     count = Expr::createZExtToPointerWidth(count);
        // //     size = size * count;
        // // }
        // // TODO
        // errs() << "Dest= " << ki->dest << " ; Ptr Size= " << elementBytesSize << "B\n";
        // // z3::expr count = eval(ki, 0, state);

        // std::stringstream _name;
        // _name << "%" << static_cast<void *>(i);
        // errs() << "[SE] " << _name.str().c_str() << " = allocate " << 8*elementBytesSize <<
        // "bits\n"; z3::expr reg = ExprCtx.bv_const(_name.str().c_str(), 8*elementBytesSize);
        // bindLocal(ki, state, reg); // allocate a register
        // break;
    }
    // Determine if this is a constant or not.
    else if (vnumber <= -3)
    {
        // constant
        // if (auto constant = dyn_cast<llvm::ConstantInt>(ki->inst->getOperand(index)))
        // {
        //     // llvm:Value* constant = ki->inst->getOperand(index);
        //     z3::expr z3Const = ExprCtx.bv_val(0, constant->getValue().getBitWidth());
        //     auto pVal = constant->getValue().getRawData();
        //     for (int j = constant->getValue().getNumWords() - 1; j >= 0 ; --j)
        //     {
        //         z3Const = z3::shl(z3Const, 64);
        //         z3Const = (z3Const | ExprCtx.bv_val(static_cast<uint64_t>(pVal[j]),
        //         constant->getValue().getBitWidth()));
        //         // errs() << "[getOperandNum] over-size constant[" << j << "]= " <<
        //         static_cast<uint64_t>(pVal[j]) << "; z3Const = " <<
        //         z3Const.simplify().to_string() << "\n";
        //     }
        //     return z3Const.simplify();
        //     // return ExprCtx.bv_val(0, 256);
        // }


        // errs() << "[getOperandNum] Gen->" << z3Const.simplify().to_string() <<  "\n";
        // m_constants.push_back();
        // errs() << "Constant  # " << constant->getBitWidth()  << "bits; [" <<
        // -1*static_cast<int>(m_constants.size()) - 1 << "]\n"; return
        // -1*static_cast<int>(m_constants.size()) - 1; // starting from -3

        unsigned index = -vnumber - 3;
        return m_constants[index];
    }
    else
    {
        // errs() << "locals Size= " << state.locals.size() << "\n";
        return state.locals[vnumber];
    }
    // errs() << "end -eval\n";
}

KConstant* SEngine::getKConstant(const llvm::ConstantInt* c)
{
    // auto it = constantMap.find(c);
    // if (it != constantMap.end())
    //     return it->second.get();
    // return NULL;
}

unsigned SEngine::getConstantID(llvm::ConstantInt* c, KInstruction* ki)
{
    // if (KConstant* kc = getKConstant(c))
    //     return kc->id;

    // unsigned id = constants.size();
    // auto kc = std::unique_ptr<KConstant>(new KConstant(c, id, ki));
    // constantMap.insert(std::make_pair(c, std::move(kc)));
    // constants.push_back(c);
    // return id;
}

/***/

KConstant::KConstant(llvm::ConstantInt* _ct, unsigned _id, KInstruction* _ki)
{
    ct = _ct;
    id = _id;
    ki = _ki;
}


SEngine::SEngine(Module* _module) : m_module(_module)
{
    m_targetData = new llvm::DataLayout(_module);
}

z3::expr resizeVec(z3::expr _e, unsigned _w, bool isSigned)
{
    assert(_e.is_bv() && "We need boolean or BitVec.\n");
    unsigned size = _e.get_sort().bv_size();
    errs() << "[resizeVec]" 
           << size << "bits ->" << _w << "bits\n";

    // if (size == _w && _e.is) return _e;
    if (size < _w)
        return isSigned ? z3::sext(_e, _w - size) : z3::zext(_e, _w - size);
    else
        return _e.extract(_w - 1, 0);
}

unsigned SEngine::getWidthForLLVMType(llvm::Type* type) const
{
    if (type->getNumContainedTypes() > 0)
    {
        errs() << "getPrim = " << type->getContainedType(0)->getPrimitiveSizeInBits() << "\n";
        return type->getContainedType(0)->getPrimitiveSizeInBits();
    }
    else
    {
        errs() << "getTypeSizeInBits = " << m_targetData->getTypeSizeInBits(type) << "\n";
        return m_targetData->getTypeSizeInBits(type);
    }
    // errs() << "getPrim = " << type->getContainedType(0)->getPrimitiveSizeInBits() << "\n";
    // errs() << "getTypeSizeInBits = " << m_targetData->getTypeSizeInBits(type) << "\n";
    // return m_targetData->getTypeSizeInBits(type);
    // return m_targetData->getTypeAllocSize(type);
    // return type->getIntegerBitWidth();
    // return type->getBitWidth();
    if (type->isIntegerTy())
    {
        return type->getIntegerBitWidth();
    }
    else if (type->isPointerTy())
    {
        return type->getPointerAddressSpace();
    }
    else
    {
        errs() << "Unknow Type::" << type->getTypeID() << "\n";
        exit(-1);
    }

    // *    # PrimitiveTypes - make sure LastPrimitiveTyID stays up to date.
    // *    VoidTyID = 0,    ///<  0: type with no size
    // *    HalfTyID,        ///<  1: 16-bit floating point type
    // *    FloatTyID,       ///<  2: 32-bit floating point type
    // *    DoubleTyID,      ///<  3: 64-bit floating point type
    // *    X86_FP80TyID,    ///<  4: 80-bit floating point type (X87)
    // *    FP128TyID,       ///<  5: 128-bit floating point type (112-bit mantissa)
    // *    PPC_FP128TyID,   ///<  6: 128-bit floating point type (two 64-bits, PowerPC)
    // *    LabelTyID,       ///<  7: Labels
    // *    MetadataTyID,    ///<  8: Metadata
    // *    X86_MMXTyID,     ///<  9: MMX vectors (64 bits, X86 specific)
    // *    TokenTyID,       ///< 10: Tokens
    // *
    // *    # Derived types... see DerivedTypes.h file.
    // *    # Make sure FirstDerivedTyID stays up to date!
    // *    IntegerTyID,     ///< 11: Arbitrary bit width integers
    // *    FunctionTyID,    ///< 12: Functions
    // *    StructTyID,      ///< 13: Structures
    // *    ArrayTyID,       ///< 14: Arrays
    // *    PointerTyID,     ///< 15: Pointers
    // *    VectorTyID       ///< 16: SIMD 'packed' format, or other vector type
}


int SEngine::getOperandNum(
    Value* v, std::map<llvm::Instruction*, unsigned>& registerMap, KInstruction* ki)
{
    if (llvm::Instruction* inst = dyn_cast<llvm::Instruction>(v))
    {
        // %x
        errs() << "instruction #" << registerMap[inst] << "\n";
        return registerMap[inst];
    }
    else if (llvm::Argument* a = dyn_cast<llvm::Argument>(v))
    {
        // args
        errs() << "argNumber  #" << a->getArgNo() << "\n";
        return a->getArgNo();
    }
    else if (isa<llvm::BasicBlock>(v) || isa<llvm::InlineAsm>(v) || isa<llvm::MetadataAsValue>(v))
    {
        // satic one
        errs() << "label     # -1\n";
        return -1;
    }
    else if (auto constant = dyn_cast<llvm::ConstantInt>(v))
    {
        // constant

        errs() << "[getOperandNum] case ConstantInt " << constant->getValue().getBitWidth()
               << " == " << constant->getValue().getNumWords() << " *64bits\n";
        z3::expr z3Const = ExprCtx.bv_val(0, constant->getValue().getBitWidth());
        auto pVal = constant->getValue().getRawData();
        for (int j = constant->getValue().getNumWords() - 1; j >= 0; --j)
        {
            z3Const = z3::shl(z3Const, 64);
            z3Const = (z3Const | ExprCtx.bv_val(static_cast<uint64_t>(pVal[j]),
                                     constant->getValue().getBitWidth()));
            // errs() << "[getOperandNum] over-size constant[" << j << "]= " <<
            // static_cast<uint64_t>(pVal[j]) << "; z3Const = " << z3Const.simplify().to_string() <<
            // "\n";
        }
        // errs() << "[getOperandNum] Gen->" << z3Const.simplify().to_string() <<  "\n";
        m_constants.push_back(z3Const.simplify());


        errs() << "Constant  # " << constant->getBitWidth() << "bits; ["
               << -1 * static_cast<int>(m_constants.size()) - 1 << "]\n";
        return -1 * static_cast<int>(m_constants.size()) - 2;  // starting from -3
    }
    else if (auto gVar = dyn_cast<llvm::GlobalVariable>(v))
    {
        return -2;
        // return gVarmap[v->getName()];
    }
    else
    {
        return -1;
        // (auto *GEP = dyn_cast<GEPOperator>(v))
        // inline arg
        // for (auto& v : getUsedVars(&I)) {
        //     Var.insert(v);
        // }
        // if (auto v = getAssignedVars(&I)) {
        //     Var.insert(v);
        // }
        // satic one

        // m_complexRegs.push_back(v); // this is an express. TODO
        // return -1;
        // llvm::dyn_cast<llvm::ConstantInt>(idx)->getSExtValue() << "\n";
    }
}

void SEngine::run(llvm::Function* function)
{

    std::map<llvm::Instruction*, unsigned> registerMap;
    unsigned rnum = 0;  // we assume there are no arguments in this function; function->arg_size()
    // initate registers
    for (auto& bb : *function)
    {
        m_basicBlockEntry[&bb] = m_numInstructions;
        m_numInstructions += bb.size();

        for (auto it = bb.begin(); it != bb.end(); ++it)
        {
            registerMap[&*it] = rnum++;
            // errs << &*it.getNumRegs() "\n";
        }
    }

    // std::map<std::string, unsigned> gVarMap;
    for (llvm::GlobalVariable& g : m_module->getGlobalList())
    {
        errs() << g.getName() << " :: " << getWidthForLLVMType(g.getType()) << "bits\n";
        // errs() << g.getName() << " :: " << g.getType()->getTypeID() << "bits  " << g.getType() <<
        // "\n";


        if (g.hasName())
        {
            z3::expr _expr = ExprCtx.bv_val(0, 1);  // init
            std::string vName = g.getName();
            if (vName == "memory.size")
                _expr = ExprCtx.bv_const(vName.c_str(), 256);
            else if (vName == "mstk")
                _expr = ExprCtx.bv_const(vName.c_str(), 256);  // ExprCtx.constant(vName.c_str(),
                                                               // ExprCtx.array_sort(ExprCtx.bv_sort(128),
                                                               // ExprCtx.bv_sort(256)));
            else if (vName == "target")
                _expr = ExprCtx.bv_const(vName.c_str(), 256);
            else if (vName == "spPtr")
                _expr = ExprCtx.bv_const(vName.c_str(), 256);
            else
            {
                errs() << "Invalid globalvariable@ " << vName << "\n";
                exit(-1);
                break;
            }
            m_gZ3Map.insert({vName, _expr});
        }
        // llvm::Instruction* ai = cast<llvm::Instruction>(&g);
        // unsigned elementBytesSize = m_targetData->getTypeStoreSize(ai->getAllocatedType());
        // errs() << g.getName() << " :: " << elementBytesSize << "Bytes\n";
        // z3::expr size = Expr::createPointer(elementSize);
        // if (ai->isArrayAllocation())
        // {
        //     z3::expr count = eval(ki, 0, state);
        //     count = Expr::createZExtToPointerWidth(count);
        //     size = size * count;
        // }

        // if (g.isConstant())
        //     errs() << "NONO\n";
        // llvm::Value _i = dyn_cast<llvm::Value>(&g);
    }

    // for (Module::const_global_iterator I = m_module->global_begin(), E = m_module->global_end();
    //     I != E; ++I){
    //     llvm::GlobalVariable& tt = *I;
    //     // llvm::Instruction _i = dyn_cast<Instruction>(*I);
    //     // registerMap[_i] = rnum++;
    // }

    // for global variable
    // for (llvm::GlobalVariable* g : m_module->getGlobalList() )
    // if (llvm::GlobalVariable* g = dyn_cast<llvm::GlobalVariable>(v))   //isa_impl<GlobalVariable,
    // Value>
    // {
    //     errs() << "Others       #" << "  ::$" << g->getName() << "\n";
    //     if (g->getValueType() == llvm::ArrayType::get(Type::Int256Ty, 128))
    //     {
    //         // global array
    //     }
    //     else
    //     {
    //         m_gSymbols.push_back()
    //     }
    //     return m_gSymbols.size() - 1;
    // }

    m_instructions = new KInstruction*[m_numInstructions];
    m_numRegisters = rnum;

    // find the idx of operand
    unsigned i = 0;
    for (llvm::BasicBlock& bb : *function)
    {
        for (auto it = bb.begin(); it != bb.end(); ++it)
        {
            errs() << "\n[init ins]->" << it->getOpcodeName();
            for (unsigned j = 0; j < it->getNumOperands(); ++j)
            {
                llvm::Value* opnd = it->getOperand(j);
                if (opnd->hasName())
                {
                    errs() << " " << opnd->getName() << ",";
                }
                else
                {
                    errs() << " ptr" << opnd << ",";
                }
            }
            errs() << "\n";

            KInstruction* ki;

            switch (it->getOpcode())
            {
            case llvm::Instruction::GetElementPtr:
            case llvm::Instruction::InsertValue:
            case llvm::Instruction::ExtractValue:
                ki = new KGEPInstruction();
                break;
            default:
                ki = new KInstruction();
                break;
            }
            errs() << "-------------------------- setting operands idx --------------------\n";

            Instruction* inst = &*it;
            ki->inst = inst;
            ki->dest = registerMap[inst];

            if (isa<llvm::CallInst>(it) || isa<llvm::InvokeInst>(it))
            {
                const CallBase& cs = cast<llvm::CallBase>(*inst);
                Value* val = cs.getCalledOperand();
                unsigned numArgs = cs.arg_size();
                ki->operands = new int[numArgs + 1];
                ki->operands[0] = registerMap[inst];
                for (unsigned j = 0; j < numArgs; j++)
                {
                    Value* v = cs.getArgOperand(j);
                    ki->operands[j + 1] = getOperandNum(v, registerMap, ki);
                }
            }
            else
            {
                unsigned numOperands = it->getNumOperands();

                ki->operands = new int[numOperands];
                for (unsigned j = 0; j < numOperands; j++)
                {
                    Value* v = it->getOperand(j);
                    errs() << "[general] ki-operands[" << j << "]= "
                           << "\n";
                    ki->operands[j] = getOperandNum(v, registerMap, ki);
                }
            }

            m_instructions[i++] = ki;
        }
    }

    // llvm::raw_os_ostream cerr{std::cerr};
    if (false)
    {
        for (unsigned i = 0; i < m_numInstructions; ++i)
        {
            KInstruction ki = *m_instructions[i];
            // errs() << ki.inst->getOpcodeName() << " =====   Args# " <<
            // (ki.inst)->getNumOperands() << "\n";
            errs() << "%" << ki.inst << " = " << ki.inst->getOpcodeName() << "{"
                   << (ki.inst)->getNumOperands() << "}  ";
            // errs() << "%" << registerMap[ki.inst] << " = " << ki.inst->getOpcodeName() << "{" <<
            // (ki.inst)->getNumOperands() << "}  ";
            for (unsigned j = 0; j < ki.inst->getNumOperands(); j++)
            {
                // // errs() << "[" << j << "]:" << ki.operands[j] <<" | ";
                if (llvm::Instruction* inst = dyn_cast<llvm::Instruction>(ki.inst->getOperand(j)))
                {
                    errs() << "%" << inst->getOpcodeName() << "  ";
                }
                else
                {
                    errs() << "%" << ki.operands[j] << "  ";
                }

                // errs() << "%" << ki.inst->getOperand(j)->getName() << "  ";
            }
            errs() << "\n\n";
        }
    }
    processBB(&function->getEntryBlock(), 0, 0, ExprCtx.bool_val(true));  // starting from BB#0
}


/* Find path constraints of the basic block and add them to constraints found so far.
 * branches specifies the branch conditions on the path. */
void SEngine::processBB(llvm::BasicBlock* b, uint _from, uint depth, z3::expr preCst)
{
    if (b->getName() == "Abort" || b->getName() == "Exit")    return;
    llvm::errs() << "---------------------------------------------------------------------BB#" << b->getName() << " ---------------------------------------------------------------------------------------------\n";
    if (false)
    {
        for (unsigned i = 0; i < m_numInstructions; ++i)
        {
            KInstruction ki = *m_instructions[i];
            // errs() << ki.inst->getOpcodeName() << " =====   Args# " <<
            // (ki.inst)->getNumOperands() << "\n";
            errs() << "%" << ki.inst << " = " << ki.inst->getOpcodeName() << "{"
                   << (ki.inst)->getNumOperands() << "}  ";
            // errs() << "%" << registerMap[ki.inst] << " = " << ki.inst->getOpcodeName() << "{" <<
            // (ki.inst)->getNumOperands() << "}  ";
            for (unsigned j = 0; j < ki.inst->getNumOperands(); j++)
            {
                // // errs() << "[" << j << "]:" << ki.operands[j] <<" | ";
                if (llvm::Instruction* inst = dyn_cast<llvm::Instruction>(ki.inst->getOperand(j)))
                {
                    errs() << "%" << inst->getOpcodeName() << "  ";
                }
                else
                {
                    errs() << "%" << ki.operands[j] << "  ";
                }
                // errs() << "%" << ki.inst->getOperand(j)->getName() << "  ";
            }
            errs() << "\n\n";
        }
    }

    if (reachedMaxDepth(depth) || reachedMaxLoopDepth(b))
        return;

    Node* nodePtr = new Node(b);
    // Node* np = nullptr;
    // for (auto _p: m_Nodes)
    //     if (_p->uid == _from) {
    //         np = _p;
    //         break;
    //     }
    // if (np != nullptr)
    //     for (auto _cst : np->constraints)
    //         nodePtr->constraints.push_back(_cst);
            
    nodePtr->constraints.push_back(preCst);

    symExec(*nodePtr, b);
    // llvm::errs() << "end symzExec\n";
    m_Nodes.push_back(nodePtr);

    m_Edges.push_back(Edge(_from, nodePtr->uid));

    auto terminator = b->getTerminator();

    // terminate
    if (isa<ReturnInst>(terminator) || isa<UnreachableInst>(terminator) ||
        (isa<CallInst>(terminator) && terminator->getOperand(1)->getName() == "ethereum.finish"))
    {
        return;
    }

    // (conditional) jump #2
    if (auto branch = dyn_cast<BranchInst>(terminator))
    {
        errs() << branch->getOpcodeName() << "-> " << branch->getOperand(0)->getName() << "\n";
        if (branch->isConditional())
        {
            z3::expr cond = nodePtr->constraints.back();
            nodePtr->constraints.pop_back();
            // errs() << "condition: " << cond.to_string() << "\n";



            z3::optimize optt(ExprCtx);
            for (auto _constraint : nodePtr->constraints)
                optt.add(_constraint);
            optt.add(cond == true);

            if (true || z3::sat == optt.check())
            {
                errs() << "Jump to Branch::ifEqual #" << branch->getSuccessor(0)->getName() << "\n";
                // JUMP to #ifEqual
                processBB(
                    branch->getSuccessor(0), nodePtr->uid, depth + 1, cond == true);
            }
            // else
            // {
            //     for (auto _constraint : nodePtr->constraints)
            //         errs() << _constraint.to_string() << "\n";
            //     errs() << cond.to_string() << "\n";
            //     errs() << "Do not Jump to Branch::ifEqual #" << branch->getSuccessor(0)->getName()
            //            << "\n";
            // }


            z3::optimize opte(ExprCtx);
            for (auto _constraint : nodePtr->constraints)
                opte.add(_constraint);
            opte.add(cond == false);

            if (true || z3::sat == opte.check())
            {
                errs() << "Jump to Branch::ifUnEqual #" << branch->getSuccessor(1)->getName()
                       << "\n";
                // JUMP to #ifUnEqual
                processBB(
                    branch->getSuccessor(1), nodePtr->uid, depth + 1, cond == false);
            }
            // else
            // {
            //     errs() << "Do not Jump to Branch::ifUnEqual #" << branch->getSuccessor(1)->getName()
            //            << "\n";
            // }
            return;
        }
        else
        {
            processBB(branch->getSuccessor(0), nodePtr->uid, depth + 1, ExprCtx.bool_val(true));
            return;
        }
    }
    else if (auto si = dyn_cast<SwitchInst>(terminator))
    {
        z3::expr cond = nodePtr->constraints.back().simplify();
        nodePtr->constraints.pop_back();
        return;
        errs() << "switch condition: " << cond.to_string() << "\n";
        if (cond.is_numeral())
        {
            unsigned index = cond.as_uint64();
            processBB(si->getSuccessor(index), nodePtr->uid, depth + 1, ExprCtx.bool_val(true));
            return;
        }
        else
        {
            // Handle possible different branch targets
            // We have the following assumptions:
            // - each case value is mutual exclusive to all other values
            // - order of case branches is based on the order of the expressions of
            //   the case values, still default is handled last

            // Iterate through all non-default cases and order them by expressions
            std::map<int64_t, llvm::BasicBlock *> expressionOrder;
            for (auto casei : si->cases()) {
                llvm::ConstantInt* value = casei.getCaseValue();
                llvm::BasicBlock *caseSuccessor = casei.getCaseSuccessor();
                expressionOrder.insert(std::make_pair(value->getSExtValue(), caseSuccessor));
                errs() << "value->" << value->getSExtValue() << " " << caseSuccessor << "\n";
            }
            // Track default the branch value
            int64_t defaultValue = 1;

            // iterate through all non-default cases but in order of the expressions
            z3::expr defaultCst = ExprCtx.bool_val(true);
            for (std::map<int64_t, llvm::BasicBlock *>::iterator it = expressionOrder.begin(),
            itE = expressionOrder.end(); it != itE; ++it)
            {
                z3::expr _caseCst = cond == ExprCtx.bv_val(it->first, 256);
                processBB(it->second, nodePtr->uid, depth + 1, _caseCst);
                defaultCst = defaultCst && (cond != ExprCtx.bv_val(it->first, 256));
            }
            processBB(si->getDefaultDest(), nodePtr->uid, depth + 1, defaultCst.simplify());
            return;
        }
    }
}

void SEngine::bindLocal(KInstruction* target, ExecutionState& _state, z3::expr value)
{
    assert(_state.locals.size() > target->dest && "register index must be less than locals size");
    errs() << "[SE:bindLocal] dst= #locals[" << target->dest << "]\n";
    // z3::context ExprCtx3;
    // z3::expr reg = ExprCtx3.bv_const("dodo", 256);
    _state.locals[target->dest] = value;

    // _state.locals.push_back(value);
    errs() << "[SE] binded.\n";
}


Node* SEngine::symExec(Node& node, BasicBlock* bb)
{
    // allocate ExecutionState for each opcode
    z3::expr _x = ExprCtx.bv_val(0, 256);
    ExecutionState* statePtr =
        new ExecutionState(m_numRegisters, _x);  // inst := expr1 op expr2... => locals[i] = xxxx
    ExecutionState& state = *statePtr;
    // llvm::errs() << "??init state3\n" << state.regsNum << "\n";

    // ExecutionState state = ExecutionState();
    for (unsigned idx = 0; idx < bb->size(); ++idx)
    {
        if (false)
        {
            for (unsigned i = 0; i < m_numInstructions; ++i)
            {
                KInstruction ki = *m_instructions[i];
                // errs() << ki.inst->getOpcodeName() << " =====   Args# " <<
                // (ki.inst)->getNumOperands() << "\n";
                errs() << "%" << ki.inst << " = " << ki.inst->getOpcodeName() << "{"
                       << (ki.inst)->getNumOperands() << "}  ";
                // errs() << "%" << registerMap[ki.inst] << " = " << ki.inst->getOpcodeName() << "{"
                // << (ki.inst)->getNumOperands() << "}  ";
                for (unsigned j = 0; j < ki.inst->getNumOperands(); j++)
                {
                    // // errs() << "[" << j << "]:" << ki.operands[j] <<" | ";
                    if (Instruction* inst = dyn_cast<Instruction>(ki.inst->getOperand(j)))
                    {
                        errs() << "%" << inst->getOpcodeName() << "  ";
                    }
                    else
                    {
                        errs() << "%" << ki.operands[j] << "  ";
                    }
                }
                errs() << "\n\n";
            }
        }

        KInstruction* ki = m_instructions[m_basicBlockEntry[bb] + idx];
        Instruction* i = ki->inst;
        errs() << "\n[symExec] @" << i->getOpcodeName() << " " << idx << "\n";


        switch (i->getOpcode())
        {
        // Control flow
        case Instruction::Ret: {
            ReturnInst* ri = cast<ReturnInst>(i);
            bool isVoidReturn = (ri->getNumOperands() == 0);
            // z3::expr result = ConstantExpr::alloc(0, Expr::Bool);
            // bindLocal(kcaller, state, result);
            break;
        }
        case Instruction::Br: {
            BranchInst* bi = cast<BranchInst>(i);
            if (bi->isConditional())
            {
                assert(bi->getCondition() == bi->getOperand(0) && "Wrong operand index!");
                z3::expr cond = eval(ki, 0, state);
                node.constraints.push_back(cond);
                errs() << "cond: " << cond.simplify().to_string() << "\n";
            }
            break;
        }
        case Instruction::IndirectBr: {
            // implements indirect branch to a label within the current function
            const auto bi = cast<IndirectBrInst>(i);
            break;
        }
        case Instruction::Switch: {
            // TODO
            SwitchInst* si = cast<SwitchInst>(i);
            z3::expr cond = eval(ki, 0, state);
            errs() << "SEVM: switch " << cond.to_string() << " \n";
            node.constraints.push_back(cond);
            break;
        }
        case Instruction::Unreachable:
            terminateStateDebug(state, "reached \"unreachable\" instruction");
            break;

        case Instruction::Invoke:
        case Instruction::Call: {
            // Ignore debug intrinsic calls
            if (isa<DbgInfoIntrinsic>(i))
                break;
            
            // errs() << (i->getOpcodeName() == "call"? "yes":"fales") << "\n";
            // exit(-1);

            const CallBase& cs = cast<CallBase>(*i);
            Value* fp = cs.getCalledOperand();
            unsigned numArgs = cs.arg_size();
            // Function* f = getTargetFunction(fp, state);

            // for (signed _i = 0; _i < i->getNumOperands(); ++_i)
            //     errs() << "[" << _i << "] = " << i->getOperand(_i)->getName() << "\n";
            if (isa<InlineAsm>(fp))
            {
                terminateStateDebug(state, "inline assembly is unsupported");
                break;
            }
            // evaluate arguments
            std::vector<z3::expr> arguments;
            arguments.reserve(numArgs);

            for (unsigned j = 0; j < numArgs; ++j)
                arguments.push_back(eval(ki, j + 1, state));
            // TODO EEI
            errs() << "argumens.size() = " << arguments.size() << "\n;";

            Function* F = dyn_cast<Function>(fp);
            errs() << "func@ " << F->getName() << "\n";
            std::string fName = F->getName().str();

            if (fName == "ethereum.call")
            {
                // bindLocal(ki, state, ExprCtx.bv_const("CALL", ));
                errs() << "[TODO] wirting somthing in the memory\n";
            }
            else if (fName == "ethereum.callCode")
            {
                errs() << "[TODO] wirting somthing in the memory\n";
            }
            else if (fName == "ethereum.callDataCopy")
            {
                z3::expr presult = arguments[0];
                z3::expr data = arguments[1];
                z3::expr offset = arguments[2];
                errs() << presult.to_string() << ";;" << data.to_string() << ";;"
                       << offset.to_string() << "\n";
                assert(offset.is_numeral() && offset.as_uint64() == 32 && "calldatacopy must read 32B / 256bits each time.");
                z3::expr val = m_calldata[data]; //z3::select(m_calldata, data).simplify();
                for (uint _i = 1; _i < offset.as_uint64(); ++_i)
                {
                    val = z3::concat(val, z3::select(m_calldata, data + _i));
                }
                val = val.simplify();

                errs() << "val = " << val.to_string() << "\n";
                errs() << "presult = " << presult.to_string() << "\n";
                memStore(presult, val);
                errs() << "read::" << memLoad(presult, 256).to_string();
            }
            else if (fName == "ethereum.callStatic")
            {
                errs() << "[TODO] wirting somthing in the memory\n";
            }
            else if (fName == "ethereum.callDelegate")
            {
                errs() << "[TODO] wirting somthing in the memory\n";
            }
            else if (fName == "ethereum.create")
            {
                errs() << "[TODO] wirting somthing in the memory\n";
            }
            else if (fName == "ethereum.finish")
            {
                errs() << "[TODO] finish\n";
            }
            else if (fName == "ethereum.getCallDataSize")
            {
                bindLocal(ki, state, m_calldatasize);
            }
            else if (fName == "ethereum.getCallValue")
            {
            }
            else if (fName == "ethereum.getCaller")
            {
            }
            else if (fName == "ethereum.codeCopy")
            {
            }
            else if (fName == "ethereum.externalCodeCopy")
            {
            }
            else if (fName == "ethereum.getGasLeft")
            {
                bindLocal(ki, state, ExprCtx.bv_val(1000000000000000000, 64));  // 1 ether
            }
            else if (fName == "ethereum.log")
            { /*do nothing*/
            }
            else if (fName == "ethereum.log0")
            { /*do nothing*/
            }
            else if (fName == "ethereum.log1")
            { /*do nothing*/
            }
            else if (fName == "ethereum.log2")
            { /*do nothing*/
            }
            else if (fName == "ethereum.log3")
            { /*do nothing*/
            }
            else if (fName == "ethereum.log4")
            { /*do nothing*/
            }
            else if (fName == "ethereum.returnDataSize")
            {
            }
            else if (fName == "ethereum.returnDataCopy")
            {
            }
            else if (fName == "ethereum.revert")
            {
            }
            else if (fName == "ethereum.selfDestruct")
            { /*do nothing*/
                errs() << "SelfDestruct Ignored\n";
                // exit(-1);
            }
            else if (fName == "ethereum.storageLoad")
            {
                z3::expr sidx = memLoad(arguments[0], 256);
                z3::expr sval = z3::select(m_storage, sidx);
                memStore(sidx, sval);
            }
            else if (fName == "ethereum.storageStore")
            {
                z3::expr sidx = memLoad(arguments[0], 256);
                z3::expr val = memLoad(arguments[1], 256);
                m_storage = z3::store(m_storage, sidx, val);
            }
            else if (fName == "ethereum.getTxGasPrice")
            {
                memStore(arguments[0], ExprCtx.bv_val(10, 128));
            }
            else if (fName == "ethereum.getTxOrigin")
            {
                memStore(arguments[0], ExprCtx.bv_const("origin", 256));
            }
            else if (fName == "ethereum.getBlockCoinbase")
            {
                memStore(arguments[0], ExprCtx.bv_const("coinbase", 256));
            }
            else if (fName == "ethereum.getBlockDifficulty")
            {
                memStore(arguments[0], ExprCtx.bv_const("difficult", 256));
            }
            else if (fName == "ethereum.getBlockGasLimit")
            {
                memStore(arguments[0], ExprCtx.bv_const("gaslimit", 256));
            }
            else if (fName == "ethereum.getBlockNumber")
            {
                memStore(arguments[0], ExprCtx.bv_const("blocknumber", 256));
            }
            else if (fName == "ethereum.getBlockTimestamp")
            {
                memStore(arguments[0], ExprCtx.bv_const("timestamp", 256));
            }
            else if (fName == "ethereum.getBlockHash")
            {
                memStore(arguments[0], ExprCtx.bv_const("blockhash", 256));
            }
            else if (fName == "ethereum.getExternalBalance")
            {
                memStore(arguments[0], ExprCtx.bv_const("balance", 256));  // TODO
            }
            else if (fName == "ethereum.getAddress")
            {
                memStore(arguments[0], ExprCtx.bv_const("address", 256));
            }
            else if (fName == "ethereum.getCodeSize")
            {
                memStore(arguments[0], ExprCtx.bv_const("codeSize", 256));
            }
            else if (fName == "ethereum.getExternalCodeSize")
            {
                memStore(arguments[1], ExprCtx.bv_const("codesize", 256));
            }
            else if (fName == "ethereum.getReturnDataSize")
            {
                memStore(arguments[0], ExprCtx.bv_const("returnsize", 256));
            }
            else if (fName == "solidity.keccak256")
            {
            }

            else if (fName == "evm.exp")
            {

            }
            else if (fName == "solidity.bswapi256")
            {
                bindLocal(ki, state, arguments[0]);
                // errs() << arguments[0].to_string() << "\n";
                // exit(-1);
            }


            // if (F->getReturnType()->isSized())
            // {
            //     // if F can return an result.
            //     z3::expr retValue = ExprCtx.bv_const(fName.c_str(),
            //     getWidthForLLVMType(F->getReturnType())); bindLocal(ki, state, retValue);
            // }
            // exit(-1);
            break;
        }

            // case Instruction::PHI: {
            //     z3::expr result = eval(ki, state.incomingBBIndex, state);
            //     bindLocal(ki, state, result);
            //     break;
            // }

            // Special instructions
        case Instruction::Select: {
            // NOTE: It is not required that operands 1 and 2 be of scalar type.
            z3::expr cond = eval(ki, 0, state);
            z3::expr tExpr = eval(ki, 1, state);
            z3::expr fExpr = eval(ki, 2, state);

            // errs() << "Type: cond@ "  << (cond.is_bv() ? "yes\n" : "no\n") << "  " <<
            // cond.to_string() << "\n"; errs() << "Type: texpr@ " << (tExpr.is_bv() ? "yes\n" :
            // "no\n") << tExpr.to_string() << "\n"; errs() << "Type: fexpr@ " << (fExpr.is_bv() ?
            // "yes\n" : "no\n") << fExpr.to_string() << "\n";
            z3::expr result = z3::ite(cond == ExprCtx.bv_val(1, 1), tExpr, fExpr);
            // errs() << "Type: result@ " << (result.is_bv() ? "yes\n" : "no\n") << "  " <<
            // result.to_string() << "\n";
            bindLocal(ki, state, result);
            break;
        }

            // case Instruction::VAArg:
            //     terminateStateDebug(state, "unexpected VAArg instruction");
            //     break;

            // Arithmetic / logical

        case Instruction::Add: {
            z3::expr left = eval(ki, 0, state);
            z3::expr right = eval(ki, 1, state);
            bindLocal(ki, state, left + right);
            errs() << "left = "<< left.to_string() << "\n";
            errs() << "right = " << right.to_string() << "\n";
            break;
        }

        case Instruction::Sub: {
            z3::expr left = eval(ki, 0, state);
            z3::expr right = eval(ki, 1, state);
            bindLocal(ki, state, left - right);
            break;
        }

        case Instruction::Mul: {
            z3::expr left = eval(ki, 0, state);
            z3::expr right = eval(ki, 1, state);
            bindLocal(ki, state, left * right);
            break;
        }

        case Instruction::UDiv: {
            z3::expr left = eval(ki, 0, state);
            z3::expr right = eval(ki, 1, state);
            z3::expr result = z3::udiv(left, right);
            bindLocal(ki, state, result);
            break;
        }

        case Instruction::SDiv: {
            z3::expr left = eval(ki, 0, state);
            z3::expr right = eval(ki, 1, state);
            bindLocal(ki, state, left / right);
            break;
        }

        case Instruction::URem: {
            z3::expr left = eval(ki, 0, state);
            z3::expr right = eval(ki, 1, state);
            z3::expr result = z3::urem(left, right);
            bindLocal(ki, state, result);
            break;
        }

        case Instruction::SRem: {
            z3::expr left = eval(ki, 0, state);
            z3::expr right = eval(ki, 1, state);
            z3::expr result = z3::srem(left, right);
            bindLocal(ki, state, result);
            break;
        }

        case Instruction::And: {
            z3::expr left = eval(ki, 0, state);
            z3::expr right = eval(ki, 1, state);
            z3::expr result = left & right;
            bindLocal(ki, state, result);
            break;
        }

        case Instruction::Or: {
            z3::expr left = eval(ki, 0, state);
            z3::expr right = eval(ki, 1, state);
            z3::expr result = left | right;
            bindLocal(ki, state, result);
            break;
        }

        case Instruction::Xor: {
            z3::expr left = eval(ki, 0, state);
            z3::expr right = eval(ki, 1, state);
            z3::expr result = left ^ right;
            bindLocal(ki, state, result);
            break;
        }

        case Instruction::Shl: {
            z3::expr left = eval(ki, 0, state);
            z3::expr right = eval(ki, 1, state);
            z3::expr result = z3::shl(left, right);
            bindLocal(ki, state, result);
            break;
        }

        case Instruction::LShr: {
            z3::expr left = eval(ki, 0, state);
            z3::expr right = eval(ki, 1, state);
            z3::expr result = z3::lshr(left, right);
            bindLocal(ki, state, result);
            break;
        }

        case Instruction::AShr: {
            z3::expr left = eval(ki, 0, state);
            z3::expr right = eval(ki, 1, state);
            z3::expr result = z3::ashr(left, right);
            bindLocal(ki, state, result);
            break;
        }

        // Compare
        case Instruction::ICmp: {
            CmpInst* ci = cast<CmpInst>(i);
            ICmpInst* ii = cast<ICmpInst>(ci);

            switch (ii->getPredicate())
            {
            case ICmpInst::ICMP_EQ: {
                z3::expr left = eval(ki, 0, state);
                z3::expr right = eval(ki, 1, state);
                z3::expr result =
                    z3::ite((left == right), ExprCtx.bv_val(1, 1),  ExprCtx.bv_val(0, 1));
                
                errs() << "left@ " << left.to_string() << "\n";
                errs() << "right@ " << right.to_string() << "\n";

                errs() << "result@ " << result.to_string() << "\n";
                bindLocal(ki, state, result);
                break;
            }

            case ICmpInst::ICMP_NE: {
                z3::expr left = eval(ki, 0, state);
                z3::expr right = eval(ki, 1, state);
                z3::expr result =
                    z3::ite((left != right), ExprCtx.bv_val(1, 1), ExprCtx.bv_val(0, 1));
                bindLocal(ki, state, result);
                break;
            }

            case ICmpInst::ICMP_UGT: {
                z3::expr left = eval(ki, 0, state);
                z3::expr right = eval(ki, 1, state);
                z3::expr result =
                    z3::ite(z3::ugt(left, right), ExprCtx.bv_val(1, 1), ExprCtx.bv_val(0, 1));
                bindLocal(ki, state, result);
                break;
            }

            case ICmpInst::ICMP_UGE: {
                z3::expr left = eval(ki, 0, state);
                z3::expr right = eval(ki, 1, state);
                z3::expr result =
                    z3::ite(z3::uge(left, right), ExprCtx.bv_val(1, 1), ExprCtx.bv_val(0, 1));
                bindLocal(ki, state, result);
                break;
            }

            case ICmpInst::ICMP_ULT: {
                z3::expr left = eval(ki, 0, state);
                z3::expr right = eval(ki, 1, state);
                z3::expr result =
                    z3::ite(z3::ult(left, right), ExprCtx.bv_val(1, 1), ExprCtx.bv_val(0, 1));
                bindLocal(ki, state, result);
                break;
            }

            case ICmpInst::ICMP_ULE: {
                z3::expr left = eval(ki, 0, state);
                z3::expr right = eval(ki, 1, state);
                z3::expr result =
                    z3::ite(z3::ule(left, right), ExprCtx.bv_val(1, 1), ExprCtx.bv_val(0, 1));
                bindLocal(ki, state, result);
                break;
            }

            case ICmpInst::ICMP_SGT: {
                z3::expr left = eval(ki, 0, state);
                z3::expr right = eval(ki, 1, state);
                z3::expr result =
                    z3::ite(z3::sgt(left, right), ExprCtx.bv_val(1, 1), ExprCtx.bv_val(0, 1));
                bindLocal(ki, state, result);
                break;
            }

            case ICmpInst::ICMP_SGE: {
                z3::expr left = eval(ki, 0, state);
                z3::expr right = eval(ki, 1, state);
                z3::expr result =
                    z3::ite(z3::sge(left, right), ExprCtx.bv_val(1, 1), ExprCtx.bv_val(0, 1));
                bindLocal(ki, state, result);
                break;
            }

            case ICmpInst::ICMP_SLT: {
                z3::expr left = eval(ki, 0, state);
                z3::expr right = eval(ki, 1, state);
                z3::expr result =
                    z3::ite(z3::slt(left, right), ExprCtx.bv_val(1, 1), ExprCtx.bv_val(0, 1));
                bindLocal(ki, state, result);
                break;
            }

            case ICmpInst::ICMP_SLE: {
                z3::expr left = eval(ki, 0, state);
                z3::expr right = eval(ki, 1, state);
                z3::expr result =
                    z3::ite(z3::sle(left, right), ExprCtx.bv_val(1, 1), ExprCtx.bv_val(0, 1));
                bindLocal(ki, state, result);
                break;
            }

            default:
                terminateStateDebug(state, "invalid ICmp predicate");
            }
            break;
        }

        // Memory instructions...
        case Instruction::Alloca: {
            AllocaInst* ai = cast<AllocaInst>(i);
            unsigned elementBytesSize = m_targetData->getTypeStoreSize(ai->getAllocatedType());
            // z3::expr size = Expr::createPointer(elementSize);
            // if (ai->isArrayAllocation())
            // {
            //     z3::expr count = eval(ki, 0, state);
            //     count = Expr::createZExtToPointerWidth(count);
            //     size = size * count;
            // }
            // TODO
            errs() << "Dest= " << ki->dest << " ; Ptr Size= " << elementBytesSize << "B\n";
            // z3::expr count = eval(ki, 0, state);

            std::stringstream _name;
            _name << "%" << static_cast<void*>(i);
            errs() << "[SE] " << _name.str().c_str() << " = allocate " << 8 * elementBytesSize
                   << "bits\n";
            z3::expr reg = ExprCtx.bv_const(_name.str().c_str(), 8 * elementBytesSize);
            bindLocal(ki, state, reg);  // allocate a register
            break;
        }

        case Instruction::Load: {
            if (i->getOperand(0)->getName() == "target") 
            {
                bindLocal(ki, state, m_targetPtr);
            }
            else 
            {
                LoadInst* li = cast<LoadInst>(i);
                unsigned elementbits = getWidthForLLVMType(li->getType());
                z3::expr base = eval(ki, 0, state);
                bindLocal(ki, state, memLoad(base, elementbits));
                errs() << "base =" << base.to_string() << "\n";
                errs() << "get :" << memLoad(base, elementbits).to_string() << "\n";
            }
            // exit(-1);
            break;
        }
        case Instruction::Store: {
            z3::expr value = eval(ki, 0, state);
            if (i->getOperand(1)->getName() == "target")
            {
                m_targetPtr = value;
            }
            else
            {
                z3::expr base = eval(ki, 1, state);
                memStore(base, value);
            }
            break;
        }

        case Instruction::GetElementPtr: {
            KGEPInstruction* kgepi = static_cast<KGEPInstruction*>(ki);
            z3::expr base = eval(ki, 0, state);
            for (std::vector<std::pair<unsigned, uint64_t> >::iterator it = kgepi->indices.begin(),
                                                                       ie = kgepi->indices.end();
                 it != ie; ++it)
            {
                uint64_t elementSize = it->second;
                z3::expr index = eval(ki, it->first, state);
                // base =  base + MulExpr::create(Expr::createSExtToPointerWidth(index),
                // Expr::createPointer(elementSize))); // size*elementSize
            }
            if (kgepi->offset)
                base = base + kgepi->offset;
            // base = AddExpr::create(base, Expr::createPointer(kgepi->offset));
            bindLocal(ki, state, base);
            break;
        }

        // Conversion
        case Instruction::Trunc: {
            // TODO
            CastInst* ci = cast<CastInst>(i);
            z3::expr result = resizeVec(eval(ki, 0, state), getWidthForLLVMType(ci->getType()));
            bindLocal(ki, state, result);
            break;
        }
        case Instruction::ZExt: {
            CastInst* ci = cast<CastInst>(i);
            auto operand = eval(ki, 0, state);
            errs() << "original one@ " << operand.to_string() << "\n";
            z3::expr result = resizeVec(operand, getWidthForLLVMType(ci->getType()), /*isSigned=*/false);
            
            errs() << "result@ " << result.to_string() << "\n";
            bindLocal(ki, state, result);
            break;
        }
        case Instruction::SExt: {
            CastInst* ci = cast<CastInst>(i);
            z3::expr result = resizeVec(
                eval(ki, 0, state), getWidthForLLVMType(ci->getType()), /*isSigned=*/true);
            bindLocal(ki, state, result);
            break;
        }

        case Instruction::IntToPtr: {
            CastInst* ci = cast<CastInst>(i);
            uint pType = getWidthForLLVMType(ci->getType());
            z3::expr arg = eval(ki, 0, state);
            errs() << "to pType::" << pType << "bits\n";
            bindLocal(ki, state, resizeVec(arg, pType, false));
            break;
        }
        case Instruction::PtrToInt: {
            CastInst* ci = cast<CastInst>(i);
            uint iType = getWidthForLLVMType(ci->getType());
            errs() << "to iType::" << iType << "bits\n";
            z3::expr arg = eval(ki, 0, state);
            bindLocal(ki, state, resizeVec(arg, iType, false));
            break;
        }

        case Instruction::BitCast: {
            z3::expr result = eval(ki, 0, state);
            bindLocal(ki, state, result);
            // errs() << "bitcast ::" << result.to_string()<<"\n";
            break;
        }

        case Instruction::InsertValue: {
            KGEPInstruction* kgepi = static_cast<KGEPInstruction*>(ki);

            z3::expr agg = eval(ki, 0, state);
            z3::expr val = eval(ki, 1, state);

            // z3::expr l = NULL, r = NULL;
            // unsigned lOffset = kgepi->offset * 8, rOffset = kgepi->offset * 8 + val->getWidth();

            // if (lOffset > 0)
            //     l = ExtractExpr::create(agg, 0, lOffset);
            // if (rOffset < agg->getWidth())
            //     r = ExtractExpr::create(agg, rOffset, agg->getWidth() - rOffset);

            // z3::expr result;
            // if (l && r)
            //     result = ConcatExpr::create(r, ConcatExpr::create(val, l));
            // else if (l)
            //     result = ConcatExpr::create(val, l);
            // else if (r)
            //     result = ConcatExpr::create(r, val);
            // else
            //     result = val;

            // bindLocal(ki, state, result);
            break;
        }
        case Instruction::ExtractValue: {
            KGEPInstruction* kgepi = static_cast<KGEPInstruction*>(ki);

            z3::expr agg = eval(ki, 0, state);

            // z3::expr result =
            //     ExtractExpr::create(agg, kgepi->offset * 8, getWidthForLLVMType(i->getType()));

            // bindLocal(ki, state, result);
            break;
        }
        case Instruction::Fence: {
            // Ignore for now
            break;
        }
        case Instruction::InsertElement: {
            InsertElementInst* iei = cast<InsertElementInst>(i);
            z3::expr vec = eval(ki, 0, state);
            z3::expr newElt = eval(ki, 1, state);
            z3::expr idx = eval(ki, 2, state);

            //             ConstantExpr* cIdx = dyn_cast<ConstantExpr>(idx);
            //             if (cIdx == NULL)
            //             {
            //                 terminateStateOnError(
            //                     state, "InsertElement, support for symbolic index not
            //                     implemented", Unhandled);
            //                 return;
            //             }
            //             uint64_t iIdx = cIdx->getZExtValue();
            // #if LLVM_VERSION_MAJOR >= 11
            //             const auto* vt = cast<FixedVectorType>(iei->getType());
            // #else
            //             const VectorType* vt = iei->getType();
            // #endif
            //             unsigned EltBits = getWidthForLLVMType(vt->getElementType());

            //             if (iIdx >= vt->getNumElements())
            //             {
            //                 // Out of bounds write
            //                 terminateStateOnError(
            //                     state, "Out of bounds write when inserting element",
            //                     BadVectorAccess);
            //                 return;
            //             }

            //             const unsigned elementCount = vt->getNumElements();
            //             SmallVector<ref<Expr>, 8> elems;
            //             elems.reserve(elementCount);
            //             for (unsigned i = elementCount; i != 0; --i)
            //             {
            //                 auto of = i - 1;
            //                 unsigned bitOffset = EltBits * of;
            //                 elems.push_back(of == iIdx ? newElt : ExtractExpr::create(vec,
            //                 bitOffset, EltBits));
            //             }

            //             assert(Context::get().isLittleEndian() && "FIXME:Broken for big endian");
            //             z3::expr Result = ConcatExpr::createN(elementCount, elems.data());
            // bindLocal(ki, state, Result);
            break;
        }
        case Instruction::ExtractElement: {
            ExtractElementInst* eei = cast<ExtractElementInst>(i);
            z3::expr vec = eval(ki, 0, state);
            z3::expr idx = eval(ki, 1, state);
            /*
            ConstantExpr* cIdx = dyn_cast<ConstantExpr>(idx);
            if (cIdx == NULL)
            {
                terminateStateOnError(
                    state, "ExtractElement, support for symbolic index not implemented", Unhandled);
                return;
            }
            uint64_t iIdx = cIdx->getZExtValue();
#if LLVM_VERSION_MAJOR >= 11
            const auto* vt = cast<FixedVectorType>(eei->getVectorOperandType());
#else
            const VectorType* vt = eei->getVectorOperandType();
#endif
            unsigned EltBits = getWidthForLLVMType(vt->getElementType());

            if (iIdx >= vt->getNumElements())
            {
                // Out of bounds read
                terminateStateOnError(
                    state, "Out of bounds read when extracting element", BadVectorAccess);
                return;
            }

            unsigned bitOffset = EltBits * iIdx;
            // z3::expr Result = ExtractExpr::create(vec, bitOffset, EltBits);
            // bindLocal(ki, state, Result);
            */
            break;
        }
        case Instruction::ShuffleVector:
            // Should never happen due to Scalarizer pass removing ShuffleVector
            // instructions.
            terminateStateDebug(state, "Unexpected ShuffleVector instruction");
            break;

        case Instruction::AtomicRMW:
            terminateStateDebug(state,
                "Unexpected Atomic instruction, should be "
                "lowered by LowerAtomicInstructionPass");
            break;
        case Instruction::AtomicCmpXchg:
            terminateStateDebug(state,
                "Unexpected AtomicCmpXchg instruction, should be "
                "lowered by LowerAtomicInstructionPass");
            break;
        // Other instructions...
        // Unhandled
        default:
            terminateStateDebug(state, "[-] illegal instruction");
            break;
        }

        // record the state of this opcode
        // llvm::errs() << "LocalRegs #" << state.regsNum << "\n";
        ExecutionState* newStatePtr = new ExecutionState(state);
        // llvm::errs() << "2;\n";
        // llvm::errs() << "new LocalRegs #" << (*newStatePtr).regsNum << "\n";
        node.states.push_back(newStatePtr);
        // llvm::errs() << "3;\n";
    }
    // return node;
}

}  // namespace se
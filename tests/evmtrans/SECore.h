#pragma once

#include "CompilerHelper.h"
#include "z3++.h"
#include "llvm/Support/raw_ostream.h"

using namespace z3;


namespace se
{
/// KInstruction - Intermediate instruction representation used
/// during execution.



static uint gbl_next_uid = 0;

// enum class JumpType: uint8_t
// {
// 	UNCONDITIONAL = 0,
//   CONDITIONAL = 1,
//   UNCONDITIONAL = 2,
//   CALL = 3,
//   RETURN = 4
// };

  enum TerminateReason {
    Abort,
    Assert,
    BadVectorAccess,
    Exec,
    External,
    Free,
    Model,
    Overflow,
    Ptr,
    ReadOnly,
    ReportError,
    User,
    Unhandled,
  };

z3::expr resizeVec(z3::expr _e, unsigned _w, bool isSigned=false);

class ExecutionState
{
public:
    std::vector<z3::expr> locals;
    unsigned regsNum;
    int32_t gas;
    ExecutionState(unsigned _regNum, z3::expr _x);
    ExecutionState(const ExecutionState& state);
};


/// KInstruction - Intermediate instruction representation used
/// during execution.
struct KInstruction
{
    llvm::Instruction* inst;

    /// Value numbers for each operand. -1 is an invalid value,
    /// otherwise negative numbers are indices (negated and offset by
    /// 2) into the module constant table and positive numbers are
    /// register indices.
    int* operands;
    /// Destination register index.
    unsigned dest;

public:
    virtual ~KInstruction();
    std::string getSourceLocation() const;
};

struct KGEPInstruction : KInstruction
{
public:
    /// indices - The list of variable sized adjustments to add to the pointer
    /// operand to execute the instruction. The first element is the operand
    /// index into the GetElementPtr instruction, and the second element is the
    /// element size to multiple that index by.
    std::vector<std::pair<unsigned, uint64_t>> indices;

    /// offset - A constant offset to add to the pointer operand to execute the
    /// instruction.
    uint64_t offset;
};

class KConstant
{
public:
    /// Actual LLVM constant this represents.
    llvm::ConstantInt* ct;

    /// The constant ID.
    unsigned id;

    /// First instruction where this constant was encountered, or NULL
    /// if not applicable/unavailable.
    KInstruction* ki;

    KConstant(llvm::ConstantInt*, unsigned, KInstruction*);
};


class Node
{
public:
    // Node(const Node& node){
    //     uid = gbl_next_uid++;
    //     bbPtr = node.bbPtr;

    // }

    uint uid;

    llvm::BasicBlock* bbPtr;
    std::vector<z3::expr> constraints;
    std::vector<ExecutionState*> states;

    Node(llvm::BasicBlock* _bbPtr)
    {
        uid = gbl_next_uid++;
        bbPtr = _bbPtr;
    }

    ~Node(){}
    //     for (unsigned i = 0;i < states.size(); ++i)
    //     {
    //         // llvm::errs() << "killed Node\n";
    //         // delete states[i];
    //     }
    // }

private:
    void print()
    { 
        llvm::errs() << "Node#" << uid << " @BB#" << bbPtr->getName() << "\n";
    }
};

class Edge
{
public:
    // Edge(const Edge& edge);
    Edge(uint _from, uint _to /*, enum JumpType _edge_type*/)
    {
        node_from = _from;
        node_to = _to;
    }

// private:
    uint node_from;
    uint node_to;
    // enum JumpType type;
};


class SEngine
{

private:
    llvm::Module* m_module;
    llvm::DataLayout* m_targetData;

    uint m_maxDepth = 32;
    uint m_maxLoopDepth = 5;



    std::vector<llvm::Value*> m_complexRegs;
    std::vector<z3::expr> m_gSymbols;

    
    const z3::expr& eval(KInstruction* ki, unsigned index, ExecutionState& state) const;
    KConstant* getKConstant(const llvm::ConstantInt* c);
    unsigned getConstantID(llvm::ConstantInt* c, KInstruction* ki);
    unsigned getWidthForLLVMType(llvm::Type* type) const;
    int getOperandNum(llvm::Value* v, std::map<llvm::Instruction*, unsigned>& registerMap, KInstruction* ki);
    void processBB(llvm::BasicBlock* b, uint _from, uint depth, z3::expr preCst);

    Node* symExec(Node& node, llvm::BasicBlock* bb);

    void terminateStateDebug(ExecutionState& state, const llvm::Twine& message, const llvm::Twine& info = "")
    {
        // std::string message = message.str();
        llvm::errs() << "terminateStateDebug\n";
    }

public:
    explicit SEngine(llvm::Module* _module);


    z3::context ExprCtx;
    
    z3::expr m_targetPtr    = ExprCtx.bv_val(0, 256);
    z3::expr m_calldata     = ExprCtx.constant("calldata", ExprCtx.array_sort(ExprCtx.bv_sort(32), ExprCtx.bv_val(0, 8).get_sort())); 
    z3::expr m_calldatasize = ExprCtx.bv_const("CALLDATASIZE", 256);

    z3::expr m_memory       = ExprCtx.constant("mem", ExprCtx.array_sort(ExprCtx.bv_sort(256), ExprCtx.bv_val(0, 8).get_sort()));  // self.memory = z3.K(z3.BitVecSort(256), z3.BitVecVal(0, 8))
    // z3::expr m_stk      = ExprCtx.constant("mstk", ExprCtx.array_sort(ExprCtx.bv_sort(256), ExprCtx.bv_sort(256)));
    z3::expr m_stk          = ExprCtx.constant("mstk", ExprCtx.array_sort(ExprCtx.bv_sort(256), ExprCtx.bv_val(0, 256).get_sort()));

    z3::expr m_storage      = ExprCtx.constant("storage", ExprCtx.array_sort(ExprCtx.bv_sort(256), ExprCtx.bv_sort(256))); 

    void memStore(z3::expr adr, z3::expr val)
    {   
        llvm::errs() << "[SEngine::memStore] storing " << val.get_sort().bv_size() << " bits\n";
        // llvm::errs() << "[SEngine::memStore] mem[" << adr.to_string() << "] <---" << z3::bv2int(val.simplify(), true).to_string() << "\n";

        if (adr.to_string().find("mstk") != std::string::npos)
        {   
            // global variable   
            z3::expr iadr = adr - ExprCtx.bv_const("mstk", 256);// ExprCtx.constant("mstk", ExprCtx.array_sort(ExprCtx.bv_sort(128), ExprCtx.bv_sort(256)));
            iadr = iadr.simplify();
            // llvm::errs() << "[SEngine::memStore] iadr = " << iadr.to_string() << "\n";
            
            m_stk = z3::store(m_stk, iadr, val);
        }
        else
        {
            for (uint i = 0; i < val.get_sort().bv_size(); i+=8)
            {
                z3::expr _shr = val.extract(i+7, i);
                m_memory = z3::store(m_memory, adr+i, _shr);
            }
        }

        llvm::errs() << "end memStore()\n";
    }
    
    z3::expr memLoad(z3::expr adr, unsigned sz)
    {   
        llvm::errs() << "[SEngine::memLoad] loading " << sz << " bits\n";

        if (adr.to_string().find("mstk") != std::string::npos)
        {   
            // global variable   
            z3::expr iadr = adr - ExprCtx.bv_const("mstk", 256);// ExprCtx.constant("mstk", ExprCtx.array_sort(ExprCtx.bv_sort(128), ExprCtx.bv_sort(256)));
            iadr = iadr.simplify();
            // llvm::errs() << "[SEngine::memStore] iadr = " << iadr.to_string() << "\n";
            
            return z3::select(m_stk, iadr).simplify();
        }
        else 
        {
            z3::expr val = z3::select(m_memory, adr).simplify();
            for (uint i = 8; i < sz; i+=8)
            {   
                val = z3::concat(z3::select(m_memory, adr + i).simplify(), val).simplify();
            }
            // llvm::errs() << "[SEngine::memLoad] loading mem[" << adr.to_string() << "]--------> " << val.simplify().to_string() << "\n";
            return val.simplify();
        }

    }

    // z3::sort Int = ExprCtx.bv_sort(256);
    
    // z3::sort memory = ExprCtx.array_sort(ExprCtx.bv_sort(256), ExprCtx.bv_val(0, 8));


    std::vector<Node*> m_Nodes;
    std::vector<Edge> m_Edges;


    void bindLocal(KInstruction* target, ExecutionState& state, z3::expr value);

    unsigned m_numRegisters;
    unsigned m_numInstructions = 0;
    KInstruction** m_instructions;
    std::map<llvm::BasicBlock*, unsigned> m_basicBlockEntry;

    std::vector<z3::expr> m_constants;
    std::map<std::string, z3::expr> m_gZ3Map;
    

    
    bool reachedMaxDepth(uint depth) { return depth >= m_maxDepth; }

    bool reachedMaxLoopDepth(llvm::BasicBlock* b)
    {
        uint loopDepth = 0;
        for (auto _n : m_Nodes)
        {
            if (_n->bbPtr == b)
                ++loopDepth;

            if (loopDepth >= m_maxLoopDepth)
                return true;
        }
        return false;
    }

    // z3::context getZ3Ctx(void) const noexcept { return ExprCtx; }
    void run(llvm::Function* function);
    virtual ~SEngine();




    
};


}  // namespace se

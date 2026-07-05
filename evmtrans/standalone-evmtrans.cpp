#include <stdio.h>
#include <chrono>
#include <fstream>
#include <iostream>

#include "preprocessor/llvm_includes_end.h"
#include "preprocessor/llvm_includes_start.h"
#include <llvm/ADT/StringSwitch.h>
#include <llvm/ADT/Triple.h>
#include <llvm/IR/Module.h>
#include <llvm/MC/SubtargetFeature.h>
#include <llvm/Support/CommandLine.h>
#include <llvm/Support/MemoryBuffer.h>
#include <llvm/Support/TargetRegistry.h>
#include <llvm/Support/TargetSelect.h>
#include <llvm/Support/raw_os_ostream.h>
#include <llvm/Target/TargetMachine.h>
#include <llvm/Target/TargetOptions.h>

#include "CodeGen.h"
#include "Optimizer.h"
#include "WASMCompiler.h"

// using namespace std;

namespace cl = llvm::cl;
cl::opt<std::string> InputFilename(
    cl::Positional, cl::desc("<input bitcode file>"), cl::init("-"), cl::value_desc("filename"));
cl::opt<std::string> OutputFilename(
    "o", cl::desc("Output filename"), cl::value_desc("filename"), cl::init("-"));

cl::opt<bool> g_debug{"debug", cl::desc{"Output Debug Info"}};

cl::opt<bool> g_dump{"dump", cl::desc{"Dump LLVM IR module"}};
// options
cl::opt<bool> g_enableCheckReentrancy{
    "check-reentrancy", cl::desc{"enable the reentrancy optimization PASS"}};
cl::opt<bool> g_enableDetectSuicide{
    "detect-suicide", cl::desc{"enable to remove dangerous suicide"}};
cl::opt<bool> g_enableCheckSend{"check-send", cl::desc{"enable to check unsafe send"}};

cl::opt<bool> g_enableEOAonly{
    "EOAonly", cl::desc{"only allow EOA to interactive with the contract"}};

cl::opt<bool> g_enableRmOrigin{"rm-origin", cl::desc{"replace tx.origin with msg.sender"}};
cl::opt<bool> g_enableUseSafeMath{"use-safeMath",
    cl::desc{"guard arithmetic opcodes with SafeMath, i.e., ADD, SUB, MUL, DIV, or EXP"}};

cl::opt<bool> g_enableUpgrade{"enable-upgrade", cl::desc{"enable create2-based Proxy"}};


cl::opt<bool> g_onlyRt{"onlyRt", cl::desc{"only transalte the Runtime Code"}};
cl::opt<bool> g_runtimeInput{
    "runtime-input", cl::desc{"treat the input as runtime bytecode without deployment code"}};
cl::opt<int> g_vulpc{"vulPC", cl::desc{"the PC of the vulnerable Opcode"}};

cl::opt<int> g_enableKlee{"enableKlee", cl::desc{"build Klee symbilic input"}};



std::unique_ptr<llvm::TargetMachine> createTarget()
{
    llvm::CodeGenOpt::Level OLvl = llvm::CodeGenOpt::Default;
    auto TheTriple = llvm::Triple(llvm::Triple::normalize("wasm32-unknown-unknown"));
    std::string Error;
    const llvm::Target* TheTarget =
        llvm::TargetRegistry::lookupTarget(TheTriple.getTriple(), Error);
    if (!TheTarget)
    {
        std::cerr << Error << "\n";
        return nullptr;
    }
    llvm::TargetOptions Options = dev::eth::trans::InitTargetOptionsFromCodeGenFlags();
    std::string CPUStr = "";
    llvm::SubtargetFeatures Features;
    std::string FeaturesStr = Features.getString();
    std::unique_ptr<llvm::TargetMachine> Target(
        TheTarget->createTargetMachine(TheTriple.getTriple(), CPUStr, FeaturesStr, Options,
            llvm::Reloc::Static, llvm::CodeModel::Small, OLvl));
    assert(Target && "Could not allocate target machine!");

    return Target;
}

void exportIR(std::string outfile, llvm::Module& module)
{
    std::error_code ec;
    llvm::raw_fd_ostream fout(llvm::StringRef(outfile), ec, llvm::sys::fs::F_None);
    if (ec)
    {
        llvm::raw_os_ostream cerr{std::cerr};
        module.print(cerr, nullptr);
    }
    else
    {
        module.print(fout, nullptr);
    }
}

void llvm_native_opt(llvm::Module& _module) {
    std::string LinkFileName = ".link.wasm";
    // const char* Args[] = {"wasm-ld", "--entry", "main", "--gc-sections", "--allow-undefined", "-O0",
    //     OutputFilename.c_str(), "-o", LinkFileName.c_str()};
    // lld::wasm::link(llvm::ArrayRef<const char*>(Args), false, llvm::outs(), llvm::errs());

    // return result;
    return;
}

int main(int argc, char** argv) {
    // PASS options
    dev::eth::trans::Options options{
        enableCheckReentrancy : g_enableCheckReentrancy,
        enableDetectSuicide : g_enableDetectSuicide,
        enableCheckSend : g_enableCheckSend,
        enableEOAonly : g_enableEOAonly,
        enableRmOrigin : g_enableRmOrigin,
        enableSafeMath : g_enableUseSafeMath,
        isRtcode : false,
        enableUpgrade : g_enableUpgrade,
        vulpc     : g_vulpc,
        enableKlee: g_enableKlee
    };
    
    using clock = std::chrono::high_resolution_clock;
    auto startTime = clock::now();
    constexpr auto to_us_str = [](clock::duration d) {
        return std::to_string(std::chrono::duration_cast<std::chrono::microseconds>(d).count());
    };

    cl::ParseCommandLineOptions(argc, argv, "Ethereum EVM to EWASM Binary Translator\n");

    llvm::InitializeAllTargets();
    llvm::InitializeAllTargetMCs();
    llvm::InitializeAllAsmPrinters();
    llvm::InitializeAllAsmParsers();

    std::unique_ptr<llvm::TargetMachine> t_machine = std::move(createTarget());

    //  Read EVM bytecode from input file
    // auto pos = InputFilename.find_last_of('/');
    // std::string filename(InputFilename.substr(pos + 1));
    // pos = filename.find_last_of('.');
    // filename = filename.substr(0, pos);

    // std::cerr << argv[0] << ": " << filename << ": filename !\n";

    auto Binary = llvm::MemoryBuffer::getFile(InputFilename);
    if (!Binary) {
        std::cerr << argv[0] << ": " << InputFilename << ": error: cannot load input file!\n";
        return 1;
    }
    auto Buffer = (*Binary)->getBuffer();
    auto code_start = (uint8_t*)const_cast<char*>(Buffer.data());
    
    uint64_t RETURNPC = 0;
    int64_t splitPoint = 0;
    if (!g_runtimeInput) {
        splitPoint = dev::eth::trans::splitCode(code_start, code_start + Buffer.size(), RETURNPC);
        if (splitPoint == -1) {
            std::cerr << "[-] Fatal Error. Cannot split runtime code.\n";
            return -1;
        }
    }

    if (g_debug) {
        std::cerr << "split Point=" << splitPoint << std::dec;
        std::cerr << "\n; size=" << Buffer.size() << "\n";
        if (g_runtimeInput) {
            std::cerr << "runtime-input mode; size=" << Buffer.size() << "\n";
        } else {
            std::cerr << " ; RETURNPC=" << RETURNPC << "\n";
            std::cerr << "======================= Split out =======================\n";
            std::cerr << "-----Runtime EVM Code:\n";
            for (auto i = splitPoint; i < Buffer.size(); i++)
                std::cerr << std::hex << (int)(*(code_start + i)) << " ";

            std::cerr << "\n-----Deploy EVM Code:\n";
            for (auto i = 0; i < splitPoint; i++)
                std::cerr << std::hex << (int)(*(code_start + i)) << " ";
            std::cerr << "\n=======================================================\n";
        }
        std::cerr << "\n[+] ====================== Compile Runtime Code ====================\n";
    }
    
    /***************************************
        deploy code  = code[:splitPoint]
        runtime code = code[splitPoint:]
    ****************************************/
    std::vector<uint8_t> EntireBytecode = {code_start, code_start + Buffer.size()};
    std::vector<uint8_t> DeployerBytecode = {code_start, code_start + splitPoint};
    std::vector<uint8_t> RuntimerBytecode = {code_start + splitPoint, code_start + Buffer.size()};
  
    llvm::LLVMContext LLVMCtx;
    auto ModulePtr = std::make_unique<llvm::Module>("runtime", LLVMCtx);
    llvm::Module &RMod = *ModulePtr.get();
    dev::eth::trans::WASMCompiler RuntimeCC(RMod, RuntimerBytecode, 0);
    std::cerr << "\n[+]2 \n";
    RuntimeCC.compileMain(RuntimerBytecode, "main");
    dev::eth::trans::prepare(RMod, options);
    if (g_dump) exportIR("./rt.ll", RMod);

    if (g_debug) std::cerr << "\n[+] ====================== Exported Bitcode of the Runtime Code====================\n";
    
    std::unique_ptr<dev::eth::trans::codeGenResult> runtimeCodeGen =
        dev::eth::trans::codeGenModule(*t_machine, RMod);

    if (g_dump) {
        std::ofstream rtfile;
        rtfile.open(
            "./runtime.wasm", std::ios::out | std::ios::binary | std::ios::trunc);
        auto rtcode = runtimeCodeGen->getCode();
        for (size_t i = 0; i < runtimeCodeGen->getCodeSize(); i++)
        {
            rtfile << rtcode[i];
        }
        rtfile << "\0";
        rtfile.close();
    }
    if (g_runtimeInput) {
        if (OutputFilename == "-") OutputFilename = "./runtime.wasm";
        std::ofstream ofp;
        ofp.open(OutputFilename, std::ios::out | std::ios::binary | std::ios::trunc);
        auto code = runtimeCodeGen->getCode();
        for (size_t i = 0; i < runtimeCodeGen->getCodeSize(); i++)
        {
            ofp << code[i];
        }
        ofp << "\0";
        ofp.close();
        std::cerr << "Runtime bytecode size: " << runtimeCodeGen->getCodeSize() << "B \n";
        std::cout << "Translation Complete. Time [us]: " << to_us_str(clock::now() - startTime) << "\n";
        return 0;
    }
    if (g_onlyRt)   return 0;

    if (g_debug) std::cerr << "\n[+] =========== Compile deploy Code ==========\n";

    // ewasm
    auto DModulePtr = std::make_unique<llvm::Module>("deploy", LLVMCtx);
    llvm::Module &DMod = *DModulePtr.get();
    llvm::StringRef RtWasmCode((char*)runtimeCodeGen->getCode(), runtimeCodeGen->getCodeSize());
    dev::eth::trans::WASMCompiler DeployerCC(DMod, EntireBytecode, RETURNPC, RtWasmCode);
    DeployerCC.compileMain(DeployerBytecode, "main");
    std::cerr << "\n[+] code gen\n";

    dev::eth::trans::prepare(DMod, options);

    if (g_dump) exportIR("./res.ll", DMod);
    std::unique_ptr<dev::eth::trans::codeGenResult> deployCodeGen =
      dev::eth::trans::codeGenModule(*t_machine, DMod);
    
    // Output EWasm
    if (OutputFilename == "-") OutputFilename = "./res.wasm";
    std::ofstream ofp;
    ofp.open(OutputFilename, std::ios::out | std::ios::binary | std::ios::trunc);
    auto code = deployCodeGen->getCode();
    for (size_t i = 0; i < deployCodeGen->getCodeSize(); i++)
    {
        ofp << code[i];
    }
    ofp << "\0";
    ofp.close();
    std::cerr << "Bytecode size: " << deployCodeGen->getCodeSize() << "B \n";
    
    std::cout << "Translation Complete. Time [us]: " << to_us_str(clock::now() - startTime) << "\n";

    return 0;
}

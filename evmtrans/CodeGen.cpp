#include "CodeGen.h"

#include <stdio.h>
#include <fstream>
#include <iostream>
#include <vector>

#include "preprocessor/llvm_includes_start.h"
#include <llvm/Analysis/TargetLibraryInfo.h>
#include <llvm/CodeGen/LinkAllAsmWriterComponents.h>
#include <llvm/CodeGen/LinkAllCodegenComponents.h>
#include <llvm/CodeGen/MIRParser/MIRParser.h>
#include <llvm/CodeGen/MachineFunctionPass.h>
#include <llvm/CodeGen/MachineModuleInfo.h>
#include <llvm/CodeGen/TargetPassConfig.h>
#include <llvm/CodeGen/TargetSubtargetInfo.h>
#include <llvm/IR/AutoUpgrade.h>
#include <llvm/IR/DataLayout.h>
#include <llvm/IR/IRPrintingPasses.h>
#include <llvm/IR/LegacyPassManager.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Verifier.h>
#include <llvm/MC/MCSubtargetInfo.h>
#include <llvm/MC/SubtargetFeature.h>
#include <llvm/Pass.h>
#include <llvm/Support/FileSystem.h>
#include <llvm/Support/TargetRegistry.h>
#include <llvm/Support/TargetSelect.h>
#include <llvm/Support/ToolOutputFile.h>
#include <llvm/Support/raw_os_ostream.h>
#include <llvm/Target/TargetMachine.h>
#include <llvm/Target/TargetOptions.h>
#include <llvm/Transforms/IPO.h>
#include <llvm/Transforms/Scalar.h>
#include <llvm/Transforms/Utils.h>
#include <llvm/Transforms/Utils/BasicBlockUtils.h>
#include <llvm/Transforms/Utils/Mem2Reg.h>
// #include <binaryen-c.h>
#include "preprocessor/llvm_includes_start.h"
#include <lld/Common/Driver.h>

namespace dev
{
namespace eth
{
namespace trans
{
llvm::TargetOptions InitTargetOptionsFromCodeGenFlags()
{
    llvm::TargetOptions Options;
    Options.AllowFPOpFusion = llvm::FPOpFusion::Standard;
    Options.UnsafeFPMath = false;
    Options.NoInfsFPMath = false;
    Options.NoNaNsFPMath = false;
    Options.NoSignedZerosFPMath = false;
    Options.NoTrappingFPMath = false;
    Options.FPDenormalMode = llvm::FPDenormal::IEEE;
    Options.HonorSignDependentRoundingFPMathOption = false;
    Options.NoZerosInBSS = false;
    Options.GuaranteedTailCallOpt = false;
    Options.StackAlignmentOverride = 0;
    Options.StackSymbolOrdering = true;
    Options.UseInitArray = true;
    Options.RelaxELFRelocations = false;
    Options.DataSections = false;
    Options.FunctionSections = false;
    Options.UniqueSectionNames = true;
    Options.EmulatedTLS = false;
    Options.ExplicitEmulatedTLS = false;
    Options.ExceptionModel = llvm::ExceptionHandling::None;
    Options.EmitStackSizeSection = false;
    Options.EmitAddrsig = false;

    // Options.MCOptions = InitMCTargetOptionsFromFlags();

    Options.ThreadModel = llvm::ThreadModel::POSIX;
    Options.EABIVersion = llvm::EABI::Default;
    Options.DebuggerTuning = llvm::DebuggerKind::Default;

    return Options;
}

// void removeExports(const std::string &Filename, codeGenResult* result) {

//     BinaryenModuleRef WasmModule;
//     size_t BufferSize = 0;

//     {
//         auto Binary = llvm::MemoryBuffer::getFile(Filename);
//         if (!Binary) {
//             return;
//         }

//         auto Buffer = (*Binary)->getBuffer();
//         BufferSize = Buffer.size();
//         WasmModule = BinaryenModuleRead(const_cast<char*>(Buffer.data()), Buffer.size());
//     }

//     // BinaryenModuleRef WasmModule = BinaryenModuleRead((char*)result->getCode(),
//     result->getCodeSize());

//     std::cerr << "after BinaryenModuleRead\n";
//     BinaryenRemoveExport(WasmModule, "__heap_base");
//     BinaryenRemoveExport(WasmModule, "__data_end");
//     std::cerr << "after BinaryenRemoveExport\n";
//     BinaryenSetOptimizeLevel(0);
//     BinaryenSetShrinkLevel(0);
//     BinaryenModuleOptimize(WasmModule);
//     std::cerr << "after BinaryenModuleOptimize\n";

//     // std::vector<char> OutputBuffer(code_size);
//     // llvm::SmallVector<char, 0> OutputBuffer(result->getCodeSize());
//     std::vector<char> OutputBuffer(BufferSize);
//     auto Size = BinaryenModuleWrite(WasmModule, OutputBuffer.data(), OutputBuffer.size());
//     std::cerr << "after BinaryenModuleWrite\n";
//     result->saveFromVector(OutputBuffer);
//     std::cerr << "after saveFromBuffer\n";
//     BinaryenModulePrint(WasmModule);
//     BinaryenModuleDispose(WasmModule);

// }

std::unique_ptr<codeGenResult> codeGenModule(llvm::TargetMachine& _target, llvm::Module& _module)
{
    std::unique_ptr<codeGenResult> result(new codeGenResult());


    // Open the output file
    std::string OutputFilename = ".code_gen.wasm";
    {
        // std::cerr << "WASM Codegen Opening.\n";

        std::error_code EC;
        llvm::sys::fs::OpenFlags OpenFlags = llvm::sys::fs::F_None;
        auto FDOut = std::make_unique<llvm::ToolOutputFile>(OutputFilename, EC, OpenFlags);
        if (EC)
        {
            std::cerr << "CodeGen Open OutputFile failed: " << EC.message() << '\n';
            return result;
        }
        // std::cerr << "WASM Codegen Opened.\n";
        // std::cerr << "setTargetTriple\n";
        auto TheTriple = llvm::Triple(llvm::Triple::normalize("wasm32-unknown-unknown-wasm"));
        _module.setTargetTriple(TheTriple.str());
        // _module.setTargetTriple("wasm32-unknown-unknown-wasm");
        

        // std::cerr << "setTargetTriple done\n";
        auto PM = llvm::legacy::PassManager{};
        llvm::TargetLibraryInfoImpl TLII(llvm::Triple(_module.getTargetTriple()));
        // auto TLII = std::make_unique<llvm::TargetLibraryInfoImpl>(
                // llvm::Triple(_module.getTargetTriple()));
        // TLII->disableAllFunctions();
        PM.add(new llvm::TargetLibraryInfoWrapperPass(TLII));

        // std::cerr << "setDataLayout\n";
        // _module.setDataLayout(_target.createDataLayout());
        _module.setDataLayout(llvm::DataLayout("e-m:e-p:32:32-i64:64-n32:64-S128"));
        
        llvm::UpgradeDebugInfo(_module);
        // std::cerr << "setDataLayout done\n";


        llvm::raw_os_ostream cerr{std::cerr};
        if (llvm::verifyModule(_module, &cerr))
        {
            std::cerr << "LLVM IR is broken \n";
            // return result;
            exit(-1);
        }
        // std::cerr << "verified \n";

        llvm::raw_pwrite_stream* OS = &FDOut->os();
        llvm::SmallVector<char, 0> Buffer;
        std::unique_ptr<llvm::raw_svector_ostream> BOS;
        BOS = std::make_unique<llvm::raw_svector_ostream>(Buffer);
        OS = BOS.get();


        llvm::LLVMTargetMachine& LLVMTM = static_cast<llvm::LLVMTargetMachine&>(_target);
        llvm::MachineModuleInfoWrapperPass* MMIWP = new llvm::MachineModuleInfoWrapperPass(&LLVMTM);
        
        // llvm::TargetPassConfig &TPC = *LLVMTM.createPassConfig(PM);

        // TPC.setDisableVerify(false);
        // PM.add(&TPC);
        // PM.add(MMI);
        // TPC.printAndVerify("");

        // TPC.setInitialized();
        // PM.add(llvm::createPrintMIRPass(*OS));
        // PM.add(llvm::createFreeMachineFunctionPass());

        _target.addPassesToEmitFile(PM, *OS, nullptr, llvm::CGFT_ObjectFile, false, MMIWP);
        // std::cerr << "addPassesToEmitFile \n";

        PM.run(_module);

        FDOut->os() << Buffer;
        FDOut->keep();
    }
    // std::cerr << "WASM Codegen Success.\n";

    std::string LinkFileName = ".link.wasm";
    const char* Args[] = {"wasm-ld", "--entry", "main", "--gc-sections", "--allow-undefined", "-O0",
        OutputFilename.c_str(), "-o", LinkFileName.c_str()};
    // "--export=__heap_base",
    lld::wasm::link(llvm::ArrayRef<const char*>(Args), false, llvm::outs(), llvm::errs());

    // removeExports(LinkFileName, result.get());

    auto Binary = llvm::MemoryBuffer::getFile(LinkFileName);
    if (!Binary)
    {
        return result;
    }
    auto Buffer = (*Binary)->getBuffer();
    result->saveFromChars(const_cast<char*>(Buffer.data()), Buffer.size());
    // std::cerr << "Bytecode size: " << result->getCodeSize() << "\n";

    return result;
}
// ------------------ End Codegen ------------------


}  // namespace trans
}  // namespace eth
}  // namespace dev
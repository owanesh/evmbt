#include <stdio.h>
#include <iostream>
#include <fstream>
#include <chrono>
#include "z3++.h"
// #include "preprocessor/llvm_includes_start.h"
// #include <llvm/IR/Module.h>
// #include <llvm/ADT/StringSwitch.h>
// #include <llvm/ADT/Triple.h>
// #include <llvm/Support/TargetSelect.h>
// #include <llvm/Support/raw_os_ostream.h>
// #include <llvm/Support/TargetRegistry.h>
// #include <llvm/Support/MemoryBuffer.h>
// #include <llvm/Target/TargetOptions.h>
// #include <llvm/Target/TargetMachine.h>
// #include <llvm/MC/SubtargetFeature.h>
// #include "preprocessor/llvm_includes_end.h"

// #include "Optimizer.h"
// #include "WASMCompiler.h"


// void exportIR(std::string outfile, llvm::Module& module)
// {
//     std::error_code ec;
//     llvm::raw_fd_ostream fout(llvm::StringRef(outfile), ec, llvm::sys::fs::F_None);
//     if (ec) {
//         llvm::raw_os_ostream cerr{std::cerr};
//         module.print(cerr, nullptr);
//     } else {
//         module.print(fout, nullptr);
//     }
// }

int main(int argc, char **argv) {

	/**
	 Demonstration of how Z3 can be used to prove validity of
	De Morgan's Duality Law: {e not(x and y) <-> (not x) or ( not y) }
	**/
	
    // z3::context c;
	// z3::expr arr = c.constant("arr", c.array_sort(c.bv_sort(256), c.bv_sort(8))); 
    
	// z3::expr x = c.bv_const("x", 8);	
	// arr = z3::store(arr, 0, x);
    // z3::expr conjecture = (z3::select(arr, 0) == c.bv_val(12, 8));
    // z3::solver s(c);
    // s.add(conjecture);
    // std::cout << s << "\n";
	// std::cout << s.check() << "\n";

	// z3::model m = s.get_model();
	// // std::cout << "Model::" << m << "\n";
	// for (unsigned i = 0; i < m.size(); i++) {
	// 	z3::func_decl v = m[i];
	// 	// assert(v.arity() == 0); 
	// 	std::cout << v.name() << " = " << m.get_const_interp(v) << "\n";
	// }
	// std::cout << "x + y + 1 = " << m.eval(x + y + 1) << "\n";
	

    // llvm::LLVMContext llvmContext;
    // auto EasmM = dev::eth::trans::WASMCompiler({}, llvmContext).debugMain();
    // dev::eth::trans::prepare(*EasmM, options); // optimize
    // exportIR("/home/toor/evmTrans/blockchain/rt.ll", *EasmM);
    return 0;


}
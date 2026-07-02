#pragma once

#include <iostream>
#include <memory>
#include <string.h>
#include <llvm/ADT/SmallVector.h>
#include <vector>

namespace llvm
{
	class Module;
	class TargetOptions;
	class TargetMachine;
}

namespace dev
{
namespace eth
{
namespace trans
{

class codeGenResult {

	std::vector<uint8_t> code;

public:
	void saveFromBuffer(llvm::SmallVector<char, 0> &buffer) {
		code.clear();
		code.reserve(buffer.size());
		for (auto j : buffer) {
			code.push_back((uint8_t)j);
		}
	}
	void saveFromVector(std::vector<char> &buffer) {
		code.assign(buffer.begin(), buffer.end());
	}
	void saveFromChars(const char* str, size_t len) {
		code.assign(str, str+len);
	}
	size_t getCodeSize() {
		return code.size();
	}
	uint8_t* getCode() {
		return code.data();
	}
	
};

llvm::TargetOptions InitTargetOptionsFromCodeGenFlags();
std::unique_ptr<codeGenResult> codeGenModule(llvm::TargetMachine& _target, llvm::Module& _module);

}
}
}
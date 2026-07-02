#include "Instruction.h"

#include "preprocessor/llvm_includes_start.h"
#include <llvm/ADT/APInt.h>
#include "preprocessor/llvm_includes_end.h"

namespace dev
{
namespace evmtrans
{

llvm::APInt readPushData(code_iterator& _curr, code_iterator _end)
{
	auto pushInst = *_curr;
	assert(Instruction(pushInst) >= Instruction::PUSH1 && Instruction(pushInst) <= Instruction::PUSH32);
	auto numBytes = pushInst - static_cast<size_t>(Instruction::PUSH1) + 1;
	llvm::APInt value(256, 0);
	++_curr;	// Point the data
	for (decltype(numBytes) i = 0; i < numBytes; ++i)
	{
		byte b = (_curr != _end) ? *_curr++ : 0;
		value <<= 8;
		value |= b;
	}
	--_curr;	// Point the last real byte read
	return value;
}

void skipPushData(code_iterator& _curr, code_iterator _end)
{
	auto pushInst = *_curr;
	assert(Instruction(pushInst) >= Instruction::PUSH1 && Instruction(pushInst) <= Instruction::PUSH32);
	auto numBytes = pushInst - static_cast<size_t>(Instruction::PUSH1) + 1;
	--_end;
	for (decltype(numBytes) i = 0; i < numBytes && _curr < _end; ++i, ++_curr) {}
}

code_iterator skipPushDataAndGetNext(code_iterator _curr, code_iterator _end)
{
	static const auto push1  = static_cast<size_t>(Instruction::PUSH1);
	static const auto push32 = static_cast<size_t>(Instruction::PUSH32);
	size_t offset = 1;
	if (*_curr >= push1 && *_curr <= push32)
		offset += std::min<size_t>(*_curr - push1 + 1, (_end - _curr) - 1);
	return _curr + offset;
}

}
}

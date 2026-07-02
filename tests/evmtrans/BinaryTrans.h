#pragma once

// #include <cstdint>
// #include <cstring>
#include <stdint.h>
#include <string.h>
#include <functional>
#include <type_traits>

#include <stdio.h>
#include <iostream>
#include <fstream>

#include <evmtrans.h>

namespace dev
{
namespace evmtrans
{

using byte = uint8_t;
using bytes_ref = std::tuple<byte const*, size_t>;

/// Representation of 256-bit value binary compatible with LLVM i256
struct i256
{
	uint64_t words[4];

	i256() = default;
};

// TODO: Merge with ExecutionContext
struct RuntimeData
{
	enum Index
	{
		Gas,
		GasPrice,
		CallData,
		CallDataSize,
		Value,  // Value of msg.value - different during DELEGATECALL.
		Code,
		CodeSize,
		Address,
		Sender,
		Depth,

		ReturnData 		   = CallData,		///< Return data pointer (set only in case of RETURN)
		ReturnDataSize 	   = CallDataSize,	///< Return data size (set only in case of RETURN)
	};

	static size_t const numElements = Depth + 1;

	int64_t 	gas = 0;
	int64_t 	gasPrice = 0;
	byte const* callData = nullptr;
	uint64_t 	callDataSize = 0;
	i256 		apparentValue;
	byte const* code = nullptr;
	uint64_t 	codeSize = 0;
	byte        address[32];
	byte        caller[32];
	int32_t     depth;
};

enum class ReturnCode
{
	// Success codes
	Stop    = 0,
	Return  = 1,
	Revert  = 2,

	// Standard error codes
	OutOfGas           = -1,

	// Internal error codes
	LLVMError          = -101,

	UnexpectedException = -111,
};

// class ExecutionContext
// {
// public:
// 	ExecutionContext() = default;
// 	ExecutionContext(RuntimeData& _data, evmc_context* _ctx) { init(_data, _ctx); }
// 	ExecutionContext(ExecutionContext const&) = delete;
// 	ExecutionContext& operator=(ExecutionContext const&) = delete;
// 	~ExecutionContext() noexcept;

// 	void init(RuntimeData& _data, evmc_context* _ctx) { m_data = &_data; m_ctx = _ctx; }

// 	byte const* code() const { return m_data->code; }
// 	uint64_t codeSize() const { return m_data->codeSize; }

// 	bytes_ref getReturnData() const;

// public:
// 	RuntimeData* m_data = nullptr;	///< Pointer to data. Expected by compiled contract.
// 	evmc_context* m_ctx = nullptr;	///< Pointer to Host execution context. Expected by compiled contract.
// 	byte* m_memData = nullptr;
// 	uint64_t m_memSize = 0;
// 	uint64_t m_memCap = 0;

// public:
// 	/// Reference to returned data (RETURN opcode used)
// 	bytes_ref returnData;
// };

}
}

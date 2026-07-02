#include "BinaryTrans.h"



#include <cstddef>
#include <mutex>
#include <evmc/evmc.h>
#include <system_error>

#include "preprocessor/llvm_includes_start.h"
#include <llvm/IR/Module.h>
#include <llvm/ADT/StringSwitch.h>
#include <llvm/ADT/Triple.h>
#include <llvm/Support/TargetSelect.h>
#include <llvm/Support/raw_os_ostream.h>
#include <llvm/Support/TargetRegistry.h>
#include <llvm/Support/CommandLine.h>
#include <llvm/Target/TargetOptions.h>
#include <llvm/Target/TargetMachine.h>
#include <llvm/MC/SubtargetFeature.h>
#include "preprocessor/llvm_includes_end.h"

#include "Optimizer.h"
// #include "Cache.h"
// #include "ExecStats.h"
// #include "Utils.h"
#include "BuildInfo.gen.h"
#include "CodeGen.h"
#include "hera/hera.h"
#include "WASMCompiler.h"

// FIXME: Move these checks to evmc tests.
static_assert(sizeof(evmc_uint256be) == 32, "evmc_uint256be is too big");
static_assert(sizeof(evmc_address) == 20, "evmc_address is too big");
static_assert(sizeof(evmc_result) == 64, "evmc_result does not fit cache line");
static_assert(sizeof(evmc_message) <= 18*8, "evmc_message not optimally packed");
// static_assert(offsetof(evmc_message, code_hash) % 8 == 0, "evmc_message.code_hash not aligned");

// Check enums match int size.
// On GCC/clang the underlying type should be unsigned int, on MSVC int
static_assert(sizeof(evmc_call_kind)  == sizeof(int), "Enum `evmc_call_kind` is not the size of int");
static_assert(sizeof(evmc_revision)       == sizeof(int), "Enum `evmc_revision` is not the size of int");

constexpr size_t optionalDataSize = sizeof(evmc_result) - offsetof(evmc_result, create_address);
static_assert(optionalDataSize == sizeof(evmc_result_optional_data), "");


namespace dev
{
namespace evmtrans
{
using namespace eth::trans;

namespace
{
// using ExecFunc = ReturnCode(*)(ExecutionContext*);
using ExecFunc = codeGenResult*;

struct CodeMapEntry
{
    ExecFunc func = nullptr;
    size_t hits = 0;

    CodeMapEntry() = default;
    explicit CodeMapEntry(ExecFunc func) : func(func) {}
};

char toChar(evmc_revision rev)
{
	switch (rev)
	{
	case EVMC_FRONTIER: return 'F';
	case EVMC_HOMESTEAD: return 'H';
	case EVMC_TANGERINE_WHISTLE: return 'T';
	case EVMC_SPURIOUS_DRAGON: return 'S';
	case EVMC_BYZANTIUM: return 'B';
	case EVMC_CONSTANTINOPLE: return 'C';
	}
	LLVM_BUILTIN_UNREACHABLE;
}

/// Combine code hash and EVM revision into a printable code identifier.
// std::string makeCodeId(evmc_uint256be codeHash, evmc_revision rev, uint32_t flags)
// {
// 	static const auto hexChars = "0123456789abcdef";
// 	std::string str;
// 	str.reserve(sizeof(codeHash) * 2 + 1);
// 	for (auto b: codeHash.bytes)
// 	{
// 		str.push_back(hexChars[b >> 4]);
// 		str.push_back(hexChars[b & 0xf]);
// 	}
// 	str.push_back(toChar(rev));
// 	if (flags & EVMC_STATIC)
// 		str.push_back('S');
// 	return str;
// }
std::string makeCodeId(evmc_address destination, evmc_revision rev, uint32_t flags)
{
	static const auto hexChars = "0123456789abcdef";
	std::string str;
	str.reserve(sizeof(destination) * 2 + 1);
	for (auto b: destination.bytes)
	{
		str.push_back(hexChars[b >> 4]);
		str.push_back(hexChars[b & 0xf]);
	}
	// str.push_back(toChar(rev));
	// if (flags & EVMC_STATIC)
	// 	str.push_back('S');
	return str;
}

// void printVersion()
// {
// 	std::cout << "Ethereum EVM to EWASM Binary Translator:\n"
// 			  << "  EVMTRANS version " << EVMTRANS_VERSION << "\n"
// #ifdef NDEBUG
// 			  << "  Optimized build, "
// #else
// 			  << "  DEBUG build, "
// #endif
// 			  << __DATE__ << " (" << __TIME__ << ")\n"
// 			  << std::endl;
// }

namespace cl = llvm::cl;
llvm::cl::opt<bool> g_optimize{"O", cl::desc{"Optimize"}};
// llvm::cl::opt<CacheMode> g_cache{"cache", cl::desc{"Cache compiled EVM code on disk"},
// 	cl::values(
// 		clEnumValN(CacheMode::off,   "0", "Disabled"),
// 		clEnumValN(CacheMode::on,    "1", "Enabled"),
// 		clEnumValN(CacheMode::read,  "r", "Read only. No new objects are added to cache."),
// 		clEnumValN(CacheMode::write, "w", "Write only. No objects are loaded from cache."),
// 		clEnumValN(CacheMode::clear, "c", "Clear the cache storage. Cache is disabled."),
// 		clEnumValN(CacheMode::preload, "p", "Preload all cached objects."))};
cl::opt<bool> g_stats{"st", cl::desc{"Statistics"}};
cl::opt<bool> g_dump{"dump", cl::desc{"Dump LLVM IR module"}};

void parseOptions()
{
	static llvm::llvm_shutdown_obj shutdownObj{};
	// cl::AddExtraVersionPrinter(printVersion);
	cl::ParseEnvironmentOptions("evmtrans", "EVMTRANS", "Ethereum EVM  Bytecode to EWASM Binary Translator");
}

class BTImpl: public evmc_instance
{
	mutable std::mutex x_codeMap;
	std::unordered_map<std::string, CodeMapEntry> m_codeMap;
	std::vector<std::unique_ptr<codeGenResult>> codeStore;

	std::unique_ptr<llvm::TargetMachine> t_machine;

	static llvm::LLVMContext& getLLVMContext()
	{
		// TODO: This probably should be thread_local, but for now that causes
		// a crash when MCJIT is destroyed.
		static llvm::LLVMContext llvmContext;
		return llvmContext;
	}

	void createTarget();

public:
	static BTImpl& instance()
	{
		// We need to keep this a singleton.
		// so we only call changeVersion on it.
		static BTImpl s_instance;
		return s_instance;
	}

	BTImpl();

	CodeMapEntry getExecFunc(std::string const& _codeIdentifier);
	void mapExecFunc(std::string const& _codeIdentifier, ExecFunc _funcAddr);
	ExecFunc storeExecFunc(std::unique_ptr<codeGenResult> result);

	ExecFunc compile(evmc_revision _rev, bool _staticCall, byte const* _code, uint64_t _codeSize, std::string const& _codeIdentifier);

	evmc_host_interface const* host = nullptr;

	evmc_message const* currentMsg = nullptr;
	std::vector<uint8_t> returnBuffer;

    std::vector<uint8_t> codeBuffer;

    size_t hitThreshold = 0;

	evmc_instance* hera_instance = nullptr;
};

CodeMapEntry BTImpl::getExecFunc(std::string const& _codeIdentifier)
{
    std::lock_guard<std::mutex> lock{x_codeMap};
    auto& entry = m_codeMap[_codeIdentifier];
    ++entry.hits;
    return entry;
}

void BTImpl::mapExecFunc(std::string const& _codeIdentifier, ExecFunc _funcAddr)
{
    std::lock_guard<std::mutex> lock{x_codeMap};
    m_codeMap[_codeIdentifier].func = _funcAddr;
}

ExecFunc BTImpl::storeExecFunc(std::unique_ptr<codeGenResult> result) {
	ExecFunc func = result.get();
	codeStore.push_back(std::move(result));
	return func;
}

ExecFunc BTImpl::compile(evmc_revision _rev, bool _staticCall, byte const* _code, uint64_t _codeSize,
	std::string const& _codeIdentifier)
{
	return nullptr;
	// // auto module = Cache::getObject(_codeIdentifier, getLLVMContext());
	// // if (!module)
	// // {
	// 	assert(_code || !_codeSize);
	// 	// std::cerr << "before WASMCompile\n";
	// 	std::string str = "";
	// 	for (auto i = 0;i <= _codeSize; i++)
	// 		str += *(_code+i);
	// 	auto module = WASMCompiler({}, getLLVMContext()).compileMain(_code, _code + _codeSize, _codeIdentifier, nullptr, 0, llvm::StringRef(str), 0);
	// 	std::cerr << "after WASMCompile\n";

	// 	// prepare(*module);
	// 	std::cerr << "after optimization\n";
	// // }

	// // Print LLVM IR
	// if (g_dump)
	// {
	// 	// auto outfile = "/home/toor/evmTrans/blockchain/llvm.ll";
	// 	auto outfile = "llvm.ll";
	// 	std::error_code ec;
	// 	llvm::raw_fd_ostream fout(llvm::StringRef(outfile), ec, llvm::sys::fs::F_None);
	// 	if (ec) {
	// 		llvm::raw_os_ostream cerr{std::cerr};
	// 		module->print(cerr, nullptr);
	// 	} else {
	// 		module->print(fout, nullptr);
	// 	}
	// 	std::cerr << "LLVM IR dump success.\n";
	// }

	// std::unique_ptr<codeGenResult> code_gen = codeGenModule(*t_machine, *module);
	// std::cerr << "after codeGen\n";

	// // prepare(*module);

	// // m_engine->addModule(std::move(module));
	// //listener->stateChanged(ExecState::CodeGen);
	// // return (ExecFunc)m_engine->getFunctionAddress(_codeIdentifier);
	// return storeExecFunc(std::move(code_gen));
}

} // anonymous namespace

void BTImpl::createTarget() {
	llvm::CodeGenOpt::Level OLvl = llvm::CodeGenOpt::Default;
	auto TheTriple = llvm::Triple(llvm::Triple::normalize("wasm32-unknown-unknown"));
	// llvm::raw_os_ostream cerr{std::cerr};
	// llvm::TargetRegistry::printRegisteredTargetsForVersion(cerr);
	std::string Error;
	const llvm::Target *TheTarget = llvm::TargetRegistry::lookupTarget(TheTriple.getTriple(), Error);
	if (!TheTarget) {
		std::cerr << Error << "\n";
		return;
	}
	llvm::TargetOptions Options = dev::eth::trans::InitTargetOptionsFromCodeGenFlags();
	std::string CPUStr = "";
	llvm::SubtargetFeatures Features;
    std::string FeaturesStr = Features.getString();
	std::unique_ptr<llvm::TargetMachine> Target(TheTarget->createTargetMachine(
		TheTriple.getTriple(), CPUStr, FeaturesStr, Options, llvm::Reloc::Static,
		llvm::CodeModel::Small, OLvl));
	assert(Target && "Could not allocate target machine!");

	t_machine = std::move(Target);
}

BTImpl::BTImpl()
  : evmc_instance({
        EVMC_ABI_VERSION,
        "evmtrans",
        EVMTRANS_VERSION,
        nullptr, nullptr, nullptr, nullptr, nullptr
    })
{
	parseOptions();

	// bool preloadCache = g_cache == CacheMode::preload;
	// if (preloadCache)
	// 	g_cache = CacheMode::on;

	// llvm::InitializeNativeTarget();
	// llvm::InitializeNativeTargetAsmPrinter();
	llvm::InitializeAllTargets();
	llvm::InitializeAllTargetMCs();
	llvm::InitializeAllAsmPrinters();
	// llvm::InitializeAllAsmParsers();

	createTarget();
	hera_instance = hera::evmc_create_hera();
}

static void destroy(evmc_instance* instance)
{
	// (void)instance;
	// assert(instance == static_cast<void*>(&BTImpl::instance()));
	BTImpl* ins = static_cast<BTImpl*>(instance);
	ins->hera_instance->destroy(ins->hera_instance);
  	// delete ins;
	(void)instance;
	assert(instance == static_cast<void*>(&BTImpl::instance()));
}

struct evmc_result nativeResult(struct evmc_result result) {
	evmc_result native_result = result;
	uint8_t *outputBuffer = new uint8_t[result.output_size];

	uint8_t *it = const_cast<uint8_t*>(result.output_data), *bf = outputBuffer;
	for (int i = 0; i < result.output_size; i++) {
		*bf++ = *it++;
	}
	std::reverse(outputBuffer, outputBuffer + result.output_size);

	native_result.output_data = outputBuffer;
	return native_result;
}

static struct evmc_result execute(struct evmc_instance* instance,
                                  struct evmc_context* context,
                                  enum evmc_revision rev,
                                  const struct evmc_message* msg,
                                  const uint8_t* code,
                                  size_t code_size)
{

    // // The EIP-1352 (https://eips.ethereum.org/EIPS/eip-1352) defines
    // // the range 0 - 0xffff (2 bytes) of addresses reserved for precompiled contracts.
    // // Check if the destination address is within the reserved range.
    // constexpr auto prefix_size = sizeof(evmc_address) - 2;
    // const auto& dst = msg->destination;
    // // Check if the address prefix is all zeros.
    // if (std::any_of(&dst.bytes[0], &dst.bytes[prefix_size], [](uint8_t x) { return x != 0; }))
    // {
    //     // If not, reject the execution request.
    //     auto result = evmc_result{};
    //     result.status_code = EVMC_REJECTED;
    //     return result;
    // }
    // // Extract the precompiled contract id from last 2 bytes of the destination address.
    // const auto id = (dst.bytes[prefix_size] << 8) | dst.bytes[prefix_size + 1];
    // switch (id)
    // {
    // case 0x0001:  // ECDSARECOVER
    // case 0x0002:  // SHA256
    // case 0x0003:  // RIPEMD160
    //     return not_implemented();
    // case 0x0004:  // Identity
    //     return execute_identity(msg);
    // case 0x0005:  // EXPMOD
    // case 0x0006:  // SNARKV
    // case 0x0007:  // BNADD
    // case 0x0008:  // BNMUL
    //     if (rev < EVMC_BYZANTIUM)
    //         return execute_empty(msg);
    //     return not_implemented();

    // default:  // As if empty code was executed.
    //     return execute_empty(msg);
    // }



	auto& bt = *reinterpret_cast<BTImpl*>(instance);

	std::cerr << "EVMTRANS Execute\n";

	if (!bt.host)
		bt.host = context->host;
	assert(bt.host == context->host);  // Require the fn_table not to change.

	// TODO: Temporary keep track of the current message.
	evmc_message const* prevMsg = bt.currentMsg;
	bt.currentMsg = msg;

	evmc_result result;
	result.status_code = EVMC_SUCCESS;
	result.gas_left = 0;
	result.output_data = nullptr;
	result.output_size = 0;
	result.release = nullptr;

    // auto codeIdentifier = makeCodeId(msg->code_hash, rev, msg->flags);
	auto codeIdentifier = makeCodeId(msg->destination, rev, msg->flags);
    auto codeEntry = bt.getExecFunc(codeIdentifier);
    auto func = codeEntry.func;
	// std::unique_ptr<codeGenResult> func;
	std::cerr << "codeEntry.func: " << codeIdentifier << "\n"; // codeIdentifier is the address

    if (!func)
    {
        //FIXME: We have a race condition here!
        if (codeEntry.hits <= bt.hitThreshold)
        {
            result.status_code = EVMC_REJECTED;
            return result;
        }

        if (g_stats)
        	std::cerr << "EVMTRANS Compile " << codeIdentifier << " (" << codeEntry.hits << ")\n";

        const bool staticCall = (msg->flags & EVMC_STATIC) != 0;
		func = bt.compile(rev, staticCall, code, code_size, codeIdentifier);
        if (!func || !func->getCode() || func->getCodeSize() == 0)
        {
            result.status_code = EVMC_INTERNAL_ERROR;
            return result;
        }
        bt.mapExecFunc(codeIdentifier, func);
    }
	
	
	std::cerr << "[-] request hera to execute\n"; 
    // auto returnCode = func(&ctx);
	result = bt.hera_instance->execute(bt.hera_instance, context, rev, msg, func->getCode(), func->getCodeSize());
	
	bt.currentMsg = prevMsg;
	if (result.output_size == 0)
		return result;
	return nativeResult(result);
}

evmc_set_option_result setOption(struct evmc_instance* evm, char const* name, char const* value) noexcept {
    try
    {
        if (name == std::string{"verbose"})
        {
            if (value && (value == std::string{"1"} || value == std::string{"0"}))
				return EVMC_SET_OPTION_SUCCESS;
            return EVMC_SET_OPTION_INVALID_VALUE;
        }
        return EVMC_SET_OPTION_INVALID_NAME;
    }
    catch (...)
    {
        return EVMC_SET_OPTION_SUCCESS;
		// return EVMC_SET_OPTION_INVALID_NAME;
    }
}

evmc_capabilities_flagset getCapabilities(evmc_instance* instance)
{
    evmc_capabilities_flagset caps = EVMC_CAPABILITY_EWASM;
//   if (static_cast<hera_instance*>(vm)->evm1mode != hera_evm1mode::reject)
    caps |= EVMC_CAPABILITY_EVM1;
    return caps;
}

void setTracer(struct evmc_instance* instance, evmc_trace_callback callback, struct evmc_tracer_context* context) {
}

extern "C"
{

evmc_instance* evmc_create_evmtrans() noexcept
{
  BTImpl* instance = &BTImpl::instance();
  instance->destroy = evmtrans::destroy;
  instance->execute = evmtrans::execute;
  instance->get_capabilities = evmtrans::getCapabilities;
  instance->set_tracer = evmtrans::setTracer;
  instance->set_option = evmtrans::setOption;
  return instance;
}

// If compiled as shared library, also export this symbol.
EVMC_EXPORT evmc_instance* evmc_create() noexcept
{
  return evmc_create_evmtrans();
}

}  // extern "C"

}
}

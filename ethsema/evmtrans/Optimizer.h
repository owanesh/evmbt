#pragma once

namespace llvm
{
	class Module;
}

namespace dev
{
namespace eth
{
namespace trans
{

struct Options {
	bool enableCheckReentrancy;
	bool enableDetectSuicide;
	bool enableCheckSend;
	bool enableEOAonly;
	bool enableRmOrigin;
	bool enableSafeMath;

	bool isRtcode;
	bool enableUpgrade;
	int vulpc;

	bool enableKlee;
};

bool optimize(llvm::Module& _module);

bool prepare(llvm::Module& _module, Options options);

}
}
}

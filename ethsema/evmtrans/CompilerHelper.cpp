#pragma once

#include "CompilerHelper.h"

namespace dev
{
namespace eth
{
namespace trans
{

InsertPointGuard::InsertPointGuard(llvm::IRBuilderBase& _builder): 
    m_builder(_builder), m_insertPoint(_builder.saveIP()) {}

}
}
}
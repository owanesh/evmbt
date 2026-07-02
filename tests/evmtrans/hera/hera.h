#pragma once

#include <evmc/evmc.h>
#include <evmc/utils.h>

namespace hera {

evmc_instance* evmc_create_hera() noexcept;
    
}

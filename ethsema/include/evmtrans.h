#pragma once

#include <evmc/evmc.h>
#include <evmc/utils.h>

#ifdef _MSC_VER
#ifdef evmtrans_EXPORTS
#define EXPORT __declspec(dllexport)
#else
#define EXPORT
#endif

#else
#define EXPORT __attribute__ ((visibility ("default")))
#endif

#if __cplusplus
extern "C" {
#endif

/// Create EVMTRANS instance.
///
/// @return  The EVMTRANS instance.
// EXPORT struct evmc_instance* evmtrans_create(void);
// EVMC_EXPORT struct evmc_vm* evmc_create_evmtrans(void) EVMC_NOEXCEPT;
EVMC_EXPORT struct evmc_instance* evmc_create_evmtrans(void) EVMC_NOEXCEPT;

#if __cplusplus
}
#endif

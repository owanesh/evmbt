// following     https://github.com/ewasm/design/commit/8946232935822723a4c80bec03eb1b8ecd237d5f
#pragma once

#include "NEARModule.h"
#include "Type.h"
// #include "Endianness.h"

#include "preprocessor/llvm_includes_start.h"
#include <llvm/IR/CFG.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/IntrinsicInst.h>
#include <llvm/IR/GlobalVariable.h>
#include <llvm/IR/Constants.h>
#include <llvm/Support/Alignment.h>
#include "preprocessor/llvm_includes_end.h"

namespace dev
{
namespace eth
{
namespace trans
{


NEARModule::NEARModule(llvm::Module &m) : VMMod(m), VMCtx(m.getContext()) {}

void NEARModule::add_attr(llvm::Function* func, const std::string name) {
  func->addFnAttr(  
    llvm::Attribute::get(VMCtx, "wasm-import-module", "env"));  
  func->addFnAttr(  
    llvm::Attribute::get(VMCtx, "wasm-import-name", name)); 
}

// #define GEN_FUNC(n, ft) do { \
//   llvm::Function *func = TheModule->getFunction(n); \
//   if (func != nullptr) return func; \
//   func = llvm::Function::Create( \ 
//     ft, llvm::Function::ExternalLinkage, n, TheModule); \ 
//   func->addFnAttr(  \
//     llvm::Attribute::get(VMContext, "wasm-import-module", "env"));  \
//   func->addFnAttr(  \
//     llvm::Attribute::get(VMContext, "wasm-import-name", n)); \
//   return func; \
// } while(0)

// #############
// # Registers #
// #############

/// pub fn read_register(register_id: u64, ptr: u64);
/// Reads the content of the `register_id`. If register is not used returns `None`.
llvm::Function* NEARModule::fn_read_register() {
  static const auto name = "read_register";
  llvm::Function *func = VMMod.getFunction(name); 
  if (func != nullptr) 
    return func; 

  llvm::FunctionType *ft = llvm::FunctionType::get(
    Type::Void, {Type::Int64Ty, Type::Int64Ty}, false);
  func = llvm::Function::Create(
    ft, llvm::Function::ExternalLinkage, name, &VMMod);
  add_attr(func, name);
  return func;
}

// pub fn :$(register_id: u64) -> u64;
// pub fn write_register(register_id: u64, data_len: u64, data_ptr: u64);
    
// ###############
// # Context API #
// ###############
// pub fn current_account_id(register_id: u64);
// pub fn signer_account_id(register_id: u64);
// pub fn signer_account_pk(register_id: u64);
// pub fn predecessor_account_id(register_id: u64);

/// pub fn input(register_id: u64);
/// The input to the contract call serialized as bytes. If input is not provided returns `None`.
llvm::Function* NEARModule::fn_input() {
  static const auto name = "input";
  llvm::Function *func = VMMod.getFunction(name); 
  if (func != nullptr) 
    return func; 

  llvm::FunctionType *ft = llvm::FunctionType::get(
    Type::Void, {Type::Int64Ty, Type::Int64Ty}, false);
  func = llvm::Function::Create(
    ft, llvm::Function::ExternalLinkage, name, &VMMod);
  add_attr(func, name);
  return func;
}
// pub fn block_index() -> u64;
// pub fn block_timestamp() -> u64;
// pub fn epoch_height() -> u64;
// pub fn storage_usage() -> u64;


// #################
// # Economics API #
// #################
// pub fn account_balance(balance_ptr: u64);
// pub fn account_locked_balance(balance_ptr: u64);
// pub fn attached_deposit(balance_ptr: u64);
// pub fn prepaid_gas() -> u64;
// pub fn used_gas() -> u64;
// // ############
// // # Math API #
// // ############
// pub fn random_seed(register_id: u64);
// pub fn sha256(value_len: u64, value_ptr: u64, register_id: u64);
// pub fn keccak256(value_len: u64, value_ptr: u64, register_id: u64);
// pub fn keccak512(value_len: u64, value_ptr: u64, register_id: u64);
// pub fn ripemd160(value_len: u64, value_ptr: u64, register_id: u64);
// pub fn ecrecover(
//     hash_len: u64,
//     hash_ptr: u64,
//     sig_len: u64,
//     sig_ptr: u64,
//     v: u64,
//     malleability_flag: u64,
//     register_id: u64,
// ) -> u64;

// #####################
// # Miscellaneous API #
// #####################

/// pub fn value_return(value_len: u64, value_ptr: u64);
/// Sets the blob of data as the return value of the contract.
llvm::Function* NEARModule::fn_value_return() {
  static const auto name = "value_return";
  llvm::Function *func = VMMod.getFunction(name); 
  if (func != nullptr) 
    return func; 

  llvm::FunctionType *ft = llvm::FunctionType::get(Type::Void, 
    {Type::Int64Ty, Type::Int64Ty}, false);
  func = llvm::Function::Create(
    ft, llvm::Function::ExternalLinkage, name, &VMMod);
  add_attr(func, name);
  return func;
}
  // pub fn panic() -> !;
  // pub fn panic_utf8(len: u64, ptr: u64) -> !;
  // pub fn log_utf8(len: u64, ptr: u64);
  // pub fn log_utf16(len: u64, ptr: u64);
  // pub fn abort(msg_ptr: u32, filename_ptr: u32, line: u32, col: u32) -> !;
  
// ################
// # Promises API #
// ################
  // pub fn promise_create(
  //     account_id_len: u64,
  //     account_id_ptr: u64,
  //     function_name_len: u64,
  //     function_name_ptr: u64,
  //     arguments_len: u64,
  //     arguments_ptr: u64,
  //     amount_ptr: u64,
  //     gas: u64,
  // ) -> u64;
  // pub fn promise_then(
  //     promise_index: u64,
  //     account_id_len: u64,
  //     account_id_ptr: u64,
  //     function_name_len: u64,
  //     function_name_ptr: u64,
  //     arguments_len: u64,
  //     arguments_ptr: u64,
  //     amount_ptr: u64,
  //     gas: u64,
  // ) -> u64;
  // pub fn promise_and(promise_idx_ptr: u64, promise_idx_count: u64) -> u64;
  // pub fn promise_batch_create(account_id_len: u64, account_id_ptr: u64) -> u64;
  // pub fn promise_batch_then(promise_index: u64, account_id_len: u64, account_id_ptr: u64) -> u64;
  
  // #######################
  // # Promise API actions #
  // #######################
  // pub fn promise_batch_action_create_account(promise_index: u64);
  // pub fn promise_batch_action_deploy_contract(promise_index: u64, code_len: u64, code_ptr: u64);
  // pub fn promise_batch_action_function_call(
  //     promise_index: u64,
  //     function_name_len: u64,
  //     function_name_ptr: u64,
  //     arguments_len: u64,
  //     arguments_ptr: u64,
  //     amount_ptr: u64,
  //     gas: u64,
  // );
  // pub fn promise_batch_action_function_call_weight(
  //     promise_index: u64,
  //     function_name_len: u64,
  //     function_name_ptr: u64,
  //     arguments_len: u64,
  //     arguments_ptr: u64,
  //     amount_ptr: u64,
  //     gas: u64,
  //     weight: u64,
  // );
  // pub fn promise_batch_action_transfer(promise_index: u64, amount_ptr: u64);
  // pub fn promise_batch_action_stake(
  //     promise_index: u64,
  //     amount_ptr: u64,
  //     public_key_len: u64,
  //     public_key_ptr: u64,
  // );
  // pub fn promise_batch_action_add_key_with_full_access(
  //     promise_index: u64,
  //     public_key_len: u64,
  //     public_key_ptr: u64,
  //     nonce: u64,
  // );
  // pub fn promise_batch_action_add_key_with_function_call(
  //     promise_index: u64,
  //     public_key_len: u64,
  //     public_key_ptr: u64,
  //     nonce: u64,
  //     allowance_ptr: u64,
  //     receiver_id_len: u64,
  //     receiver_id_ptr: u64,
  //     function_names_len: u64,
  //     function_names_ptr: u64,
  // );
  // pub fn promise_batch_action_delete_key(
  //     promise_index: u64,
  //     public_key_len: u64,
  //     public_key_ptr: u64,
  // );
  // pub fn promise_batch_action_delete_account(
  //     promise_index: u64,
  //     beneficiary_id_len: u64,
  //     beneficiary_id_ptr: u64,
  // );
  // // #######################
  // // # Promise API results #
  // // #######################
  // pub fn promise_results_count() -> u64;
  // pub fn promise_result(result_idx: u64, register_id: u64) -> u64;
  // pub fn promise_return(promise_id: u64);
  // // ###############
  // // # Storage API #
  // // ###############
  // pub fn storage_write(
  //     key_len: u64,
  //     key_ptr: u64,
  //     value_len: u64,
  //     value_ptr: u64,
  //     register_id: u64,
  // ) -> u64;
  // pub fn storage_read(key_len: u64, key_ptr: u64, register_id: u64) -> u64;
  // pub fn storage_remove(key_len: u64, key_ptr: u64, register_id: u64) -> u64;
  // pub fn storage_has_key(key_len: u64, key_ptr: u64) -> u64;
  // pub fn storage_iter_prefix(prefix_len: u64, prefix_ptr: u64) -> u64;
  // pub fn storage_iter_range(start_len: u64, start_ptr: u64, end_len: u64, end_ptr: u64) -> u64;
  // pub fn storage_iter_next(iterator_id: u64, key_register_id: u64, value_register_id: u64)
  //     -> u64;
  // ###############
  // # Validator API #
  // ###############
  // pub fn validator_stake(account_id_len: u64, account_id_ptr: u64, stake_ptr: u64);
  // pub fn validator_total_stake(stake_ptr: u64);
  // // #############
  // // # Alt BN128 #
  // // #############
  // pub fn alt_bn128_g1_multiexp(value_len: u64, value_ptr: u64, register_id: u64);
  // pub fn alt_bn128_g1_sum(value_len: u64, value_ptr: u64, register_id: u64);
  // pub fn alt_bn128_pairing_check(value_len: u64, value_ptr: u64) -> u64;
 

}
}
}
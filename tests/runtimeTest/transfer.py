import os
import json
from utils import runRPC, cmdLine, deploy_contract, activate_contract, prtInfo, prtDeug, compile_source_file, evm2ewasm

def main():
    args = cmdLine()
    w3 = runRPC()
    if len(w3.eth.accounts) < 3:
        w3.geth.personal.new_account("")
    for account in w3.eth.accounts:
        w3.geth.personal.unlock_account(account, "", 99999)

    solPath = "./contracts/Transfer.sol"
    metadataPath = solPath[:-4] + '.json'
    if os.path.exists(metadataPath) and not args.overwrite:
        with open(metadataPath, 'r') as f:
            obj = json.load(f)
            address = obj['addr']
            contractABI = obj['abi']
    else:
        cmrt = compile_source_file(solPath, version='0.4.26')
        contractHex = '0x'+cmrt['bin'] if cmrt['bin'][:2] != '0x' else cmrt['bin']
        contractABI =  cmrt['abi']
        contractEwasm = evm2ewasm(contractHex, args)

        address = deploy_contract(w3, w3.eth.accounts[0], contractABI, contractEwasm)
        with open(metadataPath, 'w') as f:
            json.dump({'abi':contractABI, 'hex':contractHex, 'bin':contractEwasm, 'addr':address}, f)

    instance = activate_contract(w3, address, contractABI)

    prtInfo(f"=========================== initial states ================================")    
    initres = w3.eth.get_balance(w3.eth.accounts[2])
    prtDeug(f"Balance[account2]@={initres}")

    # Transfer(acc1, acc2, 1 ether)
    seed = w3.toWei(1, 'ether')
    assert(w3.eth.get_balance(w3.eth.accounts[1]) > seed)

    _tx_hash = instance.functions.test(w3.eth.accounts[2]).transact({'from':w3.eth.accounts[1], 'value':seed}) 
    _receipt = w3.eth.wait_for_transaction_receipt(_tx_hash)
    usedGas = _receipt['gasUsed']
    prtDeug(f"UsedGas = {usedGas} wei")

    prtInfo(f"=========================== updated states ================================")    
    res = w3.eth.get_balance(w3.eth.accounts[1])
    prtDeug(f"Balance@{w3.eth.accounts[1]}={res}")
    
    res = w3.eth.get_balance(w3.eth.accounts[2])
    prtDeug(f"Balance@{w3.eth.accounts[2]}={res}")

    assert initres + seed == res, "unPassed"

main()
prtInfo("PASSED")
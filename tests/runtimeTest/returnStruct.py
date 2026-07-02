import os
import json
from utils import runRPC, cmdLine, deploy_contract, activate_contract, prtInfo, prtDeug, compile_source_file, evm2ewasm


def main():
    args = cmdLine()
    w3 = runRPC()
    solPath = "./contracts/returnStruct.sol"

    metadataPath = solPath[:-4] + '.json'
    if os.path.exists(metadataPath) and not args.overwrite:
        with open(metadataPath, 'r') as f:
            obj = json.load(f)
            address = obj['addr']
            contractABI = obj['abi']
    else:
        cmrt = compile_source_file(solPath, version='0.8.0')
        contractHex = '0x'+cmrt['bin'] if cmrt['bin'][:2] != '0x' else cmrt['bin']
        contractABI =  cmrt['abi']
        contractEwasm = evm2ewasm(contractHex, args)

        address = deploy_contract(w3, w3.eth.accounts[0], contractABI, contractEwasm)
        with open(metadataPath, 'w') as f:
            json.dump({'abi':contractABI, 'hex':contractHex, 'bin':contractEwasm, 'addr':address}, f)

    instance = activate_contract(w3, address, contractABI)

    with open('./t.wasm', 'wb') as f:
        f.write(w3.eth.getCode(address))

    prtInfo(f"=========================== initial states ================================")    
    prtDeug(instance.caller().get())

    seed = ("123", 987654321)
    _tx_hash = instance.functions.set(*seed).transact({'from':w3.eth.accounts[0], 'gas':100000}) 
    _receipt = w3.eth.wait_for_transaction_receipt(_tx_hash)
    usedGas = _receipt['gasUsed']
    prtDeug(f"UsedGas = {usedGas} wei")

    prtDeug(f"=========================== updated states ================================")    
    res = instance.caller().get()
    prtDeug(res)

    assert seed == res
    prtInfo(f"=========================== PASSED ================================")   

main()

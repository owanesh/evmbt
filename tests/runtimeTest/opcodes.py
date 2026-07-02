import os
import json
from utils import runRPC, cmdLine, deploy_contract, activate_contract, prtInfo, prtDeug, compile_source_file, evm2ewasm

def main():
    args = cmdLine()
    w3 = runRPC()
    solPath = "./contracts/OpCodeshh.sol"

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
    instance.caller().test()
    instance.caller().test_stop()
    instance.caller().test_revert()
    prtInfo("PASSED")

main()

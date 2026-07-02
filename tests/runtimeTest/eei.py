# author cswmchen
# date   11/26/2021
# usage  PASS: python -m runtimeTest.send --overwrite --langArgs="--check-send"

import os
import json
from utils import runRPC, cmdLine, deploy_contract, activate_contract, prtInfo, prtDeug, compile_source_file, evm2ewasm

def main():
    args = cmdLine()
    w3 = runRPC()
    solPath = "./contracts/eei.sol"

    cmrt = compile_source_file(solPath, version='0.8.7')
    contractHex = '0x'+cmrt['bin'] if cmrt['bin'][:2] != '0x' else cmrt['bin']
    contractABI =  cmrt['abi']
    contractEwasm = evm2ewasm(contractHex, args)

    address = deploy_contract(w3, w3.eth.accounts[0], contractABI, contractEwasm)
    instance = activate_contract(w3, address, contractABI)

    prtDeug(f'storage[0] = {instance.caller().test()}')
    prtInfo("PASSED")

main()

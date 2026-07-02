# author cswmchen
# date   11/26/2021
# usage  PASS: python -m runtimeTest.send --overwrite --langArgs="--check-send"

import os
import json
from utils import runRPC, cmdLine, deploy_contract, activate_contract, prtInfo, prtDeug, compile_source_file, evm2ewasm

def main():
    args = cmdLine()
    w3 = runRPC()
    solPath = "./contracts/vulSend.sol"

    cmrt = compile_source_file(solPath, version='0.5.10')
    contractHex = '0x'+cmrt['bin'] if cmrt['bin'][:2] != '0x' else cmrt['bin']
    contractABI =  cmrt['abi']
    contractEwasm = evm2ewasm(contractHex, args)

    address = deploy_contract(w3, w3.eth.accounts[0], contractABI, contractEwasm)
    instance = activate_contract(w3, address, contractABI)

    prtInfo(f"=========================== initial states ================================")    
    getStorageE = lambda : instance.caller().a()
    prtDeug(f'storage[0] = {getStorageE()}')


    assert getStorageE() == 0, "unPassed"
    prtInfo("PASSED #1")
    
    # should set a = 0
    _tx_hash = instance.functions.test().transact({'from':w3.eth.accounts[0], 'gas':100000}) 
    _receipt = w3.eth.wait_for_transaction_receipt(_tx_hash)
    prtDeug(f'storage[0] = {getStorageE()}')
    # print(_receipt["status"])


    # should set a = 10
    _tx_hash = instance.functions.test().transact({'from':w3.eth.accounts[0], 'gas':100000, 'value': 1}) 
    _receipt = w3.eth.wait_for_transaction_receipt(_tx_hash)
    # print(_receipt)
    prtDeug(f'storage[0] = {getStorageE()}')

    assert getStorageE() == 10, "unPassed"
    prtInfo("PASSED #2")

main()

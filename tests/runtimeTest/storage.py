import os
import json
from utils import runRPC, cmdLine, deploy_contract, activate_contract, prtInfo, prtDeug, compile_source_file, evm2ewasm

def main():
    args = cmdLine()
    w3 = runRPC()
    solPath = "./contracts/storage.sol"

    metadataPath = solPath[:-4] + '.json'
    if os.path.exists(metadataPath) and not args.overwrite:
        with open(metadataPath, 'r') as f:
            obj = json.load(f)
            address = obj['addr']
            contractABI = obj['abi']
    else:
        cmrt = compile_source_file(solPath, version='0.4.24')
        contractHex = '0x'+cmrt['bin'] if cmrt['bin'][:2] != '0x' else cmrt['bin']
        contractABI =  cmrt['abi']
        contractEwasm = evm2ewasm(contractHex, args)

        # _tx_hash = w3.eth.contract(abi=contractABI, bytecode=contractEwasm).constructor().transact({'from': w3.eth.accounts[0]})
        # address = w3.eth.wait_for_transaction_receipt(_tx_hash)['contractAddress'] 

        address = deploy_contract(w3, w3.eth.accounts[0], contractABI, contractEwasm)
        with open(metadataPath, 'w') as f:
            json.dump({'abi':contractABI, 'hex':contractABI, 'bin':contractEwasm, 'addr':address}, f)

    instance = activate_contract(w3, address, contractABI)

    prtInfo(f"=========================== initial states ================================")    
    prtDeug(f'storage[0] = {w3.eth.getStorageAt(address, 0x0000000000000000000000000000000000000000000000000000000000000000)}')
    initres = instance.caller().get()
    prtDeug(initres)

    seed = 0x123456789000000
    _tx_hash = instance.functions.set(0x123456789000000).transact({'from':w3.eth.accounts[0], 'gas':100000}) 
    _receipt = w3.eth.wait_for_transaction_receipt(_tx_hash)
    usedGas = _receipt['gasUsed']
    prtDeug(f"UsedGas = {usedGas} wei")

    prtInfo(f"=========================== updated states ================================")    
    res = instance.caller().get()
    prtDeug(res)
    prtDeug(f'storage[0] = {w3.eth.getStorageAt(address, 0x0000000000000000000000000000000000000000000000000000000000000000)}')
    assert initres + seed == res, "unPassed"
    prtInfo("PASSED")

    # print(instance.caller.hashstring())
    inputbytes = "0x1ed83fd40000000000000000000000000000000000000000000000000000000000000020000000000000000000000000000000000000000000000000000000000000004065323238373031373535386633626335306238616230623633666365646436323136656263326262373632363437616335383134363432613763663865383761"
    txObj = {'from': w3.eth.accounts[0], 'to': address, 'value': 0, 'gas': 63000001600, 'data': inputbytes}
    receipt = w3.eth.wait_for_transaction_receipt(w3.eth.send_transaction(txObj))
    assert receipt["status"] == 1
    # print(instance.caller.hashstring())
main()

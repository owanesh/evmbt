# author cswmchen
# date   15-11-21
# usage  python -m runtimeTest.rollback --overwrite --langArgs="--EOAonly"

import os
import json
from utils import cmdLine, runRPC, deploy_contract, activate_contract, prtInfo, prtDeug, compile_source_file, evm2ewasm

expAgentSol = '''
pragma solidity ^0.4.23;

contract Lottery {
    function rand() view public returns(uint256);
}
contract Attack {
    function getBalance(address addr) public view returns (uint256){
        return  Lottery(addr).rand();
    }
}
'''


def main():
    args = cmdLine()
    w3 = runRPC()

    cmrt = compile_source_file("./contracts/cross.sol", version='0.4.25')
    # calleeAddr = deploy_contract(w3, w3.eth.accounts[0], cmrt['abi'], cmrt['bin']) 
    calleeAddr = deploy_contract(w3, w3.eth.accounts[0], cmrt['abi'], cmrt['bin']) 


    callee = activate_contract(w3, calleeAddr, cmrt['abi'])
    del cmrt

    # deploy atkAgent
    with open('.tmp.sol', 'w') as f:
        f.write(expAgentSol)
    cmrt = compile_source_file('.tmp.sol', version='0.4.24')
    # callerAddr = deploy_contract(w3, w3.eth.accounts[1], cmrt['abi'], cmrt['bin']) 
    callerAddr = deploy_contract(w3, w3.eth.accounts[1], cmrt['abi'], evm2ewasm(cmrt['bin'], args)) 
    caller = activate_contract(w3, callerAddr, cmrt['abi'])
    del cmrt
  
    prtInfo(f"\ninteractve directly...")   
    
    # -----------------------------------------------
    # txObj = {
    #     'from': w3.eth.accounts[0],
    #     'to': calleeAddr,
    #     'value': 0,
    #     'gas': 300000000,
    #     'data': "0xf878540e",
    # }   
    # _ = w3.eth.send_transaction(txObj)
    
    # # print(callee.caller().rand())
    # return_data = w3.eth.call(
    #     txObj
    # )
    # print(return_data, "++++")

    # # _ = callee.functions.tttest().transact({'from':w3.eth.accounts[0], 'gas':300000000})
    # print(w3.eth.getTransaction(_.hex()))

    # _hash = w3.eth.wait_for_transaction_receipt(_.hex())
    # print("SUCCESS_", _hash['status'], _hash)
    #--------------------------------------


    eeRes = callee.caller().rand()
    erRes = caller.caller().getBalance(calleeAddr) 
    # print(eeRes, erRes)
    assert erRes == eeRes
    prtInfo("PASSED")
main()



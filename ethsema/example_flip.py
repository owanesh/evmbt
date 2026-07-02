# author cswmchen
# date   15-11-21
# python -m runtimeTest.example   --langArgs="--check-reentrancy"
import argparse
from asyncore import read, write
from dis import Bytecode
from web3 import Web3
import solcx
import os

victimSol = '''
contract flipper {
	bool private value;

	constructor(bool initvalue) public {
		value = initvalue;
	}

	function flip() public {
		value = !value;
	}

	function get() public view returns (bool) {
		return value;
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
    cmd = "cat ./.tmp.hex | xxd -r -ps > .tmp.bin && ./build/evmtrans/standalone-evmtrans .tmp.bin -o res.wasm " + \
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

    print("=============== Testing Ethsema ================")
    cmrt = compile_source_file(victimSol, version='0.4.24')
    _tx = {'from': w3.eth.accounts[0], 'value': 0, 'gas': 6300000160000, 'data': evm2ewasm(
        cmrt['bin'] + "0000000000000000000000000000000000000000000000000000000000000001", args)}
    receipt = w3.eth.wait_for_transaction_receipt(w3.eth.send_transaction(_tx))
    addr = receipt['contractAddress']
    instance = w3.eth.contract(address=addr, abi=cmrt['abi'])

    res = instance.caller().get()
    print("State = {}".format(res))

    _tx_hash = instance.functions.flip().transact({'from':w3.eth.accounts[0], 'gas':100000}) 
    _ = w3.eth.wait_for_transaction_receipt(_tx_hash)
    res = instance.caller().get()
    print("State = {}".format(res))

    print("=============== Testing Solang ================")

    os.system(f"mkdir -p .solang")
    with open(".solang/flip.sol", "w") as f:
        f.write(victimSol)
        
    cmd = "docker run --rm -it -v $(pwd)/.solang:/sources ghcr.io/hyperledger-labs/solang -v -o /sources --target ewasm /sources/flip.sol >/dev/null 2>&1"
    os.system(cmd)
    os.system("xxd -p ./.solang/flipper.wasm | tr -d $'\n' > .solang/hex")
    with open(".solang/hex", "r") as f:
        bytecode = f.read()

    _tx = {'from': w3.eth.accounts[0], 'value': 0, 'gas': 6300000160000, 'data': bytecode}
    receipt = w3.eth.wait_for_transaction_receipt(w3.eth.send_transaction(_tx))
    addr = receipt['contractAddress']
    instance = w3.eth.contract(address=addr, abi=cmrt['abi'])

    res = instance.caller().get()
    print("State = {}".format(res))

    _tx_hash = instance.functions.flip().transact({'from':w3.eth.accounts[0], 'gas':100000}) 
    _ = w3.eth.wait_for_transaction_receipt(_tx_hash)
    res = instance.caller().get()
    print("State = {}".format(res))

main()

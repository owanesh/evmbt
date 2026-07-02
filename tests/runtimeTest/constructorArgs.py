import os
import json
from utils import runRPC, cmdLine, deploy_contract, activate_contract, prtInfo, prtDeug, compile_source_file, evm2ewasm


instanceSol = '''
contract ADD {
    bytes a = hex"1213";
	function ADD(bytes memory _a) public{
		a = _a;
	}
    function get() public view returns(bytes memory) {
	 	return a;
	}

	function set() public {
	 	bytes memory _a = hex"998899888080080808008099889988808008080800809988998880800808080080998899888080080808008099889988808008080800809988998880800808080080998899888080080808008099889988808008080800809988998880800808080080998899888080080808008099889988808008080800809988998880800808080080998899888080080808008099889988808008080800809988998880800808080080998899888080080808008099889988808008080800809988998880800808080080998899888080080808008099889988808008080800809988998880800808080080808080808080808080808080808080808080808080808080808080808080808080808080808080808080808080808080808080808080808080808080808080808080808080808080808080808080808080808080808080808080808080808080808080808080808080";
	 	a = _a;
	}

}
'''

def main():
    args = cmdLine()
    w3 = runRPC()
    # print(w3.eth.get_block('latest'))
    # exit()
    
    with open('.tmp.sol', 'w') as f:
        f.write(instanceSol)
    cmrt = compile_source_file('.tmp.sol', version='0.4.24')
    constructArgs = "000000000000000000000000000000000000000000000000000000000000002000000000000000000000000000000000000000000000000000000000000000026080000000000000000000000000000000000000000000000000000000000000"
    contractHex = ('0x'+cmrt['bin'] if cmrt['bin'][:2] != '0x' else cmrt['bin']) + constructArgs
    contractABI =  cmrt['abi']

    contractEwasm = evm2ewasm(contractHex, args)
    # contractEwasm = contractHex 
    tx = {
        'from': w3.eth.accounts[0],
        'value': 0,
        'gas': 63000001600,
        'data': contractEwasm,
        # 'gasPrice': 400000000,
    }
    _ = w3.eth.send_transaction(tx)
    tx = w3.eth.wait_for_transaction_receipt(_.hex())
    address = tx['contractAddress']
    # print(address)
    instance = activate_contract(w3, address, contractABI)
    # print("code = ", w3.eth.getCode(address))
    # exit()
    prtInfo(f"=========================== initial states ================================")    
    prtDeug(f'storage[0] = {w3.eth.getStorageAt(address, 0x0000000000000000000000000000000000000000000000000000000000000000)}')
  
    initres = instance.caller().get()
    # print(initres)

    # print(instance.functions.set().buildTransaction({'from':w3.eth.accounts[0]}) )
    txObj = {
            'from': w3.eth.accounts[0],
            'to': address,
            'value': 0,
            'gas': 3000000,
            'data': "0xb8e010de", # set()
            'gasPrice': 400000
            # 'chainId':66
            }
    # while True:
    _txhash = w3.eth.send_transaction(txObj)  
    _ = w3.eth.wait_for_transaction_receipt(_txhash)
    prtInfo("PASSED")

main()

import argparse
import random
import os
import subprocess
import sys
import json

from web3 import Web3
from web3.middleware import geth_poa_middleware
import solcx 

def prtInfo(str):
    print(f"\033[0;37;42m[+] {str}\033[0m")

def prtWarn(str):
    print(f"\033[0;33;40m{str}\033[0m")

def prtDeug(str):
    print(f"\033[0;37;43m[-] {str}\033[0m")

def prtError(str):
    print(f"\033[0;31;40m[!] {str}\033[0m")


def compile_source_file(file_path, version='0.4.26'):
    if ('\n' not in file_path):
        with open(file_path, 'r') as f:
            source = f.read()
    else:
        source = file_path # input source code

    # return solcx.compile_source(source, output_values=["abi", "bin-runtime"],solc_version="0.7.0")
    solcx.install_solc(version)
    print(version)
    res = solcx.compile_source(source, output_values=["abi", "bin", "bin-runtime"], solc_version=version)
    # print(res)
    _name = list(res.keys())[0]
    return res[_name]


def evm2ewasm(evmHex, args):
    with open('.tmp.hex', 'w') as f:
        f.write(evmHex)
    cmd = "cat ./.tmp.hex | xxd -r -ps > .tmp.bin && ../build/evmtrans/standalone-evmtrans .tmp.bin -o res.wasm " + args.langArgs + " > /dev/null 2>&1"
    if 0 != os.system(cmd):
        # fail
        raise Exception("EVMTrans fall.")
    ewasmHex = os.popen(f"xxd -p res.wasm | tr -d $'\n'").read()
    # print("Evm2Ewasm Successed.", cmd)

    return ewasmHex


def deploy_contract(w3, owner, abi, bytecode, args=""):
    if args != "":
        _tx_hash = w3.eth.contract(abi=abi, bytecode=bytecode).constructor(args).transact({'from': owner, 'gas':300000000000})
    else:
        _tx_hash = w3.eth.contract(abi=abi, bytecode=bytecode).constructor().transact({'from': owner, 'gas':300000000000})
        
    address = w3.eth.wait_for_transaction_receipt(_tx_hash)['contractAddress'] 
    # w3.geth.miner.stop()
    # prtInfo(f'Deployed a contract to: {address}')
    return address

def activate_contract(w3, address, abi):
    return w3.eth.contract(address=address, abi=abi)


def runRPC():
    # os.system("rm /home/toor/evmTrans/testnet/ewasm-dev-env/geth/LOGS/*")    
    w3 = Web3(Web3.HTTPProvider("http://localhost:8545", request_kwargs={'timeout': 60}))
    w3.middleware_onion.inject(geth_poa_middleware, layer=0)
    _cnt = 0
    while w3.isConnected() != True:
        _cnt += 1
        if _cnt % 100 == 0:
            prtWarn(f'- @runEVM: activating web3... #{_cnt}')
    # assert w3.isConnected() == True, "[ERROR] Testnet is not activated."
    if len(w3.eth.accounts) < 2:
        w3.geth.personal.new_account("")
    for account in w3.eth.accounts:
        w3.geth.personal.unlock_account(account, "", 99999)
        
    return w3


def compile2bin():
    '''
    sys.argv[2]: solPath
    '''
    assert len(sys.argv) == 2, f"python {sys.argv[0]} <solPath>"

    solPath = sys.argv[1]
    cmrt = compile_source_file(solPath)
    with open(solPath + ".hex", 'w') as f:
        f.write('0x'+cmrt['bin'] if cmrt['bin'][:2] != '0x' else cmrt['bin'])
    
    with open(solPath + ".abi", 'w') as f:
        json.dump(cmrt['abi'], f)

    print("[+] ", solPath + ".hex", " ", solPath + ".abi")

def test():
    '''
    sys.argv[1]: evm|ewasm
    sys.argv[2]: solPath
    '''
    assert len(sys.argv) == 3, f"python {sys.argv[0]} <evm|ewasm> <solPath>"

    if sys.argv[1] == 'evm':
        w3 = runRPC("EVM")
    elif sys.argv[1] == 'ewasm':
        w3 = runRPC("EWASM")
    else:
        print("[-] ERROR parameter: evm|ewasm")
        exit(-1)

    solPath = sys.argv[2]
    cmrt = compile_source_file(solPath)
    with open(solPath + ".hex", 'w') as f:
        f.write(cmrt['bin-runtime'])
    '''
    sload.sol : 0x2dF470e2d3220EF3BA69764362BB1aE99e76c664
    store.sol: 0xdCD491f45e30d5566C756Ec9A8a5fD8f14e7A6a0
    add.sol : 0x6Da73761BCcc96dbE288406F0628B1e7b242306e
    storageAll.sol : 0x8986b767FFF23bf2Ff06CBcdD3662feeEcb91A42
    storageAll0.sol: 0x1E3A0807B3e4a73132a0ac54d6D290A4edC0e3CA
    storageNoArg.sol : 0x4Dc5738a9Db09eaF21046b37742cb95c04428690
    calldata.sol : 0x5e6b89a6aBD17a033180A9d39A0B284FC0596b80
    '''
    metadata = {
        'address' : "",
        'name' : "",
        'abi' : cmrt['abi'],
        'bytecode': cmrt['bin']
        }

    if metadata['address'] == "":
        address = deploy_contract(w3, w3.eth.accounts[0], metadata['abi'], metadata['bytecode'])
        print(f'[+] Deployed a contract to: {address}\n')
    else:
        address =  metadata['address']
    # print(metadata['bytecode'])
    instance = activate_contract(w3, address, metadata['abi'])
    
    # assert instance.bytecode == metadata['bytecode'], "Invalid Contract Address"
    # instance = w3.eth.contract(address=address, abi=metadata['abi'])


    print('storage[0] = ', w3.eth.getStorageAt(address, 0x0000000000000000000000000000000000000000000000000000000000000000))
    print('storage[0] = ', w3.eth.getStorageAt(address, 0x0000000000000000000000000000000000000000000000000000000000000001))
    # print("[-] before: get():", instance.caller().get())
    _tx_hash = instance.functions.set(0x12345678).transact({'from':w3.eth.accounts[0], 'gas':100000000000}) 
    w3.geth.miner.start()
    _receipt = w3.eth.wait_for_transaction_receipt(_tx_hash)
    w3.geth.miner.stop()
    # print('storage[0] = ', w3.eth.getStorageAt(address, 0x0000000000000000000000000000000000000000000000000000000000000000))
    print("[-] after: get():", instance.caller().get())
    exit()

    print('storage[0] = ', w3.eth.getStorageAt(address, 0))
    print("[-] before: after():", instance.caller().get())
    exit()

    # print("[-] before: get():", instance.caller().get(0)) # execute it at local only
    # print("[-] before: get():", instance.caller().get(1))
    exit()
    seed = random.randrange(1, 50)
    # print(f"[-] estimate gas = {instance.functions.get().estimateGas()}") # 2375932322900173726

    # _tx_hash = instance.functions.get().transact({'from':w3.eth.accounts[0]}) 
    # exit()
    # print("gas=", instance.functions.set(14).estimateGas())
    _tx_hash = instance.functions.get().transact({'from':w3.eth.accounts[0], 'gas':100000000000}) 
    _receipt = w3.eth.wait_for_transaction_receipt(_tx_hash)
    exit()
    print(_receipt)
    assert _receipt["status"] == 1, "The transaction was successful"

    print("[-] after: get():", instance.caller().get()) # execute it at local only
    exit()

    tx_hash = instance.functions.get().transact({'from': w3.eth.accounts[0]})
    receipt = w3.eth.wait_for_transaction_receipt(tx_hash)
    assert receipt["status"] == 1, "The transaction was successful"
    # gas_estimate = instance.functions.set(255).estimateGas()



# def main():
#     '''
#     sys.argv[1]: deploy|run
#     sys.argv[2]: solPath
#     '''
#     parser = argparse.ArgumentParser()
#     parser.add_argument("--act", help="", type=str, choices=['deploy', 'exec'], required=True)
#     parser.add_argument("--json", help="json input, incluing abi and bin", type=str)
#     parser.add_argument("--src", help="", type=str)
#     parser.add_argument("--srcs", help="", type=str)
#     args = parser.parse_args()

#     # assert len(sys.argv) == 3, f"python {sys.argv[0]} <deploy|run> --bin <bcDir> --sol <srcDir>"
#     if args.act == 'deploy':
#         runEVM()
#         if args.json:
#             with open(args.json, 'r') as f:
#                 _json = json.load(f)
#                 abi, bin = _json['abi'], _json['bin']

#             address = deploy_contract(w3, w3.eth.accounts[0], abi, bin)
            
#             with open(solPath+'.json', 'w') as f:
#                 cmrt['address'] = address
#                 json.dump(cmrt, f)
#             print(f'[+] Deployed a contract to: {address}\n')
#         os.system("pkill -INT geth") # exit process


#     w3 = runRPC()
    
#     solFiles = [os.path.join(sys.argv[2], item) for item in os.listdir(sys.argv[2]) if item.endswith('.sol')]
#     # switch your testing network
#     if sys.argv[1] == 'deploy':
#         for solPath in solFiles:
#             # deploy EVM bytecode             
#             print(solPath)
#             if "sha256.sol" not in solPath:
#                 continue
#             cmrt = compile_source_file(solPath)
#             with open(solPath + ".hex", 'w') as f:
#                 f.write(cmrt['bin-runtime'])
                
#             address = deploy_contract(w3, w3.eth.accounts[0], cmrt['abi'], cmrt['bin'])
#             with open(solPath+'.json', 'w') as f:
#                 cmrt['address'] = address
#                 json.dump(cmrt, f)
#             print(f'[+] Deployed a contract to: {address}\n')
#         os.system("pkill -INT geth") # exit process

#     elif sys.argv[1] == 'run':
#         for solPath in solFiles:
#             if "sha256.sol" not in solPath:
#                 continue
#             with open(solPath+'.json', 'r') as f:
#                 cmrt = json.load(f)
#             print("[+] Run :", solPath)
#             instance = activate_contract(w3, cmrt['address'], cmrt['abi'])
#             res = instance.caller().call()
#             print(res)

#     else:
#         print("[-] ERROR parameter: deploy|run")
#         exit(-1)


def cmdLine():
    parser = argparse.ArgumentParser()
    parser.add_argument("--abi", help="", type=str, required=False)
    parser.add_argument("--bin", help="", type=str, required=False)
    parser.add_argument("--debug", help="print debug messages to stderr", action='store_true', required=False)
    parser.add_argument("--overwrite", help="overwrite history data", action='store_true', required=False)
    parser.add_argument("--src", help="", type=str)
    parser.add_argument("--srcs", help="", type=str)
    parser.add_argument("--langArgs", help="arguments for the translator", required=False, type=str, default="")
    return parser.parse_args()

def main():
    parser = argparse.ArgumentParser()
    # parser.add_argument("--act", help="", type=str, choices=['deploy', 'exec'], required=True)
    parser.add_argument("--abi", help="", type=str, required=False)
    parser.add_argument("--bin", help="", type=str, required=False)
    parser.add_argument("--debug", help="print debug messages to stderr", action='store_true', required=False)
    parser.add_argument("--overwrite", help="overwrite history data", action='store_true', required=False)
    # TODO
    parser.add_argument("--src", help="", type=str)
    parser.add_argument("--srcs", help="", type=str)
    args = parser.parse_args()

    global DEBUG
    DEBUG = args.debug
    
    if args.abi:
        with open(args.abi, 'r') as f:
            abi = json.load(f)
    if args.bin:
        with open(args.bin, 'r') as f:
            # bin = f.read()
            _f = f.read().strip()
            bin = _f if _f[:2] == "0x" else "0x"+_f

    w3 = runRPC("EWASM")
    

    if os.path.exists(args.bin + '.json') and not args.overwrite:
        with open(args.bin + '.json', 'r') as f:
            address = json.load(f)['addr']

    else:
        address = deploy_contract(w3, w3.eth.accounts[0], abi, bin)
        with open(args.bin + '.json', 'w') as f:
            json.dump({'abi':abi, 'bin':bin, 'addr':address}, f)


    instance = activate_contract(w3, address, abi)
    with open('./t.wasm', 'wb') as f:
        f.write(w3.eth.getCode(address))
    

    # prtInfo(f'storage[0] = {w3.eth.getStorageAt(address, 0x0000000000000000000000000000000000000000000000000000000000000000)}')
    # exit()
    
    # _tx_hash = instance.functions.set(0xfffffffffffffffff8fe5684eaf30e7c8957a6fe7a19dace9d900000).transact({'from':w3.eth.accounts[0], 'gas':100000}) 
    # _receipt = w3.eth.wait_for_transaction_receipt(_tx_hash)
    # prtInfo(f'storage[0] = {w3.eth.getStorageAt(address, 0x0000000000000000000000000000000000000000000000000000000000000000)}')
    # usedGas = _receipt['gasUsed']
    # prtInfo(f"UsedGas = {usedGas} wei")

    
    # for storage.sol
    # prtInfo(f'storage[0] = {w3.eth.getStorageAt(address, 0x0000000000000000000000000000000000000000000000000000000000000000)}')
    # prtInfo(f'storage[1] = {w3.eth.getStorageAt(address, 0x0000000000000000000000000000000000000000000000000000000000000001)}')
    # res = instance.caller().get()
    # prtInfo(res)
    # exit()
    # _tx_hash = instance.functions.set(0x10).transact({'from':w3.eth.accounts[0], 'gas':100000}) 
    # _receipt = w3.eth.wait_for_transaction_receipt(_tx_hash)

    # prtInfo(f'storage[1] = {w3.eth.getStorageAt(address, 0x0000000000000000000000000000000000000000000000000000000000000001)}')
    '''
    prtInfo(f'storage[0] = {w3.eth.getStorageAt(address, 0x0000000000000000000000000000000000000000000000000000000000000000)}')

    _tx_hash = instance.functions.set(0x123).transact({'from':w3.eth.accounts[0], 'gas':100000}) 
    _receipt = w3.eth.wait_for_transaction_receipt(_tx_hash)
    usedGas = _receipt['gasUsed']
    prtInfo(f"UsedGas = {usedGas} wei")
    # exit()
    res = instance.caller().get()
    prtInfo(res)
    prtInfo(f'storage[0] = {w3.eth.getStorageAt(address, 0x0000000000000000000000000000000000000000000000000000000000000000)}')
    exit()
    '''
    

    # prtInfo(f'storage[0] = {w3.eth.getStorageAt(address, 0xc69f6229134b2a0996db801892de179be1d93b798f4f09f579fae6d4a8a4ac1a)}')
    # 98000000
    # c69f6229134b2a0996db801892de179be1d93b798f4f09f579fae6d4a8a4ac1a  1000
    # exit()
    # res = instance.caller().get()
    # prtInfo(res)
    # exit()
    # res = w3.eth.get_balance(w3.eth.accounts[0])
    # prtInfo(f"Balance@{w3.eth.accounts[0]}={res}")

    res = instance.caller().balanceOf(w3.eth.accounts[0])
    prtInfo(f"Balance@{w3.eth.accounts[0]}={res}")
    res = instance.caller().balanceOf(w3.eth.accounts[1])
    prtInfo(f"Balance@{w3.eth.accounts[1]}={res}")
    # exit()
    prtInfo("------------------ transfing --------------------- ")
    _tx_hash = instance.functions.transfer(w3.eth.accounts[1], 1000).transact({'from':w3.eth.accounts[0], 'gas':100000}) 
    _receipt = w3.eth.wait_for_transaction_receipt(_tx_hash)
    usedGas = _receipt['gasUsed']
    prtInfo(f"UsedGas = {usedGas} wei")
    prtInfo(f'storage[0] = {w3.eth.getStorageAt(address, 0x0000000000000000000000000000000000000000000000000000000000000000)}')
  
    res = instance.caller().balanceOf(w3.eth.accounts[0])
    prtInfo(f"Balance@{w3.eth.accounts[0]}={res}")

    res = instance.caller().balanceOf(w3.eth.accounts[1])
    prtInfo(f"Balance@{w3.eth.accounts[1]}={res}")
    exit()
    # with open(args.bin + ''.join(args.bin.split('/')[-1].split('.')[:-1])+'.json', 'w') as f:
    
    del w3
    w3 = runRPC("EWASM")

    instance = activate_contract(w3, address, abi)
    # res = instance.caller().call()
    res = instance.caller().get()
    prtInfo(f"EWASM: {res}")

# main()

# author cswmchen
# date   1/12/2021
# usage  PASS: python -m runtimeTest.arithmetic --overwrite --langArgs="--use-safeMath"
import json
import os
from utils import prtError, runRPC, cmdLine, deploy_contract, activate_contract, prtInfo, prtDeug, compile_source_file, evm2ewasm

def main():
    args = cmdLine()
    w3 = runRPC()
    solPath = "./contracts/arithmetic.sol"

    metadataPath = solPath[:-4] + '.json'
    if os.path.exists(metadataPath) and not args.overwrite:
        with open(metadataPath, 'r') as f:
            obj = json.load(f)
            address = obj['addr']
            contractABI = obj['abi']
    else:
        cmrt = compile_source_file(solPath, version='0.4.26')
        contractHex = '0x'+cmrt['bin'] if cmrt['bin'][:2] != '0x' else cmrt['bin']
        contractABI =  cmrt['abi']
        contractEwasm = evm2ewasm(contractHex, args)

        address = deploy_contract(w3, w3.eth.accounts[0], contractABI, contractEwasm)
        with open(metadataPath, 'w') as f:
            json.dump({'abi':contractABI, 'hex':contractHex, 'bin':contractEwasm, 'addr':address}, f)

    instance = activate_contract(w3, address, contractABI)

    # validity test
    assert instance.caller().subu256(1, 1) == 0
    assert instance.caller().subu256(2**256-1, 2**256-2) == 1

    assert instance.caller().addu256(2**256-2, 1) == 2**256-1
    assert instance.caller().mulu256(2**255-1, 2) == 2**256-2
    assert instance.caller().divu256(2**255, 2) == 2**254
    assert instance.caller().divs256(-8, 2) == -4
    assert instance.caller().divs256(8, -2) == -4
    # print(instance.caller().divu256(2**255, 2), 2**254)

    # assert instance.caller().subs256(2**255-1, 1) == 2**255 - 2
    # print(instance.caller().subs256(0, 1))
    # assert instance.caller().subs256(0, 1) == -1



    # prtDeug(f'{instance.caller().subs256(-2**255, 1)} :: signed overflow')
    # assert instance.caller().subu256(0, 1) == 2**256 - 1, "uint256 should overflow"
    # assert instance.caller().sub8(-128, 1) == 127, "int8 should overflow"
    
    # try:
    #     _ = instance.caller().subu256(0, 1)
    #     prtError(f"overflow sub@{_}")
    # except:
    #     prtInfo("Safe uint256 sub")
    
    # try:
    #     _ = instance.caller().addu256(2**256-1, 1)
    #     prtError(f"overflow add@{_}")
    # except:
    #     prtInfo("Safe uint256 add")

    # try:
    #     _ = instance.caller().mulu256(2**255, 2)
    #     prtError(f"overflow mul@{_}")
    # except:
    #     prtInfo("Safe uint256 mul")

    # try:
    #     _ = instance.caller().divu256(1, 0)
    #     prtError(f"invalid {_}")
    # except:
    #     prtInfo("Safe uint256 div")


main()
prtInfo("PASSED")

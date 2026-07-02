# Test compile effectiveness


# Test samples in Ewasm runtime
1. activate the Ewasm node
*/home/toor/evmTrans/testnet/ewasm-dev-env/geth*
'''
$ ./go-ethereum/build/bin/geth  --mine --miner.threads 8 --networkid 666 --nodiscover --vmodule "rpc=12" --datadir ./gas-wasm/  --rpc --rpcaddr '127.0.0.1' --rpcport 8545 --rpccorsdomain '*' --ws --wsaddr '0.0.0.0' --wsorigins '*'   --allow-insecure-unlock --vm.ewasm="../hera/build/src/libhera.so,metering=false"  --debug --verbosity 5 --targetgaslimit '9000000000000'  --rpcapi="db,eth,net,web3,personal,web3,miner" --gasprice "100"  --etherbase C7A5251CA88BeD637f25Aa6760f6b552fb1cF8EB console
'''
2. run your instance, e.g., soll.sol
'''
python -m runtimeTest.soll --overwrite
'''
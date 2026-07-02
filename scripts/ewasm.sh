#! /bin/bash

grep_port=`netstat -tlpn 2>/dev/null | grep "\b8545\b"`
if [ -n "$grep_port" ]
then
    echo "tcp 0.0.0.0:8545: bind: address already in use"
    exit 1
else
#! /bin/bash
    set -e
    docker run --rm  --network host -ti --privileged -v $PWD/LOGS:/FIXME --name ewasmenv  localhost/client-go:ewasm \
        --networkid 66 \
        --port 30321 \
        --rpc --rpcaddr "0.0.0.0" \
        --rpcport 8545 \
        --rpccorsdomain "*" \
        --allow-insecure-unlock \
        --vm.ewasm="/root/libhera.so,evm1mode=reject" \
        --rpcapi="db,eth,net,web3,personal,web3,miner" \
        --miner.gasprice=0 \
        --miner.gaslimit 90000000000 \
        --mine \
        --miner.threads 4 \
        --dev 

    #docker logs --follow $(./scripts/run_geth.sh)
fi
 

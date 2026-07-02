#!/bin/bash

# arg1: command (build or clean)
# arg2: benchmark
# arg3: compiler version

set -e

function download_solc() {
    wget https://github.com/ethereum/solidity/releases/download/v$1/solc-static-linux
    mv ./solc-static-linux ./solc
    chmod +x ./solc
}

function build_evm() {
    mkdir -p $1/abi/
    mkdir -p $1/bin/
    mkdir -p tmp/
    download_solc $2
    while IFS=, read -r file name
    do  
        # echo "./solc --abi -o tmp/ $1/sol/$file.sol "
        # echo "cp tmp/$name.abi $1/abi/$file.abi -r"
        # Compile ABI
        ./solc --abi -o tmp/ $1/sol/$file.sol --overwrite
        cp tmp/$name.abi $1/abi/$file.abi -r
        rm -rf tmp/*
        # Compile bytecode
        ./solc --bin -o tmp/ $1/sol/$file.sol --overwrite
        cp tmp/$name.bin $1/bin/$file.bin
        rm -rf tmp/*
    done < ./assets/$1.list
    rm -rf tmp/ ./solc 
}

function build_tool() {
    mkdir -p $1/tool-bin/
    mkdir -p tmp/
    while IFS=, read -r file name
    do  
        # Compile ewasm from the EVM bin
        cat $1/bin/$file.bin | xxd -r -ps > tmp/.tmp.bin
        ../../repo/build/evmtrans/standalone-evmtrans tmp/.tmp.bin -o tmp/res.wasm
        xxd -p tmp/res.wasm | tr -d $'\n' > $1/tool-bin/$file.bin
        rm -rf tmp/*
    done < ./assets/$1.list
    rm -rf tmp/
}

function build_solang() {
    mkdir -p $1/solang-bin/
    mkdir -p tmp/
    while IFS=, read -r file name
    do  
        # Compile ewasm from the EVM bin
        cp $1/sol/$file.sol tmp/tt.sol
        # echo "docker run --rm -it -v $(pwd)/tmp:/sources ghcr.io/hyperledger-labs/solang -v -o /sources --target ewasm /sources/tt.sol"
        docker run --rm  -v $(pwd)/tmp:/sources ghcr.io/hyperledger-labs/solang -v -o /sources --target ewasm /sources/tt.sol
        xxd -p tmp/$name.wasm | tr -d $'\n' > $1/solang-bin/$file.bin
        rm -rf tmp/*
    done < ./assets/$1.list
    rm -rf tmp/
}

function download_soll() {
    # docker pull secondstate/soll
    # docker stop soll-container && docker rm soll-container
    git clone --recursive https://github.com/second-state/soll.git tmp/
    docker run -d -t --name soll-container -v $(pwd)/tmp:/root/soll secondstate/soll
    docker exec -t soll-container bash -c "mkdir -p /build && cd /build && cmake /root/soll && make -j8"
}

function build_soll() {
    download_soll
    mkdir -p $1/soll-bin/
    mkdir -p tmp/
    while IFS=, read -r file name
    do  
        # Compile ewasm from the EVM bin
        cp $1/sol/$file.sol tmp/$file.sol
        docker exec -t soll-container /build/tools/soll/soll /root/soll/$file.sol
        xxd -p tmp/$name.wasm | tr -d $'\n' > $1/soll-bin/$file.bin
    done < ./assets/$1.list
    rm -rf tmp/
}

function build_evm2wasm() {
    mkdir -p $1/evm2wasm-bin/
    mkdir -p tmp/
    while IFS=, read -r file name
    do  
        # Compile ewasm from the EVM bin
        /home/kenun/ethsema/evm2wasm/bin/evm2wasm.js -e $1/bin/$file.bin -o tmp/$file.wasm
        xxd -p tmp/$file.wasm | tr -d $'\n' > $1/evm2wasm-bin/$file.bin
    done < ./assets/$1.list
    rm -rf tmp/
}

if [ "$#" -ne 2 ]; then
    echo "Usage:"
    echo "  ./build.sh <benchmark> <solc version|compiler>"
else
    if [ "$2" == 'ethsema' ]; then
        build_tool $1
        echo "Ethsema built eWasm contracts at $1/tool-bin"
    elif [ "$2" == 'solang' ]; then
        build_solang $1
        echo "Solang built eWasm contracts at $1/solang-bin"
    elif [ "$2" == 'soll' ]; then
        build_soll $1
        echo "SOLL built eWasm contracts at $1/soll-bin"
    elif [ "$2" == 'evm2wasm' ]; then
        build_evm2wasm $1
        echo "evm2wasm built eWasm contracts at $1/evm2wasm-bin"
    else 
        build_evm $1 $2
        echo "solc-v$2 built eWasm contracts at $1/bin/"
    fi
fi

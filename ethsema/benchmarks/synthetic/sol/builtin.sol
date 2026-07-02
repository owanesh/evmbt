// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// RUN: %soll %s
pragma solidity ^0.4.0;

contract BUILTIN {

  function call() public view returns(uint) {
    //address msgSender;
    //uint msgValue;
    //bytes memory msgData;
    uint txGasPrice;
    address txOrigin;
    address bkCoinbase;
    uint256 bkDifficulty;
    uint bkGasLimit;
    uint bkBlockNumber;
    uint bkBlockTimestamp = 0;
    uint i;
    for(i = uint(0); i < 10000; i += 1) {
        //msgSender = msg.sender;
        //msgValue = msg.value;
        //msgData = msg.data;
        txGasPrice = tx.gasprice;
        txOrigin = tx.origin;
        bkCoinbase = block.coinbase;
        bkDifficulty = block.difficulty;
        bkGasLimit = block.gaslimit;
        bkBlockNumber = block.number;
        bkBlockTimestamp = block.timestamp;
    }
    return bkBlockTimestamp;
  }
}


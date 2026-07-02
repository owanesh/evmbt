pragma solidity ^0.4.23;

contract Lottery {
    uint256 public pool = 0;
    
    function rand() private returns(uint256) {
        uint randNonce = 0;
        uint random = uint(keccak256(now, msg.sender, randNonce));
        randNonce++;
        uint random2 = uint(keccak256(now, msg.sender, randNonce));
        return random2 % 10;
    }
    
    function win(uint256 bid) public payable{
        require (msg.value >= 1 ether);
        pool += msg.value;
        if (bid == rand()){
             msg.sender.transfer(pool);
             pool = 0;
        }
    }
}
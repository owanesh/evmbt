// RUN: %soll %s
// pragma solidity >0.4.0 <=0.7.0;

contract KECCAK256 {
    function call() public pure returns(uint) {
		uint rtn = uint(1);
        bytes32 hash;
        uint i = 0;
        for(i = 0; i < 10000; i += 1) {
            hash = keccak256("HelloWorld");
		}
		return rtn;
	}
}



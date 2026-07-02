// RUN: %soll %s
pragma solidity >0.4.0 <=0.7.0;

contract SHA256 {
    function call() public pure returns(uint) {
		uint rtn = uint(1);
        bytes32 hash;
        for(uint i = uint(0); i < 10000; i += 1) {
            hash = sha256("HelloWorld");
		}
		return rtn;
	}
}
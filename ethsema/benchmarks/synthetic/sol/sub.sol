// RUN: %soll %s
pragma solidity >0.4.0 <=0.7.0;

contract SUB {
    function call() public pure returns(uint) {
		uint rtn = uint(300000);
        for(uint i = uint(0); i < 10000; i += 1) {
            rtn = rtn - 3;
		}
		return rtn;
	}
}



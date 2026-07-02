// RUN: %soll %s
pragma solidity >0.4.0 <=0.7.0;

contract SHL {
    function call() public pure returns(uint) {
		uint rtn = uint(1);
        for(uint i = uint(0); i < 10000; i += 1) {
            rtn = rtn << 1;
		}
		return rtn;
	}
}
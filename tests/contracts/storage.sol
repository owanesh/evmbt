// pragma solidity >0.4.0 <=0.7.0;

contract ADD {
    uint a = 12;
	// function ADD(uint _a){
	// 	a = _a;
	// }
	string public hashstring = "1231231231";
    function get() public view returns(uint) {
		return a;
	}
	function set(uint256 _a) public {
	    a += _a;
	}
	function setHash(string memory hash) public {
		hashstring = hash;
	}
}

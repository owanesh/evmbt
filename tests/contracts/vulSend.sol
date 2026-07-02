contract Token {
    uint256 public a = 0;
    function test() public payable {
        msg.sender.send(1 wei);
        a = 10;
    }   
}
contract BallotTest {
   
    function test() public view returns(uint, bytes32, uint, uint, uint) {
        uint _previousBlockNumber;
        bytes32 _previousBlockHash;
        _previousBlockNumber = uint(block.number);
        _previousBlockHash = bytes32(blockhash(_previousBlockNumber));
        uint _bal;
        assembly{
            _bal := selfbalance()
        }
        return (_previousBlockNumber, _previousBlockHash, block.chainid, block.basefee, _bal);
    }
}

contract Transfer {
    
   function test(address to) public payable{
       to.transfer(msg.value);
   }
}

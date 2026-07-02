contract Math {
   
    function subu256(uint a, uint b) public view returns(uint ) {
        return a - b;
    }
    function subu8(uint8 a, uint8 b) public view returns(uint8 ) {
        return a - b;
    }

    function subs256(int256 a, int256 b) public view returns(int256 ) {
        return a - b;
    }

    function subs8(int8 a, int8 b) public view returns(int8 ) {
        return a - b;
    }

    function addu256(uint a, uint b) public view returns(uint ) {
        return a + b;
    }
    function adds256(int256 a, int256 b) public view returns(int256 ) {
        return a + b;
    }

    function divu256(uint a, uint b) public view returns(uint ) {
        return a / b;
    }
    function divs256(int256 a, int256 b) public view returns(int256 ) {
        return a / b;
    }

    function mulu256(uint a, uint b) public view returns(uint ) {
        return a * b;
    }
    function muls256(int256 a, int256 b) public view returns(int256 ) {
        return a * b;
    }
}


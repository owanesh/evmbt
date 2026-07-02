contract SolaStars
{
	string public provenance_hash = "123";
	function setProvenanceHash(string memory _provenance_hash) public
	{
		provenance_hash = _provenance_hash;
	}
}
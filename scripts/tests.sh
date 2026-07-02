cd ./tests
python -m runtimeTest.arithmetic --overwrite 
python -m runtimeTest.soll  --overwrite
python -m runtimeTest.constructorArgs  --overwrite
python -m runtimeTest.crossContract  --overwrite
python -m runtimeTest.eei   
python -m runtimeTest.reentrancy  --overwrite
python -m runtimeTest.returnStruct  --overwrite
python -m runtimeTest.rollback  --overwrite
python -m runtimeTest.send  --overwrite
python -m runtimeTest.storage  --overwrite
python -m runtimeTest.string  --overwrite
python -m runtimeTest.transfer --overwrite
# taste-cfdp

Re-usable TASTE component providing CFDP (CCSDS File Delivery Protocol) Class 1 functionality, developed as a part of "Model-Based Execution Platform for Space Applications" project (contract 4000146882/24/NL/KK) financed by the European Space Agency.

It wraps [N7S OBCP](https://github.com/n7space/n7s-obcp) in a TASTE component. The component itself is located in the **component/cfdp** directory. A demonstration project is provided in **test-project/taste-cfdp** directory. Basic tests can be executed by switching to the **test-project/taste-cfdp** directory and invoking:
```
make test
``` 
Test execution requires python3 with **cfdp** (tested with version 0.2.0) library. Example output is:
```
rm -f sent_target_file.txt
rm -f received_target_file.txt
chmod +x python_cfdp_receiver.py
python3 python_cfdp_receiver.py &
./work/binaries/demo
[TASTE] Initialization completed for function test_function
[TASTE] Initialization completed for function taste_env
[TASTE] Initialization completed for function cfdp
cfdp indication type=0 source_entity_id = 6 seq_number = 1
socket bytes sent 50
socket bytes sent 31
socket bytes sent 17
cfdp indication type=1 source_entity_id = 6 seq_number = 1
cfdp indication type=2 source_entity_id = 6 seq_number = 1
SEND FILE TEST PASSED
./work/binaries/demo &
[TASTE] Initialization completed for function test_function
[TASTE] Initialization completed for function taste_env
[TASTE] Initialization completed for function cfdp
make: [Makefile:46: test-file-receive] Error 1 (ignored)
chmod +x python_cfdp_sender.py
python3 python_cfdp_sender.py
cfdp indication type=5 source_entity_id = 5 seq_number = 1
cfdp indication type=6 source_entity_id = 5 seq_number = 1
cfdp indication type=4 source_entity_id = 5 seq_number = 1
cfdp indication type=2 source_entity_id = 5 seq_number = 1
RECEIVE FILE TEST PASSED
```
# Test for SAM MCUs with MemoryAccess and FileSystem components

- build project using `make samv71 debug`
- ssh into remote machine and start gdb-server
- on local machine, start `gdb-multiarch work/binaries/partition_1`, connect to the remote machine, load and start program
- on remote machine in second session start socat `socat -x tcp-l:5006,reuseaddr,fork /dev/ttyUSB0,raw,echo=0,b38400`
- on local machine in separate session run socat `socat pty,link=/tmp/samv71,raw,echo=0 tcp:<remote ip>:5006`
- on local machine in separate session run second binary `work/binaries/partition_2`
- in separate session prepare python environment:
  - run `python3 -m venv env`
  - run `source env/bin/activate`
  - run `pip install cfdp`
- in the session with python environment run script `python3 script.py`
- after about 15 seconds the script shall finish without an error (exitcode is 0) and message `SUCCESS`

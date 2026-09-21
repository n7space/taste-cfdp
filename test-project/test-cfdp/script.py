import os
import cfdp
import time
import pathlib
from cfdp.transport.udp import UdpTransport
from cfdp.filestore import NativeFileStore


def prepare_environment():
    if not os.path.isdir('files'):
        os.mkdir('files')
    if not os.path.isfile('files/abc'):
        with open('files/abc', 'w') as f:
            f.write('def\n')
    if os.path.isfile('files/cba'):
        pathlib.Path('files/cba').unlink()
    if os.path.isfile('files/test1'):
        pathlib.Path('files/test1').unlink()

def run_cfdp_scenario():
    udp_transport = UdpTransport(routing={"*": [("127.0.0.1", 5111)]})
    udp_transport.bind("127.0.0.1", 5222)

    cfdp_entity = cfdp.CfdpEntity(entity_id=5, filestore=NativeFileStore("./files"), transport=udp_transport)

    transaction_id = cfdp_entity.put(
        destination_id=6,
        source_filename="abc",
        destination_filename="abc",
        transmission_mode=cfdp.TransmissionMode.UNACKNOWLEDGED,
    )

    print("Waiting for transaction complete")
    while not cfdp_entity.is_complete(transaction_id):
        time.sleep(0.1)
    print("Finished")

    print("Waiting for receiving")
    time.sleep(15)

    cfdp_entity.shutdown()
    udp_transport.unbind()


def verify_results():
    os.path.isfile("./files/cba")
    with open("./files/cba") as file:
        lines = file.readlines()
        assert lines == ["def\n"]
    os.path.isfile("./files/test1")
    with open("./files/test1") as file:
        lines = file.readlines()
        assert lines == ["Jupiter"]
    print("SUCCESS")

def main():
    prepare_environment()
    run_cfdp_scenario()
    verify_results()


if __name__ == '__main__':
    main()

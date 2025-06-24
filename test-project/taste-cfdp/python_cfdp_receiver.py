import logging

import cfdp
from time import sleep
from cfdp.transport.udp import UdpTransport
from cfdp.filestore import NativeFileStore

udp_transport = UdpTransport(routing={"*": [("127.0.0.1", 5111)]})
udp_transport.bind("127.0.0.1", 5222)

cfdp_entity = cfdp.CfdpEntity(
    entity_id=13, filestore=NativeFileStore("."), transport=udp_transport
)

sleep(3)

cfdp_entity.shutdown()
udp_transport.unbind()
#ifndef TRANSPORT_H
#define TRANSPORT_H

#include "stdbool.h"
#include "stdint.h"

struct transport {
	void (*transport_send_pdu)(const byte pdu[], const int size);
	bool (*transport_is_ready)();
};

#endif
#ifndef TRANSPORT_H
#define TRANSPORT_H

#include "stdbool.h"
#include "stdint.h"

struct transport {

	void *transport_data;

	bool (*transport_send_pdu)(void *transport_data, const byte pdu[],
				   const int size);
	bool (*transport_is_ready)(void *transport_data);
};

#endif
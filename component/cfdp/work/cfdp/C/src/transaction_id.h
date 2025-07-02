#ifndef TRANSACTION_ID_H
#define TRANSACTION_ID_H

#include "stdint.h"

struct transaction_id {
	uint32_t source_entity_id;
	uint32_t seq_number;
};

#endif
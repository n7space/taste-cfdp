#ifndef RECEIVER_MACHINE_H
#define RECEIVER_MACHINE_H

#include "stdbool.h"
#include "stdint.h"

#include "constants.h"
#include "cfdp_dataview.h"
#include "receiver_timer.h"
#include "transaction.h"
#include "transaction_id.h"

struct event;

struct receiver_machine {
	struct cfdp_core *core;
	struct transaction_id transaction_id;
	struct transaction transaction;
	struct receiver_timer timer;

	cfdpConditionCode condition_code;
	cfdpDeliveryCode delivery_code;

	uint32_t received_file_size;

	enum MachineState state;
};

void receiver_machine_init(struct receiver_machine *receiver_machine,
			   struct transaction transaction);
void receiver_machine_close(struct receiver_machine *receiver_machine);

void receiver_machine_update_state(struct receiver_machine *receiver_machine,
				   struct event *event, const cfdpCfdpPDU *pdu);

#endif
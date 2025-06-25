#ifndef CFDP_CORE_H
#define CFDP_CORE_H

#include "stdbool.h"
#include "stdint.h"
#include "string.h"

#include "constants.h"
#include "event.h"
#include "filestore.h"
#include "receiver_machine.h"
#include "sender_machine.h"
#include "transaction_id.h"
#include "transport.h"

struct cfdp_core {
	uint32_t entity_id;
	struct filestore_cfg *filestore;
	struct transport *transport;

	uint32_t transaction_sequence_number;
	struct sender_machine sender[MAX_NUMBER_OF_SENDER_MACHINES];
	struct receiver_machine receiver[MAX_NUMBER_OF_RECEIVER_MACHINES];
	enum ChecksumType checksum_type;
	uint32_t inactivity_timeout;

	void (*cfdp_core_indication_callback)(
	    struct cfdp_core *core, const enum IndicationType indication_type,
	    const struct transaction_id transaction_id);
	void (*cfdp_core_error_callback)(struct cfdp_core *core,
					 const enum ErrorType error_type,
					 const uint32_t error_code);
};

void cfdp_core_init(struct cfdp_core *core, struct filestore_cfg *filestore,
		    struct transport *transport, const uint32_t entity_id,
		    const enum ChecksumType checksum_type,
		    const uint32_t inactivity_timeout);

void cfdp_core_issue_request(struct cfdp_core *core,
			     struct transaction_id transaction_id,
			     enum EventType event_type);

// CFDP service requests

struct transaction_id
cfdp_core_put(struct cfdp_core *core, uint32_t destination_entity_id,
	      char *source_filename,
	      char *destination_filename);

void cfdp_core_cancel(struct cfdp_core *core,
		      struct transaction_id transaction_id);

void cfdp_core_suspend(struct cfdp_core *core,
		       struct transaction_id transaction_id);

void cfdp_core_resume(struct cfdp_core *core,
		      struct transaction_id transaction_id);

void cfdp_core_report(struct cfdp_core *core,
		      struct transaction_id transaction_id);

// CFDP indications

void cfdp_core_transaction_indication(struct cfdp_core *core,
				      struct transaction_id transaction_id);

void cfdp_core_eof_sent_indication(struct cfdp_core *core,
				   struct transaction_id transaction_id);

void cfdp_core_finished_indication(struct cfdp_core *core,
				   struct transaction_id transaction_id);

void cfdp_core_report_indication(struct cfdp_core *core,
				 struct transaction_id transaction_id);

void cfdp_core_eof_received_indication(struct cfdp_core *core,
				       struct transaction_id transaction_id);

void cfdp_core_metadata_received_indication(
    struct cfdp_core *core, struct transaction_id transaction_id);

void cfdp_core_filesegment_received_indication(
    struct cfdp_core *core, struct transaction_id transaction_id);

void cfdp_core_abandoned_indication(struct cfdp_core *core,
				    struct transaction_id transaction_id);

void cfdp_core_suspended_indication(struct cfdp_core *core,
				    struct transaction_id transaction_id);

void cfdp_core_resumed_indication(struct cfdp_core *core,
				  struct transaction_id transaction_id);

void cfdp_core_fault_indication(struct cfdp_core *core,
				struct transaction_id transaction_id);

// CFDP link state procedures

void cfdp_core_freeze(struct cfdp_core *core, uint32_t destination_entity_id);

void cfdp_core_thaw(struct cfdp_core *core, uint32_t destination_entity_id);

void cfdp_core_received_pdu(struct cfdp_core *core, unsigned char *buf,
			    long count);

void cfdp_core_run_fault_handler(struct cfdp_core *core,
				 const struct transaction_id transaction_id,
				 const enum FaultHandlerAction action);

bool cfdp_core_is_done(struct cfdp_core *core);

void cfdp_core_transport_is_ready_callback(struct cfdp_core *core);

#endif
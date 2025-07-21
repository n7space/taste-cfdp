#include "cfdp_core.h"
#include "event.h"
#include "receiver_machine.h"
#include <assert.h>

void receiver_machine_init(struct receiver_machine *receiver_machine,
			   struct transaction transaction)
{
	receiver_machine->transaction = transaction;
	receiver_machine->transaction_id.source_entity_id =
	    transaction.source_entity_id;
	receiver_machine->transaction_id.seq_number = transaction.seq_number;

	receiver_machine->timer.core = receiver_machine->core;
	receiver_machine->timer.transaction_id =
	    receiver_machine->transaction_id;

	receiver_machine->condition_code = cfdpConditionCode_no_error;
	receiver_machine->delivery_code = cfdpDeliveryCode_data_incomplete;

	receiver_machine->received_file_size = 0;
}

void receiver_machine_close(struct receiver_machine *receiver_machine)
{
	receiver_machine->state = COMPLETED;
}

void receiver_machine_update_state(struct receiver_machine *receiver_machine,
				   struct event *event, const cfdpCfdpPDU *pdu)
{
	receiver_timer_restart(&receiver_machine->timer);

	if (receiver_machine->state == WAIT_FOR_MD) {
		switch (event->type) {
		case E0_ENTERED_STATE: {
			receiver_machine_init(receiver_machine,
					      event->transaction);
			receiver_timer_restart(&receiver_machine->timer);
			break;
		}
		case E2_ABANDON_TRANSACTION: {
			cfdp_core_abandoned_indication(
			    receiver_machine->core,
			    receiver_machine->transaction_id);
			receiver_machine_close(receiver_machine);
			break;
		}
		case E3_NOTICE_OF_CANCELLATION: {
			cfdp_core_finished_indication(
			    receiver_machine->core,
			    receiver_machine->transaction_id);
			receiver_machine_close(receiver_machine);
			break;
		}
		case E10_RECEIVED_METADATA: {
			cfdp_core_metadata_received_indication(
			    receiver_machine->core,
			    receiver_machine->transaction_id);
			if (!transaction_process_messages_to_user(
				&receiver_machine->transaction)) {
				receiver_machine_close(receiver_machine);
				return;
			}
			receiver_machine->state = WAIT_FOR_EOF;
			break;
		}
		case E12_RECEIVED_EOF_NO_ERROR: {
			cfdp_core_finished_indication(
			    receiver_machine->core,
			    receiver_machine->transaction_id);
			receiver_machine_close(receiver_machine);
			break;
		}
		case E13_RECEIVED_EOF_CANCEL: {
			assert(pdu != NULL);
			receiver_machine->condition_code =
			    pdu->payload.u.file_directive.file_directive_pdu.u
				.eof_pdu.condition_code;

			cfdp_core_finished_indication(
			    receiver_machine->core,
			    receiver_machine->transaction_id);
			receiver_machine_close(receiver_machine);
			break;
		}
		case E27_INACTIVITY_TIMEOUT: {
			cfdp_core_run_fault_handler(
			    receiver_machine->core,
			    receiver_machine->transaction_id,
			    DEFAULT_FAULT_HANDLER_ACTIONS
				[cfdpConditionCode_inactivity_detected]);
			receiver_timer_restart(&receiver_machine->timer);
			break;
		}
		case E33_RECEIVED_CANCEL_REQUEST: {
			receiver_machine->condition_code =
			    cfdpConditionCode_cancel_request_received;
			cfdp_core_issue_request(
			    receiver_machine->core,
			    receiver_machine->transaction_id,
			    E3_NOTICE_OF_CANCELLATION);
			break;
		}
		case E34_RECEIVED_REPORT_REQUEST: {
			cfdp_core_report_indication(
			    receiver_machine->core,
			    receiver_machine->transaction_id);
			break;
		}
		default: {
			cfdp_core_issue_error(receiver_machine->core,
					      UNSUPPORTED_ACTION, 0);
		}
		}
	} else if (receiver_machine->state == WAIT_FOR_EOF) {
		switch (event->type) {
		case E2_ABANDON_TRANSACTION: {
			cfdp_core_abandoned_indication(
			    receiver_machine->core,
			    receiver_machine->transaction_id);
			receiver_machine_close(receiver_machine);
			break;
		}
		case E3_NOTICE_OF_CANCELLATION: {
			cfdp_core_finished_indication(
			    receiver_machine->core,
			    receiver_machine->transaction_id);
			receiver_machine_close(receiver_machine);
			break;
		}
		case E11_RECEIVED_FILEDATA: {
			assert(pdu != NULL);
			cfdp_core_filesegment_received_indication(
			    receiver_machine->core,
			    receiver_machine->transaction_id);

			if (transaction_is_file_transfer_in_progress(
				&receiver_machine->transaction)) {
				const cfdpFileDataPDU *file_data_pdu =
				    &(pdu->payload.u.file_data.file_data_pdu);
				if (!transaction_store_data_to_file(
					&receiver_machine->transaction,
					file_data_pdu)) {
					receiver_machine_close(
					    receiver_machine);
					return;
				}
				if (receiver_machine->received_file_size <
				    file_data_pdu->segment_offset +
					file_data_pdu->file_data.nCount) {
					receiver_machine->received_file_size =
					    file_data_pdu->segment_offset +
					    file_data_pdu->file_data.nCount;
				}
			}
			break;
		}
		case E12_RECEIVED_EOF_NO_ERROR: {
			assert(pdu != NULL);
			cfdp_core_eof_received_indication(
			    receiver_machine->core,
			    receiver_machine->transaction_id);

			if (transaction_is_file_transfer_in_progress(
				&receiver_machine->transaction)) {
				if (receiver_machine->received_file_size >
				    pdu->payload.u.file_directive
					.file_directive_pdu.u.eof_pdu
					.file_size) {
					cfdp_core_run_fault_handler(
					    receiver_machine->core,
					    receiver_machine->transaction_id,
					    DEFAULT_FAULT_HANDLER_ACTIONS
						[cfdpConditionCode_file_size_error]);
					break;
				}

				if (transaction_get_stored_file_checksum(
					&receiver_machine->transaction) !=
				    pdu->payload.u.file_directive
					.file_directive_pdu.u.eof_pdu
					.file_checksum) {
					cfdp_core_run_fault_handler(
					    receiver_machine->core,
					    receiver_machine->transaction_id,
					    DEFAULT_FAULT_HANDLER_ACTIONS
						[cfdpConditionCode_file_checksum_failure]);
					if (!transaction_delete_stored_file(
						&receiver_machine
						     ->transaction)) {
						receiver_machine_close(
						    receiver_machine);
						return;
					}
					break;
				}
			}

			cfdp_core_finished_indication(
			    receiver_machine->core,
			    receiver_machine->transaction_id);
			receiver_machine_close(receiver_machine);
			break;
		}
		case E13_RECEIVED_EOF_CANCEL: {
			assert(pdu != NULL);
			receiver_machine->condition_code =
			    pdu->payload.u.file_directive.file_directive_pdu.u
				.eof_pdu.condition_code;

			cfdp_core_finished_indication(
			    receiver_machine->core,
			    receiver_machine->transaction_id);
			receiver_machine_close(receiver_machine);
			break;
		}
		case E27_INACTIVITY_TIMEOUT: {
			cfdp_core_run_fault_handler(
			    receiver_machine->core,
			    receiver_machine->transaction_id,
			    DEFAULT_FAULT_HANDLER_ACTIONS
				[cfdpConditionCode_inactivity_detected]);
			receiver_timer_restart(&receiver_machine->timer);
			break;
		}
		case E33_RECEIVED_CANCEL_REQUEST: {
			receiver_machine->condition_code =
			    cfdpConditionCode_cancel_request_received;
			cfdp_core_issue_request(
			    receiver_machine->core,
			    receiver_machine->transaction_id,
			    E3_NOTICE_OF_CANCELLATION);
			break;
		}
		case E34_RECEIVED_REPORT_REQUEST: {
			cfdp_core_report_indication(
			    receiver_machine->core,
			    receiver_machine->transaction_id);
			break;
		}
		default: {
			cfdp_core_issue_error(receiver_machine->core,
					      UNSUPPORTED_ACTION, 0);
		}
		}
	}
}
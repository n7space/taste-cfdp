#include "cfdp_core.h"
#include "cfdp_dataview.h"
#include "event.h"
#include "sender_machine.h"
#include <string.h>

static void uint64_to_bytes_big_endian(uint64_t data, byte *result, int *size)
{
	*size = 0;
	uint64_t data_for_size_calculations = data;

	while (data_for_size_calculations != 0) {
		(*size)++;
		data_for_size_calculations >>= 8;
	}

	for (size_t i = 0; i < *size; i++) {
		result[*size - 1 - i] = (uint8_t)(data & 0xFF);
		data >>= 8;
	}
}

static cfdpPDUHeader create_pdu_header(struct sender_machine *sender_machine)
{
	cfdpPDUHeader header;

	header.version = 1;
	header.direction = cfdpDirection_toward_receiver;
	header.transmission_mode = cfdpTransmissionMode_unacknowledged;
	header.crc_flag = cfdpCRCFlag_crc_not_present;

	uint64_to_bytes_big_endian(sender_machine->transaction.source_entity_id,
				   header.source_entity_id.arr,
				   &header.source_entity_id.nCount);
	uint64_to_bytes_big_endian(
	    sender_machine->transaction.destination_entity_id,
	    header.destination_entity_id.arr,
	    &header.destination_entity_id.nCount);
	uint64_to_bytes_big_endian(sender_machine->transaction.seq_number,
				   header.transaction_sequence_number.arr,
				   &header.transaction_sequence_number.nCount);

	return header;
}

static void append_messages_to_user_to_bit_stream_with_metada_pdu(
    struct sender_machine *sender_machine, BitStream *bit_stream)
{
	const uint32_t message_to_user_count =
	    transaction_get_messages_to_user_count(
		&sender_machine->transaction);

	for (uint32_t i = 0; i < message_to_user_count; i++) {
		struct message_to_user message_to_user =
		    transaction_get_message_to_user(
			&sender_machine->transaction, i);
		cfdpTLV tlv;
		tlv.length_value.kind =
		    TLV_length_value_message_to_user_PRESENT;

		switch (message_to_user.message_to_user_type) {
		case DIRECTORY_LISTING_REQUEST: {
			tlv.length_value.u.message_to_user.value.message_to_user
			    .kind =
			    MessageToUser_directory_listing_request_PRESENT;

			strncpy((char *)tlv.length_value.u.message_to_user.value
				    .message_to_user.u.directory_listing_request
				    .directory_name.arr,
				message_to_user.message_to_user_union
				    .directory_listing_request.directory_name,
				MAX_LISTING_FILE_NAME_SIZE);
			tlv.length_value.u.message_to_user.value.message_to_user
			    .u.directory_listing_request.directory_name
			    .nCount = strlen(
			    (const char *)message_to_user.message_to_user_union
				.directory_listing_request.directory_name);

			strncpy(
			    (char *)tlv.length_value.u.message_to_user.value
				.message_to_user.u.directory_listing_request
				.directory_file_name.arr,
			    message_to_user.message_to_user_union
				.directory_listing_request.directory_file_name,
			    MAX_LISTING_FILE_NAME_SIZE);
			tlv.length_value.u.message_to_user.value.message_to_user
			    .u.directory_listing_request.directory_file_name
			    .nCount = strlen(
			    (const char *)message_to_user.message_to_user_union
				.directory_listing_request.directory_file_name);
			break;
		}
		case DIRECTORY_LISTING_RESPONSE: {
			tlv.length_value.u.message_to_user.value.message_to_user
			    .kind =
			    MessageToUser_directory_listing_response_PRESENT;

			if (message_to_user.message_to_user_union
				.directory_listing_response
				.listing_response_code == LISTING_SUCCESSFUL) {
				tlv.length_value.u.message_to_user.value
				    .message_to_user.u
				    .directory_listing_response
				    .listing_response_code =
				    cfdpListingResponseCode_successful;
			} else if (message_to_user.message_to_user_union
				       .directory_listing_response
				       .listing_response_code ==
				   LISTING_UNSUCCESSFUL) {
				tlv.length_value.u.message_to_user.value
				    .message_to_user.u
				    .directory_listing_response
				    .listing_response_code =
				    cfdpListingResponseCode_unsuccessful;
			}

			strncpy(
			    (char *)tlv.length_value.u.message_to_user.value
				.message_to_user.u.directory_listing_response
				.directory_name.arr,
			    message_to_user.message_to_user_union
				.directory_listing_response.directory_name,
			    MAX_LISTING_FILE_NAME_SIZE);
			tlv.length_value.u.message_to_user.value.message_to_user
			    .u.directory_listing_response.directory_name
			    .nCount = strlen(
			    (const char *)message_to_user.message_to_user_union
				.directory_listing_response.directory_name);

			strncpy(
			    (char *)tlv.length_value.u.message_to_user.value
				.message_to_user.u.directory_listing_response
				.directory_file_name.arr,
			    message_to_user.message_to_user_union
				.directory_listing_response.directory_file_name,
			    MAX_LISTING_FILE_NAME_SIZE);
			tlv.length_value.u.message_to_user.value.message_to_user
			    .u.directory_listing_response.directory_file_name
			    .nCount = strlen(
			    (const char *)message_to_user.message_to_user_union
				.directory_listing_response
				.directory_file_name);
			break;
		}
		case ORIGINATING_TRANSACTION_ID: {
			tlv.length_value.u.message_to_user.value.message_to_user
			    .kind =
			    MessageToUser_originating_transaction_id_PRESENT;

			uint64_to_bytes_big_endian(
			    sender_machine->transaction.source_entity_id,
			    tlv.length_value.u.message_to_user.value
				.message_to_user.u.originating_transaction_id
				.source_entity_id.arr,
			    &tlv.length_value.u.message_to_user.value
				 .message_to_user.u.originating_transaction_id
				 .source_entity_id.nCount);
			uint64_to_bytes_big_endian(
			    sender_machine->transaction.seq_number,
			    tlv.length_value.u.message_to_user.value
				.message_to_user.u.originating_transaction_id
				.transaction_sequence_number.arr,
			    &tlv.length_value.u.message_to_user.value
				 .message_to_user.u.originating_transaction_id
				 .transaction_sequence_number.nCount);
			break;
		}
		default: {

			cfdp_core_issue_error(sender_machine->core,
					      UNSUPPORTED_ACTION, 0);
			return;
		}
		}

		int error_code;
		if (!cfdpTLV_ACN_Encode(&tlv, bit_stream, &error_code, true)) {
			cfdp_core_issue_error(sender_machine->core,
					      ASN1SCC_ERROR, error_code);
			return;
		}
	}
}

void sender_machine_init(struct sender_machine *sender_machine,
			 struct transaction transaction)
{
	sender_machine->transaction = transaction;
	sender_machine->transaction_id.source_entity_id =
	    transaction.source_entity_id;
	sender_machine->transaction_id.seq_number = transaction.seq_number;
	sender_machine->condition_code = cfdpConditionCode_no_error;
	sender_machine->is_frozen = false;
	sender_machine->is_suspended = false;

	sender_machine->state = SEND_METADATA;
}

void sender_machine_close(struct sender_machine *sender_machine)
{
	sender_machine->state = COMPLETED;
}

static bool sender_machine_send_metadata(struct sender_machine *sender_machine)
{
	cfdpCfdpPDU pdu;
	cfdpPDUHeader header = create_pdu_header(sender_machine);
	cfdpMetadataPDU metadata_pdu;

	metadata_pdu.closure_requested = ClosureRequested_requested;
	metadata_pdu.checksum_type =
	    (cfdpChecksumType)sender_machine->core->checksum_type;
	metadata_pdu.file_size =
	    transaction_get_file_size(&sender_machine->transaction);

	strncpy((char *)metadata_pdu.source_file_name.arr,
		sender_machine->transaction.source_filename,
		MAX_FILE_NAME_SIZE);
	metadata_pdu.source_file_name.nCount =
	    strlen((const char *)metadata_pdu.source_file_name.arr);

	strncpy((char *)metadata_pdu.destination_file_name.arr,
		sender_machine->transaction.destination_filename,
		MAX_FILE_NAME_SIZE);
	metadata_pdu.destination_file_name.nCount =
	    strlen((const char *)metadata_pdu.destination_file_name.arr);

	pdu.pdu_header = header;
	pdu.payload.kind = PayloadData_file_directive_PRESENT;
	pdu.payload.u.file_directive.file_directive_pdu.kind =
	    FileDirectivePDU_metadata_pdu_PRESENT;
	pdu.payload.u.file_directive.file_directive_pdu.u.metadata_pdu =
	    metadata_pdu;

	long size = PDU_BUFFER_SIZE;
	memset(sender_machine->core->pdu_buffer, 0x0, (size_t)PDU_BUFFER_SIZE);
	BitStream bit_stream;
	cfdp_BitStream_AttachBuffer(&bit_stream, sender_machine->core->pdu_buffer,
			       size);
	int error_code;

	if (!cfdpCfdpPDU_ACN_Encode(&pdu, &bit_stream, &error_code, true)) {
		cfdp_core_issue_error(sender_machine->core, ASN1SCC_ERROR,
				      error_code);
		sender_machine_close(sender_machine);
		return false;
	}

	append_messages_to_user_to_bit_stream_with_metada_pdu(sender_machine,
							      &bit_stream);

	if (!sender_machine->core->transport->transport_send_pdu(
		sender_machine->core->transport->transport_data, bit_stream.buf,
		bit_stream.currentByte)) {
		cfdp_core_issue_error(sender_machine->core, TRANSPORT_ERROR, 0);
		return false;
	}

	return true;
}

static bool sender_machine_send_file_data(struct sender_machine *sender_machine)
{
	cfdpCfdpPDU pdu;
	cfdpPDUHeader header = create_pdu_header(sender_machine);
	cfdpFileDataPDU file_data_pdu;
	uint32_t length;

	file_data_pdu.segment_offset =
	    sender_machine->transaction.file_position;
	if (!transaction_get_file_segment(
		&sender_machine->transaction,
		(char *)sender_machine->core->file_segment_data_buffer,
		&length)) {
		return false;
	}
	file_data_pdu.file_data.nCount = length;
	strncpy((char *)file_data_pdu.file_data.arr,
		(const char *)sender_machine->core->file_segment_data_buffer,
		length);

	pdu.pdu_header = header;
	pdu.payload.kind = PayloadData_file_data_PRESENT;
	pdu.payload.u.file_data.file_data_pdu = file_data_pdu;

	long size = PDU_BUFFER_SIZE;
	memset(sender_machine->core->pdu_buffer, 0x0, (size_t)size);
	BitStream bit_stream;
	cfdp_BitStream_AttachBuffer(&bit_stream, sender_machine->core->pdu_buffer,
			       size);
	int error_code;

	if (!cfdpCfdpPDU_ACN_Encode(&pdu, &bit_stream, &error_code, true)) {
		cfdp_core_issue_error(sender_machine->core, ASN1SCC_ERROR,
				      error_code);
		return false;
	}

	// asn1scc cannot accept one determinant determining two seperate octet
	// strings with two different interpretations through mapping functions.
	// It was then decided to leave FileData octet string with default
	// determinant generated before octet string (2 bytes). It needs to be
	// removed after asn1scc encode
	const uint32_t determinant_size = 2;
	uint32_t determinant_index = bit_stream.currentByte - length - 1;
	memset(sender_machine->core->modified_pdu_buffer, 0x0, (size_t)size);
	memcpy(sender_machine->core->modified_pdu_buffer,
	       sender_machine->core->pdu_buffer, determinant_index - 1);
	memcpy(
	    sender_machine->core->modified_pdu_buffer + determinant_index - 1,
	    sender_machine->core->pdu_buffer + determinant_index + 1, length);

	for (uint32_t i = 0; i < determinant_size; i++) {
		if (--sender_machine->core->modified_pdu_buffer[2] == 0xFF) {
			sender_machine->core->modified_pdu_buffer[1]--;
		}
	}

	if (!sender_machine->core->transport->transport_send_pdu(
		sender_machine->core->transport->transport_data,
		sender_machine->core->modified_pdu_buffer,
		bit_stream.currentByte - determinant_size)) {
		cfdp_core_issue_error(sender_machine->core, TRANSPORT_ERROR, 0);
		return false;
	}

	return true;
}

static bool sender_machine_send_eof(struct sender_machine *sender_machine)
{
	cfdpCfdpPDU pdu;
	cfdpPDUHeader header = create_pdu_header(sender_machine);
	cfdpEofPDU eof_pdu;

	eof_pdu.condition_code = sender_machine->condition_code;
	eof_pdu.file_checksum =
	    transaction_get_file_checksum(&sender_machine->transaction);
	eof_pdu.file_size =
	    transaction_get_file_size(&sender_machine->transaction);

	pdu.pdu_header = header;
	pdu.payload.kind = PayloadData_file_directive_PRESENT;
	pdu.payload.u.file_directive.file_directive_pdu.kind =
	    FileDirectivePDU_eof_pdu_PRESENT;
	pdu.payload.u.file_directive.file_directive_pdu.u.eof_pdu = eof_pdu;

	long size = PDU_BUFFER_SIZE;
	memset(sender_machine->core->pdu_buffer, 0x0, (size_t)size);
	BitStream bit_stream;
	cfdp_BitStream_AttachBuffer(&bit_stream, sender_machine->core->pdu_buffer,
			       size);
	int error_code;

	if (!cfdpCfdpPDU_ACN_Encode(&pdu, &bit_stream, &error_code, true)) {
		cfdp_core_issue_error(sender_machine->core, ASN1SCC_ERROR,
				      error_code);
		return false;
	}

	if (!sender_machine->core->transport->transport_send_pdu(
		sender_machine->core->transport->transport_data, bit_stream.buf,
		bit_stream.currentByte)) {
		cfdp_core_issue_error(sender_machine->core, TRANSPORT_ERROR, 0);
		return false;
	}

	return true;
}

void sender_machine_update_state(struct sender_machine *sender_machine,
				 struct event *event)
{
	if (sender_machine->state == SEND_METADATA) {
		switch (event->type) {
		case E0_ENTERED_STATE: {
			sender_machine_init(sender_machine, event->transaction);
			break;
		}
		case E30_RECEIVED_PUT_REQUEST: {
			cfdp_core_transaction_indication(
			    sender_machine->core,
			    sender_machine->transaction_id);
			if (!sender_machine_send_metadata(sender_machine)) {
				sender_machine_close(sender_machine);
				return;
			}
			if (transaction_is_file_transfer_in_progress(
				&sender_machine->transaction)) {
				sender_machine->state = SEND_FILE;
				cfdp_core_issue_request(
				    sender_machine->core,
				    sender_machine->transaction_id,
				    E0_ENTERED_STATE);
			} else {
				if (!sender_machine_send_eof(sender_machine)) {
					sender_machine_close(sender_machine);
					return;
				}
				sender_machine_close(sender_machine);
				cfdp_core_eof_sent_indication(
				    sender_machine->core,
				    sender_machine->transaction_id);
				cfdp_core_finished_indication(
				    sender_machine->core,
				    sender_machine->transaction_id);
			}
			break;
		}
		default: {
			cfdp_core_issue_error(sender_machine->core,
					      UNSUPPORTED_ACTION, 0);
		}
		}
	} else if (sender_machine->state == SEND_FILE) {
		switch (event->type) {
		case E0_ENTERED_STATE: {
			cfdp_core_issue_request(sender_machine->core,
						sender_machine->transaction_id,
						E1_SEND_FILE_DATA);
			break;
		}
		case E1_SEND_FILE_DATA: {
			if (sender_machine->is_suspended ||
			    sender_machine->is_frozen)
				break;

			if (!sender_machine->core->transport
				 ->transport_is_ready(
				     sender_machine->core->transport
					 ->transport_data)) {
				break;
			}

			if (transaction_is_file_send_complete(
				&sender_machine->transaction)) {
				if (!sender_machine_send_eof(sender_machine)) {
					sender_machine_close(sender_machine);
					return;
				}
				cfdp_core_eof_sent_indication(
				    sender_machine->core,
				    sender_machine->transaction_id);
				cfdp_core_finished_indication(
				    sender_machine->core,
				    sender_machine->transaction_id);
				sender_machine_close(sender_machine);
				return;
			} else {
				if (!sender_machine_send_file_data(
					sender_machine)) {
					sender_machine_close(sender_machine);
					return;
				}
			}

			cfdp_core_issue_request(sender_machine->core,
						sender_machine->transaction_id,
						E1_SEND_FILE_DATA);

			break;
		}
		case E2_ABANDON_TRANSACTION: {
			cfdp_core_abandoned_indication(
			    sender_machine->core,
			    sender_machine->transaction_id);
			sender_machine_close(sender_machine);
			break;
		}
		case E3_NOTICE_OF_CANCELLATION: {
			sender_machine_send_eof(sender_machine);
			cfdp_core_finished_indication(
			    sender_machine->core,
			    sender_machine->transaction_id);
			sender_machine_close(sender_machine);
			break;
		}
		case E4_NOTICE_OF_SUSPENSION: {
			if (!sender_machine->is_suspended) {
				cfdp_core_suspended_indication(
				    sender_machine->core,
				    sender_machine->transaction_id);
				sender_machine->is_suspended = true;
			}
			break;
		}
		case E31_RECEIVED_SUSPEND_REQUEST: {
			cfdp_core_issue_request(sender_machine->core,
						sender_machine->transaction_id,
						E4_NOTICE_OF_SUSPENSION);
			break;
		}
		case E32_RECEIVED_RESUME_REQUEST: {
			if (sender_machine->is_suspended) {
				cfdp_core_resumed_indication(
				    sender_machine->core,
				    sender_machine->transaction_id);
				sender_machine->is_suspended = false;
			}
			cfdp_core_issue_request(sender_machine->core,
						sender_machine->transaction_id,
						E1_SEND_FILE_DATA);
			break;
		}
		case E33_RECEIVED_CANCEL_REQUEST: {
			sender_machine->condition_code =
			    cfdpConditionCode_cancel_request_received;
			cfdp_core_issue_request(sender_machine->core,
						sender_machine->transaction_id,
						E3_NOTICE_OF_CANCELLATION);
			break;
		}
		case E34_RECEIVED_REPORT_REQUEST: {
			cfdp_core_report_indication(
			    sender_machine->core,
			    sender_machine->transaction_id);
			break;
		}
		case E40_RECEIVED_FREEZE: {
			sender_machine->is_frozen = true;
			break;
		}
		case E41_RECEIVED_THAW: {
			if (sender_machine->is_frozen) {
				sender_machine->is_frozen = false;
				cfdp_core_issue_request(
				    sender_machine->core,
				    sender_machine->transaction_id,
				    E1_SEND_FILE_DATA);
			}
			break;
		}
		default: {
			cfdp_core_issue_error(sender_machine->core,
					      UNSUPPORTED_ACTION, 0);
		}
		}
	}
}
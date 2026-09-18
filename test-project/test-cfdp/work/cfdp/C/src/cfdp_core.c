#include "cfdp_core.h"
#include "event.h"

#define FILE_DATA_INDICATION_BIT 0x10
#define ENTITY_ID_AND_TRANSACTION_SEQUENCE_NUMBER_LENGTH_MASK 0x07
#define FULL_MASK 0xff
#define HEADER_FIXED_PART_SIZE 5
#define METADATA_PDU_FIXED_PART_SIZE 7

static uint64_t bytes_to_ulong(const byte *data, uint32_t size)
{
	uint64_t result = 0;
	for (uint32_t i = 0; i < size && i < sizeof(uint64_t); i++) {
		result <<= 8;
		result |= data[i];
	}
	return result;
}

void cfdp_core_init(struct cfdp_core *core, struct filestore_cfg *filestore,
		    struct transport *transport, const uint32_t entity_id,
		    const enum ChecksumType checksum_type,
		    struct receiver_timer *receiver_timer,
		    const uint32_t inactivity_timeout, uint8_t *data_buffer)
{
	core->sender[0].core = core;
	core->sender[0].state = COMPLETED;
	core->receiver[0].core = core;
	core->receiver[0].state = COMPLETED;
	core->receiver[0].timer.timeout = inactivity_timeout;
	core->receiver[0].timer = *receiver_timer;
	core->receiver[0].timer.core = core;

	core->cfdp_core_indication_callback = NULL;
	core->cfdp_core_error_callback = NULL;

	core->entity_id = entity_id;
	core->checksum_type = checksum_type;
	core->filestore = filestore;
	core->transport = transport;
	core->transaction_sequence_number = 0;
	core->virtual_source_file_size = 0;

	core->data_buffer = data_buffer;

	core->virtual_source_file_data =
	    core->data_buffer + VIRTUAL_SOURCE_FILE_BUFFER_OFFSET;
	core->file_segment_data_buffer =
	    core->data_buffer + FILE_SEGMENT_BUFFER_OFFSET;
	core->pdu_buffer = core->data_buffer + PDU_BUFFER_OFFSET;
	core->modified_pdu_buffer =
	    core->data_buffer + MODIFIED_PDU_BUFFER_OFFSET;
	core->typed_pdu =
	    (cfdpCfdpPDU *)(core->data_buffer + TYPED_PDU_BUFFER_OFFSET);
	core->event =
	    (struct event *)(core->data_buffer + EVENT_BUFFER_OFFSET);
}

void cfdp_core_register_indication_callback(
    struct cfdp_core *core, cfdp_core_indication_callback callback)
{
	core->cfdp_core_indication_callback = callback;
}

void cfdp_core_register_error_callback(struct cfdp_core *core,
				       cfdp_core_error_callback callback)
{
	core->cfdp_core_error_callback = callback;
}

void cfdp_core_issue_error(struct cfdp_core *core,
			   const enum ErrorType error_type,
			   const uint32_t error_code)
{
	if (core->cfdp_core_error_callback != NULL) {
		core->cfdp_core_error_callback(core, error_type, error_code);
	}
}

static bool cfdp_core_is_request_to_sender(struct cfdp_core *core,
					   struct transaction_id transaction_id)
{
	return transaction_id.source_entity_id ==
		   core->sender[0].transaction_id.source_entity_id &&
	       transaction_id.seq_number ==
		   core->sender[0].transaction_id.seq_number;
}

static bool
cfdp_core_is_request_to_receiver(struct cfdp_core *core,
				 struct transaction_id transaction_id)
{
	return transaction_id.source_entity_id ==
		   core->receiver[0].transaction_id.source_entity_id &&
	       transaction_id.seq_number ==
		   core->receiver[0].transaction_id.seq_number;
}

static void cfdp_core_issue_link_state_procedure(struct cfdp_core *core,
						 uint32_t destination_entity_id,
						 enum EventType event_type)
{
	struct event *event = core->event;

	if (core->sender[0].transaction.destination_entity_id ==
	    destination_entity_id) {
		memset(event, 0x0, sizeof(*event));
		event->transaction = core->sender[0].transaction;
		event->type = event_type;
		sender_machine_update_state(&core->sender[0], event);
	}
}

// asn1scc cannot accept one determinant determining two seperate octet
// strings with two different interpretations through mapping functions.
// It was then decided to leave FileData octet string with default
// determinant generated before octet string (2 bytes). It needs to be
// added before asn1scc decode
static void add_determinant_of_file_data_octet_string_in_encoded_bit_stream(
    unsigned char *buf, long *count)
{
	const uint32_t determinant_size = 2;

	if (*count < 1) {
		return;
	}

	const bool is_file_data = (buf[0] & FILE_DATA_INDICATION_BIT) != 0;

	if (!is_file_data) {
		return;
	}

	const uint32_t length_of_entity_id =
	    ((buf[3] >> 4) &
	     ENTITY_ID_AND_TRANSACTION_SEQUENCE_NUMBER_LENGTH_MASK) +
	    1;
	const uint32_t length_of_transaction_sequence_number =
	    (buf[3] & ENTITY_ID_AND_TRANSACTION_SEQUENCE_NUMBER_LENGTH_MASK) +
	    1;

	const uint32_t header_with_segment_offset_size =
	    8 + 2 * length_of_entity_id + length_of_transaction_sequence_number;

	const uint32_t file_data_size =
	    *count - header_with_segment_offset_size;

	for (uint32_t i = 0; i < determinant_size; i++) {
		for (uint32_t j = *count + i;
		     j > header_with_segment_offset_size - 1 + i; --j) {
			buf[j] = buf[j - 1];
		}
	}

	buf[header_with_segment_offset_size + 1] =
	    (unsigned char)(file_data_size & FULL_MASK);
	buf[header_with_segment_offset_size] =
	    (unsigned char)((file_data_size >> 8) & FULL_MASK);

	for (uint32_t i = 0; i < determinant_size; i++) {
		if (++buf[2] == 0x00) {
			buf[1]++;
		}
	}

	*count = *count + determinant_size;
}

static void add_message_to_user_to_transaction(struct cfdp_core *core,
					       struct transaction *transaction,
					       cfdpTLV *tlv, uint32_t index)
{

	switch (
	    tlv->length_value.u.message_to_user.value.message_to_user.kind) {

	case MessageToUser_directory_listing_request_PRESENT: {
		transaction->messages_to_user[index].message_to_user_type =
		    DIRECTORY_LISTING_REQUEST;

		strncpy(
		    transaction->messages_to_user[index]
			.message_to_user_union.directory_listing_request
			.directory_name,
		    (char *)tlv->length_value.u.message_to_user.value
			.message_to_user.u.directory_listing_request
			.directory_name.arr,
		    tlv->length_value.u.message_to_user.value.message_to_user.u
			.directory_listing_request.directory_name.nCount);
		transaction->messages_to_user[index]
		    .message_to_user_union.directory_listing_request
		    .directory_name[tlv->length_value.u.message_to_user.value
					.message_to_user.u
					.directory_listing_request
					.directory_name.nCount] = '\0';

		strncpy(
		    transaction->messages_to_user[index]
			.message_to_user_union.directory_listing_request
			.directory_file_name,
		    (char *)tlv->length_value.u.message_to_user.value
			.message_to_user.u.directory_listing_request
			.directory_file_name.arr,
		    tlv->length_value.u.message_to_user.value.message_to_user.u
			.directory_listing_request.directory_file_name.nCount);
		transaction->messages_to_user[index]
		    .message_to_user_union.directory_listing_request
		    .directory_file_name[tlv->length_value.u.message_to_user
					     .value.message_to_user.u
					     .directory_listing_request
					     .directory_file_name.nCount] =
		    '\0';
		transaction->messages_to_user_count++;
		break;
	}
	case MessageToUser_directory_listing_response_PRESENT: {
		transaction->messages_to_user[index].message_to_user_type =
		    DIRECTORY_LISTING_RESPONSE;

		if (tlv->length_value.u.message_to_user.value.message_to_user.u
			.directory_listing_response.listing_response_code ==
		    cfdpListingResponseCode_successful) {
			transaction->messages_to_user[index]
			    .message_to_user_union.directory_listing_response
			    .listing_response_code = LISTING_SUCCESSFUL;
		} else if (tlv->length_value.u.message_to_user.value
			       .message_to_user.u.directory_listing_response
			       .listing_response_code ==
			   cfdpListingResponseCode_unsuccessful) {
			transaction->messages_to_user[index]
			    .message_to_user_union.directory_listing_response
			    .listing_response_code = LISTING_UNSUCCESSFUL;
		}

		strncpy(
		    transaction->messages_to_user[index]
			.message_to_user_union.directory_listing_request
			.directory_name,
		    (char *)tlv->length_value.u.message_to_user.value
			.message_to_user.u.directory_listing_request
			.directory_name.arr,
		    tlv->length_value.u.message_to_user.value.message_to_user.u
			.directory_listing_request.directory_name.nCount);
		transaction->messages_to_user[index]
		    .message_to_user_union.directory_listing_request
		    .directory_name[tlv->length_value.u.message_to_user.value
					.message_to_user.u
					.directory_listing_request
					.directory_name.nCount] = '\0';

		strncpy(
		    transaction->messages_to_user[index]
			.message_to_user_union.directory_listing_request
			.directory_file_name,
		    (char *)tlv->length_value.u.message_to_user.value
			.message_to_user.u.directory_listing_request
			.directory_file_name.arr,
		    tlv->length_value.u.message_to_user.value.message_to_user.u
			.directory_listing_request.directory_file_name.nCount);
		transaction->messages_to_user[index]
		    .message_to_user_union.directory_listing_request
		    .directory_file_name[tlv->length_value.u.message_to_user
					     .value.message_to_user.u
					     .directory_listing_request
					     .directory_file_name.nCount] =
		    '\0';
		transaction->messages_to_user_count++;
		break;
	}
	case MessageToUser_originating_transaction_id_PRESENT: {
		transaction->messages_to_user[index].message_to_user_type =
		    ORIGINATING_TRANSACTION_ID;

		transaction->messages_to_user[index]
		    .message_to_user_union.originating_transaction_id
		    .source_entity_id = bytes_to_ulong(
		    tlv->length_value.u.message_to_user.value.message_to_user.u
			.originating_transaction_id.source_entity_id.arr,
		    tlv->length_value.u.message_to_user.value.message_to_user.u
			.originating_transaction_id.source_entity_id.nCount);
		transaction->messages_to_user[index]
		    .message_to_user_union.originating_transaction_id
		    .seq_number = bytes_to_ulong(
		    tlv->length_value.u.message_to_user.value.message_to_user.u
			.originating_transaction_id.transaction_sequence_number
			.arr,
		    tlv->length_value.u.message_to_user.value.message_to_user.u
			.originating_transaction_id.transaction_sequence_number
			.nCount);
		transaction->messages_to_user_count++;
		break;
	}
	default: {
		cfdp_core_issue_error(core, UNSUPPORTED_ACTION, 0);
	}
	}
}

static void decode_tlv_from_metadata_pdu(
    struct cfdp_core *core, struct transaction *transaction,
    const cfdpCfdpPDU *pdu,
    BitStream *bit_stream_with_raw_data_messages_to_user,
    long size_of_pdu_raw_data)
{
	const uint32_t header_with_directive_code_size =
	    HEADER_FIXED_PART_SIZE + pdu->pdu_header.source_entity_id.nCount +
	    pdu->pdu_header.destination_entity_id.nCount +
	    pdu->pdu_header.transaction_sequence_number.nCount;
	const uint32_t metadata_pdu_size =
	    METADATA_PDU_FIXED_PART_SIZE +
	    pdu->payload.u.file_directive.file_directive_pdu.u.metadata_pdu
		.source_file_name.nCount +
	    pdu->payload.u.file_directive.file_directive_pdu.u.metadata_pdu
		.destination_file_name.nCount;

	bit_stream_with_raw_data_messages_to_user->currentByte =
	    header_with_directive_code_size + metadata_pdu_size;
	bit_stream_with_raw_data_messages_to_user->currentBit = 0;

	for (uint32_t i = 0; i < MAX_NUMBER_OF_MESSAGES_TO_USER; i++) {
		if (bit_stream_with_raw_data_messages_to_user->currentByte <
		    size_of_pdu_raw_data) {
			int32_t error_code = 0;
			cfdpTLV tlv;
			if (!cfdpTLV_ACN_Decode(
				&tlv, bit_stream_with_raw_data_messages_to_user,
				&error_code)) {
				cfdp_core_issue_error(core, ASN1SCC_ERROR,
						      error_code);
				return;
			}

			add_message_to_user_to_transaction(core, transaction,
							   &tlv, i);
		} else {
			break;
		}
	}
}

void cfdp_core_issue_request(struct cfdp_core *core,
			     struct transaction_id transaction_id,
			     enum EventType event_type)
{
	struct event *event = core->event;
	memset(event, 0x0, sizeof(*event));

	if (cfdp_core_is_request_to_sender(core, transaction_id)) {
		event->transaction = core->sender[0].transaction;
		event->type = event_type;
		sender_machine_update_state(&core->sender[0], event);
	} else if (cfdp_core_is_request_to_receiver(core, transaction_id)) {
		event->transaction = core->receiver[0].transaction;
		event->type = event_type;
		receiver_machine_update_state(&core->receiver[0], event, NULL);
	}
}

struct transaction_id cfdp_core_put(struct cfdp_core *core,
				    const uint32_t destination_entity_id,
				    const char *source_filename,
				    const char *destination_filename,
				    const uint32_t messages_to_user_count,
				    struct message_to_user *messages_to_user)
{
	core->transaction_sequence_number++;

	struct event *event = core->event;
	struct transaction *transaction = &event->transaction;
	memset(event, 0x0, sizeof(*event));

	transaction->core = core;
	transaction->filestore = core->filestore;
	transaction->source_entity_id = core->entity_id;
	transaction->seq_number = core->transaction_sequence_number;
	transaction->destination_entity_id = destination_entity_id;
	transaction->messages_to_user_count = messages_to_user_count;

	for (uint32_t i = 0; i < messages_to_user_count; i++) {
		transaction->messages_to_user[i] = messages_to_user[i];
	}

	strncpy(transaction->source_filename, source_filename,
		MAX_FILE_NAME_SIZE);
	transaction->source_filename[MAX_FILE_NAME_SIZE - 1] = '\0';

	strncpy(transaction->destination_filename, destination_filename,
		MAX_FILE_NAME_SIZE);
	transaction->destination_filename[MAX_FILE_NAME_SIZE - 1] = '\0';

	if (strcmp(VIRTUAL_LISTING_FILENAME, transaction->source_filename) ==
	    0) {
		transaction->virtual_source_file_size =
		    core->virtual_source_file_size;
		transaction->virtual_source_file_data =
		    core->virtual_source_file_data;
	} else {
		transaction->virtual_source_file_size = 0;
		transaction->virtual_source_file_data = NULL;
	}

	if (core->sender[0].state != COMPLETED) {
		// TODO handle sender busy
	}
	core->sender[0].state = SEND_METADATA;

	event->type = E0_ENTERED_STATE;
	sender_machine_update_state(&core->sender[0], event);

	event->type = E30_RECEIVED_PUT_REQUEST;
	sender_machine_update_state(&core->sender[0], event);

	struct transaction_id transaction_id = {
	    .source_entity_id = core->entity_id,
	    .seq_number = core->transaction_sequence_number};

	return transaction_id;
}

void cfdp_core_cancel(struct cfdp_core *core,
		      struct transaction_id transaction_id)
{
	cfdp_core_issue_request(core, transaction_id,
				E33_RECEIVED_CANCEL_REQUEST);
}

void cfdp_core_suspend(struct cfdp_core *core,
		       struct transaction_id transaction_id)
{
	cfdp_core_issue_request(core, transaction_id,
				E31_RECEIVED_SUSPEND_REQUEST);
}

void cfdp_core_resume(struct cfdp_core *core,
		      struct transaction_id transaction_id)
{
	cfdp_core_issue_request(core, transaction_id,
				E32_RECEIVED_RESUME_REQUEST);
}

void cfdp_core_report(struct cfdp_core *core,
		      struct transaction_id transaction_id)
{
	cfdp_core_issue_request(core, transaction_id,
				E34_RECEIVED_REPORT_REQUEST);
}

void cfdp_core_transaction_indication(struct cfdp_core *core,
				      struct transaction_id transaction_id)
{
	if (core->cfdp_core_indication_callback != NULL) {
		core->cfdp_core_indication_callback(
		    core, TRANSACTION_INDICATION, transaction_id);
	}
}

void cfdp_core_eof_sent_indication(struct cfdp_core *core,
				   struct transaction_id transaction_id)
{
	if (core->cfdp_core_indication_callback != NULL) {
		core->cfdp_core_indication_callback(core, EOF_SENT_INDICATION,
						    transaction_id);
	}
}

void cfdp_core_finished_indication(struct cfdp_core *core,
				   struct transaction_id transaction_id)
{
	if (core->cfdp_core_indication_callback != NULL) {
		core->cfdp_core_indication_callback(core, FINISHED_INDICATION,
						    transaction_id);
	}
}

void cfdp_core_report_indication(struct cfdp_core *core,
				 struct transaction_id transaction_id)
{
	if (core->cfdp_core_indication_callback != NULL) {
		core->cfdp_core_indication_callback(core, REPORT_INDICATION,
						    transaction_id);
	}
}

void cfdp_core_eof_received_indication(struct cfdp_core *core,
				       struct transaction_id transaction_id)
{
	if (core->cfdp_core_indication_callback != NULL) {
		core->cfdp_core_indication_callback(
		    core, EOF_RECEIVED_INDICATION, transaction_id);
	}
}

void cfdp_core_metadata_received_indication(
    struct cfdp_core *core, struct transaction_id transaction_id)
{
	if (core->cfdp_core_indication_callback != NULL) {
		core->cfdp_core_indication_callback(
		    core, METADATA_RECEIVED_INDICATION, transaction_id);
	}
}

void cfdp_core_filesegment_received_indication(
    struct cfdp_core *core, struct transaction_id transaction_id)
{
	if (core->cfdp_core_indication_callback != NULL) {
		core->cfdp_core_indication_callback(
		    core, FILESEGMENT_RECEIVED_INDICATION, transaction_id);
	}
}

void cfdp_core_abandoned_indication(struct cfdp_core *core,
				    struct transaction_id transaction_id)
{
	if (core->cfdp_core_indication_callback != NULL) {
		core->cfdp_core_indication_callback(core, ABANDONED_INDICATION,
						    transaction_id);
	}
}

void cfdp_core_suspended_indication(struct cfdp_core *core,
				    struct transaction_id transaction_id)
{
	if (core->cfdp_core_indication_callback != NULL) {
		core->cfdp_core_indication_callback(core, SUSPENDED_INDICATION,
						    transaction_id);
	}
}

void cfdp_core_resumed_indication(struct cfdp_core *core,
				  struct transaction_id transaction_id)
{
	if (core->cfdp_core_indication_callback != NULL) {
		core->cfdp_core_indication_callback(core, RESUMED_INDICATION,
						    transaction_id);
	}
}

void cfdp_core_fault_indication(struct cfdp_core *core,
				struct transaction_id transaction_id)
{
	if (core->cfdp_core_indication_callback != NULL) {
		core->cfdp_core_indication_callback(core, FAULT_INDICATION,
						    transaction_id);
	}
}

void cfdp_core_successful_listing_indication(
    struct cfdp_core *core, struct transaction_id transaction_id)
{
	if (core->cfdp_core_indication_callback != NULL) {
		core->cfdp_core_indication_callback(
		    core, SUCCESSFUL_LISTING_INDICATION, transaction_id);
	}
}

void cfdp_core_unsuccessful_listing_indication(
    struct cfdp_core *core, struct transaction_id transaction_id)
{
	if (core->cfdp_core_indication_callback != NULL) {
		core->cfdp_core_indication_callback(
		    core, UNSUCCESSFUL_LISTING_INDICATION, transaction_id);
	}
}

void cfdp_core_freeze(struct cfdp_core *core, uint32_t destination_entity_id)
{
	cfdp_core_issue_link_state_procedure(core, destination_entity_id,
					     E40_RECEIVED_FREEZE);
}

void cfdp_core_thaw(struct cfdp_core *core, uint32_t destination_entity_id)
{
	cfdp_core_issue_link_state_procedure(core, destination_entity_id,
					     E41_RECEIVED_THAW);
}

static void create_event_for_delivery(struct cfdp_core *core,
				      const cfdpCfdpPDU *pdu,
				      struct event *event)
{
	enum EventType type = E50_NOOP;

	if (pdu->payload.kind == PayloadData_file_data_PRESENT) {
		type = E11_RECEIVED_FILEDATA;
	} else {
		switch (pdu->payload.u.file_directive.file_directive_pdu.kind) {
		case FileDirectivePDU_eof_pdu_PRESENT:
			if (pdu->payload.u.file_directive.file_directive_pdu.u
				.eof_pdu.condition_code ==
			    cfdpConditionCode_no_error) {
				type = E12_RECEIVED_EOF_NO_ERROR;
			} else {
				type = E13_RECEIVED_EOF_CANCEL;
			}
			break;
		case FileDirectivePDU_finished_pdu_PRESENT:
			if (pdu->payload.u.file_directive.file_directive_pdu.u
				.finished_pdu.condition_code ==
			    cfdpConditionCode_no_error) {
				type = E16_RECEIVED_FINISHED_NO_ERROR;
			} else {
				type = E17_RECEIVED_FINISHED_CANCEL;
			}
			break;
		case FileDirectivePDU_metadata_pdu_PRESENT:
			type = E10_RECEIVED_METADATA;
			break;
		default:
			// Unsupported pdus in Class 1
			// See CCSDS 720.2-G-3, Chapter 5.4, Table 5-5
			cfdp_core_issue_error(core, UNSUPPORTED_ACTION, 0);
			break;
		}
	}

	memset(event, 0x0, sizeof(*event));
	event->type = type;
}

static void deliver_pdu_to_sender_machine(struct cfdp_core *core,
					  const cfdpCfdpPDU *pdu)
{
	struct event *event = core->event;
	create_event_for_delivery(core, pdu, event);
	event->transaction = core->sender[0].transaction;
	sender_machine_update_state(&core->sender[0], event);
}

static void deliver_pdu_to_receiver_machine(struct cfdp_core *core,
					    const cfdpCfdpPDU *pdu)
{
	struct event *event = core->event;
	create_event_for_delivery(core, pdu, event);
	event->transaction = core->receiver[0].transaction;
	receiver_machine_update_state(&core->receiver[0], event, pdu);
}

static void handle_pdu_to_new_receiver_machine(
    struct cfdp_core *core, const cfdpCfdpPDU *pdu,
    BitStream *bit_stream_with_raw_data_messages_to_user,
    long size_of_pdu_raw_data)
{
	if (pdu->pdu_header.direction == cfdpDirection_toward_sender) {
		// Class 2 specific PDU unsupported
		// See CCSDS 720.2-G-3, Chapter 5.4, Table 5-5
		cfdp_core_issue_error(core, UNSUPPORTED_ACTION, 0);
		return;
	}

	if (!(pdu->payload.kind == PayloadData_file_directive_PRESENT &&
	      pdu->payload.u.file_directive.file_directive_pdu.kind ==
		  FileDirectivePDU_metadata_pdu_PRESENT) &&
	    !(pdu->payload.kind == PayloadData_file_directive_PRESENT &&
	      pdu->payload.u.file_directive.file_directive_pdu.kind ==
		  FileDirectivePDU_eof_pdu_PRESENT) &&
	    !(pdu->payload.kind == PayloadData_file_data_PRESENT)) {
		// Unsupported pdus in Class 1
		// See CCSDS 720.2-G-3, Chapter 5.4, Table 5-5
		cfdp_core_issue_error(core, UNSUPPORTED_ACTION, 0);
		return;
	}

	struct event *event = core->event;
	struct transaction *transaction = &event->transaction;
	memset(event, 0x0, sizeof(*event));

	transaction->core = core;
	transaction->filestore = core->filestore;
	transaction->source_entity_id =
	    bytes_to_ulong(pdu->pdu_header.source_entity_id.arr,
			   pdu->pdu_header.source_entity_id.nCount);
	transaction->seq_number =
	    bytes_to_ulong(pdu->pdu_header.transaction_sequence_number.arr,
			   pdu->pdu_header.transaction_sequence_number.nCount);
	transaction->destination_entity_id =
	    bytes_to_ulong(pdu->pdu_header.destination_entity_id.arr,
			   pdu->pdu_header.destination_entity_id.nCount);
	transaction->messages_to_user_count = 0;

	transaction->virtual_source_file_size = 0;
	transaction->virtual_source_file_data = NULL;

	if (pdu->payload.kind == PayloadData_file_directive_PRESENT &&
	    pdu->payload.u.file_directive.file_directive_pdu.kind ==
		FileDirectivePDU_metadata_pdu_PRESENT) {

		decode_tlv_from_metadata_pdu(
		    core, transaction, pdu,
		    bit_stream_with_raw_data_messages_to_user,
		    size_of_pdu_raw_data);

		strncpy(
		    transaction->source_filename,
		    (const char *)pdu->payload.u.file_directive
			.file_directive_pdu.u.metadata_pdu.source_file_name.arr,
		    pdu->payload.u.file_directive.file_directive_pdu.u
			.metadata_pdu.source_file_name.nCount);
		transaction->source_filename
		    [pdu->payload.u.file_directive.file_directive_pdu.u
			 .metadata_pdu.source_file_name.nCount] = '\0';

		strncpy(transaction->destination_filename,
			(const char *)
			    pdu->payload.u.file_directive.file_directive_pdu.u
				.metadata_pdu.destination_file_name.arr,
			pdu->payload.u.file_directive.file_directive_pdu.u
			    .metadata_pdu.destination_file_name.nCount);
		transaction->destination_filename
		    [pdu->payload.u.file_directive.file_directive_pdu.u
			 .metadata_pdu.destination_file_name.nCount] = '\0';

		transaction->file_size =
		    pdu->payload.u.file_directive.file_directive_pdu.u
			.metadata_pdu.file_size;
		transaction->file_position = 0;
	}

	core->receiver[0].state = WAIT_FOR_MD;

	event->type = E0_ENTERED_STATE;
	receiver_machine_update_state(&core->receiver[0], event, pdu);
	deliver_pdu_to_receiver_machine(core, pdu);
}

void cfdp_core_received_pdu(struct cfdp_core *core, unsigned char *buf,
			    long count)
{
	add_determinant_of_file_data_octet_string_in_encoded_bit_stream(buf,
									&count);

	BitStream bit_stream;
	BitStream_AttachBuffer(&bit_stream, buf, count);

	cfdpCfdpPDU *pdu = core->typed_pdu;
	memset(pdu, 0x0, sizeof(*pdu));
	int32_t error_code = 0;
	if (!cfdpCfdpPDU_ACN_Decode(pdu, &bit_stream, &error_code)) {
		cfdp_core_issue_error(core, ASN1SCC_ERROR, error_code);
		return;
	}

	if (pdu->pdu_header.transmission_mode == TransmissionMode_acknowledged) {
		cfdp_core_issue_error(core, UNSUPPORTED_ACTION, 0);
		return;
	}

	// This is done to properly initialize file_data.nCount
	if (pdu->payload.kind == PayloadData_file_data_PRESENT) {
		cfdpFileDataPDU *file_data_pdu =
		    &(pdu->payload.u.file_data.file_data_pdu);
		file_data_pdu->file_data
		    .arr[file_data_pdu->file_data.nCount - 1] = buf[count - 1];
	} else if (pdu->payload.kind == PayloadData_file_directive_PRESENT &&
		   pdu->payload.u.file_directive.file_directive_pdu.kind ==
		       FileDirectivePDU_metadata_pdu_PRESENT) {
	}

	struct transaction_id transaction_id;
	transaction_id.source_entity_id =
	    bytes_to_ulong(pdu->pdu_header.source_entity_id.arr,
			   pdu->pdu_header.source_entity_id.nCount);
	transaction_id.seq_number =
	    bytes_to_ulong(pdu->pdu_header.transaction_sequence_number.arr,
			   pdu->pdu_header.transaction_sequence_number.nCount);

	if (cfdp_core_is_request_to_sender(core, transaction_id)) {
		if (core->sender[0].state != COMPLETED) {
			deliver_pdu_to_sender_machine(core, pdu);
			return;
		}
	}

	if (cfdp_core_is_request_to_receiver(core, transaction_id)) {
		if (core->receiver[0].state != COMPLETED) {
			deliver_pdu_to_receiver_machine(core, pdu);
			return;
		}
	}
	handle_pdu_to_new_receiver_machine(core, pdu, &bit_stream, count);
}

void cfdp_core_run_fault_handler(struct cfdp_core *core,
				 const struct transaction_id transaction_id,
				 const enum FaultHandlerAction action)
{
	switch (action) {
	case FAULT_HANDLER_CANCEL: {
		cfdp_core_issue_request(core, transaction_id,
					E33_RECEIVED_CANCEL_REQUEST);
		break;
	}
	case FAULT_HANDLER_SUSPEND: {
		cfdp_core_issue_request(core, transaction_id,
					E31_RECEIVED_SUSPEND_REQUEST);
		break;
	}
	case FAULT_HANDLER_IGNORE: {
		break;
	}
	case FAULT_HANDLER_ABANDON: {
		cfdp_core_issue_request(core, transaction_id,
					E2_ABANDON_TRANSACTION);
		break;
	}
	default: {
		cfdp_core_issue_error(core, UNSUPPORTED_ACTION, 0);
	}
	}
}

bool cfdp_core_is_done(struct cfdp_core *core)
{
	return core->sender[0].state == COMPLETED;
}

void cfdp_core_transport_is_ready_callback(struct cfdp_core *core)
{
	cfdp_core_issue_request(core, core->sender[0].transaction_id,
				E1_SEND_FILE_DATA);
}
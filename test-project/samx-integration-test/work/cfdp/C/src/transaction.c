#include "cfdp_core.h"
#include "filestore.h"
#include "transaction.h"

#define MESSAGES_TO_USER_COUNT_ON_FILE_LISTING_RESPONSE 2

static uint32_t calucate_data_checksum(const uint8_t *data, uint32_t length,
				       enum ChecksumType checksum_type)
{
	if (checksum_type != CHECKSUM_TYPE_MODULAR) {
		return 0;
	}

	uint32_t checksum = 0;
	size_t offset = 0;

	while (offset < length) {
		size_t bytes_to_read = 4;
		if (offset > length - 4) {
			bytes_to_read = length % 4;
		}

		uint32_t value = 0;
		for (size_t i = 0; i < bytes_to_read; ++i) {
			value |= (uint32_t)data[offset + i] << ((3 - i) * 8);
		}

		checksum += value;
		offset += 4;
	}

	return checksum;
}

static uint32_t calucate_file_checksum(struct filestore_cfg *filestore,
				       const char *filepath,
				       enum ChecksumType checksum_type)
{
	if (checksum_type != CHECKSUM_TYPE_MODULAR) {
		return 0;
	}

	uint32_t file_size = filestore->filestore_get_file_size(
	    filestore->filestore_data, filepath);

	uint32_t offset = 0;
	uint32_t checksum = 0;
	char buffer[4];

	while (offset < file_size) {
		size_t bytes_to_read = 4;
		if (offset > file_size - 4) {
			bytes_to_read = file_size % 4;
		}

		filestore->filestore_read(filestore->filestore_data, filepath,
					  offset, buffer, bytes_to_read);
		offset += bytes_to_read;
		uint32_t value = 0;

		for (size_t i = 0; i < bytes_to_read; ++i) {
			value |= (uint32_t)buffer[i] << ((3 - i) * 8);
		}

		checksum += value;
	}

	return checksum;
}

bool transaction_store_data_to_file(struct transaction *transaction,
				    const cfdpFileDataPDU *file_data_pdu)
{
	if (!transaction->filestore->filestore_write(
		transaction->filestore->filestore_data,
		transaction->destination_filename,
		file_data_pdu->segment_offset,
		(const uint8_t *)file_data_pdu->file_data.arr,
		file_data_pdu->file_data.nCount)) {
		cfdp_core_issue_error(transaction->core, FILESTORE_ERROR, 0);
		return false;
	}

	return true;
}

uint32_t transaction_get_stored_file_checksum(struct transaction *transaction)
{
	return calucate_file_checksum(transaction->filestore,
				      transaction->destination_filename,
				      transaction->core->checksum_type);
}

bool transaction_delete_stored_file(struct transaction *transaction)
{
	if (!transaction->filestore->filestore_delete_file(
		transaction->filestore->filestore_data,
		transaction->destination_filename)) {
		cfdp_core_issue_error(transaction->core, FILESTORE_ERROR, 0);

		return false;
	}

	return true;
}

bool transaction_is_file_transfer_in_progress(struct transaction *transaction)
{
	return transaction->source_filename[0] != '\0' &&
	       transaction->destination_filename[0] != '\0';
}

bool transaction_is_file_send_complete(struct transaction *transaction)
{
	return transaction->file_position >= transaction->file_size;
}

uint32_t transaction_get_file_size(struct transaction *transaction)
{
	if (transaction->source_filename[0] == '\0') {
		return 0;
	}

	if (strcmp(VIRTUAL_LISTING_FILENAME, transaction->source_filename) ==
	    0) {
		transaction->file_size = transaction->virtual_source_file_size;
		return transaction->virtual_source_file_size;
	}

	transaction->file_size =
	    transaction->filestore->filestore_get_file_size(
		transaction->filestore->filestore_data,
		transaction->source_filename);

	return transaction->file_size;
}

uint32_t transaction_get_file_checksum(struct transaction *transaction)
{
	if (transaction->source_filename[0] == '\0') {
		return 0;
	}

	if (strcmp(VIRTUAL_LISTING_FILENAME, transaction->source_filename) ==
	    0) {
		return calucate_data_checksum(
		    transaction->virtual_source_file_data,
		    transaction->virtual_source_file_size,
		    transaction->core->checksum_type);
	} else {
		return calucate_file_checksum(transaction->filestore,
					      transaction->source_filename,
					      transaction->core->checksum_type);
	}
}

bool transaction_get_file_segment(struct transaction *transaction,
				  char *out_data, uint32_t *out_length)
{
	if (transaction->file_position >= transaction->file_size) {
		return false;
	}

	*out_length = transaction->file_size - transaction->file_position >
			      FILE_SEGMENT_BUFFER_SIZE
			  ? FILE_SEGMENT_BUFFER_SIZE
			  : transaction->file_size - transaction->file_position;

	if (strcmp(VIRTUAL_LISTING_FILENAME, transaction->source_filename) ==
	    0) {
		memcpy(out_data,
		       transaction->virtual_source_file_data +
			   transaction->file_position,
		       *out_length);
		transaction->file_position += *out_length;
		return true;
	}

	if (!transaction->filestore->filestore_read(
		transaction->filestore->filestore_data,
		transaction->source_filename, transaction->file_position,
		out_data, *out_length)) {
		cfdp_core_issue_error(transaction->core, FILESTORE_ERROR, 0);

		return false;
	}

	transaction->file_position += *out_length;

	return true;
}

uint32_t transaction_get_messages_to_user_count(struct transaction *transaction)
{
	return transaction->messages_to_user_count;
}

struct message_to_user
transaction_get_message_to_user(struct transaction *transaction, uint32_t index)
{
	return transaction->messages_to_user[index];
}

bool transaction_process_messages_to_user(struct transaction *transaction)
{
	for (uint32_t i = 0; i < transaction->messages_to_user_count; i++) {

		switch (transaction->messages_to_user[i].message_to_user_type) {
		case DIRECTORY_LISTING_REQUEST: {
			bool result =
			    transaction->filestore
				->filestore_dump_directory_listing(
				    transaction->filestore->filestore_data,
				    transaction->messages_to_user[i]
					.message_to_user_union
					.directory_listing_request
					.directory_name,
				    transaction->core->virtual_source_file_data,
				    VIRTUAL_SOURCE_FILE_BUFFER_SIZE);
			if (!result) {
				cfdp_core_issue_error(transaction->core,
						      FILESTORE_ERROR, 0);
				return false;
			}

			transaction->core->virtual_source_file_size = strlen(
			    (char *)
				transaction->core->virtual_source_file_data);

			struct message_to_user
			    messages_to_user[MAX_NUMBER_OF_MESSAGES_TO_USER];

			messages_to_user[0].message_to_user_type =
			    DIRECTORY_LISTING_RESPONSE;
			messages_to_user[0]
			    .message_to_user_union.directory_listing_response
			    .listing_response_code =
			    result ? LISTING_SUCCESSFUL : LISTING_UNSUCCESSFUL;
			strcpy(messages_to_user[0]
				   .message_to_user_union
				   .directory_listing_response.directory_name,
			       transaction->messages_to_user[i]
				   .message_to_user_union
				   .directory_listing_request.directory_name);
			strcpy(
			    messages_to_user[0]
				.message_to_user_union
				.directory_listing_response.directory_file_name,
			    transaction->messages_to_user[i]
				.message_to_user_union.directory_listing_request
				.directory_file_name);

			messages_to_user[1].message_to_user_type =
			    ORIGINATING_TRANSACTION_ID;
			messages_to_user[1]
			    .message_to_user_union.originating_transaction_id
			    .source_entity_id = transaction->source_entity_id;
			messages_to_user[1]
			    .message_to_user_union.originating_transaction_id
			    .seq_number = transaction->seq_number;

			if (result) {
				cfdp_core_put(
				    transaction->core,
				    transaction->core->entity_id,
				    VIRTUAL_LISTING_FILENAME,
				    transaction->messages_to_user[i]
					.message_to_user_union
					.directory_listing_request
					.directory_file_name,
				    MESSAGES_TO_USER_COUNT_ON_FILE_LISTING_RESPONSE,
				    messages_to_user);
			} else {
				cfdp_core_put(
				    transaction->core,
				    transaction->core->entity_id, "", "",
				    MESSAGES_TO_USER_COUNT_ON_FILE_LISTING_RESPONSE,
				    messages_to_user);
			}

			break;
		}
		case DIRECTORY_LISTING_RESPONSE: {
			if (transaction->messages_to_user[i]
				.message_to_user_union
				.directory_listing_response
				.listing_response_code == LISTING_SUCCESSFUL) {
				struct transaction_id transaction_id;
				transaction_id.source_entity_id =
				    transaction->source_entity_id;
				transaction_id.seq_number =
				    transaction->seq_number;
				cfdp_core_successful_listing_indication(
				    transaction->core, transaction_id);
			} else if (transaction->messages_to_user[i]
				       .message_to_user_union
				       .directory_listing_response
				       .listing_response_code ==
				   LISTING_UNSUCCESSFUL) {
				struct transaction_id transaction_id;
				transaction_id.source_entity_id =
				    transaction->source_entity_id;
				transaction_id.seq_number =
				    transaction->seq_number;
				cfdp_core_unsuccessful_listing_indication(
				    transaction->core, transaction_id);
			}
			break;
		}
		case ORIGINATING_TRANSACTION_ID: {
			// NOOP
			break;
		}
		default: {
			cfdp_core_issue_error(transaction->core,
					      UNSUPPORTED_ACTION, 0);
		}
		}
	}

	return true;
}

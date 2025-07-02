#include "cfdp_core.h"
#include "filestore.h"
#include "transaction.h"

void transaction_store_data_to_file(struct transaction *transaction,
				    const cfdpFileDataPDU *file_data_pdu)
{
	transaction->filestore->filestore_write(
	    transaction->destination_filename, file_data_pdu->segment_offset,
	    (const char *)file_data_pdu->file_data.arr, file_data_pdu->file_data.nCount);
}

uint32_t transaction_get_stored_file_checksum(struct transaction *transaction)
{
	if (transaction->core->checksum_type == CHECKSUM_TYPE_NONE) {
		return 0;
	}

	uint32_t checksum =
	    transaction->filestore->filestore_calculate_checksum(
		transaction->destination_filename,
		transaction->core->checksum_type);

	return checksum;
}

void transaction_delete_stored_file(struct transaction *transaction)
{
	transaction->filestore->filestore_delete_file(
	    transaction->destination_filename);
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
	transaction->file_size =
	    transaction->filestore->filestore_get_file_size(
		transaction->source_filename);

	return transaction->file_size;
}

uint32_t transaction_get_file_checksum(struct transaction *transaction)
{
	if (transaction->core->checksum_type == CHECKSUM_TYPE_NONE) {
		return 0;
	}

	uint32_t checksum =
	    transaction->filestore->filestore_calculate_checksum(
		transaction->source_filename, transaction->core->checksum_type);

	return checksum;
}

bool transaction_get_file_segment(struct transaction *transaction,
				  char *out_data, uint32_t *out_length)
{
	if (transaction->file_position >= transaction->file_size) {
		return false;
	}

	*out_length = transaction->file_size - transaction->file_position >
			      FILE_SEGMENT_LEN
			  ? FILE_SEGMENT_LEN
			  : transaction->file_size - transaction->file_position;

	transaction->filestore->filestore_read(transaction->source_filename,
					       transaction->file_position,
					       out_data, *out_length);

	transaction->file_position += *out_length;

	return true;
}

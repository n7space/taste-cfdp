#ifndef TRANSACTION_H
#define TRANSACTION_H

struct cfdp_core;
struct filestore_cfg;

struct transaction {
	struct cfdp_core *core;
	struct filestore_cfg *filestore;
	uint64_t source_entity_id;
	uint64_t seq_number;
	uint64_t destination_entity_id;
	char source_filename[MAX_FILE_NAME_SIZE];
	char destination_filename[MAX_FILE_NAME_SIZE];

	uint32_t file_size;
	uint32_t file_position;
};

void transaction_store_data_to_file(struct transaction *transaction,
				    const cfdpFileDataPDU *file_data_pdu);
uint32_t transaction_get_stored_file_checksum(struct transaction *transaction);
void transaction_delete_stored_file(struct transaction *transaction);

uint32_t transaction_get_file_size(struct transaction *transaction);
uint32_t transaction_get_file_checksum(struct transaction *transaction);
bool transaction_get_file_segment(struct transaction *transaction,
				  char *out_data, uint32_t *out_length);
bool transaction_is_file_transfer_in_progress(struct transaction *transaction);
bool transaction_is_file_send_complete(struct transaction *transaction);

#endif
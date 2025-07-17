#ifndef TRANSACTION_H
#define TRANSACTION_H

#include "messages_to_user.h"

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

	uint32_t virtual_source_file_size;
	uint8_t *virtual_source_file_data;

	uint32_t messages_to_user_count;
	struct message_to_user messages_to_user[MAX_NUMBER_OF_MESSAGES_TO_USER];

	uint32_t file_size;
	uint32_t file_position;
};

bool transaction_store_data_to_file(struct transaction *transaction,
				    const cfdpFileDataPDU *file_data_pdu);
uint32_t transaction_get_stored_file_checksum(struct transaction *transaction);
bool transaction_delete_stored_file(struct transaction *transaction);

uint32_t transaction_get_file_size(struct transaction *transaction);
uint32_t transaction_get_file_checksum(struct transaction *transaction);
bool transaction_get_file_segment(struct transaction *transaction,
				  char *out_data, uint32_t *out_length);
bool transaction_is_file_transfer_in_progress(struct transaction *transaction);
bool transaction_is_file_send_complete(struct transaction *transaction);

uint32_t
transaction_get_messages_to_user_count(struct transaction *transaction);
struct message_to_user
transaction_get_message_to_user(struct transaction *transaction,
				uint32_t index);

bool transaction_process_messages_to_user(struct transaction *transaction);

#endif
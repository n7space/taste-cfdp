#ifndef FILESTORE_H
#define FILESTORE_H

#include "stdint.h"

struct filestore_cfg {

	void (*filestore_create_file)(const char *filepath);
	void (*filestore_delete_file)(const char *filepath);
	void (*filestore_rename_file)(const char *old_file_name,
				      const char *new_file_name);
	void (*filestore_append_file)(const char *target_filepath,
				      const char *source_filepath);
	void (*filestore_replace_file)(const char *target_filepath,
				       const char *source_filepath);
	void (*filestore_list_directory)(const char *dirpath);

	uint64_t (*filestore_get_file_size)(const char *filepath);
	void (*filestore_read)(const char *filepath, uint32_t offset,
			       char *data, const uint32_t length);
	void (*filestore_write)(const char *filepath, uint32_t offset,
				const char *data, const uint32_t length);
	uint32_t (*filestore_calculate_checksum)(
	    const char *filepath, const enum ChecksumType checksum_type);

	void *file_ptr;
};

#endif
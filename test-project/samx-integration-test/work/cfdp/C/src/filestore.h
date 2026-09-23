#ifndef FILESTORE_H
#define FILESTORE_H

#include "stdint.h"

struct filestore_cfg {

	void *filestore_data;

	bool (*filestore_delete_file)(void *filestore_data,
				      const char *filepath);
	uint64_t (*filestore_get_file_size)(void *filestore_data,
					    const char *filepath);
	bool (*filestore_read)(void *filestore_data, const char *filepath,
			       uint32_t offset, char *data,
			       const uint32_t length);
	bool (*filestore_write)(void *filestore_data, const char *filepath,
				uint32_t offset, const uint8_t *data,
				const uint32_t length);
	bool (*filestore_dump_directory_listing)(void *filestore_data,
						 const char *dirpath,
						 uint8_t *listing_data,
						 uint32_t length);
};

#endif
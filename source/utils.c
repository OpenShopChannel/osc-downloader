#include <errno.h>
#include <gccore.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "main.h"

// Reads a file at the given path, returning the size.
// Upon failure, the returned buffer will be NULL,
// and errorMessage/errorCode will be updated appropiately.
void *ISFS_GetFile(const char *path, u32 *size) {
	// Default to having the size as zero in case we fail.
  *size = 0;

	// Attempt to open a handle to our file.
  s32 fd = ISFS_Open(path, ISFS_OPEN_READ);
  if (fd < 0) {
		sprintf(errorMessage, "Could not open file (%d).", fd);
		sprintf(errorCode, "ISFS_OPEN_FAILED");
		return NULL;
  }

	// Read its length so we can allocate appropiately.
	static fstats stats ATTRIBUTE_ALIGN(32);
  memset(&stats, 0, sizeof(fstats));

  s32 ret = ISFS_GetFileStats(fd, &stats);
	if (ret < 0) {
		sprintf(errorMessage, "Could not retrieve file stats (%d).", ret);
		sprintf(errorCode, "ISFS_OPEN_FAILED");

		ISFS_Close(fd);
		return NULL;
	}

  s32 length = stats.file_length;

  // We must align our length by 32.
  // memalign itself appears to be broken for unknown reasons.
  s32 aligned_length = length;
  s32 remainder = aligned_length % 32;
  if (remainder != 0) {
    aligned_length += 32 - remainder;
  }

	// Allocate a buffer to read in to.
  void* buf = aligned_alloc(32, aligned_length);
	if (buf == NULL) {
		sprintf(errorMessage, "Could not allocate buffer (%d).", errno);
		sprintf(errorCode, "MEM_ALLOC_FAILED");
		
		ISFS_Close(fd);
		return NULL;
	}

	// Attempt to read this file.
	s32 tmp_size = ISFS_Read(fd, buf, length);
	if (tmp_size == length) {
		// We were successful reading!.
    *size = tmp_size;
	} else {
		if (tmp_size >= 0) {
			// If we have a positive file that does not match, the file could not be fully read.
			sprintf(errorMessage, "Could not read file (read %d/%d bytes).", tmp_size, length);
			sprintf(errorCode, "ISFS_OPEN_FAILED");
			return NULL;
		} else {
			// If we have a positive file that does not match, the file could not be fully read.
			sprintf(errorMessage, "Could not read file (%d).", tmp_size);
			sprintf(errorCode, "ISFS_OPEN_FAILED");
			return NULL;
		}

		free(buf);
	}

	// Cleanup
  ISFS_Close(fd);
  return buf;
}

// Writes a file at the given path.
// Returns false on failure, updating errorMessage/errorCode appropiately.
bool ISFS_WriteFile(const char *path, void* fileContents, int contentsLength) {
	// Attempt to open a handle to our file.
  s32 fd = ISFS_Open(path, ISFS_OPEN_WRITE);
  if (fd < 0) {
		sprintf(errorMessage, "Could not open file (%d).", fd);
		sprintf(errorCode, "ISFS_WRITE_FAILED");
		return false;
  }

	s32 ret = ISFS_Write(fd, fileContents, contentsLength);
	if (ret < 0) {
		sprintf(errorMessage, "Could not write file (%d).", ret);
		sprintf(errorCode, "ISFS_WRITE_FAILED");
		return false;
	}

	ISFS_Close(fd);
	return true;
}

// Deletes a file at the given path and recreates it using the same permissions as the original.
// Returns false on failure, updating errorMessage/errorCode appropiately.
bool RecreateFile(const char *path) {
	// Obtain the file's original attributes.
	// These variable names are derived from ISFS_GetAttr's declaration.
	u32 ownerId = 0;
	u16 groupId = 0;
	u8 attributes, ownerperm, groupperm, otherperm = 0;

	s32 ret = ISFS_GetAttr(path, &ownerId, &groupId, &attributes, &ownerperm, &groupperm, &otherperm);
	if (ret < 0) {
		sprintf(errorMessage, "Could not obtain file permissions (%d).", ret);
		sprintf(errorCode, "FILE_RECREATE_FAILED");
		return false;
	}

	// Delete the original file.
	ret = ISFS_Delete(path);
	if (ret < 0) {
		sprintf(errorMessage, "Could not delete file (%d).", ret);
		sprintf(errorCode, "FILE_RECREATE_FAILED");
		return false;
	}

	// Recreate.
	ret = ISFS_CreateFile(path, attributes, ownerperm, groupperm, otherperm);
	if (ret < 0) {
		sprintf(errorMessage, "Could not create file (%d).", ret);
		sprintf(errorCode, "FILE_RECREATE_FAILED");
		return false;
	}

	// Restore previous attributes.
	ret = ISFS_SetAttr(path, ownerId, groupId, attributes, ownerperm, groupperm, otherperm);
	if (ret < 0) {
		sprintf(errorMessage, "Could not set attributes (%d).", ret);
		sprintf(errorCode, "FILE_RECREATE_FAILED");
		return false;
	}

	return true;
}
// ISFS_GetFile reads a file at the given path.
// The length of the read file is given to the passed size parameter,
// and a buffer is returned containing the contents of the file.
//
// Should a read fail, the returned buffer will be null
// and errorMessage/errorCode will be updated to contain the reason.
void *ISFS_GetFile(const char *path, u32 *size);

// ISFS_WriteFile writes a file at the given path.
// Returns false on failure, updating errorMessage/errorCode appropiately.
bool ISFS_WriteFile(const char *path, void* fileContents, int contentsLength);

// Deletes a file at the given path and recreates it using the same permissions as the original.
// Returns false on failure, updating errorMessage/errorCode appropiately.
bool RecreateFile(const char *path);
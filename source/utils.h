// ISFS_GetFile reads a file at the given path.
// The length of the read file is given to the passed size parameter,
// and a buffer is returned containing the contents of the file.
//
// Should a read fail, the returned buffer will be null
// and errorMessage/errorCode will be updated to contain the reason.
void *ISFS_GetFile(const char *path, u32 *size);
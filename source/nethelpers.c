#include "nethelpers.h"

// connectByHostname(hostname)
//
// Given a hostname, resolve the hostname and create a socket connection
// on port 80.
// 
// Returns a socket handle upon success.
// Returns -1 upon failure.

s32 connectByHostname(char * hostname) {
	struct hostent * hostInfo;
	struct sockaddr_in addr;
	u32 on = 1;
    s32 sock;

	hostInfo = memalign(32, sizeof(struct hostent));
	hostInfo = net_gethostbyname(hostname);

	if (hostInfo == NULL) {
		sprintf(errorMessage, "Could not resolve hostname '%s'.", hostname);
        sprintf(errorCode, "DNS_FAILED");
		return -1;
	}

	bcopy(hostInfo->h_addr, &addr.sin_addr, hostInfo->h_length);
	addr.sin_port = 80;
	addr.sin_family = AF_INET;

	sock = net_socket(AF_INET, SOCK_STREAM, IPPROTO_IP);

	if (sock < 0) {
		sprintf(errorMessage, "Could not create socket (%d).", sock);
        sprintf(errorCode, "SOCK_CREATE_FAILED");
		return -1;
	}

	net_setsockopt(sock, IPPROTO_TCP, TCP_NODELAY, (const char *)&on, sizeof(u32));

	s32 connRes = net_connect(sock, (struct sockaddr *)&addr, sizeof(struct sockaddr_in));

	if (connRes < 0) {
		sprintf(errorMessage, "Could not connect to server (%d).", connRes);
        sprintf(errorCode, "SOCK_CONNECT_FAILED");
		return -1;
	}

	return sock;
}

// getRemoteFileSize(socket, hostname, path)
//
// Given a socket handle, hostname, and path, get the value of the HTTP
// Content-Length header of the target resource.
//
// The hostname argument is necessary for the HTTP Host header in the request.
// 
// This is not a complete request, and leaves the socket open. The socket will
// be read from until the end of the response headers, leaving the socket handle
// ready for reading the body of the request. Because of this, this function
// should NOT be called more than once per socket handle.
//
// Returns the remote file size, in bytes, upon success.
// Returns -1 upon failure.

s32 getRemoteFileSize(s32 socket, char * hostname, char * path) {

	char * requestString = memalign(32,2048);

	sprintf(requestString, "GET %s HTTP/1.1\r\nHost: %s\r\n\r\n", path, hostname);

	s32 writeRes = net_write(socket, requestString, strlen(requestString));

	if (writeRes != strlen(requestString)) {
		sprintf(errorMessage, "Request failed (%d).", writeRes);
        sprintf(errorCode, "CONLEN_SOCK_WRITE_FAILED");
		free(requestString);
		return -1;
	}

	free(requestString);

	char * responseData = memalign(32, 8192);
	bzero(responseData, 8192);

	u32 index = 0;
	char * endHeaderPointer = NULL;
	s32 bytes_read = 0;

	while (endHeaderPointer == NULL) {
		bytes_read = net_read(socket, responseData + index, 1);
		if (bytes_read < 1) {
			sprintf(errorMessage, "Response could not be obtained (%d).", bytes_read);
            sprintf(errorCode, "CONLEN_SOCK_READ_FAILED");
			free(responseData);
			return -1;
		}
		index++;
		endHeaderPointer = strstr(responseData, "\r\n\r\n");
	}

	char * contentLengthPointer = strstr(responseData, "Content-Length: ");

	if (contentLengthPointer == NULL) {
		sprintf(errorMessage, "No Content-Length header in response: ");
        strncat(errorMessage, responseData, 30);
        strcat(errorMessage, "...");
        sprintf(errorCode, "CONLEN_HEADER_STRSTR_FAILED");
		free(responseData);
		return -1;
	}

	contentLengthPointer = contentLengthPointer + 16;

	char * contentLengthEndPointer = strstr(contentLengthPointer, "\r\n");
	
	if (contentLengthEndPointer == NULL) {
		sprintf(errorMessage, "Content-Length header could not be understood: ");
        strncat(errorMessage, responseData, 30);
        strcat(errorMessage, "...");
        sprintf(errorCode, "CONLEN_HEADER_STRSTR_END_FAILED");
		free(responseData);
		return -1;
	}

	char * contentLengthString = memalign(32, 64);
	bzero(contentLengthString, 64);

	memcpy(contentLengthString, contentLengthPointer, (contentLengthEndPointer - contentLengthPointer));

	s32 contentLength = atoi(contentLengthString);

	free(contentLengthString);
	free(responseData);

	return contentLength;
}

// saveResponseChunk(socket, file)
//
// Given a socket handle and a file handle, save the next 8kb chunk of the
// response to the given file.
//
// This should NOT be called independently, and must be preceeded by
// getRemoteFileSize.
//
// Returns 0 upon success.
// Returns -1 upon failure.

s32 saveResponseChunk(s32 socket, FILE * file) {

	char * chunkBuffer = memalign(32, 8192);
	bzero(chunkBuffer, 8192);

	s32 bytes_received = net_read(socket, chunkBuffer, 8192);

	if (bytes_received < 1) {
		sprintf(errorMessage, "Connection ended mid-download (%d).", bytes_received);
        sprintf(errorCode, "CHUNK_SOCK_READ_FAILED");
		free(chunkBuffer);
		return -1;
	}

	s32 bytes_written = fwrite(chunkBuffer, 1, bytes_received, file);

	if (bytes_written < bytes_received) {
		sprintf(errorMessage, "Unable to complete download (%d).", bytes_written);
        sprintf(errorCode, "CHUNK_FWRITE_FAILED");
		free(chunkBuffer);
		return -1;
	}

	fileBytesSaved = fileBytesSaved + bytes_written;
	free(chunkBuffer);
	return 0;
}
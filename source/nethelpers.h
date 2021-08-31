#ifndef _NETHELPERS_H_
#define _NETHELPERS_H_

#include <grrlib.h>
#include <stdio.h>
#include <stdlib.h>
#include <gccore.h>
#include <network.h>
#include <malloc.h>
#include <strings.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fat.h>
#include <math.h>

// See main.c for information on global variables.
extern u32 fileBytesSaved;
extern char * errorMessage;
extern char * errorCode;

// See nethelpers.c for details on these functions.
s32 connectByHostname(char * hostname);
s32 getRemoteFileSize(s32 socket, char * hostname, char * path);
s32 saveResponseChunk(s32 socket, FILE * file);

#endif

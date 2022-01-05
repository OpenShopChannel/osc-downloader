/*
 *
 *	Global Variables
 *
 */

// Stores error messages given by functions that fail
extern char * errorMessage;

// Stores error code given by functions that fail.
// Error codes are set as a HTTP GET parameter when
// returning to the shop channel. E.g., the error
// code "DNS_FAILED" set by connectByHostname in
// nethelpers.c is used in the path
// "/error?error=DNS_FALIED" when returning to the
// shop channel.
extern char * errorCode;	

// Stores the URL of the ZIP to download
extern char * downloadURL;

// Helpers for manipulating title IDs.
#define TITLE_UPPER(x) ((u32)((x) >> 32))
#define TITLE_LOWER(x) ((u32)(x)&0xFFFFFFFF)
/*
 *
 *	Headers
 *
 */

// Standard headers
#include <stdio.h>
#include <stdlib.h>
#include <malloc.h>
#include <strings.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>
#include <unistd.h>
#include <math.h>

// GRRLib-related headers
#include <grrlib.h>			// Thank you GRRLib!
							// https://github.com/GRRLIB/GRRLIB

// libOGC-related headers 
#include <gccore.h>			// Thank you libOGC & devkitPRO!
							// https://github.com/devkitPro/libogc
#include <network.h>
#include <fat.h>
#include <wiiuse/wpad.h>

// Custom headers
#include "miniz.h"
#include "nethelpers.h"		// See nethelpers.c and nethelpers.h for code regarding
							// downloading the ZIP from the web server

// Fonts and images
#include "LiberationSans-Regular_ttf.h"
#include "osc_png.h"
GRRLIB_ttfFont *libSans;
GRRLIB_texImg *osclogo;

/*
 *
 *	Global Variables
 *
 */

char * errorMessage;		// Stores error messages given by functions that fail

char * errorCode;			// Stores error code given by functions that fail.
							// Error codes are set as a HTTP GET parameter when
							// returning to the shop channel. E.g., the error
							// code "DNS_FAILED" set by connectByHostname in
							// nethelpers.c is used in the path
							// "/error?error=DNS_FALIED" when returning to the
							// shop channel.

char * downloadURL;			// Stores the URL of the ZIP to download

char * downloadHostname;	// Stores the hostname of the server to download the
							// ZIP from, which is extracted from downloadURL

char * downloadPath;		// Stores the path of the ZIP file to download on the
							// web server

							// Given a downloadURL,
							// http://example.com/dir/example.zip ,
							// the variables, downloadURL,
							// downloadHostname, and downloadPath will be set as
							// "http://example.com/dir/example.zip", "example.com",
							// and "/dir/example.zip", respectively.

u32 fileSize = 0;			// Stores the total size, in bytes, of the ZIP to be
							// downloaded.

u32 fileBytesSaved = 0;		// Stores the portion, in bytes, of the ZIP that
							// has been saved so far.

/*
 *
 *	URL extraction
 *
 */

// getPackageURL()
//
// This function extracts a URL from the file /shared2/wc24/nwc24dl.bin .
//
// The shop channel web interface is able to invoke a JS function,
// addDownloadTask(urlstring), which writes a given string to the that file.
// This function is part of the WC24 JS interface.
//
// To prevent the console from accidentally interpreting the URL as a valid
// WC24 URL, the URL is prefixed with the string "http://!|". For more information
// on the workings of WC24, see https://wiibrew.org/wiki/WiiConnect24
//
// The following function reads nwc24dl.bin into a buffer, iterates through each
// byte of the buffer, and searches for the "http://!|" 'magic phrase'. Upon finding
// the magic phrase, it will return the URL that follows it.
//
// For example, if the following string is found in nwc24.dl:
// http://!|http://example.com/example.zip
// The following URL will be returned:
// http://example.com/example.zip

char * getPackageURL() {
	char *nwc24dlBuffer = memalign(32, 63488);
	char *packageURL = memalign(32, 236);
	char *magicPhrase = "http://!|";

	s32 file = ISFS_Open("/shared2/wc24/nwc24dl.bin", ISFS_OPEN_READ);
	if (file < 0) {
		sprintf(errorMessage, "Could not access nwc24dl.bin.");
		sprintf(errorCode, "ISFS_OPEN_FAILED");
		return NULL;
	}

	ISFS_Read(file, nwc24dlBuffer, 63488);

	int i;
	for (i = 0; i < 63488; i = i + 1) {
		if (memcmp(nwc24dlBuffer+i, magicPhrase, 9) == 0) {
			memcpy(packageURL, nwc24dlBuffer+i+9, strlen(nwc24dlBuffer+i+9) + 1);
			break;
		} 
	}
	if (i >= 63487) {
		sprintf(errorMessage, "No download URL present.");
		sprintf(errorCode, "NO_URL_IN_BIN");
		return NULL;
	}

	ISFS_Close(file);
	free(nwc24dlBuffer);

	return packageURL;
}

/*
 *
 *	URL parsing
 *
 */

// getRequestPath(url)
//
// This function extracts a request path from a given URL.
//
// For example, given the following URL:
// http://example.com/dir/example.zip
// This function will return the following path:
// /dir/example.zip

char * getRequestPath(char * url) {
	char * removedPrefix = strstr(url, "//") + 2;
	char * removedHostname = strstr(removedPrefix, "/");
	return removedHostname;
}

// getHostname(url)
//
// This function extracts a hostname from a given URL.
//
// For example, given the following URL:
// http://example.com/dir/example.zip
// This function will return the following hostname:
// example.com

char * getHostname(char * url) {
	char * removedPrefix = strstr(url, "//") + 2;
	char * hostname = memalign(32, 255);
	bzero(hostname, 255);
	int i = 0;
	while (removedPrefix[i] != '/') {
		hostname[i] = removedPrefix[i];
		i++;
	}
	return hostname;
}

/*
 *
 *	Drawing
 *
 */

// renderMainScreen(title, boxcaption)
//
// This function will render the interface template given a "title" and a
// "box caption".
//
// Please refer to screenshots of the program for an example of what the interface
// looks like.
//
// Specifically, this function renders the title bar, title text, "main box",
// "main box" caption, progress bar background, and the OSC logo.
//
// To allow other objects to be added to the rendering stack, this function does
// NOT call GRRLib_Render(). That must be done after calling this function.

void renderMainScreen(char * title, char * boxcaption) {
	GRRLIB_Rectangle(41, 37, 559, 41, 0xF3F3F3FF, true);
	GRRLIB_Rectangle(123, 247, 394, 68, 0x34ED90FF, true);
	GRRLIB_Rectangle(132, 272, 377, 34, 0xE3FFF1FF, true);
	GRRLIB_PrintfTTF(53, 44, libSans, title, 20, 0x707070FF);
	GRRLIB_PrintfTTF(131, 252, libSans, boxcaption, 13, 0xFFFFFFFF);
	GRRLIB_DrawImg(237, 169, osclogo, 0, 0.741, 0.725, 0xFFFFFFFF);
}

// errorMessageLoop(title)
//
// This function will render an error message using the renderMainScreen() render
// stack as a template, then "traps" the program in a while loop until the user
// presses HOME to exit.
//
// Please refer to screenshots of the program for an example of what an error
// message looks like. If you would like to trigger one yourself, a reliable method
// is to disable your console's internet connection between pressing "download"
// in the shop channel and when the download actually starts. Or, just call
// errorMessageLoop and compile yourself.

void errorMesageLoop(char * title) {
	char * returnUrl = memalign(32, 512);
	sprintf(returnUrl, "/error?error=%s", errorCode);
	while (1) {
		renderMainScreen(title, "Press HOME to exit.");
		GRRLIB_PrintfTTF(138, 281, libSans, errorMessage, 13, 0x000000FF);
		GRRLIB_Render();
		WPAD_ScanPads();
		u32 pressed = WPAD_ButtonsDown(0);
		if ( pressed & WPAD_BUTTON_HOME ) {
			GRRLIB_Exit();
			WII_Initialize();
			WII_LaunchTitleWithArgs(0x0001000248414241LL, 0,returnUrl, NULL);
		}
		VIDEO_WaitVSync();
	}
}

// fadeIn()
//
// This function will render a "dummy" status screen while the program "fades in"
// from black.
//
// To increase the speed of the effect, increase the speed integer.

void fadeIn() {
	int opacity = 0;
	int speed = 8; 
	for (opacity = 0; opacity <= 255; opacity = opacity + speed) {
		renderMainScreen("Download", "Downloading");
		GRRLIB_Rectangle(0, 0, 640, 480, RGBA(0,0,0,0xFF - opacity), true);
		GRRLIB_Render();
	}
}

// fadeOut()
//
// This function will render a "dummy" status screen while the program "fades out"
// to black.
//
// To increase the speed of the effect, increase the speed integer.

void fadeOut() {
	int opacity = 0;
	int speed = 8; 
	for (opacity = 0; opacity <= 255; opacity = opacity + speed) {
		renderMainScreen("Install", "Install Complete");
		GRRLIB_Rectangle(132, 272, 377, 34, 0x35BEECFF, true);
		GRRLIB_Rectangle(0, 0, 640, 480, RGBA(0,0,0,0x00 + opacity), true);
		GRRLIB_Render();
	}
}

/*
 *
 *	Miscellaneous
 * 
 */

// initSystems()
//
// This function attempts to initialize the socket (network), FAT (SD card), and
// ISFS (NAND) subsystems.
//
// Upon failure, this function will return -1.

s32 initSystems() {

	s32 netInitResult = net_init();
	if (netInitResult < 0) {
		sprintf(errorMessage, "Could not initialize network (%d).", netInitResult);
		sprintf(errorCode, "NET_INIT_FAILED");
		return -1;
	}

	bool fatInitResult = fatInitDefault();
	if (!fatInitResult) {
		sprintf(errorMessage, "Could not access SD card.");
		sprintf(errorCode, "FAT_INIT_FAILED");
		return -1;
	}

	s32 ISFSInitResult = ISFS_Initialize();
	if (ISFSInitResult < 0) {
		sprintf(errorMessage, "Could not access NAND (%d).", ISFSInitResult);
		sprintf(errorCode, "NAND_INIT_FAILED");
		return -1;
	}

	return 0;
}

/*
 *
 *	Main function
 * 
 */

int main(int argc, char **argv) {

	// The odd-looking order of the following code, up until VIDEO_SetBlack(false),
	// is necessary to prevent graphical irregularities from appearing while
	// the program starts.

	// Initialize & set the video output to black
	GRRLIB_Init();
	GRRLIB_SetBackgroundColour(0xff, 0xff, 0xff, 0xff);
	VIDEO_SetBlack(true);
	GRRLIB_Render();	

	// Load font and logo
	libSans = GRRLIB_LoadTTF(LiberationSans_Regular_ttf, LiberationSans_Regular_ttf_size);
	osclogo = GRRLIB_LoadTexturePNG(osc_png);

	// Setup errorMessage and errorCode buffers
	errorMessage = memalign(32,16384);
	bzero(errorMessage, 16384);
	errorCode = memalign(32,255);
	bzero(errorCode, 255);

	// Attempt to initialize systems
	s32 initRes = initSystems();

	// Enable video output
	VIDEO_SetBlack(false);

	// Fade in from black
	fadeIn();

	// Throw error if any system failed to initialize
	if (initRes < 0) {
		errorMesageLoop("Initialization failed");
	}

	// Get URL of ZIP from nwc24dl.bin
	downloadURL = getPackageURL();
	if (downloadURL == NULL) {
		errorMesageLoop("Initialization failed");
	}

	// Get hostname & request path from extracted URL
	downloadHostname = getHostname(downloadURL);
	downloadPath = getRequestPath(downloadURL);

	// Attempt to resolve hostname & establish socket connection with server
	s32 sock = connectByHostname(downloadHostname);
	if (sock < 0) {
		errorMesageLoop("Download failed");
	}

	// Get file size of target ZIP
	s32 lenRes = getRemoteFileSize(sock, downloadHostname, downloadPath);
	if (lenRes < 0) {
		errorMesageLoop("Download failed");
	}

	// Set fileSize to returned value of getRemoteFileSize
	fileSize = lenRes;

	// Open/create __oscTempzip__.zip at the SD card root for writing
	FILE * f = fopen("__oscTempzip__.zip", "wb");
	if (f == NULL) {
		// Note- usually, errorMessage and errorCode are set in functions.
		// They are set here, independently of a function, this error-handling
		// is only for one line of code (fopen).
		sprintf(errorMessage, "Could not open file for saving (%d).", errno);
		sprintf(errorCode, "ZIP_OPEN_FAILED");
		errorMesageLoop("Download failed");
	}

	// Download + save file in chunks, updating progress bar
	char * downloadProgress = memalign(32,255);
	while (fileBytesSaved < fileSize) {
		s32 saveRes = saveResponseChunk(sock, f);
		if (saveRes < 0) {
			fclose(f);
			errorMesageLoop("Download failed");
		}
		sprintf(downloadProgress, "Downloading (%d/%d)", fileBytesSaved, fileSize);
		renderMainScreen("Download", downloadProgress);
		GRRLIB_Rectangle(132, 272, ((float)fileBytesSaved/(float)fileSize) * 377.0f, 34, 0x35BEECFF, true);
		GRRLIB_Render();
	}
	free(downloadProgress);

	// Close file handler for __oscTempzip__.zip
	fclose(f);

	// Unzip __oscTempzip__.zip
	// ( See the following URL for details & examples on how to use miniz:
	// https://github.com/richgel999/miniz )
	mz_zip_archive zip_archive;
	memset(&zip_archive, 0, sizeof(zip_archive));
	status = mz_zip_reader_init_file(&zip_archive, "__oscTempzip__.zip", 0);
	int i;
	int imax = mz_zip_reader_get_num_files(&zip_archive);
	char * fullpath = memalign(32,1024);
	for (i = 0; i < imax; i++) {
		bzero(fullpath, 1024);
		mz_zip_archive_file_stat file_stat;
		mz_zip_reader_file_stat(&zip_archive, i, &file_stat);
		sprintf(fullpath, "/%s", file_stat.m_filename);
		if (mz_zip_reader_is_file_a_directory(&zip_archive, i)) {
			mkdir(fullpath, 0777);
		} else {
			mz_zip_reader_extract_to_file(&zip_archive, i, fullpath, 0);
		}

		// Render progress bar as unzipping proceeds
		renderMainScreen("Install", "Install Progress");
		GRRLIB_Rectangle(132, 272, ((float)(i+1)/(float)imax) * 377.0f, 34, 0x35BEECFF, true);
		GRRLIB_Render();
	}
	mz_zip_reader_end(&zip_archive);

	// Delete __oscTempzip__.zip from SD card
	remove("__oscTempzip__.zip");

	// Fade out & exit to shop channel with "SUCCESS" error code
	fadeOut();
	GRRLIB_Exit();
	WII_Initialize();
	WII_LaunchTitleWithArgs(0x0001000248414241LL, 0,"/error?error=SUCCESS", NULL); 

	// In case hell freezes over, exit to loader
	exit(0);

	// In case hell freezes over AND pigs fly, return 0
	return 0;
}

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
#include <fat.h>
#include <wiiuse/wpad.h>

// oooo headers for network stuff
#include <wiisocket.h>
#include <curl/curl.h>

// Custom headers
#include "ec_cfg.h"
#include "main.h"
#include "miniz.h"
#include "utils.h"

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

// See main.h for an explanation of their purpose.
char * errorMessage;
char * errorCode;
char * downloadURL;

/*
 *
 *	URL extraction
 *
 */

// getTitleId()
//
// This function extracts a URL from the loaded ec.cfg.
//
// The shop channel web interface is able to invoke a JS function,
// ec.setPersistentValue(name, value), which writes a given key/value pair to the that file.
// This function is part of the ECommerceInterface JS interface.
// The frontend is expected to set the key "titleId" to a valid URL.
//
// If the key is not present, or another error occurs, this function will return NULL.
// Otherwise, it returns the given value for the key "titleId".
u64 getTitleId() {
	char* result = ecGetKeyValue("titleId");
	if (result == NULL) {
		sprintf(errorMessage, "No title ID present to download.");
		sprintf(errorCode, "NO_URL_IN_BIN");
		return 0;
	}

	u64 titleId = strtoull(result, NULL, 16);
	if (titleId == 0) {
		sprintf(errorMessage, "Invalid title ID.");
		sprintf(errorCode, "INVALID_TITLE_ID");
		return 0;
	} else {
		return titleId;
	}
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

void errorMessageLoop(char * title) {
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
// ISFS (NAND) subsystems. It then loads ec.cfg for later usage.
//
// Upon failure, this function will return -1.

s32 initSystems() {
	s32 netInitResult = wiisocket_init();
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

	s32 ecLoadResult = ecInitCfg();
	if (ecLoadResult < 0) {
		// An error message and code is already set upon failure.
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
		errorMessageLoop("Initialization failed");
	}

	// Get title ID of hidden SD title from ec.cfg
	u64 titleId = getTitleId();
	if (titleId == 0) {
		// An error message is set via getTitleId.
		errorMessageLoop("Reading title failed");
	}

	// Determine NAND path
	// 46 is the length of "/title/xxxxxxxx/yyyyyyyy/content/zzzzzzzz.app" plus a terminating null byte.
	char path[46] = "";
	sprintf(path, "/title/%08x/%08x/content/00000000.app", TITLE_UPPER(titleId), TITLE_LOWER(titleId));

	// Read NAND contents
	u32 zip_length = 0;
  void* zip_data = ISFS_GetFile("/title/00010008/53504f54/content/00000000.app", &zip_length);
	if (zip_data == NULL) {
		// An error message is set via ISFS_GetFile.
		errorMessageLoop("Reading title failed");
	}

	// Unzip our hidden SD title.
	// See the following URL for details & examples on how to use miniz:
	// https://github.com/richgel999/miniz
	mz_zip_archive zip_archive;
	memset(&zip_archive, 0, sizeof(zip_archive));
	mz_bool success = mz_zip_reader_init_mem(&zip_archive, zip_data, zip_length, 0);
	if (!success) {
		sprintf(errorMessage, "Could not initialize zip extraction.");
		sprintf(errorCode, "ZIP_OPEN_FAILED");
		errorMessageLoop("Extract failed");
	}

	int i;
	int imax = mz_zip_reader_get_num_files(&zip_archive);
	char * fullpath = memalign(32,1024);
	for (i = 0; i < imax; i++) {
		bzero(fullpath, 1024);
		mz_zip_archive_file_stat file_stat;
		mz_zip_reader_file_stat(&zip_archive, i, &file_stat);
		sprintf(fullpath, "/%s", file_stat.m_filename);
		if (mz_zip_reader_is_file_a_directory(&zip_archive, i)) {
			if (fullpath[strlen(fullpath)-1] == '/') {
				fullpath[strlen(fullpath)-1] = 0x00;
			}
			if (mkdir(fullpath, 0777) < 0 && errno != EEXIST) {
				sprintf(errorMessage, "Could not create directory on SD card.");
				sprintf(errorCode, "ZIP_EXTRACT_FAILED");
				errorMessageLoop("Extract failed");
			}
		} else {
			if (mz_zip_reader_extract_to_file(&zip_archive, i, fullpath, 0) < 0) {
				sprintf(errorMessage, "Could extract file to SD card.");
				sprintf(errorCode, "ZIP_EXTRACT_FAILED");
				errorMessageLoop(fullpath);
			}
		}

		// Render progress bar as unzipping proceeds
		renderMainScreen("Install", fullpath);
		GRRLIB_Rectangle(132, 272, ((float)(i+1)/(float)imax) * 377.0f, 34, 0x35BEECFF, true);
		GRRLIB_Render();
	}
	mz_zip_reader_end(&zip_archive);

	// Delete /openshopchannel/temp.zip from SD card
	remove("/openshopchannel/temp.zip");

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

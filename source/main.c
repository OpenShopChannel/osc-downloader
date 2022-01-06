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
#include <sdcard/wiisd_io.h> 

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

// For the IO
const DISC_INTERFACE *sd_slot = &__io_wiisd;
const DISC_INTERFACE *usb = &__io_usbstorage;

// See main.h for an explanation of their purpose.
char * errorMessage;
char * errorCode;
char * downloadURL;

u8 EMPTY_SHA1_HASH[20] = {0xda, 0x39, 0xa3, 0xee, 0x5e, 0x6b, 0x4b, 0x0d, 0x32, 0x55, 0xbf, 0xef, 0x95, 0x60, 0x18, 0x90, 0xaf, 0xd8, 0x07, 0x09};

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
		renderMainScreen("Extract", "Extracting");
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
        // Initialize IO
        usb->startup();
        sd_slot->startup();

        // Check if the SD Card is inserted
        bool isInserted = __io_wiisd.isInserted();

        // Try to mount the SD Card before the USB
        if (isInserted) {
                fatMountSimple("fat", sd_slot);
        } else {
                // Since the SD Card is not inserted, we will attempt to mount the USB.
                bool USB = __io_usbstorage.isInserted();
                if (USB) {
                        fatMountSimple("fat", usb);
                } else {
                        // No input devices were inserted OR it failed to mount either
                        // device.
                        sprintf(errorMessage, "Please insert either an SD Card or USB.");
                        sprintf(errorCode, "FAT_INIT_FAILED");
                        __io_usbstorage.shutdown();
                        __io_wiisd.shutdown();
                        return -1;
                }
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

// getTitleContentPath returns a path to a content for the given title ID.
// You may wish to free its result after usage ends.
char* getTitleContentPath(u64 titleId, u32 contentId) {
	// 46 is the length of "/title/xxxxxxxx/yyyyyyyy/content/zzzzzzzz.app", plus a terminating null byte.
	char* path = malloc(46);
	snprintf(path, 46, "/title/%08x/%08x/content/%08x.app", TITLE_UPPER(titleId), TITLE_LOWER(titleId), contentId);
	return path;
}

// nullifyTitle edits a TMD to have its zeroth index an empty hash.
// It then writes an empty file to NAND for its zeroth content.
bool nullifyTitle(u64 titleId) {
	// /title/%08x/%08x/content/title.tmd, plus a null terminator.
	char tmdPath[43] = "";

	// Read the TMD we are manipulating.
	u32 tmdSize = 0;
  sprintf(tmdPath, "/title/%08x/%08x/content/title.tmd", TITLE_UPPER(titleId), TITLE_LOWER(titleId));
  void *tmdbuf = ISFS_GetFile(tmdPath, &tmdSize);
  if (tmdbuf == NULL) {
		// An error message and code is already set upon failure.
		return false;
  }

	// A TMD with one content is 520 bytes. We should not modify it otherwise.
	if (tmdSize != 520) {
		sprintf(errorMessage, "Modified TMD (length %d).", tmdSize);
		sprintf(errorCode, "TITLE_CLEANUP_FAILED");
		return false;
	}

	// We only wish to access the TMD. Read over the signature.
	signed_blob *signedTmd = (signed_blob *)tmdbuf;
  tmd *titleTmd = SIGNATURE_PAYLOAD(signedTmd);
	
	// We wish to overwrite the first content with a null hash.
	// It should should be the only content.
	// This is the SHA-1 of an empty file.
	titleTmd->contents[0].size = 0;
	memcpy(titleTmd->contents[0].hash, EMPTY_SHA1_HASH, 20);

	// Open and overwrite our TMD with the given contents.
	bool success = ISFS_WriteFile(tmdPath, signedTmd, tmdSize);
	if (!success) {
		return false;
	}

	// Open and overwrite our primary content with nothing, nullifying.
	char* contentPath = getTitleContentPath(titleId, 0);
	success = RecreateFile(contentPath);
	if (!success) {
		return false;
	}

	return true;
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
        WPAD_Init();

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


	// Read NAND contents
	// Our NAND content is both index and ID 0.
	// We read at index 0.
	char* path = getTitleContentPath(titleId, 0);
	u32 zip_length = 0;
  void* zip_data = ISFS_GetFile(path, &zip_length);
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
		sprintf(fullpath, "fat:/%s", file_stat.m_filename);
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

	// Nullify the contents of our hidden SD title.
	// We do so in order to not clog up the user's available NAND space.
	renderMainScreen("Cleanup", "Cleaning up");
	GRRLIB_Render();
	success = nullifyTitle(titleId);
	if (!success) {
		// An error message is set via ISFS_GetFile.
		errorMessageLoop("Cleanup failed");
	}

	// Fade out & exit to shop channel with "SUCCESS" error code
	fadeOut();
	GRRLIB_Exit();
	WII_Initialize();
	WII_LaunchTitleWithArgs(0x0001000248414241LL, 0,"/error?error=SUCCESS", NULL); 
        
        // Unmount fat and deinit IO
        fatUnmount("fat:/");
        __io_usbstorage.shutdown();
        __io_wiisd.shutdown();

	// In case hell freezes over, exit to loader
	exit(0);

	// In case hell freezes over AND pigs fly, return 0
	return 0;
}

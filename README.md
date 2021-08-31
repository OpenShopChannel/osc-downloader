![Downloader Logo](/oscdownload.png)

The downloader application for the Open Shop Channel.

---

_This application is only possible thanks to the enormous contributions of the [devkitPRO](https://devkitpro.org/) team with their amazing [libOGC library](https://github.com/devkitPro/libogc), and the [GRRLib](https://github.com/GRRLIB/GRRLIB) team with their spectacular graphics library. Thank you!_

_Another thank-you to the team behind [MiniZ](https://github.com/richgel999/miniz) for their easy-to-use, portable, feature-packed (de)compresion library. Thank you, MiniZ team!_

---

## Purpose

This application downloads & installs the application which the user has selected using the Open Shop Channel interface.

## Technical description

This application attempts to download & extract a ZIP file from a URL stored in [nwc24dl.bin](https://wiibrew.org/wiki//shared2/wc24/nwc24dl.bin).

When the user selects an item for in the Open Shop Channel interface, the browser invokes a JavaScript function `addDownloadTask(urlstring)` which writes `urlstring` to `/shared2/wc24/nwc24dl.bin` on the system NAND. Once that is complete, this application is launched through the [forwarder channel](https://github.com/ramblecube/osc-downloader).

Once launched, this application iterates through `nwc24dl.bin` and finds that URL. Once the URL is found, the file (a ZIP archive) is downloaded and extracted on to the SD card. Control is then returned to the shop channel.

Far more details are included as comments in the source code!

## Intended Packaging

This application should be stored on the SD card in the directory `/apps/oscdownload` as `boot.dol`. Although it follows the directory convention of other Homebrew apps, this is not intended to be launched from the Homebrew Channel, and should not be launched as an independent application.

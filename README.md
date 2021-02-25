# gst-plugins-vision

GStreamer plugins related to the field of machine vision.


## Image acquisition elements

- aptinasrc: Video source for [Aptina Imaging (On Semiconductor) dev kits][14] (USB dev kits)
- bitflowsrc: Video source for [BitFlow frame grabbers][10] (analog, Camera Link, CoaXPress)
- edtpdvsrc: Video source for [EDT PDV frame grabbers][1] (Camera Link)
- euresyssrc: Video source for [Euresys PICOLO, DOMINO and GRABLINK series frame grabbers][3] (analog, Camera Link)
- idsueyesrc: Video source for [IDS uEye cameras][11] (GigE Vision, USB 2/3, USB3 Vision)
- imperxflexsrc: Video source for [IMPERX FrameLink and FrameLink Express frame grabbers][5] (Camera Link)
- imperxsdisrc: Video source for [IMPERX HD-SDI Express frame grabbers][15] (SDI, HD-SDI)
- kayasrc: Video source for [KAYA Instruments frame grabbers][16] (Camera Link HS, CoaXPress, 10GigE Vision)
- matroxsrc: Video source for [MATROX Imaging Library (MIL)][12] (analog, Camera Link, Camera Link HS, CoaXPress, DVI-D, FireWire, GigE Vision, SDI)
- niimaqsrc: Video source for [National Instruments IMAQ frame grabbers][6] (analog, parallel digital, Camera Link)
- niimaqdxsrc: Video source for [National Instruments IMAQdx library][7] (GigE Vision, FireWire, USB3 Vision)
- phoenixsrc: Video source for [Active Silicon Phoenix frame grabbers][8] (HD-SDI, LVDS, Camera Link)
- pixcisrc: Video source for [EPIX PIXCI frame grabbers][4] (analog, LVDS, Camera Link)
- pleorasrc: Video source for [Pleora eBUS SDK devices][17] (GigE Vision, USB3 Vision)
- pylonsrc: Video source for [Basler Pylon sources][20] (GigE Vision, USB3 Vision)
- qcamsrc: Video source for [QImaging QCam cameras][21] (FireWire, discontinued)
- saperasrc: Video source for [Teledyne DALSA frame grabbers][9] (analog, Camera Link, HSLink, LVDS)

## Image generation elements

- edtpdvsink: Video sink for [EDT PDV Camera Link simulator][2]
- gigesimsink: Video sink for [A&B Soft GigESim][18] GigE Vision simulator
- kayasink: Video sink for [KAYA Instruments CXP simulator][16]
- pleorasink: Video sink for [Pleora eBUS SDK][19] GigE Vision transmitter

## Other elements

- extractcolor: Extract a single color channel
- klvinjector: Inject test synchronous KLV metadata
- klvinspector: Inspect synchronous KLV metadata
- sfx3dnoise: Applies 3D noise to video
- videolevels: Scales monochrome 8- or 16-bit video to 8-bit, via manual setpoints or AGC


## Dependencies

- GStreamer 1.2.x or newer (1.8 needed for KLV)
- Specific frame grabber SDKs and/or licenses

## Installation

- Install GStreamer 1.2.x or newer (1.8 needed for KLV)
- Build project (see below) or download [a release from Github](https://github.com/joshdoe/gst-plugins-vision/releases) (ZIP files under Assets)
- Extract files somewhere
- Create an environment variable `GST_PLUGIN_PATH` that points to where you extracted the files

## Examples

Capture from a CoaXPress camera via a Kaya Komodo frame grabber, apply AGC to convert it to 8-bit monochrome, then output the video via A&B Software GigESim which generates GigE Vision video:
> `gst-launch-1.0 kayasrc ! videolevels auto=continuous ! gigesimsink`

Then in another command capture the GigE Vision video via Pleora eBUS and display the video to the screen:
> `gst-launch-1.0 pleorasrc ! autovideoconvert ! autovideosink`

## Compiling

### Windows

- Install [Git](https://git-scm.com/) or download a ZIP archive
- Install [CMake](https://cmake.org/)
- Install [GStreamer distribution](https://gstreamer.freedesktop.org/download/)
  or build from source. The installer should set
  the installation path via the `GSTREAMER_1_0_ROOT_X86_64` environment variable. If
  not set, set the CMake variable `GSTREAMER_ROOT` to your installation, the directory
  containing `bin` and `lib`
- Install any camera or framegrabber software/SDK for those plugins you wish to
  build. Check `cmake/modules` for any paths you may need to set.
- Run the following commands from a terminal or command prompt, assuming CMake
  and Git are in your `PATH`.
```
git clone https://github.com/joshdoe/gst-plugins-vision.git
cd gst-plugins-vision
mkdir build
cd build
cmake -G "Visual Studio 15 2017 Win64" ..
```

### Ubuntu

Steps should be similar on other Linux distributions.

```
apt-get install git cmake libgstreamer-plugins-base1.0-dev liborc-0.4-dev
git clone https://github.com/joshdoe/gst-plugins-vision.git
cd gst-plugins-vision
mkdir build
cd build
cmake ..
make
```

### Installation and packaging

To install plugins, first make sure you've set `CMAKE_INSTALL_PREFIX` properly,
the default might not be desired (e.g., system path). For finer grained control
you can set `PLUGIN_INSTALL_DIR` and related variables to specify exactly where
you want to install plugins
```
cmake --build . --target INSTALL
# or on Linux
make install
```
- To create a package of all compiled plugins run:
```
cmake --build . --target PACKAGE
# or on Linux
make package
```

## KLV

KLV support is based on a GStreamer [merge request](https://gitlab.freedesktop.org/gstreamer/gst-plugins-base/-/merge_requests/124) that has yet to be merged, so it is included here in the klv library. By default KLV support is disabled. To enable it set the CMake flag `ENABLE_KLV`. This will create the klv plugin, and make the pleora plugin dependent on the klv library. You'll need to ensure `libgstklv-1.0-1.dll` is in the system `PATH` on Windows, or on Linux make sure `libgstklv-1.0-1.so` is in the `LD_LIBRARY_PATH`.

See also
--------
- [Aravis][13], Linux open source GStreamer plugin for GigE Vision and USB3 Vision cameras

[1]: http://www.edt.com/camera_link.html
[2]: http://www.edt.com/pcidv_cls.html
[3]: http://www.euresys.com/Products/FrameGrabber.asp
[4]: http://www.epixinc.com/products/index.htm#divtab1
[5]: http://www.imperx.com/framegrabbers
[6]: http://sine.ni.com/nips/cds/view/p/lang/en/nid/1292
[7]: http://sine.ni.com/nips/cds/view/p/lang/en/nid/12892
[8]: http://www.activesilicon.com/products_fg_phx.htm
[9]: https://www.teledynedalsa.com/imaging/products/fg/
[10]: http://www.bitflow.com
[11]: http://www.ids-imaging.com
[12]: http://www.matrox.com/imaging
[13]: https://github.com/AravisProject/aravis
[14]: https://www.onsemi.com
[15]: https://www.imperx.com/framegrabbers
[16]: https://kayainstruments.com
[17]: https://www.pleora.com
[18]: http://www.ab-soft.com/gigesim.php
[19]: https://www.pleora.com
[20]: https://www.baslerweb.com/
[21]: https://www.photometrics.com/qimaging

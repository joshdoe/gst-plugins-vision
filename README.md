gst-plugins-vision
==================

GStreamer plugins related to the field of machine vision.

Image acquisition elements
------------------
- aptinasrc: Video source for [Aptina Imaging (On Semiconductor) dev kits][14] (USB dev kits)
- bitflowsrc: Video source for [BitFlow frame grabbers][10] (analog, Camera Link, CoaXPress)
- edtpdvsrc: Video source for [EDT PDV frame grabbers][1] (Camera Link)
- edtpdvsink: Video sink for [EDT PDV Camera Link simulator][2]
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
- saperasrc: Video source for [Teledyne DALSA frame grabbers][9] (analog, Camera Link, HSLink, LVDS)

Other elements
--------------
- sfx3dnoise: Applies 3D noise to video
- videolevels: Scales monochrome 8- or 16-bit video to 8-bit, via manual setpoints or AGC

Dependencies
------------
- GStreamer 1.2.x
- Specific frame grabber SDKs

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
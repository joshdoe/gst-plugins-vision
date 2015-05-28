gst-plugins-vision
==================

GStreamer plugins related to the field of machine vision.

Image acquisition elements
------------------
- edtpdvsrc: Video source for [EDT PDV frame grabbers][1] (Camera Link)
- edtpdvsink: Video sink for [EDT PDV Camera Link simulator][2]
- euresyssrc: Video source for [Euresys PICOLO, DOMINO and GRABLINK series frame grabbers][3] (analog, Camera Link)
- framelinksrc: Video source for [IMPERX FrameLink and FrameLink Express frame grabbers][5] (Camera Link)
- niimaqsrc: Video source for [National Instruments IMAQ frame grabbers][6] (analog, parallel digital, Camera Link)
- niimaqdxsrc: Video source for [National Instruments IMAQdx library][7] (GigE Vision, FireWire, USB3 Vision)
- phoenixsrc: Video source for [Active Silicon Phoenix frame grabbers][8] (HD-SDI, LVDS, Camera Link)
- pixcisrc: Video source for [EPIX PIXCI frame grabbers][4] (analog, LVDS, Camera Link)
- saperasrc: Video source for [Teledyne DALSA frame grabbers][9] (analog, Camera Link, HSLink, LVDS)

Other elements
--------------
- sfx3dnoise: Applies 3D noise to video
- videolevels: Scales monochrome 16-bit video to 8-bit, via manual setpoints or AGC

Dependencies
------------
- GStreamer 1.2.x
- Specific frame grabber SDKs

[1]: http://www.edt.com/camera_link.html
[2]: http://www.edt.com/pcidv_cls.html
[3]: http://www.euresys.com/Products/FrameGrabber.asp
[4]: http://www.epixinc.com/products/index.htm#divtab1
[5]: http://www.imperx.com/framegrabbers
[6]: http://sine.ni.com/nips/cds/view/p/lang/en/nid/1292
[7]: http://sine.ni.com/nips/cds/view/p/lang/en/nid/12892
[8]: http://www.activesilicon.com/products_fg_phx.htm
[9]: https://www.teledynedalsa.com/imaging/products/fg/

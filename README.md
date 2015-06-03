# Vimba Camera Source

## Installation

### Compiling the sources
1. download and install vimba sdk
2. `sh autogen.sh`
3. `./configure --prefix /opt/gst-vimba`
4. `make && make install`
5. `export GST_PLUGIN_PATH=/opt/gst-vimba`

## Properties

camera: Specify the camera id, e.g.

    `vimbasrc camera=DEV_000F3102A408`

## Capabilities

    The size of the image can be set via capabilities (this will affect the framerate)
    You can also specify the pixelformat:

```
    video/x-raw
    video/x-bayer
```

## Pipelines

### Show camera output in an x window

Here we get UYVY pixelformat from the camera and display it directly in the
ximagesink.

```
gst-launch-1.0 vimbasrc camera=DEV_000F3102A408 ! video/x-raw,format=UYVY,width=1024,height=768 ! videoconvert ! ximagesink
```

### Streaming RTP

This pipeline opens the vimbasrc camera, uses the bayer pixel format, encodes
it to h264 and sends it to a udp sink.

To make it work in VLC or Kurento you need a proper SDP file somewhere.

```
gst-launch-1.0 -vv vimbasrc camera=DEV_000F3102A408 ! video/x-bayer,format=grbg ,width=1920,height=1080 ! bayer2rgb ! videoconvert ! x264enc ! rtph264pay ! udpsink host=10.1.3.199 port=5000
```

## Troubleshooting

* Vimba SDK: start Install.sh to get rid of no transport layer errors
* If frames cannot be captured, try launching vimbaviewer first, access
the camera and then test the gsreamer pipeline again

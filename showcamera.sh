gst-launch-1.0 vimbasrc camera=DEV_000F3102A408 ! video/x-raw,format=UYVY,width=1024,height=768 ! videoconvert ! ximagesink

gst-launch-1.0 vimbasrc camera=DEV_000F3102A408 offset-x=308 offset-y=330 ! video/x-raw,format=UYVY,width=1920,height=1080 ! videoconvert ! ximagesink

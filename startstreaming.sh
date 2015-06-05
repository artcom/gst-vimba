gst-launch-1.0 -vv vimbasrc camera=DEV_000F3102A408 ! video/x-bayer,format=grbg ,width=1920,height=1080 ! bayer2rgb ! videoconvert ! x264enc ! rtph264pay ! udpsink host=10.1.3.199 port=5000

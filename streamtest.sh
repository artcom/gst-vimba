gst-launch-1.0 -vv videotestsrc is-live=true ! video/x-raw,format=UYVY,width=1024,height=768 ! videoconvert ! x264enc ! rtph264pay ! udpsink host=10.1.3.199 port=5000

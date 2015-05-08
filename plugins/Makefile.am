plugin_LTLIBRARIES = libgstvimba.la

# sources used to compile this plug-in
libgstvimba_la_SOURCES = gstvimbasrc.c gstvimbasrc.h vimbacamera.h vimbacamera.c vimba.h vimba.c pixelformat.h pixelformat.c

# compiler and linker flags used to compile this plugin, set in configure.ac
libgstvimba_la_CFLAGS = $(GST_CFLAGS)
libgstvimba_la_LIBADD = $(GST_LIBS)
libgstvimba_la_LDFLAGS = -lVimbaC $(GST_PLUGIN_LDFLAGS)
libgstvimba_la_LIBTOOLFLAGS = $(GST_PLUGIN_LIBTOOLFLAGS)


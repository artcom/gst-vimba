plugin_LTLIBRARIES = libgstvimbasrc.la

# sources used to compile this plug-in
libgstvimbasrc_la_SOURCES = gstvimbasrc.c gstvimbasrc.h vimbacamera.h vimbacamera.c vimba.h vimba.c pixelformat.h pixelformat.c

# compiler and linker flags used to compile this plugin, set in configure.ac
libgstvimbasrc_la_CFLAGS = $(GST_CFLAGS)
libgstvimbasrc_la_LIBADD = $(GST_LIBS)
libgstvimbasrc_la_LDFLAGS = -lVimbaC $(GST_PLUGIN_LDFLAGS)
libgstvimbasrc_la_LIBTOOLFLAGS = $(GST_PLUGIN_LIBTOOLFLAGS)


# Note: plugindir is set in configure

plugin_LTLIBRARIES = libgstnxvideodec.la

##############################################################################
# TODO: for the next set of variables, name the prefix if you named the .la, #
#  e.g. libmysomething.la => libmysomething_la_SOURCES                       #
#                            libmysomething_la_CFLAGS                        #
#                            libmysomething_la_LIBADD                        #
#                            libmysomething_la_LDFLAGS                       #
##############################################################################

# sources used to compile this plug-in
libgstnxvideodec_la_SOURCES = gstnxvideodec.c decoder.c

# compiler and linker flags used to compile this plugin, set in configure.ac
libgstnxvideodec_la_CFLAGS = \
	$(GST_CFLAGS)		\
	-I$(includedir)

libgstnxvideodec_la_LIBADD = \
	$(GST_LIBS)			\
	-lgstvideo-1.0		\
	-lgstpbutils-1.0	\
	-lnxgstmeta			\
	-lnx_video_api

libgstnxvideodec_la_LDFLAGS = $(GST_PLUGIN_LDFLAGS) -L$(libdir)
libgstnxvideodec_la_LIBTOOLFLAGS = --tag=disable-static

# headers we need but don't want installed
noinst_HEADERS = gstnxvideodec.h decoder.h

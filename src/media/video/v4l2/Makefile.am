include $(top_srcdir)/globals.mk

noinst_LTLIBRARIES = libv4l2.la

libv4l2_la_SOURCES = \
	video_device_impl.cpp \
	video_device_monitor_impl.cpp

if RING_VDPAU
libv4l2_la_SOURCES += vdpau.h vdpau.cpp
endif

if RING_VAAPI
libv4l2_la_SOURCES += vaapi.h vaapi.cpp
endif

AM_CXXFLAGS = @LIBAVCODEC_CFLAGS@ @LIBAVFORMAT_CFLAGS@ @LIBAVDEVICE_CFLAGS@ @LIBSWSCALE_CFLAGS@
AM_CXXFLAGS += @UDEV_CFLAGS@ @VDPAU_CFLAGS@ @LIBVA_CFLAGS@ @LIBVA_DRM_CFLAGS@ @LIBVA_X11_CFLAGS@

libv4l2_la_LIBADD = @LIBAVCODEC_LIBS@ @LIBAVFORMAT_LIBS@ @LIBAVDEVICE_LIBS@ @LIBSWSCALE_LIBS@ @LIBAVUTIL_LIBS@
libv4l2_la_LIBADD += @UDEV_LIBS@ @X11_LIBS@ @VDPAU_LIBS@ @LIBVA_LIBS@ @LIBVA_DRM_LIBS@ @LIBVA_X11_LIBS@


AM_CFLAGS = -I$(top_srcdir)/include -I$(top_srcdir)/lanserv
pkglib_LTLIBRARIES = mellanox.la

mellanox_la_SOURCES = mellanox.c
mellanox_la_LDFLAGS = -module

sdrs.bin: mellanox.sdrs \
		$(top_builddir)/lanserv/sdrcomp/sdrcomp
	$(top_builddir)/lanserv/sdrcomp/sdrcomp mellanox.sdrs >sdrs.bin

noinst_DATA = sdrs.bin

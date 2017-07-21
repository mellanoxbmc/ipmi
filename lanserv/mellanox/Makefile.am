
AM_CFLAGS = -I$(top_srcdir)/include -I$(top_srcdir)/lanserv
pkglib_LTLIBRARIES = mellanox.la

mellanox_la_SOURCES = mellanox.c
mellanox_la_LDFLAGS = -module

install-data-local:
	$(INSTALL) -m 755 -d "$(DESTDIR)$(sysconfdir)/ipmi/"; \
	$(INSTALL) -m 755 -d "$(DESTDIR)$(localstatedir)/mlx_ipmid/mellanox/"; \
	$(INSTALL) -m 644 $(srcdir)/mellanox.lan.conf "$(DESTDIR)$(sysconfdir)/ipmi/"; \
	$(INSTALL) -m 644 $(srcdir)/mellanox.hw "$(DESTDIR)$(sysconfdir)/ipmi/"; \
	$(INSTALL) -m 775 $(srcdir)/lancontrol "$(DESTDIR)$(sysconfdir)/ipmi/"; \
	$(INSTALL) -m 644 $(srcdir)/sdr.20.main "$(DESTDIR)$(localstatedir)/mlx_ipmid/mellanox/"; \
	$(INSTALL) -m 775 $(srcdir)/sel_set_log_size.sh "$(DESTDIR)$(bindir)"; \
	$(INSTALL) -m 775 $(srcdir)/50-mlx-events.rules "$(DESTDIR)$(localstatedir)/mlx_ipmid/mellanox/"; \
	$(INSTALL) -m 775 $(srcdir)/50-mlx-hwmon-events.rules "$(DESTDIR)$(localstatedir)/mlx_ipmid/mellanox/"; \
	$(INSTALL) -m 775 $(srcdir)/50-mlx-hwmon-4-10-events.rules "$(DESTDIR)$(localstatedir)/mlx_ipmid/mellanox/"; \
	$(INSTALL) -m 775 $(srcdir)/mlx-hw-events.sh "$(DESTDIR)$(bindir)"; \
	$(INSTALL) -m 775 $(srcdir)/status_led.py "$(DESTDIR)$(bindir)"; \
	$(INSTALL) -m 644 $(srcdir)/status_led.json "$(DESTDIR)$(sysconfdir)";

uninstall-local:
	-rm -f "$(DESTDIR)$(sysconfdir)/ipmi/mellanox.lan.conf"
	-rm -f "$(DESTDIR)$(sysconfdir)/ipmi/mellanox.hw"
	-rm -f "$(DESTDIR)$(sysconfdir)/ipmi/sdr.20.main"
	-rm -f "$(DESTDIR)$(bindir)/sel_set_log_size.sh"
	-rm -f "$(DESTDIR)$(bindir)/status_led.py"
	-rm -f "$(DESTDIR)$(sysconfdir)/status_led.json"
	-rmdir "$(DESTDIR)$(sysconfdir)/ipmi" 2>/dev/null"

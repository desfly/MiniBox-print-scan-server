include $(TOPDIR)/rules.mk

PKG_NAME:=minibox-mfp
PKG_VERSION:=0.3.0
PKG_RELEASE:=17

include $(INCLUDE_DIR)/package.mk

define Package/minibox-mfp
  SECTION:=utils
  CATEGORY:=Utilities
  TITLE:=MiniBox MFP Wi-Fi print and scan bridge
  DEPENDS:=+libusb-1.0 +uhttpd
endef

define Package/minibox-mfp/description
 Minimal IPP/eSCL/mDNS/WSD discovery runtime and libusb backend for HP LaserJet M1522n.
 No CUPS and no usblp dependency.
endef

define Build/Prepare
	$(RM) -r $(PKG_BUILD_DIR)
	$(INSTALL_DIR) $(PKG_BUILD_DIR)
	$(CP) $(CURDIR)/../../src/* $(PKG_BUILD_DIR)/
endef

TARGET_CFLAGS += -std=c99 -Wall -Wextra

MB_CC=$(TARGET_CC) $(TARGET_CPPFLAGS) $(TARGET_CFLAGS) $(TARGET_LDFLAGS)

define Build/Compile
	$(INSTALL_DIR) $(PKG_BUILD_DIR)/bin
	$(MB_CC) -o $(PKG_BUILD_DIR)/bin/minibox-printerd \
		$(PKG_BUILD_DIR)/minibox-printerd/main.c \
		$(PKG_BUILD_DIR)/minibox-printerd/http_body.c \
		$(PKG_BUILD_DIR)/minibox-ipp/ipp.c \
		$(PKG_BUILD_DIR)/minibox-ipp/print_job.c \
		$(PKG_BUILD_DIR)/minibox-raster/pwg_to_pcl.c \
		$(PKG_BUILD_DIR)/minibox-usb/stream.c \
		$(PKG_BUILD_DIR)/minibox-usb/libusb_m1522.c \
		$(PKG_BUILD_DIR)/minibox-usb/print_m1522.c -lusb-1.0
	$(MB_CC) -o $(PKG_BUILD_DIR)/bin/minibox-scand \
		$(PKG_BUILD_DIR)/minibox-scand/main.c \
		$(PKG_BUILD_DIR)/minibox-scand/scan_session.c \
		$(PKG_BUILD_DIR)/minibox-scand/scan_backend.c \
		$(PKG_BUILD_DIR)/minibox-scand/m1522_backend.c \
		$(PKG_BUILD_DIR)/minibox-escl/escl.c \
		$(PKG_BUILD_DIR)/minibox-printerd/http_body.c \
		$(PKG_BUILD_DIR)/minibox-scan/soapht_transport.c \
		$(PKG_BUILD_DIR)/minibox-scan/soapht_m1522_io.c \
		$(PKG_BUILD_DIR)/minibox-scan/soapht_codec.c \
		$(PKG_BUILD_DIR)/minibox-usb/scan_m1522.c -lusb-1.0
	$(MB_CC) -o $(PKG_BUILD_DIR)/bin/minibox-discoveryd \
		$(PKG_BUILD_DIR)/minibox-discoveryd/main.c \
		$(PKG_BUILD_DIR)/minibox-discoveryd/service.c \
		$(PKG_BUILD_DIR)/minibox-discoveryd/mdns.c
	$(MB_CC) -o $(PKG_BUILD_DIR)/bin/minibox-wsdd \
		$(PKG_BUILD_DIR)/minibox-discoveryd/wsd_main.c \
		$(PKG_BUILD_DIR)/minibox-discoveryd/wsd_runtime.c \
		$(PKG_BUILD_DIR)/minibox-discoveryd/wsd_identity.c \
		$(PKG_BUILD_DIR)/minibox-discoveryd/wsd.c
	$(MB_CC) -o $(PKG_BUILD_DIR)/bin/minibox-wsd-cgi \
		$(PKG_BUILD_DIR)/minibox-discoveryd/wsd_metadata.c \
		$(PKG_BUILD_DIR)/minibox-discoveryd/wsd_identity.c \
		$(PKG_BUILD_DIR)/minibox-discoveryd/wsd.c
	$(MB_CC) -o $(PKG_BUILD_DIR)/bin/minibox-scan-diag \
		$(PKG_BUILD_DIR)/minibox-usb/scan_diag.c \
		$(PKG_BUILD_DIR)/minibox-usb/scan_m1522.c -lusb-1.0
	$(TARGET_CC) $(TARGET_CFLAGS) $(TARGET_CPPFLAGS) \
		-Wall -Wextra -Werror \
		$(CURDIR)/src/m1522_usb_transport.c \
		-o $(PKG_BUILD_DIR)/m1522_usb_transport \
		$(TARGET_LDFLAGS) -lusb-1.0
	$(TARGET_CC) $(TARGET_CFLAGS) $(TARGET_CPPFLAGS) \
		-Wall -Wextra -Werror \
		$(CURDIR)/src/m1522_capture_parser.c \
		-o $(PKG_BUILD_DIR)/m1522_capture_parser \
		$(TARGET_LDFLAGS)
	$(TARGET_CC) $(TARGET_CFLAGS) $(TARGET_CPPFLAGS) \
		-Wall -Wextra -Werror \
		$(CURDIR)/src/m1522_scan_session.c \
		-o $(PKG_BUILD_DIR)/m1522_scan_session \
		$(TARGET_LDFLAGS)
endef

define Package/minibox-mfp/install
	$(INSTALL_DIR) $(1)/usr/sbin
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/bin/minibox-printerd $(1)/usr/sbin/
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/bin/minibox-scand $(1)/usr/sbin/
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/bin/minibox-discoveryd $(1)/usr/sbin/
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/bin/minibox-wsdd $(1)/usr/sbin/
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/bin/minibox-scan-diag $(1)/usr/sbin/
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/m1522_usb_transport $(1)/usr/sbin/
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/m1522_capture_parser $(1)/usr/sbin/
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/m1522_scan_session $(1)/usr/sbin/
	$(INSTALL_BIN) ./files/usr/sbin/minibox-m1522-descriptors $(1)/usr/sbin/
	$(INSTALL_BIN) ./files/usr/sbin/minibox-scan-capture $(1)/usr/sbin/
	$(INSTALL_BIN) ./files/usr/sbin/minibox-scan-endpoints $(1)/usr/sbin/
	$(INSTALL_BIN) ./files/usr/sbin/minibox-scan-usb $(1)/usr/sbin/

	$(INSTALL_DIR) $(1)/etc/init.d
	$(INSTALL_BIN) ./files/etc/init.d/minibox-printerd $(1)/etc/init.d/
	$(INSTALL_BIN) ./files/etc/init.d/minibox-scand $(1)/etc/init.d/
	$(INSTALL_BIN) ./files/etc/init.d/minibox-discoveryd $(1)/etc/init.d/
	$(INSTALL_BIN) ./files/etc/init.d/minibox-wsdd $(1)/etc/init.d/

	$(INSTALL_DIR) $(1)/etc/minibox/services.d
	$(INSTALL_CONF) ./files/etc/minibox/services.d/ipp-printer.service $(1)/etc/minibox/services.d/
	$(INSTALL_CONF) ./files/etc/minibox/services.d/scanner.service $(1)/etc/minibox/services.d/

	$(INSTALL_DIR) $(1)/usr/libexec/minibox-mfp
	$(CP) ./files/usr/libexec/minibox-mfp/* $(1)/usr/libexec/minibox-mfp/

	$(INSTALL_DIR) $(1)/www/minibox $(1)/www/cgi-bin
	$(INSTALL_DATA) ./files/www/index.html $(1)/www/index.html
	$(INSTALL_DATA) ./files/www/index.html $(1)/www/minibox/index.html
	$(INSTALL_DATA) ./files/www/minibox/app.js $(1)/www/minibox/app.js
	$(INSTALL_BIN) ./files/www/cgi-bin/minibox-status $(1)/www/cgi-bin/minibox-status
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/bin/minibox-wsd-cgi $(1)/www/cgi-bin/minibox-wsd
endef

$(eval $(call BuildPackage,minibox-mfp))

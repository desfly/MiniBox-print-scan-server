include $(TOPDIR)/rules.mk

PKG_NAME:=minibox-mfp
PKG_VERSION:=0.3.0
PKG_RELEASE:=1

include $(INCLUDE_DIR)/package.mk

define Package/minibox-mfp
  SECTION:=utils
  CATEGORY:=Utilities
  TITLE:=MiniBox HP M1522n print/scan bridge
  DEPENDS:=+libusb-1.0
endef

define Package/minibox-mfp/description
 Minimal userspace MiniBox print/scan bridge diagnostics and guarded HP LaserJet M1522n USB transport.
endef

define Build/Prepare
	$(call Build/Prepare/Default)
	$(CP) ./src/* $(PKG_BUILD_DIR)/
endef

define Build/Compile
	$(TARGET_CC) $(TARGET_CFLAGS) $(TARGET_CPPFLAGS) \
		-Wall -Wextra -Werror \
		$(PKG_BUILD_DIR)/m1522_usb_transport.c \
		-o $(PKG_BUILD_DIR)/m1522_usb_transport \
		$(TARGET_LDFLAGS) -lusb-1.0
	$(TARGET_CC) $(TARGET_CFLAGS) $(TARGET_CPPFLAGS) \
		-Wall -Wextra -Werror \
		$(PKG_BUILD_DIR)/m1522_capture_parser.c \
		-o $(PKG_BUILD_DIR)/m1522_capture_parser \
		$(TARGET_LDFLAGS)
	$(TARGET_CC) $(TARGET_CFLAGS) $(TARGET_CPPFLAGS) \
		-Wall -Wextra -Werror \
		$(PKG_BUILD_DIR)/m1522_scan_session.c \
		-o $(PKG_BUILD_DIR)/m1522_scan_session \
		$(TARGET_LDFLAGS)
endef

define Package/minibox-mfp/install
	$(INSTALL_DIR) $(1)/usr/sbin
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/m1522_usb_transport $(1)/usr/sbin/
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/m1522_capture_parser $(1)/usr/sbin/
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/m1522_scan_session $(1)/usr/sbin/
	$(INSTALL_BIN) ./files/usr/sbin/minibox-discovery $(1)/usr/sbin/
	$(INSTALL_BIN) ./files/usr/sbin/minibox-scan-capture $(1)/usr/sbin/
	$(INSTALL_BIN) ./files/usr/sbin/minibox-scan-endpoints $(1)/usr/sbin/
	$(INSTALL_BIN) ./files/usr/sbin/minibox-scand $(1)/usr/sbin/
	$(INSTALL_BIN) ./files/usr/sbin/minibox-m1522-descriptors $(1)/usr/sbin/

	$(INSTALL_DIR) $(1)/etc/init.d
	$(INSTALL_BIN) ./files/etc/init.d/minibox-discovery $(1)/etc/init.d/
	$(INSTALL_BIN) ./files/etc/init.d/minibox-scan $(1)/etc/init.d/
endef

$(eval $(call BuildPackage,minibox-mfp))

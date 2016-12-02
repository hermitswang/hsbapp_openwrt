include $(TOPDIR)/rules.mk
include $(INCLUDE_DIR)/package.mk

PKG_NAME:=hsbapp
PKG_VERSION:=1.0

#PKG_BUILD_DIR:=$(BUILD_DIR)/hsbapp-$(PKG_VERSION)

define Package/hsbapp
	CATEGORY:=My Package
	DEPENDS:=+librt glib2 libxml2
	TITLE:=HSB application
	MAINTAINER:=Xiong Wang <1818168@qq.com>
endef

define Build/Prepare
	mkdir -p $(PKG_BUILD_DIR)
	mkdir -p $(PKG_BUILD_DIR)/install
	$(CP) ./src/* $(PKG_BUILD_DIR)
endef

define Package/hsbapp/install
	$(INSTALL_DIR) $(1)/bin
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/install/* $(1)/bin/
endef

$(eval $(call BuildPackage,hsbapp))


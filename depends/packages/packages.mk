packages:=boost openssl libevent zeromq gmp backtrace cmake bls-dash

qt_native_packages = native_protobuf
qt_packages = qrencode protobuf zlib

qt_linux_packages:=qt expat libxcb xcb_proto libXau xproto freetype fontconfig libxkbcommon

qt_darwin_packages=qt
qt_mingw32_packages=qt

wallet_packages=bdb

zmq_packages=zeromq

upnp_packages=miniupnpc

darwin_native_packages = native_ds_store native_mac_alias

ifneq ($(build_os),darwin)
darwin_native_packages += native_cctools native_libtapi native_libdmg-hfsplus

ifeq ($(strip $(FORCE_USE_SYSTEM_CLANG)),)
darwin_native_packages+= native_clang
endif

endif

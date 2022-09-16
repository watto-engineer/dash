package=backtrace
$(package)_version=8602fda64e78f1f46563220f2ee9f7e70819c51d
$(package)_download_path=https://github.com/rust-lang-nursery/libbacktrace/archive
$(package)_file_name=$($(package)_version).tar.gz
$(package)_sha256_hash=201bb83fe8fa09a271c4abc9f0c174effb0c59d0de7c84153847008535d5bedb

define $(package)_set_vars
$(package)_config_opts=--disable-shared --enable-host-shared --prefix=$(host_prefix)
endef

define $(package)_config_cmds
  $($(package)_autoconf)
endef

define $(package)_build_cmds
  $(MAKE)
endef

define $(package)_stage_cmds
  $(MAKE) DESTDIR=$($(package)_staging_dir) install
endef

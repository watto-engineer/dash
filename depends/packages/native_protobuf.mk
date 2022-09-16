package=native_protobuf
$(package)_version=3.21.6
$(package)_download_path=https://github.com/protocolbuffers/protobuf/releases/download/v21.6/
$(package)_file_name=protobuf-cpp-$($(package)_version).tar.gz
$(package)_sha256_hash=a3c4c104b98a21a577ce5ecc0d9b9f43a359b917d0bcf69467b70dc27416dfdc

define $(package)_set_vars
$(package)_config_opts=--disable-shared
endef

define $(package)_config_cmds
  $($(package)_autoconf)
endef

define $(package)_build_cmds
  $(MAKE) -C src protoc
endef

define $(package)_stage_cmds
  $(MAKE) -C src DESTDIR=$($(package)_staging_dir) install-strip
endef

define $(package)_postprocess_cmds
  rm -rf lib include
endef

package=bdb
$(package)_version=4.8.30
$(package)_download_path=https://download.oracle.com/berkeley-db
$(package)_file_name=db-$($(package)_version).NC.tar.gz
$(package)_sha256_hash=12edc0df75bf9abd7f82f821795bcee50f42cb2e5f76a6a281b85732798364ef
$(package)_build_subdir=build_unix
$(package)_patches=clang_cxx_11.patch

define $(package)_set_vars
$(package)_config_opts=--disable-shared --enable-cxx --disable-replication --enable-option-checking
$(package)_config_opts_mingw32=--enable-mingw
$(package)_config_opts_linux=--with-pic
$(package)_cflags+=-Wno-error=implicit-function-declaration
$(package)_cxxflags=-std=c++17
$(package)_cppflags_mingw32=-DUNICODE -D_UNICODE
endef

define $(package)_preprocess_cmds
  sed -i.old 's/\/var\/tmp/$HOME\/snap\/wagerr\/common\/var\/tmp/g' csharp/DatabaseEnvironment.cs && \
  sed -i.old 's/\/var\/tmp/$HOME\/snap\/wagerr\/common\/var\/tmp/g' csharp/DatabaseEnvironmentConfig.cs && \
  sed -i.old 's/\/var\/tmp/$HOME\/snap\/wagerr\/common\/var\/tmp/g' csharp/doc/libdb_dotnet48.XML && \
  sed -i.old 's/\/var\/tmp/$HOME\/snap\/wagerr\/common\/var\/tmp/g' docs/api_reference/C/envset_tmp_dir.html && \
  sed -i.old 's/\/var\/tmp/$HOME\/snap\/wagerr\/common\/var\/tmp/g' docs/api_reference/CXX/envset_tmp_dir.html && \
  sed -i.old 's/\/var\/tmp/$HOME\/snap\/wagerr\/common\/var\/tmp/g' docs/csharp/html/F_BerkeleyDB_DatabaseEnvironmentConfig_TempDir.htm && \
  sed -i.old 's/\/var\/tmp/$HOME\/snap\/wagerr\/common\/var\/tmp/g' docs/csharp/html/P_BerkeleyDB_DatabaseEnvironment_TempDir.htm && \
  sed -i.old 's/\/var\/tmp/$HOME\/snap\/wagerr\/common\/var\/tmp/g' docs/java/com/sleepycat/db/EnvironmentConfig.html && \
  sed -i.old 's/\/var\/tmp/$HOME\/snap\/wagerr\/common\/var\/tmp/g' docs/programmer_reference/test.html && \
  sed -i.old 's/\/var\/tmp/$HOME\/snap\/wagerr\/common\/var\/tmp/g' java/src/com/sleepycat/db/EnvironmentConfig.java && \
  sed -i.old 's/\/var\/tmp/$HOME\/snap\/wagerr\/common\/var\/tmp/g' os/os_tmpdir.c && \
  sed -i.old 's/\/var\/tmp/$HOME\/snap\/wagerr\/common\/var\/tmp/g' php_db4/samples/simple_counter.php && \
  sed -i.old 's/\/var\/tmp/$HOME\/snap\/wagerr\/common\/var\/tmp/g' php_db4/samples/transactional_counter.php && \
  patch -p1 < $($(package)_patch_dir)/clang_cxx_11.patch && \
  cp -f $(BASEDIR)/config.guess $(BASEDIR)/config.sub dist
endef

define $(package)_config_cmds
  ../dist/$($(package)_autoconf)
endef

define $(package)_build_cmds
  $(MAKE) libdb_cxx-4.8.a libdb-4.8.a
endef

define $(package)_stage_cmds
  $(MAKE) DESTDIR=$($(package)_staging_dir) install_lib install_include
endef

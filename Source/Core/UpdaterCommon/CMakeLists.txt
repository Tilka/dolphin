add_library(updatercommon
  UI.h
  UpdaterCommon.cpp
  UpdaterCommon.h
)

target_link_libraries(updatercommon PRIVATE
  uicommon
  MbedTLS::mbedtls
  ZLIB::ZLIB
  ed25519
  cpp-optparse
)

target_precompile_headers(updatercommon REUSE_FROM use_pch)

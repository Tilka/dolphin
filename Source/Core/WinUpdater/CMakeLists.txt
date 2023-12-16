set (MANIFEST_FILE Updater.exe.manifest)

add_executable(winupdater WIN32
  Main.cpp
  Platform.cpp
  WinUI.cpp
  ${MANIFEST_FILE})

target_link_libraries(winupdater PRIVATE
  uicommon
  updatercommon
  Comctl32
)

set_target_properties(winupdater PROPERTIES OUTPUT_NAME "Updater")

target_precompile_headers(winupdater REUSE_FROM use_pch)

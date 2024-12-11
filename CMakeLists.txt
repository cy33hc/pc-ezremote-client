cmake_minimum_required(VERSION 3.5)

project(ezremote-client)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DDONT_HAVE_STRUPR")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fpermissive -Wl,--copy-dt-needed-entries")

add_definitions(-DCPPHTTPLIB_OPENSSL_SUPPORT)
add_definitions(-DCPPHTTPLIB_THREAD_POOL_COUNT=16)

include_directories(
  source
  source/pugixml
  source/imgui
)

add_executable(ezremote-client
  source/imgui/imgui_draw.cpp
  source/imgui/imgui_impl_sdl.cpp
  source/imgui/imgui_impl_sdlrenderer.cpp
  source/imgui/imgui_tables.cpp
  source/imgui/imgui_widgets.cpp
  source/imgui/imgui.cpp
  source/pugixml/pugixml.cpp
  source/http/httplib.cpp
  source/clients/baseclient.cpp
  source/clients/apache.cpp
  source/clients/archiveorg.cpp
  source/clients/ftpclient.cpp
  source/clients/myrient.cpp
  source/clients/iis.cpp
  source/clients/nginx.cpp
  source/clients/npxserve.cpp
  source/clients/nfsclient.cpp
  source/clients/smbclient.cpp
  source/clients/sftpclient.cpp
  source/clients/rclone.cpp
  source/clients/webdav.cpp
  source/filehost/alldebrid.cpp
  source/filehost/realdebrid.cpp
  source/filehost/directhost.cpp
  source/filehost/filehost.cpp
  source/filehost/1fichier.cpp
  source/filehost/mediafire.cpp
  source/filehost/pixeldrain.cpp
  source/server/http_server.cpp
  source/actions.cpp
  source/config.cpp
  source/crypt.c
  source/fs.cpp
  source/gui.cpp
  source/ime_dialog.cpp
  source/inifile.c
  source/lang.cpp
  source/main.cpp
  source/textures.cpp
  source/windows.cpp
  source/zip_util.cpp
  source/split_file.cpp
)

target_link_libraries(ezremote-client
  c
  z
  zstd
  pthread
  SDL2
  SDL2_image
  crypto
  ssl
  curl
  lexbor_static
  smb2
  nfs
  minizip
  archive
  json-c
  xml2
  ssh2
)

set(APPDIR "AppDir")

add_custom_command(OUTPUT ${APPDIR}
   COMMAND mkdir -p ${APPDIR}/usr/share
   COMMAND cp -R ${PROJECT_SOURCE_DIR}/data/* ${APPDIR}/usr/share
   COMMENT "Copying data to AppDir"
)

add_custom_target(appimage
   COMMAND rm -f AppDir/usr/bin/ezremote-client
   COMMAND rm -f ezremote-client.AppImage
   COMMAND linuxdeploy --appdir AppDir -e ./ezremote-client
   COMMAND appimagetool AppDir

   DEPENDS ${APPDIR}
   DEPENDS ezremote-client
)
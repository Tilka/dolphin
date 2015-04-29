# notes:
# - needs http://sourceforge.net/p/mingw-w64/mingw-w64/ci/6c56d0b0eb5be9fbeb552ba070a2304b842a5102/ (--> aur/mingw-w64-headers-git)
# - needs http://sourceforge.net/p/mingw-w64/mingw-w64/ci/0d95c795b44b76e1b60dfc119fd93cfd0cb35816/ (--> aur/mingw-w64-crt-git)

# problems:
# - native soundtouch is found instead of cross-compiled
# - xaudio

set(CMAKE_SYSTEM_NAME Windows)
set(COMPILER_PREFIX "x86_64-w64-mingw32")
set(CMAKE_SYSTEM_PROCESSOR "AMD64")
set(CMAKE_RC_COMPILER ${COMPILER_PREFIX}-windres)
set(CMAKE_C_COMPILER ${COMPILER_PREFIX}-gcc)
set(CMAKE_CXX_COMPILER ${COMPILER_PREFIX}-g++)
set(CMAKE_FIND_ROOT_PATH /usr/${COMPILER_PREFIX})
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)

# FindOpenAL.cmake is weird
set(ENV{OPENALDIR} ${CMAKE_FIND_ROOT_PATH})

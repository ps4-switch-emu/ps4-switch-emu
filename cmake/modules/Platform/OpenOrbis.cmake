# Platform file for CMAKE_SYSTEM_NAME OpenOrbis: a Unix-like target with static libraries only.
# Deliberately not Platform/FreeBSD: that would set BSD, and Eden's DetectPlatform would then
# take its FreeBSD paths.
set(UNIX 1)
include(Platform/UnixPaths)
set(CMAKE_STATIC_LIBRARY_PREFIX "lib")
set(CMAKE_STATIC_LIBRARY_SUFFIX ".a")
set(CMAKE_EXECUTABLE_SUFFIX "")
set(CMAKE_FIND_LIBRARY_PREFIXES "lib")
set(CMAKE_FIND_LIBRARY_SUFFIXES ".a")

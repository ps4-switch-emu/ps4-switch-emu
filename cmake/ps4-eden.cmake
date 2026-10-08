# CMake toolchain for Eden on PS4 (used inside the edenps4-build container).
#
# Combines what each project already proved:
#   - Eden's eden-orbis-ps4 branch: CMAKE_SYSTEM_NAME "OpenOrbis" (Eden's DetectPlatform sets
#     OPENORBIS from it), __OPENORBIS__, emulated TLS, btver2, ld.lld invoked directly, and the
#     extra Sce libraries it links.
#   - DolphinPS4 (toolchain/ps4-love-modern.cmake, ps4-love-style.cmake): clang 21 + libc++ 21
#     from $PS4_SYSROOT, the libc-overlay headers (FreeBSD values for the musl headers' clock ids,
#     mmap flags and signals), __PS4__ (the marker the PS4 libc++ build keys on), and a patched
#     link.x that keeps prioritised constructors and per-symbol data sections.
#
#   cmake -DCMAKE_TOOLCHAIN_FILE=/work/cmake/ps4-eden.cmake ...

set(OPENORBIS "$ENV{OPENORBIS}")
if (NOT OPENORBIS)
    set(OPENORBIS /opt/pacbrew/ps4/openorbis)
endif ()
set(PS4_SYSROOT "$ENV{PS4_SYSROOT}")
if (NOT PS4_SYSROOT)
    set(PS4_SYSROOT /build/sysroot)
endif ()
set(PS4_MESA_BUILD "$ENV{PS4_MESA_BUILD}")
if (NOT PS4_MESA_BUILD)
    set(PS4_MESA_BUILD /build/build/mesa)
endif ()
set(CMAKE_TRY_COMPILE_PLATFORM_VARIABLES OPENORBIS PS4_SYSROOT PS4_MESA_BUILD)
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/modules")

set(CMAKE_SYSTEM_NAME OpenOrbis)
set(CMAKE_SYSTEM_PROCESSOR x86_64)
set(CMAKE_SYSTEM_VERSION 12)
set(CMAKE_CROSSCOMPILING ON)
set(CMAKE_SIZEOF_VOID_P 8)
set(PS4 TRUE)

set(CMAKE_C_COMPILER clang-21)
set(CMAKE_CXX_COMPILER clang++-21)
set(CMAKE_ASM_COMPILER clang-21)
set(CMAKE_LINKER ld.lld-21 CACHE FILEPATH "")
set(CMAKE_AR llvm-ar-21 CACHE FILEPATH "")
set(CMAKE_RANLIB llvm-ranlib-21 CACHE FILEPATH "")
set(CMAKE_STRIP llvm-strip-21 CACHE FILEPATH "")
set(CMAKE_C_COMPILER_TARGET x86_64-pc-freebsd12-elf)
set(CMAKE_CXX_COMPILER_TARGET x86_64-pc-freebsd12-elf)
set(CMAKE_ASM_COMPILER_TARGET x86_64-pc-freebsd12-elf)

# Eden's PS4 CMake (OpenOrbis.cmake: ${CMAKE_SYSROOT}/bin/create-fself) and its FFmpeg configure
# (${CMAKE_SYSROOT}/usr/include) expect the sysroot to be the OpenOrbis prefix.
set(CMAKE_SYSROOT ${OPENORBIS})
set(CMAKE_FIND_ROOT_PATH ${OPENORBIS} ${OPENORBIS}/usr ${PS4_SYSROOT})
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)
set(BUILD_SHARED_LIBS OFF CACHE INTERNAL "No shared libraries on the PS4")
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
# Compiler checks link full PS4 executables otherwise.
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

set(_ps4_flags "-D__OPENORBIS__ -D__PS4__ -D__ORBIS__ -DPS4 -D_GNU_SOURCE=1 -D__BSD_VISIBLE -D_BSD_SOURCE \
 -fPIC -funwind-tables -femulated-tls -march=btver2 -mtune=btver2 \
 -isysroot ${OPENORBIS} -isystem ${PS4_SYSROOT}/libc-overlay -isystem ${OPENORBIS}/include \
 -I${OPENORBIS}/usr/include")
set(CMAKE_C_FLAGS_INIT "${_ps4_flags}")
set(CMAKE_ASM_FLAGS_INIT "${_ps4_flags}")
# -U__FreeBSD__ for C++ only: OpenOrbis' own libc++ <__config> undefines it, so with the stock
# toolchain C++ never sees it while C does, and Eden's PS4 branch (and its dependency patches,
# e.g. Boost.Asio/Process) are written against that. Our libc++ 21 keys on __PS4__ instead.
set(CMAKE_CXX_FLAGS_INIT "-nostdinc++ -isystem ${PS4_SYSROOT}/include/c++/v1 ${_ps4_flags} -U__FreeBSD__")

# Patched copy of link.x (see ps4-love-style.cmake in DolphinPS4 for why each rule matters).
set(PS4_LINK_SCRIPT "${CMAKE_BINARY_DIR}/ps4-link.x")
if (NOT EXISTS "${PS4_LINK_SCRIPT}")
    file(READ "${OPENORBIS}/link.x" _ld)
    string(REPLACE "*(.init_array);" "KEEP(*(SORT_BY_INIT_PRIORITY(.init_array.*))); KEEP(*(.init_array));" _ld "${_ld}")
    foreach (rule ".data" ".tdata" ".tbss")
        string(REPLACE "*(${rule})" "*(${rule} ${rule}.*)" _ld "${_ld}")
    endforeach ()
    file(WRITE "${PS4_LINK_SCRIPT}" "${_ld}")
endif ()

# ld.lld is called directly (as both PacBrew's ps4.cmake and Eden's build.sh do).
foreach (lang C CXX ASM)
    set(CMAKE_${lang}_LINKER_WRAPPER_FLAG "")
endforeach ()
set(CMAKE_EXE_LINKER_FLAGS_INIT "-m elf_x86_64 -pie --eh-frame-hdr --script ${PS4_LINK_SCRIPT} \
 -L${PS4_SYSROOT}/lib -L${OPENORBIS}/lib -L${OPENORBIS}/usr/lib \
 -u __cxa_thread_atexit_impl -u getentropy \
 --wrap=malloc --wrap=free --wrap=calloc --wrap=realloc --wrap=memalign --wrap=__memalign \
 --wrap=pthread_once --wrap=mmap --wrap=clock_gettime")
# -u: libc++abi references __cxa_thread_atexit_impl and getentropy weakly, which doesn't pull the
# archive members that define them (Eden's ps4sup), so they stayed imports and create-fself
# refused the ELF.
# --wrap: DolphinPS4's runtime (eden/externals/ps4sup/dps4/ps4_runtime.cpp): the malloc family on
# its own heap (the OpenOrbis libc heap fails on retail consoles), pthread_once (the kernel writes
# 16 bytes into the headers' 4-byte pthread_once_t), mmap (large and executable mappings) and
# clock_gettime (CLOCK_MONOTONIC from the TSC).

# DolphinPS4's crt1 (ps4_crt1.S): OpenOrbis' crt1.o with libc's malloc-replacement table filled
# in, so Sony's system modules allocate from the app heap instead of the ~12.7 MiB system heap.
set(PS4_CRT1_SRC "${CMAKE_CURRENT_LIST_DIR}/../eden/externals/ps4sup/dps4/ps4_crt1.S")
set(PS4_CRT1 "${CMAKE_BINARY_DIR}/ps4-crt1-malloc-replace.o")
if (NOT EXISTS "${PS4_CRT1}" AND EXISTS "${PS4_CRT1_SRC}")
    execute_process(COMMAND clang-21 -target x86_64-pc-freebsd12-elf -c "${PS4_CRT1_SRC}" -o "${PS4_CRT1}"
                    RESULT_VARIABLE _ps4_crt1_result)
    if (NOT _ps4_crt1_result EQUAL 0)
        message(FATAL_ERROR "assembling ${PS4_CRT1_SRC} failed: ${_ps4_crt1_result}")
    endif ()
endif ()
if (NOT EXISTS "${PS4_CRT1}")
    set(PS4_CRT1 "${OPENORBIS}/lib/crt1.o")
endif ()
# Library paths again as CMAKE_<LANG>_LINK_FLAGS: Eden's FFmpeg configure links its test programs
# with ld.lld and only these flags.
set(CMAKE_C_LINK_FLAGS "-L${PS4_SYSROOT}/lib -L${OPENORBIS}/lib -L${OPENORBIS}/usr/lib")
set(CMAKE_CXX_LINK_FLAGS "${CMAKE_C_LINK_FLAGS}")
set(_ps4_libs "-lkernel -lc -lSceLibcInternal -lSceUserService -lSceSysmodule -lSceNet -lSceRandom \
 ${OPENORBIS}/lib/libclang_rt.builtins-x86_64.a")
set(_ps4_cxx_libs "${PS4_SYSROOT}/lib/libc++.a ${PS4_SYSROOT}/lib/libc++abi.a ${OPENORBIS}/lib/libunwind.a")
set(CMAKE_C_LINK_EXECUTABLE
    "<CMAKE_LINKER> -o <TARGET> <CMAKE_C_LINK_FLAGS> <LINK_FLAGS> ${PS4_CRT1} ${OPENORBIS}/lib/crti.o \
     --start-group <OBJECTS> <LINK_LIBRARIES> ${_ps4_libs} --end-group ${OPENORBIS}/lib/crtn.o")
set(CMAKE_CXX_LINK_EXECUTABLE
    "<CMAKE_LINKER> -o <TARGET> <CMAKE_CXX_LINK_FLAGS> <LINK_FLAGS> ${PS4_CRT1} ${OPENORBIS}/lib/crti.o \
     --start-group <OBJECTS> <LINK_LIBRARIES> ${_ps4_cxx_libs} ${_ps4_libs} --end-group ${OPENORBIS}/lib/crtn.o")

# Mesa RADV for the PS4 (scripts/build-mesa.sh), linked as whole archives: the Vulkan dispatch
# tables refer to entry points through weak symbols, which don't pull archive members.
set(PS4_MESA_LIBS
    src/amd/vulkan/libvulkan_radeon.a src/amd/common/libamd_common.a src/amd/compiler/libaco.a
    src/amd/addrlib/libaddrlib.a src/vulkan/util/libvulkan_util.a src/compiler/spirv/libvtn.a
    src/compiler/nir/libnir.a src/compiler/libcompiler.a src/util/libxmlconfig.a
    src/util/libmesa_util.a src/util/libmesa_util_simd.a src/util/libmesa_util_clflush.a
    src/util/libmesa_util_clflushopt.a src/util/libparson.a src/util/blake3/libblake3.a
    src/c11/impl/libmesa_util_c11.a)
list(TRANSFORM PS4_MESA_LIBS PREPEND "${PS4_MESA_BUILD}/")

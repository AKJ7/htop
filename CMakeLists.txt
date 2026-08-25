cmake_minimum_required(VERSION 3.16)

# CMake equivalent of configure.ac for htop 3.6.0-dev.
#
# The original Autoconf file performs feature detection at configure time.
# This CMake version maps those options to CMake cache variables and
# find_library()/find_path()/check_*() tests.
#
# Build:
#   cmake -S . -B build
#   cmake --build build
#
# Examples:
#   cmake -S . -B build -DHTOP_UNICODE=ON
#   cmake -S . -B build -DHTOP_HWLOC=ON
#   cmake -S . -B build -DHTOP_BACKTRACE=unwind-ptrace
#   cmake -S . -B build -DHTOP_DEMANGLING=libiberty
#   cmake -S . -B build -DHTOP_STATIC=ON

project(htop VERSION 3.6.0 LANGUAGES C)

include(CheckCSourceCompiles)
include(CheckCSourceRuns)
include(CheckFunctionExists)
include(CheckIncludeFile)
include(CheckLibraryExists)
include(GNUInstallDirs)

# ---------------------------------------------------------------------------
# Options corresponding to configure.ac
# ---------------------------------------------------------------------------

option(HTOP_STATIC "Build a static htop binary" OFF)
option(HTOP_PCP "Build the PCP variant" OFF)
option(HTOP_UNICODE "Enable Unicode support" ON)
set(HTOP_CURSES "auto" CACHE STRING "Curses package to use (auto, ncursesw6, ncursesw5, ncursesw, ncurses6, ncurses5, ncurses, ...)")
set_property(CACHE HTOP_CURSES PROPERTY STRINGS auto ncursesw6 ncursesw5 ncursesw ncurses6 ncurses5 ncurses)

set(HTOP_AFFINITY "check" CACHE STRING "Affinity support: check, yes, or no")
set_property(CACHE HTOP_AFFINITY PROPERTY STRINGS check yes no)

set(HTOP_BACKTRACE "no" CACHE STRING "Backtrace backend: no, check, yes, unwind-ptrace")
set_property(CACHE HTOP_BACKTRACE PROPERTY STRINGS no check yes unwind-ptrace)

set(HTOP_LIBUNWIND "check" CACHE STRING "Use libunwind: check, yes, or no")
set_property(CACHE HTOP_LIBUNWIND PROPERTY STRINGS check yes no)

set(HTOP_DEMANGLING "check" CACHE STRING "Demangling backend: check, yes, no, libiberty, libdemangle")
set_property(CACHE HTOP_DEMANGLING PROPERTY STRINGS check yes no libiberty libdemangle)

option(HTOP_HWLOC "Enable hwloc CPU affinity support" OFF)

set(HTOP_CONFIG "/.config" CACHE PATH "Configuration path")
set(HTOP_PROC "/proc" CACHE PATH "Linux proc filesystem path")

set(HTOP_CAPABILITIES "check" CACHE STRING "Linux capabilities: check, yes, or no")
set_property(CACHE HTOP_CAPABILITIES PROPERTY STRINGS check yes no)

set(HTOP_DELAYACCT "check" CACHE STRING "Linux delay accounting: check, yes, or no")
set_property(CACHE HTOP_DELAYACCT PROPERTY STRINGS check yes no)

set(HTOP_SENSORS "check" CACHE STRING "libsensors: check, yes, or no")
set_property(CACHE HTOP_SENSORS PROPERTY STRINGS check yes no)

option(HTOP_WERROR "Treat compiler warnings as errors" OFF)
option(HTOP_DEBUG "Enable maximum debug information and internal checks" OFF)

# ---------------------------------------------------------------------------
# Platform detection
# ---------------------------------------------------------------------------

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    set(HTOP_PLATFORM linux)
elseif(CMAKE_SYSTEM_NAME STREQUAL "FreeBSD")
    set(HTOP_PLATFORM freebsd)
elseif(CMAKE_SYSTEM_NAME STREQUAL "NetBSD")
    set(HTOP_PLATFORM netbsd)
elseif(CMAKE_SYSTEM_NAME STREQUAL "OpenBSD")
    set(HTOP_PLATFORM openbsd)
elseif(CMAKE_SYSTEM_NAME STREQUAL "DragonFly")
    set(HTOP_PLATFORM dragonflybsd)
elseif(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    set(HTOP_PLATFORM darwin)
elseif(CMAKE_SYSTEM_NAME STREQUAL "SunOS")
    set(HTOP_PLATFORM solaris)
else()
    set(HTOP_PLATFORM unsupported)
endif()

message(STATUS "htop platform: ${HTOP_PLATFORM}")

# ---------------------------------------------------------------------------
# Compiler
# ---------------------------------------------------------------------------

if(NOT CMAKE_C_COMPILER_ID MATCHES "GNU|Clang|AppleClang")
    message(WARNING "The original configure script targets a C99 compiler; "
                    "your compiler is ${CMAKE_C_COMPILER_ID}.")
endif()

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

# Original AM_CFLAGS.
set(HTOP_WARNING_FLAGS
    -Wall
    -Wcast-align
    -Wcast-qual
    -Wextra
    -Wfloat-equal
    -Wformat=2
    -Winit-self
    -Wmissing-format-attribute
    -Wmissing-noreturn
    -Wmissing-prototypes
    -Wpointer-arith
    -Wshadow
    -Wstrict-prototypes
    -Wundef
    -Wunused
    -Wwrite-strings
)

if(HTOP_WERROR)
    list(APPEND HTOP_WARNING_FLAGS -Werror)
endif()

if(HTOP_DEBUG)
    add_compile_options(-ggdb3)
else()
    add_compile_definitions(NDEBUG)
endif()

# ---------------------------------------------------------------------------
# Platform-independent sources
#
# Makefile.am is normally responsible for this list. Keep the source
# discovery here intentionally explicit enough to avoid compiling the
# platform implementations for the wrong OS.
# ---------------------------------------------------------------------------

set(HTOP_COMMON_SOURCES
    Action.c
    Affinity.c
    AffinityPanel.c
    AvailableColumnsPanel.c
    AvailableMetersPanel.c
    BatteryMeter.c
    CategoriesPanel.c
    ColorsPanel.c
    ColumnsPanel.c
    CommandLine.c
    CommandScreen.c
    CPUMeter.c
    CRT.c
    DateTimeMeter.c
    DiskIOMeter.c
    DisplayOptionsPanel.c
    DynamicColumn.c
    DynamicMeter.c
    DynamicScreen.c
    EnvScreen.c
    FileDescriptorMeter.c
    FunctionBar.c
    Hashtable.c
    Header.c
    HeaderOptionsPanel.c
    HostnameMeter.c
    History.c
    IncSet.c
    InfoScreen.c
    LineEditor.c
    ListItem.c
    LoadAverageMeter.c
    Machine.c
    MainPanel.c
    MemoryMeter.c
    MemorySwapMeter.c
    Meter.c
    MetersPanel.c
    NetworkIOMeter.c
    Object.c
    OpenFilesScreen.c
    OptionItem.c
    Panel.c
    Process.c
    ProcessLocksScreen.c
    ProcessTable.c
    Row.c
    RichString.c
    Scheduling.c
    ScreenManager.c
    ScreensPanel.c
    ScreenTabsPanel.c
    Settings.c
    SignalsPanel.c
    SwapMeter.c
    SysArchMeter.c
    Table.c
    TasksMeter.c
    TraceScreen.c
    UptimeMeter.c
    UsersTable.c
    Vector.c
    XUtils.c
)

set(HTOP_COMMON_HEADERS
    Action.h
    Affinity.h
    AffinityPanel.h
    AvailableColumnsPanel.h
    AvailableMetersPanel.h
    BatteryMeter.h
    CPUMeter.h
    CRT.h
    CategoriesPanel.h
    ColorsPanel.h
    ColumnsPanel.h
    CommandLine.h
    CommandScreen.h
    DateTimeMeter.h
    Debug.h
    DiskIOMeter.h
    DisplayOptionsPanel.h
    DynamicColumn.h
    DynamicMeter.h
    DynamicScreen.h
    EnvScreen.h
    FileDescriptorMeter.h
    FunctionBar.h
    GPUMeter.h
    Hashtable.h
    Header.h
    HeaderLayout.h
    HeaderOptionsPanel.h
    HostnameMeter.h
    History.h
    IncSet.h
    InfoScreen.h
    LineEditor.h
    ListItem.h
    LoadAverageMeter.h
    Machine.h
    Macros.h
    MainPanel.h
    MemoryMeter.h
    MemorySwapMeter.h
    Meter.h
    MeterMode.h
    MetersPanel.h
    NetworkIOMeter.h
    Object.h
    OpenFilesScreen.h
    OptionItem.h
    Panel.h
    Process.h
    ProcessLocksScreen.h
    ProcessTable.h
    ProvideCurses.h
    ProvideTerm.h
    RichString.h
    Row.h
    RowField.h
    Scheduling.h
    ScreenManager.h
    ScreensPanel.h
    ScreenTabsPanel.h
    Settings.h
    SignalsPanel.h
    SwapMeter.h
    SysArchMeter.h
    Table.h
    TasksMeter.h
    TraceScreen.h
    UptimeMeter.h
    UsersTable.h
    Vector.h
    XUtils.h
    generic/ProvideDemangle.h
)

set(HTOP_SOURCES ${HTOP_COMMON_SOURCES})
set(HTOP_HEADERS ${HTOP_COMMON_HEADERS})

# ---------------------------------------------------------------------------
# Platform-specific source selection
# ---------------------------------------------------------------------------

if(HTOP_PLATFORM STREQUAL "linux")
    set(HTOP_PLATFORM_SOURCES
        GPUMeter.c
        generic/gettime.c
        generic/hostname.c
        generic/uname.c
        linux/CGroupUtils.c
        linux/Compat.c
        linux/GPU.c
        linux/HugePageMeter.c
        linux/IOPriorityPanel.c
        linux/LibSensors.c
        linux/LinuxMachine.c
        linux/LinuxProcess.c
        linux/LinuxProcessTable.c
        linux/OpenRCMeter.c
        linux/Platform.c
        linux/PressureStallMeter.c
        linux/SELinuxMeter.c
        linux/SystemdMeter.c
        linux/ZramMeter.c
        linux/ZswapMeter.c
        zfs/ZfsArcMeter.c
        zfs/ZfsCompressedArcMeter.c
    )
    set(HTOP_PLATFORM_INCLUDE_DIR linux)

    set(HTOP_PLATFORM_HEADERS
        generic/gettime.h generic/hostname.h generic/uname.h
        linux/CGroupUtils.h linux/Compat.h linux/GPU.h
        linux/HugePageMeter.h linux/IOPriority.h linux/IOPriorityPanel.h
        linux/LibSensors.h linux/LinuxMachine.h linux/LinuxProcess.h
        linux/LinuxProcessTable.h linux/OpenRCMeter.h linux/Platform.h
        linux/PressureStallMeter.h linux/ProcessField.h
        linux/SELinuxMeter.h linux/SystemdMeter.h linux/ZramMeter.h
        linux/ZramStats.h linux/ZswapMeter.h linux/ZswapStats.h
        zfs/ZfsArcMeter.h zfs/ZfsArcStats.h zfs/ZfsCompressedArcMeter.h
    )
elseif(HTOP_PLATFORM STREQUAL "freebsd")
    set(HTOP_PLATFORM_SOURCES
        freebsd/Platform.c freebsd/FreeBSDMachine.c
        freebsd/FreeBSDProcessTable.c freebsd/FreeBSDProcess.c
        generic/fdstat_sysctl.c generic/gettime.c generic/hostname.c
        generic/openzfs_sysctl.c generic/uname.c
        zfs/ZfsArcMeter.c zfs/ZfsCompressedArcMeter.c
    )
    set(HTOP_PLATFORM_INCLUDE_DIR freebsd)
elseif(HTOP_PLATFORM STREQUAL "darwin")
    set(HTOP_PLATFORM_SOURCES
        GPUMeter.c
        darwin/Platform.c darwin/PlatformHelpers.c
        darwin/DarwinMachine.c darwin/DarwinProcess.c
        darwin/DarwinProcessTable.c
        generic/fdstat_sysctl.c generic/gettime.c generic/hostname.c
        generic/openzfs_sysctl.c generic/uname.c
        zfs/ZfsArcMeter.c zfs/ZfsCompressedArcMeter.c
    )
    set(HTOP_PLATFORM_INCLUDE_DIR darwin)
elseif(HTOP_PLATFORM STREQUAL "solaris")
    set(HTOP_PLATFORM_SOURCES
        generic/gettime.c generic/hostname.c generic/uname.c
        solaris/Platform.c solaris/SolarisMachine.c
        solaris/SolarisProcess.c solaris/SolarisProcessTable.c
        zfs/ZfsArcMeter.c zfs/ZfsCompressedArcMeter.c
    )
    set(HTOP_PLATFORM_INCLUDE_DIR solaris)
elseif(HTOP_PLATFORM STREQUAL "unsupported")
    set(HTOP_PLATFORM_SOURCES
        generic/gettime.c
        unsupported/Platform.c
        unsupported/UnsupportedMachine.c
        unsupported/UnsupportedProcess.c
        unsupported/UnsupportedProcessTable.c
    )
    set(HTOP_PLATFORM_INCLUDE_DIR unsupported)
elseif(HTOP_PLATFORM STREQUAL "netbsd")
    set(HTOP_PLATFORM_SOURCES
        generic/gettime.c generic/hostname.c generic/uname.c
        netbsd/Platform.c netbsd/NetBSDMachine.c
        netbsd/NetBSDProcess.c netbsd/NetBSDProcessTable.c
    )
    set(HTOP_PLATFORM_INCLUDE_DIR netbsd)
elseif(HTOP_PLATFORM STREQUAL "openbsd")
    set(HTOP_PLATFORM_SOURCES
        generic/gettime.c generic/hostname.c generic/uname.c
        openbsd/Platform.c openbsd/OpenBSDMachine.c
        openbsd/OpenBSDProcess.c openbsd/OpenBSDProcessTable.c
    )
    set(HTOP_PLATFORM_INCLUDE_DIR openbsd)
elseif(HTOP_PLATFORM STREQUAL "dragonflybsd")
    set(HTOP_PLATFORM_SOURCES
        generic/gettime.c generic/hostname.c generic/uname.c
        dragonflybsd/Platform.c dragonflybsd/DragonFlyBSDMachine.c
        dragonflybsd/DragonFlyBSDProcess.c
        dragonflybsd/DragonFlyBSDProcessTable.c
    )
    set(HTOP_PLATFORM_INCLUDE_DIR dragonflybsd)
endif()

list(APPEND HTOP_SOURCES ${HTOP_PLATFORM_SOURCES})

# ---------------------------------------------------------------------------
# PCP build
# ---------------------------------------------------------------------------

if(HTOP_PCP)
    if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/pcp-htop.c")
        message(FATAL_ERROR "HTOP_PCP=ON requires pcp-htop.c")
    endif()

    find_path(PCP_INCLUDE_DIR pcp/pmapi.h)
    find_library(PCP_LIBRARY NAMES pcp)

    if(NOT PCP_INCLUDE_DIR OR NOT PCP_LIBRARY)
        message(FATAL_ERROR "PCP support requested, but PCP headers/library were not found")
    endif()

    list(REMOVE_ITEM HTOP_SOURCES ${HTOP_PLATFORM_SOURCES})
    list(APPEND HTOP_SOURCES
        linux/CGroupUtils.c
        linux/PressureStallMeter.c
        linux/ZramMeter.c
        pcp/Instance.c
        pcp/InDomTable.c
        pcp/Metric.c
        pcp/Platform.c
        pcp/PCPDynamicColumn.c
        pcp/PCPDynamicMeter.c
        pcp/PCPDynamicScreen.c
        pcp/PCPMachine.c
        pcp/PCPProcess.c
        pcp/PCPProcessTable.c
        zfs/ZfsArcMeter.c
        zfs/ZfsCompressedArcMeter.c
    )
    list(APPEND HTOP_SOURCES pcp-htop.c)
    set(HTOP_PLATFORM_INCLUDE_DIR pcp)
    add_compile_definitions(HTOP_PCP=1)
endif()

# ---------------------------------------------------------------------------
# Required generic headers
# ---------------------------------------------------------------------------

foreach(header stdlib.h string.h strings.h sys/param.h sys/time.h
               sys/utsname.h unistd.h)
    string(MAKE_C_IDENTIFIER "${header}" header_var)
    check_include_file("${header}" "HTOP_HAVE_${header_var}")
    if(NOT HTOP_HAVE_${header_var})
        message(FATAL_ERROR "Required header ${header} was not found")
    endif()
endforeach()

# ---------------------------------------------------------------------------
# Curses / Unicode
# ---------------------------------------------------------------------------

set(CURSES_NEED_NCURSES TRUE)
if(HTOP_UNICODE)
    set(CURSES_NEED_WIDE TRUE)
endif()

find_package(Curses REQUIRED)

if(HTOP_UNICODE)
    if(NOT CURSES_HAVE_NCURSES_WIDE)
        # CMake's FindCurses may not expose this variable on older versions.
        # Prefer a wide-character curses library when explicitly requested.
        find_library(NCURSESW_LIBRARY NAMES ncursesw ncursesw6 ncursesw5)
        if(NOT NCURSESW_LIBRARY)
            message(FATAL_ERROR "Unicode support requested, but wide ncurses was not found")
        endif()
        set(CURSES_LIBRARIES ${NCURSESW_LIBRARY} ${CURSES_LIBRARIES})
    endif()
    add_compile_definitions(HAVE_UNICODE)
endif()

# ---------------------------------------------------------------------------
# Optional dependency helpers
# ---------------------------------------------------------------------------

find_package(PkgConfig QUIET)

# libcap
set(HAVE_LIBCAP OFF)
if(HTOP_PLATFORM STREQUAL "linux" AND NOT HTOP_CAPABILITIES STREQUAL "no")
    find_path(LIBCAP_INCLUDE_DIR sys/capability.h)
    find_library(LIBCAP_LIBRARY NAMES cap)

    if(LIBCAP_INCLUDE_DIR AND LIBCAP_LIBRARY)
        set(HAVE_LIBCAP ON)
    elseif(HTOP_CAPABILITIES STREQUAL "yes")
        message(FATAL_ERROR "libcap requested but sys/capability.h/libcap were not found")
    endif()
endif()

if(HAVE_LIBCAP)
    add_compile_definitions(HAVE_CAPABILITIES)
endif()

# hwloc
if(HTOP_HWLOC)
    if(PkgConfig_FOUND)
        pkg_check_modules(HWLOC QUIET hwloc)
    endif()
    if(NOT HWLOC_FOUND)
        find_path(HWLOC_INCLUDE_DIR hwloc.h)
        find_library(HWLOC_LIBRARY NAMES hwloc)
    endif()
    if(NOT HWLOC_FOUND AND (NOT HWLOC_INCLUDE_DIR OR NOT HWLOC_LIBRARY))
        message(FATAL_ERROR "hwloc requested but was not found")
    endif()
    add_compile_definitions(HAVE_LIBHWLOC)
endif()

# libsensors
set(HAVE_SENSORS OFF)
if(NOT HTOP_SENSORS STREQUAL "no")
    find_path(SENSORS_INCLUDE_DIR sensors/sensors.h)
    find_library(SENSORS_LIBRARY NAMES sensors)
    if(SENSORS_INCLUDE_DIR)
        set(HAVE_SENSORS ON)
    elseif(HTOP_SENSORS STREQUAL "yes")
        message(FATAL_ERROR "libsensors requested but sensors/sensors.h was not found")
    endif()
endif()

if(HAVE_SENSORS OR HTOP_PLATFORM STREQUAL "freebsd")
    add_compile_definitions(BUILD_WITH_CPU_TEMP)
endif()

# libunwind / backtrace
set(HAVE_LIBUNWIND OFF)
set(HAVE_LIBUNWIND_PTRACE OFF)

if(NOT HTOP_BACKTRACE STREQUAL "no" OR NOT HTOP_LIBUNWIND STREQUAL "no")
    find_path(UNWIND_INCLUDE_DIR libunwind.h)
    find_library(UNWIND_LIBRARY NAMES unwind)

    if(UNWIND_INCLUDE_DIR AND UNWIND_LIBRARY)
        set(HAVE_LIBUNWIND ON)
    endif()

    if(HTOP_BACKTRACE STREQUAL "unwind-ptrace")
        find_path(UNWIND_PTRACE_INCLUDE_DIR libunwind-ptrace.h)
        find_library(UNWIND_PTRACE_LIBRARY NAMES unwind-ptrace)
        if(NOT UNWIND_PTRACE_LIBRARY)
            message(FATAL_ERROR "unwind-ptrace requested but libunwind-ptrace was not found")
        endif()
        set(HAVE_LIBUNWIND_PTRACE ON)
    endif()

    if(HTOP_BACKTRACE STREQUAL "yes" AND NOT HAVE_LIBUNWIND)
        message(FATAL_ERROR "backtrace support requested but libunwind was not found")
    endif()
endif()

if(HTOP_BACKTRACE STREQUAL "unwind-ptrace" AND NOT HAVE_LIBUNWIND)
    message(FATAL_ERROR "unwind-ptrace requires libunwind")
endif()

# Add the generic fallback backtrace library when libunwind is not used.
if(HTOP_BACKTRACE STREQUAL "yes" OR HTOP_BACKTRACE STREQUAL "check")
    find_library(EXECINFO_LIBRARY NAMES execinfo)
endif()

# Demangling
set(HAVE_DEMANGLING OFF)
if(NOT HTOP_DEMANGLING STREQUAL "no")
    find_library(IBERTY_LIBRARY NAMES iberty)
    find_library(DEMANGLE_LIBRARY NAMES demangle)
    find_path(DEMANGLE_INCLUDE_DIR demangle.h PATH_SUFFIXES libiberty)
    find_path(LIBIBERTY_DEMANGLE_INCLUDE_DIR libiberty/demangle.h)

    if(HTOP_DEMANGLING STREQUAL "libiberty")
        if(NOT IBERTY_LIBRARY)
            message(FATAL_ERROR "libiberty requested but libiberty was not found")
        endif()
        set(HAVE_DEMANGLING ON)
        set(DEMANGLE_BACKEND iberty)
    elseif(HTOP_DEMANGLING STREQUAL "libdemangle")
        if(NOT DEMANGLE_LIBRARY)
            message(FATAL_ERROR "libdemangle requested but libdemangle was not found")
        endif()
        set(HAVE_DEMANGLING ON)
        set(DEMANGLE_BACKEND demangle)
    elseif(HTOP_DEMANGLING STREQUAL "yes" OR HTOP_DEMANGLING STREQUAL "check")
        if(IBERTY_LIBRARY AND (DEMANGLE_INCLUDE_DIR OR LIBIBERTY_DEMANGLE_INCLUDE_DIR))
            set(HAVE_DEMANGLING ON)
            set(DEMANGLE_BACKEND iberty)
        elseif(DEMANGLE_LIBRARY)
            set(HAVE_DEMANGLING ON)
            set(DEMANGLE_BACKEND demangle)
        elseif(HTOP_DEMANGLING STREQUAL "yes")
            message(FATAL_ERROR "No usable demangling backend was found")
        endif()
    endif()
endif()

# ---------------------------------------------------------------------------
# Delay accounting / libnl-3
# ---------------------------------------------------------------------------

set(HAVE_DELAYACCT OFF)
if(HTOP_PLATFORM STREQUAL "linux" AND NOT HTOP_DELAYACCT STREQUAL "no"
   AND NOT HTOP_STATIC)

    if(PkgConfig_FOUND)
        pkg_check_modules(LIBNL3 QUIET libnl-3.0)
    endif()

    find_path(LIBNL3_INCLUDE_DIR netlink/attr.h
              HINTS ${LIBNL3_INCLUDE_DIRS}
              PATH_SUFFIXES libnl3)
    find_library(LIBNL3_LIBRARY NAMES nl-3 HINTS ${LIBNL3_LIBRARY_DIRS})
    find_library(LIBNL3_GENL_LIBRARY NAMES nl-genl-3 HINTS ${LIBNL3_LIBRARY_DIRS})

    if(LIBNL3_INCLUDE_DIR AND LIBNL3_LIBRARY AND LIBNL3_GENL_LIBRARY)
        set(HAVE_DELAYACCT ON)
    elseif(HTOP_DELAYACCT STREQUAL "yes")
        message(FATAL_ERROR "delay accounting requested but libnl-3/libnl-genl-3 were not found")
    endif()
endif()

if(HAVE_DELAYACCT)
    add_compile_definitions(HAVE_DELAYACCT)
endif()

# ---------------------------------------------------------------------------
# Generated configuration header
# ---------------------------------------------------------------------------

configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/config.h.in"
    "${CMAKE_CURRENT_BINARY_DIR}/config.h"
)

# The project may not ship a config.h.in when this CMake file is used alone.
# Generate the equivalent header if necessary.
if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/config.h.in")
    file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/config.h"
"#pragma once
/* Generated by CMake. */
")
endif()

add_compile_definitions(
    _XOPEN_SOURCE_EXTENDED=1
    SYSCONFDIR="${CMAKE_INSTALL_FULL_SYSCONFDIR}"
    CONFIGDIR="${HTOP_CONFIG}"
    PROCDIR="${HTOP_PROC}"
#    COPYRIGHT="(C) 2004-2019 Hisham Muhammad. (C) 2020-2026 htop dev team."
)

if(HTOP_PLATFORM STREQUAL "linux")
    add_compile_definitions(HTOP_LINUX)
elseif(HTOP_PLATFORM STREQUAL "freebsd")
    add_compile_definitions(HTOP_FREEBSD)
elseif(HTOP_PLATFORM STREQUAL "netbsd")
    add_compile_definitions(HTOP_NETBSD)
elseif(HTOP_PLATFORM STREQUAL "openbsd")
    add_compile_definitions(HTOP_OPENBSD)
elseif(HTOP_PLATFORM STREQUAL "dragonflybsd")
    add_compile_definitions(HTOP_DRAGONFLYBSD)
elseif(HTOP_PLATFORM STREQUAL "darwin")
    add_compile_definitions(HTOP_DARWIN)
elseif(HTOP_PLATFORM STREQUAL "solaris")
    add_compile_definitions(HTOP_SOLARIS)
elseif(HTOP_PLATFORM STREQUAL "unsupported")
    add_compile_definitions(HTOP_UNSUPPORTED)
endif()

if(HTOP_STATIC)
    add_compile_definitions(BUILD_STATIC)
endif()

# ---------------------------------------------------------------------------
# Backtrace / demangling sources
# ---------------------------------------------------------------------------

if(HAVE_LIBUNWIND_PTRACE)
    list(APPEND HTOP_SOURCES BacktraceScreen.c generic/UnwindPtrace.c)
    list(APPEND HTOP_HEADERS BacktraceScreen.h generic/UnwindPtrace.h)
    add_compile_definitions(HAVE_BACKTRACE_SCREEN HAVE_LIBUNWIND_PTRACE HAVE_LOCAL_UNWIND)
elseif(HTOP_BACKTRACE STREQUAL "yes" AND HAVE_LIBUNWIND)
    list(APPEND HTOP_SOURCES BacktraceScreen.c generic/UnwindPtrace.c)
    list(APPEND HTOP_HEADERS BacktraceScreen.h generic/UnwindPtrace.h)
    add_compile_definitions(HAVE_BACKTRACE_SCREEN HAVE_LOCAL_UNWIND)
endif()

if(HAVE_DEMANGLING)
    list(APPEND HTOP_SOURCES generic/Demangle.c)
    list(APPEND HTOP_HEADERS generic/Demangle.h)
    add_compile_definitions(HAVE_DEMANGLING)
    if(DEMANGLE_BACKEND STREQUAL "iberty")
        add_compile_definitions(HAVE_LIBIBERTY_CPLUS_DEMANGLE)
    elseif(DEMANGLE_BACKEND STREQUAL "demangle")
        add_compile_definitions(HAVE_LIBDEMANGLE_CPLUS_DEMANGLE)
    endif()
endif()

# ---------------------------------------------------------------------------
# Target
# ---------------------------------------------------------------------------

if(HTOP_PCP)
    set(HTOP_TARGET pcp-htop)
else()
    set(HTOP_TARGET htop)
    list(APPEND HTOP_SOURCES htop.c)
endif()

add_executable(${HTOP_TARGET} ${HTOP_SOURCES} ${HTOP_HEADERS})

target_include_directories(${HTOP_TARGET}
    PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}"
        "${CMAKE_CURRENT_BINARY_DIR}"
        "${CMAKE_CURRENT_SOURCE_DIR}/${HTOP_PLATFORM_INCLUDE_DIR}"
)

target_compile_options(${HTOP_TARGET} PRIVATE ${HTOP_WARNING_FLAGS})

# Dependencies discovered above.
if(CURSES_INCLUDE_DIRS)
    target_include_directories(${HTOP_TARGET} PRIVATE ${CURSES_INCLUDE_DIRS})
endif()
# TODO: Check for validity with math
target_link_libraries(${HTOP_TARGET} PRIVATE ${CURSES_LIBRARIES} m)

if(HAVE_LIBCAP)
    target_include_directories(${HTOP_TARGET} PRIVATE ${LIBCAP_INCLUDE_DIR})
    target_link_libraries(${HTOP_TARGET} PRIVATE ${LIBCAP_LIBRARY})
endif()

if(HTOP_HWLOC)
    if(HWLOC_FOUND)
        target_include_directories(${HTOP_TARGET} PRIVATE ${HWLOC_INCLUDE_DIRS})
        target_link_libraries(${HTOP_TARGET} PRIVATE ${HWLOC_LIBRARIES})
    else()
        target_include_directories(${HTOP_TARGET} PRIVATE ${HWLOC_INCLUDE_DIR})
        target_link_libraries(${HTOP_TARGET} PRIVATE ${HWLOC_LIBRARY})
    endif()
endif()

if(HAVE_SENSORS)
    target_include_directories(${HTOP_TARGET} PRIVATE ${SENSORS_INCLUDE_DIR})
    # The non-static build dynamically loads libsensors, matching configure.ac.
    if(HTOP_STATIC)
        target_link_libraries(${HTOP_TARGET} PRIVATE ${SENSORS_LIBRARY})
    endif()
endif()

if(HAVE_LIBUNWIND)
    target_include_directories(${HTOP_TARGET} PRIVATE ${UNWIND_INCLUDE_DIR})
    target_link_libraries(${HTOP_TARGET} PRIVATE ${UNWIND_LIBRARY})
endif()

if(HAVE_LIBUNWIND_PTRACE)
    target_include_directories(${HTOP_TARGET} PRIVATE ${UNWIND_PTRACE_INCLUDE_DIR})
    target_link_libraries(${HTOP_TARGET} PRIVATE ${UNWIND_PTRACE_LIBRARY})
endif()

if(EXECINFO_LIBRARY)
    target_link_libraries(${HTOP_TARGET} PRIVATE ${EXECINFO_LIBRARY})
endif()

if(HAVE_DEMANGLING)
    if(DEMANGLE_BACKEND STREQUAL "iberty")
        target_link_libraries(${HTOP_TARGET} PRIVATE ${IBERTY_LIBRARY})
    else()
        target_link_libraries(${HTOP_TARGET} PRIVATE ${DEMANGLE_LIBRARY})
    endif()
endif()

if(HAVE_DELAYACCT)
    target_include_directories(${HTOP_TARGET} PRIVATE ${LIBNL3_INCLUDE_DIR} ${LIBNL3_INCLUDE_DIRS})
    target_link_libraries(${HTOP_TARGET} PRIVATE ${LIBNL3_LIBRARY} ${LIBNL3_GENL_LIBRARY})
endif()

if(HTOP_PCP)
    target_include_directories(${HTOP_TARGET} PRIVATE ${PCP_INCLUDE_DIR})
    target_link_libraries(${HTOP_TARGET} PRIVATE ${PCP_LIBRARY})
endif()

# The original Linux build exports symbols for backtrace/dynamic lookup.
if(HTOP_PLATFORM STREQUAL "linux")
    target_link_options(${HTOP_TARGET} PRIVATE -rdynamic)
endif()

if(HTOP_STATIC)
    target_link_options(${HTOP_TARGET} PRIVATE -static)
endif()

if(APPLE)
    target_link_libraries(${HTOP_TARGET} PRIVATE "-framework IOKit" "-framework CoreFoundation")
endif()

# ---------------------------------------------------------------------------
# Installation
# ---------------------------------------------------------------------------

install(TARGETS ${HTOP_TARGET}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/htop.1")
    install(FILES htop.1 DESTINATION ${CMAKE_INSTALL_MANDIR}/man1)
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/htop.desktop")
    install(FILES htop.desktop DESTINATION ${CMAKE_INSTALL_DATADIR}/applications)
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/htop.png")
    install(FILES htop.png DESTINATION ${CMAKE_INSTALL_DATADIR}/pixmaps)
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/htop.svg")
    install(FILES htop.svg
            DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/scalable/apps)
endif()

message(STATUS "")
message(STATUS "htop ${PROJECT_VERSION}")
message(STATUS "  platform:       ${HTOP_PLATFORM}")
message(STATUS "  proc directory: ${HTOP_PROC}")
message(STATUS "  delay accounting: ${HAVE_DELAYACCT}")
message(STATUS "  sensors:         ${HAVE_SENSORS}")
message(STATUS "  capabilities:    ${HAVE_LIBCAP}")
message(STATUS "  unicode:         ${HTOP_UNICODE}")
message(STATUS "  affinity:        ${HTOP_AFFINITY}")
message(STATUS "  backtrace:       ${HTOP_BACKTRACE}")
message(STATUS "  demangling:      ${HTOP_DEMANGLING}")
message(STATUS "  hwloc:            ${HTOP_HWLOC}")
message(STATUS "  debug:            ${HTOP_DEBUG}")
message(STATUS "  static:           ${HTOP_STATIC}")

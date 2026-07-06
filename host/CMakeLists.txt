cmake_minimum_required(VERSION 3.16)
project(rn-layout)
set(CMAKE_CXX_STANDARD 17)

find_package(Qt6 REQUIRED COMPONENTS Core Gui Network)

set(HERMES_SRC   /hermes)
set(HERMES_BUILD /hermes/build)
set(YOGA_SRC     /yoga)
set(YOGA_BUILD   /yoga/build)

# quill sources from the riddle submodule (epfb-re shim + headers).
set(QUILL_SRC ${CMAKE_CURRENT_SOURCE_DIR}/../riddle/quill/src
    CACHE PATH "Path to riddle/quill/src")

# Link-time stub for the vendor engine: provides the two EPFramebuffer
# symbols so libquill.so links without the device's libqsgepaper.so.
# Never deployed — at runtime libquill's rpath resolves the real library
# from /usr/lib/plugins/scenegraph on the device.
add_library(qsgepaper SHARED qsgepaper_stub.cpp)
target_include_directories(qsgepaper PRIVATE ${QUILL_SRC})
target_link_libraries(qsgepaper PRIVATE Qt6::Gui)

# libquill.so: QImage-constructor interposition shim over libqsgepaper plus
# the C ABI (quill_init/quill_buffer/quill_swap). It must come FIRST on
# rn-layout's link line so its interposed QImage symbols win when the vendor
# engine constructs its framebuffers.
add_library(quill SHARED
  ${QUILL_SRC}/epfb.cpp
  quill_host.cpp
)
target_include_directories(quill PRIVATE ${QUILL_SRC})
target_link_libraries(quill PRIVATE Qt6::Gui Qt6::Core qsgepaper dl)

add_executable(rn-layout main.cpp layout.h input.h quill.h)

target_include_directories(rn-layout PRIVATE
  ${HERMES_SRC}/API
  ${HERMES_SRC}/API/jsi
  ${HERMES_SRC}/public
  ${HERMES_SRC}/include
  ${YOGA_SRC}
  ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(rn-layout PRIVATE
  quill
  Qt6::Gui
  Qt6::Network
  ${YOGA_BUILD}/yoga/libyogacore.a
  ${HERMES_BUILD}/lib/libhermesvm.so
  ${HERMES_BUILD}/jsi/libjsi.a
  ${HERMES_BUILD}/lib/VM/libhermesVMRuntime.a
  ${HERMES_BUILD}/lib/VM/Instrumentation/libhermesInstrumentation.a
  ${HERMES_BUILD}/lib/Platform/libhermesPlatform.a
  ${HERMES_BUILD}/lib/Platform/Unicode/libhermesPlatformUnicode.a
  ${HERMES_BUILD}/lib/Support/libhermesSupport.a
  ${HERMES_BUILD}/lib/ADT/libhermesADT.a
  ${HERMES_BUILD}/lib/Regex/libhermesRegex.a
  ${HERMES_BUILD}/public/hermes/Public/libhermesPublic.a
  ${HERMES_BUILD}/external/llvh/lib/Support/libLLVHSupport.a
  ${HERMES_BUILD}/external/llvh/lib/Demangle/libLLVHDemangle.a
  ${HERMES_BUILD}/external/dtoa/libdtoa.a
  pthread dl
)

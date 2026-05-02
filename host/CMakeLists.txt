cmake_minimum_required(VERSION 3.16)
project(rn-layout)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_AUTOMOC ON)

find_package(Qt6 REQUIRED COMPONENTS Quick Network)

set(HERMES_SRC   /hermes)
set(HERMES_BUILD /hermes/build)
set(YOGA_SRC     /yoga)
set(YOGA_BUILD   /yoga/build)

add_executable(rn-layout main.cpp layout.h input.h)

target_include_directories(rn-layout PRIVATE
  ${HERMES_SRC}/API
  ${HERMES_SRC}/API/jsi
  ${HERMES_SRC}/public
  ${HERMES_SRC}/include
  ${YOGA_SRC}
  ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(rn-layout PRIVATE
  Qt6::Quick
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

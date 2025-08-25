cmake_minimum_required(VERSION 3.10)

# Imposta il toolchain VitaSDK se non definito
if(NOT DEFINED CMAKE_TOOLCHAIN_FILE)
  if(DEFINED ENV{VITASDK})
    set(CMAKE_TOOLCHAIN_FILE "$ENV{VITASDK}/share/vita.toolchain.cmake" CACHE PATH "toolchain file")
  else()
    message(FATAL_ERROR "Please define VITASDK environment variable pointing to your SDK path!")
  endif()
endif()

project(FreeSpaceAnalyzer VERSION 1.0 LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Include VitaSDK CMake helpers
include("$ENV{VITASDK}/share/vita.cmake" REQUIRED)

# Info app
set(VITA_APP_NAME "Free Space Analyzer")
set(VITA_TITLEID  "FSA000001")
set(VITA_VERSION  "01.00")

# Include paths
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)

# Sources
file(GLOB SRC
  src/*.c
)

# Executable
add_executable(${PROJECT_NAME} ${SRC})

# Link libraries
find_package(Freetype REQUIRED)

target_link_libraries(${PROJECT_NAME}
  vita2d
  SceDisplay_stub
  SceAppMgr_stub
  SceGxm_stub
  SceCtrl_stub
  SceTouch_stub
  ScePgf_stub
  SceCommonDialog_stub
  SceAppUtil_stub
  ScePower_stub
  SceIofilemgr_stub
  png
  jpeg
  z
  freetype
  m  
)

# Flags per mksfoex
set(VITA_MKSFOEX_FLAGS "-d PARENTAL_LEVEL=1")

# Crea SELF e VPK
vita_create_self(${PROJECT_NAME}.self ${PROJECT_NAME} UNSAFE)

# Crea VPK con assets inclusi
vita_create_vpk(
  ${PROJECT_NAME}.vpk
  ${VITA_TITLEID}
  ${PROJECT_NAME}.self
  VERSION ${VITA_VERSION}
  NAME "${VITA_APP_NAME}"
  FILE assets/reihen.png assets/reihen.png           
  FILE assets/sce_sys/icon0.png sce_sys/icon0.png
  FILE assets/sce_sys/livearea/contents/template.xml sce_sys/livearea/contents/template.xml
  FILE assets/sce_sys/livearea/contents/bg.png sce_sys/livearea/contents/bg.png
  FILE assets/sce_sys/livearea/contents/startup.png sce_sys/livearea/contents/startup.png
)

cmake_minimum_required(VERSION 3.24)
project(SchlongPhysicsSwapper VERSION 1.8.1 LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(NOT DEFINED ENV{COMMONLIB_SSE_FOLDER})
  message(FATAL_ERROR "Set COMMONLIB_SSE_FOLDER to CommonLibSSE-NG")
endif()
include(FetchContent)
FetchContent_Declare(mcp_sdk URL https://github.com/QTR-Modding/SKSE-Menu-Framework-3-Example/archive/refs/heads/master.zip DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
FetchContent_GetProperties(mcp_sdk)
if(NOT mcp_sdk_POPULATED)
  FetchContent_Populate(mcp_sdk)
endif()
set(ENABLE_SKYRIM_SE ON CACHE BOOL "" FORCE)
set(ENABLE_SKYRIM_AE ON CACHE BOOL "" FORCE)
set(ENABLE_SKYRIM_VR ON CACHE BOOL "" FORCE)
set(BUILD_TESTS OFF CACHE BOOL "" FORCE)
add_subdirectory("$ENV{COMMONLIB_SSE_FOLDER}" "${CMAKE_BINARY_DIR}/commonlib")
list(APPEND CMAKE_MODULE_PATH "$ENV{COMMONLIB_SSE_FOLDER}/cmake")
include(CommonLibSSE)
# simpleini ships a CMake config in some vcpkg revisions and is header-only in others,
# so discover it if a config exists and fall back to the include directory if not.
find_package(SimpleIni CONFIG QUIET)
option(SPS_BUILD_API_TEST_PLUGIN "Build the developer-only SPS API smoke-test plugin" OFF)

add_commonlibsse_plugin(${PROJECT_NAME} AUTHOR "snowman12356" SOURCES src/plugin.cpp src/SPSAPI.h)
target_include_directories(${PROJECT_NAME} PRIVATE "${mcp_sdk_SOURCE_DIR}/include")
target_compile_definitions(${PROJECT_NAME} PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
target_precompile_headers(${PROJECT_NAME} PRIVATE src/PCH.h)
if(TARGET SimpleIni::SimpleIni)
  target_link_libraries(${PROJECT_NAME} PRIVATE SimpleIni::SimpleIni)
else()
  target_include_directories(${PROJECT_NAME} PRIVATE
      "${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/include")
endif()

if(SPS_BUILD_API_TEST_PLUGIN)
  add_commonlibsse_plugin(SPSAPITestPlugin
      AUTHOR "snowman12356"
      SOURCES tests/SPSAPITestPlugin.cpp src/SPSAPI.h)
  target_include_directories(SPSAPITestPlugin PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
  target_compile_definitions(SPSAPITestPlugin PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
  target_precompile_headers(SPSAPITestPlugin PRIVATE src/PCH.h)
endif()

install(TARGETS ${PROJECT_NAME} RUNTIME DESTINATION "SKSE/Plugins")
install(FILES
    config/SchlongPhysicsSwapper.ini
    config/CBPCMasterConfig_ZZZ_SchlongPhysicsSwapper.txt
    config/CBPConfig_ZZZ_SchlongPhysicsSwapper.txt
    DESTINATION "SKSE/Plugins"
)
install(FILES scripts/SPS_SexLabBridge.pex DESTINATION "Scripts")
install(FILES scripts/Source/SPS_SexLabBridge.psc DESTINATION "Source/Scripts")
install(DIRECTORY fomod DESTINATION ".")

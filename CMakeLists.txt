cmake_minimum_required(VERSION 3.24)
project(SchlongPhysicsSwapper VERSION 2.0.0 LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(NOT DEFINED ENV{COMMONLIB_SSE_FOLDER})
  message(FATAL_ERROR "Set COMMONLIB_SSE_FOLDER to CommonLibSSE-NG")
endif()
include(FetchContent)
if(POLICY CMP0169)
  # The Menu Framework dependency is used only for its headers. Avoid adding
  # the example plugin's own targets while keeping CMake 4.x configuration quiet.
  cmake_policy(SET CMP0169 OLD)
endif()
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
# SimpleIni ships a CMake config in some vcpkg revisions and is header-only in
# others, so use the package target when available and otherwise use vcpkg's
# include directory directly.
find_package(SimpleIni CONFIG QUIET)
option(SPS_BUILD_API_TEST_PLUGIN "Build the developer-only SPS API smoke-test plugin" OFF)
option(SPS_BUILD_CORE_TESTS "Build the game-independent SPS core tests" ON)

add_library(SPSCore STATIC
    src/api/ExternalControlRegistry.cpp
    src/api/ExternalControlRegistry.h
    src/diagnostics/ActivityLog.cpp
    src/diagnostics/ActivityLog.h
    src/controllers/ActorContext.h
    src/controllers/PhysicsOwnershipController.cpp
    src/controllers/PhysicsOwnershipController.h
    src/controllers/PositionController.cpp
    src/controllers/PositionController.h
    src/controllers/RecoveryController.cpp
    src/controllers/RecoveryController.h
    src/PhysicsDecision.cpp
    src/PhysicsDecision.h
    src/Settings.h
    src/SettingsStore.cpp
    src/SettingsStore.h)
target_include_directories(SPSCore PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/src")
if(TARGET SimpleIni::SimpleIni)
  target_link_libraries(SPSCore PUBLIC SimpleIni::SimpleIni)
else()
  target_include_directories(SPSCore PUBLIC
      "${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/include")
endif()
if(MSVC)
  target_compile_options(SPSCore PRIVATE /W4)
endif()

add_commonlibsse_plugin(${PROJECT_NAME} AUTHOR "snowman12356" SOURCES
    src/plugin.cpp
    src/SPSAPI.h
    src/controllers/ArousalController.cpp
    src/controllers/ArousalController.h
    src/controllers/SceneController.cpp
    src/controllers/SceneController.h
    src/controllers/SceneState.h
    src/diagnostics/Diagnostics.cpp
    src/diagnostics/Diagnostics.h
    src/diagnostics/SupportReport.cpp
    src/diagnostics/SupportReport.h
    src/runtime/Compatibility.cpp
    src/runtime/Compatibility.h
    src/runtime/PapyrusGateway.cpp
    src/runtime/PapyrusGateway.h
    src/runtime/PhysicsOwnershipAdapter.cpp
    src/runtime/PhysicsOwnershipAdapter.h
    src/runtime/PositionBackendAdapter.cpp
    src/runtime/PositionBackendAdapter.h)
target_include_directories(${PROJECT_NAME} SYSTEM PRIVATE "${mcp_sdk_SOURCE_DIR}/include")
target_compile_definitions(${PROJECT_NAME} PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
target_link_libraries(${PROJECT_NAME} PRIVATE SPSCore)
if(MSVC)
  target_compile_options(${PROJECT_NAME} PRIVATE /W4)
endif()

if(SPS_BUILD_CORE_TESTS)
  enable_testing()
  add_executable(SPSCoreTests tests/PhysicsDecisionTests.cpp)
  target_link_libraries(SPSCoreTests PRIVATE SPSCore)
  if(MSVC)
    target_compile_options(SPSCoreTests PRIVATE /W4)
  endif()
  add_test(NAME SPSCoreTests COMMAND SPSCoreTests)
  add_dependencies(${PROJECT_NAME} SPSCoreTests)
endif()
target_precompile_headers(${PROJECT_NAME} PRIVATE src/PCH.h)
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
install(FILES scripts/SPS_PositionBridge.pex DESTINATION "Scripts")
install(FILES scripts/Source/SPS_PositionBridge.psc DESTINATION "Source/Scripts")
install(FILES scripts/SPS_ArousalBridge.pex DESTINATION "Scripts")
install(FILES scripts/Source/SPS_ArousalBridge.psc DESTINATION "Source/Scripts")
install(FILES scripts/SPS_FSMPBridge.pex DESTINATION "Scripts")
install(FILES scripts/Source/SPS_FSMPBridge.psc DESTINATION "Source/Scripts")
install(FILES scripts/SPS_OStimBridge.pex DESTINATION "Optional/OStim/Scripts")
install(FILES scripts/Source/SPS_OStimBridge.psc DESTINATION "Optional/OStim/Source/Scripts")
install(DIRECTORY fomod DESTINATION ".")

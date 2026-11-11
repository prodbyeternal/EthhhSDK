# The fake native ABI is isolated from build/dist and never installed.
add_library(eth_test_native SHARED native_fake.cpp)
set_target_properties(eth_test_native PROPERTIES
  OUTPUT_NAME EthhhSDK
  RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_BINARY_DIR}/tests/interop"
  RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_BINARY_DIR}/tests/interop")
add_custom_command(TARGET eth_test_native PRE_LINK
  COMMAND ${Python3_EXECUTABLE} ${ETH_ROOT}/tools/gen_def.py
          ${CMAKE_CURRENT_SOURCE_DIR}/native_fake.cpp
          -o ${CMAKE_CURRENT_BINARY_DIR}/fake_exports.def
  VERBATIM)
target_link_options(eth_test_native PRIVATE /DEF:${CMAKE_CURRENT_BINARY_DIR}/fake_exports.def)

add_executable(eth_player_learning_tests PlayerLearningTests.cpp)
target_include_directories(eth_player_learning_tests PRIVATE ${ETH_ROOT}/src/core)
set_target_properties(eth_player_learning_tests PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_BINARY_DIR}/tests/interop"
  RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_BINARY_DIR}/tests/interop")

foreach(name IN ITEMS strat_identity ethhhpatch_policy ethhhpatch_projection ethhhpatch_camera ethhhpatch_interpolation ethhhpatch_entity movement_input)
  if(name STREQUAL "strat_identity")
    set(test_source StratIdentityTests.cpp)
  elseif(name STREQUAL "ethhhpatch_policy")
    set(test_source EthhhPatchPolicyTests.cpp)
  elseif(name STREQUAL "ethhhpatch_projection")
    set(test_source EthhhPatchProjectionTests.cpp)
  elseif(name STREQUAL "ethhhpatch_interpolation")
    set(test_source EthhhPatchInterpolationTests.cpp)
  elseif(name STREQUAL "ethhhpatch_entity")
    set(test_source EthhhPatchEntityTests.cpp)
  elseif(name STREQUAL "movement_input")
    set(test_source MovementInputTests.cpp)
  else()
    set(test_source EthhhPatchCameraTests.cpp)
  endif()
  add_executable(eth_${name}_tests ${test_source})
  target_include_directories(eth_${name}_tests PRIVATE ${ETH_ROOT}/src/core)
  set_target_properties(eth_${name}_tests PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_BINARY_DIR}/tests/interop"
    RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_BINARY_DIR}/tests/interop")
endforeach()

# Calls the production Sha256FileHex so the BCrypt object-length query stays
# correct. An aliased property buffer silently returned an empty hash and made
# the live WASD keyboard hook refuse to install.
add_executable(eth_sha256_tests Sha256Tests.cpp ${ETH_ROOT}/src/core/util.cpp)
target_include_directories(eth_sha256_tests PRIVATE ${ETH_ROOT}/src/core ${ETH_TP}/imgui)
target_link_libraries(eth_sha256_tests PRIVATE bcrypt.lib user32.lib)
set_target_properties(eth_sha256_tests PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_BINARY_DIR}/tests/interop"
  RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_BINARY_DIR}/tests/interop")

# Runs the actual manager drawing code on an off-screen D3D9 render target.
# The executable's main never invokes EntryGate, launcher::Run or hooks::Init.
add_executable(eth_chrome_preview EXCLUDE_FROM_ALL chrome_preview.cpp ${ETH_CORE_SRC})
target_compile_definitions(eth_chrome_preview PRIVATE ETH_TESTING)
target_compile_definitions(eth_chrome_preview PRIVATE ETH_TRAINER_SOURCE="${ETH_ROOT}/mods/trainer/mod.lua")
target_include_directories(eth_chrome_preview PRIVATE ${ETH_ROOT}/src/core ${ETH_TP}/lua)
target_link_libraries(eth_chrome_preview PRIVATE imgui lua54 minhook d3d9.lib dxguid.lib ole32.lib oleaut32.lib
  user32.lib gdi32.lib gdiplus.lib shell32.lib advapi32.lib winmm.lib bcrypt.lib shlwapi.lib)
set_target_properties(eth_chrome_preview PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_BINARY_DIR}/tests/preview"
  RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_BINARY_DIR}/tests/preview")

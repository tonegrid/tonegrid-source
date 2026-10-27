# Pinned upstream sources; encode in a separate process, outside the audio thread.
set(lame_root "${CMAKE_CURRENT_SOURCE_DIR}/vendor/lame/lame-3.100")
if(WIN32)
    configure_file("${lame_root}/configMS.h" "${CMAKE_CURRENT_BINARY_DIR}/lame-config/config.h" COPYONLY)
    file(GLOB lame_library_sources "${lame_root}/libmp3lame/*.c")
    add_executable(ToneGridLame ${lame_library_sources} "${lame_root}/libmp3lame/vector/xmm_quantize_sub.c"
        "${lame_root}/frontend/lame_main.c" "${lame_root}/frontend/main.c"
        "${lame_root}/frontend/brhist.c" "${lame_root}/frontend/console.c"
        "${lame_root}/frontend/get_audio.c" "${lame_root}/frontend/lametime.c"
        "${lame_root}/frontend/parse.c" "${lame_root}/frontend/timestatus.c")
    target_include_directories(ToneGridLame PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/lame-config"
        "${lame_root}/include" "${lame_root}/libmp3lame")
    target_compile_definitions(ToneGridLame PRIVATE HAVE_CONFIG_H _CRT_SECURE_NO_WARNINGS)
    target_link_libraries(ToneGridLame PRIVATE shell32)
    set_target_properties(ToneGridLame PROPERTIES OUTPUT_NAME lame)
elseif(APPLE)
    # Use LAME's own platform checks, never Windows integer/SSE configuration.
    # The pinned source tree is read-only; all configure/build output is isolated.
    include(ExternalProject)
    find_program(tonegrid_make NAMES make REQUIRED)
    set(lame_build "${CMAKE_CURRENT_BINARY_DIR}/lame-native")
    set(lame_cflags "-O3 -arch arm64")
    if(CMAKE_OSX_DEPLOYMENT_TARGET)
        string(APPEND lame_cflags " -mmacosx-version-min=${CMAKE_OSX_DEPLOYMENT_TARGET}")
    endif()
    set(lame_sdk_environment)
    if(IS_DIRECTORY "${CMAKE_OSX_SYSROOT}")
        list(APPEND lame_sdk_environment "SDKROOT=${CMAKE_OSX_SYSROOT}")
    endif()
    ExternalProject_Add(ToneGridLameBuild
        SOURCE_DIR "${lame_root}" BINARY_DIR "${lame_build}"
        DOWNLOAD_COMMAND "" UPDATE_COMMAND ""
        CONFIGURE_COMMAND ${CMAKE_COMMAND} -E env "CC=${CMAKE_C_COMPILER}"
            "CFLAGS=${lame_cflags}" ${lame_sdk_environment}
            # The pinned 2017 config.guess predates Apple Silicon. Its config.sub
            # accepts aarch64, so specify the native tuple without editing vendor code.
            /bin/sh "${lame_root}/configure" --build=aarch64-apple-darwin --host=aarch64-apple-darwin
            --disable-shared --enable-static --disable-decoder --with-fileio=lame
        BUILD_COMMAND "${tonegrid_make}" -j2
        INSTALL_COMMAND ""
        BUILD_BYPRODUCTS "${lame_build}/frontend/lame")
    add_executable(ToneGridLame IMPORTED GLOBAL)
    set_target_properties(ToneGridLame PROPERTIES IMPORTED_LOCATION "${lame_build}/frontend/lame")
    add_dependencies(ToneGridLame ToneGridLameBuild)
elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    include(ExternalProject)
    find_program(tonegrid_make NAMES make REQUIRED)
    set(lame_build "${CMAKE_CURRENT_BINARY_DIR}/lame-native")
    ExternalProject_Add(ToneGridLameBuild
        SOURCE_DIR "${lame_root}" BINARY_DIR "${lame_build}"
        DOWNLOAD_COMMAND "" UPDATE_COMMAND ""
        CONFIGURE_COMMAND ${CMAKE_COMMAND} -E env "CC=${CMAKE_C_COMPILER}" "CFLAGS=-O3"
            /bin/sh "${lame_root}/configure" --disable-shared --enable-static --disable-decoder --with-fileio=lame
        BUILD_COMMAND "${tonegrid_make}" -j2 INSTALL_COMMAND ""
        BUILD_BYPRODUCTS "${lame_build}/frontend/lame")
    add_executable(ToneGridLame IMPORTED GLOBAL)
    set_target_properties(ToneGridLame PROPERTIES IMPORTED_LOCATION "${lame_build}/frontend/lame")
    add_dependencies(ToneGridLame ToneGridLameBuild)
endif()
add_dependencies(ToneGrid ToneGridLame)
if(APPLE)
    set(tonegrid_encoder_destination "$<TARGET_FILE_DIR:ToneGrid>/lame")
elseif(WIN32)
    set(tonegrid_encoder_destination "${tonegrid_payload_dir}/Tools/lame.exe")
else()
    set(tonegrid_encoder_destination "${tonegrid_payload_dir}/Tools/lame")
endif()
add_custom_command(TARGET ToneGrid POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "${tonegrid_payload_dir}/Tools" "${tonegrid_payload_dir}/Licenses"
    COMMAND ${CMAKE_COMMAND} -E copy "$<TARGET_FILE:ToneGridLame>" "${tonegrid_encoder_destination}"
    COMMAND ${CMAKE_COMMAND} -E copy "${lame_root}/COPYING" "${tonegrid_payload_dir}/Licenses/LAME-COPYING.txt"
    COMMAND ${CMAKE_COMMAND} -E copy "${CMAKE_CURRENT_SOURCE_DIR}/vendor/lame/lame-3.100.tar.gz" "${tonegrid_payload_dir}/Tools/lame-3.100-source.tar.gz")

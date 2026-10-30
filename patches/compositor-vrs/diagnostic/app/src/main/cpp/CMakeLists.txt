cmake_minimum_required(VERSION 3.22.1)
project(wave_vrs_diagnostic LANGUAGES CXX)

if(NOT DEFINED WAVE_SDK_DIR)
    message(FATAL_ERROR "WAVE_SDK_DIR must point to the extracted local Wave AAR")
endif()
add_library(wvr_api SHARED IMPORTED)
set_target_properties(wvr_api PROPERTIES
    IMPORTED_LOCATION "${WAVE_SDK_DIR}/jni/${ANDROID_ABI}/libwvr_api.so")
add_library(vrs_diagnostic SHARED diagnostic.cpp)
target_include_directories(vrs_diagnostic PRIVATE "${WAVE_SDK_DIR}/include")
target_link_libraries(vrs_diagnostic PRIVATE wvr_api GLESv3 EGL log android)
set_target_properties(vrs_diagnostic PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED YES)

cmake_minimum_required(VERSION 3.16)
project(stardew_save_editor C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

set(SOURCES
    src/main.c
    src/platform/platform.c
    src/core/save_data.c
    src/ui/ui.c
)

add_executable(${PROJECT_NAME} ${SOURCES})

target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

if(VITA)
    include("$ENV{VITASDK}/share/vita.cmake" REQUIRED)

    set(VITA_APP_NAME "Stardew Save Editor")
    set(VITA_TITLEID  "SVSE00001")
    set(VITA_VERSION  "01.00")

    target_compile_definitions(${PROJECT_NAME} PRIVATE PLATFORM_VITA=1)

    target_link_libraries(${PROJECT_NAME} PRIVATE
        SDL2_ttf
        SDL2
        xml2
        freetype
        png
        z
        m
        SceDisplay_stub
        SceCtrl_stub
        SceAudio_stub
        SceSysmodule_stub
        SceGxm_stub
        SceCommonDialog_stub
        SceAppMgr_stub
        SceTouch_stub
        SceHid_stub
        SceMotion_stub
    )

    vita_create_self(${PROJECT_NAME}.self ${PROJECT_NAME})
    vita_create_vpk(${PROJECT_NAME}.vpk ${VITA_TITLEID} ${PROJECT_NAME}.self
        VERSION ${VITA_VERSION}
        NAME ${VITA_APP_NAME}
    )
else()
    find_package(PkgConfig REQUIRED)
    find_package(LibXml2 REQUIRED)

    pkg_check_modules(SDL2 REQUIRED sdl2)
    pkg_check_modules(SDL2_TTF REQUIRED SDL2_ttf)

    target_compile_definitions(${PROJECT_NAME} PRIVATE PLATFORM_DESKTOP=1)

    target_include_directories(${PROJECT_NAME} PRIVATE
        ${SDL2_INCLUDE_DIRS}
        ${SDL2_TTF_INCLUDE_DIRS}
        ${LIBXML2_INCLUDE_DIR}
    )

    target_link_directories(${PROJECT_NAME} PRIVATE
        ${SDL2_LIBRARY_DIRS}
        ${SDL2_TTF_LIBRARY_DIRS}
    )

    target_link_libraries(${PROJECT_NAME} PRIVATE
        ${SDL2_LIBRARIES}
        ${SDL2_TTF_LIBRARIES}
        ${LIBXML2_LIBRARIES}
    )

    target_compile_options(${PROJECT_NAME} PRIVATE -Wall -Wextra -Wpedantic)
endif()

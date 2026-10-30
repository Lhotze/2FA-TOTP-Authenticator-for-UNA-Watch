file(GLOB_RECURSE TOUCHGFX_SOURCES
    ${CMAKE_CURRENT_LIST_DIR}/gui/src/*.cpp
    ${CMAKE_CURRENT_LIST_DIR}/gui/src/*.c
    ${CMAKE_CURRENT_LIST_DIR}/generated/fonts/src/*.cpp
    ${CMAKE_CURRENT_LIST_DIR}/generated/gui_generated/src/*.cpp
    ${CMAKE_CURRENT_LIST_DIR}/generated/images/src/*.cpp
    ${CMAKE_CURRENT_LIST_DIR}/generated/texts/src/*.cpp
)

# Wrist Boy draws everything itself, so only one font is kept (Tools/slim_fonts.py makes the two files that tie the fonts together)
set(WB_SLIM_FONTS ON)
if(WB_SLIM_FONTS)
    list(FILTER TOUCHGFX_SOURCES EXCLUDE REGEX "generated/fonts/src/(Font|Kerning|Table)_")
    file(GLOB WB_KEPT_FONT_SOURCES ${CMAKE_CURRENT_LIST_DIR}/generated/fonts/src/*Poppins_Regular_18_2bpp*.cpp)
    list(APPEND TOUCHGFX_SOURCES ${WB_KEPT_FONT_SOURCES})
    list(FILTER TOUCHGFX_SOURCES EXCLUDE REGEX "generated/(texts/src/TypedTextDatabase|fonts/src/ApplicationFontProvider)\\.cpp")
    file(GLOB WB_SLIM_SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_LIST_DIR}/slim/src/*.cpp)
    list(APPEND TOUCHGFX_SOURCES ${WB_SLIM_SOURCES})
endif()

if(NOT DEFINED ENV{UNA_SDK})
    message(FATAL_ERROR "UNA_SDK environment variable not set")
endif()

set(TOUCHGFX_INCLUDE_DIRS
    $ENV{UNA_SDK}/ThirdParty/touchgfx/framework/include
    ${CMAKE_CURRENT_LIST_DIR}/generated/fonts/include
    ${CMAKE_CURRENT_LIST_DIR}/generated/gui_generated/include
    ${CMAKE_CURRENT_LIST_DIR}/generated/images/include
    ${CMAKE_CURRENT_LIST_DIR}/generated/texts/include
    ${CMAKE_CURRENT_LIST_DIR}/generated/videos/include
    ${CMAKE_CURRENT_LIST_DIR}/gui/include
)

set(TOUCHGFX_LIBS
    $ENV{UNA_SDK}/ThirdParty/touchgfx/lib/core/cortex_m33/gcc/libtouchgfx-float-abi-hard.a
)
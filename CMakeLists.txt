cmake_minimum_required(VERSION 2.6)
project(zyncoder)

include(CheckIncludeFiles)
include(CheckLibraryExists)

link_directories(/usr/local/lib)

check_library_exists(wiringPi wiringPiSetup "" HAVE_WIRINGPI_LIB)

if (HAVE_WIRINGPI_LIB)
	message("++ Defined HAVE_WIRINGPI_LIB")
	add_definitions(-DHAVE_WIRINGPI_LIB)
endif()

if ("$ENV{ZYNTHIAN_WIRING_LAYOUT}" STREQUAL "MCP23017_ENCODERS")
	message("++ Defined MCP23017_ENCODERS")
	add_definitions(-DMCP23017_ENCODERS)
endif()

if (DEFINED ENV{ZYNTHIAN_FORCE_WIRINGPI_EMU})
	message("++ Forced wiringPiEmu")
	set(ZYNTHIAN_FORCE_WIRINGPI_EMU "$ENV{ZYNTHIAN_FORCE_WIRINGPI_EMU}")
endif()

if (NOT ZYNTHIAN_FORCE_WIRINGPI_EMU AND HAVE_WIRINGPI_LIB)
	message("++ Using wiringPI")
	add_library(zyncoder SHARED zyncoder.h zyncoder.c)
	target_link_libraries(zyncoder wiringPi asound jack lo)
else()
	message("++ Using wiringPiEmu")
	add_library(zyncoder SHARED zyncoder.h zyncoder.c wiringPiEmu.c)
	add_library(wiringPiEmu SHARED wiringPiEmu.h wiringPiEmu.c)
	target_link_libraries(zyncoder asound jack lo)
	install(TARGETS wiringPiEmu LIBRARY DESTINATION lib)
endif()

add_executable(zyncoder_test zyncoder_test.c)
target_link_libraries(zyncoder_test zyncoder)

install(TARGETS zyncoder LIBRARY DESTINATION lib)
#install(TARGETS zyncoder RUNTIME DESTINATION bin)

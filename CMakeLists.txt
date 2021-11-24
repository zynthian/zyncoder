cmake_minimum_required(VERSION 3.0)
project(zyncore)

include(CheckIncludeFiles)
include(CheckLibraryExists)

link_directories(/usr/local/lib)

check_library_exists(wiringPi wiringPiSetup "" HAVE_WIRINGPI_LIB)

if (HAVE_WIRINGPI_LIB)
	message("++ Defined HAVE_WIRINGPI_LIB")
	add_definitions(-DHAVE_WIRINGPI_LIB)
endif()

if (("$ENV{ZYNTHIAN_WIRING_LAYOUT}" STREQUAL "MCP23017_ENCODERS") 
 OR ("$ENV{ZYNTHIAN_WIRING_LAYOUT}" STREQUAL "MCP23017_EXTRA")
 OR ("$ENV{ZYNTHIAN_WIRING_LAYOUT}" STREQUAL "MCP23017_EPDF")
 OR ("$ENV{ZYNTHIAN_WIRING_LAYOUT}" STREQUAL "MCP23017_EPDF_REVERSE")
 OR ("$ENV{ZYNTHIAN_WIRING_LAYOUT}" STREQUAL "MCP23017_ZynScreen")
 OR ("$ENV{ZYNTHIAN_WIRING_LAYOUT}" STREQUAL "MCP23017_ZynScreen_Zynaptik")
 OR ("$ENV{ZYNTHIAN_WIRING_LAYOUT}" STREQUAL "MCP23017_ZynScreen_Zynface"))
 	message("++ Defined MCP23017_ENCODERS")
	add_definitions(-DMCP23017_ENCODERS)

	if (DEFINED ENV{ZYNTHIAN_WIRING_MCP23017_INTA_PIN} AND NOT ("$ENV{ZYNTHIAN_WIRING_MCP23017_INTA_PIN}" STREQUAL ""))
		message("++ Defined MCP23017 INTA PIN $ENV{ZYNTHIAN_WIRING_MCP23017_INTA_PIN}")
		add_definitions(-DMCP23017_INTA_PIN=$ENV{ZYNTHIAN_WIRING_MCP23017_INTA_PIN})
	endif()

	if (DEFINED ENV{ZYNTHIAN_WIRING_MCP23017_INTB_PIN} AND NOT ("$ENV{ZYNTHIAN_WIRING_MCP23017_INTB_PIN}" STREQUAL ""))
		message("++ Defined MCP23017 INTB PIN $ENV{ZYNTHIAN_WIRING_MCP23017_INTB_PIN}")
		add_definitions(-DMCP23017_INTB_PIN=$ENV{ZYNTHIAN_WIRING_MCP23017_INTB_PIN})
	endif()

	if (DEFINED ENV{ZYNTHIAN_WIRING_ZYNAPTIK_CONFIG} AND NOT ("$ENV{ZYNTHIAN_WIRING_ZYNAPTIK_CONFIG}" STREQUAL ""))
		message("++ Defined ZYNAPTIK_CONFIG $ENV{ZYNTHIAN_WIRING_ZYNAPTIK_CONFIG}")
		add_definitions(-DZYNAPTIK_CONFIG="$ENV{ZYNTHIAN_WIRING_ZYNAPTIK_CONFIG}")
		set(BUILD_ZYNAPTIK "1")

		if ("$ENV{ZYNTHIAN_WIRING_ZYNAPTIK_CONFIG}" MATCHES "^Custom")
			message("++ Defined ZYNAPTIK_VERSION 1")
			add_definitions(-DZYNAPTIK_VERSION=1)
		else()
			message("++ Defined ZYNAPTIK_VERSION 2")
			add_definitions(-DZYNAPTIK_VERSION=2)
		endif()
	endif()

	if (DEFINED ENV{ZYNTHIAN_WIRING_ZYNTOF_CONFIG} AND NOT ("$ENV{ZYNTHIAN_WIRING_ZYNTOF_CONFIG}" STREQUAL ""))
		message("++ Defined ZYNTOF_CONFIG $ENV{ZYNTHIAN_WIRING_ZYNTOF_CONFIG}")
		add_definitions(-DZYNTOF_CONFIG="$ENV{ZYNTHIAN_WIRING_ZYNTOF_CONFIG}")
		set(BUILD_ZYNTOF "1")
	endif()

elseif(("$ENV{ZYNTHIAN_WIRING_LAYOUT}" STREQUAL "Z2_V1"))
	message("++ Defined MCP23017_ENCODERS")
	add_definitions(-DMCP23017_ENCODERS)

elseif(("$ENV{ZYNTHIAN_WIRING_LAYOUT}" STREQUAL "I2C_HWC"))
	message("++ Defined I2C_HWC_ENCODERS")
	add_definitions(-DI2C_HWC_ENCODERS)

elseif(("$ENV{ZYNTHIAN_WIRING_LAYOUT}" STREQUAL "DUMMIES"))
	message("++ Defined DUMMY_ENCODERS")
	add_definitions(-DDUMMY_ENCODERS)

else()
	message("++ Defined MCP23008_ENCODERS")
	add_definitions(-DMCP23008_ENCODERS)
endif()


if (DEFINED ENV{ZYNTHIAN_FORCE_WIRINGPI_EMU})
	message("++ Forced wiringPiEmu")
	set(ZYNTHIAN_FORCE_WIRINGPI_EMU "$ENV{ZYNTHIAN_FORCE_WIRINGPI_EMU}")
endif()


if ("$ENV{ZYNTHIAN_WIRING_LAYOUT}" STREQUAL "I2C_HWC")
	message("++ Using I2C HWC")
	add_library(zyncore SHARED zyncore.c zyncontrol.h zyncontrol_i2c.c zynpot.h zynpot.c zynrv112.h zynrv112.c zynads1115.h zynads1115.c zyncoder_i2c.h zyncoder_i2c.c zynmidirouter.h zynmidirouter.c zynmaster.h zynmaster.c)
	target_link_libraries(zyncore wiringPi jack lo)

elseif ("$ENV{ZYNTHIAN_WIRING_LAYOUT}" STREQUAL "Z2_V1")
	message("++ Using Z2_V1")
	add_library(zyncore SHARED zyncore.c zyncontrol.h zyncontrol_z2.c zynpot.h zynpot.c zynrv112.h zynrv112.c zynads1115.h zynads1115.c zyncoder.h zyncoder.c zynmidirouter.h zynmidirouter.c zynmaster.h zynmaster.c)
	target_link_libraries(zyncore wiringPi jack lo)

	add_executable(z2_test z2_test.c)
	target_link_libraries(z2_test zyncore)

elseif (NOT ZYNTHIAN_FORCE_WIRINGPI_EMU AND HAVE_WIRINGPI_LIB)
	message("++ Using wiringPI")
	if (BUILD_ZYNTOF AND BUILD_ZYNAPTIK)
		message("++ Building Zynaptik & Zyntof support")
		add_library(zyncore SHARED zyncore.c zyncontrol.h zyncontrol.c zynpot.h zynpot.c zynrv112.h zynrv112.c zynads1115.h zynads1115.c zyncoder.h zyncoder.c zynmidirouter.h zynmidirouter.c zynmaster.h zynmaster.c zynaptik.h zynaptik.c zyntof.h zyntof.c)
		target_link_libraries(zyncore wiringPi jack lo MCP4728 tof)
	elseif (BUILD_ZYNAPTIK)
		message("++ Building Zynaptik support")
		add_library(zyncore SHARED zyncore.c zyncontrol.h zyncontrol.c zynpot.h zynpot.c zynrv112.h zynrv112.c zynads1115.h zynads1115.c zyncoder.h zyncoder.c zynmidirouter.h zynmidirouter.c zynmaster.h zynmaster.c zynaptik.h zynaptik.c)
		target_link_libraries(zyncore wiringPi jack MCP4728 lo)
	elseif (BUILD_ZYNTOF)
		message("++ Building Zyntof support")
		add_library(zyncore SHARED zyncore.c zyncontrol.h zyncontrol.c zynpot.h zynpot.c zynrv112.h zynrv112.c zynads1115.h zynads1115.c zyncoder.h zyncoder.c zynmidirouter.h zynmidirouter.c zynmaster.h zynmaster.c zyntof.h zyntof.c)
		target_link_libraries(zyncore wiringPi jack lo tof)
	else()
		add_library(zyncore SHARED zyncore.c zyncontrol.h zyncontrol.c zynpot.h zynpot.c zynrv112.h zynrv112.c zynads1115.h zynads1115.c zyncoder.h zyncoder.c zynmidirouter.h zynmidirouter.c zynmaster.h zynmaster.c)
		target_link_libraries(zyncore wiringPi jack lo)
	endif()

	add_executable(zyncoder_test zyncoder_test.c)
	target_link_libraries(zyncoder_test zyncore)

else()
	message("++ Using wiringPiEmu")
	add_library(zyncore SHARED zyncore.c zyncontrol.h zyncontrol.c zynpot.h zynpot.c zynrv112.h zynrv112.c zynads1115.h zynads1115.c zyncoder.h zyncoder.c wiringPiEmu.c zynmidirouter.h zynmidirouter.c zynmaster.h zynmaster.c)
	target_link_libraries(zyncore jack lo)

	add_executable(zyncoder_test zyncoder_test.c)
	target_link_libraries(zyncoder_test zyncore)

endif()


install(TARGETS zyncore LIBRARY DESTINATION lib)

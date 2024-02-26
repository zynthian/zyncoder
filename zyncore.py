#!/usr/bin/python3
# -*- coding: utf-8 -*-
# ********************************************************************
# ZYNTHIAN PROJECT: Zynthian Core library python wrapper
# 
# A Python wrapper for Zynthian Core library
# 
# Copyright (C) 2015-2021 Fernando Moyano <jofemodo@zynthian.org>
#
# ********************************************************************
# 
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License as
# published by the Free Software Foundation; either version 2 of
# the License, or any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
# GNU General Public License for more details.
#
# For a full copy of the GNU General Public License see the LICENSE.txt file.
# 
# ********************************************************************

from ctypes import *
from os.path import dirname, realpath
#from numpy.ctypeslib import ndpointer

# -------------------------------------------------------------------------------
# Zyncoder Library Wrapper
# -------------------------------------------------------------------------------


global lib_zyncore
lib_zyncore = None


def lib_zyncore_init():
	global lib_zyncore
	try:
		lib_zyncore = cdll.LoadLibrary(dirname(realpath(__file__))+"/build/libzyncore.so")
		lib_zyncore.init_zyncore()
		# Setup return type for some functions
		#lib_zyncore.get_midi_filter_clone_cc.restype = ndpointer(dtype=c_ubyte, shape=(128,))

	except Exception as e:
		lib_zyncore = None
		print("Can't init zyncore library: %s" % str(e))

	return lib_zyncore


def lib_zyncore_init_minimal():
	global lib_zyncore
	try:
		lib_zyncore = cdll.LoadLibrary(dirname(realpath(__file__))+"/build/libzyncore.so")
		lib_zyncore.init_zyncore_minimal()

	except Exception as e:
		lib_zyncore = None
		print("Can't init minimal zyncore library: %s" % str(e))

	return lib_zyncore


def get_lib_zyncore():
	return lib_zyncore

# -------------------------------------------------------------------------------

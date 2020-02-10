#!/usr/bin/python3
# -*- coding: utf-8 -*-
#********************************************************************
# ZYNTHIAN PROJECT: Zyncoder Python Wrapper
# 
# A Python wrapper for zyncoder library
# 
# Copyright (C) 2015-2016 Fernando Moyano <jofemodo@zynthian.org>
#
#********************************************************************
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
#********************************************************************

from ctypes import *
from os.path import dirname, realpath
from numpy.ctypeslib import ndpointer

#-------------------------------------------------------------------------------
# Zyncoder Library Wrapper
#-------------------------------------------------------------------------------

global lib_zyncoder
lib_zyncoder=None


def lib_zyncoder_init():
	global lib_zyncoder
	try:
		lib_zyncoder=cdll.LoadLibrary(dirname(realpath(__file__))+"/build/libzyncoder.so")
		lib_zyncoder.init_zynlib()
		#Setup return type for some functions
		lib_zyncoder.get_midi_filter_clone_cc.restype = ndpointer(dtype=c_ubyte, shape=(128,))

	except Exception as e:
		lib_zyncoder=None
		print("Can't init zyncoder library: %s" % str(e))

	return lib_zyncoder


def get_lib_zyncoder():
	return lib_zyncoder

#-------------------------------------------------------------------------------

#!/bin/bash

DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null 2>&1 && pwd )"

pushd $DIR
	if [ ! -d build ]; then
		mkdir build
	fi
	pushd build

		# Detect headphones amplifier kernel driver
		if lsmod | grep -wq "^snd_soc_tpa6130a2"; then
			export TPA6130_KERNEL_DRIVER_LOADED=1
		else
			export TPA6130_KERNEL_DRIVER_LOADED=0
		fi

		if [ "$1" == "debug" ]; then
			cmake -DCMAKE_BUILD_TYPE=Debug ..
		else
			cmake ..
		fi
		make
	popd
popd

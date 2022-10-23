#!/bin/bash

DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null 2>&1 && pwd )"

pushd $DIR
	if [ ! -d build ]; then
		mkdir build
	fi
	pushd build
		if [ "$1" == "debug" ]; then
			cmake -DCMAKE_BUILD_TYPE=Debug ..
		else
			cmake ..
		fi
		make
	popd
popd

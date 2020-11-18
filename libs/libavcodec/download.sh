#!/usr/bin/env bash

PACKAGE_URL=http://ffmpeg.org/releases/ffmpeg-3.1.4.tar.xz
PACKAGE_FILE=ffmpeg-3.1.4.tar.xz
SRCDIR=ffmpeg-3.1.4

THISDIR=`dirname $0`


echo "-------------------------------"
if [ ! -f ${THISDIR}/${PACKAGE_FILE} ]; then
	echo "downloading ..."
	curl ${PACKAGE_URL} > ${THISDIR}/${PACKAGE_FILE} || exit 1
fi

echo "-------------------------------"
if [ ! -d ${THISDIR}/${SRCDIR} ]; then
	echo "unpack ..."
	tar xvf ${PACKAGE_FILE} || exit 2
fi


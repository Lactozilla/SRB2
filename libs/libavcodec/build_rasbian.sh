#!/usr/bin/env bash


PLATFORM=rasbian


THISDIR=`dirname $0`


cd ${THISDIR}/ffmpeg-3.1.4


echo "-------------------------------"
echo "configure ..."
./configure --prefix=`pwd`/../tmp-bin/	\
	--enable-static	\
	--disable-shared		\
	--enable-memalign-hack	\
	--disable-programs	\
	--disable-doc	    \
	--disable-avdevice	\
	--disable-swresample	\
	--disable-swscale 	\
	--disable-postproc	\
	--disable-avfilter	\
	--disable-network	\
	--disable-everything	\
	--enable-protocol=file	\
	--enable-omx \
	--enable-omx-rpi \
	--enable-encoder=h264_omx \
	--enable-mmal \
	--enable-hwaccel=h264_mmal \
	--enable-decoder=h264_mmal \
	--enable-decoder=h264	\
	|| exit 1

# reference: https://blog.csdn.net/sbdx/article/details/75110126

echo "-------------------------------"
echo "making ..."
make -j4 || exit 2
make install || exit 3


echo "-------------------------------"
cd ..
echo "install platform files ..."

mkdir -p include
cp -av tmp-bin/include/* include/

mkdir -p lib/${PLATFORM}
cp -fv tmp-bin/lib/libavformat.a lib/${PLATFORM}/
cp -fv tmp-bin/lib/libavcodec.a lib/${PLATFORM}/
cp -fv tmp-bin/lib/libavutil.a lib/${PLATFORM}/


rm -fr tmp-bin


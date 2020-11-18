#!/usr/bin/env bash


PLATFORM=macos


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
	--enable-libx264    \
	--enable-gpl        \
	--enable-decoder=h264	\
	--enable-demuxer=avi	\
	--enable-protocol=file	\
      || exit 1


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


// SONIC ROBO BLAST 2
//-----------------------------------------------------------------------------
// Copyright (C) 2020-2021 by Jaime Ita Passos.
// Copyright (C) 2003 by Fabrice Bellard.
//
// This program is free software distributed under the
// terms of the GNU General Public License, version 2.
// See the 'LICENSE' file for more details.
//-----------------------------------------------------------------------------
/// \file  m_videoencoder.c
/// \brief Video creation movie mode.

#include "m_videoencoder.h"
#include "d_main.h"
#include "z_zone.h"
#include "v_video.h"
#include "i_video.h"
#include "i_system.h" // I_GetPreciseTime
#include "m_misc.h"
#include "r_data.h" // R_GetRGBA*
#include "st_stuff.h" // st_palette

#ifdef HWRENDER
#include "hardware/hw_main.h"
#endif

static CV_PossibleValue_t video_bitrate_cons_t[] = {{1, "MIN"}, {100, "MAX"}, {0, NULL}};
static CV_PossibleValue_t video_audiorate_cons_t[] = {{8, "MIN"}, {512, "MAX"}, {0, NULL}};
static CV_PossibleValue_t video_gop_cons_t[] = {{(FRACUNIT / 2), "MIN"}, {(5 * FRACUNIT), "MAX"}, {0, NULL}};

consvar_t cv_videoencoder_bitrate   = CVAR_INIT ("videoencoder_bitrate", "50", CV_SAVE, video_bitrate_cons_t, NULL);
consvar_t cv_videoencoder_gopsize   = CVAR_INIT ("videoencoder_gopsize", "1.0", CV_SAVE | CV_FLOAT, video_gop_cons_t, NULL);
consvar_t cv_videoencoder_downscale = CVAR_INIT ("videoencoder_downscale", "Off", CV_SAVE, CV_OnOff, NULL);
consvar_t cv_videoencoder_audio     = CVAR_INIT ("videoencoder_audio", "Yes", CV_SAVE, CV_YesNo, NULL);
consvar_t cv_videoencoder_audiorate = CVAR_INIT ("videoencoder_audio_bitrate", "384", CV_SAVE, video_audiorate_cons_t, NULL);

#ifdef HAVE_LIBAV

#include "libavcodec/avcodec.h"
#include "libavformat/avformat.h"
#include "libswscale/swscale.h"
#include "libswresample/swresample.h"
#include "libavutil/channel_layout.h"
#include "libavutil/mathematics.h"
#include "libavutil/opt.h"
#include "libavutil/time.h"

#if defined(__ICL) || defined (__INTEL_COMPILER)
	__pragma(warning(push)) __pragma(warning(disable:1478))
#elif defined(_MSC_VER)
	__pragma(warning(push)) __pragma(warning(disable:4996))
#else
	_Pragma("GCC diagnostic ignored \"-Wdeprecated-declarations\"")
#endif

enum
{
	ENCSTREAM_VIDEO,
	ENCSTREAM_AUDIO
};

typedef struct encoderstream_s
{
	int type;

	AVStream *st;
	AVCodecContext *enc;

	AVFrame *frame;
	AVFrame *tmp_frame;

	INT32 width, height;
	INT32 downscaling;
	UINT32 *buffer;
	RGBA_t *palette;

	int lastpacket;
	int64_t next_pts;
	tic_t tic;

	precise_t prevframetime;
	UINT32 delayus;

	struct SwsContext *sws_ctx;
	struct SwrContext *swr_ctx;
} encoderstream_t;

static encoderstream_t videostream = {0};
static encoderstream_t audiostream = {0};

static AVOutputFormat *outputformat;
static AVFormatContext *formatcontext;

static INT32 Encoder_GetFramerate(void)
{
	return NEWTICRATE;
}

static boolean Encoder_IsRecordingGIF(void)
{
	return (outputformat->video_codec == AV_CODEC_ID_GIF);
}

static void Encoder_CloseStream(encoderstream_t *stream);

//
// AUDIO OUTPUT
//

#define AUDIO_STREAM_NAME "audiostream.raw"

static char *audio_out_filename;
static FILE *audio_out_stream;
static size_t audio_out_size;

static boolean encoding_audio = false;
static boolean recording_audio = false;

boolean VideoEncoder_IsRecordingAudio(void)
{
	return recording_audio;
}

boolean VideoEncoder_RecordAudio(INT16 *stream, int len)
{
	if (encoding_audio && audio_out_stream)
	{
		if (fwrite(stream, len / 2, sizeof(INT16), audio_out_stream) < sizeof(INT16))
		{
			CONS_Alert(CONS_ERROR, "VideoEncoder_RecordAudio: Could not write into audio buffer\n");
			fclose(audio_out_stream);
			audio_out_stream = NULL;
		}
		else
			return true;
	}

	return false;
}

static boolean Encoder_CanRecordAudio(void)
{
	if (!cv_videoencoder_audio.value || Encoder_IsRecordingGIF())
		return false;

	// I'm not sure personally how to deal with audio codecs,
	// and the output in AVI sounds bad, so I'm restricting
	// audio recording to H.264.
	if (outputformat->video_codec != AV_CODEC_ID_H264)
		return false;

	return (outputformat->audio_codec != AV_CODEC_ID_NONE);
}

static boolean Encoder_AddAudioStream(encoderstream_t *stream, AVFormatContext *oc, enum AVCodecID codec_id)
{
	AVCodecContext *c;
	AVCodec *codec;

	// Find the audio encoder.
	codec = avcodec_find_encoder(codec_id);
	if (!codec)
	{
		CONS_Alert(CONS_ERROR, "Audio codec not found\n");
		return false;
	}

	stream->st = avformat_new_stream(oc, NULL);
	if (!stream->st)
	{
		CONS_Alert(CONS_ERROR, "Could not allocate an audio stream\n");
		return false;
	}

	c = avcodec_alloc_context3(codec);
	if (!c)
	{
		CONS_Alert(CONS_ERROR, "Could not allocate an audio encoding context\n");
		return false;
	}

	stream->enc = c;

	c->sample_fmt = codec->sample_fmts ? codec->sample_fmts[0] : AV_SAMPLE_FMT_S16;
	c->sample_rate = 44100; //codec->supported_samplerates ? codec->supported_samplerates[0] : 44100;
	c->channel_layout = codec->channel_layouts ? codec->channel_layouts[0] : AV_CH_LAYOUT_STEREO;
	c->channels = av_get_channel_layout_nb_channels(c->channel_layout);
	c->bit_rate = cv_videoencoder_audiorate.value * 1000;

	stream->st->time_base = (AVRational){1, c->sample_rate};

	// Some formats want stream headers to be separate.
	if (oc->oformat->flags & AVFMT_GLOBALHEADER)
		c->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;

	stream->swr_ctx = swr_alloc();

	if (stream->swr_ctx == NULL)
	{
		CONS_Alert(CONS_ERROR, "Error opening the audio resampling context\n");
		return false;
	}

	swr_alloc_set_opts(stream->swr_ctx, c->channel_layout, c->sample_fmt, c->sample_rate, AV_CH_LAYOUT_STEREO, AV_SAMPLE_FMT_S16, 44100, 0, NULL);
	swr_init(stream->swr_ctx);

	if (!swr_is_initialized(stream->swr_ctx))
	{
		CONS_Alert(CONS_ERROR, "Audio resampler has not been properly initialized\n");
		return false;
	}

	return true;
}

static AVFrame *Encoder_AllocateAudioFrame(enum AVSampleFormat sample_fmt, uint64_t channel_layout, int sample_rate, int nb_samples)
{
	AVFrame *frame = av_frame_alloc();

	if (!frame)
		return NULL;

	frame->format = sample_fmt;
	frame->channel_layout = channel_layout;
	frame->sample_rate = sample_rate;
	frame->nb_samples = nb_samples;

	if (nb_samples)
	{
		int ret = av_frame_get_buffer(frame, 0);
		if (ret < 0)
		{
			CONS_Alert(CONS_ERROR, "Error allocating an audio buffer\n");
			return NULL;
		}
	}

	return frame;
}

static boolean Encoder_OpenAudioStream(encoderstream_t *ost)
{
	AVCodecContext *c;
	int nb_samples, ret;

	c = ost->enc;

	// open it
	if (avcodec_open2(c, NULL, NULL) < 0)
	{
		CONS_Alert(CONS_ERROR, "Could not open the audio codec\n");
		return false;
	}

	if (c->codec->capabilities & AV_CODEC_CAP_VARIABLE_FRAME_SIZE)
		nb_samples = 10000;
	else
		nb_samples = c->frame_size;

	ost->frame = Encoder_AllocateAudioFrame(c->sample_fmt, c->channel_layout, c->sample_rate, nb_samples);

	// copy the stream parameters to the muxer
	ret = avcodec_parameters_from_context(ost->st->codecpar, c);
	if (ret < 0)
	{
		CONS_Alert(CONS_ERROR, "Could not copy the audio stream parameters\n");
		return false;
	}

	return true;
}

static AVFrame *AudioEncoder_GetFrame(INT16 *buf, encoderstream_t *ost)
{
	AVFrame *frame = ost->frame;

	// when we pass a frame to the encoder, it may keep a reference to it
	// internally; make sure we do not overwrite it here.
	int ret = av_frame_make_writable(frame);
	if (ret >= 0)
	{
		int nb = (int)(1.0 * ost->enc->sample_rate / ost->enc->sample_rate * frame->nb_samples);
		swr_convert(ost->swr_ctx, (UINT8 **)(frame->extended_data), frame->nb_samples, (const UINT8 **)(&buf), nb);
	}
	else
		return NULL;

	return frame;
}

// if a frame is provided, send it to the encoder, otherwise flush the encoder;
// return 1 when encoding is finished, 0 otherwise
static int AudioEncoder_WriteFrame(AVFormatContext *oc, encoderstream_t *ost, AVFrame *frame)
{
	AVPacket pkt = {0};
	int ret;

	frame->pts = ost->next_pts;
	ost->next_pts += frame->nb_samples;

	ret = avcodec_send_frame(ost->enc, frame);
	if (ret < 0)
	{
		CONS_Alert(CONS_ERROR, "Error submitting an audio frame for encoding\n");
		return ret;
	}

	av_init_packet(&pkt);
	ret = avcodec_receive_packet(ost->enc, &pkt);

	if (ret < 0 && ret != AVERROR(EAGAIN) && ret != AVERROR_EOF)
	{
		CONS_Alert(CONS_ERROR, "Error encoding an audio frame\n");
		return ret;
	}
	else if (ret >= 0)
	{
		av_packet_rescale_ts(&pkt, ost->enc->time_base, ost->st->time_base);
		pkt.stream_index = ost->st->index;

		// Write the compressed frame to the media file.
		ret = av_interleaved_write_frame(oc, &pkt);
		av_packet_unref(&pkt);

		if (ret < 0)
		{
			CONS_Alert(CONS_ERROR, "Error writing an audio frame\n");
			return ret;
		}
	}

	return 1;
}

// encode one audio frame and send it to the muxer
// return true when encoding is finished, false otherwise
static boolean AudioEncoder_ProcessStream(INT16 *buf, AVFormatContext *oc, encoderstream_t *ost)
{
	AVFrame *frame = AudioEncoder_GetFrame(buf, ost);
	if (frame)
		AudioEncoder_WriteFrame(oc, ost, frame);
	return (frame != NULL);
}

static void AudioEncoder_GetStreamPath(char *filename)
{
	char *fn = Z_StrDup(filename);
	char *src = fn + strlen(fn) - 1;
	size_t baselen = 0, len = 0;

	while (src != fn)
	{
		if (*src == PATHSEP[0])
		{
			src++;
			(*src) = '\0';
			baselen = (src - fn);
			len = baselen + strlen(AUDIO_STREAM_NAME) + 1;
			break;
		}
		src--;
	}

	audio_out_filename = ZZ_Alloc(len);
	snprintf(audio_out_filename, len, "%s%s", fn, AUDIO_STREAM_NAME);

	Z_Free(fn);
}

static void AudioEncoder_OpenStream(char *filename)
{
	AudioEncoder_GetStreamPath(filename);
	audio_out_stream = fopen(audio_out_filename, "wb");
}

static void AudioEncoder_EncodeToVideo(void)
{
	encoderstream_t *stream = &audiostream;
	AVCodecContext *c = stream->enc;
	UINT8 *buf;
	size_t read;
	INT32 total = audio_out_size;

	read = c->frame_size * c->channels * 2;
	buf = calloc(read, 1);
	if (!buf)
		return;

	while (total)
	{
		if (fread(buf, 1, read, audio_out_stream) < read)
			break;
		AudioEncoder_ProcessStream((INT16 *)buf, formatcontext, stream);
		memset(buf, 0x00, read);
		total -= read;
	}

	free(buf);
}

static void AudioEncoder_Finish(void)
{
	if (audio_out_stream)
		fclose(audio_out_stream);

	if (audio_out_filename)
	{
		if (encoding_audio)
		{
			audio_out_stream = fopen(audio_out_filename, "rb");

			if (audio_out_stream)
			{
				fseek(audio_out_stream, 0, SEEK_END);
				audio_out_size = ftell(audio_out_stream);
				fseek(audio_out_stream, 0, SEEK_SET);
				AudioEncoder_EncodeToVideo();
				fclose(audio_out_stream);
			}
		}

		remove(audio_out_filename);
		Z_Free(audio_out_filename);
	}

	audio_out_stream = NULL;
	audio_out_filename = NULL;
}

//
// VIDEO OUTPUT
//

static boolean Encoder_AddVideoStream(encoderstream_t *stream, AVFormatContext *oc, enum AVCodecID codec_id)
{
	AVCodecContext *c;
	AVCodec *codec;

	// find the video encoder
	codec = avcodec_find_encoder(codec_id);
	if (!codec)
	{
		CONS_Alert(CONS_ERROR, "Video codec not found\n");
		return false;
	}

	stream->st = avformat_new_stream(oc, NULL);
	if (!stream->st)
	{
		CONS_Alert(CONS_ERROR, "Could not allocate a video stream\n");
		return false;
	}

	c = avcodec_alloc_context3(codec);
	if (!c)
	{
		CONS_Alert(CONS_ERROR, "Could not allocate a video encoding context\n");
		return false;
	}

	stream->enc = c;

	c->width = stream->width;
	c->height = stream->height;
	c->bit_rate = cv_videoencoder_bitrate.value * 100000;

	// timebase: This is the fundamental unit of time (in seconds) in terms
	// of which frame timestamps are represented. For fixed-fps content,
	// timebase should be 1/framerate and timestamp increments should be
	// identical to 1.
	stream->st->time_base = (AVRational){1, Encoder_GetFramerate()};
	c->time_base = stream->st->time_base;

	c->gop_size = (int)(FixedToFloat(cv_videoencoder_gopsize.value) * TICRATE);
	c->pix_fmt = (Encoder_IsRecordingGIF() ? AV_PIX_FMT_PAL8 : AV_PIX_FMT_YUV420P);

	if (c->codec_id == AV_CODEC_ID_MPEG1VIDEO)
	{
		// Needed to avoid using macroblocks in which some coeffs overflow.
		// This does not happen with normal video, it just happens here as
		// the motion of the chroma plane does not match the luma plane.
		c->mb_decision = 2;
	}

	// Some formats want stream headers to be separate.
	if (oc->oformat->flags & AVFMT_GLOBALHEADER)
		c->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;

	return true;
}

static AVFrame *Encoder_AllocateVideoFrame(enum AVPixelFormat pix_fmt, int width, int height)
{
	AVFrame *picture;
	int ret;

	picture = av_frame_alloc();
	if (!picture)
		return NULL;

	picture->format = pix_fmt;
	picture->width  = width;
	picture->height = height;

	// allocate the buffers for the frame data
	ret = av_frame_get_buffer(picture, 32);
	if (ret < 0)
	{
		CONS_Alert(CONS_ERROR, "Could not allocate frame data\n");
		return NULL;
	}

	return picture;
}

static boolean Encoder_OpenVideoStream(encoderstream_t *stream)
{
	AVCodecContext *c = stream->enc;
	int ret;

	// open the codec
	if (avcodec_open2(c, NULL, NULL) < 0)
	{
		CONS_Alert(CONS_ERROR, "Could not open codec\n");
		return false;
	}

	// Allocate the encoded raw picture.
	stream->frame = Encoder_AllocateVideoFrame(c->pix_fmt, c->width, c->height);
	if (!stream->frame)
	{
		CONS_Alert(CONS_ERROR, "Could not allocate picture\n");
		return false;
	}

	// If the output format is not YUV420P, then a temporary YUV420P
	// picture is needed too. It is then converted to the required
	// output format.
	stream->tmp_frame = NULL;
	if (c->pix_fmt != AV_PIX_FMT_YUV420P && !Encoder_IsRecordingGIF())
	{
		stream->tmp_frame = Encoder_AllocateVideoFrame(AV_PIX_FMT_YUV420P, c->width, c->height);
		if (!stream->tmp_frame)
		{
			CONS_Alert(CONS_ERROR, "Could not allocate temporary picture\n");
			return false;
		}
	}

	// copy the stream parameters to the muxer
	ret = avcodec_parameters_from_context(stream->st->codecpar, c);
	if (ret < 0)
	{
		CONS_Alert(CONS_ERROR, "Could not copy the stream parameters\n");
		return false;
	}

	return true;
}

#define clamp(X) ( (X) > 255 ? 255 : (X) < 0 ? 0 : X)
#define CRGB2Y(R, G, B) clamp((19595 * R + 38470 * G + 7471 * B ) >> 16)
#define CRGB2Cb(R, G, B) clamp((36962 * (B - clamp((19595 * R + 38470 * G + 7471 * B ) >> 16) ) >> 16) + 128)
#define CRGB2Cr(R, G, B) clamp((46727 * (R - clamp((19595 * R + 38470 * G + 7471 * B ) >> 16) ) >> 16) + 128)

static void Encoder_ConvertVideoBuffer(UINT32 *movie_screen, AVFrame *pict, RGBA_t *palette, int width, int height)
{
	UINT32 color = 0xFFFFFFFF;
	UINT8 r, g, b;
	int x, y, ret;

	// when we pass a frame to the encoder, it may keep a reference to it
	// internally; make sure we do not overwrite it here.
	ret = av_frame_make_writable(pict);
	if (ret < 0)
		return;

	if (Encoder_IsRecordingGIF())
	{
		UINT8 *scr = (UINT8 *)movie_screen;
		UINT32 *pal = (UINT32 *)(&pict->data[1][0]);

		// Write the palette
		for (y = 0; y < 256; y++)
		{
			RGBA_t *idx = &palette[y];
			*pal = LONG((0xFF << 24) | (idx->s.red << 16) | (idx->s.green << 8) | idx->s.blue); // BGRA
			pal++;
		}

		// Write the paletted image
		for (y = 0; y < height; y++)
			for (x = 0; x < width; x++)
			{
				color = scr[(y * width) + x];
				pict->data[0][y * pict->linesize[0] + x] = color;
			}
	}
	else
	{
		// Convert RGB to YUV420
		for (y = 0; y < height; y++)
		{
			for (x = 0; x < width; x++)
			{
				color = movie_screen[(y * width) + x];
				r = R_GetRgbaR(color);
				g = R_GetRgbaG(color);
				b = R_GetRgbaB(color);

				// Y
				pict->data[0][y * pict->linesize[0] + x] = CRGB2Y(r, g, b);

				// Cb and Cr
				pict->data[1][(y>>1) * pict->linesize[1] + (x>>1)] = CRGB2Cb(r, g, b);
				pict->data[2][(y>>1) * pict->linesize[2] + (x>>1)] = CRGB2Cr(r, g, b);
			}
		}
	}
}

static AVFrame *Encoder_GetVideoFrame(encoderstream_t *stream)
{
	AVCodecContext *c = stream->enc;
	UINT32 rate = Encoder_GetFramerate();

	if (c->pix_fmt != AV_PIX_FMT_YUV420P && !Encoder_IsRecordingGIF())
	{
		if (!stream->sws_ctx)
		{
			stream->sws_ctx = sws_getContext(c->width, c->height, AV_PIX_FMT_YUV420P, c->width, c->height, c->pix_fmt, SWS_BICUBIC, NULL, NULL, NULL);
			if (!stream->sws_ctx)
			{
				CONS_Alert(CONS_ERROR, "Could not initialize the conversion context\n");
				return NULL;
			}
		}

		Encoder_ConvertVideoBuffer(stream->buffer, stream->tmp_frame, NULL, c->width, c->height);
		sws_scale(stream->sws_ctx, (const UINT8 * const *) stream->tmp_frame->data, stream->tmp_frame->linesize, 0, c->height, stream->frame->data, stream->frame->linesize);
	}
	else
		Encoder_ConvertVideoBuffer(stream->buffer, stream->frame, stream->palette, c->width, c->height);

	stream->frame->pts = stream->next_pts;
	stream->delayus += I_PreciseToMicros(I_GetPreciseTime() - stream->prevframetime);

	if (stream->delayus/1000 > rate)
	{
		int frames = (stream->delayus/1000) / rate;
		stream->next_pts += frames;
		stream->delayus -= frames*(rate*1000);
	}
	else
		stream->next_pts++;

	return stream->frame;
}

static int Encoder_WriteVideoFrame(AVFormatContext *oc, encoderstream_t *stream)
{
	int ret;

	AVCodecContext *c;
	AVFrame *frame;
	AVPacket pkt = {0};

	c = stream->enc;
	frame = Encoder_GetVideoFrame(stream);

	ret = avcodec_send_frame(c, frame);
	if (ret < 0)
	{
		CONS_Alert(CONS_ERROR, "Error submitting a video frame for encoding\n");
		return ret;
	}

	av_init_packet(&pkt);
	ret = avcodec_receive_packet(c, &pkt);

	if (ret < 0 && ret != AVERROR(EAGAIN) && ret != AVERROR_EOF)
	{
		CONS_Alert(CONS_ERROR, "Error encoding a video frame\n");
		return ret;
	}
	else if (ret >= 0)
	{
		av_packet_rescale_ts(&pkt, c->time_base, stream->st->time_base);
		pkt.stream_index = stream->st->index;

		// Write the compressed frame to the media file.
		ret = av_interleaved_write_frame(oc, &pkt);
		if (ret < 0)
			CONS_Alert(CONS_ERROR, "Error writing a video frame\n");
	}

	return ret;
}

static void Encoder_CloseStream(encoderstream_t *stream)
{
	if (stream->enc)
		avcodec_free_context(&stream->enc);
	stream->enc = NULL;

	if (stream->frame)
		av_frame_free(&stream->frame);
	stream->frame = NULL;

	if (stream->type == ENCSTREAM_VIDEO)
	{
		if (stream->tmp_frame)
			av_frame_free(&stream->tmp_frame);
		if (stream->sws_ctx)
			sws_freeContext(stream->sws_ctx);
		stream->tmp_frame = NULL;
		stream->sws_ctx = NULL;
	}
	else if (stream->type == ENCSTREAM_AUDIO)
	{
		if (stream->swr_ctx)
			swr_free(&stream->swr_ctx);
		stream->swr_ctx = NULL;
	}

	stream->next_pts = 0;
	stream->tic = 0;

	if (stream->buffer)
	{
		Z_Free(stream->buffer);
		stream->buffer = NULL;
	}

	stream->palette = NULL;
}
#endif

boolean VideoEncoder_Start(char *filename)
{
#ifdef HAVE_LIBAV
	encoderstream_t *stream = &videostream;
	encoderstream_t *sndstream = &audiostream;
	size_t bitdepth;

	// Initialize libavcodec, and register all codecs and formats.
	av_register_all();

	// Autodetect the output format from the name. Default is MP4.
	outputformat = av_guess_format(NULL, filename, NULL);

	if (!outputformat)
	{
		CONS_Alert(CONS_NOTICE, "Could not deduce the output format from the file extension: using MP4.\n");
		outputformat = av_guess_format("mp4", NULL, NULL);
	}

	if (!outputformat)
	{
		CONS_Alert(CONS_ERROR, "Could not find a suitable output format\n");
		return false;
	}

	// Allocate the output media context.
	formatcontext = avformat_alloc_context();
	if (!formatcontext)
	{
		CONS_Alert(CONS_ERROR, "Could not allocate a video context\n");
		return false;
	}

	formatcontext->oformat = outputformat;
	snprintf(formatcontext->filename, sizeof(formatcontext->filename), "%s", filename);

	if (cv_videoencoder_downscale.value)
		stream->downscaling = vid.dupx;
	else
		stream->downscaling = 1;

	stream->width = vid.width / stream->downscaling;
	stream->height = vid.height / stream->downscaling;

	// Add the audio video streams using the default format codecs
	// and initialize the codecs.
	if (outputformat->video_codec == AV_CODEC_ID_NONE)
		return false;

	stream->type = ENCSTREAM_VIDEO;
	sndstream->type = ENCSTREAM_AUDIO;

	if (!Encoder_AddVideoStream(stream, formatcontext, outputformat->video_codec))
		return false;

	if (Encoder_CanRecordAudio())
	{
		AudioEncoder_OpenStream(filename);

		if (audio_out_stream)
			recording_audio = encoding_audio = Encoder_AddAudioStream(sndstream, formatcontext, outputformat->audio_codec);

		if (!recording_audio) // damn.................................. Okay
			CONS_Alert(CONS_WARNING, "Audio will not be recorded\n");
	}
	else
	{
		AudioEncoder_GetStreamPath(filename);
		AudioEncoder_Finish(); // Remove any leftover audiostream.raw
		recording_audio = encoding_audio = false;
	}

	// Now that all the parameters are set, we can open the video codec
	// and allocate the necessary encode buffers.
	if (!Encoder_OpenVideoStream(stream))
		return false;

	if (encoding_audio)
		encoding_audio = Encoder_OpenAudioStream(sndstream);

	av_dump_format(formatcontext, 0, filename, 1);

	// Open the output file, if needed.
	if (!(outputformat->flags & AVFMT_NOFILE))
	{
		if (avio_open(&formatcontext->pb, filename, AVIO_FLAG_WRITE) < 0)
		{
			CONS_Alert(CONS_ERROR, "Could not open '%s'\n", filename);
			Encoder_CloseStream(stream);
			return false;
		}
	}

	// Write the stream header, if any.
	if (avformat_write_header(formatcontext, NULL) < 0)
		return false;

	bitdepth = (Encoder_IsRecordingGIF() ? sizeof(UINT8) : sizeof(UINT32));
	stream->buffer = Z_Calloc(stream->width * stream->height * bitdepth, PU_STATIC, NULL);
	stream->prevframetime = I_GetPreciseTime();
	stream->delayus = stream->tic = 0;

	return true;
#else
	return false;
#endif
}

#ifdef HWRENDER
static colorlookup_t encoder_colorlookup;
#endif

boolean VideoEncoder_WriteFrame(void)
{
#ifdef HAVE_LIBAV
	int x, y;
	size_t bitdepth = 1;

	encoderstream_t *stream = &videostream;

	UINT8 *base_screen = screens[0];
	UINT32 *movie_screen = stream->buffer;
	RGBA_t *palette = NULL;

#ifdef HWRENDER
	UINT8 *hwr_screen = NULL;
	UINT8 r, g, b;

	if (rendermode == render_opengl)
	{
		hwr_screen = base_screen = HWR_GetScreenshot();
		palette = pLocalPalette;
		bitdepth = 3;
	}
	else
#endif
		palette = &pLocalPalette[max(st_palette, 0) * 256];

#define GETSCREENLINE(line) base_screen + (((line * vid.width) * stream->downscaling) * bitdepth)
#define GETPIXELRGB(source) \
	r = *(source); \
	g = *(source + 1); \
	b = *(source + 2)
#define STEPSCREEN(source) source += (bitdepth * stream->downscaling)

	// GIF encoding
	if (Encoder_IsRecordingGIF())
	{
		UINT8 *movie_screen_8 = (UINT8 *)(movie_screen);

		stream->palette = palette;

#ifdef HWRENDER
		// OpenGL buffer
		if (hwr_screen)
		{
			InitColorLUT(&encoder_colorlookup, palette, false);

			for (y = 0; y < stream->height; y++)
			{
				UINT8 *scr = (UINT8 *)(GETSCREENLINE(y));
				for (x = 0; x < stream->width; x++)
				{
					GETPIXELRGB(scr);
					movie_screen_8[(y * stream->width) + x] = GetColorLUT(&encoder_colorlookup, r, g, b);
					STEPSCREEN(scr);
				}
			}
		}
		else
#endif
		{
			// Software buffer
			for (y = 0; y < stream->height; y++)
			{
				UINT8 *scr = (UINT8 *)(GETSCREENLINE(y));
				for (x = 0; x < stream->width; x++)
				{
					movie_screen_8[(y * stream->width) + x] = (*scr);
					STEPSCREEN(scr);
				}
			}
		}
	}
	else
	{
#ifdef HWRENDER
		// OpenGL buffer
		if (hwr_screen)
		{
			for (y = 0; y < stream->height; y++)
			{
				UINT8 *scr = (UINT8 *)(GETSCREENLINE(y));
				for (x = 0; x < stream->width; x++)
				{
					GETPIXELRGB(scr);
					movie_screen[(y * stream->width) + x] = R_PutRgbaRGBA(r, g, b, 0xFF);
					STEPSCREEN(scr);
				}
			}
		}
		else
#endif
		{
			// Software buffer
			for (y = 0; y < stream->height; y++)
			{
				UINT8 *scr = (UINT8 *)(GETSCREENLINE(y));
				for (x = 0; x < stream->width; x++)
				{
					movie_screen[(y * stream->width) + x] = palette[(*scr)].rgba;
					STEPSCREEN(scr);
				}
			}
		}
	}

#ifdef HWRENDER
	if (hwr_screen)
		free(hwr_screen);
#endif

#undef GETSCREENLINE
#undef GETPIXELRGB
#undef STEPSCREEN

	stream->lastpacket = Encoder_WriteVideoFrame(formatcontext, stream);
	stream->prevframetime = I_GetPreciseTime();
	stream->tic++;

#endif

	return true;
}

boolean VideoEncoder_Stop(void)
{
#ifdef HAVE_LIBAV
	encoderstream_t *stream = &videostream;
	encoderstream_t *sndstream = &audiostream;
	int needed = 1;

	AVPacket pkt = {0};
	av_init_packet(&pkt);

	recording_audio = false;

	if (encoding_audio)
		AudioEncoder_Finish();

	// Give the encoder some frames
	while (stream->lastpacket == AVERROR(EAGAIN))
		stream->lastpacket = Encoder_WriteVideoFrame(formatcontext, stream);

	// Let the encoder finish
	while (needed)
	{
		AVCodecContext *ctx = stream->enc;
		int ret;

		avcodec_encode_video2(ctx, &pkt, NULL, &needed);

		if (needed)
		{
			av_packet_rescale_ts(&pkt, ctx->time_base, stream->st->time_base);
			pkt.stream_index = stream->st->index;

			// Write the compressed frame to the media file.
			ret = av_interleaved_write_frame(formatcontext, &pkt);
			av_packet_unref(&pkt);

			if (ret < 0)
				break;
		}
	}

	avcodec_send_frame(stream->enc, NULL); // Probably not needed, but flushes the stream.
	av_write_trailer(formatcontext);

	Encoder_CloseStream(stream);
	if (encoding_audio)
		Encoder_CloseStream(sndstream);

	if (!(outputformat->flags & AVFMT_NOFILE))
		avio_close(formatcontext->pb);
	avformat_free_context(formatcontext);
#endif

	return true;
}

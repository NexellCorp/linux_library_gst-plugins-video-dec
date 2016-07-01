/*
 * GStreamer
 * Copyright (C) 2005 Thomas Vander Stichele <thomas@apestaart.org>
 * Copyright (C) 2005 Ronald S. Bultje <rbultje@ronald.bitfreak.net>
 * Copyright (C) 2016 ray <<user@hostname.org>>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 *
 * Alternatively, the contents of this file may be used under the
 * GNU Lesser General Public License Version 2.1 (the "LGPL"), in
 * which case the following provisions apply instead of the ones
 * mentioned above:
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

/**
 * SECTION:element-nxvideodec
 *
 * FIXME:Describe nxvideodec here.
 *
 * <refsect2>
 * <title>Example launch line</title>
 * |[
 * gst-launch -v -m fakesrc ! nxvideodec ! fakesink silent=TRUE
 * ]|
 * </refsect2>
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>
#include <gst/gst.h>
#include <gst/video/video.h>
#include <gst/video/gstvideodecoder.h>
#include <gstmmvideobuffermeta.h>
#include <linux/videodev2.h>
#include "gstnxvideodec.h"

GST_DEBUG_CATEGORY_STATIC (gst_nxvideodec_debug_category);
#define GST_CAT_DEFAULT gst_nxvideodec_debug_category

/* prototypes */
static void gst_nxvideodec_set_property (GObject * object,
		guint property_id, const GValue * value, GParamSpec * pspec);
static void gst_nxvideodec_get_property (GObject * object,
		guint property_id, GValue * value, GParamSpec * pspec);

static gboolean gst_nxvideodec_start (GstVideoDecoder * decoder);
static gboolean gst_nxvideodec_stop (GstVideoDecoder * decoder);
static gboolean gst_nxvideodec_set_format (GstVideoDecoder * decoder,
		GstVideoCodecState * state);
static gboolean gst_nxvideodec_flush (GstVideoDecoder * decoder);
static GstFlowReturn gst_nxvideodec_handle_frame (GstVideoDecoder * decoder,
		GstVideoCodecFrame * frame);
static void nxvideodec_base_init (gpointer gclass);
static void nxvideodec_buffer_finalize(GstNxDecOutBuffer *pBuffer);
static GstNxDecOutBuffer *nxvideodec_buffer_new(void);
static GstMemory *nxvideodec_copy_data(NX_V4L2DEC_OUT *pDecOut);

enum
{
	PROP_0,
	PROP_TYPE	//0: 1:MM_VIDEO_BUFFER_TYPE_GEM
};

enum
{
	BUFFER_TYPE_NORMAL,
	BUFFER_TYPE_GEM
};

enum
{
	STOP,
	PLAY
};

#define	PLUGIN_LONG_NAME		"S5P6818 H/W Video Decoder"
#define PLUGIN_DESC				"Nexell H/W Video Decoder for S5P6818, Version: 0.1.0"
#define	PLUGIN_AUTHOR			"Hyun Chul Jun <hcjun@nexell.co.kr>"

// pad templates
static GstStaticPadTemplate gst_nxvideodec_src_template =
GST_STATIC_PAD_TEMPLATE ("src",
		GST_PAD_SRC,
		GST_PAD_ALWAYS,
		GST_STATIC_CAPS ("video/x-raw, "
						 "format = (string) { I420 }, "
						 "width = (int) [ 64, 1920 ], "
						"height = (int) [ 64, 1088 ] "
										)

		);


static void
nxvideodec_base_init (gpointer gclass)
{
	GstElementClass *pElement_class = GST_ELEMENT_CLASS (gclass);
	GstCaps *pCapslist = NULL;
	GstNxVideoDecClass *pKlass = GST_NXVIDEODEC_CLASS(pElement_class);

	FUNC_IN();

	gst_element_class_set_details_simple(
		pElement_class,
		PLUGIN_LONG_NAME,
		"Codec/Decoder/Video",
		PLUGIN_DESC,
		PLUGIN_AUTHOR
		);

	pCapslist = gst_caps_new_empty();

	//	H.263
	gst_caps_append_structure (
		pCapslist,
		gst_structure_new(
			"video/x-h263",
			"variant", G_TYPE_STRING, "itu",
			NULL) );

	//	H.264
	gst_caps_append_structure (
		pCapslist,
		gst_structure_new(
			"video/x-h264",
			"width", GST_TYPE_INT_RANGE, 64, NX_MAX_WIDTH,
			"height", GST_TYPE_INT_RANGE, 64, NX_MAX_HEIGHT,
			NULL) );

	//	XVID
	gst_caps_append_structure (
		pCapslist,
		gst_structure_new(
			"video/x-xvid",
			"width", GST_TYPE_INT_RANGE, 64, NX_MAX_WIDTH,
			"height", GST_TYPE_INT_RANGE, 64, NX_MAX_HEIGHT,
			NULL) );


	//	MPEG2
	gst_caps_append_structure (
		pCapslist,
		gst_structure_new(
			"video/mpeg",
			"mpegversion", GST_TYPE_INT_RANGE, 1, 2,
			"systemstream", G_TYPE_BOOLEAN, FALSE,
			NULL) );

	//	MPEG4
	gst_caps_append_structure (
		pCapslist,
		gst_structure_new(
			"video/mpeg",
			"mpegversion", G_TYPE_INT, 4,
			"systemstream", G_TYPE_BOOLEAN, FALSE,
			NULL) );

	//	DIVX
	gst_caps_append_structure (
		pCapslist,
		gst_structure_new(
			"video/x-divx",
			"width", GST_TYPE_INT_RANGE, 64, NX_MAX_WIDTH,
			"height", GST_TYPE_INT_RANGE, 64, NX_MAX_HEIGHT,
			"divxversion", GST_TYPE_INT_RANGE, 3, 6,
			NULL) );

	//	MSMPEG
	gst_caps_append_structure (
		pCapslist,
		gst_structure_new(
			"video/x-msmpeg",
			"width", GST_TYPE_INT_RANGE, 64, NX_MAX_WIDTH,
			"height", GST_TYPE_INT_RANGE, 64, NX_MAX_HEIGHT,
			"msmpegversion", G_TYPE_INT, 43,
			NULL) );

	// pad templates
	pKlass->pSinktempl = gst_pad_template_new ("sink", GST_PAD_SINK, GST_PAD_ALWAYS, pCapslist);
	gst_element_class_add_pad_template (pElement_class, pKlass->pSinktempl);
	gst_element_class_add_pad_template (pElement_class, gst_static_pad_template_get (&gst_nxvideodec_src_template));

	FUNC_OUT();
}

G_DEFINE_TYPE_WITH_CODE (GstNxVideoDec, gst_nxvideodec, GST_TYPE_VIDEO_DECODER,
	GST_DEBUG_CATEGORY_INIT (gst_nxvideodec_debug_category, "nxvideodec", 0,
	"debug category for nxvideodec element"));

// class initialization
static void
gst_nxvideodec_class_init (GstNxVideoDecClass * pKlass)
{
	FUNC_IN();

	GObjectClass *pGobjectClass = G_OBJECT_CLASS (pKlass);
	GstVideoDecoderClass *pVideoDecoderClass = GST_VIDEO_DECODER_CLASS (pKlass);

	nxvideodec_base_init(pKlass);

	pGobjectClass->set_property = gst_nxvideodec_set_property;
	pGobjectClass->get_property = gst_nxvideodec_get_property;

	pVideoDecoderClass->start = GST_DEBUG_FUNCPTR (gst_nxvideodec_start);
	pVideoDecoderClass->stop = GST_DEBUG_FUNCPTR (gst_nxvideodec_stop);

	pVideoDecoderClass->set_format = GST_DEBUG_FUNCPTR (gst_nxvideodec_set_format);
	pVideoDecoderClass->flush = GST_DEBUG_FUNCPTR (gst_nxvideodec_flush);
	pVideoDecoderClass->handle_frame = GST_DEBUG_FUNCPTR (gst_nxvideodec_handle_frame);

	g_object_class_install_property (
		pGobjectClass,
		PROP_TYPE,
		g_param_spec_int ("buffer-type", "buffer-type", "Buffer Type(0:NORMAL 1:MM_VIDEO_BUFFER_TYPE_GEM)", 0, 1, BUFFER_TYPE_GEM, G_PARAM_READWRITE));

	FUNC_OUT();
}

static void
gst_nxvideodec_init (GstNxVideoDec *pNxVideoDec)
{
	FUNC_IN();

	GST_DEBUG_OBJECT (pNxVideoDec, "dec_init");

	// Initialize variables
	pNxVideoDec->pNxVideoDecHandle = NULL;
	pNxVideoDec->pInputState = NULL;
	pNxVideoDec->isState = STOP;
	pNxVideoDec->bufferType = BUFFER_TYPE_GEM;
	pthread_mutex_init(&pNxVideoDec->mutex, NULL);

	FUNC_OUT();
}

void
gst_nxvideodec_set_property (GObject *pObject, guint propertyId,
		const GValue *pValue, GParamSpec *pPspec)
{
	GstNxVideoDec *pNxvideodec = GST_NXVIDEODEC (pObject);
	FUNC_IN();

	GST_DEBUG_OBJECT (pNxvideodec, "set_property");

	switch (propertyId)
	{
		case PROP_TYPE:
			pNxvideodec->bufferType = g_value_get_int(pValue);
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (pObject, propertyId, pPspec);
			break;
	}

	FUNC_OUT();
}

void
gst_nxvideodec_get_property (GObject *pObject, guint propertyId,
		GValue *pValue, GParamSpec *pPspec)
{
	GstNxVideoDec *pNxvideodec = GST_NXVIDEODEC (pObject);
	FUNC_IN();

	GST_DEBUG_OBJECT (pNxvideodec, "get_property");

	switch (propertyId)
	{
		case PROP_TYPE:
			g_value_set_int(pValue, pNxvideodec->bufferType);
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (pObject, propertyId, pPspec);
			break;
	}

	FUNC_OUT();
}

static gboolean
gst_nxvideodec_start (GstVideoDecoder *pDecoder)
{
	GstNxVideoDec *pNxVideoDec = GST_NXVIDEODEC (pDecoder);
	FUNC_IN();

	GST_DEBUG_OBJECT (pNxVideoDec, "start");

	if (pNxVideoDec->pNxVideoDecHandle)
	{
		CloseVideoDec(pNxVideoDec->pNxVideoDecHandle);
		pNxVideoDec->pNxVideoDecHandle = NULL;
	}

	pNxVideoDec->pNxVideoDecHandle = OpenVideoDec();

	if( pNxVideoDec->pNxVideoDecHandle == NULL )
	{
		GST_ERROR("VideoDecHandle is NULL !\n");
		return FALSE;
	}

	pthread_mutex_lock( &pNxVideoDec->mutex );
	pNxVideoDec->isState = PLAY;
	pthread_mutex_unlock( &pNxVideoDec->mutex );

	FUNC_OUT();

	return TRUE;
}

static gboolean
gst_nxvideodec_stop (GstVideoDecoder *pDecoder)
{
	GstNxVideoDec *pNxVideoDec = GST_NXVIDEODEC (pDecoder);
	FUNC_IN();

	GST_DEBUG_OBJECT (pNxVideoDec, "stop");

	pthread_mutex_lock( &pNxVideoDec->mutex );
	pNxVideoDec->isState = STOP;
	pthread_mutex_unlock( &pNxVideoDec->mutex );

	if( pNxVideoDec->pNxVideoDecHandle->pSem )
	{
		VDecSemSignal( pNxVideoDec->pNxVideoDecHandle->pSem );
		VDecSemDestroy( pNxVideoDec->pNxVideoDecHandle->pSem );
		pNxVideoDec->pNxVideoDecHandle->pSem = NULL;
	}

	if (pNxVideoDec->pNxVideoDecHandle)
	{
		 CloseVideoDec(pNxVideoDec->pNxVideoDecHandle);
	}

	pthread_mutex_destroy( &pNxVideoDec->mutex );

	FUNC_OUT();
	return TRUE;
}

static gboolean
gst_nxvideodec_set_format (GstVideoDecoder *pDecoder, GstVideoCodecState *pState)
{
	GstNxVideoDec *pNxVideoDec = GST_NXVIDEODEC (pDecoder);
	GstStructure *pStructure = NULL;
	const gchar *pMimeType = NULL;
	GstBuffer *pCodecData = NULL;
	GstVideoCodecState *pOutputState = NULL;
	NX_VIDEO_DEC_STRUCT *pDecHandle = NULL;
	gint ret = FALSE;

	FUNC_IN();

	GST_DEBUG_OBJECT (pNxVideoDec, "set_format");

	if (pNxVideoDec->pInputState)
	{
		gst_video_codec_state_unref (pNxVideoDec->pInputState);
		pNxVideoDec->pInputState = NULL;
	}

	pNxVideoDec->pInputState = gst_video_codec_state_ref (pState);

	// Check Support Codec Type
	pStructure = gst_caps_get_structure( pNxVideoDec->pInputState->caps, 0 );
	pMimeType = gst_structure_get_name( pStructure );
	if( pNxVideoDec->pNxVideoDecHandle )
	{
		pDecHandle = pNxVideoDec->pNxVideoDecHandle;
	}
	else
	{
		return FALSE;
	}

	pDecHandle->codecType = FindCodecInfo(pState, pDecHandle);

	if ( pDecHandle->codecType < 0 )
	{
		GST_ERROR( "Unsupported VideoDecoder Mime Type : %s\n", pMimeType );
		return FALSE;
	}

	if( pDecHandle->pExtraData )
	{
		g_free(pDecHandle->pExtraData);
		pDecHandle->pExtraData = NULL;
		pDecHandle->extraDataSize = 0;
	}

	pCodecData = pNxVideoDec->pInputState->codec_data;

	if(pCodecData)
	{
		GstMapInfo mapInfo;

		if (!gst_buffer_map (pCodecData, &mapInfo, GST_MAP_READ))
		{
			GST_ERROR( "Cannot map input buffer!\n");
			return FALSE;
		}

		if( mapInfo.size > 0 && mapInfo.data )
		{
			pDecHandle->pExtraData = (guint8*)g_malloc( mapInfo.size );
			pDecHandle->extraDataSize = mapInfo.size;
		}

		if( FALSE == GetExtraInfo(pDecHandle, (guint8 *)mapInfo.data, mapInfo.size) )
		{
			gst_buffer_unmap (pCodecData, &mapInfo);
			return FALSE;
		}
		gst_buffer_unmap (pCodecData, &mapInfo);
	}
	else
	{
		g_print("No Codec Data\n");
	}

	if ( pDecHandle->codecType == V4L2_PIX_FMT_H264 )
	{
		const gchar *pStr = NULL;

		if ((pStr = gst_structure_get_string (pStructure, "alignment")))
		{
			if (strcmp (pStr, "au") == 0)
			{
				pDecHandle->h264Alignment = H264_PARSE_ALIGN_AU;
				GST_DEBUG_OBJECT (pNxVideoDec, ">>>>> H264 alignment: au Type.");
			}
			else if (strcmp (pStr, "nal") == 0)
			{
				pDecHandle->h264Alignment = H264_PARSE_ALIGN_NAL;
				GST_DEBUG_OBJECT (pNxVideoDec, ">>>>> H264 alignment: nal Type.");
			}
			else
			{
				GST_DEBUG_OBJECT (pNxVideoDec, "unknown alignment: %s", pStr);
			}
		}
	}

	pOutputState =	gst_video_decoder_set_output_state (pDecoder, GST_VIDEO_FORMAT_I420,
								pDecHandle->width, pDecHandle->height, pNxVideoDec->pInputState);

	pOutputState->caps = gst_caps_new_simple ("video/x-raw",
			"format", G_TYPE_STRING, gst_video_format_to_string (GST_VIDEO_FORMAT_I420),
			"width", G_TYPE_INT, pDecHandle->width,
			"height", G_TYPE_INT, pDecHandle->height,
			"framerate", GST_TYPE_FRACTION, pDecHandle->fpsNum, pDecHandle->fpsDen, NULL);

	gst_video_codec_state_unref( pOutputState );

	pNxVideoDec->pNxVideoDecHandle->imgPlaneNum = 3;
	if( BUFFER_TYPE_GEM == pNxVideoDec->bufferType )
	{
		pNxVideoDec->pNxVideoDecHandle->imgPlaneNum = 1;
		GST_DEBUG_OBJECT( pNxVideoDec, ">>>>> Accelerable.");
	}

	ret = gst_video_decoder_negotiate( pDecoder );

	if( FALSE == ret)
	{
		GST_ERROR( "Fail Negotiate !\n");
		return ret;
	}

	if( 0 != InitVideoDec(pNxVideoDec->pNxVideoDecHandle) )
	{
		return FALSE;
	}

	FUNC_OUT();

	return ret;
}

static gboolean
gst_nxvideodec_flush (GstVideoDecoder *pDecoder)
{
	GstNxVideoDec *pNxvideodec = GST_NXVIDEODEC (pDecoder);

	FUNC_IN();

	GST_DEBUG_OBJECT (pNxvideodec, "flush");

	if( pNxvideodec->pNxVideoDecHandle )
	{
		pNxvideodec->pNxVideoDecHandle->bFlush = TRUE;
	}

	FUNC_OUT();

	return TRUE;
}

static GstFlowReturn
gst_nxvideodec_handle_frame (GstVideoDecoder *pDecoder, GstVideoCodecFrame *pFrame)
{
	GstNxVideoDec *pNxVideoDec = GST_NXVIDEODEC (pDecoder);
	NX_V4L2DEC_OUT decOut;
	gint64 timeStamp = 0;
	GstMapInfo mapInfo;
	gint ret;
	gboolean bKeyFrame = FALSE;

	FUNC_IN();

	if (!gst_buffer_map (pFrame->input_buffer, &mapInfo, GST_MAP_READ))
	{
		GST_ERROR("Cannot map input buffer!");
		gst_video_codec_frame_unref (pFrame);
		return GST_FLOW_ERROR;
	}

	bKeyFrame = GST_VIDEO_CODEC_FRAME_IS_SYNC_POINT(pFrame);

	ret = VideoDecodeFrame(pNxVideoDec->pNxVideoDecHandle, pFrame->input_buffer, &decOut, bKeyFrame);

	gst_buffer_unmap (pFrame->input_buffer, &mapInfo);
	if( DEC_ERR == ret )
	{
		GetTimeStamp(pNxVideoDec->pNxVideoDecHandle, &timeStamp);
		return gst_video_decoder_drop_frame(pDecoder, pFrame);
	}
	else if( DEC_INIT_ERR == ret )
	{
		return GST_FLOW_ERROR;
	}

	if( decOut.dispIdx < 0 )
	{
		return GST_FLOW_OK;
	}

	GST_DEBUG_OBJECT( pNxVideoDec, " decOut.dispIdx: %d\n",decOut.dispIdx );

	if( BUFFER_TYPE_GEM == pNxVideoDec->bufferType )
	{
		GstNxDecOutBuffer *pDecOutBuffer = NULL;
		GstMemory *pMemCopyData = NULL;
		GstMemory *pMemDecOutBuffer = NULL;

		pDecOutBuffer = nxvideodec_buffer_new();
		pDecOutBuffer->v4l2BufferIdx = decOut.dispIdx;
		pDecOutBuffer->pNxVideoDec = pNxVideoDec;
		pMemCopyData = nxvideodec_copy_data(&decOut);

		if (!pMemCopyData)
		{
			GST_ERROR("failed to get zero copy data");
			return GST_FLOW_ERROR;
		}
		else
		{
			gst_buffer_append_memory(pDecOutBuffer->pGstBuffer, pMemCopyData);
		}

		pMemDecOutBuffer = gst_memory_new_wrapped(GST_MEMORY_FLAG_READONLY,
								pDecOutBuffer,
								sizeof(GstNxDecOutBuffer),
								0,
								sizeof(GstNxDecOutBuffer),
								pDecOutBuffer,
					(GDestroyNotify)nxvideodec_buffer_finalize);
		if (pMemDecOutBuffer)
		{
			gst_buffer_append_memory(pDecOutBuffer->pGstBuffer, pMemDecOutBuffer);
		}
		else
		{
			GST_ERROR("failed to create mem_decoutbuf");
			if (pMemDecOutBuffer)
			{
				nxvideodec_buffer_finalize(pDecOutBuffer);
				gst_buffer_unref((GstBuffer *)pDecOutBuffer);
			}
			return GST_FLOW_ERROR;
		}
		pFrame->output_buffer = pDecOutBuffer->pGstBuffer;
		gst_buffer_add_mmvideobuffer_meta( pDecOutBuffer->pGstBuffer, 0 );

		if( -1 == GetTimeStamp(pNxVideoDec->pNxVideoDecHandle, &timeStamp) )
		{
			GST_DEBUG_OBJECT (pNxVideoDec, "Cannot Found Time Stamp!!!");
		}
		pFrame->pts = timeStamp;
		GST_BUFFER_PTS(pFrame->output_buffer) = timeStamp;
	}
	else
	{
		GstVideoFrame videoFrame;
		NX_VID_MEMORY_INFO *pImg = NULL;
		guint8 *pPtr = NULL;;
		GstVideoCodecState *pState = NULL;
		GstFlowReturn flowRet;
		flowRet = gst_video_decoder_allocate_output_frame (pDecoder, pFrame);
		pState = gst_video_decoder_get_output_state (pDecoder);
		if (flowRet != GST_FLOW_OK)
		{
			gst_video_codec_state_unref (pState);
			gst_video_codec_frame_unref (pFrame);
			return flowRet;
		}

		if (!gst_video_frame_map (&videoFrame, &pState->info, pFrame->output_buffer, GST_MAP_WRITE))
		{
			GST_ERROR ("Cannot video frame map!\n");
			gst_video_codec_state_unref (pState);
			gst_video_codec_frame_unref (pFrame);
			return GST_FLOW_ERROR;
		}

		if( -1 == GetTimeStamp(pNxVideoDec->pNxVideoDecHandle, &timeStamp) )
		{
			GST_DEBUG_OBJECT (pNxVideoDec, "Cannot Found Time Stamp!!!");
		}
		pFrame->pts = timeStamp;
		GST_BUFFER_PTS(pFrame->output_buffer) = timeStamp;

		pImg = &decOut.hImg;
		pPtr = GST_VIDEO_FRAME_COMP_DATA (&videoFrame, 0);

		CopyImageToBufferYV12( (guint8*)pImg->pBuffer[0], (guint8*)pImg->pBuffer[1], (guint8*)pImg->pBuffer[2],
			pPtr, pImg->stride[0], pImg->stride[1], pNxVideoDec->pNxVideoDecHandle->width, pNxVideoDec->pNxVideoDecHandle->height );

		DisplayDone( pNxVideoDec->pNxVideoDecHandle, decOut.dispIdx );

		gst_video_frame_unmap (&videoFrame);
		gst_video_codec_state_unref (pState);
	}

	ret = gst_video_decoder_finish_frame (pDecoder, pFrame);

	FUNC_OUT();

	return ret;
}

static void nxvideodec_buffer_finalize(GstNxDecOutBuffer *pBuffer)
{
	gint ret = 0;

	FUNC_IN();

	if ( ( pBuffer->pNxVideoDec) && ( pBuffer->pNxVideoDec->pNxVideoDecHandle ) )
	{
		pthread_mutex_lock( &pBuffer->pNxVideoDec->mutex );
		if( PLAY == pBuffer->pNxVideoDec->isState )
		{
			GST_DEBUG_OBJECT( pBuffer->pNxVideoDec, "v4l2BufferIdx: %d\n",pBuffer->v4l2BufferIdx );
			ret = DisplayDone( pBuffer->pNxVideoDec->pNxVideoDecHandle, pBuffer->v4l2BufferIdx );
			if ( ret )
			{
				g_print("Fail: DisplayDone !");
			}
		}
		pthread_mutex_unlock( &pBuffer->pNxVideoDec->mutex );
	}
	else
	{
		GST_ERROR("Error: hCodec is null !");
	}

	if( pBuffer )
	{
		g_free(pBuffer);
	}

	FUNC_OUT();
}

static GstNxDecOutBuffer *nxvideodec_buffer_new(void)
{
	FUNC_IN();
	GstNxDecOutBuffer *pBuffer = NULL;

	pBuffer = (GstNxDecOutBuffer *)g_malloc(sizeof(GstNxDecOutBuffer));
	pBuffer->pGstBuffer = gst_buffer_new();
	FUNC_OUT();
	return pBuffer;
}

static GstMemory *nxvideodec_copy_data(NX_V4L2DEC_OUT *pDecOut)
{
	GstMemory *pMeta = NULL;
	MMVideoBuffer *pMMVideoBuf = NULL;

	pMMVideoBuf = (MMVideoBuffer *)g_malloc(sizeof(MMVideoBuffer));
	if (!pMMVideoBuf)
	{
		GST_ERROR("failed to alloc MMVideoBuffer");
		return NULL;
	}

	memset((void*)pMMVideoBuf, 0, sizeof(MMVideoBuffer));

	if( 1 == pDecOut->hImg.planes)
	{
		pMMVideoBuf->type = MM_VIDEO_BUFFER_TYPE_GEM;
		pMMVideoBuf->format = MM_PIXEL_FORMAT_I420;
		pMMVideoBuf->plane_num = 3;
		pMMVideoBuf->width[0] = pDecOut->hImg.width;
		pMMVideoBuf->height[0] = pDecOut->hImg.height;
		pMMVideoBuf->stride_width[0] = GST_ROUND_UP_32(pDecOut->hImg.stride[0]);
		pMMVideoBuf->stride_width[1] = GST_ROUND_UP_16(pMMVideoBuf->stride_width[0] >> 1);
		pMMVideoBuf->stride_width[2] = pMMVideoBuf->stride_width[1];
		pMMVideoBuf->stride_height[0] = GST_ROUND_UP_16(pDecOut->hImg.height);
		pMMVideoBuf->stride_height[1] = GST_ROUND_UP_16(pDecOut->hImg.height >> 1);
		pMMVideoBuf->stride_height[2] = pMMVideoBuf->stride_height[1];
		pMMVideoBuf->size[0] = pDecOut->hImg.size[0];
		pMMVideoBuf->data[0] = pDecOut->hImg.pBuffer[0];
		pMMVideoBuf->handle_num = 1;
		pMMVideoBuf->handle.gem[0] = pDecOut->hImg.flink[0];
		pMMVideoBuf->buffer_index = pDecOut->dispIdx;
	}
	else if( 3 == pDecOut->hImg.planes)
	{
		pMMVideoBuf->type = MM_VIDEO_BUFFER_TYPE_GEM;
		pMMVideoBuf->format = MM_PIXEL_FORMAT_I420;
		pMMVideoBuf->plane_num = 3;
		pMMVideoBuf->width[0] = pDecOut->hImg.width;
		pMMVideoBuf->height[0] = pDecOut->hImg.height;
		pMMVideoBuf->stride_width[0] = pDecOut->hImg.stride[0];
		pMMVideoBuf->stride_width[1] = pDecOut->hImg.stride[1];
		pMMVideoBuf->stride_width[2] = pDecOut->hImg.stride[2];
		pMMVideoBuf->size[0] = pDecOut->hImg.size[0];
		pMMVideoBuf->size[1] = pDecOut->hImg.size[1];
		pMMVideoBuf->size[2] = pDecOut->hImg.size[2];
		pMMVideoBuf->data[0] = pDecOut->hImg.pBuffer[0];
		pMMVideoBuf->data[1] = pDecOut->hImg.pBuffer[1];
		pMMVideoBuf->data[2] = pDecOut->hImg.pBuffer[2];
		pMMVideoBuf->handle_num = 3;
		pMMVideoBuf->handle.gem[0] = pDecOut->hImg.flink[0];
		pMMVideoBuf->handle.gem[1] = pDecOut->hImg.flink[1];
		pMMVideoBuf->handle.gem[2] = pDecOut->hImg.flink[2];
		pMMVideoBuf->buffer_index = pDecOut->dispIdx;
	}

	pMeta = gst_memory_new_wrapped(GST_MEMORY_FLAG_READONLY,
						pMMVideoBuf,
						sizeof(MMVideoBuffer),
						0,
						sizeof(MMVideoBuffer),
						pMMVideoBuf,
						g_free);

	return pMeta;
}

static gboolean
plugin_init (GstPlugin * plugin)
{
	gboolean ret;

	FUNC_IN();

	/* FIXME Remember to set the rank if it's an element that is meant
		 to be autoplugged by decodebin. */
	ret = gst_element_register (plugin, "nxvideodec", GST_RANK_NONE,
			GST_TYPE_NXVIDEODEC);
	FUNC_OUT();

	return ret;
}

#ifndef VERSION
#define VERSION "0.1.0"
#endif
#ifndef PACKAGE
#define PACKAGE "S5P6818 GStreamer PlugIn"
#endif
#ifndef PACKAGE_NAME
#define PACKAGE_NAME "S5P6818 GStreamer PlugIn"
#endif
#ifndef GST_PACKAGE_ORIGIN
#define GST_PACKAGE_ORIGIN "http://www.nexell.co.kr"
#endif

GST_PLUGIN_DEFINE (
	GST_VERSION_MAJOR,
	GST_VERSION_MINOR,
	nxvideodec,
	"Nexell H/W Video Decoder for S5P6818",
	plugin_init, VERSION,
	"LGPL",
	PACKAGE_NAME,
	GST_PACKAGE_ORIGIN)

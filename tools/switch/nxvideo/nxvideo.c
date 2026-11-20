// culdcept-nx (2026-11-20): the decoder of nxvideo.h on devkitPro's FFmpeg 7.1 (switch-ffmpeg with nvtegra). Built by
// tools/switch/nxvideo/build.sh into one object with its FFmpeg linked in and every symbol but nxv_* made local.
#include "nxvideo.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <libavcodec/avcodec.h>
#include <libavutil/frame.h>
#include <libavutil/hwcontext.h>
#include <libavutil/mem.h>
#include <libavutil/pixfmt.h>
#include <libavutil/version.h>

struct NxvDecoder {
  const AVCodec *codec;
  AVCodecContext *avctx;
  AVBufferRef *device;  // the NVDEC device, when hardware
  AVPacket *packet;
  AVFrame *frame;     // as decoded (AV_PIX_FMT_NVTEGRA on hardware)
  AVFrame *sw_frame;  // the hardware frame transferred to memory
  uint8_t *extradata;
  int extradata_size, width, height, hw, fell_back;
};

static enum AVPixelFormat pick_nvtegra(AVCodecContext *avctx, const enum AVPixelFormat *formats) {
  for (const enum AVPixelFormat *f = formats; *f != AV_PIX_FMT_NONE; ++f)
    if (*f == AV_PIX_FMT_NVTEGRA) return *f;
  return formats[0];  // the hardware path was not offered: software
}

// The decoders by name, not through avcodec_find_decoder: its table names every codec in the library (18 MB of
// libavcodec.a), and the partial link would take them all. Each is an FFCodec, whose first member is its AVCodec.
extern const AVCodec ff_hevc_decoder, ff_h264_decoder, ff_wmv3_decoder;

static const AVCodec *find_codec(int codec) {
  switch (codec) {
    case NXV_HEVC: return &ff_hevc_decoder;
    case NXV_H264: return &ff_h264_decoder;
    case NXV_WMV3: return &ff_wmv3_decoder;
  }
  return NULL;
}

// FFmpeg's own lookups, over these three decoders only. libavcodec's general code (options.o, avcodec.o, utils.o)
// calls them, and their definitions in allcodecs.o, parsers.o and bitstream_filters.o hold the tables of every codec,
// parser and filter in the library (497 decoders: 10.8 MB of code and 21 MB of tables in the object). Defined here, the
// partial link takes none of those three files. No parser and no bitstream filter is used (the decoders take whole
// frames; HEVC, H.264 and WMV3 name no bitstream filter of their own).
#include <libavcodec/bsf.h>
const AVCodec *av_codec_iterate(void **opaque) {
  static const AVCodec *const list[] = {&ff_hevc_decoder, &ff_h264_decoder, &ff_wmv3_decoder};
  uintptr_t i = (uintptr_t)*opaque;
  if (i >= sizeof(list) / sizeof(list[0])) return NULL;
  *opaque = (void *)(i + 1);
  return list[i];
}
const AVCodec *avcodec_find_decoder(enum AVCodecID id) {
  void *it = NULL;
  for (const AVCodec *c; (c = av_codec_iterate(&it));)
    if (c->id == id) return c;
  return NULL;
}
const AVCodec *avcodec_find_decoder_by_name(const char *name) {
  void *it = NULL;
  for (const AVCodec *c; (c = av_codec_iterate(&it));)
    if (name && !strcmp(c->name, name)) return c;
  return NULL;
}
const AVCodec *avcodec_find_encoder(enum AVCodecID id) { return NULL; }
const AVCodec *avcodec_find_encoder_by_name(const char *name) { return NULL; }
const struct AVCodecParser *av_parser_iterate(void **opaque) { return NULL; }
const AVBitStreamFilter *av_bsf_get_by_name(const char *name) { return NULL; }
const AVBitStreamFilter *av_bsf_iterate(void **opaque) { return NULL; }
const AVClass *ff_bsf_child_class_iterate(void **opaque) { return NULL; }

static void close_codec(NxvDecoder *d) {
  if (d->avctx) avcodec_free_context(&d->avctx);
  if (d->device) av_buffer_unref(&d->device);
  d->hw = 0;
}

// The codec context, on NVDEC when `hw` and the device opens.
static int open_codec(NxvDecoder *d, int hw, char *why, int why_size) {
  d->avctx = avcodec_alloc_context3(d->codec);
  if (!d->avctx) return -1;
  d->avctx->width = d->avctx->coded_width = d->width;
  d->avctx->height = d->avctx->coded_height = d->height;
  d->avctx->flags |= AV_CODEC_FLAG_LOW_DELAY;  // a picture out for each packet in (the movies have no reordering)
  d->avctx->thread_count = 1;
  if (d->extradata_size > 0) {
    d->avctx->extradata = av_mallocz(d->extradata_size + AV_INPUT_BUFFER_PADDING_SIZE);
    if (!d->avctx->extradata) return -1;
    memcpy(d->avctx->extradata, d->extradata, d->extradata_size);
    d->avctx->extradata_size = d->extradata_size;
  }
  int device_err = 0;
  if (hw) {
    device_err = av_hwdevice_ctx_create(&d->device, AV_HWDEVICE_TYPE_NVTEGRA, NULL, NULL, 0);
    if (device_err >= 0) {
      d->avctx->hw_device_ctx = av_buffer_ref(d->device);
      d->avctx->get_format = pick_nvtegra;
      d->hw = 1;
    }
  }
  const int err = avcodec_open2(d->avctx, d->codec, NULL);
  if (why && why_size > 0)
    snprintf(why, (size_t)why_size, "%s %dx%d on %s%s (%s)", d->codec->name, d->width, d->height,
             d->hw ? "NVDEC (nvtegra)" : "the CPU",
             hw && !d->hw ? ", the NVDEC device did not open" : "", nxv_version());
  return err;
}

NxvDecoder *nxv_open(int codec, const uint8_t *extradata, int extradata_size, int width, int height, int hw, char *why,
                     int why_size) {
  const AVCodec *c = find_codec(codec);
  if (!c) {
    if (why && why_size > 0) snprintf(why, (size_t)why_size, "no decoder for codec %d (%s)", codec, nxv_version());
    return NULL;
  }
  NxvDecoder *d = calloc(1, sizeof(*d));
  if (!d) return NULL;
  d->codec = c;
  d->width = width;
  d->height = height;
  if (extradata_size > 0 && extradata) {
    d->extradata = malloc((size_t)extradata_size);
    if (d->extradata) {
      memcpy(d->extradata, extradata, (size_t)extradata_size);
      d->extradata_size = extradata_size;
    }
  }
  d->packet = av_packet_alloc();
  d->frame = av_frame_alloc();
  d->sw_frame = av_frame_alloc();
  if (!d->packet || !d->frame || !d->sw_frame || open_codec(d, hw, why, why_size) < 0) {
    // the hardware path did not open: software
    close_codec(d);
    if (!hw || !d->packet || !d->frame || !d->sw_frame || open_codec(d, 0, why, why_size) < 0) {
      nxv_close(d);
      return NULL;
    }
    d->fell_back = 1;
  }
  return d;
}

static int fill_picture(NxvDecoder *d, NxvPicture *out) {
  const AVFrame *f = d->frame;
  if (f->format == AV_PIX_FMT_NVTEGRA) {
    av_frame_unref(d->sw_frame);
    if (av_hwframe_transfer_data(d->sw_frame, d->frame, 0) < 0) return -1;
    f = d->sw_frame;
  }
  if (f->format != AV_PIX_FMT_YUV420P && f->format != AV_PIX_FMT_YUVJ420P && f->format != AV_PIX_FMT_NV12)
    return -1;
  out->nv12 = f->format == AV_PIX_FMT_NV12;
  for (int k = 0; k < 3; ++k) {
    out->data[k] = f->data[k];
    out->linesize[k] = f->linesize[k];
  }
  out->width = f->width;
  out->height = f->height;
  return 1;
}

int nxv_decode(NxvDecoder *d, const uint8_t *data, int size, NxvPicture *out) {
  if (!d || !d->avctx || !data || size <= 0) return -1;
  av_packet_unref(d->packet);
  if (av_new_packet(d->packet, size) < 0) return -1;
  memcpy(d->packet->data, data, (size_t)size);
  int err = avcodec_send_packet(d->avctx, d->packet);
  if (err < 0 && d->hw && !d->fell_back) {
    // NVDEC refused the stream: software from here on (the next keyframe shows again)
    close_codec(d);
    d->fell_back = 1;
    if (open_codec(d, 0, NULL, 0) < 0) return -1;
    err = avcodec_send_packet(d->avctx, d->packet);
  }
  if (err < 0) return -1;
  err = avcodec_receive_frame(d->avctx, d->frame);
  if (err < 0) return err == AVERROR(EAGAIN) ? 0 : -1;
  return fill_picture(d, out);
}

void nxv_flush(NxvDecoder *d) {
  if (d && d->avctx) avcodec_flush_buffers(d->avctx);
}

void nxv_close(NxvDecoder *d) {
  if (!d) return;
  close_codec(d);
  if (d->packet) av_packet_free(&d->packet);
  if (d->frame) av_frame_free(&d->frame);
  if (d->sw_frame) av_frame_free(&d->sw_frame);
  free(d->extradata);
  free(d);
}

int nxv_is_hw(const NxvDecoder *d) { return d && d->hw; }

const char *nxv_version(void) {
  static char text[64];
  if (!text[0])
    snprintf(text, sizeof(text), "FFmpeg %s, libavcodec %d.%d.%d", av_version_info(), LIBAVCODEC_VERSION_MAJOR,
             LIBAVCODEC_VERSION_MINOR, LIBAVCODEC_VERSION_MICRO);
  return text;
}

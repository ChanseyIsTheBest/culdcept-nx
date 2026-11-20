// culdcept-nx (2026-11-20, the user: "I think getting hardware h.265 decoding would br good in thr long run would br good
// for other ports as well and most sagings and most performant so lets do that"): a video decoder on the Switch's NVDEC
// through devkitPro's FFmpeg 7.1 (switch-ffmpeg, built with averne's nvtegra backend: the hardware decoding of
// Moonlight-Switch and SwitchWave), with FFmpeg's software decoder of the same library when the hardware path cannot
// open (Ryujinx emulates no HEVC NVDEC).
//
// The port's own FFmpeg is 4.4 (sdk/thirdparty/FFmpeg: the XMA audio, the WMV3 movies) and has the same symbol names, so
// this library is one relocatable object (tools/switch/nxvideo/build.sh): this shim, devkitPro's libavcodec and
// libavutil linked into it, and every symbol made local but the nxv_ functions below. Nothing of FFmpeg 7.1 is seen
// outside it. C only: no FFmpeg header is needed to use it.
#pragma once
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct NxvDecoder NxvDecoder;

enum NxvCodec { NXV_HEVC = 1, NXV_H264 = 2, NXV_WMV3 = 3 };

// A decoded picture: YUV 4:2:0, planar (data[0..2]: Y, U, V) or NV12 (nv12 != 0: data[0] Y, data[1] UV interleaved).
// Valid until the next call on the decoder.
typedef struct NxvPicture {
  const uint8_t *data[3];
  int linesize[3];
  int width, height;
  int nv12;
} NxvPicture;

// A decoder for `codec`; hw != 0 asks for NVDEC (and falls back to software when it does not open). `extradata`: the
// codec's setup bytes, or none (HEVC with its parameter sets in band). `why` gets a line of what was opened, or why not.
NxvDecoder *nxv_open(int codec, const uint8_t *extradata, int extradata_size, int width, int height, int hw, char *why,
                     int why_size);
// One packet (one frame's bytes, no reordering): 1 and *out filled when a picture is ready, 0 when none, < 0 on error.
int nxv_decode(NxvDecoder *d, const uint8_t *data, int size, NxvPicture *out);
void nxv_flush(NxvDecoder *d);
void nxv_close(NxvDecoder *d);
// 1 while the decoder decodes on NVDEC.
int nxv_is_hw(const NxvDecoder *d);
// "FFmpeg 7.1, libavcodec 61.19.100"
const char *nxv_version(void);

#ifdef __cplusplus
}
#endif

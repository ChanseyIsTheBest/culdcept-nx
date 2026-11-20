// culdcept-nx (2026-11-20, the user: "I'd like you to see how much AV1 software decoding costs on switch cpu it may be
// worth it"): a benchmark NRO. The same 20 s of a 720p60 movie (narration_09, re-encoded on the PC) decoded on the
// console as
//   AV1 by dav1d (devkitPro's portlib), low delay (one picture out per frame in, as the port's movie hook needs it), 1-3
//   threads, and with frame threading (more throughput, pictures come out later);
//   H.265 by FFmpeg 7.1 (devkitPro's switch-ffmpeg) in software, one thread;
//   H.265 on NVDEC through nvtegra (the port's nxvideo path), each picture copied back to memory.
// Every picture is also copied into a 1280x720 I420 buffer, as the hook copies it into the game's. The CPU clock is
// read, never set. Input: sdmc:/switch/culdcept-nx/av1bench_clip.ivf (AV1) and av1bench_clip.hevc (H.265, Annex B);
// results in av1bench_results.txt there and on screen. Built by tools/switch/av1bench/build.sh.
// 2026-11-20 (the user: "check which movies are 30 fps and test AV1 on those"): the clips are a table, each with its
// frame budget (the 30 fps ones: credit_us and CI_01, 33.3 ms); a clip whose files are missing is skipped. Each line
// also gives the 99th percentile and the frames over twice the budget.
#include <switch.h>

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <dav1d/dav1d.h>
#include <libavcodec/avcodec.h>
#include <libavutil/frame.h>
#include <libavutil/hwcontext.h>

#define DIR "sdmc:/switch/culdcept-nx/av1bench_"
static FILE *out;
static uint8_t *i420;

static void say(const char *fmt, ...) {
  char line[512];
  va_list a;
  va_start(a, fmt);
  vsnprintf(line, sizeof(line), fmt, a);
  va_end(a);
  printf("%s\n", line);
  consoleUpdate(NULL);
  if (out) { fprintf(out, "%s\n", line); fflush(out); }
}

static double now_ms(void) { return (double)armTicksToNs(armGetSystemTick()) / 1e6; }

static uint8_t *load(const char *path, size_t *size) {
  FILE *f = fopen(path, "rb");
  if (!f) return NULL;
  fseek(f, 0, SEEK_END);
  *size = (size_t)ftell(f);
  fseek(f, 0, SEEK_SET);
  uint8_t *b = malloc(*size);
  if (b && fread(b, 1, *size, f) != *size) { free(b); b = NULL; }
  fclose(f);
  return b;
}

typedef struct { const uint8_t *data; int size; } Packet;

#define MAX_FRAMES 8192
static double g_budget = 16.67;  // ms a frame: the clip's (Clip.budget)
typedef struct { int frames; double total, worst, over, over2; double ms[MAX_FRAMES]; } Stats;
static Stats g_stats;

static void note(Stats *s, double ms) {
  if (s->frames < MAX_FRAMES) s->ms[s->frames] = ms;
  s->frames++;
  s->total += ms;
  if (ms > s->worst) s->worst = ms;
  if (ms > g_budget) s->over++;
  if (ms > 2 * g_budget) s->over2++;
}

static int cmp_double(const void *a, const void *b) {
  const double x = *(const double *)a, y = *(const double *)b;
  return x < y ? -1 : x > y;
}

static void report(const char *name, Stats *s, double wall) {
  if (!s->frames) { say("%-34s no pictures", name); return; }
  const double avg = s->total / s->frames;
  const int n = s->frames < MAX_FRAMES ? s->frames : MAX_FRAMES;
  qsort(s->ms, (size_t)n, sizeof(double), cmp_double);
  const double p99 = s->ms[(int)(n * 0.99) < n ? (int)(n * 0.99) : n - 1];
  say("%-34s %4d pictures: %6.2f ms avg, %6.2f p99, %6.2f worst, %4.1f%% over %.1f ms, %d over %.0f ms; %5.1f fps; "
      "%3.0f%% of a core at %.0f fps",
      name, s->frames, avg, p99, s->worst, 100.0 * s->over / s->frames, g_budget, (int)s->over2, 2 * g_budget,
      s->frames / (wall / 1000.0), avg / g_budget * 100.0, 1000.0 / g_budget);
}

// I420 into the 1280x720 buffer, as the hook writes the game's.
static void copy_planes(uint8_t *const data[3], const ptrdiff_t stride[3], int w, int h, int nv12) {
  uint8_t *y = i420, *u = i420 + w * h, *v = u + (w / 2) * (h / 2);
  for (int r = 0; r < h; ++r) memcpy(y + r * w, data[0] + r * stride[0], w);
  if (nv12) {
    for (int r = 0; r < h / 2; ++r) {
      const uint8_t *uv = data[1] + r * stride[1];
      for (int x = 0; x < w / 2; ++x) { u[r * (w / 2) + x] = uv[2 * x]; v[r * (w / 2) + x] = uv[2 * x + 1]; }
    }
  } else {
    for (int r = 0; r < h / 2; ++r) {
      memcpy(u + r * (w / 2), data[1] + r * stride[1], w / 2);
      memcpy(v + r * (w / 2), data[2] + r * stride[2], w / 2);
    }
  }
}

// ---- AV1 (dav1d) -----------------------------------------------------------------------------------------------------
static int ivf_packets(const uint8_t *b, size_t n, Packet *p, int max) {
  if (n < 32 || memcmp(b, "DKIF", 4)) return 0;
  size_t i = b[6] | (b[7] << 8);
  int k = 0;
  while (i + 12 <= n && k < max) {
    const int size = b[i] | (b[i + 1] << 8) | (b[i + 2] << 16) | (b[i + 3] << 24);
    i += 12;
    if (i + (size_t)size > n) break;
    p[k].data = b + i;
    p[k].size = size;
    ++k;
    i += (size_t)size;
  }
  return k;
}

static void av1_take(Dav1dPicture *pic, Stats *s, double t0) {
  uint8_t *d[3] = {pic->data[0], pic->data[1], pic->data[2]};
  ptrdiff_t st[3] = {pic->stride[0], pic->stride[1], pic->stride[1]};
  copy_planes(d, st, pic->p.w, pic->p.h, 0);
  note(s, now_ms() - t0);
  dav1d_picture_unref(pic);
}

static void bench_av1(const Packet *p, int n, int threads, int delay) {
  Dav1dSettings set;
  dav1d_default_settings(&set);
  set.n_threads = threads;
  set.max_frame_delay = delay;
  Dav1dContext *c = NULL;
  if (dav1d_open(&c, &set) < 0) { say("dav1d did not open"); return; }
  Stats *s = &g_stats;
  memset(s, 0, sizeof(*s));
  const double wall0 = now_ms();
  for (int k = 0; k < n; ++k) {
    Dav1dData data = {0};
    memcpy(dav1d_data_create(&data, (size_t)p[k].size), p[k].data, (size_t)p[k].size);
    const double t0 = now_ms();
    for (;;) {
      const int r = dav1d_send_data(c, &data);
      Dav1dPicture pic = {0};
      if (dav1d_get_picture(c, &pic) == 0) av1_take(&pic, s, t0);
      if (r != DAV1D_ERR(EAGAIN)) break;
    }
  }
  for (;;) {  // the pictures still inside (frame threading)
    Dav1dPicture pic = {0};
    const double t0 = now_ms();
    const int r = dav1d_get_picture(c, &pic);
    if (r == 0) av1_take(&pic, s, t0); else if (r != DAV1D_ERR(EAGAIN) || s->frames >= n) break;
  }
  const double wall = now_ms() - wall0;
  char name[64];
  snprintf(name, sizeof(name), "AV1 dav1d %d thread%s%s", threads, threads > 1 ? "s" : "", delay > 1 ? ", frame-threaded" : "");
  report(name, s, wall);
  dav1d_close(&c);
}

// ---- H.265 (FFmpeg 7.1: software, NVDEC) ---------------------------------------------------------------------------
static int annexb_packets(const uint8_t *b, size_t n, Packet *p, int max) {
  // one packet per access unit, split at each AUD or first slice of a picture (FFmpeg's parser does the same work)
  AVCodecParserContext *pc = av_parser_init(AV_CODEC_ID_HEVC);
  AVCodecContext *cx = avcodec_alloc_context3(NULL);
  int k = 0;
  size_t i = 0;
  while (i < n && k < max) {
    uint8_t *data = NULL;
    int size = 0;
    const int used = av_parser_parse2(pc, cx, &data, &size, b + i, (int)(n - i), AV_NOPTS_VALUE, AV_NOPTS_VALUE, 0);
    i += (size_t)used;
    if (size) { p[k].data = data; p[k].size = size; ++k; }
    if (!used && !size) break;
  }
  uint8_t *data = NULL;
  int size = 0;
  av_parser_parse2(pc, cx, &data, &size, NULL, 0, AV_NOPTS_VALUE, AV_NOPTS_VALUE, 0);
  if (size && k < max) { p[k].data = data; p[k].size = size; ++k; }
  // the parser's buffers live until it is closed: copy them
  for (int j = 0; j < k; ++j) { uint8_t *c = malloc((size_t)p[j].size); memcpy(c, p[j].data, (size_t)p[j].size); p[j].data = c; }
  av_parser_close(pc);
  avcodec_free_context(&cx);
  return k;
}

static enum AVPixelFormat pick_hw(AVCodecContext *c, const enum AVPixelFormat *f) {
  for (; *f != AV_PIX_FMT_NONE; ++f) if (*f == AV_PIX_FMT_NVTEGRA) return *f;
  return AV_PIX_FMT_NONE;
}

static void bench_hevc(const Packet *p, int n, int hw) {
  const AVCodec *codec = avcodec_find_decoder(AV_CODEC_ID_HEVC);
  AVCodecContext *c = avcodec_alloc_context3(codec);
  c->flags |= AV_CODEC_FLAG_LOW_DELAY;
  c->thread_count = 1;
  AVBufferRef *dev = NULL;
  if (hw) {
    const int e = av_hwdevice_ctx_create(&dev, AV_HWDEVICE_TYPE_NVTEGRA, NULL, NULL, 0);
    if (e < 0) { say("H.265 NVDEC: the nvtegra device did not open (%d)", e); avcodec_free_context(&c); return; }
    c->hw_device_ctx = av_buffer_ref(dev);
    c->get_format = pick_hw;
  }
  if (avcodec_open2(c, codec, NULL) < 0) { say("H.265 %s: the decoder did not open", hw ? "NVDEC" : "software"); return; }
  AVPacket *pkt = av_packet_alloc();
  AVFrame *f = av_frame_alloc(), *sw = av_frame_alloc();
  Stats *s = &g_stats;
  memset(s, 0, sizeof(*s));
  int hw_frames = 0;
  const double wall0 = now_ms();
  for (int k = 0; k < n; ++k) {
    av_new_packet(pkt, p[k].size);
    memcpy(pkt->data, p[k].data, (size_t)p[k].size);
    const double t0 = now_ms();
    if (avcodec_send_packet(c, pkt) >= 0) {
      while (avcodec_receive_frame(c, f) >= 0) {
        AVFrame *pic = f;
        if (f->format == AV_PIX_FMT_NVTEGRA) {
          ++hw_frames;
          av_frame_unref(sw);
          if (av_hwframe_transfer_data(sw, f, 0) < 0) continue;
          pic = sw;
        }
        uint8_t *d[3] = {pic->data[0], pic->data[1], pic->data[2]};
        ptrdiff_t st[3] = {pic->linesize[0], pic->linesize[1], pic->linesize[2]};
        copy_planes(d, st, pic->width, pic->height, pic->format == AV_PIX_FMT_NV12);
        note(s, now_ms() - t0);
      }
    }
    av_packet_unref(pkt);
  }
  const double wall = now_ms() - wall0;
  report(hw ? (hw_frames ? "H.265 NVDEC (nvtegra) + copy" : "H.265 NVDEC asked, software ran") : "H.265 FFmpeg software, 1 thread",
         s, wall);
  av_frame_free(&f);
  av_frame_free(&sw);
  av_packet_free(&pkt);
  avcodec_free_context(&c);
  if (dev) av_buffer_unref(&dev);
}

int main(int argc, char **argv) {
  consoleInit(NULL);
  out = fopen(DIR "results.txt", "w");
  i420 = malloc(1280 * 720 * 3 / 2);
  say("culdcept-nx AV1 / H.265 decode benchmark");
  u32 hz = 0;
  if (R_SUCCEEDED(clkrstInitialize())) {
    ClkrstSession sess;
    if (R_SUCCEEDED(clkrstOpenSession(&sess, PcvModuleId_CpuBus, 3))) { clkrstGetClockRate(&sess, &hz); clkrstCloseSession(&sess); }
    clkrstExit();
  }
  say("CPU clock: %s%u MHz (read, not set); dav1d %s; %s", hz ? "" : "unknown ", hz / 1000000, dav1d_version(),
      av_version_info());
  // the clips: AV1 (IVF) and H.265 (Annex B) of the same picture, and the frame budget
  static const struct { const char *name, *ivf, *hevc; double budget; } clips[] = {
      {"credit_us (720p30, 3:36), AV1 by SVT-AV1", DIR "credit.ivf", DIR "credit.hevc", 1000.0 / 30},
      {"credit_us, AV1 by libaom real-time (as a browser)", DIR "credit_rt.ivf", NULL, 1000.0 / 30},
      {"CI_01 (720p30, 6.7 s)", DIR "ci01.ivf", DIR "ci01.hevc", 1000.0 / 30},
  };
  static Packet av1[MAX_FRAMES], hevc[MAX_FRAMES];
  for (size_t c = 0; c < sizeof(clips) / sizeof(clips[0]); ++c) {
    size_t n1 = 0, n2 = 0;
    uint8_t *b1 = clips[c].ivf ? load(clips[c].ivf, &n1) : NULL, *b2 = clips[c].hevc ? load(clips[c].hevc, &n2) : NULL;
    if (!b1 && !b2) continue;
    const int k1 = b1 ? ivf_packets(b1, n1, av1, MAX_FRAMES) : 0;
    const int k2 = b2 ? annexb_packets(b2, n2, hevc, MAX_FRAMES) : 0;
    g_budget = clips[c].budget;
    say("");
    say("%s: AV1 %d frames (%.2f MB), H.265 %d frames (%.2f MB); budget %.1f ms a frame", clips[c].name, k1,
        n1 / 1048576.0, k2, n2 / 1048576.0, g_budget);
    if (k1) {
      bench_av1(av1, k1, 1, 1);
      bench_av1(av1, k1, 2, 1);
      bench_av1(av1, k1, 3, 3);
    }
    if (k2) {
      bench_hevc(hevc, k2, 0);
      bench_hevc(hevc, k2, 1);
      for (int j = 0; j < k2; ++j) free((void *)hevc[j].data);
    }
    free(b1);
    free(b2);
  }
  say("done: results in " DIR "results.txt; closing in 5 s");
  if (out) fclose(out);
  const u64 end = armGetSystemTick() + armNsToTicks(5000000000ULL);
  while (appletMainLoop() && armGetSystemTick() < end) { consoleUpdate(NULL); svcSleepThread(100000000); }
  consoleExit(NULL);
  return 0;
}

/**
 * culdcept-nx (2026-11-20): NAME.xmo, an XMA file whose frames are Opus (rex/filesystem/xmo.h has the format). The
 * handle below synthesizes NAME's bytes on each read, a packet at a time: its RIFF header, packet headers and trailer as
 * they were, each frame's 15-bit length field and last bit, and the packet's tag in its tag bits (everything else
 * zero), so the game and the XMA emulation walk the same packets and frames as before. Deterministic (the read caches
 * and parked handles keep what it gives).
 */
#include <rex/filesystem/xmo.h>

#include <algorithm>
#include <cstring>
#include <list>
#include <map>
#include <mutex>

#include <rex/filesystem.h>
#include <rex/logging.h>
#include <rex/string.h>

namespace rex::filesystem {
namespace {

constexpr char kMagic[8] = {'R', 'X', 'X', 'M', 'A', 'O', 'P', '1'};
constexpr size_t kHeaderSize = 64;
constexpr uint32_t kPacketBits = kXmoPacketBytes * 8;
constexpr uint32_t kPacketHeaderBits = 32;
constexpr uint32_t kFrameHeaderBits = 15;
constexpr uint32_t kMaxFrameLength = 0x7FFF;
// The Opus data of this many bytes stays read at most (sounds not in use are read again when they play): a piece of
// music is 2-5 MB at 128 kbit/s, an effect a few KB.
constexpr size_t kLoadedLimit = 16u << 20;

uint16_t Le16(const uint8_t* p) { return uint16_t(p[0] | (p[1] << 8)); }
uint32_t Le32(const uint8_t* p) { return uint32_t(p[0]) | (uint32_t(p[1]) << 8) | (uint32_t(p[2]) << 16) | (uint32_t(p[3]) << 24); }
uint64_t Le64(const uint8_t* p) { return uint64_t(Le32(p)) | (uint64_t(Le32(p + 4)) << 32); }

bool ReadExactly(FileHandle& f, uint64_t offset, void* to, size_t length) {
  size_t got = 0;
  return f.Read(size_t(offset), to, length, &got) && got == length;
}

struct Header {
  uint32_t id = 0;
  uint64_t size = 0;
  uint32_t data_offset = 0, packets = 0, frames = 0, rate = 0;
  uint16_t channels = 0, pre_skip = 0;
  uint32_t opus_count = 0;
  uint64_t opus_table = 0;
  uint32_t trailer = 0;
  uint64_t packets_end() const { return uint64_t(data_offset) + uint64_t(packets) * kXmoPacketBytes; }
  uint64_t frame_table() const { return kHeaderSize + data_offset + uint64_t(packets) * 4; }
};

bool ParseHeader(const uint8_t* h, Header* out) {
  if (std::memcmp(h, kMagic, 8) != 0 || Le32(h + 8) != 1) return false;
  out->id = Le32(h + 12);
  out->size = Le64(h + 16);
  out->data_offset = Le32(h + 24);
  out->packets = Le32(h + 28);
  out->frames = Le32(h + 32);
  out->rate = Le32(h + 36);
  out->channels = Le16(h + 40);
  out->pre_skip = Le16(h + 42);
  out->opus_count = Le32(h + 44);
  out->opus_table = Le64(h + 48);
  out->trailer = Le32(h + 56);
  return out->data_offset <= 4096 && out->channels >= 1 && out->channels <= 2 && out->rate == 48000 &&
         out->trailer <= (1u << 20) && out->packets_end() + out->trailer == out->size &&
         out->packets < (1u << 24);  // the tag's packet index is 24 bits
}

bool ReadHeader(FileHandle& f, Header* out) {
  uint8_t h[kHeaderSize];
  return ReadExactly(f, 0, h, sizeof(h)) && ParseHeader(h, out);
}

struct Frame {
  uint32_t pos;  // bit position in the data
  uint16_t len;  // bits
  uint8_t last;  // the frame's last bit
};

inline uint32_t Bit(const uint8_t* p, uint32_t b) { return (p[b >> 3] >> (7 - (b & 7))) & 1; }
// Bit k of a tag (72 bits, more than a uint64_t: magic, sound id, packet index).
inline uint32_t TagBit(uint32_t k, uint32_t id, uint32_t packet_index) {
  return k < 16 ? (kXmoTagMagic >> (15 - k)) & 1 : k < 48 ? (id >> (47 - k)) & 1 : (packet_index >> (71 - k)) & 1;
}
inline void SetBit(uint8_t* p, uint32_t b) { p[b >> 3] |= uint8_t(0x80 >> (b & 7)); }

// --- the registry: sound id -> its .xmo, the Opus data read when the game reads the file's data ---
struct Registered {
  std::filesystem::path path;
  Header header;
  std::shared_ptr<XmoSound> sound;  // null until read (and again after it is let go)
};
std::mutex g_mutex;
std::map<uint32_t, Registered> g_sounds;
std::list<uint32_t> g_loaded;  // read sounds, the most recently used first
size_t g_loaded_bytes = 0;

void Register(const std::filesystem::path& path, const Header& h) {
  std::lock_guard lock(g_mutex);
  auto [it, inserted] = g_sounds.try_emplace(h.id);
  if (!inserted && it->second.path != path) {
    REXFS_WARN("[xmo] sound id {:08X} is in both {} and {}: the second is used", h.id,
               rex::path_to_utf8(it->second.path), rex::path_to_utf8(path));
    it->second.sound.reset();
  }
  it->second.path = path;
  it->second.header = h;
}

std::shared_ptr<XmoSound> Load(const std::filesystem::path& path, const Header& h) {
  auto f = FileHandle::OpenExisting(path, FileAccess::kFileReadData);
  if (!f) return nullptr;
  std::vector<uint8_t> table(size_t(h.frames) * 8), sizes(size_t(h.opus_count) * 2);
  if (!ReadExactly(*f, h.frame_table(), table.data(), table.size()) ||
      !ReadExactly(*f, h.opus_table, sizes.data(), sizes.size())) {
    return nullptr;
  }
  auto s = std::make_shared<XmoSound>();
  s->id = h.id;
  s->sample_rate = h.rate;
  s->channels = h.channels;
  s->pre_skip = h.pre_skip;
  s->frame_pos.resize(h.frames);
  for (uint32_t i = 0; i < h.frames; ++i) s->frame_pos[i] = Le32(table.data() + size_t(i) * 8);
  s->opus_offset.resize(size_t(h.opus_count) + 1);
  uint32_t total = 0;
  for (uint32_t i = 0; i < h.opus_count; ++i) {
    s->opus_offset[i] = total;
    total += Le16(sizes.data() + size_t(i) * 2);
  }
  s->opus_offset[h.opus_count] = total;
  s->opus.resize(total);
  if (total && !ReadExactly(*f, h.opus_table + sizes.size(), s->opus.data(), total)) return nullptr;
  return s;
}

// --- the synthesized XMA file ---
class XmoHandle : public FileHandle {
 public:
  XmoHandle(std::unique_ptr<FileHandle> packed, Header h, std::vector<uint8_t> riff, std::vector<uint8_t> packet_headers,
            std::vector<Frame> frames, std::vector<uint8_t> trailer)
      : FileHandle(packed->path()),
        packed_(std::move(packed)),
        h_(h),
        riff_(std::move(riff)),
        packet_headers_(std::move(packet_headers)),
        frames_(std::move(frames)),
        trailer_(std::move(trailer)) {}

  bool Read(size_t file_offset, void* buffer, size_t buffer_length, size_t* out_bytes_read) override {
    *out_bytes_read = 0;
    if (file_offset >= h_.size) return true;
    const size_t n = size_t(std::min<uint64_t>(buffer_length, h_.size - file_offset));
    uint8_t* out = static_cast<uint8_t*>(buffer);
    const uint64_t lo = file_offset, hi = file_offset + n;
    // NAME's RIFF header
    if (lo < h_.data_offset) std::memcpy(out, riff_.data() + lo, size_t(std::min<uint64_t>(hi, h_.data_offset) - lo));
    // NAME's trailer (its seek chunk)
    const uint64_t t = h_.packets_end();
    if (hi > t) {
      const uint64_t from = std::max(lo, t);
      std::memcpy(out + (from - lo), trailer_.data() + (from - t), size_t(hi - from));
    }
    // the packets, each synthesized whole and the part in the range copied
    if (hi > h_.data_offset && lo < t) {
      // The game reads the data before it plays it: the Opus is read now, on its thread, not by the first frame's
      // decode on the audio thread.
      FindXmoSound(h_.id);
      const uint64_t d_lo = lo > h_.data_offset ? lo - h_.data_offset : 0, d_hi = std::min(hi, t) - h_.data_offset;
      uint8_t packet[kXmoPacketBytes];
      for (uint64_t p = d_lo / kXmoPacketBytes; p * kXmoPacketBytes < d_hi; ++p) {
        Synthesize(uint32_t(p), packet);
        const uint64_t a = std::max(d_lo, p * kXmoPacketBytes), b = std::min(d_hi, (p + 1) * kXmoPacketBytes);
        std::memcpy(out + (h_.data_offset + a - lo), packet + (a - p * kXmoPacketBytes), size_t(b - a));
      }
    }
    *out_bytes_read = n;
    return true;
  }
  bool Write(size_t, const void*, size_t, size_t*) override { return false; }
  bool SetLength(size_t) override { return false; }
  void Flush() override {}

 private:
  // Packet p of NAME's data: its header, the length fields and last bits of the frames in it (a frame is at most 32767
  // bits: one that starts two packets before can reach into it), then its tag if a frame starts in it.
  void Synthesize(uint32_t p, uint8_t* packet) const {
    std::memset(packet, 0, kXmoPacketBytes);
    std::memcpy(packet, packet_headers_.data() + size_t(p) * 4, 4);
    const uint64_t from = p >= 2 ? uint64_t(p - 2) * kPacketBits : 0, to = uint64_t(p + 1) * kPacketBits;
    auto it = std::lower_bound(frames_.begin(), frames_.end(), from,
                               [](const Frame& f, uint64_t v) { return f.pos < v; });
    bool starts = false;  // a frame of the table starts in packet p
    for (; it != frames_.end() && it->pos < to; ++it) {
      Put(packet, p, *it, 0, kFrameHeaderBits, it->len);
      Put(packet, p, *it, uint32_t(it->len) - 1, 1, it->last);
      starts = starts || it->pos >= uint64_t(p) * kPacketBits;
    }
    // culdcept-nx 1.0.1 (2026-11-21, found by the sasrt-nx session; its PacketMaker::Synthesize has the same rule): only
    // a packet a frame starts in carries a tag (xmo.h). A sound's last packet can say a frame starts in it (skip count
    // not 0xFF) with its first frame offset past its end: XmoTagBits then gives bits 47 on, the end of the frame that
    // runs into it, and the tag set that frame's last bit ("another frame follows") when it ends before bit 119. Of the
    // 619 .xmo files, 12 had such a packet and se_024's last frame was changed (an extra frame read from the tag).
    if (!starts) return;
    uint16_t bits[kXmoTagBits];
    if (XmoTagBits(packet, bits, kXmoTagBits) < kXmoTagBits) return;  // no frame starts here (the installer checked)
    for (uint32_t k = 0; k < kXmoTagBits; ++k) {
      if (TagBit(k, h_.id, p)) SetBit(packet, bits[k]);
    }
  }
  // Bit `j` of frame `f` as packet and bit: a frame goes on past its packet's end in the packet its header's skip count
  // names (the next one, for a single stream), after that packet's 32-bit header.
  void FrameBit(const Frame& f, uint32_t j, uint64_t* packet, uint32_t* in) const {
    uint64_t pk = f.pos / kPacketBits, b = f.pos % kPacketBits + j;
    while (b >= kPacketBits) {
      b -= kPacketBits;
      const uint8_t skip = pk < h_.packets ? packet_headers_[size_t(pk * 4 + 3)] : 0;
      pk += uint64_t(skip == 0xFF ? 0 : skip) + 1;
      b += kPacketHeaderBits;
    }
    *packet = pk;
    *in = uint32_t(b);
  }
  // `count` bits of `value` (MSB first) at frame bits [j, j + count), those that fall in packet p.
  void Put(uint8_t* packet, uint32_t p, const Frame& f, uint32_t j, uint32_t count, uint32_t value) const {
    for (uint32_t k = 0; k < count; ++k) {
      if (!((value >> (count - 1 - k)) & 1)) continue;
      uint64_t pk;
      uint32_t b;
      FrameBit(f, j + k, &pk, &b);
      if (pk == p) SetBit(packet, b);
    }
  }

  std::unique_ptr<FileHandle> packed_;
  Header h_;
  std::vector<uint8_t> riff_, packet_headers_;
  std::vector<Frame> frames_;
  std::vector<uint8_t> trailer_;
};

}  // namespace

bool HasXmoSuffix(std::string_view name) {
  if (name.size() <= kXmoSuffix.size()) return false;
  const std::string_view tail = name.substr(name.size() - kXmoSuffix.size());
  for (size_t i = 0; i < tail.size(); ++i) {
    char c = tail[i];
    if (c >= 'A' && c <= 'Z') c = char(c - 'A' + 'a');
    if (c != kXmoSuffix[i]) return false;
  }
  return true;
}

bool ReadXmoSize(const std::filesystem::path& path, uint64_t* out_size) {
  auto f = FileHandle::OpenExisting(path, FileAccess::kFileReadData);
  Header h;
  if (!f || !ReadHeader(*f, &h)) return false;
  *out_size = h.size;
  return true;
}

std::unique_ptr<FileHandle> OpenXmo(std::unique_ptr<FileHandle> packed) {
  Header h;
  if (!packed || !ReadHeader(*packed, &h)) {
    REXFS_WARN("[xmo] {}: not a valid .xmo file", packed ? rex::path_to_utf8(packed->path()) : std::string("?"));
    return nullptr;
  }
  std::vector<uint8_t> riff(h.data_offset), packets(size_t(h.packets) * 4), table(size_t(h.frames) * 8),
      trailer(h.trailer);
  uint64_t at = kHeaderSize;
  if (!ReadExactly(*packed, at, riff.data(), riff.size()) ||
      !ReadExactly(*packed, at += riff.size(), packets.data(), packets.size()) ||
      !ReadExactly(*packed, at += packets.size(), table.data(), table.size()) ||
      (h.trailer && !ReadExactly(*packed, at += table.size(), trailer.data(), trailer.size()))) {
    REXFS_WARN("[xmo] {}: truncated", rex::path_to_utf8(packed->path()));
    return nullptr;
  }
  std::vector<Frame> frames(h.frames);
  for (uint32_t i = 0; i < h.frames; ++i) {
    const uint8_t* e = table.data() + size_t(i) * 8;
    frames[i] = {Le32(e), Le16(e + 4), e[6]};
    if (frames[i].len <= kFrameHeaderBits || frames[i].pos / kPacketBits >= h.packets ||
        (i && frames[i].pos <= frames[i - 1].pos)) {
      REXFS_WARN("[xmo] {}: frame {} is not valid", rex::path_to_utf8(packed->path()), i);
      return nullptr;
    }
  }
  Register(packed->path(), h);
  return std::make_unique<XmoHandle>(std::move(packed), h, std::move(riff), std::move(packets), std::move(frames),
                                     std::move(trailer));
}

bool XmoSoundKnown(uint32_t id) {
  std::lock_guard lock(g_mutex);
  return g_sounds.count(id) != 0;
}

std::shared_ptr<const XmoSound> FindXmoSound(uint32_t id) {
  std::filesystem::path path;
  Header h;
  {
    std::lock_guard lock(g_mutex);
    auto it = g_sounds.find(id);
    if (it == g_sounds.end()) return nullptr;
    if (it->second.sound) {
      if (g_loaded.front() != id) {
        g_loaded.remove(id);
        g_loaded.push_front(id);
      }
      return it->second.sound;
    }
    path = it->second.path;
    h = it->second.header;
  }
  auto s = Load(path, h);  // outside the lock: a read from the SD
  if (!s) {
    REXFS_WARN("[xmo] {}: its Opus data could not be read", rex::path_to_utf8(path));
    return nullptr;
  }
  std::lock_guard lock(g_mutex);
  auto it = g_sounds.find(id);
  if (it == g_sounds.end()) return s;
  if (!it->second.sound) {
    it->second.sound = s;
    g_loaded.push_front(id);
    g_loaded_bytes += s->opus.size();
    // let go of the least recently used (a decoder still holding one keeps it alive)
    while (g_loaded_bytes > kLoadedLimit && g_loaded.size() > 1) {
      const uint32_t old = g_loaded.back();
      g_loaded.pop_back();
      auto o = g_sounds.find(old);
      if (o != g_sounds.end() && o->second.sound) {
        g_loaded_bytes -= o->second.sound->opus.size();
        o->second.sound.reset();
      }
    }
  }
  return it->second.sound;
}

int64_t FindXmoFrame(const XmoSound& sound, uint64_t bit_pos) {
  auto it = std::lower_bound(sound.frame_pos.begin(), sound.frame_pos.end(), bit_pos);
  return it != sound.frame_pos.end() && *it == bit_pos ? int64_t(it - sound.frame_pos.begin()) : -1;
}

uint32_t XmoTagBits(const uint8_t* packet, uint16_t* bits, uint32_t max) {
  if (packet[3] == 0xFF) return 0;  // no frame starts in it
  const uint32_t first =
      ((uint32_t(packet[0] & 3) << 13) | (uint32_t(packet[1]) << 5) | (uint32_t(packet[2]) >> 3)) + kPacketHeaderBits;
  uint32_t n = 0;
  // the frames that start here, as XmaContext::GetPacketInfo walks them
  for (uint32_t off = first; off < kPacketBits && n < max;) {
    if (kPacketBits - off < kFrameHeaderBits) break;  // a header split across packets
    uint32_t len = 0;
    for (uint32_t k = 0; k < kFrameHeaderBits; ++k) len = (len << 1) | Bit(packet, off + k);
    if (len == 0 || len == kMaxFrameLength) break;
    const uint32_t end = std::min(off + len - 1, kPacketBits);  // the last bit is not a tag bit
    for (uint32_t b = off + kFrameHeaderBits; b < end && n < max; ++b) bits[n++] = uint16_t(b);
    if (off + len > kPacketBits) break;  // it goes on in the next packet
    const uint32_t last = Bit(packet, off + len - 1);
    off += len;
    if (!last) break;
  }
  // then the end of a frame that began in an earlier packet
  for (uint32_t b = kPacketHeaderBits + kFrameHeaderBits; b + 1 < std::min(first, kPacketBits) && n < max; ++b)
    bits[n++] = uint16_t(b);
  return n;
}

bool ReadXmoPacketTag(const uint8_t* packet, uint32_t* id, uint32_t* packet_index) {
  uint16_t bits[kXmoTagBits];
  if (XmoTagBits(packet, bits, kXmoTagBits) < kXmoTagBits) return false;
  uint32_t magic = 0, sound = 0, index = 0;
  for (uint32_t k = 0; k < 16; ++k) magic = (magic << 1) | Bit(packet, bits[k]);
  if (magic != kXmoTagMagic) return false;
  for (uint32_t k = 16; k < 48; ++k) sound = (sound << 1) | Bit(packet, bits[k]);
  for (uint32_t k = 48; k < kXmoTagBits; ++k) index = (index << 1) | Bit(packet, bits[k]);
  *id = sound;
  *packet_index = index;
  return true;
}

}  // namespace rex::filesystem

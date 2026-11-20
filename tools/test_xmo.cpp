// culdcept-nx (2026-11-20): host test of the .xmo reader (sdk/src/filesystem/xmo.cpp) against the XMA file it stands
// for. The synthesized file is read whole and in random pieces; against the original: the same size, RIFF header and
// trailer, and each packet byte for byte as it should be (the header, the table's frames' length fields and last bits,
// which are checked against the original's: a wrong position in the installer's frame walk reads another length, and
// the packet's tag); every frame is found from its packet's tag and position as the emulation finds it; the sound's
// Opus data loads. The SDK's logging and file handles are tools/test_stubs (stdio):
//   PATH=/c/devkitPro/msys2/clang64/bin:$PATH clang++ -O2 -std=c++20 -Itools/test_stubs -Isdk/include \
//     tools/test_xmo.cpp sdk/src/filesystem/xmo.cpp -o out/test_xmo.exe
//   out/test_xmo.exe <NAME.xmo> <NAME> [random reads]
#include <cstdio>
#include <cstring>
#include <random>
#include <vector>

#include <rex/filesystem.h>
#include <rex/filesystem/xmo.h>

namespace rex::filesystem {
namespace {
class StdioHandle final : public FileHandle {
 public:
  StdioHandle(const std::filesystem::path& path, std::FILE* f) : FileHandle(path), f_(f) {}
  ~StdioHandle() override { std::fclose(f_); }
  bool Read(size_t offset, void* buffer, size_t length, size_t* out) override {
    if (_fseeki64(f_, int64_t(offset), SEEK_SET) != 0) return false;
    *out = std::fread(buffer, 1, length, f_);
    return true;
  }
  bool Write(size_t, const void*, size_t, size_t*) override { return false; }
  bool SetLength(size_t) override { return false; }
  void Flush() override {}

 private:
  std::FILE* f_;
};
}  // namespace

std::unique_ptr<FileHandle> FileHandle::OpenExisting(const std::filesystem::path& path, uint32_t, bool) {
  std::FILE* f = _wfopen(path.wstring().c_str(), L"rb");
  return f ? std::make_unique<StdioHandle>(path, f) : nullptr;
}
}  // namespace rex::filesystem

using namespace rex::filesystem;

static std::vector<uint8_t> ReadAll(const char* path) {
  std::vector<uint8_t> v;
  if (std::FILE* f = std::fopen(path, "rb")) {
    std::fseek(f, 0, SEEK_END);
    v.resize(size_t(std::ftell(f)));
    std::fseek(f, 0, SEEK_SET);
    if (std::fread(v.data(), 1, v.size(), f) != v.size()) v.clear();
    std::fclose(f);
  }
  return v;
}
static uint32_t Le32(const uint8_t* p) { return p[0] | (p[1] << 8) | (p[2] << 16) | (uint32_t(p[3]) << 24); }
static uint16_t Le16(const uint8_t* p) { return uint16_t(p[0] | (p[1] << 8)); }

int main(int argc, char** argv) {
  if (argc < 3) {
    std::fprintf(stderr, "usage: test_xmo <NAME.xmo> <NAME> [random reads]\n");
    return 2;
  }
  const int reads = argc > 3 ? std::atoi(argv[3]) : 2000;
  const std::vector<uint8_t> xmo = ReadAll(argv[1]), orig = ReadAll(argv[2]);
  if (xmo.size() < 64 || orig.empty()) {
    std::fprintf(stderr, "FAIL: read the files\n");
    return 1;
  }
  uint64_t size = 0;
  if (!ReadXmoSize(argv[1], &size) || size != orig.size()) {
    std::fprintf(stderr, "FAIL: size %llu for %zu\n", (unsigned long long)size, orig.size());
    return 1;
  }
  auto h = OpenXmo(FileHandle::OpenExisting(argv[1], FileAccess::kFileReadData));
  if (!h) {
    std::fprintf(stderr, "FAIL: OpenXmo\n");
    return 1;
  }
  // the whole synthesized file, in 64 KB reads
  std::vector<uint8_t> syn(orig.size());
  for (size_t at = 0; at < syn.size(); at += 65536) {
    size_t got = 0;
    const size_t n = std::min<size_t>(65536, syn.size() - at);
    if (!h->Read(at, syn.data() + at, n, &got) || got != n) {
      std::fprintf(stderr, "FAIL: read at %zu\n", at);
      return 1;
    }
  }
  // random reads must give the same bytes
  std::mt19937_64 rng(1234);
  std::vector<uint8_t> piece;
  for (int i = 0; i < reads; ++i) {
    const size_t at = size_t(rng() % syn.size()), n = size_t(1 + rng() % 20000);
    piece.assign(n, 0xCD);
    size_t got = 0;
    if (!h->Read(at, piece.data(), n, &got) || got != std::min(n, syn.size() - at) ||
        std::memcmp(piece.data(), syn.data() + at, got) != 0) {
      std::fprintf(stderr, "FAIL: random read %d at %zu (%zu bytes)\n", i, at, n);
      return 1;
    }
  }

  const uint8_t* hd = xmo.data();
  const uint32_t id = Le32(hd + 12), data_offset = Le32(hd + 24), packets = Le32(hd + 28), frames = Le32(hd + 32);
  const uint32_t trailer = Le32(hd + 56);
  const uint64_t packets_end = uint64_t(data_offset) + uint64_t(packets) * 2048;
  if (std::memcmp(syn.data(), orig.data(), data_offset) != 0) {
    std::fprintf(stderr, "FAIL: RIFF header\n");
    return 1;
  }
  for (uint32_t p = 0; p < packets; ++p) {
    if (std::memcmp(syn.data() + data_offset + p * 2048, orig.data() + data_offset + p * 2048, 4) != 0) {
      std::fprintf(stderr, "FAIL: packet header %u\n", p);
      return 1;
    }
  }
  if (std::memcmp(syn.data() + packets_end, orig.data() + packets_end, trailer) != 0) {
    std::fprintf(stderr, "FAIL: trailer\n");
    return 1;
  }
  // Each packet as it should be: the original's header, the frames' length fields and last bits (frame bits as the
  // emulation joins them: past a packet's end, after the next packet's header; skip counts are 0), and the tag in the
  // tag bits XmoTagBits finds in the original packet (the same header, lengths and last bits).
  auto bit = [&](const std::vector<uint8_t>& f, uint64_t pos) {
    const uint64_t at = data_offset + pos / 8;
    return (f[size_t(at)] >> (7 - pos % 8)) & 1;
  };
  auto frame_pos = [&](uint64_t start, uint32_t j) {
    uint64_t packet = start / 16384, in = start % 16384 + j;
    while (in >= 16384) {
      in -= 16384;
      packet += 1;
      in += 32;
    }
    return packet * 16384 + in;
  };
  std::vector<uint8_t> expected(orig.size(), 0);
  auto set = [&](uint64_t pos) { expected[size_t(data_offset + pos / 8)] |= uint8_t(0x80 >> (pos % 8)); };
  const uint8_t* table = hd + 64 + data_offset + size_t(packets) * 4;
  for (uint32_t i = 0; i < frames; ++i) {
    const uint32_t pos = Le32(table + size_t(i) * 8), len = Le16(table + size_t(i) * 8 + 4);
    const uint8_t last = table[size_t(i) * 8 + 6];
    uint32_t b = 0;
    for (uint32_t j = 0; j < 15; ++j) {
      b = (b << 1) | bit(orig, frame_pos(pos, j));
      if ((len >> (14 - j)) & 1) set(frame_pos(pos, j));
    }
    if (b != len || bit(orig, frame_pos(pos, len - 1)) != last) {
      std::fprintf(stderr, "FAIL: frame %u: the table's length %u or last bit %u is not the original's (%u)\n", i, len,
                   last, b);
      return 1;
    }
    if (last) set(frame_pos(pos, len - 1));
  }
  uint32_t tagged = 0;
  for (uint32_t p = 0; p < packets; ++p) {
    const uint8_t* op = orig.data() + data_offset + size_t(p) * 2048;
    std::memcpy(expected.data() + data_offset + size_t(p) * 2048, op, 4);
    uint16_t bits[kXmoTagBits];
    if (XmoTagBits(op, bits, kXmoTagBits) < kXmoTagBits) continue;
    // the 72-bit tag, MSB first: magic, sound id, packet index
    for (uint32_t k = 0; k < kXmoTagBits; ++k) {
      const uint32_t v = k < 16 ? (kXmoTagMagic >> (15 - k)) & 1 : k < 48 ? (id >> (47 - k)) & 1 : (p >> (71 - k)) & 1;
      if (v) set(uint64_t(p) * 16384 + bits[k]);
    }
    ++tagged;
  }
  for (uint32_t p = 0; p < packets; ++p) {
    const size_t at = data_offset + size_t(p) * 2048;
    for (uint32_t k = 0; k < 2048; ++k) {
      if (syn[at + k] != expected[at + k]) {
        std::fprintf(stderr, "FAIL: packet %u byte %u: %02X, expected %02X\n", p, k, syn[at + k], expected[at + k]);
        return 1;
      }
    }
  }
  if (!XmoSoundKnown(id)) {
    std::fprintf(stderr, "FAIL: the sound is not registered\n");
    return 1;
  }
  auto s = FindXmoSound(id);
  if (!s || s->frame_pos.size() != frames || s->opus_offset.size() < 2) {
    std::fprintf(stderr, "FAIL: the Opus data\n");
    return 1;
  }
  // every frame found as the emulation finds it: its packet's tag in the synthesized file, then its position
  for (uint32_t i = 0; i < frames; ++i) {
    const uint32_t pos = Le32(table + size_t(i) * 8);
    uint32_t tag_id = 0, tag_packet = 0;
    if (!ReadXmoPacketTag(syn.data() + data_offset + size_t(pos / 16384) * 2048, &tag_id, &tag_packet) ||
        tag_id != id || tag_packet != pos / 16384 ||
        FindXmoFrame(*s, uint64_t(tag_packet) * 16384 + pos % 16384) != int64_t(i)) {
      std::fprintf(stderr, "FAIL: frame %u: tag %08X packet %u\n", i, tag_id, tag_packet);
      return 1;
    }
  }
  // and the original's packets carry no tag
  for (uint32_t p = 0; p < packets; ++p) {
    uint32_t tag_id = 0, tag_packet = 0;
    if (ReadXmoPacketTag(orig.data() + data_offset + size_t(p) * 2048, &tag_id, &tag_packet)) {
      std::fprintf(stderr, "NOTE: the original's packet %u reads as a tag (%08X)\n", p, tag_id);
    }
  }
  std::printf("OK %s: %zu bytes as %zu (%.1f %%), %u packets (%u tagged), %u frames, %zu Opus packets (%.1f s), "
              "pre-skip %u\n",
              argv[1], orig.size(), xmo.size(), 100.0 * xmo.size() / orig.size(), packets, tagged, frames,
              s->opus_offset.size() - 1, (s->opus_offset.size() - 1) * 0.02, s->pre_skip);
  return 0;
}

/**
 * culdcept-nx (2026-11-20, the user: re-encode the game's audio to Opus, music at 128 kbit/s): NAME.xmo is a game's XMA
 * file NAME whose frames are Opus. The game sees NAME as it was (the same size, RIFF header, packet headers, frame
 * length fields and frames' last bits, and its trailer), but the frames' other bits are zero except a tag in each packet
 * a frame starts in: the XMA emulation (sdk/src/audio/xma_context.cpp, XmaContext::DecodeXmoFrame) finds it and takes
 * the frame's 512 samples from the sound's Opus stream instead of decoding XMA bits. Loops, subframes and the output
 * buffer stay the emulation's own: only the samples come from elsewhere.
 *
 * The file (little-endian; made by culdcept-nx-installer's lib/xmo.js):
 *   header, 64 bytes:  "RXXMAOP1", u32 version (1), u32 sound id (the tag's), u64 size of NAME, u32 data offset (where
 *                      NAME's XMA packets start: after its RIFF header), u32 packet count, u32 frame count, u32 sample
 *                      rate (48000), u16 channels, u16 Opus pre-skip, u32 Opus packet count, u64 offset of the Opus
 *                      packet table, u32 trailer size (NAME's bytes after its packets: the XMA files' "seek" chunk),
 *                      u32 0
 *   NAME's first `data offset` bytes (its RIFF header, as it was)
 *   the packet headers: packet count x 4 bytes, as they were (a packet is 2048 bytes of NAME from the data offset)
 *   the frames: frame count x 8 bytes: u32 bit position in NAME's data (packet headers counted), u16 length in bits (the
 *               frame's own 15-bit field), u8 its last bit ("another frame follows in this packet"), u8 0
 *   the trailer: NAME's last `trailer size` bytes, as they were
 *   the Opus packets: u16 size each (count of them), then the packets back to back (20 ms each, 48 kHz)
 *
 * The tag, 72 bits: 16 bits 0xA55C, 32 bits the sound id, 24 bits the packet's index in NAME, MSB first, in the packet's
 * tag bits (XmoTagBits): walking the frames that start in the packet as the emulation does (from the header's first
 * frame offset, while a frame's last bit says another follows), each one's bits after its 15-bit length field and
 * before its last bit, as far as the packet's end; then the packet's bits from 47 (after the header and the 15 bits a
 * frame header split across packets can put there) to the bit before its first frame (the end of a frame that began in
 * an earlier packet). Every packet a frame of the game's 48 kHz files starts in has 104 such bits or more; the installer
 * keeps a file as XMA when one has fewer than 72.
 *
 * A frame is the one at (the tag's packet, the frame's bit in it): its index k in the frame table. Its samples are
 * [512 k, 512 k + 512) of the sound after the Opus pre-skip: what FFmpeg's xmaframes decoder gave for that frame when
 * the frames were decoded in order (the installer made the Opus from that).
 */
#pragma once

#include <cstdint>
#include <filesystem>
#include <memory>
#include <string_view>
#include <vector>

namespace rex::filesystem {
class FileHandle;

inline constexpr std::string_view kXmoSuffix = ".xmo";
inline constexpr uint16_t kXmoTagMagic = 0xA55C;
inline constexpr uint32_t kXmoTagBits = 16 + 32 + 24;
inline constexpr uint32_t kXmoPacketBytes = 2048;

bool HasXmoSuffix(std::string_view name);
// The size of the XMA file a NAME.xmo stands for (its header only), or false.
bool ReadXmoSize(const std::filesystem::path& path, uint64_t* out_size);
// The XMA file NAME, synthesized from NAME.xmo (`packed`, read only); null when it is not a valid .xmo.
std::unique_ptr<FileHandle> OpenXmo(std::unique_ptr<FileHandle> packed);

// A sound's Opus stream and frame positions, for the XMA emulation.
struct XmoSound {
  uint32_t id = 0;
  uint32_t sample_rate = 48000;
  uint16_t channels = 0;
  uint16_t pre_skip = 0;
  std::vector<uint32_t> frame_pos;    // each frame's bit position in NAME's data, ascending
  std::vector<uint8_t> opus;          // the packets back to back
  std::vector<uint32_t> opus_offset;  // each packet's offset in `opus`, and its end as the last entry
};
// Whether a NAME.xmo the game has opened has this sound id (a tag naming an unknown id is not one: XMA bits).
bool XmoSoundKnown(uint32_t id);
// The sound with this id, its Opus data read if it is not held (the game's read of the file's data reads it, on the
// game's thread); null if unknown or unreadable.
std::shared_ptr<const XmoSound> FindXmoSound(uint32_t id);
// The index of the frame at `bit_pos` of the sound's data, or -1.
int64_t FindXmoFrame(const XmoSound& sound, uint64_t bit_pos);

// The tag bits of a packet (its 2048 bytes: the header, frame length fields and last bits as in the file): up to `max`
// bit positions in it, in order; how many.
uint32_t XmoTagBits(const uint8_t* packet, uint16_t* bits, uint32_t max);
// The tag of a packet: true with the sound id and the packet's index when it has one.
bool ReadXmoPacketTag(const uint8_t* packet, uint32_t* id, uint32_t* packet_index);

}  // namespace rex::filesystem

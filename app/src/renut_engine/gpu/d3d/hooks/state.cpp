/**
 * @file    gpu/d3d/hooks/state.cpp
 * @brief   N&B's D3D state calls, hooked with the native ports (d3d_native.h).
 *
 * The render and sampler state setters are the ones N&B's state tables name
 * (D3D::InitializeApiState puts them into every device's dispatch table,
 * which the game's inline SetRenderState / SetSamplerState call through).
 * The rest are D3DDevice_Set* and the CDevice internals that write the ring.
 */
#include <cstring>

#include <rex/hook.h>

#include "renut_engine/gpu/banjo_renut_select.h"
#include <rex/types.h>

#include "renut_engine/gpu/d3d/d3d_native.h"
#include "renut_engine/gpu/guest/guest_memory.h"
#include "renut_engine/gpu/banjo_renut_tuning.h"  // blur-nx (2026-11-10, port 76; its key 213): OcclusionQueriesOn
#include "renut_engine/gpu/log.h"
#include "renut_engine/gpu/render/surfaces.h"  // blur-nx (2026-11-10, port 76; its key 215): NoteSurfaceHeader
#include <algorithm>

using namespace renut::gpu;

namespace {

template <u32 State>
void SetRenderStateHook(u32 device, u32 value) {
  native::SetRenderState(native::DeviceAt(device), State, value);
}

template <u32 State>
void SetSamplerStateHook(u32 device, u32 sampler, u32 value) {
  native::SetSamplerState(native::DeviceAt(device), sampler, State, value);
}

template <u32 State>
u32 GetRenderStateHook(u32 device) {
  return native::GetRenderState(native::DeviceAt(device), State);
}

template <u32 State>
u32 GetSamplerStateHook(u32 device, u32 sampler) {
  return native::GetSamplerState(native::DeviceAt(device), sampler, State);
}

// The device's AddRef of a resource it hands out (D3DResource_AddRef,
// resource.cpp).
u32 AddRefResource(u32 resource) {
  return native::AddRefResource(resource);
}

// D3DSurface *GetRenderTarget(D3DDevice*, DWORD Index) [0x82636ED0]
u32 D3DDevice_GetRenderTarget_hook(u32 device, u32 index) {
  const native::Device d = native::DeviceAt(device);
  const u32 surface = d && index < 4 ? d.W(native::off::RenderTargets + index * 4) : 0;
  if (surface)
    AddRefResource(surface);
  return surface;
}

// D3DSurface *GetDepthStencilSurface(D3DDevice*) [0x82636F10]
u32 D3DDevice_GetDepthStencilSurface_hook(u32 device) {
  const native::Device d = native::DeviceAt(device);
  const u32 surface = d ? d.W(native::off::DepthStencil) : 0;
  if (surface)
    AddRefResource(surface);
  return surface;
}

// D3DVertexBuffer *GetStreamSource(D3DDevice*, UINT StreamNumber,
//     UINT *pOffsetInBytes, UINT *pStride) [0x82636E40]: the offset is the
// stream's fetch constant address less the buffer's.
u32 D3DDevice_GetStreamSource_hook(u32 device, u32 stream, mapped_u32 offset, mapped_u32 stride) {
  const native::Device d = native::DeviceAt(device);
  if (!d || stream >= 16)
    return 0;
  const u32 buffer = d.W(native::off::StreamSources + stream * 4);
  u32 bytes = 0;
  if (buffer) {
    const u32 fetch = d.W(native::off::TextureFetch + (95 - stream) * 8);
    bytes = fetch - native::GpuAddress(GuestPtr<const be_u32>(buffer)[6]);
    AddRefResource(buffer);
  }
  if (offset)
    *offset = bytes;
  if (stride)
    *stride = u32(d.B(native::off::StreamStrides + stream)) << 2;
  return buffer;
}

// void D3DDevice_GetViewport(D3DDevice*, D3DVIEWPORT9*) [0x82244B18]
void D3DDevice_GetViewport_hook(u32 device, mapped_u32 viewport) {
  const native::Device d = native::DeviceAt(device);
  if (!d || !viewport)
    return;
  for (u32 i = 0; i < 4; ++i)  // fctidz, the low word
    viewport[i] = d.W(0x3058 + i * 4);  // culdcept-nx (vivapinata-nx 24da6e4f): integers in this XDK  // katamari-nx: 0x3170->0x3160 (culdcept-nx: 0x3160->0x3058)
  viewport[4] = d.W(0x3068);  // katamari-nx: 0x3180->0x3170 (culdcept-nx: 0x3170->0x3068)
  viewport[5] = d.W(0x306C);  // katamari-nx: 0x3184->0x3174 (culdcept-nx: 0x3174->0x306C)
}

// void D3DDevice_SetClipPlane(D3DDevice*, DWORD Index, const float *pPlane) [0x826370E8]
void D3DDevice_SetClipPlane_hook(u32 device, u32 index, mapped_u32 plane) {
  native::Device d = native::DeviceAt(device);
  if (!d || !plane || index >= 6)
    return;
  for (u32 i = 0; i < 4; ++i)
    d.SetW(0x27A0 + index * 16 + i * 4, plane[i]);  // culdcept-nx: 0x2820->0x27a0
  d.Dirty(4, u64(1) << (54 - index));
}

// void D3DDevice_SetVertexFetchConstant(D3DDevice*, UINT VertexFetchRegister,
//     D3DVertexBuffer*, UINT Offset) [0x82637138]: a vertex fetch slot of the
// 3 in a fetch constant, made a vertex fetch constant first.
void D3DDevice_SetVertexFetchConstant_hook(u32 device, u32 reg, u32 vertex_buffer, u32 offset) {
  native::Device d = native::DeviceAt(device);
  const auto *vb = vertex_buffer ? GuestPtr<const be_u32>(vertex_buffer) : nullptr;
  if (!d || !vb || reg > 0x5F)
    return;
  const u32 slot = (0x5F - reg) / 3;
  const u32 part = (0x5F - reg) - slot * 3;
  const u32 constant = native::off::TextureFetch + slot * 24;
  const u32 type = d.W(constant) & 3;
  if (type == 0 || type == 2)
    std::memset(d.bytes + constant, 0, 24);
  d.TouchRange(constant, 24);  // banjo-nx (2026-10-26, banjo_renut_tags)
  d.SetW(constant, native::Insert(d.W(constant), 1, 0, 2));
  d.Dirty(3, u64(1) << (31 - slot));
  d.SetW(constant + part * 8, native::GpuAddress(u32(vb[6]) + offset));
  d.SetW(constant + part * 8 + 4, native::Insert(u32(vb[7]) - offset, 1, 28, 2));
}

// The boolean and integer shader constants (D3DDevice_Set{Vertex,Pixel}
// ShaderConstant{B,I}): bits from 0x2780 (vertex) / 0x2790 (pixel), a word
// of x, y, z bytes each from 0x27A0 / 0x27E0.
void SetShaderConstantsB(u32 device, u32 base, u32 start, mapped_u32 data, u32 count) {
  native::Device d = native::DeviceAt(device);
  if (!d || !data)
    return;
  for (u32 i = 0; i < count; ++i, ++start) {
    const u32 word = base + (start >> 5) * 4;
    const u32 bit = 1u << (start & 31);
    d.SetW(word, (d.W(word) & ~bit) | ((u32(data[i]) & 1) << (start & 31)));
  }
  d.Dirty(4, u64(1) << 56);
}

void SetShaderConstantsI(u32 device, u32 base, u32 start, mapped_u32 data, u32 count) {
  native::Device d = native::DeviceAt(device);
  if (!d || !data)
    return;
  for (u32 i = 0; i < count; ++i) {
    const u32 x = u32(data[i * 4]) & 0xFF, y = u32(data[i * 4 + 1]) & 0xFF;
    const u32 z = u32(data[i * 4 + 2]) & 0xFF;
    d.SetW(base + (start + i) * 4, (z << 16) | (y << 8) | x);
  }
  d.Dirty(4, u64(1) << 56);
}

// void D3DDevice_SetVertexShaderConstantB(D3DDevice*, UINT StartRegister,
//     const BOOL*, UINT BoolCount) [0x821F0B40], and the others alike.
void D3DDevice_SetVertexShaderConstantB_hook(u32 device, u32 start, mapped_u32 data, u32 count) {
  SetShaderConstantsB(device, 0x2700, start, data, count);  // culdcept-nx: 0x2780->0x2700 (-128, the constants at 0x400)
}
void D3DDevice_SetPixelShaderConstantB_hook(u32 device, u32 start, mapped_u32 data, u32 count) {
  // culdcept-nx: 0x2790->0x2710: Culdcept's own SetPixelShaderConstantB (sub_8217C790) indexes word 2500 + start / 32.
  SetShaderConstantsB(device, 0x2710, start, data, count);
}
void D3DDevice_SetVertexShaderConstantI_hook(u32 device, u32 start, mapped_u32 data, u32 count) {
  SetShaderConstantsI(device, 0x27A0, start, data, count);
}
void D3DDevice_SetPixelShaderConstantI_hook(u32 device, u32 start, mapped_u32 data, u32 count) {
  SetShaderConstantsI(device, 0x27E0, start, data, count);
}

// void D3DDevice_EndIndexedVertices(D3DDevice*) [0x8263ED90]: the inline end
// of BeginIndexedVertices, back to the ring pointer it saved.
void D3DDevice_EndIndexedVertices_hook(u32 device) {
  native::Device d = native::DeviceAt(device);
  if (d)
    d.SetW(native::off::Ring, d.W(0x3314));  // katamari-nx: 0x3484->0x3474 (culdcept-nx: 0x3474->0x3314)
}

// void D3DDevice_SetTexture(D3DDevice*, DWORD Sampler, D3DBaseTexture*,
//     UINT64 PendingMask3) [0x821FE558]; the mask is the sampler's fetch
// constant bit, which the port derives from Sampler.
void D3DDevice_SetTexture_hook(u32 device, u32 sampler, u32 texture) {
  native::SetTexture(native::DeviceAt(device), sampler, texture);
}

// void D3DDevice_SetStreamSource(D3DDevice*, UINT StreamNumber, D3DVertexBuffer*,
//     UINT OffsetInBytes, UINT Stride, UINT64 PendingMask3) [0x821F9438]
void D3DDevice_SetStreamSource_hook(u32 device, u32 stream, u32 vertex_buffer, u32 offset,
                                    u32 stride) {
  native::SetStreamSource(native::DeviceAt(device), stream, vertex_buffer, offset, stride);
}

// void D3DDevice_SetIndices(D3DDevice*, D3DIndexBuffer*) [0x821DE438]
void D3DDevice_SetIndices_hook(u32 device, u32 index_buffer) {
  native::SetIndices(native::DeviceAt(device), index_buffer);
}

// void D3DDevice_SetVertexDeclaration(D3DDevice*, D3DVertexDeclaration*) [0x821D5060]
void D3DDevice_SetVertexDeclaration_hook(u32 device, u32 declaration) {
  native::SetVertexDeclaration(native::DeviceAt(device), declaration);
}

// void D3DDevice_SetViewport(D3DDevice*, const D3DVIEWPORT9*) [0x821F2090]
void D3DDevice_SetViewport_hook(u32 device, u32 viewport) {
  native::SetViewport(native::DeviceAt(device), viewport);
}

// void D3D::SetViewport(D3DDevice*, float X, float Y, float Width, float Height,
//     float MinZ, float MaxZ) [0x821F2110]
void D3D_SetViewport_hook(u32 device, float x, float y, float width, float height, float min_z,
                          float max_z) {
  native::SetViewport(native::DeviceAt(device), x, y, width, height, min_z, max_z);
}

// void D3DDevice_SetScissorRect(D3DDevice*, const RECT*) [0x821F2368]
void D3DDevice_SetScissorRect_hook(u32 device, mapped_u32 rect) {
  if (!rect)
    return;
  native::SetScissorRect(native::DeviceAt(device), i32(u32(rect[0])), i32(u32(rect[1])),
                         i32(u32(rect[2])), i32(u32(rect[3])));
}

// void D3D::SetSurfaceClip(CDevice*, int, int, int, int) [0x821F2460]: the
// window scissor and per-tile clip packets, nothing else.
void D3D_SetSurfaceClip_hook(u32 /*device*/, u32, u32, u32, u32) {}

// void D3DDevice_SetRenderTarget(D3DDevice*, DWORD Index, D3DSurface*) [0x82248BD0]
void D3DDevice_SetRenderTarget_hook(u32 device, u32 index, u32 surface) {
  NoteSurfaceHeader(surface);  // blur-nx (2026-11-10, port 76; its key 215: blur_renut_stack_surface_headers)
  native::SetRenderTarget(native::DeviceAt(device), index, surface);
}

// void D3DDevice_SetDepthStencilSurface(D3DDevice*, D3DSurface*) [0x82248FB0]
void D3DDevice_SetDepthStencilSurface_hook(u32 device, u32 surface) {
  NoteSurfaceHeader(surface);  // blur-nx (2026-11-10, port 76; its key 215: blur_renut_stack_surface_headers)
  native::SetDepthStencilSurface(native::DeviceAt(device), surface);
}

// void D3D::SetSurfaceInfo(CDevice*, D3DSurface*) [0x82248F20]
void D3D_SetSurfaceInfo_hook(u32 device, u32 surface) {
  native::SetSurfaceInfo(native::DeviceAt(device), surface);
}

// void D3DDevice_SetPredication(D3DDevice*, DWORD PredicationMask) [0x821F9A48]
void D3DDevice_SetPredication_hook(u32 device, u32 mask) {
  native::SetPredication(native::DeviceAt(device), mask);
}

// void D3DDevice_SetCommandBufferPredication(D3DDevice*, DWORD TilePredication,
//     DWORD RunPredication) [0x826411C0]
void D3DDevice_SetCommandBufferPredication_hook(u32 device, u32 tile_predication,
                                                u32 run_predication) {
  native::SetCommandBufferPredication(native::DeviceAt(device), tile_predication,
                                      run_predication);
}

// void D3DDevice_SetShaderGPRAllocation(D3DDevice*, DWORD Flags,
//     DWORD VertexShaderCount, DWORD PixelShaderCount) [0x821EFD38]
void D3DDevice_SetShaderGPRAllocation_hook(u32 device, u32 flags, u32 vertex_count,
                                           u32 pixel_count) {
  native::SetShaderGPRAllocation(native::DeviceAt(device), flags, vertex_count, pixel_count);
}

// void D3D::CleanupGprAllocationSetup(CDevice*) [0x82637208]
void D3D_CleanupGprAllocationSetup_hook(u32 device) {
  native::CleanupGprAllocationSetup(native::DeviceAt(device));
}

// void D3DDevice_SetScreenExtentQueryMode(D3DDevice*, D3DSCREENEXTENTQUERYMODE)
// [0x826414C0]
void D3DDevice_SetScreenExtentQueryMode_hook(u32 device, u32 mode) {
  native::SetScreenExtentQueryMode(native::DeviceAt(device), mode);
}

// void D3DDevice_UnsetAll(D3DDevice*) [0x82634C10]
void D3DDevice_UnsetAll_hook(u32 device) {
  native::UnsetAll(native::DeviceAt(device));
}

// void D3D::ResetAllState(CDevice*, BOOL) [0x82255C98]
void D3D_ResetAllState_hook(u32 device, u32 /*skip_packets*/) {
  native::ResetAllState(native::DeviceAt(device));
}

// void D3D::InvalidateConstants(D3DDevice*) [0x82634B90]
void D3D_InvalidateConstants_hook(u32 device) {
  native::InvalidateConstants(native::DeviceAt(device));
}

// void D3DDevice_BlockUntilIdle(D3DDevice*) [0x82640010] (IDA: the second
// D3D::InvalidateConstants): waits for the device's current fence (N&B calls
// it before releasing a device).
void D3DDevice_BlockUntilIdle_hook(u32 device) {
  native::InvalidateConstantsAndWait(native::DeviceAt(device));
}

} // namespace

// blur-nx (2026-11-10/11, port 76; its key 213: blur_renut_occlusion_queries): D3DQuery_GetData's answer. Renut runs no
// GPU packets, so the counts an occlusion query waits for never come: under Renut, Issue(END) leaves the sample blocks
// as they were (zeros, then whatever the memory held) and GetData said "not ready" to every query for ever, so whatever
// a game hides by a query was never drawn (Blur's light glows, the tail lights' red glow). AnswerOcclusionQuery, called
// before the game's own GetData runs, writes into every block of an issued occlusion query what the reference's GPU
// writes at the ZPASS event (the SDK's command_processor.cpp, ExecutePacketType3_EVENT_WRITE_ZPD): the block zeroed,
// Total_A and ZPass_A = 1000 samples passed (little-endian, as the GPU writes them), and clears the issue's fence word,
// which GetData compares with the GPU's read pointer: the counts are in.
// The query object as XDK 2.0.11164's GetData reads it (blur-nx, sub_8224CD70; D3DQuery_Issue is sub_8224CAE8, the two
// functions holding the 0xFFFFFEED sentinel): type 9 (occlusion) at +4, issued (bit 0x40) in the byte at +20, the fence
// at +24, up to 30 sample-block pointers from +28 and their number at +148, 64-byte blocks (END counts at +0, BEGIN at
// +32). A game at another XDK checks its own GetData against this before it registers it.
// Not hooked in the base: Beautiful Katamari's XDK 2.0.5632 image links no D3DQuery_Issue / GetData (no instruction or
// data word of it holds 0xFFFFFEED). A game whose image has GetData adds RENUT_D3DQUERY_GETDATA_HOOK(sub_<address>) to
// the hook list below (or calls AnswerOcclusionQuery from a GetData hook of its own), and runs post-codegen again
// (tools/post_codegen.py: direct_calls.py --redo) so GetData's callers go through the hooked entry.
namespace renut::gpu::native {
namespace {
constexpr u32 kQueryFakeSamples = 1000;  // the SDK's query_occlusion_fake_sample_count
u32 QueryBe(const u8 *p) { return u32(p[0]) << 24 | u32(p[1]) << 16 | u32(p[2]) << 8 | u32(p[3]); }
void QueryLe(u8 *p, u32 v) {
  p[0] = u8(v);
  p[1] = u8(v >> 8);
  p[2] = u8(v >> 16);
  p[3] = u8(v >> 24);
}
}  // namespace

// blur-nx (2026-11-11, port 76; its key 213): a query block's physical address as the game's GetData forms it from a
// pointer at query +28: (p & 0x1FFFFFFF) + (((p >> 20) + 0x200) & 0x1000), read through the 0xC0000000 mapping. A
// pointer in the 0xE0000000 range (Blur's blocks: E6D8DFC0) is a page further on (the console's 4 KB offset of that
// range). Answers written at the pointer's own page were never read: GetData read the sentinel there and said not ready.
u32 QueryBlockPhysical(u32 p) { return (p & 0x1FFFFFFFu) + ((((p >> 20) + 0x200u) & 0x1000u)); }

bool AnswerOcclusionQuery(u32 query) {
  if (!banjo::renut_gpu::OcclusionQueriesOn())
    return false;
  u8 *q = GuestPtr<u8>(query);
  if (!q || QueryBe(q + 4) != 9 || !(q[20] & 0x40))
    return false;
  const u32 blocks = std::min<u32>(QueryBe(q + 148), 30);
  bool answered = false;
  for (u32 i = 0; i < blocks; ++i) {
    const u32 pointer = QueryBe(q + 28 + 4 * i);
    if (!pointer)
      continue;
    // Every issued query, not only blocks holding the sentinel (under Renut they hold zeros or leftover memory): the
    // reference's answer for every query is 1000 passed.
    u8 *block = GuestPhysicalPtr<u8>(QueryBlockPhysical(pointer));
    std::memset(block, 0, 64);
    QueryLe(block + 0, kQueryFakeSamples);   // Total_A
    QueryLe(block + 16, kQueryFakeSamples);  // ZPass_A
    answered = true;
  }
  if (answered) {
    std::memset(q + 24, 0, 4);
    static uint32_t logged = 0;
    if (logged++ < 4)
      RGPU_INFO("[renut] occlusion query {:08X}: {} block(s) answered with {} samples passed, as the reference's GPU "
                "does (blur_renut_occlusion_queries)", query, blocks, kQueryFakeSamples);
  }
  return answered;
}
}  // namespace renut::gpu::native

// blur-nx (2026-11-10/11, port 76; its key 213): D3DQuery_GetData(query, data, size, flags) at the game's own address:
// the answer above, then the game's GetData (its __imp__), whose result the game gets. Under the emulated renderer the
// game's GetData alone (BANJO_RENUT_HOOK). One line in the hook list: RENUT_D3DQUERY_GETDATA_HOOK(sub_8224CD70);
// (blur-nx's address; the base registers none, see AnswerOcclusionQuery).
#define RENUT_D3DQUERY_GETDATA_HOOK(subroutine)                                                                   \
  REX_IMPORT(__imp__##subroutine, renut_query_get_data_original_##subroutine, u32(u32, u32, u32, u32));          \
  static u32 renut_query_get_data_##subroutine(u32 query, u32 data, u32 size, u32 flags) {                        \
    ::renut::gpu::native::AnswerOcclusionQuery(query);                                                            \
    return renut_query_get_data_original_##subroutine(query, data, size, flags);                                  \
  }                                                                                                               \
  BANJO_RENUT_HOOK(subroutine, renut_query_get_data_##subroutine)

// Render states, by their index in N&B's table (off_82E506F0).
BANJO_RENUT_HOOK(sub_82179848, SetRenderStateHook<10>);  // culdcept-nx: Katamari sub_824DC2F8; render state table row 10 (setter)
BANJO_RENUT_HOOK(sub_821798C0, SetRenderStateHook<11>);  // culdcept-nx: Katamari sub_824DC368; render state table row 11 (setter)
BANJO_RENUT_HOOK(sub_82179890, SetRenderStateHook<12>);  // culdcept-nx: Katamari sub_824DC338; render state table row 12 (setter)
BANJO_RENUT_HOOK(sub_82179200, SetRenderStateHook<13>);  // culdcept-nx: Katamari sub_824DBBD8; render state table row 13 (setter)
BANJO_RENUT_HOOK(sub_821791D0, SetRenderStateHook<14>);  // culdcept-nx: Katamari sub_824DBBA8; render state table row 14 (setter)
BANJO_RENUT_HOOK(sub_82179270, SetRenderStateHook<15>);  // culdcept-nx: Katamari sub_824DBC40; render state table row 15 (setter)
BANJO_RENUT_HOOK(sub_82179600, SetRenderStateHook<16>);  // culdcept-nx: Katamari sub_824DBFD0; render state table row 16 (setter)
BANJO_RENUT_HOOK(sub_82179730, SetRenderStateHook<17>);  // culdcept-nx: Katamari sub_824DC100; render state table row 17 (setter)
BANJO_RENUT_HOOK(sub_82179390, SetRenderStateHook<18>);  // culdcept-nx: Katamari sub_824DBD60; render state table row 18 (setter)
BANJO_RENUT_HOOK(sub_82179420, SetRenderStateHook<19>);  // culdcept-nx: Katamari sub_824DBDF0; render state table row 19 (setter)
BANJO_RENUT_HOOK(sub_82179300, SetRenderStateHook<20>);  // culdcept-nx: Katamari sub_824DBCD0; render state table row 20 (setter)
BANJO_RENUT_HOOK(sub_82179520, SetRenderStateHook<21>);  // culdcept-nx: Katamari sub_824DBEF0; render state table row 21 (setter)
BANJO_RENUT_HOOK(sub_82179590, SetRenderStateHook<22>);  // culdcept-nx: Katamari sub_824DBF60; render state table row 22 (setter)
BANJO_RENUT_HOOK(sub_821794B0, SetRenderStateHook<23>);  // culdcept-nx: Katamari sub_824DBE80; render state table row 23 (setter)
BANJO_RENUT_HOOK(sub_82179230, SetRenderStateHook<24>);  // culdcept-nx: Katamari sub_824DBC08; render state table row 24 (setter)
BANJO_RENUT_HOOK(sub_82179698, SetRenderStateHook<25>);  // culdcept-nx: Katamari sub_824DC068; render state table row 25 (setter)
BANJO_RENUT_HOOK(sub_82179700, SetRenderStateHook<26>);  // culdcept-nx: Katamari sub_824DC0D0; render state table row 26 (setter)
BANJO_RENUT_HOOK(sub_82179900, SetRenderStateHook<27>);  // culdcept-nx: Katamari sub_824DC3A0; render state table row 27 (setter)
BANJO_RENUT_HOOK(sub_82179948, SetRenderStateHook<28>);  // culdcept-nx: Katamari sub_824DC3E0; render state table row 28 (setter)
BANJO_RENUT_HOOK(sub_821799B8, SetRenderStateHook<29>);  // culdcept-nx: Katamari sub_824DC448; render state table row 29 (setter)
BANJO_RENUT_HOOK(sub_821799F8, SetRenderStateHook<30>);  // culdcept-nx: Katamari sub_824DC480; render state table row 30 (setter)
BANJO_RENUT_HOOK(sub_82179A38, SetRenderStateHook<31>);  // culdcept-nx: Katamari sub_824DC4B8; render state table row 31 (setter)
BANJO_RENUT_HOOK(sub_82179988, SetRenderStateHook<32>);  // culdcept-nx: Katamari sub_824DC418; render state table row 32 (setter)
BANJO_RENUT_HOOK(sub_82179B48, SetRenderStateHook<33>);  // culdcept-nx: Katamari sub_824DC5B8; render state table row 33 (setter)
BANJO_RENUT_HOOK(sub_82179B68, SetRenderStateHook<34>);  // culdcept-nx: Katamari sub_824DC5D8; render state table row 34 (setter)
BANJO_RENUT_HOOK(sub_82179B88, SetRenderStateHook<35>);  // culdcept-nx: Katamari sub_824DC5F8; render state table row 35 (setter)
BANJO_RENUT_HOOK(sub_82179A98, SetRenderStateHook<36>);  // culdcept-nx: Katamari sub_824DC518; render state table row 36 (setter)
BANJO_RENUT_HOOK(sub_82179AD8, SetRenderStateHook<37>);  // culdcept-nx: Katamari sub_824DC550; render state table row 37 (setter)
BANJO_RENUT_HOOK(sub_82179B18, SetRenderStateHook<38>);  // culdcept-nx: Katamari sub_824DC588; render state table row 38 (setter)
BANJO_RENUT_HOOK(sub_82179A68, SetRenderStateHook<39>);  // culdcept-nx: Katamari sub_824DC4E8; render state table row 39 (setter)
BANJO_RENUT_HOOK(sub_82179BA8, SetRenderStateHook<40>);  // culdcept-nx: Katamari sub_824DC618; render state table row 40 (setter)
BANJO_RENUT_HOOK(sub_82179BC8, SetRenderStateHook<41>);  // culdcept-nx: Katamari sub_824DC638; render state table row 41 (setter)
BANJO_RENUT_HOOK(sub_82179BE8, SetRenderStateHook<42>);  // culdcept-nx: Katamari sub_824DC658; render state table row 42 (setter)
BANJO_RENUT_HOOK(sub_82179C08, SetRenderStateHook<43>);  // culdcept-nx: Katamari sub_824DC678; render state table row 43 (setter)
BANJO_RENUT_HOOK(sub_82179F20, SetRenderStateHook<44>);  // culdcept-nx: Katamari sub_824DC9A0; render state table row 44 (setter)
BANJO_RENUT_HOOK(sub_82179F80, SetRenderStateHook<45>);  // culdcept-nx: Katamari sub_824DCA00; render state table row 45 (setter)
BANJO_RENUT_HOOK(sub_82179F10, SetRenderStateHook<46>);  // culdcept-nx: Katamari sub_824DC990; render state table row 46 (setter)
BANJO_RENUT_HOOK(sub_82179FD8, SetRenderStateHook<47>);  // culdcept-nx: Katamari sub_824DCA58; render state table row 47 (setter)
BANJO_RENUT_HOOK(sub_82179DE0, SetRenderStateHook<48>);  // culdcept-nx: Katamari sub_824DC840; render state table row 48 (setter)
BANJO_RENUT_HOOK(sub_82179E10, SetRenderStateHook<49>);  // culdcept-nx: Katamari sub_824DC870; render state table row 49 (setter)
BANJO_RENUT_HOOK(sub_82179C58, SetRenderStateHook<50>);  // culdcept-nx: Katamari sub_824DE720; render state table row 50 (setter)
BANJO_RENUT_HOOK(sub_82179C70, SetRenderStateHook<51>);  // culdcept-nx: Katamari sub_824DC6D0; render state table row 51 (setter)
BANJO_RENUT_HOOK(sub_82179D38, SetRenderStateHook<52>);  // culdcept-nx: Katamari sub_824DC798; render state table row 52 (setter)
BANJO_RENUT_HOOK(sub_82179E30, SetRenderStateHook<53>);  // culdcept-nx: Katamari sub_824DC890; render state table row 53 (setter)
BANJO_RENUT_HOOK(sub_82179E68, SetRenderStateHook<54>);  // culdcept-nx: Katamari sub_824DC8D0; render state table row 54 (setter)
BANJO_RENUT_HOOK(sub_82179EA0, SetRenderStateHook<55>);  // culdcept-nx: Katamari sub_824DC910; render state table row 55 (setter)
BANJO_RENUT_HOOK(sub_82179ED8, SetRenderStateHook<56>);  // culdcept-nx: Katamari sub_824DC950; render state table row 56 (setter)
BANJO_RENUT_HOOK(sub_8217A730, SetRenderStateHook<57>);  // culdcept-nx: Katamari sub_824DD190; render state table row 57 (setter)
BANJO_RENUT_HOOK(sub_8217A6C0, SetRenderStateHook<58>);  // culdcept-nx: Katamari sub_824DD120; render state table row 58 (setter)
BANJO_RENUT_HOOK(sub_8217A6F8, SetRenderStateHook<59>);  // culdcept-nx: Katamari sub_824DD158; render state table row 59 (setter)
BANJO_RENUT_HOOK(sub_8217A030, SetRenderStateHook<60>);  // culdcept-nx: Katamari sub_824DCAB0; render state table row 60 (setter)
BANJO_RENUT_HOOK(sub_8217A058, SetRenderStateHook<61>);  // culdcept-nx: Katamari sub_824DCAD0; render state table row 61 (setter)
BANJO_RENUT_HOOK(sub_8217A088, SetRenderStateHook<62>);  // culdcept-nx: Katamari sub_824DCAF8; render state table row 62 (setter)
BANJO_RENUT_HOOK(sub_8217A0B8, SetRenderStateHook<63>);  // culdcept-nx: Katamari sub_824DCB20; render state table row 63 (setter)
BANJO_RENUT_HOOK(sub_8217A0E8, SetRenderStateHook<64>);  // culdcept-nx: Katamari sub_824DCB48; render state table row 64 (setter)
BANJO_RENUT_HOOK(sub_8217A118, SetRenderStateHook<65>);  // culdcept-nx: Katamari sub_824DCB70; render state table row 65 (setter)
BANJO_RENUT_HOOK(sub_8217A148, SetRenderStateHook<66>);  // culdcept-nx: Katamari sub_824DCB98; render state table row 66 (setter)
BANJO_RENUT_HOOK(sub_8217A178, SetRenderStateHook<67>);  // culdcept-nx: Katamari sub_824DCBC0; render state table row 67 (setter)
BANJO_RENUT_HOOK(sub_8217A1A0, SetRenderStateHook<68>);  // culdcept-nx: Katamari sub_824DCBE0; render state table row 68 (setter)
BANJO_RENUT_HOOK(sub_8217A1C8, SetRenderStateHook<69>);  // culdcept-nx: Katamari sub_824DCC00; render state table row 69 (setter)
BANJO_RENUT_HOOK(sub_8217A1F8, SetRenderStateHook<70>);  // culdcept-nx: Katamari sub_824DCC28; render state table row 70 (setter)
BANJO_RENUT_HOOK(sub_8217A228, SetRenderStateHook<71>);  // culdcept-nx: Katamari sub_824DCC50; render state table row 71 (setter)
BANJO_RENUT_HOOK(sub_8217A258, SetRenderStateHook<72>);  // culdcept-nx: Katamari sub_824DCC78; render state table row 72 (setter)
BANJO_RENUT_HOOK(sub_8217A288, SetRenderStateHook<73>);  // culdcept-nx: Katamari sub_824DCCA0; render state table row 73 (setter)
BANJO_RENUT_HOOK(sub_8217A2B8, SetRenderStateHook<74>);  // culdcept-nx: Katamari sub_824DCCC8; render state table row 74 (setter)
BANJO_RENUT_HOOK(sub_8217A2E8, SetRenderStateHook<75>);  // culdcept-nx: Katamari sub_824DCCF0; render state table row 75 (setter)
BANJO_RENUT_HOOK(sub_8217A3B0, SetRenderStateHook<76>);  // culdcept-nx: Katamari sub_824DCE10; render state table row 76 (setter)
BANJO_RENUT_HOOK(sub_8217A400, SetRenderStateHook<77>);  // culdcept-nx: Katamari sub_824DCE60; render state table row 77 (setter)
BANJO_RENUT_HOOK(sub_8217A4B0, SetRenderStateHook<78>);  // culdcept-nx: Katamari sub_824DCF10; render state table row 78 (setter)
BANJO_RENUT_HOOK(sub_8217A560, SetRenderStateHook<79>);  // culdcept-nx: Katamari sub_824DCFC0; render state table row 79 (setter)
BANJO_RENUT_HOOK(sub_8217A610, SetRenderStateHook<80>);  // culdcept-nx: Katamari sub_824DD070; render state table row 80 (setter)
BANJO_RENUT_HOOK(sub_8217A768, SetRenderStateHook<81>);  // culdcept-nx: Katamari sub_824DD1C8; render state table row 81 (setter)
BANJO_RENUT_HOOK(sub_8217A7A0, SetRenderStateHook<82>);  // culdcept-nx: Katamari sub_824DD200; render state table row 82 (setter)
BANJO_RENUT_HOOK(sub_8217A7D0, SetRenderStateHook<83>);  // culdcept-nx: Katamari sub_824DD230; render state table row 83 (setter)
BANJO_RENUT_HOOK(sub_8217A7F0, SetRenderStateHook<84>);  // culdcept-nx: Katamari sub_824DD258; render state table row 84 (setter)
BANJO_RENUT_HOOK(sub_8217A820, SetRenderStateHook<85>);  // culdcept-nx: Katamari sub_824DD288; render state table row 85 (setter)
BANJO_RENUT_HOOK(sub_8217A858, SetRenderStateHook<86>);  // culdcept-nx: Katamari sub_824DD2C0; render state table row 86 (setter)
BANJO_RENUT_HOOK(sub_8217A888, SetRenderStateHook<87>);  // culdcept-nx: Katamari sub_824DD2F0; render state table row 87 (setter)
BANJO_RENUT_HOOK(sub_8217A8B8, SetRenderStateHook<88>);  // culdcept-nx: Katamari sub_824DD320; render state table row 88 (setter)
BANJO_RENUT_HOOK(sub_8217A8E0, SetRenderStateHook<89>);  // culdcept-nx: Katamari sub_824DD348; render state table row 89 (setter)
BANJO_RENUT_HOOK(sub_8217A948, SetRenderStateHook<90>);  // culdcept-nx: Katamari sub_824DD410; render state table row 90 (setter)
BANJO_RENUT_HOOK(sub_8217A978, SetRenderStateHook<91>);  // culdcept-nx: Katamari sub_824DD440; render state table row 91 (setter)
BANJO_RENUT_HOOK(sub_8217A9A8, SetRenderStateHook<92>);  // culdcept-nx: Katamari sub_824DD470; render state table row 92 (setter)
BANJO_RENUT_HOOK(sub_8217A9D8, SetRenderStateHook<93>);  // culdcept-nx: Katamari sub_824DD4A0; render state table row 93 (setter)
BANJO_RENUT_HOOK(sub_8217A9F8, SetRenderStateHook<94>);  // culdcept-nx: Katamari sub_824DD4C0; render state table row 94 (setter)
BANJO_RENUT_HOOK(sub_8217AA08, SetRenderStateHook<95>);  // culdcept-nx: Katamari sub_824DD4D0; render state table row 95 (setter)
BANJO_RENUT_HOOK(sub_8217A910, SetRenderStateHook<96>);  // culdcept-nx: Katamari sub_824DD378; render state table row 96 (setter)
BANJO_RENUT_UNHOOKED_HOOK(unlinked_824DD3A8, SetRenderStateHook<97>);  // culdcept-nx: Katamari sub_824DD3A8; not placed in this game: render state 97: not in this XDK's table
BANJO_RENUT_UNHOOKED_HOOK(unlinked_824DC220, SetRenderStateHook<98>);  // culdcept-nx: Katamari sub_824DC220; not placed in this game: render state 98: not in this XDK's table
BANJO_RENUT_UNHOOKED_HOOK(unlinked_824DC258, SetRenderStateHook<99>);  // culdcept-nx: Katamari sub_824DC258; not placed in this game: render state 99: not in this XDK's table
BANJO_RENUT_UNHOOKED_HOOK(unlinked_824DC2C8, SetRenderStateHook<100>);  // culdcept-nx: Katamari sub_824DC2C8; not placed in this game: render state 100: not in this XDK's table

// Sampler states, by their index in N&B's table (off_82E50BD0).
BANJO_RENUT_HOOK(sub_8217B158, SetSamplerStateHook<0>);  // culdcept-nx: Katamari sub_824DDC30; sampler state table row 0 (setter)
BANJO_RENUT_HOOK(sub_8217B1A8, SetSamplerStateHook<1>);  // culdcept-nx: Katamari sub_824DDC80; sampler state table row 1 (setter)
BANJO_RENUT_HOOK(sub_8217B1F8, SetSamplerStateHook<2>);  // culdcept-nx: Katamari sub_824DDCD0; sampler state table row 2 (setter)
BANJO_RENUT_HOOK(sub_8217B0E8, SetSamplerStateHook<3>);  // culdcept-nx: Katamari sub_824DDBC0; sampler state table row 3 (setter)
BANJO_RENUT_HOOK(sub_8217ABB0, SetSamplerStateHook<4>);  // culdcept-nx: Katamari sub_824DD680; sampler state table row 4 (setter)
BANJO_RENUT_HOOK(sub_8217AA28, SetSamplerStateHook<5>);  // culdcept-nx: Katamari sub_824DD4F0; sampler state table row 5 (setter)
BANJO_RENUT_HOOK(sub_8217AD38, SetSamplerStateHook<6>);  // culdcept-nx: Katamari sub_824DD810; sampler state table row 6 (setter)
BANJO_RENUT_HOOK(sub_8217AF48, SetSamplerStateHook<7>);  // culdcept-nx: Katamari sub_824DDA20; sampler state table row 7 (setter)
BANJO_RENUT_HOOK(sub_8217AFE8, SetSamplerStateHook<8>);  // culdcept-nx: Katamari sub_824DDAC0; sampler state table row 8 (setter)
BANJO_RENUT_HOOK(sub_8217AE28, SetSamplerStateHook<9>);  // culdcept-nx: Katamari sub_824DD900; sampler state table row 9 (setter)
BANJO_RENUT_HOOK(sub_8217ACA0, SetSamplerStateHook<10>);  // culdcept-nx: Katamari sub_824DD778; sampler state table row 10 (setter)
BANJO_RENUT_HOOK(sub_8217AB18, SetSamplerStateHook<11>);  // culdcept-nx: Katamari sub_824DD5E8; sampler state table row 11 (setter)
BANJO_RENUT_HOOK(sub_8217AD90, SetSamplerStateHook<12>);  // culdcept-nx: Katamari sub_824DD868; sampler state table row 12 (setter)
BANJO_RENUT_HOOK(sub_8217B068, SetSamplerStateHook<13>);  // culdcept-nx: Katamari sub_824DDB40; sampler state table row 13 (setter)
BANJO_RENUT_HOOK(sub_8217B248, SetSamplerStateHook<14>);  // culdcept-nx: Katamari sub_824DDD20; sampler state table row 14 (setter)
BANJO_RENUT_HOOK(sub_8217AEA0, SetSamplerStateHook<15>);  // culdcept-nx: Katamari sub_824DD978; sampler state table row 15 (setter)
BANJO_RENUT_HOOK(sub_8217B2A0, SetSamplerStateHook<16>);  // culdcept-nx: Katamari sub_824DDD78; sampler state table row 16 (setter)
BANJO_RENUT_HOOK(sub_8217B2F8, SetSamplerStateHook<17>);  // culdcept-nx: Katamari sub_824DDDD0; sampler state table row 17 (setter)
BANJO_RENUT_HOOK(sub_8217B350, SetSamplerStateHook<18>);  // culdcept-nx: Katamari sub_824DDE28; sampler state table row 18 (setter)
BANJO_RENUT_HOOK(sub_8217B3A8, SetSamplerStateHook<19>);  // culdcept-nx: Katamari sub_824DDE80; sampler state table row 19 (setter)

BANJO_RENUT_HOOK(sub_82171DA8, D3DDevice_SetTexture_hook);  // culdcept-nx: Katamari sub_824D2640; Viva 8245C900
BANJO_RENUT_UNHOOKED_HOOK(unlinked_824A3D40, D3DDevice_SetTexture_hook);  // culdcept-nx: Katamari sub_824A3D40; not placed in this game: Viva 8268E5A8 not paired
BANJO_RENUT_HOOK(sub_8217B6F8, D3DDevice_SetStreamSource_hook);  // culdcept-nx: Katamari sub_824DDFE0; Viva 8245A320
BANJO_RENUT_HOOK(sub_8217B8B0, D3DDevice_SetIndices_hook);  // culdcept-nx: Katamari sub_824DE188; Viva 8245A4D8
BANJO_RENUT_HOOK(sub_8217CFE8, D3DDevice_SetVertexDeclaration_hook);  // culdcept-nx: Katamari sub_824D4B48; Viva 8245E0B8
BANJO_RENUT_HOOK(sub_8217B4D0, D3DDevice_SetViewport_hook);  // culdcept-nx: Katamari sub_824DE730; Viva 8245A0F8
BANJO_RENUT_UNHOOKED_HOOK(unlinked_824DE4B8, D3D_SetViewport_hook);  // culdcept-nx: Katamari sub_824DE4B8; not placed in this game: no Viva line
BANJO_RENUT_HOOK(sub_8217B408, D3DDevice_SetScissorRect_hook);  // culdcept-nx: Katamari sub_824DDEE0; Viva 8245A030
BANJO_RENUT_HOOK(sub_82178EF0, D3D_SetSurfaceClip_hook);  // culdcept-nx: Katamari sub_824DB8C8; Viva 82457B18
BANJO_RENUT_HOOK(sub_8217B948, D3DDevice_SetRenderTarget_hook);  // culdcept-nx: Katamari sub_824DE868; Viva 8245A568
BANJO_RENUT_HOOK(sub_8217BC78, D3DDevice_SetDepthStencilSurface_hook);  // culdcept-nx: Katamari sub_824DEBD0; Viva 8245A898
BANJO_RENUT_HOOK(sub_82178E58, D3D_SetSurfaceInfo_hook);  // culdcept-nx: Katamari sub_824DE7D0; Viva 82457A80
BANJO_RENUT_HOOK(sub_82184360, D3DDevice_SetPredication_hook);  // culdcept-nx: Katamari sub_824EFA58; Viva 8246E520
BANJO_RENUT_HOOK_SHARED(sub_8218CB98, D3DDevice_SetCommandBufferPredication_hook);  // culdcept-nx: N&B sub_826411C0; Viva 82468268 by signature 0.87 (next 0.38 sub_82174F10)
BANJO_RENUT_HOOK(sub_8217D120, D3DDevice_SetShaderGPRAllocation_hook);  // culdcept-nx: Katamari sub_824D4CD8; Viva 8245E1B8
BANJO_RENUT_UNHOOKED_HOOK(unlinked_824D3FE0, D3D_CleanupGprAllocationSetup_hook);  // culdcept-nx: Katamari sub_824D3FE0; not placed in this game: no Viva line
BANJO_RENUT_HOOK(sub_82184720, D3DDevice_SetScreenExtentQueryMode_hook);  // culdcept-nx: Katamari sub_824E67C8; Viva 82465948
BANJO_RENUT_HOOK(sub_821731A8, D3DDevice_UnsetAll_hook);  // culdcept-nx: Katamari sub_824D0488; Viva 82457838
BANJO_RENUT_HOOK(sub_82178D60, D3D_ResetAllState_hook);  // culdcept-nx: Katamari sub_824DE3C0; Viva 82457958
BANJO_RENUT_UNHOOKED_HOOK_SHARED(unlinked_82634B90, D3D_InvalidateConstants_hook);  // culdcept-nx: N&B sub_82634B90; not placed in this game: no Viva line
BANJO_RENUT_HOOK(sub_82174670, D3DDevice_BlockUntilIdle_hook);  // culdcept-nx: Katamari sub_824E0518; Viva 824647A0
// Through a thunk (j_D3DDevice_SetRenderTarget 0x82636EC8).
BANJO_RENUT_UNHOOKED_HOOK(unlinked_82636EC8, D3DDevice_SetRenderTarget_hook);  // culdcept-nx: N&B sub_82636EC8; not placed in this game: Viva 8245A568
BANJO_RENUT_UNHOOKED_HOOK(unlinked_82636ED0, D3DDevice_GetRenderTarget_hook);  // culdcept-nx: N&B sub_82636ED0; not placed in this game: no Viva line
BANJO_RENUT_UNHOOKED_HOOK(unlinked_82636F10, D3DDevice_GetDepthStencilSurface_hook);  // culdcept-nx: N&B sub_82636F10; not placed in this game: no Viva line
BANJO_RENUT_HOOK(sub_8217B820, D3DDevice_GetStreamSource_hook);  // culdcept-nx: N&B sub_82636E40; Viva 8245A448
BANJO_RENUT_UNHOOKED_HOOK(unlinked_82244B18, D3DDevice_GetViewport_hook);  // culdcept-nx: N&B sub_82244B18; not placed in this game: no Viva line
BANJO_RENUT_UNHOOKED_HOOK(unlinked_826370E8, D3DDevice_SetClipPlane_hook);  // culdcept-nx: N&B sub_826370E8; not placed in this game: Viva 8245AC00 not paired
BANJO_RENUT_UNHOOKED_HOOK(unlinked_82637138, D3DDevice_SetVertexFetchConstant_hook);  // culdcept-nx: N&B sub_82637138; not placed in this game: no Viva line
BANJO_RENUT_UNHOOKED_HOOK(unlinked_824D4470, D3DDevice_SetVertexShaderConstantB_hook);  // culdcept-nx: Katamari sub_824D4470; not placed in this game: no Viva line
BANJO_RENUT_HOOK(sub_8217C790, D3DDevice_SetPixelShaderConstantB_hook);  // culdcept-nx: Katamari sub_824D44D0; Viva 8245D988 by signature 0.79 (next 0.71 sub_8217C738)
BANJO_RENUT_UNHOOKED_HOOK(unlinked_826374B0, D3DDevice_SetVertexShaderConstantI_hook);  // culdcept-nx: N&B sub_826374B0; not placed in this game: no Viva line
BANJO_RENUT_UNHOOKED_HOOK(unlinked_82637500, D3DDevice_SetPixelShaderConstantI_hook);  // culdcept-nx: N&B sub_82637500; not placed in this game: no Viva line
BANJO_RENUT_UNHOOKED_HOOK(unlinked_8263ED90, D3DDevice_EndIndexedVertices_hook);  // culdcept-nx: N&B sub_8263ED90; not placed in this game: no Viva line

// Render state getters, by their index in N&B's table (off_82E506F0, field 0).
BANJO_RENUT_HOOK(sub_82179888, GetRenderStateHook<10>);  // culdcept-nx: Katamari sub_824DC330; render state table row 10 (getter)
BANJO_RENUT_HOOK(sub_821798F0, GetRenderStateHook<11>);  // culdcept-nx: Katamari sub_824DC390; render state table row 11 (getter)
BANJO_RENUT_HOOK(sub_821798B0, GetRenderStateHook<12>);  // culdcept-nx: Katamari sub_824DC358; render state table row 12 (getter)
BANJO_RENUT_HOOK(sub_82179220, GetRenderStateHook<13>);  // culdcept-nx: Katamari sub_824DBBF8; render state table row 13 (getter)
BANJO_RENUT_HOOK(sub_821791F0, GetRenderStateHook<14>);  // culdcept-nx: Katamari sub_824DBBC8; render state table row 14 (getter)
BANJO_RENUT_HOOK(sub_821792F0, GetRenderStateHook<15>);  // culdcept-nx: Katamari sub_824DBCC0; render state table row 15 (getter)
BANJO_RENUT_HOOK(sub_82179688, GetRenderStateHook<16>);  // culdcept-nx: Katamari sub_824DC058; render state table row 16 (getter)
BANJO_RENUT_HOOK(sub_821797C8, GetRenderStateHook<17>);  // culdcept-nx: Katamari sub_824DC1A0; render state table row 17 (getter)
BANJO_RENUT_HOOK(sub_82179410, GetRenderStateHook<18>);  // culdcept-nx: Katamari sub_824DBDE0; render state table row 18 (getter)
BANJO_RENUT_HOOK(sub_821794A0, GetRenderStateHook<19>);  // culdcept-nx: Katamari sub_824DBE70; render state table row 19 (getter)
BANJO_RENUT_HOOK(sub_82179380, GetRenderStateHook<20>);  // culdcept-nx: Katamari sub_824DBD50; render state table row 20 (getter)
BANJO_RENUT_HOOK(sub_82179580, GetRenderStateHook<21>);  // culdcept-nx: Katamari sub_824DBF50; render state table row 21 (getter)
BANJO_RENUT_HOOK(sub_821795F0, GetRenderStateHook<22>);  // culdcept-nx: Katamari sub_824DBFC0; render state table row 22 (getter)
BANJO_RENUT_HOOK(sub_82179510, GetRenderStateHook<23>);  // culdcept-nx: Katamari sub_824DBEE0; render state table row 23 (getter)
BANJO_RENUT_HOOK(sub_82179260, GetRenderStateHook<24>);  // culdcept-nx: Katamari sub_824DBC30; render state table row 24 (getter)
BANJO_RENUT_HOOK(sub_821796D0, GetRenderStateHook<25>);  // culdcept-nx: Katamari sub_824DC0A0; render state table row 25 (getter)
BANJO_RENUT_HOOK(sub_82179720, GetRenderStateHook<26>);  // culdcept-nx: Katamari sub_824DC0F0; render state table row 26 (getter)
BANJO_RENUT_HOOK(sub_82179940, GetRenderStateHook<27>);  // culdcept-nx: Katamari sub_824DC3D8; render state table row 27 (getter)
BANJO_RENUT_HOOK(sub_82179978, GetRenderStateHook<28>);  // culdcept-nx: Katamari sub_824DC408; render state table row 28 (getter)
BANJO_RENUT_HOOK(sub_821799E8, GetRenderStateHook<29>);  // culdcept-nx: Katamari sub_824DC470; render state table row 29 (getter)
BANJO_RENUT_HOOK(sub_82179A28, GetRenderStateHook<30>);  // culdcept-nx: Katamari sub_824DC4A8; render state table row 30 (getter)
BANJO_RENUT_HOOK(sub_82179A58, GetRenderStateHook<31>);  // culdcept-nx: Katamari sub_824DC4D8; render state table row 31 (getter)
BANJO_RENUT_HOOK(sub_821799A8, GetRenderStateHook<32>);  // culdcept-nx: Katamari sub_824DC438; render state table row 32 (getter)
BANJO_RENUT_HOOK(sub_82179B60, GetRenderStateHook<33>);  // culdcept-nx: Katamari sub_824DC5D0; render state table row 33 (getter)
BANJO_RENUT_HOOK(sub_82179B80, GetRenderStateHook<34>);  // culdcept-nx: Katamari sub_824DC5F0; render state table row 34 (getter)
BANJO_RENUT_HOOK(sub_82179BA0, GetRenderStateHook<35>);  // culdcept-nx: Katamari sub_824DC610; render state table row 35 (getter)
BANJO_RENUT_HOOK(sub_82179AC8, GetRenderStateHook<36>);  // culdcept-nx: Katamari sub_824DC540; render state table row 36 (getter)
BANJO_RENUT_HOOK(sub_82179B08, GetRenderStateHook<37>);  // culdcept-nx: Katamari sub_824DC578; render state table row 37 (getter)
BANJO_RENUT_HOOK(sub_82179B38, GetRenderStateHook<38>);  // culdcept-nx: Katamari sub_824DC5A8; render state table row 38 (getter)
BANJO_RENUT_HOOK(sub_82179A88, GetRenderStateHook<39>);  // culdcept-nx: Katamari sub_824DC508; render state table row 39 (getter)
BANJO_RENUT_HOOK(sub_82179BC0, GetRenderStateHook<40>);  // culdcept-nx: Katamari sub_824DC630; render state table row 40 (getter)
BANJO_RENUT_HOOK(sub_82179BE0, GetRenderStateHook<41>);  // culdcept-nx: Katamari sub_824DC650; render state table row 41 (getter)
BANJO_RENUT_HOOK(sub_82179C00, GetRenderStateHook<42>);  // culdcept-nx: Katamari sub_824DC670; render state table row 42 (getter)
BANJO_RENUT_HOOK(sub_82179C48, GetRenderStateHook<43>);  // culdcept-nx: Katamari sub_824DC6B8; render state table row 43 (getter)
BANJO_RENUT_HOOK(sub_82179F70, GetRenderStateHook<44>);  // culdcept-nx: Katamari sub_824DC9F0; render state table row 44 (getter)
BANJO_RENUT_HOOK(sub_82179FC8, GetRenderStateHook<45>);  // culdcept-nx: Katamari sub_824DCA48; render state table row 45 (getter)
BANJO_RENUT_HOOK(sub_82179F18, GetRenderStateHook<46>);  // culdcept-nx: Katamari sub_824DC998; render state table row 46 (getter)
BANJO_RENUT_HOOK(sub_8217A020, GetRenderStateHook<47>);  // culdcept-nx: Katamari sub_824DCAA0; render state table row 47 (getter)
BANJO_RENUT_HOOK(sub_82179E00, GetRenderStateHook<48>);  // culdcept-nx: Katamari sub_824DC860; render state table row 48 (getter)
BANJO_RENUT_HOOK(sub_82179E28, GetRenderStateHook<49>);  // culdcept-nx: Katamari sub_824DC888; render state table row 49 (getter)
BANJO_RENUT_HOOK(sub_82179C68, GetRenderStateHook<50>);  // culdcept-nx: Katamari sub_824DC6C8; render state table row 50 (getter)
BANJO_RENUT_HOOK(sub_82179D18, GetRenderStateHook<51>);  // culdcept-nx: Katamari sub_824DC778; render state table row 51 (getter)
BANJO_RENUT_HOOK(sub_82179DD0, GetRenderStateHook<52>);  // culdcept-nx: Katamari sub_824DC830; render state table row 52 (getter)
BANJO_RENUT_HOOK(sub_82179E60, GetRenderStateHook<53>);  // culdcept-nx: Katamari sub_824DC8C8; render state table row 53 (getter)
BANJO_RENUT_HOOK(sub_82179E98, GetRenderStateHook<54>);  // culdcept-nx: Katamari sub_824DC908; render state table row 54 (getter)
BANJO_RENUT_HOOK(sub_82179ED0, GetRenderStateHook<55>);  // culdcept-nx: Katamari sub_824DC948; render state table row 55 (getter)
BANJO_RENUT_HOOK(sub_82179F08, GetRenderStateHook<56>);  // culdcept-nx: Katamari sub_824DC988; render state table row 56 (getter)
BANJO_RENUT_HOOK(sub_8217A758, GetRenderStateHook<57>);  // culdcept-nx: Katamari sub_824DD1B8; render state table row 57 (getter)
BANJO_RENUT_HOOK(sub_8217A6E8, GetRenderStateHook<58>);  // culdcept-nx: Katamari sub_824DD148; render state table row 58 (getter)
BANJO_RENUT_HOOK(sub_8217A720, GetRenderStateHook<59>);  // culdcept-nx: Katamari sub_824DD180; render state table row 59 (getter)
BANJO_RENUT_HOOK(sub_8217A310, GetRenderStateHook<60>);  // culdcept-nx: Katamari sub_824DCD10; render state table row 60 (getter)
BANJO_RENUT_HOOK(sub_8217A320, GetRenderStateHook<61>);  // culdcept-nx: Katamari sub_824DCD20; render state table row 61 (getter)
BANJO_RENUT_HOOK(sub_8217A330, GetRenderStateHook<62>);  // culdcept-nx: Katamari sub_824DCD30; render state table row 62 (getter)
BANJO_RENUT_HOOK(sub_8217A340, GetRenderStateHook<63>);  // culdcept-nx: Katamari sub_824DCD40; render state table row 63 (getter)
BANJO_RENUT_HOOK(sub_8217A350, GetRenderStateHook<64>);  // culdcept-nx: Katamari sub_824DCD50; render state table row 64 (getter)
BANJO_RENUT_HOOK(sub_8217A360, GetRenderStateHook<65>);  // culdcept-nx: Katamari sub_824DCD60; render state table row 65 (getter)
BANJO_RENUT_HOOK(sub_8217A370, GetRenderStateHook<66>);  // culdcept-nx: Katamari sub_824DCD70; render state table row 66 (getter)
BANJO_RENUT_HOOK(sub_8217A380, GetRenderStateHook<67>);  // culdcept-nx: Katamari sub_824DCD80; render state table row 67 (getter)
BANJO_RENUT_HOOK(sub_8217A390, GetRenderStateHook<68>);  // culdcept-nx: Katamari sub_824DCD90; render state table row 68 (getter)
BANJO_RENUT_HOOK(sub_8217A3A0, GetRenderStateHook<69>);  // culdcept-nx: Katamari sub_824DCDA0; render state table row 69 (getter)
BANJO_RENUT_UNHOOKED_HOOK(unlinked_824DCDB0, GetRenderStateHook<70>);  // culdcept-nx: Katamari sub_824DCDB0; not placed in this game: hand list
BANJO_RENUT_UNHOOKED_HOOK(unlinked_824DCDC0, GetRenderStateHook<71>);  // culdcept-nx: Katamari sub_824DCDC0; not placed in this game: hand list
BANJO_RENUT_UNHOOKED_HOOK(unlinked_824DCDD0, GetRenderStateHook<72>);  // culdcept-nx: Katamari sub_824DCDD0; not placed in this game: hand list
BANJO_RENUT_UNHOOKED_HOOK(unlinked_824DCDE0, GetRenderStateHook<73>);  // culdcept-nx: Katamari sub_824DCDE0; not placed in this game: hand list
BANJO_RENUT_UNHOOKED_HOOK(unlinked_824DCDF0, GetRenderStateHook<74>);  // culdcept-nx: Katamari sub_824DCDF0; not placed in this game: hand list
BANJO_RENUT_UNHOOKED_HOOK(unlinked_824DCE00, GetRenderStateHook<75>);  // culdcept-nx: Katamari sub_824DCE00; not placed in this game: hand list
BANJO_RENUT_HOOK(sub_8217A3F0, GetRenderStateHook<76>);  // culdcept-nx: Katamari sub_824DCE50; render state table row 76 (getter)
BANJO_RENUT_HOOK(sub_8217A4A8, GetRenderStateHook<77>);  // culdcept-nx: Katamari sub_824DCF08; render state table row 77 (getter)
BANJO_RENUT_HOOK(sub_8217A558, GetRenderStateHook<78>);  // culdcept-nx: Katamari sub_824DCFB8; render state table row 78 (getter)
BANJO_RENUT_HOOK(sub_8217A608, GetRenderStateHook<79>);  // culdcept-nx: Katamari sub_824DD068; render state table row 79 (getter)
BANJO_RENUT_HOOK(sub_8217A6B8, GetRenderStateHook<80>);  // culdcept-nx: Katamari sub_824DD118; render state table row 80 (getter)
BANJO_RENUT_HOOK(sub_8217A790, GetRenderStateHook<81>);  // culdcept-nx: Katamari sub_824DD1F0; render state table row 81 (getter)
BANJO_RENUT_HOOK(sub_8217A7C0, GetRenderStateHook<82>);  // culdcept-nx: Katamari sub_824DD220; render state table row 82 (getter)
BANJO_RENUT_HOOK(sub_8217A7E8, GetRenderStateHook<83>);  // culdcept-nx: Katamari sub_824DD250; render state table row 83 (getter)
BANJO_RENUT_HOOK(sub_8217A810, GetRenderStateHook<84>);  // culdcept-nx: Katamari sub_824DD278; render state table row 84 (getter)
BANJO_RENUT_HOOK(sub_8217A840, GetRenderStateHook<85>);  // culdcept-nx: Katamari sub_824DD2A8; render state table row 85 (getter)
BANJO_RENUT_HOOK(sub_8217A880, GetRenderStateHook<86>);  // culdcept-nx: Katamari sub_824DD2E8; render state table row 86 (getter)
BANJO_RENUT_HOOK(sub_8217A8B0, GetRenderStateHook<87>);  // culdcept-nx: Katamari sub_824DD318; render state table row 87 (getter)
BANJO_RENUT_HOOK(sub_8217A8D8, GetRenderStateHook<88>);  // culdcept-nx: Katamari sub_824DD340; render state table row 88 (getter)
BANJO_RENUT_HOOK(sub_8217A908, GetRenderStateHook<89>);  // culdcept-nx: Katamari sub_824DD370; render state table row 89 (getter)
BANJO_RENUT_HOOK(sub_8217A968, GetRenderStateHook<90>);  // culdcept-nx: Katamari sub_824DD430; render state table row 90 (getter)
BANJO_RENUT_HOOK(sub_8217A998, GetRenderStateHook<91>);  // culdcept-nx: Katamari sub_824DD460; render state table row 91 (getter)
BANJO_RENUT_HOOK(sub_8217A9C8, GetRenderStateHook<92>);  // culdcept-nx: Katamari sub_824DD490; render state table row 92 (getter)
BANJO_RENUT_HOOK(sub_8217A9F0, GetRenderStateHook<93>);  // culdcept-nx: Katamari sub_824DD4B8; render state table row 93 (getter)
BANJO_RENUT_HOOK(sub_8217AA00, GetRenderStateHook<94>);  // culdcept-nx: Katamari sub_824DD4C8; render state table row 94 (getter)
BANJO_RENUT_HOOK(sub_8217AA18, GetRenderStateHook<95>);  // culdcept-nx: Katamari sub_824DD4E0; render state table row 95 (getter)
BANJO_RENUT_HOOK(sub_8217A938, GetRenderStateHook<96>);  // culdcept-nx: Katamari sub_824DD398; render state table row 96 (getter)
BANJO_RENUT_UNHOOKED_HOOK(unlinked_824DD400, GetRenderStateHook<97>);  // culdcept-nx: Katamari sub_824DD400; not placed in this game: render state 97: not in this XDK's table
BANJO_RENUT_UNHOOKED_HOOK(unlinked_824DC248, GetRenderStateHook<98>);  // culdcept-nx: Katamari sub_824DC248; not placed in this game: render state 98: not in this XDK's table
BANJO_RENUT_UNHOOKED_HOOK(unlinked_824DC298, GetRenderStateHook<99>);  // culdcept-nx: Katamari sub_824DC298; not placed in this game: render state 99: not in this XDK's table
BANJO_RENUT_UNHOOKED_HOOK(unlinked_824DC2E8, GetRenderStateHook<100>);  // culdcept-nx: Katamari sub_824DC2E8; not placed in this game: render state 100: not in this XDK's table

// Sampler state getters (off_82E50BD0, field 0).
BANJO_RENUT_HOOK(sub_8217B190, GetSamplerStateHook<0>);  // culdcept-nx: Katamari sub_824DDC68; sampler state table row 0 (getter)
BANJO_RENUT_HOOK(sub_8217B1E0, GetSamplerStateHook<1>);  // culdcept-nx: Katamari sub_824DDCB8; sampler state table row 1 (getter)
BANJO_RENUT_HOOK(sub_8217B230, GetSamplerStateHook<2>);  // culdcept-nx: Katamari sub_824DDD08; sampler state table row 2 (getter)
BANJO_RENUT_HOOK(sub_8217B138, GetSamplerStateHook<3>);  // culdcept-nx: Katamari sub_824DDC10; sampler state table row 3 (getter)
BANJO_RENUT_HOOK(sub_8217AC70, GetSamplerStateHook<4>);  // culdcept-nx: Katamari sub_824DD740; sampler state table row 4 (getter)
BANJO_RENUT_HOOK(sub_8217AAE8, GetSamplerStateHook<5>);  // culdcept-nx: Katamari sub_824DD5B0; sampler state table row 5 (getter)
BANJO_RENUT_HOOK(sub_8217AD78, GetSamplerStateHook<6>);  // culdcept-nx: Katamari sub_824DD850; sampler state table row 6 (getter)
BANJO_RENUT_HOOK(sub_8217AFA8, GetSamplerStateHook<7>);  // culdcept-nx: Katamari sub_824DDA80; sampler state table row 7 (getter)
BANJO_RENUT_HOOK(sub_8217B058, GetSamplerStateHook<8>);  // culdcept-nx: Katamari sub_824DDB30; sampler state table row 8 (getter)
BANJO_RENUT_HOOK(sub_8217AE90, GetSamplerStateHook<9>);  // culdcept-nx: Katamari sub_824DD968; sampler state table row 9 (getter)
BANJO_RENUT_HOOK(sub_8217AD28, GetSamplerStateHook<10>);  // culdcept-nx: Katamari sub_824DD800; sampler state table row 10 (getter)
BANJO_RENUT_HOOK(sub_8217ABA0, GetSamplerStateHook<11>);  // culdcept-nx: Katamari sub_824DD670; sampler state table row 11 (getter)
BANJO_RENUT_HOOK(sub_8217AE18, GetSamplerStateHook<12>);  // culdcept-nx: Katamari sub_824DD8F0; sampler state table row 12 (getter)
BANJO_RENUT_HOOK(sub_8217B0D8, GetSamplerStateHook<13>);  // culdcept-nx: Katamari sub_824DDBB0; sampler state table row 13 (getter)
BANJO_RENUT_HOOK(sub_8217B288, GetSamplerStateHook<14>);  // culdcept-nx: Katamari sub_824DDD60; sampler state table row 14 (getter)
BANJO_RENUT_HOOK(sub_8217AF00, GetSamplerStateHook<15>);  // culdcept-nx: Katamari sub_824DD9D8; sampler state table row 15 (getter)
BANJO_RENUT_HOOK(sub_8217B2E0, GetSamplerStateHook<16>);  // culdcept-nx: Katamari sub_824DDDB8; sampler state table row 16 (getter)
BANJO_RENUT_HOOK(sub_8217B338, GetSamplerStateHook<17>);  // culdcept-nx: Katamari sub_824DDE10; sampler state table row 17 (getter)
BANJO_RENUT_HOOK(sub_8217B390, GetSamplerStateHook<18>);  // culdcept-nx: Katamari sub_824DDE68; sampler state table row 18 (getter)
BANJO_RENUT_HOOK(sub_8217B3F0, GetSamplerStateHook<19>);  // culdcept-nx: Katamari sub_824DDEC8; sampler state table row 19 (getter)

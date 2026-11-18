#!/usr/bin/env python3
"""vivapinata-nx (2026-11-02, from pacman-nx's renut_port_pacman.py): the edits that move Renut's Direct3D layer to Viva PiÃ±ata (XDK 2.0.3529, D3D9LTCG),
after tools/renut_hook_remap_culdcept.py (hooks) and tools/renut_device_offsets_culdcept.py (the literals katamari-nx annotated). docs/culdcept-notes.md,
"Renut's Direct3D at this game's addresses", has the evidence.

Three kinds of edit:
  EDITS     exact (file, old, new) text pairs; a pair whose new text is already there is skipped (idempotent), one whose old text is missing is reported.
  LITERALS  in the listed files, on lines not yet marked, literals in Katamari's device range that katamari-nx did not annotate because N&B's and
            Katamari's layouts agree there: moved by tools/renut_device_offsets_culdcept.py's zones and marked.

  python -I tools/renut_port_viva.py [--check]
"""
import argparse
import os
import re
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from renut_device_offsets_culdcept import culdcept as viva  # noqa: E402

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
GPU = os.path.join(ROOT, 'app', 'src', 'renut_engine', 'gpu')

EDITS = [
    # 1. Guest data addresses (tools/d3d_data_addrs_viva.py, tools/renut_state_tables.py).
    ('d3d/d3d_native_state.cpp', 'kRenderStateTable = 0x82AD8E48;', 'kRenderStateTable = 0x828B7500;  // culdcept-nx: tools/renut_state_tables.py, 97 rows paired by default;'),
    ('d3d/d3d_native_state.cpp', 'kSamplerStateTable = 0x82AD9308;', 'kSamplerStateTable = 0x828B7990;  // culdcept-nx: tools/renut_state_tables.py, 20 rows;'),
    ('d3d/d3d_native_state.cpp', 'kFormatTable = 0x820FA9D0;', 'kFormatTable = 0x8214F720;  // culdcept-nx: Katamari\'s and Viva\'s 128 bytes, word for word (tools/d3d_data_addrs_culdcept.py);'),
    ('d3d/hooks/resource.cpp', 'kFormatTable = 0x820FA9D0;', 'kFormatTable = 0x8214F720;  // culdcept-nx: Katamari\'s and Viva\'s 128 bytes, word for word (tools/d3d_data_addrs_culdcept.py);'),
    ('d3d/hooks/device.cpp', 'kGlobalQwordVa = 0x82AD93F8;', 'kGlobalQwordVa = 0x828B7D40;  // culdcept-nx: the build qword 2.0.3424.0, read by the device init sub_82182DE0;'),
    ('d3d/hooks/device.cpp', 'kVdGlobalDeviceImportSlot = 0x82000750;', 'kVdGlobalDeviceImportSlot = 0x820007B0;  // culdcept-nx: D3D_LockResource sub_82172120, its first two references;'),
    ('d3d/hooks/device.cpp', 'kDeviceCapsVa = 0x820FC1F8;', 'kDeviceCapsVa = 0;  // culdcept-nx: as Viva Pinata, no GetDeviceCaps and no copy of the table;'),
    ('d3d/hooks/resource.cpp', 'kGameNoLargePagesVa = 0x8315DFE0;', 'kGameNoLargePagesVa = 0x83E323BC;  // culdcept-nx: XPhysicalAlloc sub_823582D0, its first reference;'),
    ('d3d/hooks/resource.cpp', 'kHiZOwnerVa = 0x8315E830;', 'kHiZOwnerVa = 0x83F40478;  // culdcept-nx: CreateSurface sub_82171AE0, Destroy sub_82171F20;'),
    # This game overrides XMemAlloc / XMemFree: Direct3D's device goes through sub_82357BC8 / sub_82357C60 (`lis r4,0x2480; lwz r3,-4(r31); bl`).
    ('d3d/hooks/resource.cpp', 'REX_IMPORT(__imp__sub_824CAB98, GameXMemFree', 'REX_IMPORT(__imp__sub_82357C60, GameXMemFree'),
    ('d3d/hooks/device.cpp', 'REX_IMPORT(__imp__sub_824C1C10, GuestGetCurrentThreadId', 'REX_IMPORT(__imp__sub_8239EA68, GuestGetCurrentThreadId'),
    # 2. The state tables: XDK 2.0.3529's render table has 97 rows; the device's tables follow it. N&B's and Katamari's last four rows (HiZWriteEnable,
    # LastPixel, LineWidth, Buffer2Frames) are not in this XDK: their fields keep what the device init leaves, HiZ's default as N&B's table gives it.
    ('d3d/d3d_native_state.cpp', '''  for (u32 i = 0; i < kRenderStates; ++i) {
    device.SetW(off::SetRenderStateCall + i * 4, TableWord(kRenderStateTable, i, 1));
    device.SetW(off::GetRenderStateCall + i * 4, TableWord(kRenderStateTable, i, 0));
    kRenderStateFns[i](device, TableWord(kRenderStateTable, i, 2));
  }''', '''  // culdcept-nx: sub_82182688 walks 97 rows (`cmplwi cr6,r30,388`), the sampler table right after them.
  for (u32 i = 0; i < kGuestRenderStates; ++i) {
    device.SetW(off::SetRenderStateCall + i * 4, TableWord(kRenderStateTable, i, 1));
    device.SetW(off::GetRenderStateCall + i * 4, TableWord(kRenderStateTable, i, 0));
    kRenderStateFns[i](device, TableWord(kRenderStateTable, i, 2));
  }
  RsHiZWriteEnable(device, 2);  // N&B's row 97'''),
    ('d3d/d3d_native_state.cpp', 'constexpr u32 kRenderStates = 101;', 'constexpr u32 kRenderStates = 101;\nconstexpr u32 kGuestRenderStates = 97;  // culdcept-nx: this XDK\'s table'),
    # No m_Predicated_PendingMask2 in this XDK: the ring pointers sit at +0x28.
    ('d3d/d3d_native_state.cpp', '''  u64 predicated = d.Q(off::PredicatedPending2);
  if (!(d.B(off::Flags3) & 0x20) && (flags & 0xE0000) == 0x40000)
    predicated &= ~u64(0x100);
  else
    predicated |= 0x100;
  d.SetQ(off::PredicatedPending2, predicated);
''', ''),
    # 3. The device's front (d3d_native.h off::, d3d.h Device): tools/ppc_dis.py on sub_82173000 (`stw r11,40(r31)` the ring, +44 the limit, +48 the guarantee),
    # sub_821730E8 (`lwz r10,52(r11)` the reference count) and sub_82182688: the render table's setters at +0x38 + 4 i, the sampler's at +0x1BC, the getters at +0x20C and +0x390; constants at +0x400.
    ('d3d/d3d_native.h', '''constexpr u32 Ring = 0x30;                 // m_pRing
constexpr u32 RingLimit = 0x34;            // m_pRingLimit
constexpr u32 RingGuarantee = 0x38;        // m_pRingGuarantee
constexpr u32 PredicatedPending2 = 0x28;   // m_Predicated_PendingMask2
constexpr u32 SetRenderStateCall = 0x40;   // [101]
constexpr u32 SetSamplerStateCall = 0x1D4; // [20]
constexpr u32 GetRenderStateCall = 0x224;  // [101]
constexpr u32 GetSamplerStateCall = 0x3B8; // [20]
constexpr u32 TextureFetch = 0x480;        // m_Constants.TextureFetch[26], 24 bytes each
constexpr u32 VertexFetch = 0x6F0;         // m_Constants.VertexFetch[18], 8 bytes each
constexpr u32 SurfaceInfo = 0x2880;        // m_DestinationPacket
constexpr u32 Color0Info = 0x2884;
constexpr u32 DepthInfo = 0x2888;
constexpr u32 WindowOffset = 0x28C0;       // m_WindowPacket
constexpr u32 WindowScissorTL = 0x28C4;
constexpr u32 WindowScissorBR = 0x28C8;
constexpr u32 ColorMask = 0x28DC;          // m_ValuesPacket
constexpr u32 DepthControl = 0x2934;       // m_ControlPacket
constexpr u32 BlendControl0 = 0x2938;
constexpr u32 ColorControl = 0x293C;
constexpr u32 HiControl = 0x2940;
constexpr u32 ClipControl = 0x2944;
constexpr u32 ModeControl = 0x2948;
constexpr u32 BlendControl1 = 0x2958;
constexpr u32 BlendControl2 = 0x295C;
constexpr u32 BlendControl3 = 0x2960;
constexpr u32 FenceCompletion = 0x2A90;    // -> the completed-fence word
constexpr u32 FenceTag = 0x2A9C;           // the current fence
constexpr u32 CommandBufferTagMask = 0x2AA0;  // Common bits a recorded buffer tracks
constexpr u32 TilePredication = 0x2AB4;    // SetPredication / SetCommandBufferPredication args
constexpr u32 RunPredication = 0x2AB8;
constexpr u32 Flags = 0x2ABC;              // bytes 0x2ABC-0x2ABF, see below
constexpr u32 Flags1 = 0x2ABD;
constexpr u32 Flags2 = 0x2ABE;
constexpr u32 Flags3 = 0x2ABF;''', '''// culdcept-nx: XDK 2.0.3529's layout (tools/renut_port_viva.py): no m_Predicated_PendingMask2, 97 render states, the constants at
// 0x400, the packets 128 bytes below Katamari's, the private words by the zones of tools/renut_device_offsets_culdcept.py.
constexpr u32 Ring = 0x28;                 // m_pRing
constexpr u32 RingLimit = 0x2C;            // m_pRingLimit
constexpr u32 RingGuarantee = 0x30;        // m_pRingGuarantee
constexpr u32 SetRenderStateCall = 0x38;   // [97]
constexpr u32 SetSamplerStateCall = 0x1BC; // [20]
constexpr u32 GetRenderStateCall = 0x20C;  // [97]
constexpr u32 GetSamplerStateCall = 0x390; // [20]
constexpr u32 TextureFetch = 0x400;        // m_Constants.TextureFetch[26], 24 bytes each
constexpr u32 VertexFetch = 0x670;         // m_Constants.VertexFetch[18], 8 bytes each
constexpr u32 SurfaceInfo = 0x2800;        // m_DestinationPacket
constexpr u32 Color0Info = 0x2804;
constexpr u32 DepthInfo = 0x2808;
constexpr u32 WindowOffset = 0x2840;       // m_WindowPacket
constexpr u32 WindowScissorTL = 0x2844;
constexpr u32 WindowScissorBR = 0x2848;
constexpr u32 ColorMask = 0x285C;          // m_ValuesPacket
constexpr u32 DepthControl = 0x28B4;       // m_ControlPacket
constexpr u32 BlendControl0 = 0x28B8;
constexpr u32 ColorControl = 0x28BC;
constexpr u32 HiControl = 0x28C0;
constexpr u32 ClipControl = 0x28C4;
constexpr u32 ModeControl = 0x28C8;
constexpr u32 BlendControl1 = 0x28D8;
constexpr u32 BlendControl2 = 0x28DC;
constexpr u32 BlendControl3 = 0x28E0;
constexpr u32 FenceCompletion = 0x2A10;    // -> the completed-fence word (sub_82174360 lwz.10768)
constexpr u32 FenceTag = 0x2A1C;           // the current fence
constexpr u32 CommandBufferTagMask = 0x2A20;  // Common bits a recorded buffer tracks
constexpr u32 TilePredication = 0x2A30;    // SetPredication / SetCommandBufferPredication args
constexpr u32 RunPredication = 0x2A34;
constexpr u32 Flags = 0x2A38;              // bytes 0x2A38-0x2A3B, see below (`lbz r9,10808(r31)`)
constexpr u32 Flags1 = 0x2A39;
constexpr u32 Flags2 = 0x2A3A;
constexpr u32 Flags3 = 0x2A3B;'''),
    ('d3d/d3d_native.h', 'constexpr u32 kTouchFetchBegin = 0x480, kTouchFetchEnd = 0x780, kTouchTailBegin = 0x2780;', 'constexpr u32 kTouchFetchBegin = 0x400, kTouchFetchEnd = 0x700, kTouchTailBegin = 0x2700;  // culdcept-nx: 128 below Katamari\'s'),
    ('d3d/d3d_native.h', 'TouchBits(0x2780, 0x300) | TouchBits(0x2DF4, 0x108);', 'TouchBits(0x2700, 0x300) | TouchBits(0x2CE4, 0x108);  // culdcept-nx: -128, -272;'),
    ('d3d/d3d.h', '''constexpr u32 kDeviceAllocSize = 0x5700;  // katamari-nx: 0x5E00->0x5700''', '''constexpr u32 kDeviceAllocSize = 0x5000;  // culdcept-nx: Direct3D_CreateDevice sub_82173000 clears 20480 of its 20608 bytes (katamari-nx's 0x5700)'''),
    ('d3d/d3d.h', '''  be_u64 m_Pending[5];                // +0x0000  D3DTAGCOLLECTION dirty masks
  be_u64 m_Predicated_PendingMask2;   // +0x0028
  be_u32 m_pRing;                     // +0x0030
  be_u32 m_pRingLimit;                // +0x0034
  be_u32 m_pRingGuarantee;            // +0x0038
  be_u32 m_ReferenceCount;            // +0x003C
  be_u32 m_SetRenderStateCall[101];   // +0x0040
  be_u32 m_SetSamplerStateCall[20];   // +0x01D4
  be_u32 m_GetRenderStateCall[101];   // +0x0224
  be_u32 m_GetSamplerStateCall[20];   // +0x03B8
  u8 pad_0408[0x480 - 0x408];         // +0x0408
  Constants m_Constants;              // +0x0480
  be_f32 m_ClipPlanes[6][4];          // +0x2820
  DestinationPacket m_DestinationPacket;  // +0x2880
  WindowPacket m_WindowPacket;        // +0x28C0
  ValuesPacket m_ValuesPacket;        // +0x28CC
  ProgramPacket m_ProgramPacket;      // +0x2920
  ControlPacket m_ControlPacket;      // +0x2934
  u8 m_TessellatorPacket[84];         // +0x2964
  u8 m_MiscPacket[152];               // +0x29B8
  u8 m_PointPacket[32];               // +0x2A50
  u8 m_MaxAnisotropy[26];             // +0x2A70
  u8 m_ZFilter[26];                   // +0x2A8A
  u8 pad_2AA4[0x2B00 - 0x2AA4];       // +0x2AA4
  u8 private_[kDeviceAllocSize - 0x2B00];  // +0x2B00  CDevice private state''', '''  // culdcept-nx: XDK 2.0.3529: see d3d_native.h off::. m_Predicated_PendingMask2 does not exist yet; 97 render states.
  be_u64 m_Pending[5];                // +0x0000  D3DTAGCOLLECTION dirty masks
  be_u32 m_pRing;                     // +0x0028
  be_u32 m_pRingLimit;                // +0x002C
  be_u32 m_pRingGuarantee;            // +0x0030
  be_u32 m_ReferenceCount;            // +0x0034
  be_u32 m_SetRenderStateCall[97];    // +0x0038
  be_u32 m_SetSamplerStateCall[20];   // +0x01BC
  be_u32 m_GetRenderStateCall[97];    // +0x020C
  be_u32 m_GetSamplerStateCall[20];   // +0x0390
  u8 pad_03E0[0x400 - 0x3E0];         // +0x03E0
  Constants m_Constants;              // +0x0400
  be_f32 m_ClipPlanes[6][4];          // +0x27A0
  DestinationPacket m_DestinationPacket;  // +0x2800
  WindowPacket m_WindowPacket;        // +0x2840
  ValuesPacket m_ValuesPacket;        // +0x284C
  ProgramPacket m_ProgramPacket;      // +0x28A0
  ControlPacket m_ControlPacket;      // +0x28B4
  u8 m_TessellatorPacket[84];         // +0x28E4
  u8 m_MiscPacket[152];               // +0x2938
  u8 m_PointPacket[32];               // +0x29D0
  u8 m_MaxAnisotropy[26];             // +0x29F0
  u8 m_ZFilter[26];                   // +0x2A0A
  u8 pad_2A24[0x2A80 - 0x2A24];       // +0x2A24
  u8 private_[kDeviceAllocSize - 0x2A80];  // +0x2A80  CDevice private state'''),
    ('d3d/d3d.h', '''static_assert(offsetof(Device, m_pRing) == 0x30);
static_assert(offsetof(Device, m_ReferenceCount) == 0x3C);
static_assert(offsetof(Device, m_SetRenderStateCall) == 0x40);
static_assert(offsetof(Device, m_SetSamplerStateCall) == 0x1D4);
static_assert(offsetof(Device, m_GetRenderStateCall) == 0x224);
static_assert(offsetof(Device, m_GetSamplerStateCall) == 0x3B8);
static_assert(offsetof(Device, m_Constants) == 0x480);
static_assert(offsetof(Device, m_ClipPlanes) == 0x2820);
static_assert(offsetof(Device, m_DestinationPacket) == 0x2880);
static_assert(offsetof(Device, m_WindowPacket) == 0x28C0);
static_assert(offsetof(Device, m_ValuesPacket) == 0x28CC);
static_assert(offsetof(Device, m_ProgramPacket) == 0x2920);
static_assert(offsetof(Device, m_ControlPacket) == 0x2934);
static_assert(offsetof(Device, m_TessellatorPacket) == 0x2964);
static_assert(offsetof(Device, m_MiscPacket) == 0x29B8);
static_assert(offsetof(Device, m_PointPacket) == 0x2A50);
static_assert(offsetof(Device, m_MaxAnisotropy) == 0x2A70);
static_assert(offsetof(Device, m_ZFilter) == 0x2A8A);''', '''static_assert(offsetof(Device, m_pRing) == 0x28);
static_assert(offsetof(Device, m_ReferenceCount) == 0x34);
static_assert(offsetof(Device, m_SetRenderStateCall) == 0x38);
static_assert(offsetof(Device, m_SetSamplerStateCall) == 0x1BC);
static_assert(offsetof(Device, m_GetRenderStateCall) == 0x20C);
static_assert(offsetof(Device, m_GetSamplerStateCall) == 0x390);
static_assert(offsetof(Device, m_Constants) == 0x400);
static_assert(offsetof(Device, m_ClipPlanes) == 0x27A0);
static_assert(offsetof(Device, m_DestinationPacket) == 0x2800);
static_assert(offsetof(Device, m_WindowPacket) == 0x2840);
static_assert(offsetof(Device, m_ValuesPacket) == 0x284C);
static_assert(offsetof(Device, m_ProgramPacket) == 0x28A0);
static_assert(offsetof(Device, m_ControlPacket) == 0x28B4);
static_assert(offsetof(Device, m_TessellatorPacket) == 0x28E4);
static_assert(offsetof(Device, m_MiscPacket) == 0x2938);
static_assert(offsetof(Device, m_PointPacket) == 0x29D0);
static_assert(offsetof(Device, m_MaxAnisotropy) == 0x29F0);
static_assert(offsetof(Device, m_ZFilter) == 0x2A0A);'''),
    # The vertex declaration moved 4 bytes more than its neighbours: SetVertexDeclaration sub_8217CFE8 `stw r4,11536(r3)`.
    ('d3d/d3d.h', 'kDeviceOffsetVertexDeclaration = 0x2E24;', 'kDeviceOffsetVertexDeclaration = 0x2D10;  // culdcept-nx: sub_8217CFE8 `stw r4,11536(r3)`;'),
    # The snapshot ends at the word after the predication so its tail is a whole number of words past a 16-byte step: 2456 bytes from the tail's start, the last chunk 24.
    ('d3d/d3d.h', 'kDeviceSnapshotSize = 0x31A0;', 'kDeviceSnapshotSize = 0x3098;  // culdcept-nx: 12440, the predication 12432 and the word after it;'),
    # Swap's own word: sub_8218DA68 `lwz r?,19980; stw r?,19980`, no field at Katamari's zone.
    ('d3d/d3d.h', 'kDeviceOffsetSwapCount = 0x4098;', 'kDeviceOffsetSwapCount = 0x4E0C;  // culdcept-nx: sub_8218DA68 lwz/stw 19980;'),
    # 4. The field map (render/draw.h) and the chain's layout.
    ('render/draw.h', '''  if (at < 0x480)
    return 0;
  if (at < 0x6F0)
    return kFieldFetch;
  if (at < 0x780)
    return kFieldVertexFetch;
  if (at < 0x2780)
    return 0;  // the float constants: by group (m_Pending)
  if (at < 0x2880)
    return kFieldBools;
  if (at < 0x28C0)
    return kFieldDest;
  if (at < 0x2968)
    return kFieldViewPackets;
  if (at >= 0x29C0 && at < 0x29C4)
    return kField29C0;
  if (at < 0x2A80)
    return kFieldMisc;''', '''  // culdcept-nx: every boundary 128 bytes below Katamari's (the constants at 0x400).
  if (at < 0x400)
    return 0;
  if (at < 0x670)
    return kFieldFetch;
  if (at < 0x700)
    return kFieldVertexFetch;
  if (at < 0x2700)
    return 0;  // the float constants: by group (m_Pending)
  if (at < 0x2800)
    return kFieldBools;
  if (at < 0x2840)
    return kFieldDest;
  if (at < 0x28E8)
    return kFieldViewPackets;
  if (at >= 0x2940 && at < 0x2944)
    return kField29C0;
  if (at < 0x2A00)
    return kFieldMisc;'''),
    ('render/draw.h', 'if (at < 0x2DF8)  // katamari-nx: 0x2E04->0x2DF8', 'if (at < 0x2CE8)  // katamari-nx: 0x2E04->0x2DF8 (culdcept-nx: -272 there)'),
    ('render/draw.h', 'if (at < 0x2EF8)  // katamari-nx: 0x2F08->0x2EF8', 'if (at < 0x2DF0)  // katamari-nx: 0x2F08->0x2EF8 (culdcept-nx: -264 there)'),
    ('render/draw.h', 'if (at < 0x317C)  // katamari-nx: 0x318C->0x317C', 'if (at < 0x3070)  // katamari-nx: 0x318C->0x317C (culdcept-nx: SetScissorRect `stw r11,12400(r3)`)'),
    ('render/draw.h', 'constexpr u32 kChainHeadChunks = 30;', 'constexpr u32 kChainHeadChunks = 28;  // culdcept-nx: the float constants at 0x700'),
    ('render/draw.h', 'constexpr u32 kChainTailBegin = 10112;', 'constexpr u32 kChainTailBegin = 9984;  // culdcept-nx: VertexShaderB at 0x2700'),
    ('render/draw.cpp', 'static_assert(FieldsOfRange(10112, 10600 - 10112) == (kFieldBools | kFieldDest | kFieldViewPackets))', 'static_assert(FieldsOfRange(9984, 10472 - 9984) == (kFieldBools | kFieldDest | kFieldViewPackets))'),
    ('render/draw.cpp', 'static_assert(FieldsOfRange(10432, 10600 - 10432) == kFieldViewPackets && FieldOfOffset(0x29C0) == k', 'static_assert(FieldsOfRange(10304, 10472 - 10304) == kFieldViewPackets && FieldOfOffset(0x2940) == k'),
    ('render/draw.cpp', 'key[11] = device.U32(0x29C0);', 'key[11] = device.U32(0x2940);  // culdcept-nx: 128 below Katamari\'s'),
    ('render/draw.cpp', '!(device.U32(0x29C0) & 1)', '!(device.U32(0x2940) & 1)'),
    ('render/draw.cpp', 'constexpr u32 kDeviceDeclarationOffset = 0x2F98;', 'constexpr u32 kDeviceDeclarationOffset = 0x2E90;  // culdcept-nx: sub_82182688 `stw r?,11920(r31)`'),
    ('render/draw.cpp', 'static_assert(kCensusTailBegin < 10880 && 11776 < d3d::kDeviceSnapshotSize);', 'static_assert(kCensusTailBegin < 10752 && 11520 < d3d::kDeviceSnapshotSize);'),
    ('render/draw.cpp', 'state = XXH3_64bits_withSeed(device_state + kCensusTailBegin, 10880 - kCensusTailBegin, state);', 'state = XXH3_64bits_withSeed(device_state + kCensusTailBegin, 10752 - kCensusTailBegin, state);'),
    ('render/draw.cpp', 'state = XXH3_64bits_withSeed(device_state + 11776, d3d::kDeviceSnapshotSize - 11776, state);', 'state = XXH3_64bits_withSeed(device_state + 11520, d3d::kDeviceSnapshotSize - 11520, state);'),
    ('render/draw.cpp', 'const float vx = device.F32(0x3160), vy = device.F32(0x3164);', 'const float vx = device.F32(0x3058), vy = device.F32(0x305C);  // culdcept-nx: -264'),
    ('render/draw.cpp', 'const float vw = device.F32(0x3168), vh = device.F32(0x316C);', 'const float vw = device.F32(0x3060), vh = device.F32(0x3064);  // culdcept-nx: -264'),
    ('backend/queued.cpp', 'constexpr u32 kDeltaPrivateUnreadBegin = 10880, kDeltaPrivateUnreadEnd = 11776;', 'constexpr u32 kDeltaPrivateUnreadBegin = 10752, kDeltaPrivateUnreadEnd = 11520;  // culdcept-nx: 0x2A00, past m_MiscPacket; the last boundary before the declaration at 11536'),
    ('backend/queued.cpp', 'static_assert(kDeltaUnreadEnd == 0x480 && kDeltaUnreadEnd % kDeltaUnitBytes == 0);', 'static_assert(kDeltaUnreadEnd == 0x400 && kDeltaUnreadEnd % kDeltaUnitBytes == 0);'),
    ('guest/command_buffers.cpp', 'static_assert(kLastChunkBytes == 32, "TakeLastChunk\'s loads");',
     '// culdcept-nx: XDK 2.0.3529\'s snapshot (12440 bytes) ends in a chunk of 24 bytes (pacman-nx\'s form).\nstatic_assert(kLastChunkBytes == 24, "TakeLastChunk\'s loads");'),
    ('guest/command_buffers.cpp', '''  const uint8x16_t a0 = vld1q_u8(device), a1 = vld1q_u8(device + 16);
  const uint8x16_t d0 = veorq_u8(a0, vld1q_u8(shadow)), d1 = veorq_u8(a1, vld1q_u8(shadow + 16));
  vst1q_u8(shadow, a0);
  vst1q_u8(shadow + 16, a1);
  return vmaxvq_u8(vorrq_u8(d0, d1)) != 0;''', '''  // culdcept-nx: 24 bytes (16 + 8; Katamari's 32 were 16 + 16).
  const uint8x16_t a0 = vld1q_u8(device);
  const uint8x8_t a1 = vld1_u8(device + 16);
  const uint8x16_t d0 = veorq_u8(a0, vld1q_u8(shadow));
  const uint8x8_t d1 = veor_u8(a1, vld1_u8(shadow + 16));
  vst1q_u8(shadow, a0);
  vst1_u8(shadow + 16, a1);
  return (vmaxvq_u8(d0) | vmaxv_u8(d1)) != 0;'''),
    # 5. The 41 -1 words: no such loop in this XDK's device init (no `li rN,41` in the Direct3D range): their place holds other fields.
    ('d3d/hooks/device.cpp', 'd.SetW(kDeviceOffsetTitleMinusOne + i * 4, 0xFFFFFFFFu);', '(void)kDeviceOffsetTitleMinusOne;  // culdcept-nx: XDK 2.0.3529 has no such words'),
    # 6. The full viewport: this game keeps none in its image (Katamari's {0, 0, 0xFFFF, 0xFFFF, 0.0, 1.0}): Renut's calls pass the values.
    ('d3d/hooks/draw.cpp', 'native::SetViewport(d, kFullViewport);', 'native::SetViewport(d, 0.0f, 0.0f, 65535.0f, 65535.0f, 0.0f, 1.0f);  // culdcept-nx: Katamari\'s kFullViewport'),
    # 7. The automatic front buffer (pacman-nx's fix; this game's InitializePresentationParameters sub_821828F0 calls CreateTexture first too).
    ('d3d/hooks/device.cpp', '''  if (!p[15])  // DisableAutoFrontBuffer
    RGPU_ERROR("InitializePresentationParameters: an automatic front buffer is not supported");''', '''  // pacman-nx: a game that asks for the automatic front buffer (N&B's parameters disable it): D3D's Present resolves the back buffer into it and
  // swaps it. As the game's own InitializePresentationParameters (culdcept-nx: sub_821828F0, its CreateTexture call).
  if (!p[15]) {  // DisableAutoFrontBuffer
    const u32 texture = native::CreateFrontBufferTexture(width, height, front_format);
    if (!texture)
      return false;
    d.SetW(native::off::AutoFrontBuffer, texture);
  }'''),
    ('d3d/d3d_native.h', 'u32 CreateSurface(u32 width, u32 height, u32 format, u32 multisample, u32 parameters_va);', '''u32 CreateSurface(u32 width, u32 height, u32 format, u32 multisample, u32 parameters_va);
// pacman-nx: the automatic front buffer's texture, as the game's own InitializePresentationParameters makes it.
u32 CreateFrontBufferTexture(u32 width, u32 height, u32 format);'''),
    ('d3d/hooks/resource.cpp', '''u32 AddRefResource(u32 resource) {
  return AddRef(resource);
}
''', '''u32 AddRefResource(u32 resource) {
  return AddRef(resource);
}

// pacman-nx; this game's call is sub_821828F0 at 0x8246B780: CreateTexture(w, h, 1, 1, 0, format, 0, 3).
u32 CreateFrontBufferTexture(u32 width, u32 height, u32 format) {
  return D3DDevice_CreateTexture_hook(width, height, 1, 1, 0, format, 0, 3);
}
'''),
]

# Files whose unannotated literals in Katamari's device range are moved by the zones. Each is a device access by construction (read in out/offset_scan.txt).
LITERAL_FILES = ['d3d/d3d_native_state.cpp', 'd3d/hooks/device.cpp', 'd3d/hooks/state.cpp', 'd3d/hooks/draw.cpp', 'd3d/d3d.h', 'render/draw.cpp']
HEX = re.compile(r'(?<![0-9A-Za-z_.])0x(2[789A-F][0-9A-Fa-f]{2}|3[0-9A-Fa-f]{3}|4[0-9A-Fa-f]{3}|5[0-7][0-9A-Fa-f]{2})(?:[uU])?(?![0-9A-Za-z_.])')
DEC = re.compile(r'(?<![0-9A-Za-z_.])(1[0-9]{4}|2[0-2][0-9]{3})(?![0-9A-Za-z_.])')
LINE_USE = re.compile(r'(\.(W|B|F|Q|H|SetW|SetB|SetF|SetQ|SetH|U32|F32)\(|\bd\.bytes\b|device \+ 0x|kDeviceOffset\w+ =|kDeviceOffsetsMinusOne|constexpr u32 k\w+ = 0x[0-9A-F]{4};|Video::NoteGameGamma|Swap\(device, front_buffer)')
SKIP = re.compile(r'static_assert|0x3FFF\b|0x2A23B9')


def apply_literals(check):
    moved = 0
    for rel in LITERAL_FILES:
        p = os.path.join(GPU, rel.replace('/', os.sep))
        t = open(p, encoding='utf-8', errors='surrogateescape', newline='').read()
        lines = t.split('\n')
        dirty = False
        for i, line in enumerate(lines):
            code, sep, comment = line.partition('//')
            if 'culdcept-nx' in line or not code.strip() or not LINE_USE.search(code) or SKIP.search(code):
                continue
            notes = []

            def sub(m, base):
                text = m.group(0)
                v = int(m.group(1), base)
                new = viva(v)
                if new is None or new == v:
                    return text
                notes.append(f'{v:#x}->{new:#x}')
                return f'0x{new:X}' + ('u' if text[-1] in 'uU' else '') if base == 16 else str(new)
            code2 = HEX.sub(lambda m: sub(m, 16), code)
            code2 = DEC.sub(lambda m: sub(m, 10), code2)
            if notes:
                eol = '\r' if line.endswith('\r') else ''
                body = (comment.rstrip('\r') + ' ') if sep else '  // '
                lines[i] = code2 + (sep if sep else '') + body + 'culdcept-nx: ' + ', '.join(notes) + eol if sep else code2.rstrip('\r') + '  // culdcept-nx: ' + ', '.join(notes) + eol
                dirty = True
                moved += 1
        if dirty and not check:
            open(p, 'w', encoding='utf-8', errors='surrogateescape', newline='').write('\n'.join(lines))
    return moved


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--check', action='store_true')
    a = ap.parse_args()
    done = skipped = 0
    bad = []
    for rel, old, new in EDITS:
        p = os.path.join(GPU, rel.replace('/', os.sep))
        t = open(p, encoding='utf-8', errors='surrogateescape', newline='').read()
        crlf = '\r\n' in t
        o, n = (old.replace('\n', '\r\n'), new.replace('\n', '\r\n')) if crlf else (old, new)
        if n and n in t:
            skipped += 1
        elif o in t:
            if not a.check:
                t = t.replace(o, n)
                open(p, 'w', encoding='utf-8', errors='surrogateescape', newline='').write(t)
            done += 1
        else:
            bad.append((rel, old[:70]))
    print(f'{done} edits applied, {skipped} already there, {len(bad)} missing')
    for b in bad:
        print('MISSING', b)
    print(f'{apply_literals(a.check)} literal lines moved')
    sys.exit(1 if bad else 0)


if __name__ == '__main__':
    main()

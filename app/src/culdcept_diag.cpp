// culdcept-nx (2026-11-19): diagnostics of Culdcept Saga's own functions, each behind a key (off).
//
// culdcept_diag_shader_compile: the game compiles its shaders at run time from data\shader.fx with the XDK's D3DX
// (D3DXCompileShaderFromFile sub_823A0488, called by the shader manager's sub_82380F70 / sub_82381030 for "VS00" /
// "PS00", vs_3_0 / ps_3_0). Logs each call's arguments, result and outputs: the compiled shader's buffer and the
// ID3DXConstantTable (with its vtable and SetBool slot), which the first boots called through as a null.
#include <cstdint>
#include <cstdio>
#include <string>

#include <rex/cvar.h>
#include <rex/hook.h>
#include <rex/logging/macros.h>
#include <rex/ppc.h>
#include <rex/system/kernel_state.h>
#include <rex/system/xmemory.h>
#include <fmt/format.h>

REXCVAR_DEFINE_BOOL(culdcept_diag_shader_compile, false, "Culdcept/Debug",
                    "Log every D3DXCompileShaderFromFile call (sub_823A0488): file, function, profile, result, the "
                    "shader buffer and the constant table")
    .lifecycle(rex::cvar::Lifecycle::kInitOnly);

namespace {

uint32_t Rd(uint8_t* base, uint32_t a) {
  if (!a) return 0;
  return __builtin_bswap32(*reinterpret_cast<uint32_t*>(base + a));
}

std::string Str(uint8_t* base, uint32_t a) {
  std::string s;
  if (!a) return "(null)";
  for (uint32_t i = 0; i < 96; ++i) {
    const char c = char(base[a + i]);
    if (!c) break;
    s += c;
  }
  return s;
}

}  // namespace

REX_EXTERN(__imp__sub_823A0488);
REX_HOOK_RAW(sub_823A0488) {
  if (!REXCVAR_GET(culdcept_diag_shader_compile)) {
    __imp__sub_823A0488(ctx, base);
    return;
  }
  const uint32_t file = ctx.r3.u32, function = ctx.r6.u32, profile = ctx.r7.u32, flags = ctx.r8.u32;
  const uint32_t pp_shader = ctx.r9.u32, pp_errors = ctx.r10.u32;
  const uint32_t pp_table = Rd(base, ctx.r1.u32 + 84);  // the ninth argument, in the caller's parameter area
  // The game asks for no error messages: give the call a slot of our own (a guest system-heap word) so the compiler's
  // text can be logged. The buffer is left to leak: a diagnostic.
  static uint32_t s_err_slot = rex::system::kernel_state()->memory()->SystemHeapAlloc(16, 16);
  uint32_t own_errors = 0;
  if (!pp_errors && s_err_slot) {
    *reinterpret_cast<uint32_t*>(base + s_err_slot) = 0;
    ctx.r10.u64 = s_err_slot;
    own_errors = s_err_slot;
  }
  __imp__sub_823A0488(ctx, base);
  if (own_errors) {
    const uint32_t buf = Rd(base, own_errors);
    if (buf) {
      // ID3DXBuffer: log its first words, and the text at the first pointer-like word that holds printable bytes.
      std::string words, text;
      for (uint32_t i = 0; i < 6; ++i) {
        const uint32_t w = Rd(base, buf + 4 * i);
        words += fmt::format(" {:08X}", w);
        if (text.empty() && w >= 0x40000000u && w < 0xC0000000u && base[w] >= 0x20 && base[w] < 0x7F) {
          for (uint32_t k = 0; k < 1200 && base[w + k]; ++k)
            text += char(base[w + k]) == '\n' ? '|' : char(base[w + k]);
        }
      }
      REXLOG_INFO("[culdcept] compiler errors (buffer {:08X}:{}): {}", buf, words, text);
    }
  }
  const uint32_t hr = ctx.r3.u32;
  const uint32_t shader = pp_shader ? Rd(base, pp_shader) : 0;
  const uint32_t table = pp_table ? Rd(base, pp_table) : 0;
  const uint32_t vtable = table ? Rd(base, table) : 0;
  const uint32_t errors = pp_errors ? Rd(base, pp_errors) : 0;
  // The compiled container (the ID3DXBuffer: size at +8, data at +12), saved as out/culdcept_shaders/<function>.bin
  // beside the executable's folder for the container study (game data: never committed).
  if (shader) {
    const uint32_t size = Rd(base, shader + 8), data = Rd(base, shader + 12);
    if (size && size < 0x100000 && data) {
      const std::string name = "culdcept_shader_" + Str(base, function) + "_" + Str(base, profile) + ".bin";
      if (FILE* f = std::fopen(name.c_str(), "wb")) {
        std::fwrite(base + data, 1, size, f);
        std::fclose(f);
      }
    }
  }
  REXLOG_INFO("[culdcept] D3DXCompileShaderFromFile('{}', '{}', '{}', flags {:08X}) = {:08X}; shader buffer {:08X}, "
              "constant table at {:08X} = {:08X} (vtable {:08X}, SetBool slot {:08X}), errors {:08X}",
              Str(base, file), Str(base, function), Str(base, profile), flags, hr, shader, pp_table, table, vtable,
              vtable ? Rd(base, vtable + 52) : 0, errors);
}

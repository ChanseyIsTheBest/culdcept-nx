// culdcept-nx (2026-11-19): diagnostics of Culdcept Saga's own functions, each behind a key (off).
//
// culdcept_diag_shader_compile: the game compiles its shaders at run time from data\shader.fx with the XDK's D3DX
// (D3DXCompileShaderFromFile sub_823A0488, called by the shader manager's sub_82380F70 / sub_82381030 for "VS00" /
// "PS00", vs_3_0 / ps_3_0). Logs each call's arguments, result and outputs: the compiled shader's buffer and the
// ID3DXConstantTable (with its vtable and SetBool slot), which the first boots called through as a null.
#include <cstdint>
#include <string>

#include <rex/cvar.h>
#include <rex/hook.h>
#include <rex/logging/macros.h>
#include <rex/ppc.h>

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
  __imp__sub_823A0488(ctx, base);
  const uint32_t hr = ctx.r3.u32;
  const uint32_t shader = pp_shader ? Rd(base, pp_shader) : 0;
  const uint32_t table = pp_table ? Rd(base, pp_table) : 0;
  const uint32_t vtable = table ? Rd(base, table) : 0;
  const uint32_t errors = pp_errors ? Rd(base, pp_errors) : 0;
  REXLOG_INFO("[culdcept] D3DXCompileShaderFromFile('{}', '{}', '{}', flags {:08X}) = {:08X}; shader buffer {:08X}, "
              "constant table at {:08X} = {:08X} (vtable {:08X}, SetBool slot {:08X}), errors {:08X}",
              Str(base, file), Str(base, function), Str(base, profile), flags, hr, shader, pp_table, table, vtable,
              vtable ? Rd(base, vtable + 52) : 0, errors);
}

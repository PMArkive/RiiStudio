#pragma once

#include <core/common.h>

#include "GPUAddressSpace.hpp"
#include "GPUCommand.hpp"

#include <glm/glm.hpp>
#include <librii/gx.h>
#include <tuple> // std::pair

namespace librii::gpu {

union AlphaTest {
  enum CompareMode : u32 {
    NEVER = 0,
    LESS = 1,
    EQUAL = 2,
    LEQUAL = 3,
    GREATER = 4,
    NEQUAL = 5,
    GEQUAL = 6,
    ALWAYS = 7
  };

  enum Op : u32 { AND = 0, OR = 1, XOR = 2, XNOR = 3 };

  BitField<0, 8, u32> ref0;
  BitField<8, 8, u32> ref1;
  BitField<16, 3, CompareMode> comp0;
  BitField<19, 3, CompareMode> comp1;
  BitField<22, 2, Op> logic;

  u32 hex = 0;

  enum TEST_RESULT {
    UNDETERMINED = 0,
    FAIL = 1,
    PASS = 2,
  };

  inline TEST_RESULT TestResult() const {
    switch (logic) {
    case AND:
      if (comp0 == ALWAYS && comp1 == ALWAYS)
        return PASS;
      if (comp0 == NEVER || comp1 == NEVER)
        return FAIL;
      break;

    case OR:
      if (comp0 == ALWAYS || comp1 == ALWAYS)
        return PASS;
      if (comp0 == NEVER && comp1 == NEVER)
        return FAIL;
      break;

    case XOR:
      if ((comp0 == ALWAYS && comp1 == NEVER) ||
          (comp0 == NEVER && comp1 == ALWAYS))
        return PASS;
      if ((comp0 == ALWAYS && comp1 == ALWAYS) ||
          (comp0 == NEVER && comp1 == NEVER))
        return FAIL;
      break;

    case XNOR:
      if ((comp0 == ALWAYS && comp1 == NEVER) ||
          (comp0 == NEVER && comp1 == ALWAYS))
        return FAIL;
      if ((comp0 == ALWAYS && comp1 == ALWAYS) ||
          (comp0 == NEVER && comp1 == NEVER))
        return PASS;
      break;

    default:
      return UNDETERMINED;
    }
    return UNDETERMINED;
  }
  operator librii::gx::AlphaComparison() {
    librii::gx::AlphaComparison tmp;
    tmp.refLeft = ref0.Value();
    tmp.refRight = ref1.Value();
    tmp.compLeft = static_cast<librii::gx::Comparison>(comp0.Value());
    tmp.compRight = static_cast<librii::gx::Comparison>(comp1.Value());
    tmp.op = static_cast<librii::gx::AlphaOp>(logic.Value());
    return tmp;
  }
};
union ZMode {
  enum CompareMode : u32 {
    NEVER = 0,
    LESS = 1,
    EQUAL = 2,
    LEQUAL = 3,
    GREATER = 4,
    NEQUAL = 5,
    GEQUAL = 6,
    ALWAYS = 7
  };

  BitField<0, 1, u32> testenable;
  BitField<1, 3, CompareMode> func;
  BitField<4, 1, u32> updateenable;

  u32 hex = 0;

  operator librii::gx::ZMode() {
    librii::gx::ZMode tmp;

    tmp.compare = testenable.Value();
    tmp.function = static_cast<librii::gx::Comparison>(func.Value());
    tmp.update = updateenable.Value();

    return tmp;
  }
};
union CMODE0 {
  enum BlendFactor : u32 {
    ZERO = 0,
    ONE = 1,
    SRCCLR = 2,            // for dst factor
    INVSRCCLR = 3,         // for dst factor
    DSTCLR = SRCCLR,       // for src factor
    INVDSTCLR = INVSRCCLR, // for src factor
    SRCALPHA = 4,
    INVSRCALPHA = 5,
    DSTALPHA = 6,
    INVDSTALPHA = 7
  };

  enum LogicOp : u32 {
    CLEAR = 0,
    AND = 1,
    AND_REVERSE = 2,
    COPY = 3,
    AND_INVERTED = 4,
    NOOP = 5,
    XOR = 6,
    OR = 7,
    NOR = 8,
    EQUIV = 9,
    INVERT = 10,
    OR_REVERSE = 11,
    COPY_INVERTED = 12,
    OR_INVERTED = 13,
    NAND = 14,
    SET = 15
  };

  BitField<0, 1, u32> blendenable;
  BitField<1, 1, u32> logicopenable;
  // Masked
  BitField<2, 1, u32> dither;
  BitField<3, 1, u32> colorupdate;
  BitField<4, 1, u32> alphaupdate;
  // ---
  BitField<5, 3, BlendFactor> dstfactor;
  BitField<8, 3, BlendFactor> srcfactor;
  BitField<11, 1, u32> subtract;
  BitField<12, 4, LogicOp> logicmode;

  u32 hex = 0;

  bool UseLogicOp() const;

  operator librii::gx::BlendMode() const {
    using namespace librii::gx;
    BlendMode tmp;

    if (logicopenable.Value())
      tmp.type = BlendModeType::logic;
    else if (subtract.Value())
      tmp.type = BlendModeType::subtract;
    else if (blendenable.Value())
      tmp.type = BlendModeType::blend;
    else
      tmp.type = BlendModeType::none;

    tmp.source = static_cast<BlendModeFactor>(srcfactor.Value());
    tmp.dest = static_cast<BlendModeFactor>(dstfactor.Value());
    tmp.logic = static_cast<librii::gx::LogicOp>(logicmode.Value());
    return tmp;
  }
};
union CMODE1 {
  BitField<0, 8, u32> alpha;
  BitField<8, 1, u32> enable;
  u32 hex = 0;

  operator gx::DstAlpha() const {
    return {.enabled = enable.Value() != 0,
            .alpha = static_cast<u8>(alpha.Value())};
  }
};
union ras1_ss {
  struct {
    u32 ss0 : 4; // Indirect tex stage 0, 2^(-ss0)
    u32 ts0 : 4; // Indirect tex stage 0
    u32 ss1 : 4; // Indirect tex stage 1
    u32 ts1 : 4; // Indirect tex stage 1
    u32 pad : 8;
    u32 rid : 8;
  };
  u32 hex = 0;
};
union IND_MTXA {
  struct {
    s32 ma : 11;
    s32 mb : 11;
    u32 s0 : 2; // bits 0-1 of scale factor
    u32 rid : 8;
  };
  u32 hex = 0;
};

union IND_MTXB {
  struct {
    s32 mc : 11;
    s32 md : 11;
    u32 s1 : 2; // bits 2-3 of scale factor
    u32 rid : 8;
  };
  u32 hex = 0;
};

union IND_MTXC {
  struct {
    s32 me : 11;
    s32 mf : 11;
    u32 s2 : 2; // bits 4-5 of scale factor
    u32 rid : 8;
  };
  u32 hex = 0;
};

struct IND_MTX {
  IND_MTXA col0;
  IND_MTXB col1;
  IND_MTXC col2;

  operator glm::mat4();
  Result<librii::gx::IndirectMatrix> lift(std::vector<std::string>& warnings);
};
union GPUTevReg {
  enum RegType { REGISTER, KONSTANT };
  u64 hex;

  // Access to individual registers
  BitField<0, 32, u64> low;
  BitField<32, 32, u64> high;

  // TODO: Check if Konst uses all 11 bits or just 8

  // Low register
  BitField<0, 11, s64> red;

  BitField<12, 11, s64> alpha;
  BitField<23, 1, u64> type_ra;

  // High register
  BitField<32, 11, s64> blue;

  BitField<44, 11, s64> green;
  BitField<55, 1, u64> type_bg;

  operator librii::gx::Color() {
    return {(u8)red.Value(), (u8)green.Value(), (u8)blue.Value(),
            (u8)alpha.Value()};
  }
  operator librii::gx::ColorS10() {
    return {(s16)red.Value(), (s16)green.Value(), (s16)blue.Value(),
            (s16)alpha.Value()};
  }
  GPUTevReg(const librii::gx::Color& color, RegType type) {
    red = color.r;
    green = color.g;
    blue = color.b;
    alpha = color.a;

    type_ra = type;
    type_bg = type;
  }
  GPUTevReg() { low = high = 0; }
};

// XF
union TexMtxInfo {
  BitField<0, 1, u32> pad;                //
  BitField<1, 1, u32> projection;         // XF_TEXPROJ_X
  BitField<2, 2, u32> inputform;          // XF_TEXINPUT_X
  BitField<4, 3, u32> texgentype;         // XF_TEXGEN_X
  BitField<7, 5, u32> sourcerow;          // XF_SRCGEOM_X
  BitField<12, 3, u32> embosssourceshift; // what generated texcoord to use
  BitField<15, 3, u32> embosslightshift;  // light index that is used

  u32 hex = 0;
};
union PostMtxInfo {
  BitField<0, 8, u32> index;     // base row of dual transform matrix
  BitField<8, 1, u32> normalize; // normalize before send operation
  u32 hex = 0;
};

enum {
  XF_TEX_ST = 0,
  XF_TEX_STQ = 1,
  XF_TEX_AB11 = 0,
  XF_TEX_ABC1 = 1,
  XF_TEXGEN_REGULAR = 0,
  XF_TEXGEN_EMBOSS_MAP = 1,
  XF_TEXGEN_COLOR_STRGBC0 = 2,
  XF_TEXGEN_COLOR_STRGBC1 = 3,
  XF_GEOM_INROW = 0,
  XF_NORMAL_INROW = 1,
  XF_COLORS_INROW = 2,
  XF_BINORMAL_T_INROW = 3,
  XF_BINORMAL_B_INROW = 4,
  XF_TEX0_INROW = 0x5,
  XF_TEX1_INROW = 0x6,
  XF_TEX2_INROW = 0x7,
  XF_TEX3_INROW = 0x8,
  XF_TEX4_INROW = 0x9,
  XF_TEX5_INROW = 0xa,
  XF_TEX6_INROW = 0xb,
  XF_TEX7_INROW = 0xc,

  XF_TEX0_ID = 0x1040,
  XF_DUALTEX0_ID = 0x1050,

  XF_INVTXSPEC_ID = 0x1008
};
struct XF_TEXTURE {
  u32 id;
  TexMtxInfo tex;
  PostMtxInfo dualTex;

  operator Result<librii::gx::TexCoordGen>();
};

union LitChannel {
  BitField<0, 1, u32> matsource;
  BitField<1, 1, u32> lightFunc;
  BitField<2, 4, u32> lightMask0_3;
  BitField<6, 1, u32> ambsource;
  BitField<7, 2, u32> diffuseAtten;
  BitField<9, 1, u32> attnEnable;
  BitField<10, 1, u32> attnSelect;
  BitField<11, 4, u32> lightMask4_7;
  u32 hex = 0;

  unsigned int GetFullLightMask() const {
    return lightFunc ? (lightMask0_3 | (lightMask4_7 << 4)) : 0;
  }
  void SetFullLightMask(u32 mask) {
    lightMask0_3 = mask & 0b1111;
    lightMask4_7 = (mask >> 4) & 0b1111;
  }

  void from(const librii::gx::ChannelControl& ctrl);
  operator librii::gx::ChannelControl() const;
};
union TevKSel {
  BitField<0, 2, u32> swaprb;
  BitField<2, 2, u32> swapga;
  BitField<4, 5, u32> kcsel0;
  BitField<9, 5, u32> kasel0;
  BitField<14, 5, u32> kcsel1;
  BitField<19, 5, u32> kasel1;
  u32 hex = 0;

  u32 getKC(int i) const { return i ? kcsel1.Value() : kcsel0.Value(); }
  u32 getKA(int i) const { return i ? kasel1.Value() : kasel0.Value(); }
};
union RAS1_IREF {
  struct {
    u32 bi0 : 3; // Indirect tex stage 0 ntexmap
    u32 bc0 : 3; // Indirect tex stage 0 ntexcoord
    u32 bi1 : 3;
    u32 bc1 : 3;
    u32 bi2 : 3;
    u32 bc2 : 3;
    u32 bi3 : 3;
    u32 bc3 : 3;
    u32 rid : 8;
  };
  u32 hex = 0;

  u32 getTexCoord(int i) const { return (hex >> (6 * i + 3)) & 7; }
  u32 getTexMap(int i) const { return (hex >> (6 * i)) & 7; }
};
union RAS1_TREF // or "TwoTevStageOrders"
{
  BitField<0, 3, u32> texmap0; // Indirect tex stage texmap
  BitField<3, 3, u32> texcoord0;
  BitField<6, 1, u32> enable0;    // 1 if should read from texture
  BitField<7, 3, u32> colorchan0; // RAS1_CC_X

  BitField<12, 3, u32> texmap1;
  BitField<15, 3, u32> texcoord1;
  BitField<18, 1, u32> enable1;    // 1 if should read from texture
  BitField<19, 3, u32> colorchan1; // RAS1_CC_X

  BitField<24, 8, u32> rid;

  u32 hex = 0;
  u32 getTexMap(int i) const { return i ? texmap1.Value() : texmap0.Value(); }
  u32 getTexCoord(int i) const {
    return i ? texcoord1.Value() : texcoord0.Value();
  }
  u32 getEnable(int i) const { return i ? enable1.Value() : enable0.Value(); }
  u32 getColorChan(int i) const {
    return i ? colorchan1.Value() : colorchan0.Value();
  }
};
union ColorCombiner {
  // abc=8bit,d=10bit
  BitField<0, 4, u32> d;  // TEVSELCC_X
  BitField<4, 4, u32> c;  // TEVSELCC_X
  BitField<8, 4, u32> b;  // TEVSELCC_X
  BitField<12, 4, u32> a; // TEVSELCC_X

  BitField<16, 2, u32> bias;
  BitField<18, 1, u32> op;
  BitField<19, 1, u32> clamp;

  BitField<20, 2, u32> shift;
  BitField<22, 2, u32> dest; // 1,2,3

  u32 hex = 0;
};
union AlphaCombiner {
  BitField<0, 2, u32> rswap;
  BitField<2, 2, u32> tswap;
  BitField<4, 3, u32> d;  // TEVSELCA_
  BitField<7, 3, u32> c;  // TEVSELCA_
  BitField<10, 3, u32> b; // TEVSELCA_
  BitField<13, 3, u32> a; // TEVSELCA_

  BitField<16, 2, u32> bias; // GXTevBias
  BitField<18, 1, u32> op;
  BitField<19, 1, u32> clamp;

  BitField<20, 2, u32> shift;
  BitField<22, 2, u32> dest; // 1,2,3

  u32 hex = 0;
};
union TevStageIndirect {
  BitField<0, 2, u32> bt;   // Indirect tex stage ID
  BitField<2, 2, u32> fmt;  // Format: ITF_X
  BitField<4, 3, u32> bias; // ITB_X
  BitField<7, 2, u32>
      bs; // ITBA_X, indicates which coordinate will become the 'bump alpha'
  BitField<9, 4, u32> mid; // Matrix ID to multiply offsets with
  BitField<13, 3, u32> sw; // ITW_X, wrapping factor for S of regular coord
  BitField<16, 3, u32> tw; // ITW_X, wrapping factor for T of regular coord
  BitField<19, 1, u32> lb_utclod;  // Use modified or unmodified texture
                                   // coordinates for LOD computation
  BitField<20, 1, u32> fb_addprev; // 1 if the texture coordinate results from
                                   // the previous TEV stage should be added

  struct {
    u32 hex : 21;
    u32 unused : 11;
  };

  // If bs and mid are zero, the result of the stage is independent of
  // the texture sample data, so we can skip sampling the texture.
  // bool IsActive() const { return bs != (u32)IndTexBiasSel::itb_none || mid !=
  // 0; }
};

#pragma pack(4)
union TVtxDesc {
  u64 Hex = 0;
  struct {
    // 0: not present
    // 1: present
    u64 PosMatIdx : 1;
    u64 Tex0MatIdx : 1;
    u64 Tex1MatIdx : 1;
    u64 Tex2MatIdx : 1;
    u64 Tex3MatIdx : 1;
    u64 Tex4MatIdx : 1;
    u64 Tex5MatIdx : 1;
    u64 Tex6MatIdx : 1;
    u64 Tex7MatIdx : 1;

    // 00: not present
    // 01: direct
    // 10: 8 bit index
    // 11: 16 bit index
    u64 Position : 2;
    u64 Normal : 2;
    u64 Color0 : 2;
    u64 Color1 : 2;
    u64 Tex0Coord : 2;
    u64 Tex1Coord : 2;
    u64 Tex2Coord : 2;
    u64 Tex3Coord : 2;
    u64 Tex4Coord : 2;
    u64 Tex5Coord : 2;
    u64 Tex6Coord : 2;
    u64 Tex7Coord : 2;
    u64 : 31;
  };

  struct {
    u32 Hex0;
    u32 Hex1;
  };

  // Easily index into the Position..Tex7Coord fields.
  u32 GetVertexArrayStatus(int idx) { return (Hex >> (9 + idx * 2)) & 0x3; }
};

union UVAT_group0 {
  u32 Hex = 0;
  struct {
    // 0:8
    u32 PosElements : 1;
    u32 PosFormat : 3;
    u32 PosFrac : 5;
    // 9:12
    u32 NormalElements : 1;
    u32 NormalFormat : 3;
    // 13:16
    u32 Color0Elements : 1;
    u32 Color0Comp : 3;
    // 17:20
    u32 Color1Elements : 1;
    u32 Color1Comp : 3;
    // 21:29
    u32 Tex0CoordElements : 1;
    u32 Tex0CoordFormat : 3;
    u32 Tex0Frac : 5;
    // 30:31
    u32 ByteDequant : 1;
    u32 NormalIndex3 : 1;
  };
};

union UVAT_group1 {
  u32 Hex = 0;
  struct {
    // 0:8
    u32 Tex1CoordElements : 1;
    u32 Tex1CoordFormat : 3;
    u32 Tex1Frac : 5;
    // 9:17
    u32 Tex2CoordElements : 1;
    u32 Tex2CoordFormat : 3;
    u32 Tex2Frac : 5;
    // 18:26
    u32 Tex3CoordElements : 1;
    u32 Tex3CoordFormat : 3;
    u32 Tex3Frac : 5;
    // 27:30
    u32 Tex4CoordElements : 1;
    u32 Tex4CoordFormat : 3;
    //
    u32 pad : 1;
  };
};

union UVAT_group2 {
  u32 Hex = 0;
  struct {
    // 0:4
    u32 Tex4Frac : 5;
    // 5:13
    u32 Tex5CoordElements : 1;
    u32 Tex5CoordFormat : 3;
    u32 Tex5Frac : 5;
    // 14:22
    u32 Tex6CoordElements : 1;
    u32 Tex6CoordFormat : 3;
    u32 Tex6Frac : 5;
    // 23:31
    u32 Tex7CoordElements : 1;
    u32 Tex7CoordFormat : 3;
    u32 Tex7Frac : 5;
  };
};

union TMatrixIndexA {
  struct {
    u32 PosNormalMtxIdx : 6;
    u32 Tex0MtxIdx : 6;
    u32 Tex1MtxIdx : 6;
    u32 Tex2MtxIdx : 6;
    u32 Tex3MtxIdx : 6;
  };
  struct {
    u32 Hex : 30;
    u32 unused : 2;
  };
};

union TMatrixIndexB {
  struct {
    u32 Tex4MtxIdx : 6;
    u32 Tex5MtxIdx : 6;
    u32 Tex6MtxIdx : 6;
    u32 Tex7MtxIdx : 6;
  };
  struct {
    u32 Hex : 24;
    u32 unused : 8;
  };
};

template <typename T> // T must be a range of texgens
std::pair<TMatrixIndexA, TMatrixIndexB> buildMtxIdx(T& texgens) {
  TMatrixIndexA first;
  first.Hex = first.unused = 0;
  TMatrixIndexB second;
  second.Hex = first.unused = 0;

  int i = 0;
  for (const auto& tg : texgens) {
    const auto mtx = (static_cast<u32>(tg.matrix) & ((1 << 6) - 1));
    if (i > 3) {
      first.Hex |= mtx >> (i * 6);
    } else {
      second.Hex |= mtx >> ((i - 4) * 6);
    }
    ++i;
  }

  return {first, second};
}

union TexMode0 {
  enum TextureFilter : u32 { TEXF_NONE = 0, TEXF_POINT = 1, TEXF_LINEAR = 2 };

  struct {
    u32 wrap_s : 2;
    u32 wrap_t : 2;
    u32 mag_filter : 1;
    u32 min_filter : 3;
    u32 diag_lod : 1;
    s32 lod_bias : 8;
    u32 pad0 : 2;
    u32 max_aniso : 2;
    u32 lod_clamp : 1;
  };
  u32 hex = 0;
};
union TexMode1 {
  struct {
    u32 min_lod : 8;
    u32 max_lod : 8;
  };
  u32 hex = 0;
};
// ZCOMPARE
union PEControl {
  enum PixelFormat : u32 {
    RGB8_Z24 = 0,
    RGBA6_Z24 = 1,
    RGB565_Z16 = 2,
    Z24 = 3,
    Y8 = 4,
    U8 = 5,
    V8 = 6,
    YUV420 = 7,
    INVALID_FMT = 0xffffffff, // Used by Dolphin to represent a missing value.
  };

  enum DepthFormat : u32 {
    ZLINEAR = 0,
    ZNEAR = 1,
    ZMID = 2,
    ZFAR = 3,

    // It seems these Z formats aren't supported/were removed ?
    ZINV_LINEAR = 4,
    ZINV_NEAR = 5,
    ZINV_MID = 6,
    ZINV_FAR = 7
  };

  BitField<0, 3, PixelFormat> pixel_format;
  BitField<3, 3, DepthFormat> zformat;
  BitField<6, 1, u32> early_ztest;

  u32 hex = 0;
};
union GenMode {
  enum CullMode : u32 {
    CULL_NONE = 0,
    CULL_BACK = 1,  // cull back-facing primitives
    CULL_FRONT = 2, // cull front-facing primitives
    CULL_ALL = 3,   // cull all primitives
  };

  BitField<0, 4, u32> numtexgens;
  BitField<4, 3, u32> numcolchans;
  // 1 bit unused?
  BitField<8, 1, u32> flat_shading; // unconfirmed
  BitField<9, 1, u32> multisampling;
  BitField<10, 4, u32> numtevstages;
  BitField<14, 2, CullMode> cullmode;
  BitField<16, 3, u32> numindstages;
  BitField<19, 1, u32> zfreeze;

  u32 hex = 0;
};
struct GPUMesh {
  TVtxDesc VCD;
};
#pragma pack()

} // namespace librii::gpu

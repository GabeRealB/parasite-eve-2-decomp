#include "gameplay/model_lighting.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/inline_c.h>
#include <psyq/memory.h>
#include <psyq/rand.h>

#include "common.h"
#include "gte.h"

#include "gameplay/attachment_state.h"
#include "gameplay/attachments.h"
#include "attachments.h"
#include "hud_sprites.h"
#include "model_lighting.h"
#include "model_objects.h"
#include "pad_input.h"
#include "gameplay/player_actor.h"
#include "scene_runtime.h"

/// Writable packet word containing vertex 0's texture coordinates and CLUT address.
///
/// `primitive` must point to a live, writable Psy-Q `POLY_FT3`, `POLY_FT4`,
/// `POLY_GT3` or `POLY_GT4` aligned to four bytes. On the little-endian target,
/// the `u32` lvalue spans `u0` (bits 0..7), `v0` (bits 8..15) and `clut`
/// (bits 16..31): two unsigned texel coordinates and the GPU's encoded palette
/// address. The word view copies all four bytes from the model stream together
/// before palette offsets are added. Evaluates `primitive` once and captures
/// no caller variables.
#define MODEL_LIGHTING_UV0_CLUT_WORD(primitive) (*(u32*)&((primitive)->u0))

/// Writable packet word containing vertex 1's texture coordinates and texture-page settings.
///
/// `primitive` must point to a live, writable Psy-Q `POLY_FT3`, `POLY_FT4`,
/// `POLY_GT3` or `POLY_GT4` aligned to four bytes. On the little-endian target,
/// the `u32` lvalue spans `u1` (bits 0..7), `v1` (bits 8..15) and `tpage`
/// (bits 16..31): two unsigned texel coordinates and the GPU's encoded page
/// location, colour depth and semi-transparency mode. The word view copies all
/// four bytes from the model stream together before page offsets are added.
/// Evaluates `primitive` once and captures no caller variables.
#define MODEL_LIGHTING_UV1_TPAGE_WORD(primitive) (*(u32*)&((primitive)->u1))

#include "main/display.h"
#include "main/fs.h"
#include "main/gamemain.h"
#include "main/mc.h"
#include "main/mem.h"
#include "main/pad.h"
#include "main/random.h"
#include "main/session.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/text.h"
#include "main/wipsys.h"
#include <psyq/memory.h>
#include <psyq/rand.h>

/// Work block of the task that runs play.
///
/// Each frame that task advances the play clock, watches for the death of the
/// player or the companion, and otherwise updates the HUD. It allocates this
/// block zero-filled when it starts, and the block is freed with the task. The
/// block carries the play time as the clock readout shows it, the tick sample
/// the clock advances from, and the HUD's state.
///
/// `hours` and `minutes` are the saved play time, a count of minutes, split
/// once at start and then kept in step with it: each whole minute of
/// `DisplayState.gameTick` adds one to both, and both stop at 999:59 with the
/// saved count at 59999. Only the clock readout reads them, and it is drawn
/// only while the saved state selects attract demo 1.
typedef struct {
    s32      hours;        // Whole hours of play time, 0..999
    s32      minutes;      // Minutes past the hour, 0..59
    s32      lastGameTick; // `DisplayState.gameTick` when the clock last advanced; the next advance adds the difference
    HudState hud;          // HUD state, handed to the HUD routines by address
} _PlayClockWork;
STATIC_ASSERT_SIZEOF(_PlayClockWork, 0x30);

extern CVECTOR D_80114BA4;

extern CVECTOR D_80114BA8;

/// Unreferenced nonzero word before the stored BSS.
extern u32 D_80114BAC;

static u32* func_8009FCDC(TmdStreamWorkspace* arg0, s32 arg1, u32* arg2);

static u32* func_8009FD28(TmdStreamWorkspace* arg0, s32 arg1, u32* arg2);

void func_807150F8(s32 arg0);

void func_80715198(void);

CVECTOR D_80114BA4 = { 0, 0, 0, 0 };
CVECTOR D_80114BA8 = { 0, 0, 0, 0 };
/// Unreferenced nonzero word before the stored BSS.
u32 D_80114BAC = 0x10FF2220;

// "Item obtained!"
// "Bonus item!!"

/* r1 = long vector in, r2 = long vector out: r2 = RT * r1 + TR at full
 * 32-bit precision, the input split into three 10/11-bit slices. */
#define gte_RotTransLV(r1, r2) __asm__ volatile( \
    "lw	$14, 0( %0 );"                           \
    "lw	$15, 4( %0 );"                           \
    "addiu	$16, $0, -0x400;"                     \
    "sra	$12, $14, 21;"                          \
    "and	$12, $16, $12;"                         \
    "andi	$13, $14, 0x3ff;"                      \
    "or	$12, $13, $12;"                          \
    "andi	$12, $12, 0xffff;"                     \
    "sra	$13, $15, 21;"                          \
    "and	$13, $16, $13;"                         \
    "andi	$16, $15, 0x3ff;"                      \
    "or	$13, $16, $13;"                          \
    "sll	$13, $13, 16;"                          \
    "or	$12, $13, $12;"                          \
    "mtc2	$12, $0;"                              \
    "sra	$14, $14, 10;"                          \
    "sra	$15, $15, 10;"                          \
    "addiu	$16, $0, -0x400;"                     \
    "sra	$12, $14, 21;"                          \
    "and	$12, $16, $12;"                         \
    "andi	$13, $14, 0x3ff;"                      \
    "or	$12, $13, $12;"                          \
    "sra	$13, $15, 21;"                          \
    "and	$13, $16, $13;"                         \
    "andi	$16, $15, 0x3ff;"                      \
    "or	$13, $16, $13;"                          \
    "srl	$16, $15, 31;"                          \
    "addu	$13, $13, $16;"                        \
    "sll	$13, $13, 16;"                          \
    "srl	$16, $14, 31;"                          \
    "addu	$12, $12, $16;"                        \
    "andi	$12, $12, 0xffff;"                     \
    "or	$12, $13, $12;"                          \
    "mtc2	$12, $2;"                              \
    "sra	$14, $14, 10;"                          \
    "sra	$15, $15, 10;"                          \
    "andi	$12, $14, 0xffff;"                     \
    "srl	$16, $14, 31;"                          \
    "addu	$12, $16, $12;"                        \
    "andi	$12, $12, 0xffff;"                     \
    "andi	$13, $15, 0xffff;"                     \
    "srl	$16, $15, 31;"                          \
    "addu	$13, $16, $13;"                        \
    "sll	$13, $13, 16;"                          \
    "or	$12, $13, $12;"                          \
    "mtc2	$12, $4;"                              \
    "lw	$16, 8( %0 );"                           \
    "addiu	$14, $0, -0x400;"                     \
    "srl	$15, $16, 31;"                          \
    "sra	$12, $16, 21;"                          \
    "and	$12, $14, $12;"                         \
    "andi	$13, $16, 0x3ff;"                      \
    "or	$12, $13, $12;"                          \
    "mtc2	$12, $1;"                              \
    "sra	$16, $16, 10;"                          \
    "sra	$12, $16, 21;"                          \
    "and	$12, $14, $12;"                         \
    "andi	$13, $16, 0x3ff;"                      \
    "or	$12, $13, $12;"                          \
    "addu	$12, $12, $15;"                        \
    "mtc2	$12, $3;"                              \
    "sra	$16, $16, 10;"                          \
    "addu	$12, $16, $15;"                        \
    "mtc2	$12, $5;"                              \
    "nop;"                                       \
    "nop;"                                       \
    ".word 0x4A480012;"                          \
    "mfc2	$14, $25;"                             \
    "mfc2	$15, $26;"                             \
    "mfc2	$16, $27;"                             \
    "nop;"                                       \
    "nop;"                                       \
    ".word 0x4A40E012;"                          \
    "mfc2	$12, $25;"                             \
    "nop;"                                       \
    "sra	$12, $12, 2;"                           \
    "addu	$14, $12, $14;"                        \
    "mfc2	$12, $26;"                             \
    "nop;"                                       \
    "sra	$12, $12, 2;"                           \
    "addu	$15, $12, $15;"                        \
    "mfc2	$12, $27;"                             \
    "nop;"                                       \
    "sra	$12, $12, 2;"                           \
    "addu	$16, $12, $16;"                        \
    "nop;"                                       \
    "nop;"                                       \
    ".word 0x4A416012;"                          \
    "mfc2	$12, $25;"                             \
    "nop;"                                       \
    "sll	$12, $12, 8;"                           \
    "addu	$14, $12, $14;"                        \
    "mfc2	$12, $26;"                             \
    "nop;"                                       \
    "sll	$12, $12, 8;"                           \
    "addu	$15, $12, $15;"                        \
    "mfc2	$12, $27;"                             \
    "nop;"                                       \
    "sll	$12, $12, 8;"                           \
    "addu	$16, $12, $16;"                        \
    "sw	$14, 0( %1 );"                           \
    "sw	$15, 4( %1 );"                           \
    "sw	$16, 8( %1 )"                            \
    :                                            \
    : "r"(r1), "r"(r2)                           \
    : "$12", "$13", "$14", "$15", "$16", "memory")

/// Initializes one flat textured triangle's persistent texture coordinates and GPU addresses.
///
/// `triangle` is a writable, four-byte-aligned `POLY_FT3`. `elementWords` points
/// to a four-byte-aligned element of a `0x1C`/`0x1E` stream record, past its
/// three-word header, with at least five readable u32 words. Words 2 and 3 pack
/// unsigned byte U/V texel coordinates with encoded CLUT and texture-page
/// settings; word 4's low half packs U2/V2. Its high half is ignored, preserving
/// `triangle->pad1`.
///
/// The workspace supplies signed displacements in encoded address units:
/// `texturePageOffset` (-128..127) and `encodedClutOffset` (-8192..8128, 64 per
/// palette row). Each sum wraps in its u16 field without changing U/V. The
/// packet's tag, colour/command and screen positions remain untouched. All
/// three objects are borrowed for the call; no cursor or count is changed.
static inline void _modelLightingInitFt3Texture(POLY_FT3* triangle, const u32* elementWords, const TmdStreamWorkspace* workspace)
{
    // Word indices within the element, excluding the record header.
    enum {
        MODEL_LIGHTING_FT3_UV0_CLUT_WORD  = 2, // U0/V0 in low half, CLUT address in high half
        MODEL_LIGHTING_FT3_UV1_TPAGE_WORD = 3, // U1/V1 in low half, texture-page settings in high half
        MODEL_LIGHTING_FT3_UV2_WORD       = 4  // U2/V2 in low half; high half is not copied
    };

    MODEL_LIGHTING_UV0_CLUT_WORD(triangle)  = elementWords[MODEL_LIGHTING_FT3_UV0_CLUT_WORD];
    MODEL_LIGHTING_UV1_TPAGE_WORD(triangle) = elementWords[MODEL_LIGHTING_FT3_UV1_TPAGE_WORD];
    *(u16*)&triangle->u2                    = (u16)elementWords[MODEL_LIGHTING_FT3_UV2_WORD];
    triangle->tpage                        += workspace->texturePageOffset;
    triangle->clut                         += workspace->encodedClutOffset;
}

/// Initializes one flat textured quad's persistent texture coordinates and GPU addresses.
///
/// `quad` is a writable, four-byte-aligned `POLY_FT4`. `elementWords` points to
/// a four-byte-aligned element of a `0x5C`/`0x5E` stream record, past its
/// three-word header, with at least five readable u32 words. Words 2 and 3 pack
/// unsigned byte U/V texel coordinates with encoded CLUT and texture-page
/// settings; word 4 packs U2/V2 in its low half and U3/V3 in its high half.
///
/// The construction workspace supplies signed displacements in encoded address
/// units: `texturePageOffset` (-128..127) and `encodedClutOffset` (-8192..8128,
/// 64 per palette row). Each sum wraps in its u16 field without changing U/V.
/// The packet's tag, colour/command, screen positions and pad1/pad2 remain
/// untouched. All three objects are borrowed for the call; no cursor or count
/// is changed and no pointer is retained.
static inline void _modelLightingInitFt4Texture(POLY_FT4* quad, const u32* elementWords, const TmdStreamWorkspace* workspace)
{
    // Word indices within the element, excluding the record header.
    enum {
        MODEL_LIGHTING_FT4_UV0_CLUT_WORD  = 2, // U0/V0 in low half, CLUT address in high half
        MODEL_LIGHTING_FT4_UV1_TPAGE_WORD = 3, // U1/V1 in low half, texture-page settings in high half
        MODEL_LIGHTING_FT4_UV2_UV3_WORD   = 4  // U2/V2 in low half, U3/V3 in high half
    };

    MODEL_LIGHTING_UV0_CLUT_WORD(quad)  = elementWords[MODEL_LIGHTING_FT4_UV0_CLUT_WORD];
    MODEL_LIGHTING_UV1_TPAGE_WORD(quad) = elementWords[MODEL_LIGHTING_FT4_UV1_TPAGE_WORD];
    // Halfword stores copy each U/V pair without overwriting the SDK pad fields.
    *(u16*)&quad->u2 = (u16)elementWords[MODEL_LIGHTING_FT4_UV2_UV3_WORD];
    *(u16*)&quad->u3 = (u16)(elementWords[MODEL_LIGHTING_FT4_UV2_UV3_WORD] >> 16);
    quad->tpage     += workspace->texturePageOffset;
    quad->clut      += workspace->encodedClutOffset;
}

/// Initializes one gouraud textured triangle's persistent texture coordinates and GPU addresses.
///
/// `triangle` is a writable, four-byte-aligned `POLY_GT3`. `elementWords` points
/// to a four-byte-aligned element of a `0x38`-family stream record, past its
/// three-word header, with at least six readable u32 words. The first three
/// words hold geometry references. Words 3 and 4 pack unsigned byte U/V texel
/// coordinates in their low halves and encoded CLUT and texture-page settings
/// in their high halves. Word 5's low half packs U2/V2; its high half is ignored.
///
/// The construction workspace supplies signed displacements in encoded address
/// units: `texturePageOffset` (-128..127) and `encodedClutOffset` (-8192..8128,
/// 64 per palette row). Each sum wraps in its u16 field without changing U/V.
/// The packet's tag, colours/command, screen positions and pad fields remain
/// untouched for the draw pass. All three objects are borrowed for the call;
/// no pointer is retained and no workspace cursor or count is changed.
static inline void _tmdInitGt3Texture(POLY_GT3* triangle, const u32* elementWords, const TmdStreamWorkspace* workspace)
{
    // Word indices within the element, excluding the record header.
    enum {
        TMD_GT3_UV0_CLUT_WORD  = 3, // U0/V0 in low half, CLUT address in high half
        TMD_GT3_UV1_TPAGE_WORD = 4, // U1/V1 in low half, texture-page settings in high half
        TMD_GT3_UV2_WORD       = 5  // U2/V2 in low half; high half is not copied
    };

    MODEL_LIGHTING_UV0_CLUT_WORD(triangle)  = elementWords[TMD_GT3_UV0_CLUT_WORD];
    MODEL_LIGHTING_UV1_TPAGE_WORD(triangle) = elementWords[TMD_GT3_UV1_TPAGE_WORD];
    // A halfword store copies U2/V2 without overwriting the adjacent pad2.
    *(u16*)&triangle->u2 = (u16)elementWords[TMD_GT3_UV2_WORD];
    triangle->tpage     += workspace->texturePageOffset;
    triangle->clut      += workspace->encodedClutOffset;
}

/// Seeds one Gouraud textured quad's persistent texture data for the draw pass.
///
/// `quad` must address a writable, four-byte-aligned `POLY_GT4` packet.
/// `elementWords` starts at a four-byte-aligned element payload, after the
/// record's three-word header, with at least seven readable u32 words. This
/// minimum does not establish the full element stride. Words 0..3 pack four
/// vertex and four normal byte offsets; this helper does not read geometry.
/// On the little-endian target, words 4 and 5 pack unsigned byte U/V texel
/// coordinates in bits 0..15 and encoded CLUT/page settings in bits 16..31.
/// Word 6 packs U2/V2 in its low half and U3/V3 in its high half.
///
/// `workspace` supplies construction-time signed encoded-address displacements:
/// `texturePageOffset` (-128..127) and `encodedClutOffset` (-8192..8128, 64 per
/// palette row). Both sums wrap modulo 65536 in their u16 packet fields without
/// changing U/V. The packet's tag, colours/command, screen positions and SDK
/// pad fields are preserved for drawing. All three objects are borrowed for
/// this call; no pointer is retained and the workspace is unchanged.
static inline void _modelLightingInitGt4Texture(POLY_GT4* quad, const u32* elementWords, const TmdStreamWorkspace* workspace)
{
    // Word indices within the element, excluding the record header.
    enum {
        MODEL_LIGHTING_GT4_UV0_CLUT_WORD  = 4, // U0/V0 in low half, CLUT address in high half
        MODEL_LIGHTING_GT4_UV1_TPAGE_WORD = 5, // U1/V1 in low half, texture-page settings in high half
        MODEL_LIGHTING_GT4_UV2_UV3_WORD   = 6  // U2/V2 in low half, U3/V3 in high half
    };

    STATIC_ASSERT(OFFSET_OF(POLY_GT4, v2) == OFFSET_OF(POLY_GT4, u2) + sizeof(u8) &&
                      OFFSET_OF(POLY_GT4, pad2) == OFFSET_OF(POLY_GT4, u2) + sizeof(u16) &&
                      OFFSET_OF(POLY_GT4, v3) == OFFSET_OF(POLY_GT4, u3) + sizeof(u8) &&
                      OFFSET_OF(POLY_GT4, pad3) == OFFSET_OF(POLY_GT4, u3) + sizeof(u16),
                  model_lighting_gt4_uv_pair_layout);

    MODEL_LIGHTING_UV0_CLUT_WORD(quad)  = elementWords[MODEL_LIGHTING_GT4_UV0_CLUT_WORD];
    MODEL_LIGHTING_UV1_TPAGE_WORD(quad) = elementWords[MODEL_LIGHTING_GT4_UV1_TPAGE_WORD];
    // Split the packed U/V pairs without overwriting the adjacent pad2/pad3.
    *(u16*)&quad->u2 = (u16)elementWords[MODEL_LIGHTING_GT4_UV2_UV3_WORD];
    *(u16*)&quad->u3 = (u16)(elementWords[MODEL_LIGHTING_GT4_UV2_UV3_WORD] >> 16);
    quad->tpage     += workspace->texturePageOffset;
    quad->clut      += workspace->encodedClutOffset;
}

/// Seeds one per-corner-colour Gouraud quad's persistent texture data for the draw pass.
///
/// `quad` must address a writable, four-byte-aligned `POLY_GT4` packet.
/// `elementWords` starts at a four-byte-aligned element payload, after an
/// opcode `0x170` record's three-word header, and must provide at least eleven
/// readable u32 words. Eleven words is the readable minimum, not the element's
/// stride. Words 0..3 pack eight u16 byte offsets, vertices 0..3 then normals
/// 0..3. Words 4..7 are the corners' material RGB and command bytes, lit when
/// the quad is drawn. This helper copies only the texture suffix. On the
/// little-endian target, word 8 packs U0/V0 in bits 0..15 and the encoded CLUT
/// in bits 16..31, and word 9 packs U1/V1 in bits 0..15 and the encoded
/// texture-page settings in bits 16..31. Word 10 packs U2/V2 in its low half
/// and U3/V3 in its high half.
///
/// `workspace` supplies construction-time signed encoded-address displacements:
/// `texturePageOffset` (-128..127) and `encodedClutOffset` (-8192..8128, 64 per
/// palette row). Both sums wrap modulo 65536 in their u16 packet fields and
/// leave U/V unchanged. The packet's tag, colours/command, screen positions and
/// SDK pad fields stay as they are for drawing. All three objects are borrowed
/// for this call; no pointer is retained and the workspace is not modified.
static inline void _modelLightingInitGt4CornerColorsTexture(POLY_GT4* quad, const u32* elementWords,
                                                            const TmdStreamWorkspace* workspace)
{
    // Word indices within the element, excluding the record header.
    enum {
        MODEL_LIGHTING_GT4_CORNER_COLORS_UV0_CLUT_WORD  = 8, // U0/V0 in low half, CLUT address in high half
        MODEL_LIGHTING_GT4_CORNER_COLORS_UV1_TPAGE_WORD = 9, // U1/V1 in low half, texture-page settings in high half
        MODEL_LIGHTING_GT4_CORNER_COLORS_UV2_UV3_WORD   = 10 // U2/V2 in low half, U3/V3 in high half
    };

    STATIC_ASSERT(OFFSET_OF(POLY_GT4, v2) == OFFSET_OF(POLY_GT4, u2) + sizeof(u8) &&
                      OFFSET_OF(POLY_GT4, pad2) == OFFSET_OF(POLY_GT4, u2) + sizeof(u16) &&
                      OFFSET_OF(POLY_GT4, v3) == OFFSET_OF(POLY_GT4, u3) + sizeof(u8) &&
                      OFFSET_OF(POLY_GT4, pad3) == OFFSET_OF(POLY_GT4, u3) + sizeof(u16),
                  model_lighting_gt4_corner_colors_uv_pair_layout);

    MODEL_LIGHTING_UV0_CLUT_WORD(quad)  = elementWords[MODEL_LIGHTING_GT4_CORNER_COLORS_UV0_CLUT_WORD];
    MODEL_LIGHTING_UV1_TPAGE_WORD(quad) = elementWords[MODEL_LIGHTING_GT4_CORNER_COLORS_UV1_TPAGE_WORD];
    // Split the packed U/V pairs without overwriting the adjacent pad2/pad3.
    *(u16*)&quad->u2 = (u16)elementWords[MODEL_LIGHTING_GT4_CORNER_COLORS_UV2_UV3_WORD];
    *(u16*)&quad->u3 = (u16)(elementWords[MODEL_LIGHTING_GT4_CORNER_COLORS_UV2_UV3_WORD] >> 16);
    quad->tpage     += workspace->texturePageOffset;
    quad->clut      += workspace->encodedClutOffset;
}

/// Initializes one Gouraud textured quad's persistent texture fields from packed element words.
///
/// `elementWords` is a four-byte-aligned element base, after the stream record's
/// three-word header. `uv0ClutWordIndex` counts u32 words from that base and
/// selects three consecutive readable words in the same element. It must be
/// nonnegative, with index + 2 representable in s32; bounds are not checked.
/// The first two words pack unsigned byte U/V texel coordinates in their low
/// halves and encoded CLUT and texture-page settings in their high halves.
/// The third word packs U2/V2 in its low half and U3/V3 in its high half.
/// This readable extent does not establish the element's complete size.
///
/// `quad` must be a writable, four-byte-aligned `POLY_GT4`. A construction
/// workspace supplies signed encoded-address displacements: `texturePageOffset`
/// (-128..127) and `encodedClutOffset` (-8192..8128, 64 per palette row). Sums
/// wrap modulo 65536 in the u16 packet fields without changing U/V. The tag,
/// colours/command, screen positions and SDK pad fields remain untouched.
/// All storage is borrowed for the call; no pointer is retained and no workspace
/// cursor or count is changed.
static inline void _modelLightingInitGt4TextureWords(POLY_GT4* quad, const u32* elementWords, s32 uv0ClutWordIndex,
                                                     const TmdStreamWorkspace* workspace)
{
    // Relative word positions in the packed texture suffix, independent of its element prefix.
    enum {
        /// Offset in u32 stream words from a GT4 element's U0/V0/CLUT word to its U1/V1/texture-page word.
        ///
        /// The next four-byte word packs U1 in bits 0..7, V1 in bits 8..15 and
        /// encoded texture-page settings in bits 16..31 on the little-endian
        /// target. This is independent of the element prefix and of the byte
        /// spacing between the destination packet's texture fields. Adding it
        /// to `uv0ClutWordIndex` must fit in s32 and select a readable word in
        /// the same element. The full word is copied before page relocation.
        MODEL_LIGHTING_GT4_UV1_TPAGE_WORD_OFFSET = 1,
        /// Offset in u32 stream words from a GT4 element's U0/V0/CLUT word to its U2/V2/U3/V3 word.
        ///
        /// The word two positions later packs U2/V2 in bits 0..15 and U3/V3 in
        /// bits 16..31 on the little-endian target. Each half is copied into
        /// the packet's adjacent u/v bytes, preserving pad2 and pad3. This
        /// offset is independent of the element prefix and the destination
        /// packet's byte layout. Adding it to `uv0ClutWordIndex` must fit in
        /// s32 and select a readable u32 word in the same element; it does not
        /// establish the element's size.
        MODEL_LIGHTING_GT4_UV2_UV3_WORD_OFFSET = 2
    };

    STATIC_ASSERT(OFFSET_OF(POLY_GT4, v2) == OFFSET_OF(POLY_GT4, u2) + sizeof(u8) &&
                      OFFSET_OF(POLY_GT4, pad2) == OFFSET_OF(POLY_GT4, u2) + sizeof(u16) &&
                      OFFSET_OF(POLY_GT4, v3) == OFFSET_OF(POLY_GT4, u3) + sizeof(u8) &&
                      OFFSET_OF(POLY_GT4, pad3) == OFFSET_OF(POLY_GT4, u3) + sizeof(u16),
                  model_lighting_gt4_texture_words_uv_pair_layout);

    MODEL_LIGHTING_UV0_CLUT_WORD(quad)  = elementWords[uv0ClutWordIndex];
    MODEL_LIGHTING_UV1_TPAGE_WORD(quad) = elementWords[uv0ClutWordIndex + MODEL_LIGHTING_GT4_UV1_TPAGE_WORD_OFFSET];
    // Split the packed U/V pairs without overwriting the adjacent pad2/pad3.
    *(u16*)&quad->u2 = (u16)elementWords[uv0ClutWordIndex + MODEL_LIGHTING_GT4_UV2_UV3_WORD_OFFSET];
    *(u16*)&quad->u3 = (u16)(elementWords[uv0ClutWordIndex + MODEL_LIGHTING_GT4_UV2_UV3_WORD_OFFSET] >> 16);
    quad->tpage     += workspace->texturePageOffset;
    quad->clut      += workspace->encodedClutOffset;
}

/// Initializes the offset-layer texture of one layered Gouraud triangle.
///
/// `triangle` must be a writable, four-byte-aligned `POLY_GT3` for the first
/// packet in a layered pair. `elementWords` starts after a `0x4038` record's
/// three-word header and provides at least six readable, four-byte-aligned u32
/// words. Words 3 and 4 pack unsigned byte U/V texel coordinates with encoded
/// CLUT and texture-page settings; only word 5's low half supplies U2/V2.
/// This minimum readable extent does not establish the element's full stride.
///
/// `workspace->obj` must be a live object. Its `layerTexturePageOffset` adds
/// -128..127 encoded page units; `layerClutRowOffset` adds -128..127 palette
/// rows (-8192..8128 encoded CLUT units, 64 per row). These offsets are
/// independent of the workspace's base-texture offsets. Address sums wrap
/// modulo 65536 in the packet's u16 fields. Setting ABR bit 5 after relocation
/// preserves bit 6, selecting mode 1 or 3; drawing enables semi-transparency.
/// The tag, colours/command, positions and SDK pad fields remain untouched.
/// All storage is borrowed for the call; no pointer is retained and neither
/// workspace nor object is modified.
static inline void _modelLightingInitGt3OffsetLayerTexture(POLY_GT3* triangle, const u32* elementWords, const TmdStreamWorkspace* workspace)
{
    enum {
        /// Index of vertex 0's packed texture/CLUT word for the offset layer.
        ///
        /// Counts u32 words from the element base after the stream record's
        /// three-word header. Words 0..2 hold three vertex and three normal
        /// references. Word 3 packs unsigned texel coordinates U0 in bits
        /// 0..7 and V0 in bits 8..15, with the encoded GPU palette address in
        /// bits 16..31. All four bytes are copied to the first `POLY_GT3`
        /// packet's u0/v0/clut before the object's signed CLUT-row displacement
        /// is added. The selected word must be readable and four-byte aligned.
        MODEL_LIGHTING_GT3_OFFSET_LAYER_UV0_CLUT_WORD_INDEX = 3,
        /// Index of vertex 1's packed texture/page word for the offset layer.
        ///
        /// Counts u32 words from the element base after the stream record's
        /// three-word header. On the little-endian target, word 4 packs
        /// unsigned texel coordinates U1 in bits 0..7 and V1 in bits 8..15,
        /// with encoded page location, colour depth and semi-transparency mode
        /// in bits 16..31. All four bytes initialize the first `POLY_GT3`
        /// packet's u1/v1/tpage before the object's signed page displacement
        /// and blend-mode adjustment; U1/V1 remain unchanged. The selected
        /// word must be readable and four-byte aligned. This index does not
        /// define the element's stride or complete extent.
        MODEL_LIGHTING_GT3_OFFSET_LAYER_UV1_TPAGE_WORD_INDEX = 4,
        /// Index of vertex 2's packed texture-coordinate word for the offset layer.
        ///
        /// Counts u32 words from the element payload after the stream record's
        /// three-word header. On the little-endian target, word 5 packs
        /// unsigned texel coordinates U2 in bits 0..7 and V2 in bits 8..15.
        /// Only the low half initializes the first `POLY_GT3` packet's u2/v2
        /// bytes, preserving its adjacent `pad2`; the source high half is
        /// ignored and its role is unproven. The selected word must be readable
        /// and four-byte aligned. This index does not define the element's
        /// stride or complete extent.
        MODEL_LIGHTING_GT3_OFFSET_LAYER_UV2_WORD_INDEX = 5,
        /// Encoded texture-page mask setting the offset layer's low ABR bit.
        ///
        /// Apply to the page after its signed displacement has wrapped to u16.
        /// The GPU's two-bit semi-transparency mode occupies bits 5..6; ORing
        /// this mask retains bit 6, selecting mode 1 when clear or 3 when set.
        /// Every other page bit is preserved. The primitive command separately
        /// enables semi-transparency.
        MODEL_LIGHTING_OFFSET_LAYER_TPAGE_ABR_LOW_BIT = 1 << 5,
        /// Shift from signed offset-layer palette rows to encoded CLUT displacement.
        ///
        /// The GPU CLUT address stores Y above six X/16 column bits.
        /// Sign-extending the object's s8 row offset before this shift converts
        /// -128..127 rows to -8192..8128 encoded units (64 per row). The sum
        /// wraps in the packet's u16 CLUT field, preserving the six column bits.
        MODEL_LIGHTING_OFFSET_LAYER_CLUT_ROW_SHIFT = 6
    };
    u32 layerTexturePage;
    u8  layerClutRowByte;

    MODEL_LIGHTING_UV0_CLUT_WORD(triangle)  = elementWords[MODEL_LIGHTING_GT3_OFFSET_LAYER_UV0_CLUT_WORD_INDEX];
    MODEL_LIGHTING_UV1_TPAGE_WORD(triangle) = elementWords[MODEL_LIGHTING_GT3_OFFSET_LAYER_UV1_TPAGE_WORD_INDEX];
    // Copy the U/V byte pair without overwriting the adjacent packet pad2.
    *(u16*)&triangle->u2 = (u16)elementWords[MODEL_LIGHTING_GT3_OFFSET_LAYER_UV2_WORD_INDEX];
    // The page sum wraps to u16 before its ABR mode is adjusted.
    triangle->tpage += workspace->obj->layerTexturePageOffset;
    // Keep the row byte unsigned until the CLUT calculation restores its sign.
    layerClutRowByte  = workspace->obj->layerClutRowOffset;
    layerTexturePage  = triangle->tpage;
    layerTexturePage |= MODEL_LIGHTING_OFFSET_LAYER_TPAGE_ABR_LOW_BIT;
    triangle->tpage   = layerTexturePage;
    triangle->clut   += (s8)layerClutRowByte << MODEL_LIGHTING_OFFSET_LAYER_CLUT_ROW_SHIFT;
}

/// Initializes the offset-layer texture of one layered Gouraud quad.
///
/// `quad` must be a writable, four-byte-aligned `POLY_GT4` for the first
/// packet in a layered pair. `elementWords` starts after a `0x4078` record's
/// three-word header and provides at least seven readable, four-byte-aligned
/// u32 words. Words 0..3 pack four vertex and four normal references and are
/// not read here. Words 4 and 5 pack unsigned byte U/V texel coordinates with
/// encoded CLUT and texture-page settings. Word 6 packs U2/V2 in its low half
/// and U3/V3 in its high half. This minimum readable extent does not establish
/// the element's full stride.
///
/// `workspace->obj` must be a live object. Its `layerTexturePageOffset` adds
/// -128..127 encoded page units; `layerClutRowOffset` adds -128..127 palette
/// rows (-8192..8128 encoded CLUT units, 64 per row). These offsets are
/// independent of the workspace's base-texture offsets. Address sums wrap
/// modulo 65536 in the packet's u16 fields. Setting ABR bit 5 after relocation
/// preserves bit 6, selecting mode 1 or 3; drawing enables semi-transparency.
/// The tag, colours/command, positions and SDK pad fields remain untouched.
/// All storage is borrowed for the call; no pointer is retained and neither
/// workspace nor object is modified.
static inline void _modelLightingInitGt4OffsetLayerTexture(POLY_GT4* quad, const u32* elementWords,
                                                           const TmdStreamWorkspace* workspace)
{
    enum {
        MODEL_LIGHTING_GT4_OFFSET_LAYER_UV0_CLUT_WORD  = 4,      // Packed U0/V0 bytes and encoded CLUT
        MODEL_LIGHTING_GT4_OFFSET_LAYER_UV1_TPAGE_WORD = 5,      // Packed U1/V1 bytes and encoded page settings
        MODEL_LIGHTING_GT4_OFFSET_LAYER_UV2_UV3_WORD   = 6,      // U2/V2 in the low half, U3/V3 in the high half
        MODEL_LIGHTING_OFFSET_LAYER_TPAGE_ABR_LOW_BIT  = 1 << 5, // OR after relocation; retains ABR bit 6 (mode 1 or 3)
        MODEL_LIGHTING_OFFSET_LAYER_CLUT_ROW_SHIFT     = 6       // Signed palette rows to encoded CLUT units (64 per row)
    };
    u32 layerTexturePage;
    u8  layerClutRowByte;

    STATIC_ASSERT(OFFSET_OF(POLY_GT4, v2) == OFFSET_OF(POLY_GT4, u2) + sizeof(u8) &&
                      OFFSET_OF(POLY_GT4, pad2) == OFFSET_OF(POLY_GT4, u2) + sizeof(u16) &&
                      OFFSET_OF(POLY_GT4, v3) == OFFSET_OF(POLY_GT4, u3) + sizeof(u8) &&
                      OFFSET_OF(POLY_GT4, pad3) == OFFSET_OF(POLY_GT4, u3) + sizeof(u16),
                  model_lighting_gt4_offset_layer_uv_pair_layout);

    MODEL_LIGHTING_UV0_CLUT_WORD(quad)  = elementWords[MODEL_LIGHTING_GT4_OFFSET_LAYER_UV0_CLUT_WORD];
    MODEL_LIGHTING_UV1_TPAGE_WORD(quad) = elementWords[MODEL_LIGHTING_GT4_OFFSET_LAYER_UV1_TPAGE_WORD];
    // Split the packed U/V pairs without overwriting the adjacent pad2/pad3.
    *(u16*)&quad->u2 = (u16)elementWords[MODEL_LIGHTING_GT4_OFFSET_LAYER_UV2_UV3_WORD];
    *(u16*)&quad->u3 = (u16)(elementWords[MODEL_LIGHTING_GT4_OFFSET_LAYER_UV2_UV3_WORD] >> 16);
    // The page sum wraps to u16 before its ABR mode is adjusted.
    quad->tpage += workspace->obj->layerTexturePageOffset;
    // Keep the row byte unsigned until the CLUT calculation restores its sign.
    layerClutRowByte  = workspace->obj->layerClutRowOffset;
    layerTexturePage  = quad->tpage;
    layerTexturePage |= MODEL_LIGHTING_OFFSET_LAYER_TPAGE_ABR_LOW_BIT;
    quad->tpage       = layerTexturePage;
    quad->clut       += (s8)layerClutRowByte << MODEL_LIGHTING_OFFSET_LAYER_CLUT_ROW_SHIFT;
}

/// Initializes one Gouraud textured triangle's texture from a per-corner-colour element.
///
/// `triangle` must be a writable, four-byte-aligned `POLY_GT3`. `elementWords`
/// starts after an opcode `0x130` record's three-word header and must provide
/// at least nine readable, four-byte-aligned u32 words. Words 0..2 pack vertex
/// and normal byte offsets; words 3..5 hold the corners' material colours.
/// Words 6 and 7 pack unsigned byte U/V texel coordinates in their low halves
/// and encoded CLUT and texture-page settings in their high halves. Only the
/// low half of word 8 supplies U2/V2; its upper half is ignored, with no role
/// established here. Nine words is a minimum readable extent, not an element
/// stride or complete record size.
///
/// The construction workspace supplies signed encoded-address displacements:
/// `texturePageOffset` (-128..127) and `encodedClutOffset` (-8192..8128, 64 per
/// palette row). Each sum wraps modulo 65536 in its u16 packet field without
/// changing U/V. The tag, colours/command, screen positions and pad fields are
/// preserved for drawing. All storage is borrowed for this call; no pointer is
/// retained and no workspace cursor or count is changed.
static inline void _modelLightingInitGt3CornerColorsTexture(POLY_GT3* triangle, const u32* elementWords,
                                                            const TmdStreamWorkspace* workspace)
{
    // Word indices within the element, excluding the record header.
    enum {
        MODEL_LIGHTING_GT3_CORNER_COLORS_UV0_CLUT_WORD  = 6, // U0/V0 in low half, CLUT address in high half
        MODEL_LIGHTING_GT3_CORNER_COLORS_UV1_TPAGE_WORD = 7, // U1/V1 in low half, texture-page settings in high half
        MODEL_LIGHTING_GT3_CORNER_COLORS_UV2_WORD       = 8  // U2/V2 in low half; high half is not copied
    };

    MODEL_LIGHTING_UV0_CLUT_WORD(triangle)  = elementWords[MODEL_LIGHTING_GT3_CORNER_COLORS_UV0_CLUT_WORD];
    MODEL_LIGHTING_UV1_TPAGE_WORD(triangle) = elementWords[MODEL_LIGHTING_GT3_CORNER_COLORS_UV1_TPAGE_WORD];
    // Copy the U2/V2 pair without overwriting the SDK's adjacent pad2 field.
    *(u16*)&triangle->u2 = (u16)elementWords[MODEL_LIGHTING_GT3_CORNER_COLORS_UV2_WORD];
    triangle->tpage     += workspace->texturePageOffset;
    triangle->clut      += workspace->encodedClutOffset;
}

/// Initializes a Gouraud textured triangle's persistent texture fields from packed element words.
///
/// `elementWords` is a four-byte-aligned element base, after the stream record's
/// three-word header. `uv0ClutWordIndex` counts u32 words from that base and
/// selects three consecutive readable words in the same element. It must be
/// nonnegative, with index + 2 representable in s32; bounds are not checked.
/// The first two words pack unsigned byte U/V texel coordinates in their low
/// halves and encoded CLUT and texture-page settings in their high halves.
/// Only the last word's low half supplies U2/V2; its high half is ignored.
/// This readable extent does not establish the element's complete size.
///
/// `triangle` must be a writable, four-byte-aligned `POLY_GT3`. A construction
/// workspace supplies signed encoded-address displacements: `texturePageOffset`
/// (-128..127) and `encodedClutOffset` (-8192..8128, 64 per palette row). Sums
/// wrap modulo 65536 in the u16 packet fields without changing U/V. The tag,
/// colours/command, screen positions and pad fields remain untouched for drawing.
/// All storage is borrowed for the call; no pointer is retained and no workspace
/// cursor or count is changed.
static inline void _modelLightingInitGt3TextureWords(POLY_GT3* triangle, const u32* elementWords, s32 uv0ClutWordIndex,
                                                     const TmdStreamWorkspace* workspace)
{
    // Relative word positions in the packed texture suffix, independent of its element prefix.
    enum {
        /// Offset in u32 stream words from a GT3 element's U0/V0/CLUT word to its U1/V1/texture-page word.
        ///
        /// The next four-byte word packs U1 in bits 0..7, V1 in bits 8..15 and
        /// encoded texture-page settings in bits 16..31 on the little-endian
        /// target. This is independent of the element prefix and of the byte
        /// spacing between the destination packet's texture fields. Adding it
        /// to `uv0ClutWordIndex` must fit in s32 and select a readable word in
        /// the same element. The full word is copied before page relocation.
        MODEL_LIGHTING_GT3_UV1_TPAGE_WORD_OFFSET = 1,
        /// Offset in u32 stream words from a GT3 element's U0/V0/CLUT word to its U2/V2 word.
        ///
        /// The word two positions later packs U2 in bits 0..7 and V2 in bits
        /// 8..15 on the little-endian target. Only its low half is copied into
        /// the packet's adjacent u2/v2 bytes, preserving pad2; the source high
        /// half is ignored and its role is unproven. This offset is independent
        /// of the element prefix and destination packet's byte layout. Adding
        /// it to `uv0ClutWordIndex` must fit in s32 and select a readable u32
        /// word in the same element; it does not establish the element's size.
        MODEL_LIGHTING_GT3_UV2_WORD_OFFSET = 2
    };

    MODEL_LIGHTING_UV0_CLUT_WORD(triangle)  = elementWords[uv0ClutWordIndex];
    MODEL_LIGHTING_UV1_TPAGE_WORD(triangle) = elementWords[uv0ClutWordIndex + MODEL_LIGHTING_GT3_UV1_TPAGE_WORD_OFFSET];
    // The halfword view copies the U/V byte pair while preserving the SDK's adjacent pad2.
    *(u16*)&triangle->u2 = (u16)elementWords[uv0ClutWordIndex + MODEL_LIGHTING_GT3_UV2_WORD_OFFSET];
    triangle->tpage     += workspace->texturePageOffset;
    triangle->clut      += workspace->encodedClutOffset;
}

/// Initializes the blended offset-layer texture of one pre-transformed Gouraud triangle.
///
/// `triangle` must be a writable, four-byte-aligned `POLY_GT3`; the caller
/// supplies the first packet of an offset-layer/base pair. `elementWords`
/// starts after an opcode `0x4039` record's three-word header and must provide
/// at least five readable, four-byte-aligned u32 words. Words 0..1 contain
/// depth-cache references and are not read here. Words 2 and 3 pack unsigned
/// U/V texel bytes in their low halves and encoded CLUT and texture-page
/// settings in their high halves. Only word 4's low half supplies U2/V2;
/// its high half is ignored. This minimum extent does not establish a stride.
///
/// `workspace->obj` must be a live object. Its `layerTexturePageOffset` adds
/// -128..127 encoded page units; `layerClutRowOffset` adds -128..127 palette
/// rows, with 64 encoded CLUT units per row. These displacements are independent
/// of the workspace's base-texture offsets. Address sums wrap modulo 65536 in
/// the packet's u16 fields. Setting ABR bit 5 after page relocation preserves
/// bit 6, selecting blend mode 1 or 3; drawing supplies the semitransparent
/// command separately. Tags, colours/command, positions and SDK pad fields
/// remain untouched. All storage is borrowed for the call; no pointer is
/// retained and neither the workspace nor its object is modified.
static inline void _modelLightingInitGt3PreXformOffsetLayerTexture(POLY_GT3* triangle, const u32* elementWords,
                                                                   const TmdStreamWorkspace* workspace)
{
    enum {
        MODEL_LIGHTING_GT3_PRE_XFORM_LAYER_UV0_CLUT_WORD  = 2,      // Packed U0/V0 bytes and encoded CLUT
        MODEL_LIGHTING_GT3_PRE_XFORM_LAYER_UV1_TPAGE_WORD = 3,      // Packed U1/V1 bytes and encoded page settings
        MODEL_LIGHTING_GT3_PRE_XFORM_LAYER_UV2_WORD       = 4,      // Low half is U2/V2; high half is ignored
        MODEL_LIGHTING_OFFSET_LAYER_TPAGE_ABR_LOW_BIT     = 1 << 5, // OR after relocation; retains ABR bit 6 (mode 1 or 3)
        MODEL_LIGHTING_OFFSET_LAYER_CLUT_ROW_SHIFT        = 6       // Signed palette rows to encoded CLUT units (64 per row)
    };
    u32 layerTexturePage;
    u8  layerClutRowByte;

    MODEL_LIGHTING_UV0_CLUT_WORD(triangle)  = elementWords[MODEL_LIGHTING_GT3_PRE_XFORM_LAYER_UV0_CLUT_WORD];
    MODEL_LIGHTING_UV1_TPAGE_WORD(triangle) = elementWords[MODEL_LIGHTING_GT3_PRE_XFORM_LAYER_UV1_TPAGE_WORD];
    // Copy U2/V2 together without overwriting the adjacent pad2.
    *(u16*)&triangle->u2 = elementWords[MODEL_LIGHTING_GT3_PRE_XFORM_LAYER_UV2_WORD];
    // Wrap page relocation before selecting the layer's blend mode.
    triangle->tpage += workspace->obj->layerTexturePageOffset;
    // Keep the row byte unsigned until conversion to signed encoded CLUT units.
    layerClutRowByte  = workspace->obj->layerClutRowOffset;
    layerTexturePage  = triangle->tpage;
    layerTexturePage |= MODEL_LIGHTING_OFFSET_LAYER_TPAGE_ABR_LOW_BIT;
    triangle->tpage   = layerTexturePage;
    triangle->clut   += (s8)layerClutRowByte * (1 << MODEL_LIGHTING_OFFSET_LAYER_CLUT_ROW_SHIFT);
}

u32* func_8009AF90(TmdStreamWorkspace* ws, s32 arg1, u32* arg2)
{
    s32      prev;
    s32      count;
    u32      idx;
    u16*     rec;
    CVECTOR  col;
    u8*      dest;
    u8*      rgb;
    s16*     xy;
    SVECTOR* sv;
    s32      dp;
    s32      flag;
    s32      x;

    col   = gGpColorGrey;
    count = ws->elemCount;
    if (count == 0) {
        return arg2;
    }
    prev          = -1;
    ws->elemCount = count + prev;
    if (count > 0) {
        do {
            rec = (u16*)arg2;
            idx = rec[0];
            if (idx != prev) {
                gte_ldv0((u8*)ws->verts + (idx & 0xFFF8));
                gte_rtps();
                gte_stsz(&ws->gteResult);
                gte_stflg(&ws->gteFlag);
                if (ws->gteFlag & TMD_GTE_ERROR_FLAG) {
                    ws->gteResult |= TMD_VERTEX_DEPTH_INVALID;
                }
                ws->szTable[*(u16*)arg2 >> 3] = ws->gteResult;
            }
            prev = rec[0];
            gte_stsxy(ws->preXformWrite + rec[2] + 4);
            gte_stsxy(ws->preXformWrite + rec[3] + 4);
            gte_stsxy(&ws->texCoord);
            gte_ldv0((u8*)ws->normals + (rec[1] & 0xFFF8));
            gte_ldrgb(&D_80114BA4);
            gte_nccs();
            gte_strgb(ws->preXformWrite + rec[2]);
            gte_ldrgb(&D_80114BA8);
            gte_nccs();
            gte_strgb(ws->preXformWrite + rec[3]);
            gte_rtv0();
            gte_stsv(&ws->elemNormal);
            arg2 += ws->elemStride;
            // Weight the lit layer colour by the object's blend and the grey
            // reference by its complement below the full-colour endpoint.
            dp = ws->obj->shading.colorBlend;
            if (dp < TMD_OBJECT_COLOR_BLEND_ONE) {
                gte_lddp(dp);
                rgb = ws->preXformWrite + rec[2];
                gte_ldcv(rgb);
                gte_gpf12();
                gte_lddp(TMD_OBJECT_COLOR_BLEND_ONE - dp);
                gte_ldcv(&col);
                gte_gpl12();
                gte_stcv(rgb);
            }
            xy   = &ws->texCoord.vx;
            flag = 0;
            dest = ws->preXformWrite + rec[2] + 8;
            sv   = &ws->elemNormal;
            gte_lddp(ws->obj->shading.colorBlend >> 9);
            gte_ldsv(sv);
            gte_gpf12();
            gte_stsv(sv);
            // dest[8]/dest[9] are the U/V pair; dest[3] (dest[-6] once dest has
            // been advanced onto the V byte) is the primitive's code byte, set
            // when U had to be pulled back onto the second texture page. xy
            // steps from the screen X to Y alongside dest.
            x  = *xy + 0xA0;
            x -= sv->vx;
            if (x < 0) {
                x = 0;
            } else if (x >= 0x100) {
                x   -= 0x80;
                flag = 1;
                if (x >= 0xC0) {
                    x = 0xBF;
                }
            }
            *dest = x;
            xy++;
            dest++;
            x  = *xy + 0x78;
            x -= sv->vy;
            if (x < 0) {
                x = 0;
            } else if (x >= 0xF0) {
                x = 0xEF;
            }
            *dest    = x;
            dest[-6] = flag;
        } while (ws->elemCount-- > 0);
    }
    return arg2;
}

u32* gpXformStreamVertsOffsetLayer(TmdStreamWorkspace* ws, s32 flags, u32* stream)
{
    s32     prev;
    s32     count;
    u32     idx;
    u16*    rec;
    CVECTOR col;
    CVECTOR col2;
    u8*     dest;
    s32     val;
    s32     inv;

    col    = gGpColorWhite;
    col2   = gGpColorGrey;
    val    = ws->obj->shading.colorBlend >> 5;
    inv    = (TMD_OBJECT_COLOR_BLEND_ONE >> 5) - val;
    col.b  = val;
    col.g  = val;
    col.r  = val;
    col2.b = inv;
    col2.g = inv;
    col2.r = inv;
    count  = ws->elemCount;
    if (count == 0) {
        return stream;
    }
    prev          = -1;
    ws->elemCount = count + prev;
    if (count > 0) {
        do {
            rec = (u16*)stream;
            idx = rec[0];
            if (idx != prev) {
                gte_ldv0((u8*)ws->verts + (idx & 0xFFF8));
                gte_rtps();
                gte_stsz(&ws->gteResult);
                gte_stflg(&ws->gteFlag);
                if (ws->gteFlag & TMD_GTE_ERROR_FLAG) {
                    ws->gteResult |= TMD_VERTEX_DEPTH_INVALID;
                }
                ws->szTable[*(u16*)stream >> 3] = ws->gteResult;
            }
            prev = rec[0];
            dest = ws->preXformWrite + rec[2] + 4;
            gte_stsxy(dest);
            dest = ws->preXformWrite + rec[3] + 4;
            gte_stsxy(dest);
            gte_stsxy(&ws->texCoord);
            gte_ldv0((u8*)ws->normals + (rec[1] & 0xFFF8));
            gte_ldrgb(&col);
            gte_nccs();
            gte_strgb(ws->preXformWrite + rec[2]);
            gte_ldrgb(&col2);
            gte_nccs();
            gte_strgb(ws->preXformWrite + rec[3]);
            gte_rtv0();
            gte_stsv(&ws->elemNormal);
            stream += ws->elemStride;
        } while (ws->elemCount-- > 0);
    }
    return stream;
}

u32* func_8009B500(TmdStreamWorkspace* ws, s32 arg1, u32* arg2)
{
    POLY_GT3* poly;
    u16*      rec;
    u8*       verts;
    u8*       norms;
    CVECTOR   col;
    SVECTOR*  sv;
    s16*      xy;
    u8*       dest;
    u8*       rgb;
    u8*       pCode;
    u8*       pU;
    s32       combined;
    s32       flag;
    s32       x;
    s32       i;
    s32       len;

    poly = (POLY_GT3*)ws->primWrite;
    col  = gGpColorGrey;
    len  = 9;
    if (ws->elemCount-- > 0) {
        do {
            rec   = (u16*)arg2;
            verts = (u8*)ws->verts;
            gte_ldv3(verts + (rec[0] & 0xFFF8), verts + (rec[1] & 0xFFF8), verts + (rec[2] & 0xFFF8));
            gte_rtpt();
            gte_stflg(&ws->gteFlag);
            if (ws->gteFlag >= 0) {
                gte_nclip();
                gte_stopz(&ws->gteResult);
                if (ws->gteResult > 0) {
                    gte_stsxy3_gt3(&poly[0]);
                    gte_stsxy3_gt3(&poly[1]);
                    gte_avsz3();
                    norms = (u8*)ws->normals;
                    gte_ldv3(norms + (rec[3] & 0xFFF8), norms + (rec[4] & 0xFFF8), norms + (rec[5] & 0xFFF8));
                    gte_ldrgb(&D_80114BA4);
                    gte_ncct();
                    gte_strgb3_gt3(&poly[0]);
                    gte_ldrgb(&D_80114BA8);
                    gte_ncct();
                    gte_strgb3_gt3(&poly[1]);
                    if (ws->obj->shading.colorBlend < TMD_OBJECT_COLOR_BLEND_ONE) {
                        gte_lddp(ws->obj->shading.colorBlend);
                        rgb = &poly[0].r0;
                        gte_ldcv(rgb);
                        gte_gpf12();
                        gte_lddp(TMD_OBJECT_COLOR_BLEND_ONE - ws->obj->shading.colorBlend);
                        gte_ldcv(&col);
                        gte_gpl12();
                        gte_stcv(rgb);
                        gte_lddp(ws->obj->shading.colorBlend);
                        rgb = &poly[0].r1;
                        gte_ldcv(rgb);
                        gte_gpf12();
                        gte_lddp(TMD_OBJECT_COLOR_BLEND_ONE - ws->obj->shading.colorBlend);
                        gte_ldcv(&col);
                        gte_gpl12();
                        gte_stcv(rgb);
                        gte_lddp(ws->obj->shading.colorBlend);
                        rgb = &poly[0].r2;
                        gte_ldcv(rgb);
                        gte_gpf12();
                        gte_lddp(TMD_OBJECT_COLOR_BLEND_ONE - ws->obj->shading.colorBlend);
                        gte_ldcv(&col);
                        gte_gpl12();
                        gte_stcv(rgb);
                    }

                    /* Environment-map UVs: each vertex's rotated normal, scaled by the
                     * blend value, offsets its screen position into the reflection
                     * texture. xy steps from X to Y alongside the U/V destination.
                     * A U past the first page wraps onto the second one and
                     * is flagged in the pad byte after that vertex's colour. */
                    gte_rtv0();
                    gte_stsv(&ws->elemNormal);
                    combined = 0;
                    xy       = &poly[0].x0;
                    dest     = &poly[0].u0;
                    flag     = 0;
                    sv       = &ws->elemNormal;
                    gte_lddp(ws->obj->shading.colorBlend >> 9);
                    gte_ldsv(sv);
                    gte_gpf12();
                    gte_stsv(sv);
                    x  = *xy + 0xA0;
                    x -= sv->vx;
                    if (x < 0) {
                        x = 0;
                    } else if (x >= 0x100) {
                        x   -= 0x80;
                        flag = 1;
                        if (x >= 0xC0) {
                            x = 0xBF;
                        }
                    }
                    *dest = x;
                    xy++;
                    dest++;
                    x  = *xy + 0x78;
                    x -= sv->vy;
                    if (x < 0) {
                        x = 0;
                    } else if (x >= 0xF0) {
                        x = 0xEF;
                    }
                    combined |= flag;
                    *dest     = x;
                    dest[-6]  = flag;

                    gte_rtv1();
                    gte_stsv(&ws->elemNormal);
                    xy   = &poly[0].x1;
                    dest = &poly[0].u1;
                    flag = 0;
                    sv   = &ws->elemNormal;
                    gte_lddp(ws->obj->shading.colorBlend >> 9);
                    gte_ldsv(sv);
                    gte_gpf12();
                    gte_stsv(sv);
                    x  = *xy + 0xA0;
                    x -= sv->vx;
                    if (x < 0) {
                        x = 0;
                    } else if (x >= 0x100) {
                        x   -= 0x80;
                        flag = 1;
                        if (x >= 0xC0) {
                            x = 0xBF;
                        }
                    }
                    *dest = x;
                    xy++;
                    dest++;
                    x  = *xy + 0x78;
                    x -= sv->vy;
                    if (x < 0) {
                        x = 0;
                    } else if (x >= 0xF0) {
                        x = 0xEF;
                    }
                    combined |= flag;
                    *dest     = x;
                    dest[-6]  = flag;

                    gte_rtv2();
                    gte_stsv(&ws->elemNormal);
                    xy   = &poly[0].x2;
                    dest = &poly[0].u2;
                    flag = 0;
                    sv   = &ws->elemNormal;
                    gte_lddp(ws->obj->shading.colorBlend >> 9);
                    gte_ldsv(sv);
                    gte_gpf12();
                    gte_stsv(sv);
                    x  = *xy + 0xA0;
                    x -= sv->vx;
                    if (x < 0) {
                        x = 0;
                    } else if (x >= 0x100) {
                        x   -= 0x80;
                        flag = 1;
                        if (x >= 0xC0) {
                            x = 0xBF;
                        }
                    }
                    *dest = x;
                    xy++;
                    dest++;
                    x  = *xy + 0x78;
                    x -= sv->vy;
                    if (x < 0) {
                        x = 0;
                    } else if (x >= 0xF0) {
                        x = 0xEF;
                    }
                    combined |= flag;
                    *dest     = x;
                    dest[-6]  = flag;

                    if (combined == 0) {
                        poly[0].tpage = 0x137;
                    } else {
                        /* The primitive samples the second page: a vertex that
                         * did not wrap is shifted back 0x80, or clamped to 0. */
                        pCode = &poly[0].code;
                        i     = 0;
                        pU    = &poly[0].u0;
                        do {
                            if (*pCode == 0) {
                                if ((s8)*pU < 0) {
                                    *pU = *pU + 0x80;
                                } else {
                                    *pU = 0;
                                }
                            }
                            pU += 0xC;
                            i++;
                            pCode += 0xC;
                        } while (i < 3);
                        poly[0].tpage = 0x139;
                    }
                    setlen(&poly[0], len);
                    setcode(&poly[0], 0x36);
                    setlen(&poly[1], len);
                    setcode(&poly[1], 0x34);
                    gte_stotz(&ws->gteResult);
                    addPrim((&ws->ot[(((((u32)ws->gteResult << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)) / sizeof(*ws->ot)]), &poly[0]);
                    addPrim((&ws->ot[(((((u32)ws->gteResult << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)) / sizeof(*ws->ot)]), &poly[1]);
                }
            }
            poly += 2;
            arg2 += ws->elemStride;
        } while (ws->elemCount-- > 0);
    }
    ws->primWrite = (u8*)poly;
    return arg2;
}

u32* gpDrawStreamPrimGt3OffsetLayer(TmdStreamWorkspace* ws, s32 flags, u32* stream)
{
    POLY_GT3*     poly;
    s32*          opz;
    DisplayState* ds;
    u32           mask;
    u32           maskHi;
    u16*          rec;
    u8*           verts;
    u8*           norms;
    CVECTOR       col;
    CVECTOR       col2;
    s32           len;
    s32           code;
    s32           val;
    s32           inv;

    poly   = (POLY_GT3*)ws->primWrite;
    col    = gGpColorGrey;
    col2   = gGpColorGrey;
    val    = ws->obj->shading.colorBlend >> 5;
    inv    = (TMD_OBJECT_COLOR_BLEND_ONE >> 5) - val;
    col.b  = val;
    col.g  = val;
    col.r  = val;
    col2.b = inv;
    col2.g = inv;
    col2.r = inv;
    if (ws->elemCount-- > 0) {
        opz    = &ws->gteResult;
        len    = 9;
        code   = 0x34;
        ds     = &gDisplayState;
        mask   = 0xFFFFFF;
        maskHi = 0xFF000000;
        do {
            rec   = (u16*)stream;
            verts = (u8*)ws->verts;
            gte_ldv3(verts + (rec[0] & 0xFFF8), verts + (rec[1] & 0xFFF8), verts + (rec[2] & 0xFFF8));
            gte_rtpt();
            gte_stflg(&ws->gteFlag);
            if (ws->gteFlag >= 0) {
                gte_nclip();
                gte_stopz(opz);
                if (ws->gteResult > 0) {
                    gte_stsxy3_gt3(&poly[0]);
                    gte_stsxy3_gt3(&poly[1]);
                    gte_avsz3();
                    norms = (u8*)ws->normals;
                    gte_ldv3(norms + (rec[3] & 0xFFF8), norms + (rec[4] & 0xFFF8), norms + (rec[5] & 0xFFF8));
                    gte_ldrgb(&col);
                    gte_ncct();
                    gte_strgb3_gt3(&poly[0]);
                    gte_ldrgb(&col2);
                    gte_ncct();
                    gte_strgb3_gt3(&poly[1]);
                    setlen(&poly[0], len);
                    setcode(&poly[0], 0x36);
                    setlen(&poly[1], len);
                    setcode(&poly[1], code);
                    poly[0].tpage |= 0x20;
                    gte_stotz(opz);
                    poly[0].tag = (poly[0].tag & maskHi) | (*(&ws->ot[(((((u32)ws->gteResult << ds->otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)) / sizeof(*ws->ot)]) & mask);
                    *(&ws->ot[(((((u32)ws->gteResult << ds->otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)) / sizeof(*ws->ot)]) =
                        (*(&ws->ot[(((((u32)ws->gteResult << ds->otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)) / sizeof(*ws->ot)]) & maskHi) | ((u32)&poly[0] & mask);
                    poly[1].tag = (poly[1].tag & maskHi) | (*(&ws->ot[(((((u32)ws->gteResult << ds->otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)) / sizeof(*ws->ot)]) & mask);
                    *(&ws->ot[(((((u32)ws->gteResult << ds->otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)) / sizeof(*ws->ot)]) =
                        (*(&ws->ot[(((((u32)ws->gteResult << ds->otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)) / sizeof(*ws->ot)]) & maskHi) | ((u32)&poly[1] & mask);
                }
            }
            poly   += 2;
            stream += ws->elemStride;
        } while (ws->elemCount-- > 0);
    }
    ws->primWrite = (u8*)poly;
    return stream;
}

u32* gpDrawStreamPrimGt4OffsetLayer(TmdStreamWorkspace* ws, s32 flags, u32* stream)
{
    POLY_GT4*     poly;
    s32*          opz;
    DisplayState* ds;
    u32           mask;
    u32           maskHi;
    u32           clipMask;
    s32*          flg;
    u16*          rec;
    u8*           verts;
    u8*           norms;
    CVECTOR       col;
    CVECTOR       col2;
    s32           len;
    s32           code;
    s32           val;
    s32           inv;

    poly   = (POLY_GT4*)ws->primWrite;
    col    = gGpColorGrey;
    col2   = gGpColorGrey;
    val    = ws->obj->shading.colorBlend >> 5;
    inv    = (TMD_OBJECT_COLOR_BLEND_ONE >> 5) - val;
    col.b  = val;
    col.g  = val;
    col.r  = val;
    col2.b = inv;
    col2.g = inv;
    col2.r = inv;
    if (ws->elemCount-- > 0) {
        flg      = &ws->gteFlag;
        clipMask = TMD_GTE_ERROR_FLAG;
        opz      = &ws->gteResult;
        do {
            rec   = (u16*)stream;
            verts = (u8*)ws->verts;
            gte_ldv3(verts + (rec[0] & 0xFFF8), verts + (rec[1] & 0xFFF8), verts + (rec[2] & 0xFFF8));
            gte_rtpt();
            gte_stflg(flg);
            if ((ws->gteFlag & clipMask) == 0) {
                gte_nclip();
                gte_stopz(opz);
                gte_stsxy3_gt4(&poly[0]);
                gte_stsxy3_gt4(&poly[1]);
                gte_ldv0((u8*)ws->verts + (rec[3] & 0xFFF8));
                gte_rtps();
                gte_stflg(flg);
                if ((ws->gteFlag & clipMask) == 0) {
                    if (ws->gteResult > 0) {
                        goto draw;
                    }
                    gte_nclip();
                    gte_stopz(opz);
                    if (ws->gteResult < 0) {
                    draw:
                        gte_stsxy2(&poly[0].x3);
                        gte_stsxy2(&poly[1].x3);
                        gte_avsz4();
                        norms = (u8*)ws->normals;
                        gte_ldv3(norms + (rec[4] & 0xFFF8), norms + (rec[5] & 0xFFF8), norms + (rec[6] & 0xFFF8));
                        gte_ldrgb(&col);
                        gte_ncct();
                        gte_strgb3_gt4(&poly[0]);
                        gte_ldrgb(&col2);
                        gte_ncct();
                        gte_strgb3_gt4(&poly[1]);
                        gte_ldv0((u8*)ws->normals + (rec[7] & 0xFFF8));
                        gte_ldrgb(&col);
                        gte_nccs();
                        gte_strgb(&poly[0].r3);
                        gte_ldrgb(&col2);
                        gte_nccs();
                        gte_strgb(&poly[1].r3);
                        len    = 0xC;
                        code   = 0x3C;
                        ds     = &gDisplayState;
                        mask   = 0xFFFFFF;
                        maskHi = 0xFF000000;
                        setlen(&poly[0], len);
                        setcode(&poly[0], 0x3E);
                        setlen(&poly[1], len);
                        setcode(&poly[1], code);
                        gte_stotz(opz);
                        poly[0].tag = (poly[0].tag & maskHi) | (*(&ws->ot[(((((u32)ws->gteResult << ds->otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)) / sizeof(*ws->ot)]) & mask);
                        *(&ws->ot[(((((u32)ws->gteResult << ds->otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)) / sizeof(*ws->ot)]) =
                            (*(&ws->ot[(((((u32)ws->gteResult << ds->otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)) / sizeof(*ws->ot)]) & maskHi) | ((u32)&poly[0] & mask);
                        poly[1].tag = (poly[1].tag & maskHi) | (*(&ws->ot[(((((u32)ws->gteResult << ds->otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)) / sizeof(*ws->ot)]) & mask);
                        *(&ws->ot[(((((u32)ws->gteResult << ds->otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)) / sizeof(*ws->ot)]) =
                            (*(&ws->ot[(((((u32)ws->gteResult << ds->otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)) / sizeof(*ws->ot)]) & maskHi) | ((u32)&poly[1] & mask);
                    }
                }
            }
            poly   += 2;
            stream += ws->elemStride;
        } while (ws->elemCount-- > 0);
    }
    ws->primWrite = (u8*)poly;
    return stream;
}

u32* func_8009C414(TmdStreamWorkspace* ws, s32 arg1, u32* arg2)
{
    POLY_GT4* poly;
    s32*      opz;
    s32*      flg;
    u16*      rec;
    u8*       verts;
    u8*       norms;
    CVECTOR   col;
    SVECTOR*  sv;
    s16*      xy;
    s16*      xy3;
    u8*       rgb3;
    u8*       dest;
    u8*       uv;
    u8*       rgb;
    u8*       codep;
    s32       flag;
    s32       anyflag;
    s32       x;
    s32       i;
    s32       len;

    poly = (POLY_GT4*)ws->primWrite;
    col  = gGpColorGrey;
    if (ws->elemCount-- > 0) {
        flg = &ws->gteFlag;
        opz = &ws->gteResult;
        do {
            rec   = (u16*)arg2;
            verts = (u8*)ws->verts;
            gte_ldv3(verts + (rec[0] & 0xFFF8), verts + (rec[1] & 0xFFF8), verts + (rec[2] & 0xFFF8));
            gte_rtpt();
            gte_stflg(flg);
            if ((ws->gteFlag & TMD_GTE_ERROR_FLAG) == 0) {
                gte_nclip();
                gte_stopz(opz);
                gte_stsxy3_gt4(&poly[0]);
                gte_stsxy3_gt4(&poly[1]);
                gte_ldv0((u8*)ws->verts + (rec[3] & 0xFFF8));
                gte_rtps();
                gte_stflg(flg);
                if ((ws->gteFlag & TMD_GTE_ERROR_FLAG) == 0) {
                    gte_nclip();
                    gte_stsxy2(&poly[0].x3);
                    gte_stsxy2(&poly[1].x3);
                    if (ws->gteResult <= 0) {
                        *(u_long*)&poly[0].x0 = *(u_long*)&poly[0].x1;
                        *(u_long*)&poly[1].x0 = *(u_long*)&poly[1].x1;
                        gte_stopz(opz);
                        if (ws->gteResult >= 0) {
                            goto skip;
                        }
                    } else {
                        gte_stopz(opz);
                        if (ws->gteResult >= 0) {
                            *(u_long*)&poly[0].x3 = *(u_long*)&poly[0].x2;
                            *(u_long*)&poly[1].x3 = *(u_long*)&poly[1].x2;
                        }
                    }
                    gte_avsz4();
                    norms = (u8*)ws->normals;
                    gte_ldv3(norms + (rec[4] & 0xFFF8), norms + (rec[5] & 0xFFF8), norms + (rec[6] & 0xFFF8));
                    gte_ldrgb(&D_80114BA4);
                    gte_ncct();
                    gte_strgb3_gt4(&poly[0]);
                    gte_ldrgb(&D_80114BA8);
                    gte_ncct();
                    gte_strgb3_gt4(&poly[1]);

                    /* Environment-map UVs: each vertex's rotated normal, scaled by the
                     * blend value, offsets its screen position into the reflection
                     * texture. xy steps from X to Y while dest steps from U to V, then
                     * back to the pad byte after that vertex's colour, which records
                     * whether U wrapped onto the second texture page. */

                    /* vertex 0 */
                    gte_rtv0();
                    gte_stsv(&ws->elemNormal);
                    anyflag = 0;
                    xy      = &poly[0].x0;
                    dest    = &poly[0].u0;
                    flag    = 0;
                    sv      = &ws->elemNormal;
                    gte_lddp(ws->obj->shading.colorBlend >> 9);
                    gte_ldsv(sv);
                    gte_gpf12();
                    gte_stsv(sv);
                    x  = *xy + 0xA0;
                    x -= sv->vx;
                    if (x < 0) {
                        x = 0;
                    } else if (x >= 0x100) {
                        x   -= 0x80;
                        flag = 1;
                        if (x >= 0xC0) {
                            x = 0xBF;
                        }
                    }
                    *dest = x;
                    xy++;
                    dest++;
                    x  = *xy + 0x78;
                    x -= sv->vy;
                    if (x < 0) {
                        x = 0;
                    } else if (x >= 0xF0) {
                        x = 0xEF;
                    }
                    *dest    = x;
                    dest    -= 6;
                    *dest    = flag;
                    anyflag |= flag;

                    /* vertex 1 */
                    gte_rtv1();
                    gte_stsv(&ws->elemNormal);
                    xy   = &poly[0].x1;
                    dest = &poly[0].u1;
                    flag = 0;
                    sv   = &ws->elemNormal;
                    gte_lddp(ws->obj->shading.colorBlend >> 9);
                    gte_ldsv(sv);
                    gte_gpf12();
                    gte_stsv(sv);
                    x  = *xy + 0xA0;
                    x -= sv->vx;
                    if (x < 0) {
                        x = 0;
                    } else if (x >= 0x100) {
                        x   -= 0x80;
                        flag = 1;
                        if (x >= 0xC0) {
                            x = 0xBF;
                        }
                    }
                    *dest = x;
                    xy++;
                    dest++;
                    x  = *xy + 0x78;
                    x -= sv->vy;
                    if (x < 0) {
                        x = 0;
                    } else if (x >= 0xF0) {
                        x = 0xEF;
                    }
                    *dest    = x;
                    dest    -= 6;
                    *dest    = flag;
                    anyflag |= flag;

                    /* vertex 2 */
                    gte_rtv2();
                    gte_stsv(&ws->elemNormal);
                    xy   = &poly[0].x2;
                    dest = &poly[0].u2;
                    flag = 0;
                    sv   = &ws->elemNormal;
                    gte_lddp(ws->obj->shading.colorBlend >> 9);
                    gte_ldsv(sv);
                    gte_gpf12();
                    gte_stsv(sv);
                    x  = *xy + 0xA0;
                    x -= sv->vx;
                    if (x < 0) {
                        x = 0;
                    } else if (x >= 0x100) {
                        x   -= 0x80;
                        flag = 1;
                        if (x >= 0xC0) {
                            x = 0xBF;
                        }
                    }
                    *dest = x;
                    xy++;
                    dest++;
                    x  = *xy + 0x78;
                    x -= sv->vy;
                    if (x < 0) {
                        x = 0;
                    } else if (x >= 0xF0) {
                        x = 0xEF;
                    }
                    *dest    = x;
                    dest    -= 6;
                    *dest    = flag;
                    anyflag |= flag;

                    /* vertex 3 */
                    xy3 = &poly[0].x3;
                    gte_ldv0((u8*)ws->normals + (rec[7] & 0xFFF8));
                    gte_ldrgb(&D_80114BA4);
                    gte_nccs();
                    gte_strgb(&poly[0].r3);
                    gte_ldrgb(&D_80114BA8);
                    gte_nccs();
                    gte_strgb(&poly[1].r3);
                    gte_rtv0();
                    gte_stsv(&ws->elemNormal);
                    xy   = xy3;
                    dest = &poly[0].u3;
                    flag = 0;
                    sv   = &ws->elemNormal;
                    gte_lddp(ws->obj->shading.colorBlend >> 9);
                    gte_ldsv(sv);
                    gte_gpf12();
                    gte_stsv(sv);
                    x  = *xy + 0xA0;
                    x -= sv->vx;
                    if (x < 0) {
                        x = 0;
                    } else if (x >= 0x100) {
                        x   -= 0x80;
                        flag = 1;
                        if (x >= 0xC0) {
                            x = 0xBF;
                        }
                    }
                    *dest = x;
                    xy++;
                    dest++;
                    x  = *xy + 0x78;
                    x -= sv->vy;
                    if (x < 0) {
                        x = 0;
                    } else if (x >= 0xF0) {
                        x = 0xEF;
                    }
                    *dest = x;
                    dest -= 6;
                    *dest = flag;

                    anyflag |= flag;
                    if (ws->obj->shading.colorBlend < TMD_OBJECT_COLOR_BLEND_ONE) {
                        gte_lddp(ws->obj->shading.colorBlend);
                        rgb = &poly[0].r0;
                        gte_ldcv(rgb);
                        gte_gpf12();
                        gte_lddp(TMD_OBJECT_COLOR_BLEND_ONE - ws->obj->shading.colorBlend);
                        gte_ldcv(&col);
                        gte_gpl12();
                        gte_stcv(rgb);

                        gte_lddp(ws->obj->shading.colorBlend);
                        rgb = &poly[0].r1;
                        gte_ldcv(rgb);
                        gte_gpf12();
                        gte_lddp(TMD_OBJECT_COLOR_BLEND_ONE - ws->obj->shading.colorBlend);
                        gte_ldcv(&col);
                        gte_gpl12();
                        gte_stcv(rgb);

                        gte_lddp(ws->obj->shading.colorBlend);
                        rgb = &poly[0].r2;
                        gte_ldcv(rgb);
                        gte_gpf12();
                        gte_lddp(TMD_OBJECT_COLOR_BLEND_ONE - ws->obj->shading.colorBlend);
                        gte_ldcv(&col);
                        gte_gpl12();
                        gte_stcv(rgb);

                        gte_lddp(ws->obj->shading.colorBlend);
                        rgb3 = &poly[0].r3;
                        gte_ldcv(rgb3);
                        gte_gpf12();
                        gte_lddp(TMD_OBJECT_COLOR_BLEND_ONE - ws->obj->shading.colorBlend);
                        gte_ldcv(&col);
                        gte_gpl12();
                        gte_stcv(rgb3);
                    }

                    codep = &poly[0].code;
                    if (anyflag == 0) {
                        poly[0].tpage = 0x137;
                    } else {
                        i  = 0;
                        uv = &poly[0].u0;
                        do {
                            if (*codep == 0) {
                                if ((s8)*uv < 0) {
                                    *uv = *uv + 0x80;
                                } else {
                                    *uv = 0;
                                }
                            }
                            uv    += 0xC;
                            i     += 1;
                            codep += 0xC;
                        } while (i < 4);
                        poly[0].tpage = 0x139;
                    }

                    len = 0xC;
                    setlen(&poly[0], len);
                    setcode(&poly[0], 0x3E);
                    setlen(&poly[1], len);
                    setcode(&poly[1], 0x3C);
                    gte_stotz(opz);
                    addPrim((&ws->ot[(((((u32)ws->gteResult << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)) / sizeof(*ws->ot)]), &poly[0]);
                    addPrim((&ws->ot[(((((u32)ws->gteResult << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)) / sizeof(*ws->ot)]), &poly[1]);
                }
            }
        skip:
            poly += 2;
            arg2 += ws->elemStride;
        } while (ws->elemCount-- > 0);
    }
    ws->primWrite = (u8*)poly;
    return arg2;
}

u32* gpDrawStreamPrimGt3ElemColor(TmdStreamWorkspace* ws, s32 flags, u32* stream)
{
    POLY_GT3*     poly;
    s32*          opz;
    DisplayState* ds;
    u16*          rec;
    u8*           verts;
    u8*           norms;

    poly = (POLY_GT3*)ws->primWrite;
    if (ws->elemCount-- > 0) {
        opz = &ws->gteResult;
        ds  = &gDisplayState;
        do {
            rec   = (u16*)stream;
            verts = (u8*)ws->verts;
            gte_ldv3(verts + (rec[0] & 0xFFF8), verts + (rec[1] & 0xFFF8), verts + (rec[2] & 0xFFF8));
            gte_rtpt();
            gte_stflg(&ws->gteFlag);
            if (ws->gteFlag >= 0) {
                gte_nclip();
                gte_stopz(opz);
                if (ws->gteResult > 0) {
                    gte_ldrgb(stream + 3);
                    gte_stsxy3_gt3(poly);
                    gte_avsz3();
                    norms = (u8*)ws->normals;
                    gte_ldv3(norms + (rec[3] & 0xFFF8), norms + (rec[4] & 0xFFF8), norms + (rec[5] & 0xFFF8));
                    gte_ncct();
                    gte_strgb3_gt3(poly);
                    setlen(poly, 9);
                    setcode(poly, 0x34);
                    if (ws->obj->flags & TMD_OBJECT_SEMI_TRANS) {
                        setcode(poly, 0x36);
                    }
                    gte_stotz(opz);
                    addPrim(&ws->ot[((u32)ws->gteResult << ds->otDepthShift) >> 4 & 0x3FF], poly);
                }
            }
            poly++;
            stream += ws->elemStride;
        } while (ws->elemCount-- > 0);
    }
    ws->primWrite = (u8*)poly;
    return stream;
}

u32* gpDrawStreamPrimGt4ElemColor(TmdStreamWorkspace* ws, s32 flags, u32* stream)
{
    POLY_GT4*     poly;
    s32*          opz;
    DisplayState* ds;
    u32           clipMask;
    s32*          flg;
    u16*          rec;
    u8*           verts;
    u8*           norms;

    poly = (POLY_GT4*)ws->primWrite;
    if (ws->elemCount-- > 0) {
        flg      = &ws->gteFlag;
        clipMask = TMD_GTE_ERROR_FLAG;
        opz      = &ws->gteResult;
        ds       = &gDisplayState;
        do {
            rec   = (u16*)stream;
            verts = (u8*)ws->verts;
            gte_ldv3(verts + (rec[0] & 0xFFF8), verts + (rec[1] & 0xFFF8), verts + (rec[2] & 0xFFF8));
            gte_rtpt();
            gte_stflg(flg);
            if ((ws->gteFlag & clipMask) == 0) {
                gte_nclip();
                gte_stopz(opz);
                gte_ldrgb(stream + 4);
                gte_stsxy3_gt4(poly);
                gte_ldv0((u8*)ws->verts + (rec[3] & 0xFFF8));
                gte_rtps();
                gte_stflg(flg);
                if ((ws->gteFlag & clipMask) == 0) {
                    if (ws->gteResult > 0) {
                        goto draw;
                    }
                    gte_nclip();
                    gte_stopz(opz);
                    if (ws->gteResult < 0) {
                    draw:
                        gte_stsxy2(&poly->x3);
                        gte_avsz4();
                        norms = (u8*)ws->normals;
                        gte_ldv3(norms + (rec[4] & 0xFFF8), norms + (rec[5] & 0xFFF8), norms + (rec[6] & 0xFFF8));
                        gte_ncct();
                        gte_strgb3_gt4(poly);
                        gte_ldv0((u8*)ws->normals + (rec[7] & 0xFFF8));
                        gte_nccs();
                        gte_strgb(&poly->r3);
                        setlen(poly, 0xC);
                        setcode(poly, 0x3C);
                        if (ws->obj->flags & TMD_OBJECT_SEMI_TRANS) {
                            setcode(poly, 0x3E);
                        }
                        gte_stotz(opz);
                        addPrim(&ws->ot[((u32)ws->gteResult << ds->otDepthShift) >> 4 & 0x3FF], poly);
                    }
                }
            }
            poly++;
            stream += ws->elemStride;
        } while (ws->elemCount-- > 0);
    }
    ws->primWrite = (u8*)poly;
    return stream;
}

u32* func_8009D388(TmdStreamWorkspace* arg0, s32 arg1, u32* arg2)
{
    TmdStreamWorkspace* ws;
    POLY_FT3*           poly;
    s32*                opz;
    DisplayState*       ds;
    u16*                rec;
    u8*                 verts;

    ws   = arg0;
    poly = (POLY_FT3*)ws->primWrite;
    if (ws->elemCount-- > 0) {
        opz = &ws->gteResult;
        ds  = &gDisplayState;
        do {
            rec   = (u16*)arg2;
            verts = (u8*)ws->verts;
            gte_ldv3(verts + (rec[0] & 0xFFF8), verts + (rec[1] & 0xFFF8), verts + (rec[2] & 0xFFF8));
            gte_rtpt();
            gte_stflg(&ws->gteFlag);
            if (ws->gteFlag >= 0) {
                gte_nclip();
                gte_stopz(opz);
                if (ws->gteResult > 0) {
                    gte_stsxy3_ft3(poly);
                    gte_avsz3();
                    setlen(poly, 7);
                    setcode(poly, 0x25);
                    gte_stotz(opz);
                    addPrim(&ws->ot[((u32)ws->gteResult << ds->otDepthShift) >> 4 & 0x3FF], poly);
                }
            }
            poly++;
            arg2 += ws->elemStride;
        } while (ws->elemCount-- > 0);
    }
    ws->primWrite = (u8*)poly;
    return arg2;
}

u32* func_8009D518(TmdStreamWorkspace* arg0, s32 arg1, u32* arg2)
{
    TmdStreamWorkspace* ws;
    POLY_FT4*           poly;
    s32*                opz;
    DisplayState*       ds;
    u32                 clipMask;
    s32*                flg;
    u16*                rec;
    u8*                 verts;

    ws   = arg0;
    poly = (POLY_FT4*)ws->primWrite;
    if (ws->elemCount-- > 0) {
        flg      = &ws->gteFlag;
        clipMask = TMD_GTE_ERROR_FLAG;
        opz      = &ws->gteResult;
        ds       = &gDisplayState;
        do {
            rec   = (u16*)arg2;
            verts = (u8*)ws->verts;
            gte_ldv3(verts + (rec[0] & 0xFFF8), verts + (rec[1] & 0xFFF8), verts + (rec[2] & 0xFFF8));
            gte_rtpt();
            gte_stflg(flg);
            if ((ws->gteFlag & clipMask) == 0) {
                gte_nclip();
                gte_stopz(opz);
                gte_stsxy3_ft4(poly);
                gte_ldv0((u8*)ws->verts + (rec[3] & 0xFFF8));
                gte_rtps();
                gte_stflg(flg);
                if ((ws->gteFlag & clipMask) == 0) {
                    if (ws->gteResult > 0) {
                        goto draw;
                    }
                    gte_nclip();
                    gte_stopz(opz);
                    if (ws->gteResult < 0) {
                    draw:
                        gte_stsxy2(&poly->x3);
                        gte_avsz4();
                        setlen(poly, 9);
                        setcode(poly, 0x2D);
                        gte_stotz(opz);
                        addPrim(&ws->ot[((u32)ws->gteResult << ds->otDepthShift) >> 4 & 0x3FF], poly);
                    }
                }
            }
            poly++;
            arg2 += ws->elemStride;
        } while (ws->elemCount-- > 0);
    }
    ws->primWrite = (u8*)poly;
    return arg2;
}

u32* func_8009D718(TmdStreamWorkspace* arg0, s32 arg1, u32* arg2)
{
    TmdStreamWorkspace* ws;
    POLY_GT4*           poly;
    s32*                opz;
    DisplayState*       ds;
    u32                 clipMask;
    s32*                flg;
    u16*                rec;
    u8*                 verts;

    ws   = arg0;
    poly = (POLY_GT4*)ws->primWrite;
    if (ws->elemCount-- > 0) {
        flg      = &ws->gteFlag;
        clipMask = TMD_GTE_ERROR_FLAG;
        opz      = &ws->gteResult;
        ds       = &gDisplayState;
        do {
            rec   = (u16*)arg2;
            verts = (u8*)ws->verts;
            gte_ldv3(verts + (rec[0] & 0xFFF8), verts + (rec[1] & 0xFFF8), verts + (rec[2] & 0xFFF8));
            gte_rtpt();
            gte_stflg(flg);
            if ((ws->gteFlag & clipMask) == 0) {
                gte_nclip();
                gte_stopz(opz);
                gte_stsxy3_gt4(poly);
                gte_ldv0((u8*)ws->verts + (rec[3] & 0xFFF8));
                gte_rtps();
                gte_stflg(flg);
                if ((ws->gteFlag & clipMask) == 0) {
                    if (ws->gteResult > 0) {
                        goto draw;
                    }
                    gte_nclip();
                    gte_stopz(opz);
                    if (ws->gteResult < 0) {
                    draw:
                        gte_stsxy2(&poly->x3);
                        gte_avsz4();
                        gte_stotz(opz);
                        addPrim(&ws->ot[((u32)ws->gteResult << ds->otDepthShift) >> 4 & 0x3FF], poly);
                    }
                }
            }
            poly++;
            arg2 += ws->elemStride;
        } while (ws->elemCount-- > 0);
    }
    ws->primWrite = (u8*)poly;
    return arg2;
}

u32* func_8009D900(TmdStreamWorkspace* arg0, s32 arg1, u32* arg2)
{
    TmdStreamWorkspace* ws;
    POLY_F4*            poly;
    s32*                opz;
    DisplayState*       ds;
    u32                 clipMask;
    s32*                flg;
    u16*                rec;
    u8*                 verts;

    ws   = arg0;
    poly = (POLY_F4*)ws->primWrite;
    if (ws->elemCount-- > 0) {
        flg      = &ws->gteFlag;
        clipMask = TMD_GTE_ERROR_FLAG;
        opz      = &ws->gteResult;
        ds       = &gDisplayState;
        do {
            rec   = (u16*)arg2;
            verts = (u8*)ws->verts;
            gte_ldv3(verts + (rec[0] & 0xFFF8), verts + (rec[1] & 0xFFF8), verts + (rec[2] & 0xFFF8));
            gte_rtpt();
            gte_stflg(flg);
            if ((ws->gteFlag & clipMask) == 0) {
                gte_nclip();
                gte_stopz(opz);
                gte_stsxy3_f4(poly);
                gte_ldv0((u8*)ws->verts + (rec[3] & 0xFFF8));
                gte_rtps();
                gte_stflg(flg);
                if ((ws->gteFlag & clipMask) == 0) {
                    if (ws->gteResult > 0) {
                        goto draw;
                    }
                    gte_nclip();
                    gte_stopz(opz);
                    if (ws->gteResult < 0) {
                    draw:
                        gte_stsxy2(&poly->x3);
                        gte_avsz4();
                        setlen(poly, 5);
                        setcode(poly, 0x28);
                        gte_stotz(opz);
                        addPrim(&ws->ot[((u32)ws->gteResult << ds->otDepthShift) >> 4 & 0x3FF], poly);
                    }
                }
            }
            poly++;
            arg2 += ws->elemStride;
        } while (ws->elemCount-- > 0);
    }
    ws->primWrite = (u8*)poly;
    return arg2;
}

u32* func_8009DB00(TmdStreamWorkspace* arg0, s32 arg1, u32* arg2)
{
    TmdStreamWorkspace* ws;
    POLY_F3*            poly;
    s32*                opz;
    DisplayState*       ds;
    u32                 clipMask;
    s32*                flg;
    u16*                rec;
    u8*                 verts;

    ws   = arg0;
    poly = (POLY_F3*)ws->primWrite;
    if (ws->elemCount-- > 0) {
        flg      = &ws->gteFlag;
        clipMask = TMD_GTE_ERROR_FLAG;
        opz      = &ws->gteResult;
        ds       = &gDisplayState;
        do {
            rec   = (u16*)arg2;
            verts = (u8*)ws->verts;
            gte_ldv3(verts + (rec[0] & 0xFFF8), verts + (rec[1] & 0xFFF8), verts + (rec[2] & 0xFFF8));
            gte_rtpt();
            gte_stflg(flg);
            if ((ws->gteFlag & clipMask) == 0) {
                gte_nclip();
                gte_stopz(opz);
                if (ws->gteResult > 0) {
                    gte_stsxy3_f3(poly);
                    gte_stflg(flg);
                    if ((ws->gteFlag & clipMask) == 0) {
                        gte_avsz3();
                        setlen(poly, 4);
                        setcode(poly, 0x20);
                        gte_stotz(opz);
                        addPrim(&ws->ot[((u32)ws->gteResult << ds->otDepthShift) >> 4 & 0x3FF], poly);
                    }
                }
            }
            poly++;
            arg2 += ws->elemStride;
        } while (ws->elemCount-- > 0);
    }
    ws->primWrite = (u8*)poly;
    return arg2;
}

u32* func_8009DCB8(TmdStreamWorkspace* arg0, s32 arg1, u32* arg2)
{
    TmdStreamWorkspace* ws;
    POLY_FT3*           poly;
    s32*                opz;
    DisplayState*       ds;
    u16*                rec;
    u8*                 verts;

    ws   = arg0;
    poly = (POLY_FT3*)ws->primWrite;
    if (ws->elemCount-- > 0) {
        opz = &ws->gteResult;
        ds  = &gDisplayState;
        do {
            rec   = (u16*)arg2;
            verts = (u8*)ws->verts;
            gte_ldv3(verts + (rec[0] & 0xFFF8), verts + (rec[1] & 0xFFF8), verts + (rec[2] & 0xFFF8));
            gte_rtpt();
            gte_stflg(&ws->gteFlag);
            if (ws->gteFlag >= 0) {
                gte_nclip();
                gte_stopz(opz);
                if (ws->gteResult > 0) {
                    gte_stsxy3_ft3(poly);
                    gte_avsz3();
                    setlen(poly, 7);
                    setcode(poly, 0x27);
                    gte_stotz(opz);
                    addPrim(&ws->ot[((u32)ws->gteResult << ds->otDepthShift) >> 4 & 0x3FF], poly);
                }
            }
            poly++;
            arg2 += ws->elemStride;
        } while (ws->elemCount-- > 0);
    }
    ws->primWrite = (u8*)poly;
    return arg2;
}

u32* func_8009DE48(TmdStreamWorkspace* arg0, s32 arg1, u32* arg2)
{
    TmdStreamWorkspace* ws;
    POLY_FT4*           poly;
    s32*                opz;
    DisplayState*       ds;
    u32                 clipMask;
    s32*                flg;
    u16*                rec;
    u8*                 verts;

    ws   = arg0;
    poly = (POLY_FT4*)ws->primWrite;
    if (ws->elemCount-- > 0) {
        flg      = &ws->gteFlag;
        clipMask = TMD_GTE_ERROR_FLAG;
        opz      = &ws->gteResult;
        ds       = &gDisplayState;
        do {
            rec   = (u16*)arg2;
            verts = (u8*)ws->verts;
            gte_ldv3(verts + (rec[0] & 0xFFF8), verts + (rec[1] & 0xFFF8), verts + (rec[2] & 0xFFF8));
            gte_rtpt();
            gte_stflg(flg);
            if ((ws->gteFlag & clipMask) == 0) {
                gte_nclip();
                gte_stopz(opz);
                gte_stsxy3_ft4(poly);
                gte_ldv0((u8*)ws->verts + (rec[3] & 0xFFF8));
                gte_rtps();
                gte_stflg(flg);
                if ((ws->gteFlag & clipMask) == 0) {
                    if (ws->gteResult > 0) {
                        goto draw;
                    }
                    gte_nclip();
                    gte_stopz(opz);
                    if (ws->gteResult < 0) {
                    draw:
                        gte_stsxy2(&poly->x3);
                        gte_avsz4();
                        setlen(poly, 9);
                        setcode(poly, 0x2F);
                        gte_stotz(opz);
                        addPrim(&ws->ot[((u32)ws->gteResult << ds->otDepthShift) >> 4 & 0x3FF], poly);
                    }
                }
            }
            poly++;
            arg2 += ws->elemStride;
        } while (ws->elemCount-- > 0);
    }
    ws->primWrite = (u8*)poly;
    return arg2;
}

u32* func_8009E048(TmdStreamWorkspace* arg0, s32 arg1, u32* arg2)
{
    TmdStreamWorkspace* ws;
    POLY_G3*            poly;
    s32*                opz;
    DisplayState*       ds;
    u16*                rec;
    u8*                 verts;

    ws   = arg0;
    poly = (POLY_G3*)ws->primWrite;
    if (ws->elemCount-- > 0) {
        opz = &ws->gteResult;
        ds  = &gDisplayState;
        do {
            rec   = (u16*)arg2;
            verts = (u8*)ws->verts;
            gte_ldv3(verts + (rec[0] & 0xFFF8), verts + (rec[1] & 0xFFF8), verts + (rec[2] & 0xFFF8));
            gte_rtpt();
            gte_stflg(&ws->gteFlag);
            if (ws->gteFlag >= 0) {
                gte_nclip();
                gte_stopz(opz);
                if (ws->gteResult > 0) {
                    gte_stsxy3_g3(poly);
                    gte_avsz3();
                    gte_ldrgb(arg2 + 3);
                    gte_ldv0((u8*)ws->normals + (rec[3] & 0xFFF8));
                    gte_nccs();
                    gte_strgb(&poly->r0);
                    gte_ldrgb(arg2 + 4);
                    gte_ldv0((u8*)ws->normals + (rec[4] & 0xFFF8));
                    gte_nccs();
                    gte_strgb(&poly->r1);
                    gte_ldrgb(arg2 + 5);
                    gte_ldv0((u8*)ws->normals + (rec[5] & 0xFFF8));
                    gte_nccs();
                    gte_strgb(&poly->r2);
                    setlen(poly, 6);
                    setcode(poly, 0x30);
                    gte_stotz(opz);
                    addPrim(&ws->ot[((u32)ws->gteResult << ds->otDepthShift) >> 4 & 0x3FF], poly);
                }
            }
            poly++;
            arg2 += ws->elemStride;
        } while (ws->elemCount-- > 0);
    }
    ws->primWrite = (u8*)poly;
    return arg2;
}

u32* func_8009E274(TmdStreamWorkspace* arg0, s32 arg1, u32* arg2)
{
    TmdStreamWorkspace* ws;
    POLY_G3*            poly;
    s32*                opz;
    DisplayState*       ds;
    u16*                rec;
    u8*                 verts;

    ws   = arg0;
    poly = (POLY_G3*)ws->primWrite;
    if (ws->elemCount-- > 0) {
        opz = &ws->gteResult;
        ds  = &gDisplayState;
        do {
            rec   = (u16*)arg2;
            verts = (u8*)ws->verts;
            gte_ldv3(verts + (rec[0] & 0xFFF8), verts + (rec[1] & 0xFFF8), verts + (rec[2] & 0xFFF8));
            gte_rtpt();
            gte_stflg(&ws->gteFlag);
            if (ws->gteFlag >= 0) {
                gte_nclip();
                gte_stopz(opz);
                if (ws->gteResult > 0) {
                    gte_stsxy3_g3(poly);
                    gte_avsz3();
                    gte_ldrgb(arg2 + 3);
                    gte_ldv0((u8*)ws->normals + (rec[3] & 0xFFF8));
                    gte_nccs();
                    gte_strgb(&poly->r0);
                    gte_ldrgb(arg2 + 4);
                    gte_ldv0((u8*)ws->normals + (rec[4] & 0xFFF8));
                    gte_nccs();
                    gte_strgb(&poly->r1);
                    gte_ldrgb(arg2 + 5);
                    gte_ldv0((u8*)ws->normals + (rec[5] & 0xFFF8));
                    gte_nccs();
                    gte_strgb(&poly->r2);
                    setlen(poly, 6);
                    setcode(poly, 0x32);
                    gte_stotz(opz);
                    addPrim(&ws->ot[((u32)ws->gteResult << ds->otDepthShift) >> 4 & 0x3FF], poly);
                }
            }
            poly++;
            arg2 += ws->elemStride;
        } while (ws->elemCount-- > 0);
    }
    ws->primWrite = (u8*)poly;
    return arg2;
}

u32* func_8009E4A0(TmdStreamWorkspace* arg0, s32 arg1, u32* arg2)
{
    TmdStreamWorkspace* ws;
    POLY_G4*            poly;
    s32*                opz;
    DisplayState*       ds;
    u32                 clipMask;
    s32*                flg;
    u16*                rec;
    u8*                 verts;

    ws   = arg0;
    poly = (POLY_G4*)ws->primWrite;
    if (ws->elemCount-- > 0) {
        flg      = &ws->gteFlag;
        clipMask = TMD_GTE_ERROR_FLAG;
        opz      = &ws->gteResult;
        ds       = &gDisplayState;
        do {
            rec   = (u16*)arg2;
            verts = (u8*)ws->verts;
            gte_ldv3(verts + (rec[0] & 0xFFF8), verts + (rec[1] & 0xFFF8), verts + (rec[2] & 0xFFF8));
            gte_rtpt();
            gte_stflg(flg);
            if ((ws->gteFlag & clipMask) == 0) {
                gte_nclip();
                gte_stopz(opz);
                gte_stsxy3_g4(poly);
                gte_ldv0((u8*)ws->verts + (rec[3] & 0xFFF8));
                gte_rtps();
                gte_stflg(flg);
                if ((ws->gteFlag & clipMask) == 0) {
                    if (ws->gteResult > 0) {
                        goto draw;
                    }
                    gte_nclip();
                    gte_stopz(opz);
                    if (ws->gteResult < 0) {
                    draw:
                        gte_stsxy2(&poly->x3);
                        gte_avsz4();
                        gte_ldrgb(arg2 + 4);
                        gte_ldv0((u8*)ws->normals + (rec[4] & 0xFFF8));
                        gte_nccs();
                        gte_strgb(&poly->r0);
                        gte_ldrgb(arg2 + 5);
                        gte_ldv0((u8*)ws->normals + (rec[5] & 0xFFF8));
                        gte_nccs();
                        gte_strgb(&poly->r1);
                        gte_ldrgb(arg2 + 6);
                        gte_ldv0((u8*)ws->normals + (rec[6] & 0xFFF8));
                        gte_nccs();
                        gte_strgb(&poly->r2);
                        gte_ldrgb(arg2 + 7);
                        gte_ldv0((u8*)ws->normals + (rec[7] & 0xFFF8));
                        gte_nccs();
                        gte_strgb(&poly->r3);
                        setlen(poly, 8);
                        setcode(poly, 0x38);
                        gte_stotz(opz);
                        addPrim(&ws->ot[((u32)ws->gteResult << ds->otDepthShift) >> 4 & 0x3FF], poly);
                    }
                }
            }
            poly++;
            arg2 += ws->elemStride;
        } while (ws->elemCount-- > 0);
    }
    ws->primWrite = (u8*)poly;
    return arg2;
}

u32* gpDrawStreamPrimG4CornerColorsSemiTrans(TmdStreamWorkspace* ws, s32 flags, u32* stream)
{
    POLY_G4*      poly;
    s32*          opz;
    DisplayState* ds;
    u32           clipMask;
    s32*          flg;
    u16*          rec;
    u8*           verts;

    poly = (POLY_G4*)ws->primWrite;
    if (ws->elemCount-- > 0) {
        flg      = &ws->gteFlag;
        clipMask = TMD_GTE_ERROR_FLAG;
        opz      = &ws->gteResult;
        ds       = &gDisplayState;
        do {
            rec   = (u16*)stream;
            verts = (u8*)ws->verts;
            gte_ldv3(verts + (rec[0] & 0xFFF8), verts + (rec[1] & 0xFFF8), verts + (rec[2] & 0xFFF8));
            gte_rtpt();
            gte_stflg(flg);
            if ((ws->gteFlag & clipMask) == 0) {
                gte_nclip();
                gte_stopz(opz);
                if (ws->gteResult > 0) {
                    gte_stsxy3_g4(poly);
                    gte_ldv0((u8*)ws->verts + (rec[3] & 0xFFF8));
                    gte_rtps();
                    gte_stflg(flg);
                    if ((ws->gteFlag & clipMask) == 0) {
                        if (ws->gteResult > 0) {
                            goto draw;
                        }
                        gte_nclip();
                        gte_stopz(opz);
                        if (ws->gteResult < 0) {
                        draw:
                            gte_stsxy2(&poly->x3);
                            gte_avsz4();
                            gte_ldrgb(stream + 4);
                            gte_ldv0((u8*)ws->normals + (rec[4] & 0xFFF8));
                            gte_nccs();
                            gte_strgb(&poly->r0);
                            gte_ldrgb(stream + 5);
                            gte_ldv0((u8*)ws->normals + (rec[5] & 0xFFF8));
                            gte_nccs();
                            gte_strgb(&poly->r1);
                            gte_ldrgb(stream + 6);
                            gte_ldv0((u8*)ws->normals + (rec[6] & 0xFFF8));
                            gte_nccs();
                            gte_strgb(&poly->r2);
                            gte_ldrgb(stream + 7);
                            gte_ldv0((u8*)ws->normals + (rec[7] & 0xFFF8));
                            gte_nccs();
                            gte_strgb(&poly->r3);
                            setlen(poly, 8);
                            setcode(poly, 0x3A);
                            gte_stotz(opz);
                            addPrim(&ws->ot[((u32)ws->gteResult << ds->otDepthShift) >> 4 & 0x3FF], poly);
                        }
                    }
                }
            }
            poly++;
            stream += ws->elemStride;
        } while (ws->elemCount-- > 0);
    }
    ws->primWrite = (u8*)poly;
    return stream;
}

void func_8009EA50(s32 arg0)
{
    s32 temp;

    if (arg0 <= 0) {
        arg0 = 0;
        temp = 0x80;
    } else {
        if (arg0 >= 0x100) {
            arg0 = 0xFF;
        }
        temp = (0xFF - arg0) >> 1;
    }

    D_80114BA4.r = D_80114BA4.g = D_80114BA4.b = arg0;
    D_80114BA8.r = D_80114BA8.g = D_80114BA8.b = temp;
}

u32* gpXformStreamVertsUnlit(TmdStreamWorkspace* ws, s32 flags, u32* stream)
{
    s32  prev;
    s32  count;
    u32  idx;
    u16* rec;

    count = ws->elemCount;
    if (count == 0) {
        return stream;
    }
    prev          = -1;
    ws->elemCount = count + prev;
    if (count > 0) {
        do {
            rec = (u16*)stream;
            idx = rec[0];
            if (idx != prev) {
                gte_ldv0((u8*)ws->verts + (idx & 0xFFF8));
                gte_rtps();
                gte_stsz(&ws->gteResult);
                if (ws->gteFlag & TMD_GTE_ERROR_FLAG) {
                    ws->gteResult |= TMD_VERTEX_DEPTH_INVALID;
                }
                ws->szTable[*(u16*)stream >> 3] = ws->gteResult;
            }
            prev = rec[0];
            gte_stsxy(ws->preXformWrite + rec[1]);
            stream += ws->elemStride;
        } while (ws->elemCount-- > 0);
    }
    return stream;
}

u32* tmdBuildStreamGt3PreXform(TmdStreamWorkspace* workspace, s32 objectFlags, u32* elements)
{
    // Word offset after the element's three u16 depth references and unused high half.
    enum { TMD_GT3_PRE_XFORM_UV0_CLUT_WORD_INDEX = 2 };
    POLY_GT3* triangle;

    triangle = (POLY_GT3*)workspace->preXformWrite;
    // Seed texture data in the region whose positions and colours the projection pre-pass writes.
    while (workspace->elemCount-- > 0) {
        _modelLightingInitGt3TextureWords(triangle, elements, TMD_GT3_PRE_XFORM_UV0_CLUT_WORD_INDEX, workspace);
        triangle++;
        elements += workspace->elemStride;
    }
    workspace->preXformWrite = (u8*)triangle;
    return elements;
}

u32* gpStreamPrimGt4PreXform(TmdStreamWorkspace* ws, s32 flags, u32* stream)
{
    POLY_GT4* poly;

    poly = (POLY_GT4*)ws->preXformWrite;
    if (ws->elemCount-- > 0) {
        do {
            MODEL_LIGHTING_UV0_CLUT_WORD(poly)  = stream[2];
            MODEL_LIGHTING_UV1_TPAGE_WORD(poly) = stream[3];
            *(u16*)&poly->u2                    = (u16)stream[4];
            *(u16*)&poly->u3                    = ((u16*)&stream[4])[1];
            poly->tpage                        += ws->texturePageOffset;
            poly->clut                         += ws->encodedClutOffset;
            poly++;
            stream += ws->elemStride;
        } while (ws->elemCount-- > 0);
    }
    ws->preXformWrite = (u8*)poly;
    return stream;
}

u32* modelLightingStreamPrimF4PreXform(TmdStreamWorkspace* ws, s32 flags, u32* stream)
{
    POLY_F4* poly;
    s32      color;

    poly = (POLY_F4*)ws->preXformWrite;
    if (ws->elemCount-- > 0) {
        do {
            color = stream[2];
            setlen(poly, 5);
            GPU_PRIMITIVE_COLOR_WORD(poly, 0) = color;
            // The colour word includes the command byte, so the flat code follows it.
            setcode(poly, 0x28);
            poly++;
            stream += ws->elemStride;
        } while (ws->elemCount-- > 0);
    }
    ws->preXformWrite = (u8*)poly;
    return stream;
}

u32* modelLightingStreamPrimF3PreXform(TmdStreamWorkspace* ws, s32 flags, u32* stream)
{
    POLY_F3* poly;
    s32      color;

    poly = (POLY_F3*)ws->preXformWrite;
    if (ws->elemCount-- > 0) {
        do {
            color = stream[2];
            setlen(poly, 4);
            GPU_PRIMITIVE_COLOR_WORD(poly, 0) = color;
            // The colour word includes the command byte, so the flat code follows it.
            setcode(poly, 0x20);
            poly++;
            stream += ws->elemStride;
        } while (ws->elemCount-- > 0);
    }
    ws->preXformWrite = (u8*)poly;
    return stream;
}

u32* tmdBuildStreamGt3(TmdStreamWorkspace* workspace, s32 objectFlags, u32* elements)
{
    POLY_GT3* triangle;

    triangle = (POLY_GT3*)workspace->primWrite;
    if (workspace->elemCount-- > 0) {
        do {
            _tmdInitGt3Texture(triangle, elements, workspace);
            triangle++;
            elements += workspace->elemStride;
        } while (workspace->elemCount-- > 0);
    }
    workspace->primWrite = (u8*)triangle;
    return elements;
}

u32* tmdBuildStreamGt4(TmdStreamWorkspace* workspace, s32 objectFlags, u32* elements)
{
    POLY_GT4* quad;

    quad = (POLY_GT4*)workspace->primWrite;
    // Seed texture data that persists while drawing updates geometry and lighting.
    while (workspace->elemCount-- > 0) {
        _modelLightingInitGt4Texture(quad, elements, workspace);
        quad++;
        elements += workspace->elemStride;
    }
    workspace->primWrite = (u8*)quad;
    return elements;
}

u32* tmdBuildStreamGt3ElemColor(TmdStreamWorkspace* workspace, s32 objectFlags, u32* elements)
{
    /// Index of the first texture word in a GT3 element with one material colour.
    ///
    /// Counts u32 words from the element base, after the three-word record
    /// header. Opcode `0x30` stores three geometry-reference words followed by
    /// one material-colour word. This word packs U0/V0 in bits 0..15 and the
    /// encoded CLUT in bits 16..31; the next two words supply U1/V1/texture-page
    /// settings and U2/V2 in the last low half. Seven readable words are needed,
    /// without establishing the element's full extent or the last high half's role.
    enum { MODEL_LIGHTING_GT3_ELEMENT_COLOR_UV0_CLUT_WORD = 4 };
    POLY_GT3* triangle;

    triangle = (POLY_GT3*)workspace->primWrite;
    // Seed texture data for the draw pass that lights the element's material colour.
    while (workspace->elemCount-- > 0) {
        _modelLightingInitGt3TextureWords(triangle, elements, MODEL_LIGHTING_GT3_ELEMENT_COLOR_UV0_CLUT_WORD, workspace);
        triangle++;
        elements += workspace->elemStride;
    }
    workspace->primWrite = (u8*)triangle;
    return elements;
}

u32* tmdBuildStreamGt3CornerColors(TmdStreamWorkspace* workspace, s32 objectFlags, u32* elements)
{
    POLY_GT3* triangle;

    triangle = (POLY_GT3*)workspace->primWrite;
    // Seed texture data for the draw pass that lights each corner's material colour.
    while (workspace->elemCount-- > 0) {
        _modelLightingInitGt3CornerColorsTexture(triangle, elements, workspace);
        triangle++;
        elements += workspace->elemStride;
    }
    workspace->primWrite = (u8*)triangle;
    return elements;
}

u32* tmdBuildStreamGt4ElemColor(TmdStreamWorkspace* workspace, s32 objectFlags, u32* elements)
{
    /// Index of the first texture word in a GT4 element with one material colour.
    ///
    /// Counts u32 words from the element base, after the three-word record
    /// header. Opcode `0x70` stores four geometry-reference words, then one
    /// material-colour word, then this word. It packs U0/V0 in bits 0..15 and
    /// the encoded CLUT in bits 16..31. The next word supplies U1/V1 and the
    /// encoded texture-page settings, and the word after that packs U2/V2 in
    /// its low half and U3/V3 in its high half. Eight readable words are
    /// needed, without establishing the element's full extent.
    enum { MODEL_LIGHTING_GT4_ELEMENT_COLOR_UV0_CLUT_WORD = 5 };
    POLY_GT4* quad;

    quad = (POLY_GT4*)workspace->primWrite;
    // Seed texture data for the draw pass that lights the element's material colour.
    while (workspace->elemCount-- > 0) {
        _modelLightingInitGt4TextureWords(quad, elements, MODEL_LIGHTING_GT4_ELEMENT_COLOR_UV0_CLUT_WORD, workspace);
        quad++;
        elements += workspace->elemStride;
    }
    workspace->primWrite = (u8*)quad;
    return elements;
}

u32* tmdBuildStreamGt4CornerColors(TmdStreamWorkspace* workspace, s32 objectFlags, u32* elements)
{
    POLY_GT4* quad;

    quad = (POLY_GT4*)workspace->primWrite;
    // Seed texture data for the draw pass that lights each corner's material colour.
    while (workspace->elemCount-- > 0) {
        _modelLightingInitGt4CornerColorsTexture(quad, elements, workspace);
        quad++;
        elements += workspace->elemStride;
    }
    workspace->primWrite = (u8*)quad;
    return elements;
}

u32* tmdBuildStreamGt3OneNormal(TmdStreamWorkspace* workspace, s32 objectFlags, u32* elements)
{
    enum {
        /// Index of the first packed texture word in a one-face-normal GT3 element.
        ///
        /// Counts u32 words from the element base, after the three-word stream
        /// header for opcodes 0x18/0x1A. Words 0/1 hold three u16 vertex byte
        /// offsets and one u16 face-normal byte offset. On the little-endian
        /// target, word 2 packs U0 in bits 0..7, V0 in bits 8..15 and the
        /// model-relative encoded CLUT address in bits 16..31, copied together
        /// before palette relocation. The next two words supply U1/V1/tpage
        /// and U2/V2; only the latter's low half is copied. Each element must
        /// provide at least five readable words; its complete extent comes
        /// from the record's word stride rather than this index.
        MODEL_LIGHTING_GT3_ONE_NORMAL_UV0_CLUT_WORD_INDEX = 2
    };
    POLY_GT3* triangle;

    triangle = (POLY_GT3*)workspace->primWrite;
    // Seed texture data for the draw pass that lights all corners from the face normal.
    while (workspace->elemCount-- > 0) {
        _modelLightingInitGt3TextureWords(triangle, elements, MODEL_LIGHTING_GT3_ONE_NORMAL_UV0_CLUT_WORD_INDEX, workspace);
        triangle++;
        elements += workspace->elemStride;
    }
    workspace->primWrite = (u8*)triangle;
    return elements;
}

u32* gpStreamPrimGt4OneNormal(TmdStreamWorkspace* ws, s32 flags, u32* stream)
{
    POLY_GT4* poly;

    poly = (POLY_GT4*)ws->primWrite;
    if (ws->elemCount-- > 0) {
        do {
            MODEL_LIGHTING_UV0_CLUT_WORD(poly)  = stream[3];
            MODEL_LIGHTING_UV1_TPAGE_WORD(poly) = stream[4];
            *(u16*)&poly->u2                    = (u16)stream[5];
            *(u16*)&poly->u3                    = ((u16*)&stream[5])[1];
            poly->tpage                        += ws->texturePageOffset;
            poly->clut                         += ws->encodedClutOffset;
            poly++;
            stream += ws->elemStride;
        } while (ws->elemCount-- > 0);
    }
    ws->primWrite = (u8*)poly;
    return stream;
}

u32* gpStreamPrimGt4Unlit(TmdStreamWorkspace* ws, s32 flags, u32* stream)
{
    POLY_GT4* poly;
    s32       color;

    poly = (POLY_GT4*)ws->primWrite;
    if (ws->elemCount-- > 0) {
        do {
            GPU_PRIMITIVE_COLOR_WORD(poly, 0) = stream[2];
            GPU_PRIMITIVE_COLOR_WORD(poly, 1) = stream[3];
            GPU_PRIMITIVE_COLOR_WORD(poly, 2) = stream[4];
            color                             = stream[5];
            setlen(poly, 12);
            setcode(poly, 0x3E);
            GPU_PRIMITIVE_COLOR_WORD(poly, 3)   = color;
            MODEL_LIGHTING_UV0_CLUT_WORD(poly)  = stream[6];
            MODEL_LIGHTING_UV1_TPAGE_WORD(poly) = stream[7];
            *(u16*)&poly->u2                    = (u16)stream[8];
            *(u16*)&poly->u3                    = ((u16*)&stream[8])[1];
            poly->tpage                        += ws->texturePageOffset;
            poly->clut                         += ws->encodedClutOffset;
            poly++;
            stream += ws->elemStride;
        } while (ws->elemCount-- > 0);
    }
    ws->primWrite = (u8*)poly;
    return stream;
}

u32* modelLightingStreamPrimFt3(TmdStreamWorkspace* workspace, s32 objectFlags, u32* elements)
{
    POLY_FT3* triangle;

    triangle = (POLY_FT3*)workspace->primWrite;
    if (workspace->elemCount-- > 0) {
        // Seed persistent texture data before drawing transforms and links the packets.
        do {
            _modelLightingInitFt3Texture(triangle, elements, workspace);
            triangle++;
            elements += workspace->elemStride;
        } while (workspace->elemCount-- > 0);
    }
    workspace->primWrite = (u8*)triangle;
    return elements;
}

u32* modelLightingStreamPrimFt4(TmdStreamWorkspace* workspace, s32 objectFlags, u32* elements)
{
    POLY_FT4* quad;

    quad = (POLY_FT4*)workspace->primWrite;
    if (workspace->elemCount-- > 0) {
        // Seed persistent texture data before drawing projects and links the packets.
        do {
            _modelLightingInitFt4Texture(quad, elements, workspace);
            quad++;
            elements += workspace->elemStride;
        } while (workspace->elemCount-- > 0);
    }
    workspace->primWrite = (u8*)quad;
    return elements;
}

u32* modelLightingStreamPrimF4(TmdStreamWorkspace* ws, s32 flags, u32* stream)
{
    POLY_F4* poly;
    s32      color;

    poly = (POLY_F4*)ws->primWrite;
    if (ws->elemCount-- > 0) {
        do {
            color = stream[2];
            setlen(poly, 5);
            GPU_PRIMITIVE_COLOR_WORD(poly, 0) = color;
            // The colour word includes the command byte, so the flat code follows it.
            setcode(poly, 0x28);
            poly++;
            stream += ws->elemStride;
        } while (ws->elemCount-- > 0);
    }
    ws->primWrite = (u8*)poly;
    return stream;
}

u32* modelLightingStreamPrimF3(TmdStreamWorkspace* ws, s32 flags, u32* stream)
{
    POLY_F3* poly;
    s32      color;

    poly = (POLY_F3*)ws->primWrite;
    if (ws->elemCount-- > 0) {
        do {
            color = stream[2];
            setlen(poly, 4);
            GPU_PRIMITIVE_COLOR_WORD(poly, 0) = color;
            // The colour word includes the command byte, so the flat code follows it.
            setcode(poly, 0x20);
            poly++;
            stream += ws->elemStride;
        } while (ws->elemCount-- > 0);
    }
    ws->primWrite = (u8*)poly;
    return stream;
}

u32* tmdBuildStreamGt3OffsetLayer(TmdStreamWorkspace* workspace, s32 objectFlags, u32* elements)
{
    POLY_GT3* triangle;

    triangle = (POLY_GT3*)workspace->primWrite;
    if (workspace->elemCount-- > 0) {
        do {
            // Layer and base share UVs, with independent stream-relative GPU addresses.
            _modelLightingInitGt3OffsetLayerTexture(triangle, elements, workspace);
            triangle++;
            _tmdInitGt3Texture(triangle, elements, workspace);
            triangle++;
            elements += workspace->elemStride;
        } while (workspace->elemCount-- > 0);
    }
    workspace->primWrite = (u8*)triangle;
    return elements;
}

u32* tmdBuildStreamGt3LayeredBase(TmdStreamWorkspace* workspace, s32 objectFlags, u32* elements)
{
    POLY_GT3* triangle;

    triangle = (POLY_GT3*)workspace->primWrite;
    if (workspace->elemCount-- > 0) {
        do {
            // Leave the first packet for the environment layer; texture the opaque base.
            triangle++;
            _tmdInitGt3Texture(triangle, elements, workspace);
            triangle++;
            elements += workspace->elemStride;
        } while (workspace->elemCount-- > 0);
    }
    workspace->primWrite = (u8*)triangle;
    return elements;
}

u32* tmdBuildStreamGt4OffsetLayer(TmdStreamWorkspace* workspace, s32 objectFlags, u32* elements)
{
    /// Index of the texture suffix shared by both packets of a layered GT4 element.
    ///
    /// Counts u32 words from the element base, after the three-word record
    /// header. Opcode `0x4078` stores four geometry-reference words, then this
    /// word. It packs U0/V0 in bits 0..15 and the encoded CLUT in bits 16..31.
    /// The next word supplies U1/V1 and the encoded texture-page settings, and
    /// the word after that packs U2/V2 in its low half and U3/V3 in its high
    /// half. The layer packet relocates this suffix with the object's layer
    /// offsets; the base packet adds the workspace's base displacements.
    enum { MODEL_LIGHTING_GT4_OFFSET_LAYER_UV0_CLUT_WORD = 4 };
    POLY_GT4* quad;

    quad = (POLY_GT4*)workspace->primWrite;
    if (workspace->elemCount-- > 0) {
        do {
            // Layer and base share UVs, with independent stream-relative GPU addresses.
            _modelLightingInitGt4OffsetLayerTexture(quad, elements, workspace);
            quad++;
            _modelLightingInitGt4TextureWords(quad, elements, MODEL_LIGHTING_GT4_OFFSET_LAYER_UV0_CLUT_WORD, workspace);
            quad++;
            elements += workspace->elemStride;
        } while (workspace->elemCount-- > 0);
    }
    workspace->primWrite = (u8*)quad;
    return elements;
}

u32* tmdBuildStreamGt4LayeredBase(TmdStreamWorkspace* workspace, s32 objectFlags, u32* elements)
{
    POLY_GT4* quad;

    quad = (POLY_GT4*)workspace->primWrite;
    while (workspace->elemCount-- > 0) {
        // Leave the first packet for the environment layer; texture the opaque base.
        quad++;
        _modelLightingInitGt4Texture(quad, elements, workspace);
        quad++;
        elements += workspace->elemStride;
    }
    workspace->primWrite = (u8*)quad;
    return elements;
}

u32* tmdBuildStreamGt3PreXformEnvLayer(TmdStreamWorkspace* workspace, s32 objectFlags, u32* elements)
{
    enum {
        TMD_GT3_ENV_INITIAL_TEXTURE_DEPTH_4BIT = 0, // GPU texture-depth selector before drawing replaces the page
        TMD_GT3_ENV_BASE_UV0_CLUT_WORD_INDEX   = 2  // After three u16 depth references and an unused high half
    };
    POLY_GT3* triangle;

    triangle = (POLY_GT3*)workspace->preXformWrite;
    while (workspace->elemCount-- > 0) {
        // Seed the environment slot; drawing replaces its page after projection supplies U/V.
        triangle->tpage = getTPage(TMD_GT3_ENV_INITIAL_TEXTURE_DEPTH_4BIT, GPU_BLEND_ADD, 960, 256);
        triangle->clut  = getClut(256, 240);
        triangle++;
        // Only the opaque base receives the element texture and model displacements.
        _modelLightingInitGt3TextureWords(triangle, elements, TMD_GT3_ENV_BASE_UV0_CLUT_WORD_INDEX, workspace);
        triangle++;
        elements += workspace->elemStride;
    }
    workspace->preXformWrite = (u8*)triangle;
    return elements;
}

u32* gpStreamPrimGt4PreXformLayer(TmdStreamWorkspace* ws, s32 flags, u32* stream)
{
    POLY_GT4* poly;

    poly = (POLY_GT4*)ws->preXformWrite;
    if (ws->elemCount-- > 0) {
        do {
            poly->tpage = 0x3F;
            poly->clut  = 0x3C10;
            poly++;
            MODEL_LIGHTING_UV0_CLUT_WORD(poly)  = stream[2];
            MODEL_LIGHTING_UV1_TPAGE_WORD(poly) = stream[3];
            *(u16*)&poly->u2                    = (u16)stream[4];
            *(u16*)&poly->u3                    = ((u16*)&stream[4])[1];
            poly->tpage                        += ws->texturePageOffset;
            poly->clut                         += ws->encodedClutOffset;
            poly++;
            stream += ws->elemStride;
        } while (ws->elemCount-- > 0);
    }
    ws->preXformWrite = (u8*)poly;
    return stream;
}

u32* tmdBuildStreamGt3PreXformOffsetLayer(TmdStreamWorkspace* workspace, s32 objectFlags, u32* elements)
{
    enum { TMD_GT3_PRE_XFORM_OFFSET_LAYER_BASE_UV0_CLUT_WORD = 2 };
    POLY_GT3* triangle;

    triangle = (POLY_GT3*)workspace->preXformWrite;
    while (workspace->elemCount-- > 0) {
        // Layer and base share UVs, with independent stream-relative GPU addresses.
        _modelLightingInitGt3PreXformOffsetLayerTexture(triangle, elements, workspace);
        triangle++;
        _modelLightingInitGt3TextureWords(triangle, elements, TMD_GT3_PRE_XFORM_OFFSET_LAYER_BASE_UV0_CLUT_WORD, workspace);
        triangle++;
        elements += workspace->elemStride;
    }
    workspace->preXformWrite = (u8*)triangle;
    return elements;
}

u32* gpStreamPrimGt4PreXformOffsetLayer(TmdStreamWorkspace* ws, s32 flags, u32* stream)
{
    POLY_GT4* poly;
    s32       tpage;
    s32       tmp;

    poly = (POLY_GT4*)ws->preXformWrite;
    if (ws->elemCount-- > 0) {
        do {
            MODEL_LIGHTING_UV0_CLUT_WORD(poly)  = stream[2];
            MODEL_LIGHTING_UV1_TPAGE_WORD(poly) = stream[3];
            *(u16*)&poly->u2                    = (u16)stream[4];
            *(u16*)&poly->u3                    = ((u16*)&stream[4])[1];
            poly->tpage                        += ws->obj->layerTexturePageOffset;
            tmp                                 = (u8)ws->obj->layerClutRowOffset;
            tpage                               = poly->tpage;
            tpage                              |= 0x20;
            poly->tpage                         = tpage;
            poly->clut                         += (s8)tmp << 6;
            poly++;
            MODEL_LIGHTING_UV0_CLUT_WORD(poly)  = stream[2];
            MODEL_LIGHTING_UV1_TPAGE_WORD(poly) = stream[3];
            *(u16*)&poly->u2                    = (u16)stream[4];
            *(u16*)&poly->u3                    = ((u16*)&stream[4])[1];
            poly->tpage                        += ws->texturePageOffset;
            poly->clut                         += ws->encodedClutOffset;
            poly++;
            stream += ws->elemStride;
        } while (ws->elemCount-- > 0);
    }
    ws->preXformWrite = (u8*)poly;
    return stream;
}

u32* modelLightingReserveStreamPrimG4(TmdStreamWorkspace* workspace, s32 objectFlags, u32* elements)
{
    POLY_G4* quad;
    s32      elementStrideWords;

    quad = (POLY_G4*)workspace->primWrite;
    if (workspace->elemCount-- > 0) {
        elementStrideWords = workspace->elemStride;
        // Keep each element's packet slot in step with the later draw pass.
        do {
            elements += elementStrideWords;
            quad++;
        } while (workspace->elemCount-- > 0);
    }
    workspace->primWrite = (u8*)quad;
    return elements;
}

u32* modelLightingReserveStreamPrimG3(TmdStreamWorkspace* workspace, s32 objectFlags, u32* elements)
{
    POLY_G3* triangle;
    s32      elementStrideWords;

    triangle = (POLY_G3*)workspace->primWrite;
    if (workspace->elemCount-- > 0) {
        elementStrideWords = workspace->elemStride;
        // Keep each element's packet slot in step with the later draw pass.
        do {
            elements += elementStrideWords;
            triangle++;
        } while (workspace->elemCount-- > 0);
    }
    workspace->primWrite = (u8*)triangle;
    return elements;
}

static u32* func_8009FCDC(TmdStreamWorkspace* arg0, s32 arg1, u32* arg2)
{
    u8* prims;
    s32 stride;

    prims = arg0->primWrite;
    if (arg0->elemCount-- > 0) {
        stride = arg0->elemStride;
        do {
            arg2  += stride;
            prims += 0x14;
        } while (arg0->elemCount-- > 0);
    }
    arg0->primWrite = prims;
    return arg2;
}

static u32* func_8009FD28(TmdStreamWorkspace* arg0, s32 arg1, u32* arg2)
{
    u8* prims;
    s32 stride;

    prims = arg0->primWrite;
    if (arg0->elemCount-- > 0) {
        stride = arg0->elemStride;
        do {
            arg2  += stride;
            prims += 0x18;
        } while (arg0->elemCount-- > 0);
    }
    arg0->primWrite = prims;
    return arg2;
}

void Gp_ApplyPadReplay(s32 arg0, u16* arg1)
{
    u16 temp_v0;
    u16 temp_v1;
    s32 offset;

    if (arg0 == GAME_DEBUG_INPUT_OVERRIDE_EXTERNAL) {
        func_807150F8(1);
        return;
    }

    offset = (u8*)Gp_ReplayCursor - (u8*)Fs_ActorLoadBase2;
    if (gDisplayState.demoScene == DISPLAY_DEMO_FIXED_REPLAY) {
        offset = (u8*)Gp_ReplayCursor - FILE_SYSTEM_FIXED_REPLAY_BASE;
    }
    if (offset <= 0x17FDF) {
        if (GameMain_HaltFlags != 0) {
            *arg1 = Gp_ReplayButtons;
            return;
        }
        temp_v1 = Gp_ReplayCursor[0];
        if (temp_v1 != Gp_ReplayButtons) {
            Gp_ReplayButtons    = temp_v1;
            Gp_ReplayFramesLeft = Gp_ReplayCursor[1];
        }
        if (*arg1 & 0x800) {
            *arg1                       = Gp_ReplayButtons | 0x800;
            Wip_SysFlags.skipTitleIntro = 1;
        } else {
            *arg1 = Gp_ReplayButtons;
        }
        temp_v0             = Gp_ReplayFramesLeft - 1;
        Gp_ReplayFramesLeft = temp_v0;
        if (!(temp_v0 & 0xFFFF)) {
            u16* next = Gp_ReplayCursor + 2;

            Gp_ReplayButtons = 0xFFFF;
            Gp_ReplayCursor  = next;
            if (*next == 0xFFFF) {
                Wip_SysFlags.skipTitleIntro       = 0;
                Pad_RemapState->inputOverrideMode = GAME_DEBUG_INPUT_OVERRIDE_NONE;
            }
        }
    } else {
        Pad_RemapState->inputOverrideMode = GAME_DEBUG_INPUT_OVERRIDE_NONE;
    }
}

void Gp_InitPlayClock(Task* task)
{
    _PlayClockWork* work;
    DisplayState*   ds;

    Gp_UpdatePadInput();
    gGameSession->field_5E = 1;
    work                   = memCalloc(sizeof(_PlayClockWork), 0);
    if (work == NULL) {
        taskKill(task);
        return;
    }
    Gp_ResetHudFx(&work->hud);
    GameMain_SetFrameTiming(DISPLAY_TIMING_TWO_VBLANKS);
    task->work         = work;
    work->hours        = gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.playTime / 60;
    work->minutes      = gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.playTime % 60;
    ds                 = &gDisplayState;
    work->lastGameTick = ds->gameTick;
    func_800B25B0();
    if (ds->demoScene != DISPLAY_DEMO_NONE) {
        srand(1);
        ds->animFrame            = 0;
        gDisplayState.frameCount = 0;
        gRandomLcgState          = 0;
        ds->gameTick             = 0;
        ds->loopCount            = 0;
        ds->vsyncCount           = 0;
        ds->loopTicks            = 0;
        if (ds->demoScene == DISPLAY_DEMO_FIXED_REPLAY) {
            Gp_ReplayCursor = (u16*)(FILE_SYSTEM_FIXED_REPLAY_BASE + 0xD4C);
        } else {
            Gp_ReplayCursor = (u16*)((u8*)Fs_ActorLoadBase2 + 0xD4C);
        }
        Gp_ReplayButtons                  = 0xFFFF;
        Gp_ReplayFramesLeft               = 1;
        Pad_RemapState->inputOverrideMode = GAME_DEBUG_INPUT_OVERRIDE_REPLAY;
    } else if (Pad_RemapState->field_9 == 1) {
        func_80715198();
    }
    task->state++;
}

void Gp_TickPlayClock(Task* task)
{
    TextDrawReq     req;
    u8              buf[0x20];
    _PlayClockWork* work;
    McSaveData*     save;
    PlayerStatus*   cfg;
    GameSession*    session;
    s32             one;
    s32             temp;
    s32             companion;

    work = task->work;
    cfg  = &gPlayerStatus;
    Gp_UpdatePadInput();

    temp               = gDisplayState.gameTick;
    D_8005ED68        += temp - work->lastGameTick;
    work->lastGameTick = temp;
    if (D_8005ED68 >= 0xE10) {
        McSaveData* p;
        D_8005ED68 -= 0xE10;
        p           = &gMcSaveData[MEMORY_CARD_SAVE_LIVE];
        if (p->state.playTime <= 0xEA5E) {
            p->state.playTime++;
            work->minutes++;
            if (work->minutes >= 60) {
                work->minutes -= 60;
                work->hours++;
            }
        } else {
            p->state.playTime = 0xEA5F;
            work->hours       = 999;
            work->minutes     = 59;
        }
    }

    save = &gMcSaveData[MEMORY_CARD_SAVE_LIVE];
    one  = 1;
    if (save->state.demoScene == one) {
        req.x          = -0x96;
        req.y          = 0x64;
        req.otIndex    = 4;
        req.colorRgb   = 0x502008;
        req.glyphTable = TEXT_GLYPH_TABLE_LARGE_ALTERNATE;
        req.alignment  = TEXT_ALIGNMENT_LEFT;
        req.drawMode   = one;
        Text_DrawString(&req, Text_ItoaUnsigned(buf, work->hours));
        Text_DrawString(&req, ":");
        Text_DrawString(&req, Text_ItoaPadded(buf, work->minutes, 2));
        Text_DrawString(&req, "'");
        Text_DrawString(&req, Text_ItoaPadded(buf, D_8005ED68 / 60, 2));
        padCheckButtons(one, one, PAD_BUTTON_SELECT);
    }

    if (gGameSession->suppressDeathChecks == 0) {
        if (cfg->hp > 0) {
            companion = save->state.companionType;
            if (companion == one) {
                if (save->state.companionHp <= 0) {
                    goto block_hp;
                }
            }
            if (companion != 3) {
                goto block_normal;
            }
            if (save->state.companionHp > 0) {
                goto block_normal;
            }
        block_hp:
            if (cfg->hp > 0) {
                goto block_companion;
            }
        }

        if (gGameSession->eventState != 0) {
            cfg->hp = 1;
            return;
        }
        Gp_StateC08.effectPhase = ATTACHMENT_EFFECT_IDLE;
        func_800A7DE0();
        Gp_PulseState1C80();
        session = gGameSession;
        if (session->restartMode != GAME_SESSION_RESTART_PRESERVE_DISPLAY) {
            gRandomLcgState       = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            session->deathVariant = (gRandomLcgState >> 16 & 1) + 1;
            SndEvt_EnqueueType7(SOUND_BANK_TYPE_WEAPON_ALL, 8);
            SndBank_SetEnableFlags(0, 0x20000000);
            CdCmd_EnqueueLoadFile(9, ((u8)gGameSession->deathVariant + 0x1D) & 0xFF, 3);
        }

    block_companion: {
        McSaveData* p;
        p = &gMcSaveData[MEMORY_CARD_SAVE_LIVE];
        if (p->state.companionHp <= 0) {
            if (gGameSession->eventState != 0) {
                p->state.companionHp = 1;
                return;
            }
            Gp_StateC08.effectPhase = ATTACHMENT_EFFECT_IDLE;
            func_800A7DE0();
            Gp_PulseState1C80();
            companion = p->state.companionType;
            if (companion == 1) {
                gGameSession->restartMode  = companion;
                gRandomLcgState            = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                gGameSession->deathVariant = (gRandomLcgState >> 16 & 1) + 1;
                SndEvt_EnqueueType7(SOUND_BANK_TYPE_WEAPON_ALL, 8);
                SndBank_SetEnableFlags(0, 0x20000000);
                CdCmd_EnqueueLoadFile(9, ((u8)gGameSession->deathVariant + 0x20) & 0xFF, 3);
                companion = p->state.companionType;
            }
            if (companion == 3) {
                gGameSession->restartMode = GAME_SESSION_RESTART_COMPANION_3_DOWN;
            }
        }
    }
        Display_AcquireRef();
        task->killCountdown   = gGameSession->deathRestartDelay;
        Wip_SysFlags.gameOver = 1;
        task->state++;
        return;
    }

block_normal:
    if (gGameSession->restartMode == GAME_SESSION_RESTART_ENDING) {
        Display_AcquireRef();
        gGameSession->deathVariant = 1;
        task->state++;
    } else {
        Gp_HudTask(&work->hud);
    }
}

void Gp_RestartSessionTask(Task* arg0)
{
    RECT          rect;
    DisplayState* ds;
    GameSession*  session;
    CdCmdQueue*   queue;
    s32           flag;

    queue = &gCdCmdQueue;
    Gp_StartAreaBgm(&arg0->killCountdown);
    arg0->spawnArg1.value += 0xA;
    if (arg0->spawnArg1.value < 0x100) {
        return;
    }
    if (gGameSession->restartMode != GAME_SESSION_RESTART_PRESERVE_DISPLAY) {
        SetDispMask(0);
    }
    SndEvt_EnqueueType2(0, 8);
    SndEvt_EnqueueType7(SOUND_BANK_TYPE_ALL_NON_AMBIENT, 0x78);
    SndEvt_EnqueueType7(SOUND_STAGE_AMBIENT, 0x78);
    flag                  = 0xFF;
    arg0->spawnArg1.value = flag;
    Pad_SetCooldown(0);
    Game_ClearPtrSlots();
    ds               = &gDisplayState;
    ds->stopTaskWalk = 1;
    Task_ResetDefaultList();
    Gpu_ClearOTag(0);
    Gpu_ClearOTag(1);
    Mem_Init();
    CdCmd_ActivatePhase1();
    session                          = gGameSession;
    queue->suppressMoviePresentation = 1;
    if (session->restartMode != GAME_SESSION_RESTART_PRESERVE_DISPLAY) {
        rect.w = 0x140;
        rect.y = 0;
        rect.x = 0;
        rect.h = 0x200;
        ClearImage(&rect, 0, 0, 0);
        DrawSync(0);
        ds->control.flags.imageSource = DISPLAY_IMAGE_NONE;
    }
    memset(&gGameSession->location, 0, sizeof(gGameSession->location));
    Mem_ConfigureAuxHeap(0, 0);
    if (gGameSession->restartMode == flag) {
        Gpu_PrimHeapSize   = 0xB000;
        GActiveAuxHeapSize = 0x30000;
        Gpu_PrimHeapBase   = (u8*)Fs_ImgBuffers - 0x35800;
        gMemActiveAuxHeap  = (u8*)Fs_ImgBuffers - 0xA800;
    }
    Mem_Init();
    Mem_InitAux();
    if (gGameSession->restartMode != flag) {
        CdCmd_SetupMdecBuffers();
    }
    Task_SpawnFromTable(&D_8010D1FC, 0, 0, 0);
}

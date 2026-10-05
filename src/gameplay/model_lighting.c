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

/// Geometry references are byte offsets into eight-byte SVECTOR entries.
/// Low reference bits are discarded; their meaning is unproven for these records.
enum {
    TMD_STREAM_GEOMETRY_BYTE_OFFSET_MASK = 0xFFF8,
    TMD_STREAM_VERTEX_INDEX_SHIFT        = 3,
    TMD_FT3_RAW_OPAQUE_COMMAND           = 0x25,
    TMD_FT4_RAW_OPAQUE_COMMAND           = 0x2D,
    TMD_DRAW_OT_INDEX_SHIFT              = 4, // Display-scaled OTZ units to four-byte OT buckets
    TMD_GT3_CORNER_COUNT                 = 3,
    TMD_GT4_CORNER_COUNT                 = 4
};

/// Geometry-reference prefix of a directly transformed Gouraud textured quad element.
///
/// Four vertex references precede four normal references in corner order.
/// Each is a byte offset into its respective borrowed SVECTOR array after
/// masking with `TMD_STREAM_GEOMETRY_BYTE_OFFSET_MASK`; low-bit roles and array
/// extents are unproven. Material and texture words follow this prefix and are
/// outside this type.
typedef struct {
    u16 vertexByteRefs[TMD_GT4_CORNER_COUNT]; // Part-local vertex references for corners 0..3
    u16 normalByteRefs[TMD_GT4_CORNER_COUNT]; // Part-local normal references for corners 0..3
} _ModelLightingGt4GeometryRefs;
STATIC_ASSERT_SIZEOF(_ModelLightingGt4GeometryRefs, 4 * sizeof(u32));

/// Environment mapping uses fixed 320x240 screen centring and overlapping pages.
/// Normal scaling is GPF12 with colorBlend >> 9; U/V stores truncate to bytes.
enum {
    TMD_ENV_SCREEN_CENTER_X        = 160,
    TMD_ENV_SCREEN_CENTER_Y        = 120,
    TMD_ENV_NORMAL_BLEND_SHIFT     = 9,
    TMD_ENV_FIRST_PAGE_U_LIMIT     = 256,
    TMD_ENV_PAGE_U_DISPLACEMENT    = 128,
    TMD_ENV_SECOND_PAGE_U_LIMIT    = 192,
    TMD_ENV_V_LIMIT                = 240,
    TMD_ENV_FIRST_PAGE_MARKER      = 0,
    TMD_ENV_SECOND_PAGE_MARKER     = 1,
    TMD_ENV_FIRST_TEXTURE_PAGE     = getTPage(2, GPU_BLEND_ADD, 448, 256),
    TMD_ENV_SECOND_TEXTURE_PAGE    = getTPage(2, GPU_BLEND_ADD, 576, 256),
    TMD_ENV_V_TO_PAGE_MARKER_BYTES = OFFSET_OF(POLY_GT3, v0) - OFFSET_OF(POLY_GT3, code),
    TMD_ENV_CORNER_STRIDE_BYTES    = OFFSET_OF(POLY_GT3, u1) - OFFSET_OF(POLY_GT3, u0)
};
STATIC_ASSERT(OFFSET_OF(POLY_GT4, v0) - OFFSET_OF(POLY_GT4, code) == TMD_ENV_V_TO_PAGE_MARKER_BYTES,
              tmd_env_quad_marker_displacement);
STATIC_ASSERT(OFFSET_OF(POLY_GT4, u1) - OFFSET_OF(POLY_GT4, u0) == TMD_ENV_CORNER_STRIDE_BYTES,
              tmd_env_quad_corner_stride);

/// Lights the layer/base colour groups of one offset-layer corner and saves its rotated normal.
///
/// Destination halfwords 2/3 are byte offsets into aligned writable CVECTOR
/// groups in the first packet region; halfword 1 references a complete normal
/// SVECTOR after masking its low bits. The caller configures GTE lighting and
/// supplies the layer/base material RGB/code words. GTE lighting registers and
/// workspace normal scratch change; packet cursors and counts do not.
/// Arguments are evaluated repeatedly: supply side-effect-free pointers.
/// Captures no caller identifiers. Expands to several statements: use only
/// inside an enclosing compound block, never as an unbraced conditional body.
#define TMD_LIGHT_OFFSET_LAYER_CORNER(workspace, elementHalfwords, layerMaterial, baseMaterial)                 \
    gte_ldv0((const u8*)(workspace)->normals + ((elementHalfwords)[1] & TMD_STREAM_GEOMETRY_BYTE_OFFSET_MASK)); \
    gte_ldrgb((layerMaterial));                                                                                 \
    gte_nccs();                                                                                                 \
    gte_strgb((workspace)->preXformWrite + (elementHalfwords)[2]);                                              \
    gte_ldrgb((baseMaterial));                                                                                  \
    gte_nccs();                                                                                                 \
    gte_strgb((workspace)->preXformWrite + (elementHalfwords)[3]);                                              \
    gte_rtv0();                                                                                                 \
    gte_stsv(&(workspace)->elemNormal);

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

/// Lights a Gouraud textured quad's four corners from one loaded GTE material.
///
/// The caller supplies GTE light/colour matrices and background colour, and
/// loads the shared material RGB/code word into RGBC. `geometryRefs` borrows
/// the element's readable reference prefix; each masked normal byte reference
/// must select a complete eight-byte SVECTOR in `workspace->normals`. The vertex
/// references are unused here. Geometry, references and the writable `quad`
/// must be word aligned and live for the call; normal-array bounds are unchecked.
///
/// Writes all four RGB/code words, including the high bytes `p1`, `p2`, `p3`;
/// the caller sets `quad->code` afterwards. Tag, XY and texture fields stay
/// intact. GTE normal/lighting registers change; workspace storage, packet
/// cursors and counts do not. No borrowed pointer is retained.
static inline void _modelLightingLightGt4CornerNormals(POLY_GT4* quad, const TmdStreamWorkspace* workspace, const _ModelLightingGt4GeometryRefs* geometryRefs)
{
    const u8* normalBytes;

    normalBytes = (const u8*)workspace->normals;
    gte_ldv3(normalBytes + (geometryRefs->normalByteRefs[0] & TMD_STREAM_GEOMETRY_BYTE_OFFSET_MASK), normalBytes + (geometryRefs->normalByteRefs[1] & TMD_STREAM_GEOMETRY_BYTE_OFFSET_MASK), normalBytes + (geometryRefs->normalByteRefs[2] & TMD_STREAM_GEOMETRY_BYTE_OFFSET_MASK));
    gte_ncct();
    gte_strgb3_gt4(quad);
    // Complete the fourth corner without reloading the shared material.
    gte_ldv0((const u8*)workspace->normals + (geometryRefs->normalByteRefs[3] & TMD_STREAM_GEOMETRY_BYTE_OFFSET_MASK));
    gte_nccs();
    gte_strgb(&quad->r3);
}

/// Links an offset-texture layer/base quad pair for GPU traversal in base-then-layer order.
///
/// `packetPair` borrows two consecutive writable, word-aligned `POLY_GT4`s:
/// slot 0 is the layer, slot 1 the base. Their DMA payload lengths must already
/// be set. `workspace->gteResult` supplies the pair's AVSZ4 OTZ, 0..65535;
/// `displayState->otDepthShift` is normally 0..3. The unsigned scaled depth,
/// shifted right by four and masked, selects entry 0..1023 in `workspace->ot`,
/// whose base already includes the object's signed table displacement. That
/// entry must fit its backing table; the mask wraps depth rather than clamps it.
///
/// `dmaAddressMask` and `dmaLengthMask` must be `GPU_DMA_LINK_ADDRESS_MASK` and
/// `GPU_DMA_PACKET_LENGTH_MASK`. Each prepend retains the packet and OT tag's
/// high length byte and encodes the next pointer in the low 24 bits. The final
/// chain is base -> layer -> previous head. Only the packet and OT tags change;
/// GTE registers, workspace storage and cursors/counts stay intact. Packets and
/// OT must remain GPU-visible until DMA consumption ends; no pointer is retained.
static inline void _modelLightingLinkOffsetLayerQuadPair(POLY_GT4 packetPair[2], const TmdStreamWorkspace* workspace, const DisplayState* displayState, u32 dmaAddressMask, u32 dmaLengthMask)
{
    enum { MODEL_LIGHTING_OFFSET_LAYER_PACKET = 0,
           MODEL_LIGHTING_OFFSET_BASE_PACKET  = 1 };

    // Prepend the layer first so GPU traversal reaches the base before the layer.
    packetPair[MODEL_LIGHTING_OFFSET_LAYER_PACKET].tag = (packetPair[MODEL_LIGHTING_OFFSET_LAYER_PACKET].tag & dmaLengthMask) | (workspace->ot[(((u32)workspace->gteResult << displayState->otDepthShift) >> TMD_DRAW_OT_INDEX_SHIFT) & (GPU_ORDERING_TABLE_DEPTH_BYTE_MASK / sizeof(*workspace->ot))] & dmaAddressMask);
    workspace->ot[(((u32)workspace->gteResult << displayState->otDepthShift) >> TMD_DRAW_OT_INDEX_SHIFT) & (GPU_ORDERING_TABLE_DEPTH_BYTE_MASK / sizeof(*workspace->ot))] =
        (workspace->ot[(((u32)workspace->gteResult << displayState->otDepthShift) >> TMD_DRAW_OT_INDEX_SHIFT) & (GPU_ORDERING_TABLE_DEPTH_BYTE_MASK / sizeof(*workspace->ot))] & dmaLengthMask) | ((u32)&packetPair[MODEL_LIGHTING_OFFSET_LAYER_PACKET] & dmaAddressMask);
    packetPair[MODEL_LIGHTING_OFFSET_BASE_PACKET].tag = (packetPair[MODEL_LIGHTING_OFFSET_BASE_PACKET].tag & dmaLengthMask) | (workspace->ot[(((u32)workspace->gteResult << displayState->otDepthShift) >> TMD_DRAW_OT_INDEX_SHIFT) & (GPU_ORDERING_TABLE_DEPTH_BYTE_MASK / sizeof(*workspace->ot))] & dmaAddressMask);
    workspace->ot[(((u32)workspace->gteResult << displayState->otDepthShift) >> TMD_DRAW_OT_INDEX_SHIFT) & (GPU_ORDERING_TABLE_DEPTH_BYTE_MASK / sizeof(*workspace->ot))] =
        (workspace->ot[(((u32)workspace->gteResult << displayState->otDepthShift) >> TMD_DRAW_OT_INDEX_SHIFT) & (GPU_ORDERING_TABLE_DEPTH_BYTE_MASK / sizeof(*workspace->ot))] & dmaLengthMask) | ((u32)&packetPair[MODEL_LIGHTING_OFFSET_BASE_PACKET] & dmaAddressMask);
}

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
/// `quad` is the writable, four-byte-aligned first `POLY_GT4` of a layered
/// pair. `elementWords` is the aligned payload base after the record header;
/// `uv0ClutWordIndex` selects three consecutive readable u32 words within
/// that element. The index is nonnegative and index + 2 must fit s32. It is
/// 4 for `0x4078` and 2 for `0x4079`; this does not establish the full stride.
/// The first two words pack unsigned byte U/V texel coordinates with encoded
/// CLUT and page settings. The third packs U2/V2 in its low half and U3/V3 in
/// its high half. Their destination halfword views preserve SDK pad2/pad3.
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
static inline void _modelLightingInitGt4OffsetLayerTexture(POLY_GT4* quad, const u32* elementWords, s32 uv0ClutWordIndex,
                                                           const TmdStreamWorkspace* workspace)
{
    enum {
        MODEL_LIGHTING_OFFSET_LAYER_TPAGE_ABR_LOW_BIT = 1 << 5, // OR after relocation; retains ABR bit 6 (mode 1 or 3)
        MODEL_LIGHTING_OFFSET_LAYER_CLUT_ROW_SHIFT    = 6       // Signed palette rows to encoded CLUT units (64 per row)
    };
    u32 layerTexturePage;
    u8  layerClutRowByte;

    STATIC_ASSERT(OFFSET_OF(POLY_GT4, v2) == OFFSET_OF(POLY_GT4, u2) + sizeof(u8) &&
                      OFFSET_OF(POLY_GT4, pad2) == OFFSET_OF(POLY_GT4, u2) + sizeof(u16) &&
                      OFFSET_OF(POLY_GT4, v3) == OFFSET_OF(POLY_GT4, u3) + sizeof(u8) &&
                      OFFSET_OF(POLY_GT4, pad3) == OFFSET_OF(POLY_GT4, u3) + sizeof(u16),
                  model_lighting_gt4_offset_layer_uv_pair_layout);

    MODEL_LIGHTING_UV0_CLUT_WORD(quad)  = elementWords[uv0ClutWordIndex];
    MODEL_LIGHTING_UV1_TPAGE_WORD(quad) = elementWords[uv0ClutWordIndex + 1];
    // Split the packed U/V pairs without overwriting the adjacent pad2/pad3.
    *(u16*)&quad->u2 = (u16)elementWords[uv0ClutWordIndex + 2];
    *(u16*)&quad->u3 = (u16)(elementWords[uv0ClutWordIndex + 2] >> 16);
    // The page sum wraps to u16 before its ABR mode is adjusted.
    quad->tpage += workspace->obj->layerTexturePageOffset;
    // Keep the row byte unsigned until the CLUT calculation restores its sign.
    layerClutRowByte  = workspace->obj->layerClutRowOffset;
    layerTexturePage  = quad->tpage;
    layerTexturePage |= MODEL_LIGHTING_OFFSET_LAYER_TPAGE_ABR_LOW_BIT;
    quad->tpage       = layerTexturePage;
    quad->clut       += (s8)layerClutRowByte * (1 << MODEL_LIGHTING_OFFSET_LAYER_CLUT_ROW_SHIFT);
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

/// Blends one environment-layer RGB word toward neutral grey.
///
/// Uses GPF12/GPL12 with the object's twelve-fraction-bit blend, reading the
/// blend separately at both loads. Caller gates this at colorBlend < 4096.
/// `colorCursor` is a writable pointer lvalue; `colorDestination` and the
/// reference address select live word-aligned colour groups. Cursor assignment
/// follows the first GTE weight load. Workspace is evaluated twice, destination
/// and reference once; use simple side-effect-free arguments. Captures no caller
/// locals. Clobbers GTE interpolation state and writes the destination RGB.
/// This statement sequence must stand inside a compound block.
#define TMD_BLEND_ENVIRONMENT_LAYER_COLOR(workspace, colorCursor, colorDestination, referenceColor) \
    gte_lddp((workspace)->obj->shading.colorBlend);                                                 \
    (colorCursor) = (colorDestination);                                                             \
    gte_ldcv(colorCursor);                                                                          \
    gte_gpf12();                                                                                    \
    gte_lddp(TMD_OBJECT_COLOR_BLEND_ONE - (workspace)->obj->shading.colorBlend);                    \
    gte_ldcv(referenceColor);                                                                       \
    gte_gpl12();                                                                                    \
    gte_stcv(colorCursor)

/// Rebases unmarked environment U bytes for the second overlapping page.
///
/// The U/marker cursors must traverse three or four live packet corner groups,
/// twelve bytes apart; the caller initializes cornerIndex to zero. Marker 0
/// means first page and 1 second page. The signed-byte test selects U >= 128;
/// adding 128 then storing to u8 subtracts 128 modulo 256. Smaller U clamps
/// to zero; marked corners retain U. Both byte cursors advance past the last
/// addressed group and the index ends at cornerCount. Arguments are evaluated
/// repeatedly and must be simple cursor/index lvalues or a side-effect-free
/// count, with no aliasing among the cursors. Captures no caller locals; does
/// not use the GTE. This loop is a single do/while statement.
#define TMD_FOLD_ENVIRONMENT_PAGE_U(pageMarker, textureU, cornerIndex, cornerCount) \
    do {                                                                            \
        if (*(pageMarker) == TMD_ENV_FIRST_PAGE_MARKER) {                           \
            if ((s8) * (textureU) < 0) {                                            \
                *(textureU) = *(textureU) + TMD_ENV_PAGE_U_DISPLACEMENT;            \
            } else {                                                                \
                *(textureU) = 0;                                                    \
            }                                                                       \
        }                                                                           \
        (textureU) += TMD_ENV_CORNER_STRIDE_BYTES;                                  \
        (cornerIndex)++;                                                            \
        (pageMarker) += TMD_ENV_CORNER_STRIDE_BYTES;                                \
    } while ((cornerIndex) < (cornerCount))

/// Lights one corner from its material RGB and complete referenced normal.
///
/// Material RGB/code, a complete SVECTOR and the writable colour group must be
/// word aligned and remain live for the operation. Each argument is evaluated
/// once, in material/normal/destination order; no caller identifier is captured.
/// Uses the caller's light/colour matrices and overwrites GTE lighting state.
/// Expands to several statements: use only as a standalone sequence inside a
/// compound block, never as an unbraced conditional or loop body.
#define TMD_LIGHT_STREAM_CORNER(materialColor, normal, rgb) \
    gte_ldrgb((materialColor));                             \
    gte_ldv0((normal));                                     \
    gte_nccs();                                             \
    gte_strgb((rgb));

/// Prepends layer then base, retaining GPU DMA address and length fields.
///
/// `packetPair` addresses two writable, aligned POLY_GT3 slots; the workspace's
/// GTE result is their valid AVSZ3 OTZ. The display shift is normally 0..3 and
/// every resulting masked bucket must fit the displaced OT. Address/length masks
/// must be GPU_DMA_LINK_ADDRESS_MASK/GPU_DMA_PACKET_LENGTH_MASK. Packets and OT
/// remain GPU-visible through consumption. Arguments are evaluated repeatedly:
/// supply simple side-effect-free pointers/mask values. Captures no caller locals.
/// This statement sequence requires an enclosing compound block; it cannot serve
/// as an unbraced conditional or loop body. Neither GTE nor workspace scratch changes.
#define TMD_LINK_OFFSET_LAYER_TRIANGLE_PAIR(packetPair, workspace, displayState, addressMask, lengthMask)                                                                                                                                                       \
    (packetPair)[0].tag = ((packetPair)[0].tag & (lengthMask)) | ((workspace)->ot[((u32)(workspace)->gteResult << (displayState)->otDepthShift) >> TMD_DRAW_OT_INDEX_SHIFT & (GPU_ORDERING_TABLE_DEPTH_BYTE_MASK / sizeof(*(workspace)->ot))] & (addressMask)); \
    (workspace)->ot[((u32)(workspace)->gteResult << (displayState)->otDepthShift) >> TMD_DRAW_OT_INDEX_SHIFT & (GPU_ORDERING_TABLE_DEPTH_BYTE_MASK / sizeof(*(workspace)->ot))] =                                                                               \
        ((workspace)->ot[((u32)(workspace)->gteResult << (displayState)->otDepthShift) >> TMD_DRAW_OT_INDEX_SHIFT & (GPU_ORDERING_TABLE_DEPTH_BYTE_MASK / sizeof(*(workspace)->ot))] & (lengthMask)) | ((u32) & (packetPair)[0] & (addressMask));               \
    (packetPair)[1].tag = ((packetPair)[1].tag & (lengthMask)) | ((workspace)->ot[((u32)(workspace)->gteResult << (displayState)->otDepthShift) >> TMD_DRAW_OT_INDEX_SHIFT & (GPU_ORDERING_TABLE_DEPTH_BYTE_MASK / sizeof(*(workspace)->ot))] & (addressMask)); \
    (workspace)->ot[((u32)(workspace)->gteResult << (displayState)->otDepthShift) >> TMD_DRAW_OT_INDEX_SHIFT & (GPU_ORDERING_TABLE_DEPTH_BYTE_MASK / sizeof(*(workspace)->ot))] =                                                                               \
        ((workspace)->ot[((u32)(workspace)->gteResult << (displayState)->otDepthShift) >> TMD_DRAW_OT_INDEX_SHIFT & (GPU_ORDERING_TABLE_DEPTH_BYTE_MASK / sizeof(*(workspace)->ot))] & (lengthMask)) | ((u32) & (packetPair)[1] & (addressMask));

/// Maps a corner's screen pixels and rotated normal to environment texels.
///
/// Requires simple pointer/lvalue arguments with no side effects: arguments are
/// evaluated repeatedly. Captures no caller identifiers. GPF12 overwrites
/// rotated-normal scratch. Writes U, advances screen/texture cursors from X/U
/// to Y/V, leaves the clamped V in `textureComponent`, and sets `secondPage`
/// to 1 on U rebasing (the caller initializes it to 0). Expands to a statement
/// sequence that requires an enclosing compound block; never use as an unbraced
/// conditional or loop body. The caller stores V
/// and the page marker in its required order. Normal coefficients have twelve
/// fractional bits; colorBlend is normally 0..4096. GTE state is clobbered.
#define TMD_CALCULATE_ENVIRONMENT_CORNER_UV(workspace, rotatedNormal, screenComponent, textureCoordinate, textureComponent, secondPage) \
    gte_lddp((workspace)->obj->shading.colorBlend >> TMD_ENV_NORMAL_BLEND_SHIFT);                                                       \
    gte_ldsv((rotatedNormal));                                                                                                          \
    gte_gpf12();                                                                                                                        \
    gte_stsv((rotatedNormal));                                                                                                          \
    (textureComponent)  = *(screenComponent) + TMD_ENV_SCREEN_CENTER_X;                                                                 \
    (textureComponent) -= (rotatedNormal)->vx;                                                                                          \
    if ((textureComponent) < 0) {                                                                                                       \
        (textureComponent) = 0;                                                                                                         \
    } else if ((textureComponent) >= TMD_ENV_FIRST_PAGE_U_LIMIT) {                                                                      \
        (textureComponent) -= TMD_ENV_PAGE_U_DISPLACEMENT;                                                                              \
        (secondPage)        = TMD_ENV_SECOND_PAGE_MARKER;                                                                               \
        if ((textureComponent) >= TMD_ENV_SECOND_PAGE_U_LIMIT) {                                                                        \
            (textureComponent) = TMD_ENV_SECOND_PAGE_U_LIMIT - 1;                                                                       \
        }                                                                                                                               \
    }                                                                                                                                   \
    *(textureCoordinate) = (textureComponent);                                                                                          \
    (screenComponent)++;                                                                                                                \
    (textureCoordinate)++;                                                                                                              \
    (textureComponent)  = *(screenComponent) + TMD_ENV_SCREEN_CENTER_Y;                                                                 \
    (textureComponent) -= (rotatedNormal)->vy;                                                                                          \
    if ((textureComponent) < 0) {                                                                                                       \
        (textureComponent) = 0;                                                                                                         \
    } else if ((textureComponent) >= TMD_ENV_V_LIMIT) {                                                                                 \
        (textureComponent) = TMD_ENV_V_LIMIT - 1;                                                                                       \
    }

/// Completes and links a projected, front-facing raw-texture triangle.
///
/// The GTE must retain this triangle's screen XY and depths with valid ZSF3.
/// `triangle` is its prebuilt, aligned POLY_FT3 slot; texture fields persist.
/// `gteResultDestination` addresses `workspace->gteResult`. Its OT and the
/// packet must remain GPU-visible through consumption. The display shift is
/// 0..3 in normal drawing; the wrapped bucket must fit the displaced OT base.
/// `rawTextureCommand` is 0x25 (opaque) or 0x27 (semitransparent). Pointer
/// arguments are evaluated repeatedly and must have no side effects; the command
/// is evaluated once. Captures no caller locals. The do/while wrapper is one
/// statement. GTE depth/result state and both DMA tags are updated.
#define TMD_LINK_PROJECTED_RAW_FT3(triangle, workspace, gteResultDestination, displayState, rawTextureCommand)             \
    do {                                                                                                                   \
        enum {                                                                                                             \
            TMD_FT3_OT_INDEX_SHIFT = 4 /* Sixteen scaled depth units per OT tag */                                         \
        };                                                                                                                 \
                                                                                                                           \
        gte_stsxy3_ft3((triangle));                                                                                        \
        gte_avsz3();                                                                                                       \
        setlen((triangle), (sizeof(*(triangle)) - sizeof((triangle)->tag)) / sizeof(u32));                                 \
        setcode((triangle), (rawTextureCommand));                                                                          \
        gte_stotz((gteResultDestination));                                                                                 \
        addPrim(&(workspace)->ot[((u32)(workspace)->gteResult << (displayState)->otDepthShift) >> TMD_FT3_OT_INDEX_SHIFT & \
                                 (GPU_ORDERING_TABLE_DEPTH_BYTE_MASK / sizeof(*(workspace)->ot))],                         \
                (triangle));                                                                                               \
    } while (0)

/// Completes and links a projected, front-facing raw-texture quad.
///
/// The first three XY pairs are already stored in the aligned prebuilt
/// `quad`; GTE SXY2 holds corner 3 and the depth FIFO holds all four depths.
/// ZSF4 must be initialized. `gteResultDestination` is `&workspace->gteResult`.
/// Texture fields persist. The wrapped display-scaled bucket must fit the
/// displaced OT base; packet and OT storage must outlive GPU consumption.
/// `rawTextureCommand` is 0x2D (opaque) or 0x2F (semitransparent). Pointer
/// arguments are evaluated repeatedly and must have no side effects; the command
/// is evaluated once. Captures no caller locals. The do/while wrapper is one
/// statement. GTE depth/result state and both DMA tags are updated.
#define TMD_LINK_PROJECTED_RAW_FT4(quad, workspace, gteResultDestination, displayState, rawTextureCommand)                 \
    do {                                                                                                                   \
        enum {                                                                                                             \
            TMD_FT4_OT_INDEX_SHIFT = 4                                                                                     \
        };                                                                                                                 \
                                                                                                                           \
        gte_stsxy2(&(quad)->x3);                                                                                           \
        gte_avsz4();                                                                                                       \
        setlen((quad), (sizeof(*(quad)) - sizeof((quad)->tag)) / sizeof(u32));                                             \
        setcode((quad), (rawTextureCommand));                                                                              \
        gte_stotz((gteResultDestination));                                                                                 \
        addPrim(&(workspace)->ot[((u32)(workspace)->gteResult << (displayState)->otDepthShift) >> TMD_FT4_OT_INDEX_SHIFT & \
                                 (GPU_ORDERING_TABLE_DEPTH_BYTE_MASK / sizeof(*(workspace)->ot))],                         \
                (quad));                                                                                                   \
    } while (0)

u32* tmdXformStreamVertsEnvLayer(TmdStreamWorkspace* workspace, s32 objectFlags, u32* elements)
{
    s32        previousVertexRef;
    s32        elementCount;
    u32        vertexRef;
    const u16* elementHalfwords;
    CVECTOR    referenceColor;
    u8*        textureCoordinate;
    u8*        layerColor;
    s16*       screenComponent;
    SVECTOR*   rotatedNormal;
    s32        colorBlend;
    s32        secondPage;
    s32        textureComponent;

    referenceColor = gGpColorGrey;
    elementCount   = workspace->elemCount;
    if (elementCount == 0) {
        return elements;
    }
    previousVertexRef    = -1;
    workspace->elemCount = elementCount + previousVertexRef;
    if (elementCount > 0) {
        do {
            elementHalfwords = (const u16*)elements;
            vertexRef        = elementHalfwords[0];
            // Consecutive identical references reuse the GTE projection and cached depth.
            if (vertexRef != previousVertexRef) {
                gte_ldv0((const u8*)workspace->verts + (vertexRef & TMD_STREAM_GEOMETRY_BYTE_OFFSET_MASK));
                gte_rtps();
                gte_stsz(&workspace->gteResult);
                gte_stflg(&workspace->gteFlag);
                if (workspace->gteFlag & TMD_GTE_ERROR_FLAG) {
                    workspace->gteResult |= TMD_VERTEX_DEPTH_INVALID;
                }
                workspace->szTable[elementHalfwords[0] >> TMD_STREAM_VERTEX_INDEX_SHIFT] = workspace->gteResult;
            }
            previousVertexRef = elementHalfwords[0];
            gte_stsxy(workspace->preXformWrite + elementHalfwords[2] + 4);
            gte_stsxy(workspace->preXformWrite + elementHalfwords[3] + 4);
            gte_stsxy(&workspace->texCoord);
            gte_ldv0((const u8*)workspace->normals + (elementHalfwords[1] & TMD_STREAM_GEOMETRY_BYTE_OFFSET_MASK));
            gte_ldrgb(&D_80114BA4);
            gte_nccs();
            gte_strgb(workspace->preXformWrite + elementHalfwords[2]);
            gte_ldrgb(&D_80114BA8);
            gte_nccs();
            gte_strgb(workspace->preXformWrite + elementHalfwords[3]);
            gte_rtv0();
            gte_stsv(&workspace->elemNormal);
            elements += workspace->elemStride;
            // Weight the lit layer colour by the object's blend and the grey
            // reference by its complement below the full-colour endpoint.
            colorBlend = workspace->obj->shading.colorBlend;
            if (colorBlend < TMD_OBJECT_COLOR_BLEND_ONE) {
                gte_lddp(colorBlend);
                layerColor = workspace->preXformWrite + elementHalfwords[2];
                gte_ldcv(layerColor);
                gte_gpf12();
                gte_lddp(TMD_OBJECT_COLOR_BLEND_ONE - colorBlend);
                gte_ldcv(&referenceColor);
                gte_gpl12();
                gte_stcv(layerColor);
            }
            screenComponent   = &workspace->texCoord.vx;
            secondPage        = TMD_ENV_FIRST_PAGE_MARKER;
            textureCoordinate = workspace->preXformWrite + elementHalfwords[2] + 8;
            rotatedNormal     = &workspace->elemNormal;
            TMD_CALCULATE_ENVIRONMENT_CORNER_UV(workspace, rotatedNormal, screenComponent, textureCoordinate, textureComponent, secondPage);
            *textureCoordinate                                 = textureComponent;
            textureCoordinate[-TMD_ENV_V_TO_PAGE_MARKER_BYTES] = secondPage;
        } while (workspace->elemCount-- > 0);
    }
    return elements;
}

u32* tmdXformStreamVertsOffsetLayer(TmdStreamWorkspace* workspace, s32 objectFlags, u32* elements)
{
    enum { TMD_OFFSET_LAYER_COLOR_SHIFT = 5 };
    enum { TMD_PREVIOUS_VERTEX_REF_NONE = -1 };
    s32        previousVertexRef;
    s32        elementCount;
    u32        vertexRef;
    const u16* elementHalfwords;
    CVECTOR    layerMaterial;
    CVECTOR    baseMaterial;
    u8*        screenDestination;
    s32        layerIntensity;
    s32        baseIntensity;

    layerMaterial   = gGpColorWhite;
    baseMaterial    = gGpColorGrey;
    layerIntensity  = workspace->obj->shading.colorBlend >> TMD_OFFSET_LAYER_COLOR_SHIFT;
    baseIntensity   = (TMD_OBJECT_COLOR_BLEND_ONE >> TMD_OFFSET_LAYER_COLOR_SHIFT) - layerIntensity;
    layerMaterial.b = layerIntensity;
    layerMaterial.g = layerIntensity;
    layerMaterial.r = layerIntensity;
    baseMaterial.b  = baseIntensity;
    baseMaterial.g  = baseIntensity;
    baseMaterial.r  = baseIntensity;
    elementCount    = workspace->elemCount;
    if (elementCount == 0) {
        return elements;
    }
    previousVertexRef    = TMD_PREVIOUS_VERTEX_REF_NONE;
    workspace->elemCount = elementCount + previousVertexRef;
    if (elementCount > 0) {
        do {
            elementHalfwords = (const u16*)elements;
            vertexRef        = elementHalfwords[0];
            // Reuse screen XY and cached Z only for consecutive identical references.
            if (vertexRef != previousVertexRef) {
                gte_ldv0((const u8*)workspace->verts + (vertexRef & TMD_STREAM_GEOMETRY_BYTE_OFFSET_MASK));
                gte_rtps();
                gte_stsz(&workspace->gteResult);
                gte_stflg(&workspace->gteFlag);
                if (workspace->gteFlag & TMD_GTE_ERROR_FLAG) {
                    workspace->gteResult |= TMD_VERTEX_DEPTH_INVALID;
                }
                workspace->szTable[elementHalfwords[0] >> TMD_STREAM_VERTEX_INDEX_SHIFT] = workspace->gteResult;
            }
            previousVertexRef = elementHalfwords[0];
            screenDestination = workspace->preXformWrite + elementHalfwords[2] + sizeof(CVECTOR);
            gte_stsxy(screenDestination);
            screenDestination = workspace->preXformWrite + elementHalfwords[3] + sizeof(CVECTOR);
            gte_stsxy(screenDestination);
            gte_stsxy(&workspace->texCoord);
            TMD_LIGHT_OFFSET_LAYER_CORNER(workspace, elementHalfwords, &layerMaterial, &baseMaterial);
            elements += workspace->elemStride;
        } while (workspace->elemCount-- > 0);
    }
    return elements;
}

u32* tmdDrawStreamPrimGt3EnvLayer(TmdStreamWorkspace* workspace, s32 objectFlags, u32* elements)
{
    enum { TMD_GT3_OPAQUE_COMMAND     = 0x34,
           TMD_GT3_SEMI_TRANS_COMMAND = 0x36 };
    POLY_GT3*  packetPair;
    const u16* elementHalfwords;
    const u8*  vertexBytes;
    const u8*  normalBytes;
    CVECTOR    referenceColor;
    SVECTOR*   rotatedNormal;
    s16*       screenComponent;
    u8*        textureCoordinate;
    u8*        layerColor;
    u8*        pageMarker;
    u8*        textureU;
    s32        anySecondPage;
    s32        secondPage;
    s32        textureComponent;
    s32        cornerIndex;
    s32        payloadWordCount;

    packetPair       = (POLY_GT3*)workspace->primWrite;
    referenceColor   = gGpColorGrey;
    payloadWordCount = sizeof(*packetPair) / sizeof(u32) - 1;
    if (workspace->elemCount-- > 0) {
        do {
            elementHalfwords = (const u16*)elements;
            vertexBytes      = (const u8*)workspace->verts;
            // Project packed byte references; packet slots are consumed even on rejection.
            gte_ldv3(vertexBytes + (elementHalfwords[0] & TMD_STREAM_GEOMETRY_BYTE_OFFSET_MASK), vertexBytes + (elementHalfwords[1] & TMD_STREAM_GEOMETRY_BYTE_OFFSET_MASK), vertexBytes + (elementHalfwords[2] & TMD_STREAM_GEOMETRY_BYTE_OFFSET_MASK));
            gte_rtpt();
            gte_stflg(&workspace->gteFlag);
            if (workspace->gteFlag >= 0) {
                gte_nclip();
                gte_stopz(&workspace->gteResult);
                if (workspace->gteResult > 0) {
                    gte_stsxy3_gt3(&packetPair[0]);
                    gte_stsxy3_gt3(&packetPair[1]);
                    gte_avsz3();
                    normalBytes = (const u8*)workspace->normals;
                    gte_ldv3(normalBytes + (elementHalfwords[3] & TMD_STREAM_GEOMETRY_BYTE_OFFSET_MASK), normalBytes + (elementHalfwords[4] & TMD_STREAM_GEOMETRY_BYTE_OFFSET_MASK), normalBytes + (elementHalfwords[5] & TMD_STREAM_GEOMETRY_BYTE_OFFSET_MASK));
                    gte_ldrgb(&D_80114BA4);
                    gte_ncct();
                    gte_strgb3_gt3(&packetPair[0]);
                    gte_ldrgb(&D_80114BA8);
                    gte_ncct();
                    gte_strgb3_gt3(&packetPair[1]);
                    if (workspace->obj->shading.colorBlend < TMD_OBJECT_COLOR_BLEND_ONE) {
                        TMD_BLEND_ENVIRONMENT_LAYER_COLOR(workspace, layerColor, &packetPair[0].r0, &referenceColor);
                        TMD_BLEND_ENVIRONMENT_LAYER_COLOR(workspace, layerColor, &packetPair[0].r1, &referenceColor);
                        TMD_BLEND_ENVIRONMENT_LAYER_COLOR(workspace, layerColor, &packetPair[0].r2, &referenceColor);
                    }
                    // Map screen pixels minus scaled view-space normals into the environment texture.
                    gte_rtv0();
                    gte_stsv(&workspace->elemNormal);
                    anySecondPage     = 0;
                    screenComponent   = &packetPair[0].x0;
                    textureCoordinate = &packetPair[0].u0;
                    secondPage        = TMD_ENV_FIRST_PAGE_MARKER;
                    rotatedNormal     = &workspace->elemNormal;
                    TMD_CALCULATE_ENVIRONMENT_CORNER_UV(workspace, rotatedNormal, screenComponent, textureCoordinate, textureComponent, secondPage);
                    anySecondPage                                     |= secondPage;
                    *textureCoordinate                                 = textureComponent;
                    textureCoordinate[-TMD_ENV_V_TO_PAGE_MARKER_BYTES] = secondPage;

                    gte_rtv1();
                    gte_stsv(&workspace->elemNormal);
                    screenComponent   = &packetPair[0].x1;
                    textureCoordinate = &packetPair[0].u1;
                    secondPage        = TMD_ENV_FIRST_PAGE_MARKER;
                    rotatedNormal     = &workspace->elemNormal;
                    TMD_CALCULATE_ENVIRONMENT_CORNER_UV(workspace, rotatedNormal, screenComponent, textureCoordinate, textureComponent, secondPage);
                    anySecondPage                                     |= secondPage;
                    *textureCoordinate                                 = textureComponent;
                    textureCoordinate[-TMD_ENV_V_TO_PAGE_MARKER_BYTES] = secondPage;

                    gte_rtv2();
                    gte_stsv(&workspace->elemNormal);
                    screenComponent   = &packetPair[0].x2;
                    textureCoordinate = &packetPair[0].u2;
                    secondPage        = TMD_ENV_FIRST_PAGE_MARKER;
                    rotatedNormal     = &workspace->elemNormal;
                    TMD_CALCULATE_ENVIRONMENT_CORNER_UV(workspace, rotatedNormal, screenComponent, textureCoordinate, textureComponent, secondPage);
                    anySecondPage                                     |= secondPage;
                    *textureCoordinate                                 = textureComponent;
                    textureCoordinate[-TMD_ENV_V_TO_PAGE_MARKER_BYTES] = secondPage;

                    if (anySecondPage == 0) {
                        packetPair[0].tpage = TMD_ENV_FIRST_TEXTURE_PAGE;
                    } else {
                        // Rebase unmarked U bytes when any corner selects the second page.
                        pageMarker  = &packetPair[0].code;
                        cornerIndex = 0;
                        textureU    = &packetPair[0].u0;
                        TMD_FOLD_ENVIRONMENT_PAGE_U(pageMarker, textureU, cornerIndex, TMD_GT3_CORNER_COUNT);
                        packetPair[0].tpage = TMD_ENV_SECOND_TEXTURE_PAGE;
                    }
                    setlen(&packetPair[0], payloadWordCount);
                    setcode(&packetPair[0], TMD_GT3_SEMI_TRANS_COMMAND);
                    setlen(&packetPair[1], payloadWordCount);
                    setcode(&packetPair[1], TMD_GT3_OPAQUE_COMMAND);
                    gte_stotz(&workspace->gteResult);
                    addPrim((&workspace->ot[(((((u32)workspace->gteResult << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)) / sizeof(*workspace->ot)]), &packetPair[0]);
                    addPrim((&workspace->ot[(((((u32)workspace->gteResult << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)) / sizeof(*workspace->ot)]), &packetPair[1]);
                }
            }
            packetPair += 2;
            elements   += workspace->elemStride;
        } while (workspace->elemCount-- > 0);
    }
    workspace->primWrite = (u8*)packetPair;
    return elements;
}

u32* tmdDrawStreamPrimGt3OffsetLayer(TmdStreamWorkspace* workspace, s32 objectFlags, u32* elements)
{
    enum { TMD_OFFSET_LAYER_COLOR_SHIFT       = 5,
           TMD_OFFSET_LAYER_TPAGE_ABR_LOW_BIT = 1 << 5 };
    enum { TMD_GT3_OPAQUE_COMMAND     = 0x34,
           TMD_GT3_SEMI_TRANS_COMMAND = 0x36 };
    POLY_GT3*           packetPair;
    s32*                gteResultDestination;
    const DisplayState* displayState;
    u32                 addressMask;
    u32                 lengthMask;
    const u16*          elementHalfwords;
    const u8*           vertexBytes;
    const u8*           normalBytes;
    CVECTOR             layerMaterial;
    CVECTOR             baseMaterial;
    s32                 payloadWordCount;
    s32                 baseCommand;
    s32                 layerIntensity;
    s32                 baseIntensity;

    packetPair      = (POLY_GT3*)workspace->primWrite;
    layerMaterial   = gGpColorGrey;
    baseMaterial    = gGpColorGrey;
    layerIntensity  = workspace->obj->shading.colorBlend >> TMD_OFFSET_LAYER_COLOR_SHIFT;
    baseIntensity   = (TMD_OBJECT_COLOR_BLEND_ONE >> TMD_OFFSET_LAYER_COLOR_SHIFT) - layerIntensity;
    layerMaterial.b = layerIntensity;
    layerMaterial.g = layerIntensity;
    layerMaterial.r = layerIntensity;
    baseMaterial.b  = baseIntensity;
    baseMaterial.g  = baseIntensity;
    baseMaterial.r  = baseIntensity;
    if (workspace->elemCount-- > 0) {
        gteResultDestination = &workspace->gteResult;
        payloadWordCount     = sizeof(*packetPair) / sizeof(u32) - 1;
        baseCommand          = TMD_GT3_OPAQUE_COMMAND;
        displayState         = &gDisplayState;
        addressMask          = GPU_DMA_LINK_ADDRESS_MASK;
        lengthMask           = GPU_DMA_PACKET_LENGTH_MASK;
        do {
            elementHalfwords = (const u16*)elements;
            vertexBytes      = (const u8*)workspace->verts;
            // Project packed byte references; packet slots are consumed even on rejection.
            gte_ldv3(vertexBytes + (elementHalfwords[0] & TMD_STREAM_GEOMETRY_BYTE_OFFSET_MASK), vertexBytes + (elementHalfwords[1] & TMD_STREAM_GEOMETRY_BYTE_OFFSET_MASK), vertexBytes + (elementHalfwords[2] & TMD_STREAM_GEOMETRY_BYTE_OFFSET_MASK));
            gte_rtpt();
            gte_stflg(&workspace->gteFlag);
            if (workspace->gteFlag >= 0) {
                gte_nclip();
                gte_stopz(gteResultDestination);
                if (workspace->gteResult > 0) {
                    gte_stsxy3_gt3(&packetPair[0]);
                    gte_stsxy3_gt3(&packetPair[1]);
                    gte_avsz3();
                    normalBytes = (const u8*)workspace->normals;
                    gte_ldv3(normalBytes + (elementHalfwords[3] & TMD_STREAM_GEOMETRY_BYTE_OFFSET_MASK), normalBytes + (elementHalfwords[4] & TMD_STREAM_GEOMETRY_BYTE_OFFSET_MASK), normalBytes + (elementHalfwords[5] & TMD_STREAM_GEOMETRY_BYTE_OFFSET_MASK));
                    gte_ldrgb(&layerMaterial);
                    gte_ncct();
                    gte_strgb3_gt3(&packetPair[0]);
                    gte_ldrgb(&baseMaterial);
                    gte_ncct();
                    gte_strgb3_gt3(&packetPair[1]);
                    setlen(&packetPair[0], payloadWordCount);
                    setcode(&packetPair[0], TMD_GT3_SEMI_TRANS_COMMAND);
                    setlen(&packetPair[1], payloadWordCount);
                    setcode(&packetPair[1], baseCommand);
                    packetPair[0].tpage |= TMD_OFFSET_LAYER_TPAGE_ABR_LOW_BIT;
                    gte_stotz(gteResultDestination);
                    TMD_LINK_OFFSET_LAYER_TRIANGLE_PAIR(packetPair, workspace, displayState, addressMask, lengthMask);
                }
            }
            packetPair += 2;
            elements   += workspace->elemStride;
        } while (workspace->elemCount-- > 0);
    }
    workspace->primWrite = (u8*)packetPair;
    return elements;
}

u32* tmdDrawStreamPrimGt4OffsetLayer(TmdStreamWorkspace* workspace, s32 objectFlags, u32* elements)
{
    enum { TMD_GT4_OPAQUE_COMMAND     = 0x3C,
           TMD_GT4_SEMI_TRANS_COMMAND = 0x3E };
    enum { TMD_OFFSET_LAYER_COLOR_SHIFT = 5 };
    POLY_GT4*           packetPair;
    s32*                gteResultDestination;
    const DisplayState* displayState;
    u32                 addressMask;
    u32                 lengthMask;
    u32                 projectionErrorMask;
    s32*                gteFlagDestination;
    const u16*          elementHalfwords;
    const u8*           vertexBytes;
    const u8*           normalBytes;
    CVECTOR             layerMaterial;
    CVECTOR             baseMaterial;
    s32                 payloadWordCount;
    s32                 baseCommand;
    s32                 layerIntensity;
    s32                 baseIntensity;

    packetPair      = (POLY_GT4*)workspace->primWrite;
    layerMaterial   = gGpColorGrey;
    baseMaterial    = gGpColorGrey;
    layerIntensity  = workspace->obj->shading.colorBlend >> TMD_OFFSET_LAYER_COLOR_SHIFT;
    baseIntensity   = (TMD_OBJECT_COLOR_BLEND_ONE >> TMD_OFFSET_LAYER_COLOR_SHIFT) - layerIntensity;
    layerMaterial.b = layerIntensity;
    layerMaterial.g = layerIntensity;
    layerMaterial.r = layerIntensity;
    baseMaterial.b  = baseIntensity;
    baseMaterial.g  = baseIntensity;
    baseMaterial.r  = baseIntensity;
    if (workspace->elemCount-- > 0) {
        gteFlagDestination   = &workspace->gteFlag;
        projectionErrorMask  = TMD_GTE_ERROR_FLAG;
        gteResultDestination = &workspace->gteResult;
        do {
            elementHalfwords = (const u16*)elements;
            vertexBytes      = (const u8*)workspace->verts;
            // Projection errors reject the packet; facing is tested separately below.
            gte_ldv3(vertexBytes + (elementHalfwords[0] & TMD_STREAM_GEOMETRY_BYTE_OFFSET_MASK), vertexBytes + (elementHalfwords[1] & TMD_STREAM_GEOMETRY_BYTE_OFFSET_MASK), vertexBytes + (elementHalfwords[2] & TMD_STREAM_GEOMETRY_BYTE_OFFSET_MASK));
            gte_rtpt();
            gte_stflg(gteFlagDestination);
            if ((workspace->gteFlag & projectionErrorMask) == 0) {
                gte_nclip();
                gte_stopz(gteResultDestination);
                gte_stsxy3_gt4(&packetPair[0]);
                gte_stsxy3_gt4(&packetPair[1]);
                gte_ldv0((const u8*)workspace->verts + (elementHalfwords[3] & TMD_STREAM_GEOMETRY_BYTE_OFFSET_MASK));
                gte_rtps();
                gte_stflg(gteFlagDestination);
                if ((workspace->gteFlag & projectionErrorMask) == 0) {
                    // Draw if NCLIP(0,1,2) > 0, or otherwise NCLIP(1,2,3) < 0.
                    if (workspace->gteResult > 0) {
                        goto draw;
                    }
                    gte_nclip();
                    gte_stopz(gteResultDestination);
                    if (workspace->gteResult < 0) {
                    draw:
                        gte_stsxy2(&packetPair[0].x3);
                        gte_stsxy2(&packetPair[1].x3);
                        gte_avsz4();
                        normalBytes = (const u8*)workspace->normals;
                        gte_ldv3(normalBytes + (elementHalfwords[4] & TMD_STREAM_GEOMETRY_BYTE_OFFSET_MASK), normalBytes + (elementHalfwords[5] & TMD_STREAM_GEOMETRY_BYTE_OFFSET_MASK), normalBytes + (elementHalfwords[6] & TMD_STREAM_GEOMETRY_BYTE_OFFSET_MASK));
                        gte_ldrgb(&layerMaterial);
                        gte_ncct();
                        gte_strgb3_gt4(&packetPair[0]);
                        gte_ldrgb(&baseMaterial);
                        gte_ncct();
                        gte_strgb3_gt4(&packetPair[1]);
                        gte_ldv0((const u8*)workspace->normals + (elementHalfwords[7] & TMD_STREAM_GEOMETRY_BYTE_OFFSET_MASK));
                        gte_ldrgb(&layerMaterial);
                        gte_nccs();
                        gte_strgb(&packetPair[0].r3);
                        gte_ldrgb(&baseMaterial);
                        gte_nccs();
                        gte_strgb(&packetPair[1].r3);
                        payloadWordCount = sizeof(*packetPair) / sizeof(u32) - 1;
                        baseCommand      = TMD_GT4_OPAQUE_COMMAND;
                        displayState     = &gDisplayState;
                        addressMask      = GPU_DMA_LINK_ADDRESS_MASK;
                        lengthMask       = GPU_DMA_PACKET_LENGTH_MASK;
                        setlen(&packetPair[0], payloadWordCount);
                        setcode(&packetPair[0], TMD_GT4_SEMI_TRANS_COMMAND);
                        setlen(&packetPair[1], payloadWordCount);
                        setcode(&packetPair[1], baseCommand);
                        gte_stotz(gteResultDestination);
                        _modelLightingLinkOffsetLayerQuadPair(packetPair, workspace, displayState, addressMask, lengthMask);
                    }
                }
            }
            packetPair += 2;
            elements   += workspace->elemStride;
        } while (workspace->elemCount-- > 0);
    }
    workspace->primWrite = (u8*)packetPair;
    return elements;
}

u32* tmdDrawStreamPrimGt4EnvLayer(TmdStreamWorkspace* workspace, s32 objectFlags, u32* elements)
{
    enum { TMD_GT4_OPAQUE_COMMAND     = 0x3C,
           TMD_GT4_SEMI_TRANS_COMMAND = 0x3E };
    POLY_GT4*  packetPair;
    s32*       gteResultDestination;
    s32*       gteFlagDestination;
    const u16* elementHalfwords;
    const u8*  vertexBytes;
    const u8*  normalBytes;
    CVECTOR    referenceColor;
    SVECTOR*   rotatedNormal;
    s16*       screenComponent;
    s16*       corner3ScreenPosition;
    u8*        corner3LayerColor;
    u8*        textureCoordinate;
    u8*        textureU;
    u8*        layerColor;
    u8*        pageMarker;
    s32        secondPage;
    s32        anySecondPage;
    s32        textureComponent;
    s32        cornerIndex;
    s32        payloadWordCount;

    packetPair     = (POLY_GT4*)workspace->primWrite;
    referenceColor = gGpColorGrey;
    if (workspace->elemCount-- > 0) {
        gteFlagDestination   = &workspace->gteFlag;
        gteResultDestination = &workspace->gteResult;
        do {
            elementHalfwords = (const u16*)elements;
            vertexBytes      = (const u8*)workspace->verts;
            // Project packed byte references; packet slots are consumed even on rejection.
            gte_ldv3(vertexBytes + (elementHalfwords[0] & TMD_STREAM_GEOMETRY_BYTE_OFFSET_MASK), vertexBytes + (elementHalfwords[1] & TMD_STREAM_GEOMETRY_BYTE_OFFSET_MASK), vertexBytes + (elementHalfwords[2] & TMD_STREAM_GEOMETRY_BYTE_OFFSET_MASK));
            gte_rtpt();
            gte_stflg(gteFlagDestination);
            if ((workspace->gteFlag & TMD_GTE_ERROR_FLAG) == 0) {
                gte_nclip();
                gte_stopz(gteResultDestination);
                gte_stsxy3_gt4(&packetPair[0]);
                gte_stsxy3_gt4(&packetPair[1]);
                gte_ldv0((const u8*)workspace->verts + (elementHalfwords[3] & TMD_STREAM_GEOMETRY_BYTE_OFFSET_MASK));
                gte_rtps();
                gte_stflg(gteFlagDestination);
                if ((workspace->gteFlag & TMD_GTE_ERROR_FLAG) == 0) {
                    gte_nclip();
                    gte_stsxy2(&packetPair[0].x3);
                    gte_stsxy2(&packetPair[1].x3);
                    // Collapse the rejected half into a repeated endpoint; retain AVSZ4 depths.
                    if (workspace->gteResult <= 0) {
                        *(u_long*)&packetPair[0].x0 = *(u_long*)&packetPair[0].x1;
                        *(u_long*)&packetPair[1].x0 = *(u_long*)&packetPair[1].x1;
                        gte_stopz(gteResultDestination);
                        if (workspace->gteResult >= 0) {
                            goto skip;
                        }
                    } else {
                        gte_stopz(gteResultDestination);
                        if (workspace->gteResult >= 0) {
                            *(u_long*)&packetPair[0].x3 = *(u_long*)&packetPair[0].x2;
                            *(u_long*)&packetPair[1].x3 = *(u_long*)&packetPair[1].x2;
                        }
                    }
                    gte_avsz4();
                    normalBytes = (const u8*)workspace->normals;
                    gte_ldv3(normalBytes + (elementHalfwords[4] & TMD_STREAM_GEOMETRY_BYTE_OFFSET_MASK), normalBytes + (elementHalfwords[5] & TMD_STREAM_GEOMETRY_BYTE_OFFSET_MASK), normalBytes + (elementHalfwords[6] & TMD_STREAM_GEOMETRY_BYTE_OFFSET_MASK));
                    gte_ldrgb(&D_80114BA4);
                    gte_ncct();
                    gte_strgb3_gt4(&packetPair[0]);
                    gte_ldrgb(&D_80114BA8);
                    gte_ncct();
                    gte_strgb3_gt4(&packetPair[1]);
                    // Map screen pixels minus scaled view-space normals into the environment texture.
                    gte_rtv0();
                    gte_stsv(&workspace->elemNormal);
                    anySecondPage     = 0;
                    screenComponent   = &packetPair[0].x0;
                    textureCoordinate = &packetPair[0].u0;
                    secondPage        = TMD_ENV_FIRST_PAGE_MARKER;
                    rotatedNormal     = &workspace->elemNormal;
                    TMD_CALCULATE_ENVIRONMENT_CORNER_UV(workspace, rotatedNormal, screenComponent, textureCoordinate, textureComponent, secondPage);
                    *textureCoordinate = textureComponent;
                    textureCoordinate -= TMD_ENV_V_TO_PAGE_MARKER_BYTES;
                    *textureCoordinate = secondPage;
                    anySecondPage     |= secondPage;
                    gte_rtv1();
                    gte_stsv(&workspace->elemNormal);
                    screenComponent   = &packetPair[0].x1;
                    textureCoordinate = &packetPair[0].u1;
                    secondPage        = TMD_ENV_FIRST_PAGE_MARKER;
                    rotatedNormal     = &workspace->elemNormal;
                    TMD_CALCULATE_ENVIRONMENT_CORNER_UV(workspace, rotatedNormal, screenComponent, textureCoordinate, textureComponent, secondPage);
                    *textureCoordinate = textureComponent;
                    textureCoordinate -= TMD_ENV_V_TO_PAGE_MARKER_BYTES;
                    *textureCoordinate = secondPage;
                    anySecondPage     |= secondPage;
                    gte_rtv2();
                    gte_stsv(&workspace->elemNormal);
                    screenComponent   = &packetPair[0].x2;
                    textureCoordinate = &packetPair[0].u2;
                    secondPage        = TMD_ENV_FIRST_PAGE_MARKER;
                    rotatedNormal     = &workspace->elemNormal;
                    TMD_CALCULATE_ENVIRONMENT_CORNER_UV(workspace, rotatedNormal, screenComponent, textureCoordinate, textureComponent, secondPage);
                    *textureCoordinate    = textureComponent;
                    textureCoordinate    -= TMD_ENV_V_TO_PAGE_MARKER_BYTES;
                    *textureCoordinate    = secondPage;
                    anySecondPage        |= secondPage;
                    corner3ScreenPosition = &packetPair[0].x3;
                    gte_ldv0((const u8*)workspace->normals + (elementHalfwords[7] & TMD_STREAM_GEOMETRY_BYTE_OFFSET_MASK));
                    gte_ldrgb(&D_80114BA4);
                    gte_nccs();
                    gte_strgb(&packetPair[0].r3);
                    gte_ldrgb(&D_80114BA8);
                    gte_nccs();
                    gte_strgb(&packetPair[1].r3);
                    gte_rtv0();
                    gte_stsv(&workspace->elemNormal);
                    screenComponent   = corner3ScreenPosition;
                    textureCoordinate = &packetPair[0].u3;
                    secondPage        = TMD_ENV_FIRST_PAGE_MARKER;
                    rotatedNormal     = &workspace->elemNormal;
                    TMD_CALCULATE_ENVIRONMENT_CORNER_UV(workspace, rotatedNormal, screenComponent, textureCoordinate, textureComponent, secondPage);
                    *textureCoordinate = textureComponent;
                    textureCoordinate -= TMD_ENV_V_TO_PAGE_MARKER_BYTES;
                    *textureCoordinate = secondPage;

                    anySecondPage |= secondPage;
                    if (workspace->obj->shading.colorBlend < TMD_OBJECT_COLOR_BLEND_ONE) {
                        TMD_BLEND_ENVIRONMENT_LAYER_COLOR(workspace, layerColor, &packetPair[0].r0, &referenceColor);

                        TMD_BLEND_ENVIRONMENT_LAYER_COLOR(workspace, layerColor, &packetPair[0].r1, &referenceColor);

                        TMD_BLEND_ENVIRONMENT_LAYER_COLOR(workspace, layerColor, &packetPair[0].r2, &referenceColor);

                        TMD_BLEND_ENVIRONMENT_LAYER_COLOR(workspace, corner3LayerColor, &packetPair[0].r3, &referenceColor);
                    }

                    pageMarker = &packetPair[0].code;
                    if (anySecondPage == 0) {
                        packetPair[0].tpage = TMD_ENV_FIRST_TEXTURE_PAGE;
                    } else {
                        cornerIndex = 0;
                        textureU    = &packetPair[0].u0;
                        TMD_FOLD_ENVIRONMENT_PAGE_U(pageMarker, textureU, cornerIndex, TMD_GT4_CORNER_COUNT);
                        packetPair[0].tpage = TMD_ENV_SECOND_TEXTURE_PAGE;
                    }

                    payloadWordCount = sizeof(*packetPair) / sizeof(u32) - 1;
                    setlen(&packetPair[0], payloadWordCount);
                    setcode(&packetPair[0], TMD_GT4_SEMI_TRANS_COMMAND);
                    setlen(&packetPair[1], payloadWordCount);
                    setcode(&packetPair[1], TMD_GT4_OPAQUE_COMMAND);
                    gte_stotz(gteResultDestination);
                    addPrim((&workspace->ot[(((((u32)workspace->gteResult << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)) / sizeof(*workspace->ot)]), &packetPair[0]);
                    addPrim((&workspace->ot[(((((u32)workspace->gteResult << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)) / sizeof(*workspace->ot)]), &packetPair[1]);
                }
            }
        skip:
            packetPair += 2;
            elements   += workspace->elemStride;
        } while (workspace->elemCount-- > 0);
    }
    workspace->primWrite = (u8*)packetPair;
    return elements;
}

u32* tmdDrawStreamPrimGt3ElemColor(TmdStreamWorkspace* workspace, s32 objectFlags, u32* elements)
{
    enum { TMD_GT3_ELEMENT_COLOR_WORD_INDEX = 3 };
    enum { TMD_GT3_OPAQUE_COMMAND     = 0x34,
           TMD_GT3_SEMI_TRANS_COMMAND = 0x36 };
    POLY_GT3*           triangle;
    s32*                gteResultDestination;
    const DisplayState* displayState;
    const u16*          elementHalfwords;
    const u8*           vertexBytes;
    const u8*           normalBytes;

    triangle = (POLY_GT3*)workspace->primWrite;
    if (workspace->elemCount-- > 0) {
        gteResultDestination = &workspace->gteResult;
        displayState         = &gDisplayState;
        do {
            elementHalfwords = (const u16*)elements;
            vertexBytes      = (const u8*)workspace->verts;
            // Project packed byte references; packet slots are consumed even on rejection.
            gte_ldv3(vertexBytes + (elementHalfwords[0] & TMD_STREAM_GEOMETRY_BYTE_OFFSET_MASK), vertexBytes + (elementHalfwords[1] & TMD_STREAM_GEOMETRY_BYTE_OFFSET_MASK), vertexBytes + (elementHalfwords[2] & TMD_STREAM_GEOMETRY_BYTE_OFFSET_MASK));
            gte_rtpt();
            gte_stflg(&workspace->gteFlag);
            if (workspace->gteFlag >= 0) {
                gte_nclip();
                gte_stopz(gteResultDestination);
                if (workspace->gteResult > 0) {
                    gte_ldrgb(elements + TMD_GT3_ELEMENT_COLOR_WORD_INDEX);
                    gte_stsxy3_gt3(triangle);
                    gte_avsz3();
                    normalBytes = (const u8*)workspace->normals;
                    gte_ldv3(normalBytes + (elementHalfwords[3] & TMD_STREAM_GEOMETRY_BYTE_OFFSET_MASK), normalBytes + (elementHalfwords[4] & TMD_STREAM_GEOMETRY_BYTE_OFFSET_MASK), normalBytes + (elementHalfwords[5] & TMD_STREAM_GEOMETRY_BYTE_OFFSET_MASK));
                    gte_ncct();
                    gte_strgb3_gt3(triangle);
                    setlen(triangle, sizeof(*triangle) / sizeof(u32) - 1);
                    setcode(triangle, TMD_GT3_OPAQUE_COMMAND);
                    if (workspace->obj->flags & TMD_OBJECT_SEMI_TRANS) {
                        setcode(triangle, TMD_GT3_SEMI_TRANS_COMMAND);
                    }
                    gte_stotz(gteResultDestination);
                    addPrim(&workspace->ot[((u32)workspace->gteResult << displayState->otDepthShift) >> TMD_DRAW_OT_INDEX_SHIFT & (GPU_ORDERING_TABLE_DEPTH_BYTE_MASK / sizeof(*workspace->ot))], triangle);
                }
            }
            triangle++;
            elements += workspace->elemStride;
        } while (workspace->elemCount-- > 0);
    }
    workspace->primWrite = (u8*)triangle;
    return elements;
}

u32* tmdDrawStreamPrimGt4ElemColor(TmdStreamWorkspace* workspace, s32 objectFlags, u32* elements)
{
    enum { TMD_GT4_ELEMENT_COLOR_WORD_INDEX = 4 };
    enum { TMD_GT4_OPAQUE_COMMAND     = 0x3C,
           TMD_GT4_SEMI_TRANS_COMMAND = 0x3E };
    POLY_GT4*           quad;
    s32*                gteResultDestination;
    const DisplayState* displayState;
    u32                 projectionErrorMask;
    s32*                gteFlagDestination;
    const u16*          elementHalfwords;
    const u8*           vertexBytes;

    quad = (POLY_GT4*)workspace->primWrite;
    if (workspace->elemCount-- > 0) {
        gteFlagDestination   = &workspace->gteFlag;
        projectionErrorMask  = TMD_GTE_ERROR_FLAG;
        gteResultDestination = &workspace->gteResult;
        displayState         = &gDisplayState;
        do {
            elementHalfwords = (const u16*)elements;
            vertexBytes      = (const u8*)workspace->verts;
            // Projection errors reject the packet; facing is tested separately below.
            gte_ldv3(vertexBytes + (elementHalfwords[0] & TMD_STREAM_GEOMETRY_BYTE_OFFSET_MASK), vertexBytes + (elementHalfwords[1] & TMD_STREAM_GEOMETRY_BYTE_OFFSET_MASK), vertexBytes + (elementHalfwords[2] & TMD_STREAM_GEOMETRY_BYTE_OFFSET_MASK));
            gte_rtpt();
            gte_stflg(gteFlagDestination);
            if ((workspace->gteFlag & projectionErrorMask) == 0) {
                gte_nclip();
                gte_stopz(gteResultDestination);
                gte_ldrgb(elements + TMD_GT4_ELEMENT_COLOR_WORD_INDEX);
                gte_stsxy3_gt4(quad);
                gte_ldv0((const u8*)workspace->verts + (elementHalfwords[3] & TMD_STREAM_GEOMETRY_BYTE_OFFSET_MASK));
                gte_rtps();
                gte_stflg(gteFlagDestination);
                if ((workspace->gteFlag & projectionErrorMask) == 0) {
                    // Draw if NCLIP(0,1,2) > 0, or otherwise NCLIP(1,2,3) < 0.
                    if (workspace->gteResult > 0) {
                        goto draw;
                    }
                    gte_nclip();
                    gte_stopz(gteResultDestination);
                    if (workspace->gteResult < 0) {
                    draw:
                        gte_stsxy2(&quad->x3);
                        gte_avsz4();
                        _modelLightingLightGt4CornerNormals(quad, workspace, (const _ModelLightingGt4GeometryRefs*)elementHalfwords);
                        setlen(quad, sizeof(*quad) / sizeof(u32) - 1);
                        setcode(quad, TMD_GT4_OPAQUE_COMMAND);
                        if (workspace->obj->flags & TMD_OBJECT_SEMI_TRANS) {
                            setcode(quad, TMD_GT4_SEMI_TRANS_COMMAND);
                        }
                        gte_stotz(gteResultDestination);
                        addPrim(&workspace->ot[((u32)workspace->gteResult << displayState->otDepthShift) >> TMD_DRAW_OT_INDEX_SHIFT & (GPU_ORDERING_TABLE_DEPTH_BYTE_MASK / sizeof(*workspace->ot))], quad);
                    }
                }
            }
            quad++;
            elements += workspace->elemStride;
        } while (workspace->elemCount-- > 0);
    }
    workspace->primWrite = (u8*)quad;
    return elements;
}

u32* tmdDrawStreamPrimFt3(TmdStreamWorkspace* workspace, s32 objectFlags, u32* elements)
{
    enum { TMD_FT3_VERTEX_BYTE_OFFSET_MASK = 0xFFF8 }; // Keep aligned eight-byte vertex offsets
    POLY_FT3*           triangle;
    s32*                gteResultDestination;
    const DisplayState* displayState;
    const u16*          vertexRefs;
    const u8*           vertexBytes;

    triangle = (POLY_FT3*)workspace->primWrite;
    if (workspace->elemCount-- > 0) {
        gteResultDestination = &workspace->gteResult;
        displayState         = &gDisplayState;
        do {
            // Stream halfwords encode byte offsets; low reference bits are discarded.
            vertexRefs  = (const u16*)elements;
            vertexBytes = (const u8*)workspace->verts;
            gte_ldv3(vertexBytes + (vertexRefs[0] & TMD_FT3_VERTEX_BYTE_OFFSET_MASK),
                     vertexBytes + (vertexRefs[1] & TMD_FT3_VERTEX_BYTE_OFFSET_MASK),
                     vertexBytes + (vertexRefs[2] & TMD_FT3_VERTEX_BYTE_OFFSET_MASK));
            gte_rtpt();
            gte_stflg(&workspace->gteFlag);
            if (workspace->gteFlag >= 0) {
                gte_nclip();
                gte_stopz(gteResultDestination);
                if (workspace->gteResult > 0) {
                    TMD_LINK_PROJECTED_RAW_FT3(triangle, workspace, gteResultDestination, displayState, TMD_FT3_RAW_OPAQUE_COMMAND);
                }
            }
            // Rejected triangles consume their construction-reserved slots too.
            triangle++;
            elements += workspace->elemStride;
        } while (workspace->elemCount-- > 0);
    }
    workspace->primWrite = (u8*)triangle;
    return elements;
}

u32* tmdDrawStreamPrimFt4(TmdStreamWorkspace* workspace, s32 objectFlags, u32* elements)
{
    enum { TMD_FT4_VERTEX_BYTE_OFFSET_MASK = 0xFFF8 }; // Keep aligned eight-byte vertex offsets
    POLY_FT4*           quad;
    s32*                gteResultDestination;
    const DisplayState* displayState;
    u32                 projectionErrorMask;
    s32*                gteFlagDestination;
    const u16*          vertexRefs;
    const u8*           vertexBytes;

    quad = (POLY_FT4*)workspace->primWrite;
    if (workspace->elemCount-- > 0) {
        gteFlagDestination   = &workspace->gteFlag;
        projectionErrorMask  = TMD_GTE_ERROR_FLAG;
        gteResultDestination = &workspace->gteResult;
        displayState         = &gDisplayState;
        do {
            vertexRefs  = (const u16*)elements;
            vertexBytes = (const u8*)workspace->verts;
            gte_ldv3(vertexBytes + (vertexRefs[0] & TMD_FT4_VERTEX_BYTE_OFFSET_MASK),
                     vertexBytes + (vertexRefs[1] & TMD_FT4_VERTEX_BYTE_OFFSET_MASK),
                     vertexBytes + (vertexRefs[2] & TMD_FT4_VERTEX_BYTE_OFFSET_MASK));
            gte_rtpt();
            gte_stflg(gteFlagDestination);
            if ((workspace->gteFlag & projectionErrorMask) == 0) {
                gte_nclip();
                gte_stopz(gteResultDestination);
                // Retain the first facing result while projecting the fourth corner.
                gte_stsxy3_ft4(quad);
                gte_ldv0((const u8*)workspace->verts + (vertexRefs[3] & TMD_FT4_VERTEX_BYTE_OFFSET_MASK));
                gte_rtps();
                gte_stflg(gteFlagDestination);
                if ((workspace->gteFlag & projectionErrorMask) == 0) {
                    if (workspace->gteResult > 0) {
                        goto drawQuad;
                    }
                    // The FIFO now holds corners 1,2,3; either half may face forward.
                    gte_nclip();
                    gte_stopz(gteResultDestination);
                    if (workspace->gteResult < 0) {
                    drawQuad:
                        TMD_LINK_PROJECTED_RAW_FT4(quad, workspace, gteResultDestination, displayState, TMD_FT4_RAW_OPAQUE_COMMAND);
                    }
                }
            }
            // Rejected quads may keep partial XY writes and still consume a slot.
            quad++;
            elements += workspace->elemStride;
        } while (workspace->elemCount-- > 0);
    }
    workspace->primWrite = (u8*)quad;
    return elements;
}

u32* tmdDrawStreamPrimGt4Unlit(TmdStreamWorkspace* workspace, s32 objectFlags, u32* elements)
{
    POLY_GT4*           quad;
    s32*                gteResultDestination;
    const DisplayState* displayState;
    u32                 projectionErrorMask;
    s32*                gteFlagDestination;
    const u16*          elementHalfwords;
    const u8*           vertexBytes;

    quad = (POLY_GT4*)workspace->primWrite;
    if (workspace->elemCount-- > 0) {
        gteFlagDestination   = &workspace->gteFlag;
        projectionErrorMask  = TMD_GTE_ERROR_FLAG;
        gteResultDestination = &workspace->gteResult;
        displayState         = &gDisplayState;
        do {
            elementHalfwords = (const u16*)elements;
            vertexBytes      = (const u8*)workspace->verts;
            // Project packed byte references; packet slots are consumed even on rejection.
            gte_ldv3(vertexBytes + (elementHalfwords[0] & TMD_STREAM_GEOMETRY_BYTE_OFFSET_MASK), vertexBytes + (elementHalfwords[1] & TMD_STREAM_GEOMETRY_BYTE_OFFSET_MASK), vertexBytes + (elementHalfwords[2] & TMD_STREAM_GEOMETRY_BYTE_OFFSET_MASK));
            gte_rtpt();
            gte_stflg(gteFlagDestination);
            if ((workspace->gteFlag & projectionErrorMask) == 0) {
                gte_nclip();
                gte_stopz(gteResultDestination);
                gte_stsxy3_gt4(quad);
                gte_ldv0((const u8*)workspace->verts + (elementHalfwords[3] & TMD_STREAM_GEOMETRY_BYTE_OFFSET_MASK));
                gte_rtps();
                gte_stflg(gteFlagDestination);
                if ((workspace->gteFlag & projectionErrorMask) == 0) {
                    if (workspace->gteResult > 0) {
                        goto draw;
                    }
                    gte_nclip();
                    gte_stopz(gteResultDestination);
                    if (workspace->gteResult < 0) {
                    draw:
                        gte_stsxy2(&quad->x3);
                        gte_avsz4();
                        gte_stotz(gteResultDestination);
                        addPrim(&workspace->ot[((u32)workspace->gteResult << displayState->otDepthShift) >> TMD_DRAW_OT_INDEX_SHIFT & (GPU_ORDERING_TABLE_DEPTH_BYTE_MASK / sizeof(*workspace->ot))], quad);
                    }
                }
            }
            quad++;
            elements += workspace->elemStride;
        } while (workspace->elemCount-- > 0);
    }
    workspace->primWrite = (u8*)quad;
    return elements;
}

u32* tmdDrawStreamPrimF4(TmdStreamWorkspace* workspace, s32 objectFlags, u32* elements)
{
    enum { TMD_F4_OPAQUE_COMMAND = 0x28 };
    POLY_F4*            quad;
    s32*                gteResultDestination;
    const DisplayState* displayState;
    u32                 projectionErrorMask;
    s32*                gteFlagDestination;
    const u16*          elementHalfwords;
    const u8*           vertexBytes;

    quad = (POLY_F4*)workspace->primWrite;
    if (workspace->elemCount-- > 0) {
        gteFlagDestination   = &workspace->gteFlag;
        projectionErrorMask  = TMD_GTE_ERROR_FLAG;
        gteResultDestination = &workspace->gteResult;
        displayState         = &gDisplayState;
        do {
            elementHalfwords = (const u16*)elements;
            vertexBytes      = (const u8*)workspace->verts;
            // Project packed byte references; packet slots are consumed even on rejection.
            gte_ldv3(vertexBytes + (elementHalfwords[0] & TMD_STREAM_GEOMETRY_BYTE_OFFSET_MASK), vertexBytes + (elementHalfwords[1] & TMD_STREAM_GEOMETRY_BYTE_OFFSET_MASK), vertexBytes + (elementHalfwords[2] & TMD_STREAM_GEOMETRY_BYTE_OFFSET_MASK));
            gte_rtpt();
            gte_stflg(gteFlagDestination);
            if ((workspace->gteFlag & projectionErrorMask) == 0) {
                gte_nclip();
                gte_stopz(gteResultDestination);
                gte_stsxy3_f4(quad);
                gte_ldv0((const u8*)workspace->verts + (elementHalfwords[3] & TMD_STREAM_GEOMETRY_BYTE_OFFSET_MASK));
                gte_rtps();
                gte_stflg(gteFlagDestination);
                if ((workspace->gteFlag & projectionErrorMask) == 0) {
                    if (workspace->gteResult > 0) {
                        goto draw;
                    }
                    gte_nclip();
                    gte_stopz(gteResultDestination);
                    if (workspace->gteResult < 0) {
                    draw:
                        gte_stsxy2(&quad->x3);
                        gte_avsz4();
                        setlen(quad, sizeof(*quad) / sizeof(u32) - 1);
                        setcode(quad, TMD_F4_OPAQUE_COMMAND);
                        gte_stotz(gteResultDestination);
                        addPrim(&workspace->ot[((u32)workspace->gteResult << displayState->otDepthShift) >> TMD_DRAW_OT_INDEX_SHIFT & (GPU_ORDERING_TABLE_DEPTH_BYTE_MASK / sizeof(*workspace->ot))], quad);
                    }
                }
            }
            quad++;
            elements += workspace->elemStride;
        } while (workspace->elemCount-- > 0);
    }
    workspace->primWrite = (u8*)quad;
    return elements;
}

u32* tmdDrawStreamPrimF3(TmdStreamWorkspace* workspace, s32 objectFlags, u32* elements)
{
    enum { TMD_F3_OPAQUE_COMMAND = 0x20 };
    POLY_F3*            triangle;
    s32*                gteResultDestination;
    const DisplayState* displayState;
    u32                 projectionErrorMask;
    s32*                gteFlagDestination;
    const u16*          elementHalfwords;
    const u8*           vertexBytes;

    triangle = (POLY_F3*)workspace->primWrite;
    if (workspace->elemCount-- > 0) {
        gteFlagDestination   = &workspace->gteFlag;
        projectionErrorMask  = TMD_GTE_ERROR_FLAG;
        gteResultDestination = &workspace->gteResult;
        displayState         = &gDisplayState;
        do {
            elementHalfwords = (const u16*)elements;
            vertexBytes      = (const u8*)workspace->verts;
            // Project packed byte references; packet slots are consumed even on rejection.
            gte_ldv3(vertexBytes + (elementHalfwords[0] & TMD_STREAM_GEOMETRY_BYTE_OFFSET_MASK), vertexBytes + (elementHalfwords[1] & TMD_STREAM_GEOMETRY_BYTE_OFFSET_MASK), vertexBytes + (elementHalfwords[2] & TMD_STREAM_GEOMETRY_BYTE_OFFSET_MASK));
            gte_rtpt();
            gte_stflg(gteFlagDestination);
            if ((workspace->gteFlag & projectionErrorMask) == 0) {
                gte_nclip();
                gte_stopz(gteResultDestination);
                if (workspace->gteResult > 0) {
                    gte_stsxy3_f3(triangle);
                    gte_stflg(gteFlagDestination);
                    if ((workspace->gteFlag & projectionErrorMask) == 0) {
                        gte_avsz3();
                        setlen(triangle, sizeof(*triangle) / sizeof(u32) - 1);
                        setcode(triangle, TMD_F3_OPAQUE_COMMAND);
                        gte_stotz(gteResultDestination);
                        addPrim(&workspace->ot[((u32)workspace->gteResult << displayState->otDepthShift) >> TMD_DRAW_OT_INDEX_SHIFT & (GPU_ORDERING_TABLE_DEPTH_BYTE_MASK / sizeof(*workspace->ot))], triangle);
                    }
                }
            }
            triangle++;
            elements += workspace->elemStride;
        } while (workspace->elemCount-- > 0);
    }
    workspace->primWrite = (u8*)triangle;
    return elements;
}

u32* tmdDrawStreamPrimFt3SemiTrans(TmdStreamWorkspace* workspace, s32 objectFlags, u32* elements)
{
    enum { TMD_FT3_RAW_SEMI_TRANS_COMMAND = 0x27 };
    POLY_FT3*           triangle;
    s32*                gteResultDestination;
    const DisplayState* displayState;
    const u16*          elementHalfwords;
    const u8*           vertexBytes;

    triangle = (POLY_FT3*)workspace->primWrite;
    if (workspace->elemCount-- > 0) {
        gteResultDestination = &workspace->gteResult;
        displayState         = &gDisplayState;
        do {
            elementHalfwords = (const u16*)elements;
            vertexBytes      = (const u8*)workspace->verts;
            // Project packed byte references; packet slots are consumed even on rejection.
            gte_ldv3(vertexBytes + (elementHalfwords[0] & TMD_STREAM_GEOMETRY_BYTE_OFFSET_MASK), vertexBytes + (elementHalfwords[1] & TMD_STREAM_GEOMETRY_BYTE_OFFSET_MASK), vertexBytes + (elementHalfwords[2] & TMD_STREAM_GEOMETRY_BYTE_OFFSET_MASK));
            gte_rtpt();
            gte_stflg(&workspace->gteFlag);
            if (workspace->gteFlag >= 0) {
                gte_nclip();
                gte_stopz(gteResultDestination);
                if (workspace->gteResult > 0) {
                    TMD_LINK_PROJECTED_RAW_FT3(triangle, workspace, gteResultDestination, displayState, TMD_FT3_RAW_SEMI_TRANS_COMMAND);
                }
            }
            triangle++;
            elements += workspace->elemStride;
        } while (workspace->elemCount-- > 0);
    }
    workspace->primWrite = (u8*)triangle;
    return elements;
}

u32* tmdDrawStreamPrimFt4SemiTrans(TmdStreamWorkspace* workspace, s32 objectFlags, u32* elements)
{
    enum { TMD_FT4_RAW_SEMI_TRANS_COMMAND = 0x2F };
    POLY_FT4*           quad;
    s32*                gteResultDestination;
    const DisplayState* displayState;
    u32                 projectionErrorMask;
    s32*                gteFlagDestination;
    const u16*          elementHalfwords;
    const u8*           vertexBytes;

    quad = (POLY_FT4*)workspace->primWrite;
    if (workspace->elemCount-- > 0) {
        gteFlagDestination   = &workspace->gteFlag;
        projectionErrorMask  = TMD_GTE_ERROR_FLAG;
        gteResultDestination = &workspace->gteResult;
        displayState         = &gDisplayState;
        do {
            elementHalfwords = (const u16*)elements;
            vertexBytes      = (const u8*)workspace->verts;
            // Project packed byte references; packet slots are consumed even on rejection.
            gte_ldv3(vertexBytes + (elementHalfwords[0] & TMD_STREAM_GEOMETRY_BYTE_OFFSET_MASK), vertexBytes + (elementHalfwords[1] & TMD_STREAM_GEOMETRY_BYTE_OFFSET_MASK), vertexBytes + (elementHalfwords[2] & TMD_STREAM_GEOMETRY_BYTE_OFFSET_MASK));
            gte_rtpt();
            gte_stflg(gteFlagDestination);
            if ((workspace->gteFlag & projectionErrorMask) == 0) {
                gte_nclip();
                gte_stopz(gteResultDestination);
                gte_stsxy3_ft4(quad);
                gte_ldv0((const u8*)workspace->verts + (elementHalfwords[3] & TMD_STREAM_GEOMETRY_BYTE_OFFSET_MASK));
                gte_rtps();
                gte_stflg(gteFlagDestination);
                if ((workspace->gteFlag & projectionErrorMask) == 0) {
                    if (workspace->gteResult > 0) {
                        goto draw;
                    }
                    gte_nclip();
                    gte_stopz(gteResultDestination);
                    if (workspace->gteResult < 0) {
                    draw:
                        TMD_LINK_PROJECTED_RAW_FT4(quad, workspace, gteResultDestination, displayState, TMD_FT4_RAW_SEMI_TRANS_COMMAND);
                    }
                }
            }
            quad++;
            elements += workspace->elemStride;
        } while (workspace->elemCount-- > 0);
    }
    workspace->primWrite = (u8*)quad;
    return elements;
}

u32* tmdDrawStreamPrimG3CornerColors(TmdStreamWorkspace* workspace, s32 objectFlags, u32* elements)
{
    enum { TMD_CORNER_COLOR_WORD_INDEX = 3 };
    enum { TMD_G3_OPAQUE_COMMAND = 0x30 };
    POLY_G3*            triangle;
    s32*                gteResultDestination;
    const DisplayState* displayState;
    const u16*          elementHalfwords;
    const u8*           vertexBytes;

    triangle = (POLY_G3*)workspace->primWrite;
    if (workspace->elemCount-- > 0) {
        gteResultDestination = &workspace->gteResult;
        displayState         = &gDisplayState;
        do {
            elementHalfwords = (const u16*)elements;
            vertexBytes      = (const u8*)workspace->verts;
            // Project packed byte references; packet slots are consumed even on rejection.
            gte_ldv3(vertexBytes + (elementHalfwords[0] & TMD_STREAM_GEOMETRY_BYTE_OFFSET_MASK), vertexBytes + (elementHalfwords[1] & TMD_STREAM_GEOMETRY_BYTE_OFFSET_MASK), vertexBytes + (elementHalfwords[2] & TMD_STREAM_GEOMETRY_BYTE_OFFSET_MASK));
            gte_rtpt();
            gte_stflg(&workspace->gteFlag);
            if (workspace->gteFlag >= 0) {
                gte_nclip();
                gte_stopz(gteResultDestination);
                if (workspace->gteResult > 0) {
                    gte_stsxy3_g3(triangle);
                    gte_avsz3();
                    TMD_LIGHT_STREAM_CORNER(elements + TMD_CORNER_COLOR_WORD_INDEX, (const u8*)workspace->normals + (elementHalfwords[3] & TMD_STREAM_GEOMETRY_BYTE_OFFSET_MASK), &triangle->r0);
                    TMD_LIGHT_STREAM_CORNER(elements + TMD_CORNER_COLOR_WORD_INDEX + 1, (const u8*)workspace->normals + (elementHalfwords[4] & TMD_STREAM_GEOMETRY_BYTE_OFFSET_MASK), &triangle->r1);
                    TMD_LIGHT_STREAM_CORNER(elements + TMD_CORNER_COLOR_WORD_INDEX + 2, (const u8*)workspace->normals + (elementHalfwords[5] & TMD_STREAM_GEOMETRY_BYTE_OFFSET_MASK), &triangle->r2);
                    setlen(triangle, sizeof(*triangle) / sizeof(u32) - 1);
                    setcode(triangle, TMD_G3_OPAQUE_COMMAND);
                    gte_stotz(gteResultDestination);
                    addPrim(&workspace->ot[((u32)workspace->gteResult << displayState->otDepthShift) >> TMD_DRAW_OT_INDEX_SHIFT & (GPU_ORDERING_TABLE_DEPTH_BYTE_MASK / sizeof(*workspace->ot))], triangle);
                }
            }
            triangle++;
            elements += workspace->elemStride;
        } while (workspace->elemCount-- > 0);
    }
    workspace->primWrite = (u8*)triangle;
    return elements;
}

u32* tmdDrawStreamPrimG3CornerColorsSemiTrans(TmdStreamWorkspace* workspace, s32 objectFlags, u32* elements)
{
    enum { TMD_CORNER_COLOR_WORD_INDEX = 3 };
    enum { TMD_G3_SEMI_TRANS_COMMAND = 0x32 };
    POLY_G3*            triangle;
    s32*                gteResultDestination;
    const DisplayState* displayState;
    const u16*          elementHalfwords;
    const u8*           vertexBytes;

    triangle = (POLY_G3*)workspace->primWrite;
    if (workspace->elemCount-- > 0) {
        gteResultDestination = &workspace->gteResult;
        displayState         = &gDisplayState;
        do {
            elementHalfwords = (const u16*)elements;
            vertexBytes      = (const u8*)workspace->verts;
            // Project packed byte references; packet slots are consumed even on rejection.
            gte_ldv3(vertexBytes + (elementHalfwords[0] & TMD_STREAM_GEOMETRY_BYTE_OFFSET_MASK), vertexBytes + (elementHalfwords[1] & TMD_STREAM_GEOMETRY_BYTE_OFFSET_MASK), vertexBytes + (elementHalfwords[2] & TMD_STREAM_GEOMETRY_BYTE_OFFSET_MASK));
            gte_rtpt();
            gte_stflg(&workspace->gteFlag);
            if (workspace->gteFlag >= 0) {
                gte_nclip();
                gte_stopz(gteResultDestination);
                if (workspace->gteResult > 0) {
                    gte_stsxy3_g3(triangle);
                    gte_avsz3();
                    TMD_LIGHT_STREAM_CORNER(elements + TMD_CORNER_COLOR_WORD_INDEX, (const u8*)workspace->normals + (elementHalfwords[3] & TMD_STREAM_GEOMETRY_BYTE_OFFSET_MASK), &triangle->r0);
                    TMD_LIGHT_STREAM_CORNER(elements + TMD_CORNER_COLOR_WORD_INDEX + 1, (const u8*)workspace->normals + (elementHalfwords[4] & TMD_STREAM_GEOMETRY_BYTE_OFFSET_MASK), &triangle->r1);
                    TMD_LIGHT_STREAM_CORNER(elements + TMD_CORNER_COLOR_WORD_INDEX + 2, (const u8*)workspace->normals + (elementHalfwords[5] & TMD_STREAM_GEOMETRY_BYTE_OFFSET_MASK), &triangle->r2);
                    setlen(triangle, sizeof(*triangle) / sizeof(u32) - 1);
                    setcode(triangle, TMD_G3_SEMI_TRANS_COMMAND);
                    gte_stotz(gteResultDestination);
                    addPrim(&workspace->ot[((u32)workspace->gteResult << displayState->otDepthShift) >> TMD_DRAW_OT_INDEX_SHIFT & (GPU_ORDERING_TABLE_DEPTH_BYTE_MASK / sizeof(*workspace->ot))], triangle);
                }
            }
            triangle++;
            elements += workspace->elemStride;
        } while (workspace->elemCount-- > 0);
    }
    workspace->primWrite = (u8*)triangle;
    return elements;
}

u32* tmdDrawStreamPrimG4CornerColors(TmdStreamWorkspace* workspace, s32 objectFlags, u32* elements)
{
    enum { TMD_CORNER_COLOR_WORD_INDEX = 4 };
    enum { TMD_G4_OPAQUE_COMMAND = 0x38 };
    POLY_G4*            quad;
    s32*                gteResultDestination;
    const DisplayState* displayState;
    u32                 projectionErrorMask;
    s32*                gteFlagDestination;
    const u16*          elementHalfwords;
    const u8*           vertexBytes;

    quad = (POLY_G4*)workspace->primWrite;
    if (workspace->elemCount-- > 0) {
        gteFlagDestination   = &workspace->gteFlag;
        projectionErrorMask  = TMD_GTE_ERROR_FLAG;
        gteResultDestination = &workspace->gteResult;
        displayState         = &gDisplayState;
        do {
            elementHalfwords = (const u16*)elements;
            vertexBytes      = (const u8*)workspace->verts;
            // Project packed byte references; packet slots are consumed even on rejection.
            gte_ldv3(vertexBytes + (elementHalfwords[0] & TMD_STREAM_GEOMETRY_BYTE_OFFSET_MASK), vertexBytes + (elementHalfwords[1] & TMD_STREAM_GEOMETRY_BYTE_OFFSET_MASK), vertexBytes + (elementHalfwords[2] & TMD_STREAM_GEOMETRY_BYTE_OFFSET_MASK));
            gte_rtpt();
            gte_stflg(gteFlagDestination);
            if ((workspace->gteFlag & projectionErrorMask) == 0) {
                gte_nclip();
                gte_stopz(gteResultDestination);
                gte_stsxy3_g4(quad);
                gte_ldv0((const u8*)workspace->verts + (elementHalfwords[3] & TMD_STREAM_GEOMETRY_BYTE_OFFSET_MASK));
                gte_rtps();
                gte_stflg(gteFlagDestination);
                if ((workspace->gteFlag & projectionErrorMask) == 0) {
                    if (workspace->gteResult > 0) {
                        goto draw;
                    }
                    gte_nclip();
                    gte_stopz(gteResultDestination);
                    if (workspace->gteResult < 0) {
                    draw:
                        gte_stsxy2(&quad->x3);
                        gte_avsz4();
                        TMD_LIGHT_STREAM_CORNER(elements + TMD_CORNER_COLOR_WORD_INDEX, (const u8*)workspace->normals + (elementHalfwords[4] & TMD_STREAM_GEOMETRY_BYTE_OFFSET_MASK), &quad->r0);
                        TMD_LIGHT_STREAM_CORNER(elements + TMD_CORNER_COLOR_WORD_INDEX + 1, (const u8*)workspace->normals + (elementHalfwords[5] & TMD_STREAM_GEOMETRY_BYTE_OFFSET_MASK), &quad->r1);
                        TMD_LIGHT_STREAM_CORNER(elements + TMD_CORNER_COLOR_WORD_INDEX + 2, (const u8*)workspace->normals + (elementHalfwords[6] & TMD_STREAM_GEOMETRY_BYTE_OFFSET_MASK), &quad->r2);
                        TMD_LIGHT_STREAM_CORNER(elements + TMD_CORNER_COLOR_WORD_INDEX + 3, (const u8*)workspace->normals + (elementHalfwords[7] & TMD_STREAM_GEOMETRY_BYTE_OFFSET_MASK), &quad->r3);
                        setlen(quad, sizeof(*quad) / sizeof(u32) - 1);
                        setcode(quad, TMD_G4_OPAQUE_COMMAND);
                        gte_stotz(gteResultDestination);
                        addPrim(&workspace->ot[((u32)workspace->gteResult << displayState->otDepthShift) >> TMD_DRAW_OT_INDEX_SHIFT & (GPU_ORDERING_TABLE_DEPTH_BYTE_MASK / sizeof(*workspace->ot))], quad);
                    }
                }
            }
            quad++;
            elements += workspace->elemStride;
        } while (workspace->elemCount-- > 0);
    }
    workspace->primWrite = (u8*)quad;
    return elements;
}

u32* tmdDrawStreamPrimG4CornerColorsSemiTrans(TmdStreamWorkspace* workspace, s32 objectFlags, u32* elements)
{
    enum { TMD_CORNER_COLOR_WORD_INDEX = 4 };
    enum { TMD_G4_SEMI_TRANS_COMMAND = 0x3A };
    POLY_G4*            quad;
    s32*                gteResultDestination;
    const DisplayState* displayState;
    u32                 projectionErrorMask;
    s32*                gteFlagDestination;
    const u16*          elementHalfwords;
    const u8*           vertexBytes;

    quad = (POLY_G4*)workspace->primWrite;
    if (workspace->elemCount-- > 0) {
        gteFlagDestination   = &workspace->gteFlag;
        projectionErrorMask  = TMD_GTE_ERROR_FLAG;
        gteResultDestination = &workspace->gteResult;
        displayState         = &gDisplayState;
        do {
            elementHalfwords = (const u16*)elements;
            vertexBytes      = (const u8*)workspace->verts;
            // Project packed byte references; packet slots are consumed even on rejection.
            gte_ldv3(vertexBytes + (elementHalfwords[0] & TMD_STREAM_GEOMETRY_BYTE_OFFSET_MASK), vertexBytes + (elementHalfwords[1] & TMD_STREAM_GEOMETRY_BYTE_OFFSET_MASK), vertexBytes + (elementHalfwords[2] & TMD_STREAM_GEOMETRY_BYTE_OFFSET_MASK));
            gte_rtpt();
            gte_stflg(gteFlagDestination);
            if ((workspace->gteFlag & projectionErrorMask) == 0) {
                gte_nclip();
                gte_stopz(gteResultDestination);
                if (workspace->gteResult > 0) {
                    gte_stsxy3_g4(quad);
                    gte_ldv0((const u8*)workspace->verts + (elementHalfwords[3] & TMD_STREAM_GEOMETRY_BYTE_OFFSET_MASK));
                    gte_rtps();
                    gte_stflg(gteFlagDestination);
                    if ((workspace->gteFlag & projectionErrorMask) == 0) {
                        if (workspace->gteResult > 0) {
                            goto draw;
                        }
                        gte_nclip();
                        gte_stopz(gteResultDestination);
                        if (workspace->gteResult < 0) {
                        draw:
                            gte_stsxy2(&quad->x3);
                            gte_avsz4();
                            TMD_LIGHT_STREAM_CORNER(elements + TMD_CORNER_COLOR_WORD_INDEX, (const u8*)workspace->normals + (elementHalfwords[4] & TMD_STREAM_GEOMETRY_BYTE_OFFSET_MASK), &quad->r0);
                            TMD_LIGHT_STREAM_CORNER(elements + TMD_CORNER_COLOR_WORD_INDEX + 1, (const u8*)workspace->normals + (elementHalfwords[5] & TMD_STREAM_GEOMETRY_BYTE_OFFSET_MASK), &quad->r1);
                            TMD_LIGHT_STREAM_CORNER(elements + TMD_CORNER_COLOR_WORD_INDEX + 2, (const u8*)workspace->normals + (elementHalfwords[6] & TMD_STREAM_GEOMETRY_BYTE_OFFSET_MASK), &quad->r2);
                            TMD_LIGHT_STREAM_CORNER(elements + TMD_CORNER_COLOR_WORD_INDEX + 3, (const u8*)workspace->normals + (elementHalfwords[7] & TMD_STREAM_GEOMETRY_BYTE_OFFSET_MASK), &quad->r3);
                            setlen(quad, sizeof(*quad) / sizeof(u32) - 1);
                            setcode(quad, TMD_G4_SEMI_TRANS_COMMAND);
                            gte_stotz(gteResultDestination);
                            addPrim(&workspace->ot[((u32)workspace->gteResult << displayState->otDepthShift) >> TMD_DRAW_OT_INDEX_SHIFT & (GPU_ORDERING_TABLE_DEPTH_BYTE_MASK / sizeof(*workspace->ot))], quad);
                        }
                    }
                }
            }
            quad++;
            elements += workspace->elemStride;
        } while (workspace->elemCount-- > 0);
    }
    workspace->primWrite = (u8*)quad;
    return elements;
}

void modelLightingSetLayerMaterials(s32 layerIntensity)
{
    enum { MODEL_LIGHTING_LAYER_INTENSITY_MAX    = 255,
           MODEL_LIGHTING_BASE_NEUTRAL_INTENSITY = 128 };
    s32 baseIntensity;

    if (layerIntensity <= 0) {
        layerIntensity = 0;
        baseIntensity  = MODEL_LIGHTING_BASE_NEUTRAL_INTENSITY;
    } else {
        if (layerIntensity >= MODEL_LIGHTING_LAYER_INTENSITY_MAX + 1) {
            layerIntensity = MODEL_LIGHTING_LAYER_INTENSITY_MAX;
        }
        baseIntensity = (MODEL_LIGHTING_LAYER_INTENSITY_MAX - layerIntensity) >> 1;
    }

    D_80114BA4.r = D_80114BA4.g = D_80114BA4.b = layerIntensity;
    D_80114BA8.r = D_80114BA8.g = D_80114BA8.b = baseIntensity;
}

u32* tmdXformStreamVertsUnlit(TmdStreamWorkspace* workspace, s32 objectFlags, u32* elements)
{
    enum { TMD_PREVIOUS_VERTEX_REF_NONE = -1 };
    s32        previousVertexRef;
    s32        elementCount;
    u32        vertexRef;
    const u16* elementHalfwords;

    elementCount = workspace->elemCount;
    if (elementCount == 0) {
        return elements;
    }
    previousVertexRef    = TMD_PREVIOUS_VERTEX_REF_NONE;
    workspace->elemCount = elementCount + previousVertexRef;
    if (elementCount > 0) {
        do {
            elementHalfwords = (const u16*)elements;
            vertexRef        = elementHalfwords[0];
            // Reuse screen XY and cached Z only for consecutive identical references.
            if (vertexRef != previousVertexRef) {
                gte_ldv0((const u8*)workspace->verts + (vertexRef & TMD_STREAM_GEOMETRY_BYTE_OFFSET_MASK));
                gte_rtps();
                gte_stsz(&workspace->gteResult);
                // Retain the incoming saved FLAG decision rather than publishing this RTPS's FLAG.
                if (workspace->gteFlag & TMD_GTE_ERROR_FLAG) {
                    workspace->gteResult |= TMD_VERTEX_DEPTH_INVALID;
                }
                workspace->szTable[elementHalfwords[0] >> TMD_STREAM_VERTEX_INDEX_SHIFT] = workspace->gteResult;
            }
            previousVertexRef = elementHalfwords[0];
            gte_stsxy(workspace->preXformWrite + elementHalfwords[1]);
            elements += workspace->elemStride;
        } while (workspace->elemCount-- > 0);
    }
    return elements;
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

u32* tmdBuildStreamGt4PreXform(TmdStreamWorkspace* workspace, s32 objectFlags, u32* elements)
{
    enum { TMD_GT4_PRE_XFORM_UV0_CLUT_WORD_INDEX = 2 };
    POLY_GT4* quad;

    quad = (POLY_GT4*)workspace->preXformWrite;
    // Seed texture data before projection commands supply positions and colours.
    while (workspace->elemCount-- > 0) {
        _modelLightingInitGt4TextureWords(quad, elements, TMD_GT4_PRE_XFORM_UV0_CLUT_WORD_INDEX, workspace);
        quad++;
        elements += workspace->elemStride;
    }
    workspace->preXformWrite = (u8*)quad;
    return elements;
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

u32* tmdBuildStreamGt4OneNormal(TmdStreamWorkspace* workspace, s32 objectFlags, u32* elements)
{
    enum { TMD_GT4_ONE_NORMAL_UV0_CLUT_WORD_INDEX = 3 };
    POLY_GT4* quad;

    quad = (POLY_GT4*)workspace->primWrite;
    // Drawing lights all four corners from the element's face normal.
    while (workspace->elemCount-- > 0) {
        _modelLightingInitGt4TextureWords(quad, elements, TMD_GT4_ONE_NORMAL_UV0_CLUT_WORD_INDEX, workspace);
        quad++;
        elements += workspace->elemStride;
    }
    workspace->primWrite = (u8*)quad;
    return elements;
}

u32* tmdBuildStreamGt4Unlit(TmdStreamWorkspace* workspace, s32 objectFlags, u32* elements)
{
    enum {
        TMD_GT4_UNLIT_COLOR_WORD_INDEX    = 2,
        TMD_GT4_UNLIT_UV0_CLUT_WORD_INDEX = 6,
        TMD_GT4_UNLIT_SEMI_TRANS_COMMAND  = 0x3E
    };
    POLY_GT4* quad;
    u32       corner3ColorWord;

    quad = (POLY_GT4*)workspace->primWrite;
    if (workspace->elemCount-- > 0) {
        do {
            // Copy all four colour bytes; only corner 0's command is replaced.
            GPU_PRIMITIVE_COLOR_WORD(quad, 0) = elements[TMD_GT4_UNLIT_COLOR_WORD_INDEX];
            GPU_PRIMITIVE_COLOR_WORD(quad, 1) = elements[TMD_GT4_UNLIT_COLOR_WORD_INDEX + 1];
            GPU_PRIMITIVE_COLOR_WORD(quad, 2) = elements[TMD_GT4_UNLIT_COLOR_WORD_INDEX + 2];
            corner3ColorWord                  = elements[TMD_GT4_UNLIT_COLOR_WORD_INDEX + 3];
            setlen(quad, sizeof(*quad) / sizeof(u32) - 1);
            setcode(quad, TMD_GT4_UNLIT_SEMI_TRANS_COMMAND);
            GPU_PRIMITIVE_COLOR_WORD(quad, 3) = corner3ColorWord;
            _modelLightingInitGt4TextureWords(quad, elements, TMD_GT4_UNLIT_UV0_CLUT_WORD_INDEX, workspace);
            quad++;
            elements += workspace->elemStride;
        } while (workspace->elemCount-- > 0);
    }
    workspace->primWrite = (u8*)quad;
    return elements;
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
            _modelLightingInitGt4OffsetLayerTexture(quad, elements, MODEL_LIGHTING_GT4_OFFSET_LAYER_UV0_CLUT_WORD, workspace);
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

u32* tmdBuildStreamGt4PreXformEnvLayer(TmdStreamWorkspace* workspace, s32 objectFlags, u32* elements)
{
    enum {
        TMD_GT4_ENV_INITIAL_TEXTURE_DEPTH_4BIT = 0,
        TMD_GT4_ENV_BASE_UV0_CLUT_WORD_INDEX   = 2
    };
    POLY_GT4* quad;

    quad = (POLY_GT4*)workspace->preXformWrite;
    while (workspace->elemCount-- > 0) {
        // Projection supplies the environment UVs; drawing replaces its page.
        quad->tpage = getTPage(TMD_GT4_ENV_INITIAL_TEXTURE_DEPTH_4BIT, GPU_BLEND_ADD, 960, 256);
        quad->clut  = getClut(256, 240);
        quad++;
        _modelLightingInitGt4TextureWords(quad, elements, TMD_GT4_ENV_BASE_UV0_CLUT_WORD_INDEX, workspace);
        quad++;
        elements += workspace->elemStride;
    }
    workspace->preXformWrite = (u8*)quad;
    return elements;
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

u32* tmdBuildStreamGt4PreXformOffsetLayer(TmdStreamWorkspace* workspace, s32 objectFlags, u32* elements)
{
    enum { TMD_GT4_PRE_XFORM_OFFSET_LAYER_UV0_CLUT_WORD_INDEX = 2 };
    POLY_GT4* quad;

    quad = (POLY_GT4*)workspace->preXformWrite;
    while (workspace->elemCount-- > 0) {
        // Both packets share UVs; their GPU addresses have independent displacements.
        _modelLightingInitGt4OffsetLayerTexture(quad, elements, TMD_GT4_PRE_XFORM_OFFSET_LAYER_UV0_CLUT_WORD_INDEX, workspace);
        quad++;
        _modelLightingInitGt4TextureWords(quad, elements, TMD_GT4_PRE_XFORM_OFFSET_LAYER_UV0_CLUT_WORD_INDEX, workspace);
        quad++;
        elements += workspace->elemStride;
    }
    workspace->preXformWrite = (u8*)quad;
    return elements;
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

/// Reserves one untouched 20-byte packet slot per stream element in the second region.
///
/// This callback-shaped function has no known callers. Its packet kind is
/// unproven; a 20-byte stride alone does not establish a POLY_F3 or a sprite.
/// The caller must supply initial count 0..65535, a stride in u32 words,
/// readable complete element strides and enough second-region storage.
/// Bounds are unchecked. Returns elements + initial count * elemStride and
/// advances primWrite by 20 * count bytes without accessing either payload.
/// Count ends at -1, even for empty input; preXformWrite is unchanged.
/// objectFlags is ignored. No allocation occurs and no storage is retained.
static u32* _tmdReserveStreamPackets20(TmdStreamWorkspace* workspace, s32 objectFlags, u32* elements)
{
    enum { TMD_PACKET_SLOT_BYTES = 20 };
    u8* packetCursor;
    s32 elementStrideWords;

    packetCursor = workspace->primWrite;
    if (workspace->elemCount-- > 0) {
        elementStrideWords = workspace->elemStride;
        do {
            elements     += elementStrideWords;
            packetCursor += TMD_PACKET_SLOT_BYTES;
        } while (workspace->elemCount-- > 0);
    }
    workspace->primWrite = packetCursor;
    return elements;
}

/// Reserves one untouched 24-byte packet slot per stream element in the second region.
///
/// This callback-shaped function has no known callers. Its packet kind is
/// unproven; the 24-byte stride does not by itself establish a POLY_F4.
/// The caller must supply initial count 0..65535, a stride in u32 words,
/// readable complete element strides and enough second-region storage.
/// Bounds are unchecked. Returns elements + initial count * elemStride and
/// advances primWrite by 24 * count bytes without accessing either payload.
/// Count ends at -1, even for empty input; preXformWrite is unchanged.
/// objectFlags is ignored. No allocation occurs and no storage is retained.
static u32* _tmdReserveStreamPackets24(TmdStreamWorkspace* workspace, s32 objectFlags, u32* elements)
{
    enum { TMD_PACKET_SLOT_BYTES = 24 };
    u8* packetCursor;
    s32 elementStrideWords;

    packetCursor = workspace->primWrite;
    if (workspace->elemCount-- > 0) {
        elementStrideWords = workspace->elemStride;
        do {
            elements     += elementStrideWords;
            packetCursor += TMD_PACKET_SLOT_BYTES;
        } while (workspace->elemCount-- > 0);
    }
    workspace->primWrite = packetCursor;
    return elements;
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
    displaySetFrameTiming(DISPLAY_TIMING_TWO_VBLANKS);
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
        textDrawString(&req, Text_ItoaUnsigned(buf, work->hours));
        textDrawString(&req, ":");
        textDrawString(&req, textItoaPadded(buf, work->minutes, 2));
        textDrawString(&req, "'");
        textDrawString(&req, textItoaPadded(buf, D_8005ED68 / 60, 2));
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
            sndEvtRequestScriptStop(SOUND_BANK_TYPE_WEAPON_ALL, 8);
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
                sndEvtRequestScriptStop(SOUND_BANK_TYPE_WEAPON_ALL, 8);
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
    sndEvtRequestScriptStop(SOUND_BANK_TYPE_ALL_NON_AMBIENT, 0x78);
    sndEvtRequestScriptStop(SOUND_STAGE_AMBIENT, 0x78);
    flag                  = 0xFF;
    arg0->spawnArg1.value = flag;
    Pad_SetCooldown(0);
    gameClearTaskSlots();
    ds               = &gDisplayState;
    ds->stopTaskWalk = 1;
    taskResetDefaultList();
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
    memConfigureImageMemory(GAME_STAGE_NONE, 0);
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
    taskSpawnFromTable(&D_8010D1FC, 0, 0, 0);
}

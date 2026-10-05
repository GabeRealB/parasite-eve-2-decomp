#include "gameplay/model_objects.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/gtemac.h>
#include <psyq/inline_c.h>
#include <psyq/stdio.h>

#include "common.h"
#include "gte.h"
#include "types.h"

#include "gameplay/actor_render.h"
#include "actor_render.h"
#include "model_objects.h"
#include "gameplay/room_effects.h"

#include "main/display.h"
#include "main/gfx.h"
#include "main/mem.h"
#include "main/task.h"
#include "main/tmd.h"

extern const CVECTOR Gp_ColorOrange;

// Bank-0 descriptor that refreshes coordinates and draws the active model list.
enum { MODEL_OBJECT_STASH_DRAW_TASK = 0x1A };

/// Saved model-list endpoints; the first element still refers to the active sentinel.
static TmdListNode _gModelObjectSavedModelList = { NULL, NULL };

/// Saved coordinate-body-list endpoints; restored to their original sentinel together.
static TmdListNode _gModelObjectSavedDisp2dList = { NULL, NULL };

/// Temporary draw task active while the previous body lists are stashed.
static Task* _gModelObjectTemporaryDrawTask = NULL;

/// Geometry references encode byte offsets into complete eight-byte SVECTOR entries.
/// Low reference bits are discarded for loads; their meaning is unproven.
enum {
    TMD_ENV_MAP_GEOMETRY_BYTE_OFFSET_MASK = 0xFFF8,
    TMD_ENV_MAP_VERTEX_INDEX_SHIFT        = 3,
    TMD_ENV_MAP_NO_PREVIOUS_VERTEX        = -1,
    TMD_ENV_MAP_REDUCED_SCREEN_SHIFT      = 4,
    TMD_ENV_MAP_REDUCED_NORMAL_SHIFT      = 8,
    TMD_ENV_MAP_REDUCED_CENTER            = 32
};

/// Four-halfword prefix of an environment-mapped corner element.
///
/// Destinations are independent byte offsets from `preXformWrite`, not
/// indices of complete packets. The stream's word stride determines the
/// full element extent; this type describes only the fields read here.
typedef struct {
    u16 vertexByteRef; // Vertex byte reference; shifted by three to index the depth cache
    u16 normalByteRef; // Normal byte reference; low three bits masked before loading
    u16 xyByteOffset;  // Destination of packed screen XY, followed by two texture bytes
    u16 rgbByteOffset; // Destination of the lit RGB/code word
} _TmdEnvMapCornerRefs;
STATIC_ASSERT_SIZEOF(_TmdEnvMapCornerRefs, 2 * sizeof(u32));

/// Four-halfword prefix of a corner shared by two environment-layer packets.
///
/// Each destination selects a colour group; screen XY follows four bytes
/// later and the layer's U/V follow eight bytes later. Geometry references
/// encode SVECTOR byte offsets. Any words beyond this prefix remain outside
/// this type; `elemStride` supplies the full stream stride.
typedef struct {
    u16 vertexByteRef;        // Vertex reference; shifted by three to index the depth cache
    u16 normalByteRef;        // Normal reference; low three bits masked before loading
    u16 layerColorByteOffset; // Layer colour-group byte offset from preXformWrite
    u16 baseColorByteOffset;  // Base colour-group byte offset from preXformWrite
} _TmdEnvLayerCornerRefs;
STATIC_ASSERT_SIZEOF(_TmdEnvLayerCornerRefs, 2 * sizeof(u32));

static inline u32* _tmdXformStreamVertsEnvMapLitReduced(TmdStreamWorkspace* workspace, u32* elements);

static __inline__ void _actorRenderRefreshCoord(GfxCoord* coord, s32 stamp, s32 parity, GfxCoord* root);

static u32* _tmdXformStreamVertsGreyEnvLayerReduced(TmdStreamWorkspace* workspace, s32 objectFlags, u32* elements);

static u32* func_8009AA5C(TmdStreamWorkspace* ws, s32 arg1, u32* arg2);

static u32* _tmdXformStreamVertsEnvMapLit(TmdStreamWorkspace* workspace, s32 objectFlags, u32* elements);

/// Rotates a signed 32-bit vector and adds the translation already loaded into the GTE.
///
/// Load RT (12 fractional bits) and TR before calling. `inputVector` and
/// `outputVector` each address three contiguous signed 32-bit components,
/// aligned to four bytes; input and TR use the same coordinate units. Reads
/// and writes exactly 12 bytes. The buffers may overlap: all input loads
/// precede the output stores. Each pointer expression is evaluated once,
/// with no ordering between the two evaluations and no captured C variables.
///
/// Splits each component into signed low, middle and high chunks weighted by
/// 1, 2^10 and 2^20. The low product includes TR and shifts right by 12; the
/// middle product shifts right by 2; the high product shifts left by 8.
/// Low and middle products round down separately, so this can differ from
/// shifting a single full product. Final additions wrap at 32 bits.
/// Clobbers GTE V0..V2, MAC1..3, IR1..3 and FLAG; leaves RT and TR intact.
/// No combined overflow status is returned.
#define gte_RotTransLV(inputVector, outputVector)                                                                     \
    do {                                                                                                              \
        enum {                                                                                                        \
            GTE_ROT_TRANS_LV_LOW_ROTATE_TRANSLATE = 0x4A480012, /* Rotates the low chunks and adds translation.       \
                                                                 *                                                    \
                                                                 * Same instruction word as gte_rtv0tr.               \
                                                                 * MVMVA(sf=1, mx=0, v=0, cv=0, lm=0).                \
                                                                 * V0 = (input & 1023) - 1024 * (input < 0),          \
                                                                 * for each input component.                          \
                                                                 * Read MAC1..3 = (RT * V0 + TR * 4096) >> 12.        \
                                                                 * Code reads MAC1..3, not saturated IR. */           \
            GTE_ROT_TRANS_LV_MIDDLE_ROTATE = 0x4A40E012,        /* Rotates signed middle chunks without translation.  \
                                                                 *                                                    \
                                                                 * gte_rtv1 operands with the fraction shift off.     \
                                                                 * MVMVA(sf=0, mx=0, v=1, cv=3, lm=0).                \
                                                                 * V1 = ((input >> 10) & 1023) - 1023 * (input < 0),  \
                                                                 * for each input component.                          \
                                                                 * Read MAC1..3 = RT * V1; IR1..3 saturate.           \
                                                                 * Arithmetic MAC >> 2 restores the 2^10 weight       \
                                                                 * against RT's 12 fractional bits. */                \
            GTE_ROT_TRANS_LV_HIGH_ROTATE = 0x4A416012           /* Rotates signed high chunks without translation.    \
                                                                 *                                                    \
                                                                 * MVMVA(sf=0, mx=0, v=2, cv=3, lm=0).                \
                                                                 * V2 = (input >> 20) + (input < 0), componentwise.   \
                                                                 * Read MAC1..3 = RT * V2; IR1..3 saturate.           \
                                                                 * MAC << 8 restores the 2^20 chunk's weight          \
                                                                 * with RT's 12 fractional bits; wraps at 32 bits. */ \
        };                                                                                                            \
        /* Sign corrections make the weighted chunks reconstruct negative inputs too. */                              \
        __asm__ volatile(                                                                                             \
            "lw	$14, 0( %0 );"                                                                                        \
            "lw	$15, 4( %0 );"                                                                                        \
            "addiu	$16, $0, -0x400;"                                                                                  \
            "sra	$12, $14, 21;"                                                                                       \
            "and	$12, $16, $12;"                                                                                      \
            "andi	$13, $14, 0x3ff;"                                                                                   \
            "or	$12, $13, $12;"                                                                                       \
            "andi	$12, $12, 0xffff;"                                                                                  \
            "sra	$13, $15, 21;"                                                                                       \
            "and	$13, $16, $13;"                                                                                      \
            "andi	$16, $15, 0x3ff;"                                                                                   \
            "or	$13, $16, $13;"                                                                                       \
            "sll	$13, $13, 16;"                                                                                       \
            "or	$12, $13, $12;"                                                                                       \
            "mtc2	$12, $0;"                                                                                           \
            "sra	$14, $14, 10;"                                                                                       \
            "sra	$15, $15, 10;"                                                                                       \
            "addiu	$16, $0, -0x400;"                                                                                  \
            "sra	$12, $14, 21;"                                                                                       \
            "and	$12, $16, $12;"                                                                                      \
            "andi	$13, $14, 0x3ff;"                                                                                   \
            "or	$12, $13, $12;"                                                                                       \
            "sra	$13, $15, 21;"                                                                                       \
            "and	$13, $16, $13;"                                                                                      \
            "andi	$16, $15, 0x3ff;"                                                                                   \
            "or	$13, $16, $13;"                                                                                       \
            "srl	$16, $15, 31;"                                                                                       \
            "addu	$13, $13, $16;"                                                                                     \
            "sll	$13, $13, 16;"                                                                                       \
            "srl	$16, $14, 31;"                                                                                       \
            "addu	$12, $12, $16;"                                                                                     \
            "andi	$12, $12, 0xffff;"                                                                                  \
            "or	$12, $13, $12;"                                                                                       \
            "mtc2	$12, $2;"                                                                                           \
            "sra	$14, $14, 10;"                                                                                       \
            "sra	$15, $15, 10;"                                                                                       \
            "andi	$12, $14, 0xffff;"                                                                                  \
            "srl	$16, $14, 31;"                                                                                       \
            "addu	$12, $16, $12;"                                                                                     \
            "andi	$12, $12, 0xffff;"                                                                                  \
            "andi	$13, $15, 0xffff;"                                                                                  \
            "srl	$16, $15, 31;"                                                                                       \
            "addu	$13, $16, $13;"                                                                                     \
            "sll	$13, $13, 16;"                                                                                       \
            "or	$12, $13, $12;"                                                                                       \
            "mtc2	$12, $4;"                                                                                           \
            "lw	$16, 8( %0 );"                                                                                        \
            "addiu	$14, $0, -0x400;"                                                                                  \
            "srl	$15, $16, 31;"                                                                                       \
            "sra	$12, $16, 21;"                                                                                       \
            "and	$12, $14, $12;"                                                                                      \
            "andi	$13, $16, 0x3ff;"                                                                                   \
            "or	$12, $13, $12;"                                                                                       \
            "mtc2	$12, $1;"                                                                                           \
            "sra	$16, $16, 10;"                                                                                       \
            "sra	$12, $16, 21;"                                                                                       \
            "and	$12, $14, $12;"                                                                                      \
            "andi	$13, $16, 0x3ff;"                                                                                   \
            "or	$12, $13, $12;"                                                                                       \
            "addu	$12, $12, $15;"                                                                                     \
            "mtc2	$12, $3;"                                                                                           \
            "sra	$16, $16, 10;"                                                                                       \
            "addu	$12, $16, $15;"                                                                                     \
            "mtc2	$12, $5;"                                                                                           \
            "nop;"                                                                                                    \
            "nop;" /* Use MAC results so IR saturation does not clamp the accumulated output. */                      \
            ".word %2;"                                                                                               \
            "mfc2	$14, $25;"                                                                                          \
            "mfc2	$15, $26;"                                                                                          \
            "mfc2	$16, $27;"                                                                                          \
            "nop;"                                                                                                    \
            "nop;"                                                                                                    \
            ".word %3;"                                                                                               \
            "mfc2	$12, $25;"                                                                                          \
            "nop;"                                                                                                    \
            "sra	$12, $12, 2;"                                                                                        \
            "addu	$14, $12, $14;"                                                                                     \
            "mfc2	$12, $26;"                                                                                          \
            "nop;"                                                                                                    \
            "sra	$12, $12, 2;"                                                                                        \
            "addu	$15, $12, $15;"                                                                                     \
            "mfc2	$12, $27;"                                                                                          \
            "nop;"                                                                                                    \
            "sra	$12, $12, 2;"                                                                                        \
            "addu	$16, $12, $16;"                                                                                     \
            "nop;"                                                                                                    \
            "nop;"                                                                                                    \
            ".word %4;"                                                                                               \
            "mfc2	$12, $25;"                                                                                          \
            "nop;"                                                                                                    \
            "sll	$12, $12, 8;"                                                                                        \
            "addu	$14, $12, $14;"                                                                                     \
            "mfc2	$12, $26;"                                                                                          \
            "nop;"                                                                                                    \
            "sll	$12, $12, 8;"                                                                                        \
            "addu	$15, $12, $15;"                                                                                     \
            "mfc2	$12, $27;"                                                                                          \
            "nop;"                                                                                                    \
            "sll	$12, $12, 8;"                                                                                        \
            "addu	$16, $12, $16;"                                                                                     \
            "sw	$14, 0( %1 );"                                                                                        \
            "sw	$15, 4( %1 );"                                                                                        \
            "sw	$16, 8( %1 )"                                                                                         \
            :                                                                                                         \
            : "r"(inputVector), "r"(outputVector),                                                                    \
              "i"(GTE_ROT_TRANS_LV_LOW_ROTATE_TRANSLATE),                                                             \
              "i"(GTE_ROT_TRANS_LV_MIDDLE_ROTATE),                                                                    \
              "i"(GTE_ROT_TRANS_LV_HIGH_ROTATE)                                                                       \
            : "$12", "$13", "$14", "$15", "$16", "memory");                                                           \
    } while (0)

/// Stores the signed screen-space area used to test a flat triangle's winding.
///
/// Reads the three word-aligned packed XY pairs from `packet` in vertex order;
/// their signed halves are pixel coordinates. `facingArea` must address one
/// word-aligned writable s32 and receives NCLIP's signed double area in square
/// pixels. Negative values pass the flat-triangle draw handler's facing test.
/// All coordinate reads precede the result store, so the buffers may overlap.
/// Performs no projection or clipping. Replaces SXY0..SXY2 and clobbers MAC0/FLAG.
static inline void _tmdStoreFlatTriangleFacing(const POLY_F3* packet, s32* facingArea)
{
    gte_ldSXYP(*(const u32*)&packet->x0);
    gte_ldSXYP(*(const u32*)&packet->x1);
    gte_ldSXYP(*(const u32*)&packet->x2);
    gte_nclip();
    gte_stopz(facingArea);
}

/// Stores the signed double area for a flat quad's first facing test.
///
/// `packet` supplies projected vertices 0..2 as word-aligned packed XY pairs:
/// signed 16-bit X in the low half and Y in the high half, both in pixels.
/// Reads only those three pairs (12 bytes), not the header or vertex 3.
/// `facingArea` addresses one word-aligned writable s32 and receives MAC0's
/// signed 32-bit NCLIP result in square pixels; overflow is not checked.
/// Positive results accept the quad's facing; zero or negative results require
/// the caller's second-triangle test.
///
/// Leaves vertices 0..2 in SXY0..SXY2. Preserve that FIFO until pushing vertex 3
/// to test vertices 1..3, whose accepted winding is negative. No prior GTE
/// setup is required. Performs no projection or clipping and clobbers MAC0/FLAG.
/// All coordinate reads precede the result store, so the buffers may overlap.
/// Both pointers are borrowed for this call and are not retained.
static inline void _tmdStoreFlatQuadFirstTriangleFacing(const POLY_F4* packet, s32* facingArea)
{
    gte_ldSXYP(*(const u32*)&packet->x0);
    gte_ldSXYP(*(const u32*)&packet->x1);
    gte_ldSXYP(*(const u32*)&packet->x2);
    gte_nclip();
    gte_stopz(facingArea);
}

/// Stores a projected Gouraud textured triangle's signed double area for facing tests.
///
/// `packet` supplies three word-aligned packed XY pairs in corner order 0..2:
/// signed 16-bit X in the low half and Y in the high half, both in pixels.
/// Reads only those pairs (12 bytes); the header, colours and texture fields
/// are not inputs. `facingArea` must address one word-aligned writable s32;
/// it receives MAC0's signed 32-bit NCLIP result in square pixels. Overflow is
/// not checked, and the caller chooses which winding survives.
///
/// Leaves corners 0..2 in SXY0..SXY2 and clobbers MAC0/FLAG. No prior GTE setup
/// is required; performs no projection, clipping or lighting. All coordinate
/// reads precede the result store, so the buffers may overlap. Both pointers
/// are borrowed for the call and are not retained.
static inline void _tmdStoreTexturedTriangleFacing(const POLY_GT3* packet, s32* facingArea)
{
    gte_ldSXYP(*(const u32*)&packet->x0);
    gte_ldSXYP(*(const u32*)&packet->x1);
    gte_ldSXYP(*(const u32*)&packet->x2);
    gte_nclip();
    gte_stopz(facingArea);
}

/// Stores the signed double area of a projected Gouraud textured quad's first triangle.
///
/// `packet` supplies word-aligned packed XY pairs for corners 0..2: signed
/// 16-bit X in the low half and Y in the high half, both in pixels. Reads
/// exactly those three four-byte pairs; the header, colours, texture fields
/// and corner 3 are not inputs. `facingArea` must address one word-aligned
/// writable s32 and receives MAC0's signed NCLIP result in square pixels.
/// Overflow is not checked, and no facing decision is made here.
///
/// Leaves corners 0..2 in SXY0..SXY2. Preserve that FIFO until pushing corner 3
/// to test corners 1..3. The environment-layer caller accepts a positive first
/// result and a negative second result; zero rejects either triangle.
/// No prior GTE setup is required. Performs no projection, clipping or lighting;
/// replaces the screen FIFO and clobbers MAC0/FLAG. All coordinate reads precede
/// the store, so the result may overlap writable packet storage. Both pointers
/// are borrowed for this call and are not retained.
static inline void _tmdStoreTexturedQuadFirstTriangleFacing(const POLY_GT4* packet, s32* facingArea)
{
    gte_ldSXYP(*(const u32*)&packet->x0);
    gte_ldSXYP(*(const u32*)&packet->x1);
    gte_ldSXYP(*(const u32*)&packet->x2);
    gte_nclip();
    gte_stopz(facingArea);
}

/// Projects and lights corners with reduced-scale screen/normal environment mapping.
///
/// `elements` starts at a readable four-halfword prefix per element; stride is
/// in u32 words. Initial `elemCount` is 0..65535. Masked geometry references
/// must select complete SVECTORs and vertexRef >> 3 must be below 1024 in the
/// writable depth cache. XY and RGB destinations are word-aligned byte offsets
/// in the first packet region, covering respectively six and four bytes.
///
/// Requires GTE projection, rotation, light and colour matrices and material
/// RGB/code already loaded. Adjacent equal vertex references retain SXY and Z;
/// new projections publish FLAG and mark invalid cached depth, while still
/// writing XY and colour. U/V are (screen >> 4) + 32 + (rotated normal >> 8),
/// with signed halfword scratch stores between operations and byte truncation
/// at the final stores. No texture clipping or ordering-table linking occurs.
///
/// Returns the advanced word cursor. Zero count leaves the count unchanged;
/// positive count is consumed to -1. Leaves packet cursors unchanged, overwrites
/// GTE/workspace projection and normal scratch, and retains no borrowed storage.
static inline u32* _tmdXformStreamVertsEnvMapLitReduced(TmdStreamWorkspace* workspace, u32* elements)
{
    s32                         previousVertexRef;
    s32                         initialElementCount;
    u32                         vertexRef;
    const _TmdEnvMapCornerRefs* cornerRefs;
    u8*                         packetDestination;

    initialElementCount = workspace->elemCount;
    if (initialElementCount == 0) {
        return elements;
    }
    previousVertexRef    = TMD_ENV_MAP_NO_PREVIOUS_VERTEX;
    workspace->elemCount = initialElementCount + previousVertexRef;
    if (initialElementCount > 0) {
        do {
            cornerRefs = (const _TmdEnvMapCornerRefs*)elements;
            vertexRef  = cornerRefs->vertexByteRef;
            // Reuse projection only for adjacent identical vertex references.
            if (vertexRef != previousVertexRef) {
                gte_ldv0((const u8*)workspace->verts + (vertexRef & TMD_ENV_MAP_GEOMETRY_BYTE_OFFSET_MASK));
                gte_rtps();
                gte_stsz(&workspace->gteResult);
                gte_stflg(&workspace->gteFlag);
                if (workspace->gteFlag & TMD_GTE_ERROR_FLAG) {
                    workspace->gteResult |= TMD_VERTEX_DEPTH_INVALID;
                }
                workspace->szTable[cornerRefs->vertexByteRef >> TMD_ENV_MAP_VERTEX_INDEX_SHIFT] = workspace->gteResult;
            }
            previousVertexRef = cornerRefs->vertexByteRef;
            packetDestination = workspace->preXformWrite + cornerRefs->xyByteOffset;
            gte_stsxy(packetDestination);
            gte_stsxy(&workspace->texCoord);
            gte_ldv0((const u8*)workspace->normals + (cornerRefs->normalByteRef & TMD_ENV_MAP_GEOMETRY_BYTE_OFFSET_MASK));
            gte_nccs();
            gte_rtv0();
            // Preserve the signed halfword intermediates before truncating U/V to bytes.
            workspace->texCoord.vx = (workspace->texCoord.vx >> TMD_ENV_MAP_REDUCED_SCREEN_SHIFT) + TMD_ENV_MAP_REDUCED_CENTER;
            workspace->texCoord.vy = (workspace->texCoord.vy >> TMD_ENV_MAP_REDUCED_SCREEN_SHIFT) + TMD_ENV_MAP_REDUCED_CENTER;
            gte_stsv(&workspace->elemNormal);
            workspace->texCoord.vx                                              += workspace->elemNormal.vx >> TMD_ENV_MAP_REDUCED_NORMAL_SHIFT;
            workspace->texCoord.vy                                              += workspace->elemNormal.vy >> TMD_ENV_MAP_REDUCED_NORMAL_SHIFT;
            packetDestination                                                    = workspace->preXformWrite + cornerRefs->xyByteOffset;
            packetDestination[OFFSET_OF(POLY_GT3, u0) - OFFSET_OF(POLY_GT3, x0)] = (u8)workspace->texCoord.vx;
            packetDestination                                                    = workspace->preXformWrite + cornerRefs->xyByteOffset;
            packetDestination[OFFSET_OF(POLY_GT3, v0) - OFFSET_OF(POLY_GT3, x0)] = (u8)workspace->texCoord.vy;
            elements                                                            += workspace->elemStride;
            gte_strgb(workspace->preXformWrite + cornerRefs->rgbByteOffset);
        } while (workspace->elemCount-- > 0);
    }
    return elements;
}

/// Rebuilds one coordinate after the ancestors that `root` does not exclude.
///
/// A stale parent continues through `actorRenderComposeCoordChain`. At the
/// excluded ancestor, a clear stamp copies the local matrix into `workm` and
/// any other stamp keeps the cached matrix. `gte_RotTransLV` keeps the local
/// translation's full signed range; its chunk products round separately.
static __inline__ void _actorRenderRefreshCoord(GfxCoord* coord, s32 stamp, s32 parity, GfxCoord* root)
{
    GfxCoord* parent;

    // Clear visit parity. The shift pair is `GRAPHICS_COORD_STAMP_MASK`, and it
    // keeps this copy and the draw pass's copy on the same `sll`/`srl`.
    parent              = coord->parent;
    coord->composeStamp = (coord->composeStamp << 1) >> 1;
    if (parent == root) {
        // Excluded ancestor. A nonzero stamp preserves a caller-supplied cache.
        if (coord->composeStamp == GRAPHICS_COORD_DIRTY) {
            coord->workm        = coord->coord;
            coord->composeStamp = stamp;
        }
    } else {
        // Ancestors first. A dirty stamp is parity 0, so an even pass still refreshes it.
        if ((parent->composeStamp == GRAPHICS_COORD_DIRTY) || ((parent->composeStamp >> 31) != parity)) {
            actorRenderComposeCoordChain(parent, stamp, parity, root);
        }
        if (coord->composeStamp < (parent->composeStamp & GRAPHICS_COORD_STAMP_MASK)) {
            // Parent cache is newer. CompMatrix writes the product, then the same
            // rotation is repeated and the short-vector translation is replaced.
            gte_CompMatrix(&parent->workm, &coord->coord, &coord->workm);
            coord->composeStamp = stamp;
            gte_SetRotMatrix(&parent->workm);
            gte_ldclmv(&coord->coord.m[0][0]);
            gte_rtir();
            gte_stclmv(&coord->workm.m[0][0]);
            gte_ldclmv(&coord->coord.m[0][1]);
            gte_rtir();
            // Between the second column's rotation and its writeback.
            coord->composeStamp = stamp;
            gte_stclmv(&coord->workm.m[0][1]);
            gte_ldclmv(&coord->coord.m[0][2]);
            gte_rtir();
            gte_SetTransVector(parent->workm.t);
            gte_stclmv(&coord->workm.m[0][2]);
            gte_RotTransLV(coord->coord.t, coord->workm.t);
        }
    }
    if (parity != 0) {
        coord->composeStamp |= GRAPHICS_COORD_VISIT_PARITY_BIT;
    }
}

TmdObject* modelObjectAttachTmd(Task* task, TmdSource* source)
{
    TmdObject*   model;
    TmdListNode* tail;
    TmdListNode* sentinel;

    model = tmdCreateModel(source, 0);
    if (model != NULL) {
        // Append only after allocation succeeds; task ownership starts below.
        sentinel         = &gTmdList;
        tail             = sentinel->prev;
        model->link.next = tail->next;
        tail->next       = &model->link;
        model->link.prev = tail;
        sentinel->prev   = &model->link;
        task->extra.tmd  = model;
        task->bodyKind   = TASK_BODY_TMD;
    }
    return model;
}

ModelObjectCoordBody* modelObjectAttachCoordBody(Task* task)
{
    ModelObjectCoordBody* body;
    TmdListNode*          tail;
    TmdListNode*          sentinel;
    GfxCoord*             coord;

    body = memCalloc(sizeof(*body), false);
    if (body != NULL) {
        coord         = &body->ownedCoord;
        body->coord   = coord;
        body->field_C = 1;
        coord->parent = &gGfxViewCoord;
        gfxSetRotIdentity(&coord->coord);
        coord->coord.t[2]   = 0;
        coord->coord.t[1]   = 0;
        coord->coord.t[0]   = 0;
        coord->param.rot.vz = 0;
        coord->param.rot.vy = 0;
        coord->param.rot.vx = 0;
        coord->composeStamp = GRAPHICS_COORD_DIRTY;
        // Append the initialized coordinate; task ownership starts below.
        sentinel              = &gModelObjectCoordBodyList;
        tail                  = sentinel->prev;
        body->link.next       = tail->next;
        tail->next            = &body->link;
        body->link.prev       = tail;
        sentinel->prev        = &body->link;
        task->extra.coordBody = body;
        task->bodyKind        = TASK_BODY_COORD;
    } else {
        printf("new_disp_2d ----> NULL\n");
    }
    return body;
}

TmdObject* modelObjectAttachTmdWithBufferFlags(Task* task, TmdSource* source, s32 bufferFlags)
{
    TmdObject*   model;
    TmdListNode* tail;
    TmdListNode* sentinel;

    model = tmdCreateModel(source, bufferFlags);
    if (model != NULL) {
        // Append only after allocation succeeds; task ownership starts below.
        sentinel         = &gTmdList;
        tail             = sentinel->prev;
        model->link.next = tail->next;
        tail->next       = &model->link;
        model->link.prev = tail;
        sentinel->prev   = &model->link;
        task->extra.tmd  = model;
        task->bodyKind   = TASK_BODY_TMD;
    }
    return model;
}

void modelObjectUnlinkTmd(TmdListNode* node)
{
    TmdListNode*  next;
    TmdListNode** prevSlot;
    TmdListNode*  prev;

    next = node->next;
    if (next == NULL) {
        // No successor holds the back-link, so retarget the sentinel's prev.
        prevSlot = &gTmdList.prev;
    } else {
        prevSlot = &next->prev;
    }
    prev       = node->prev;
    *prevSlot  = prev;
    prev->next = node->next;
}

void modelObjectFreeTmd(TmdObject* model)
{
    // The primitive buffer is a separate auxiliary-heap allocation.
    if (model->buffer != NULL) {
        memFreeFromHeap(model->buffer, true);
        model->buffer = NULL;
    }
    memFree(model);
}

void modelObjectUnlinkCoordBody(TmdListNode* node)
{
    TmdListNode*  next;
    TmdListNode** prevSlot;
    TmdListNode*  prev;

    next = node->next;
    if (next == NULL) {
        // No successor holds the back-link, so retarget the sentinel's prev.
        prevSlot = &gModelObjectCoordBodyList.prev;
    } else {
        prevSlot = &next->prev;
    }
    prev       = node->prev;
    *prevSlot  = prev;
    prev->next = node->next;
}

void modelObjectFreeCoordBody(ModelObjectCoordBody* body)
{
    memFree(body);
}

/// Saves the current lists and starts drawing from temporary empty lists.
///
/// Only one stash may be outstanding, and the saved elements must stay alive
/// until the lists are restored to their original sentinels.
static void _modelObjectStashLists(void)
{
    // Save endpoints, keeping the elements tied to their original sentinels.
    _gModelObjectSavedModelList    = gTmdList;
    _gModelObjectSavedDisp2dList   = gModelObjectCoordBodyList;
    gTmdList.next                  = NULL;
    gTmdList.prev                  = &gTmdList;
    gModelObjectCoordBodyList.next = NULL;
    gModelObjectCoordBodyList.prev = &gModelObjectCoordBodyList;
    _gModelObjectTemporaryDrawTask = Task_Spawn(0, MODEL_OBJECT_STASH_DRAW_TASK, 0, 0);
}

/// Stops the temporary draw task and restores the saved lists to their sentinels.
///
/// Requires a preceding stash whose saved elements are still alive.
static void _modelObjectRestoreLists(void)
{
    taskCallExit(_gModelObjectTemporaryDrawTask);
    gTmdList                  = _gModelObjectSavedModelList;
    gModelObjectCoordBodyList = _gModelObjectSavedDisp2dList;
}

void actorRenderComposeCoordChain(GfxCoord* coord, s32 stamp, s32 parity, GfxCoord* root)
{
    _actorRenderRefreshCoord(coord, stamp, parity, root);
}

/// Returns the first task on the active list that owns `targetCoord`, or `NULL`.
static Task* _modelObjectFindTaskByCoord(GfxCoord* targetCoord)
{
    Task*      task;
    TmdObject* model;
    GfxCoord*  coord;
    u32        partIndex;
    s32        found;
    u32        partCount;

    task = taskGetActiveList()->next;
    if (task != NULL) {
        do {
            found = 0;
            switch (task->bodyKind) {
                case TASK_BODY_TMD:
                    model     = task->extra.tmd;
                    partCount = model->partCount;
                    coord     = model->coords;
                    for (partIndex = 0; partIndex < partCount; partIndex++) {
                        if (coord == targetCoord) {
                            found = 1;
                            break;
                        }
                        coord++;
                    }
                    break;
                case TASK_BODY_COORD:
                    coord = task->extra.coordBody->coord;
                    if (coord == targetCoord) {
                        found = 1;
                    }
                    break;
            }
            if (found != 0) {
                break;
            }
            task = task->node.next;
        } while (task != NULL);
    }
    return task;
}

void Gp_DrawDisp2dOt(Task* unused)
{
    Gp_DrawActorTmdActive(&Gpu_OtBuffers[gDisplayState.drawBuffer]);
}

u32* tmdDrawStreamPrimF4PreXform(TmdStreamWorkspace* workspace, s32 objectFlags, u32* elements)
{
    /// Extracts the aligned depth-cache byte offset from a flat quad's corner reference.
    ///
    /// Applied to each of the element's first four u16 values, keeps bits 2..15
    /// and clears bits 0..1, whose meaning is unproven. The result is 0..65532
    /// bytes, divided by sizeof(*vertexDepths) to select one s32 depth entry.
    /// Each offset must name a depth initialized earlier in this draw walk;
    /// normal drawing supplies 1024 entries, allowing offsets 0..4092.
    /// The mask enforces alignment without checking the cache's bounds.
    enum { TMD_F4_PRE_XFORM_DEPTH_REFERENCE_MASK = 0xFFFC };
    /// Converts a flat quad's scaled GTE ordering depth to an OT tag index.
    ///
    /// After the u32 OTZ left shift by `gDisplayState.otDepthShift` (0..3),
    /// discard four low bits: sixteen scaled depth units select one tag.
    /// The following mask keeps ten index bits (scaled-depth bits 4..13),
    /// wrapping to 0..1023 rather than clamping. Indices count four-byte
    /// tags, not bytes. The selected table must contain the resulting entry,
    /// including the model's signed offset already applied to `workspace->ot`.
    enum { TMD_F4_PRE_XFORM_OT_INDEX_SHIFT = 4 };
    POLY_F4*            packet;
    s32*                gteResultDestination;
    const DisplayState* displayState;
    u32                 invalidDepthMask;
    const u16*          depthRefs;
    s32                 cachedDepth;
    u32                 depthByteOffset;
    const s32*          vertexDepths;

    packet = (POLY_F4*)workspace->preXformWrite;
    if (workspace->elemCount-- > 0) {
        gteResultDestination = &workspace->gteResult;
        invalidDepthMask     = TMD_VERTEX_DEPTH_INVALID;
        displayState         = &gDisplayState;
        do {
            depthRefs = (const u16*)elements;
            // Either triangle may accept the quad; their winding signs are opposite.
            _tmdStoreFlatQuadFirstTriangleFacing(packet, gteResultDestination);
            if (workspace->gteResult > 0) {
                goto draw;
            }
            gte_ldSXYP(GPU_PRIMITIVE_XY_WORD(packet, 3));
            gte_nclip();
            gte_stopz(gteResultDestination);
            if (workspace->gteResult < 0) {
            draw:
                // Reject failed projections before averaging all four cached depths.
                vertexDepths    = workspace->szTable;
                depthByteOffset = depthRefs[0] & TMD_F4_PRE_XFORM_DEPTH_REFERENCE_MASK;
                cachedDepth     = vertexDepths[depthByteOffset / sizeof(*vertexDepths)];
                if ((cachedDepth & invalidDepthMask) == 0) {
                    gte_ldSZ0(cachedDepth);
                    depthByteOffset = depthRefs[1] & TMD_F4_PRE_XFORM_DEPTH_REFERENCE_MASK;
                    cachedDepth     = vertexDepths[depthByteOffset / sizeof(*vertexDepths)];
                    if ((cachedDepth & invalidDepthMask) == 0) {
                        gte_ldSZ1(cachedDepth);
                        depthByteOffset = depthRefs[2] & TMD_F4_PRE_XFORM_DEPTH_REFERENCE_MASK;
                        cachedDepth     = vertexDepths[depthByteOffset / sizeof(*vertexDepths)];
                        if ((cachedDepth & invalidDepthMask) == 0) {
                            gte_ldSZ2(cachedDepth);
                            depthByteOffset = depthRefs[3] & TMD_F4_PRE_XFORM_DEPTH_REFERENCE_MASK;
                            cachedDepth     = vertexDepths[depthByteOffset / sizeof(*vertexDepths)];
                            if ((cachedDepth & invalidDepthMask) == 0) {
                                gte_ldSZ3(cachedDepth);
                                gte_avsz4();
                                gte_stotz(gteResultDestination);
                                gte_stotz(gteResultDestination);
                                addPrim(&workspace->ot[((u32)workspace->gteResult << displayState->otDepthShift) >>
                                                           TMD_F4_PRE_XFORM_OT_INDEX_SHIFT &
                                                       (GPU_ORDERING_TABLE_DEPTH_BYTE_MASK / sizeof(*workspace->ot))],
                                        packet);
                            }
                        }
                    }
                }
            }
            // Culled elements still consume their construction pass's packet slot.
            packet++;
            elements += workspace->elemStride;
        } while (workspace->elemCount-- > 0);
    }
    workspace->preXformWrite = (u8*)packet;
    return elements;
}

u32* tmdDrawStreamPrimF3PreXform(TmdStreamWorkspace* workspace, s32 objectFlags, u32* elements)
{
    enum {
        /// Selects the aligned byte offset in a pre-transformed flat triangle's depth reference.
        ///
        /// Each u16 corner reference selects a four-byte `szTable` entry after
        /// clearing bits 0..1; their meaning is unproven. The result is 0..65532
        /// bytes, divided by sizeof(*vertexDepths) for indexing. Valid offsets
        /// are only 0..4092 and must name a depth initialized earlier in this
        /// draw walk; masking provides alignment without checking that range.
        TMD_F3_PRE_XFORM_DEPTH_REFERENCE_MASK = 0xFFFC
    };
    /// Converts the scaled GTE ordering depth to an ordering-table tag index.
    ///
    /// After the unsigned OTZ left shift by `gDisplayState.otDepthShift`,
    /// discard four low bits: sixteen scaled OTZ units select one tag.
    /// The following mask keeps ten index bits (scaled-depth bits 4..13),
    /// wrapping to 0..1023 relative to `workspace->ot`. Indices count
    /// four-byte tags; the selected table must contain the resulting entry,
    /// including the model's signed offset already applied to that base.
    enum { TMD_F3_PRE_XFORM_OT_INDEX_SHIFT = 4 };
    POLY_F3*            packet;
    s32*                gteResultDestination;
    const DisplayState* displayState;
    u32                 invalidDepthMask;
    const u16*          depthRefs;
    s32                 cachedDepth;
    u32                 depthByteOffset;
    const s32*          vertexDepths;

    packet = (POLY_F3*)workspace->preXformWrite;
    if (workspace->elemCount-- > 0) {
        gteResultDestination = &workspace->gteResult;
        invalidDepthMask     = TMD_VERTEX_DEPTH_INVALID;
        displayState         = &gDisplayState;
        do {
            depthRefs = (const u16*)elements;
            // Test the packet's projected winding before looking up its depths.
            _tmdStoreFlatTriangleFacing(packet, gteResultDestination);
            if (workspace->gteResult < 0) {
                vertexDepths    = workspace->szTable;
                depthByteOffset = depthRefs[0] & TMD_F3_PRE_XFORM_DEPTH_REFERENCE_MASK;
                cachedDepth     = vertexDepths[depthByteOffset / sizeof(*vertexDepths)];
                if ((cachedDepth & invalidDepthMask) == 0) {
                    gte_ldSZ0(cachedDepth);
                    depthByteOffset = depthRefs[1] & TMD_F3_PRE_XFORM_DEPTH_REFERENCE_MASK;
                    cachedDepth     = vertexDepths[depthByteOffset / sizeof(*vertexDepths)];
                    if ((cachedDepth & invalidDepthMask) == 0) {
                        gte_ldSZ1(cachedDepth);
                        depthByteOffset = depthRefs[2] & TMD_F3_PRE_XFORM_DEPTH_REFERENCE_MASK;
                        cachedDepth     = vertexDepths[depthByteOffset / sizeof(*vertexDepths)];
                        if ((cachedDepth & invalidDepthMask) == 0) {
                            // Preserve SZ0..SZ2 loads: AVSZ3 also reads the previous SZ3.
                            gte_ldSZ2(cachedDepth);
                            gte_avsz3();
                            gte_stotz(gteResultDestination);
                            gte_stotz(gteResultDestination);
                            addPrim(&workspace->ot[((u32)workspace->gteResult << displayState->otDepthShift) >>
                                                       TMD_F3_PRE_XFORM_OT_INDEX_SHIFT &
                                                   (GPU_ORDERING_TABLE_DEPTH_BYTE_MASK / sizeof(*workspace->ot))],
                                    packet);
                        }
                    }
                }
            }
            // Culled elements still consume their construction pass's packet slot.
            packet++;
            elements += workspace->elemStride;
        } while (workspace->elemCount-- > 0);
    }
    workspace->preXformWrite = (u8*)packet;
    return elements;
}

u32* tmdDrawStreamPrimGt3PreXformEnvLayer(TmdStreamWorkspace* workspace, s32 objectFlags, u32* elements)
{
    enum {
        /// Extracts the aligned depth-cache byte offset from an environment triangle's corner reference.
        ///
        /// Each of the element's first three u16 references retains bits 15..2;
        /// the discarded low bits' role is unproven. Results are 0..65532 bytes,
        /// divided by sizeof(*vertexDepths) to select a four-byte `szTable` entry.
        /// The draw pass supplies 1024 entries, so valid offsets are 0..4092 and
        /// must name depths initialized earlier in the same draw walk. This mask
        /// provides alignment without checking bounds or projection validity;
        /// `TMD_VERTEX_DEPTH_INVALID` is tested in the selected cache entry.
        TMD_GT3_ENV_DEPTH_BYTE_OFFSET_MASK = 0xFFFC,
        /// GPU texture-depth selector for direct-colour environment sampling.
        ///
        /// Passed unshifted to `getTPage`: selector 2 sets texture-depth bits
        /// 8..7 to binary 10. Each texel occupies one 16-bit VRAM word, so U
        /// advances one word per texel and the packet's CLUT is unused.
        /// Both environment pages use this format; `GPU_BLEND_ADD` selects
        /// their blend mode separately.
        TMD_GT3_ENV_TEXTURE_DEPTH_DIRECT = 2,
        /// Packed GPU texture-page settings for environment triangles with no second-page markers.
        ///
        /// Encodes `0x137`: direct colour and additive semitransparency at
        /// VRAM word coordinates (448,256). Projection supplies U in 0..255
        /// and V in 0..239 relative to this origin; the model's texture-page
        /// offset is not applied. Written to the layer's unsigned 16-bit
        /// `POLY_GT3.tpage`, with semitransparency enabled by the layer command.
        /// Any marked corner instead selects `TMD_GT3_ENV_SECOND_TEXTURE_PAGE`,
        /// whose origin is 128 texels to the right, overlapping this page.
        TMD_GT3_ENV_FIRST_TEXTURE_PAGE = getTPage(TMD_GT3_ENV_TEXTURE_DEPTH_DIRECT, GPU_BLEND_ADD, 448, 256),
        /// Packed GPU texture-page settings for environment triangles with any second-page marker.
        ///
        /// Encodes `0x139`: direct colour and additive semitransparency at
        /// VRAM word coordinates (576,256), 128 texels right of the overlapping
        /// first page. Projection has already rebased marked corners' U;
        /// unmarked corners subtract `TMD_GT3_ENV_PAGE_U_DISPLACEMENT` when
        /// U >= 128 and clamp smaller U to zero. Written to the layer's unsigned
        /// 16-bit `POLY_GT3.tpage`, with semitransparency enabled by its command.
        TMD_GT3_ENV_SECOND_TEXTURE_PAGE = getTPage(TMD_GT3_ENV_TEXTURE_DEPTH_DIRECT, GPU_BLEND_ADD, 576, 256),
        /// Horizontal origin displacement between the environment texture pages, in texels.
        ///
        /// The direct-colour pages start at VRAM X=448 and X=576. Selecting the
        /// second page rebases only unmarked corners: subtract from U=128..255
        /// and clamp smaller U to zero. Marked corners were already rebased by
        /// projection. The signed-byte test selects unsigned U >= 128; the
        /// stored U remains an unsigned GPU texel coordinate.
        TMD_GT3_ENV_PAGE_U_DISPLACEMENT = 128,
        /// GPU command byte for the opaque, colour-modulated Gouraud-textured base triangle.
        ///
        /// Bits 1 and 0 are clear: semi-transparency is disabled and vertex colours
        /// modulate the texture. The second `POLY_GT3` in each pair uses the model's
        /// texture and is linked ahead of the environment layer.
        TMD_GT3_ENV_BASE_COMMAND = 0x34,
        /// GPU command byte for the colour-modulated, semitransparent environment triangle.
        ///
        /// Bit 1 enables semitransparency; bit 0 stays clear so lit vertex colours
        /// modulate the texture. The layer's `tpage` selects additive blending.
        /// Written to the first `POLY_GT3` of each pair after its code byte has
        /// been read as a second-page marker. The opaque base is drawn first.
        /// This command is fixed, independent of `objectFlags`.
        TMD_GT3_ENV_LAYER_COMMAND = 0x36,
        /// Number of environment-triangle corners visited during second-page U adjustment.
        ///
        /// Covers `u0/u1/u2` and their `code/p1/p2` page markers in the
        /// environment packet of each environment/base pair.
        TMD_GT3_ENV_CORNER_COUNT = 3,
        /// Number of GPU packets occupied by one pre-transformed environment-layered triangle.
        ///
        /// Slot 0 is the semitransparent environment layer; slot 1 is the opaque
        /// model-texture base. Counts complete `POLY_GT3` objects, including DMA
        /// tags. Construction reserves both slots, and drawing advances past
        /// both even when the triangle is culled.
        TMD_GT3_ENV_PACKET_COUNT = 2,
        /// Byte stride between corresponding fields of successive environment-triangle corners.
        ///
        /// Advances the U and 0/1 second-page-marker cursors through `u0/u1/u2`
        /// and `code/p1/p2` in the layer packet. The byte view covers the complete
        /// packet pair, including the final undereferenced cursor advances.
        TMD_GT3_ENV_CORNER_STRIDE_BYTES = OFFSET_OF(POLY_GT3, u1) - OFFSET_OF(POLY_GT3, u0),
        /// Right shift converting scaled GTE OTZ to an ordering-table tag index.
        ///
        /// Applied after the u32 left shift by `gDisplayState.otDepthShift`:
        /// sixteen scaled OTZ units select one four-byte tag. This combines
        /// the depth-to-byte-offset right shift by two with bytes-to-tags
        /// conversion. The following mask, `GPU_ORDERING_TABLE_DEPTH_BYTE_MASK`
        /// divided by sizeof(*workspace->ot), keeps scaled-depth bits 4..13
        /// and wraps indices to 0..1023; it does not clamp depth. Both packets
        /// use the same index relative to `workspace->ot`, already displaced
        /// by the model's signed tag offset. The selected OT must contain it.
        TMD_GT3_ENV_OT_INDEX_SHIFT = 4
    };
    /// One element's environment packet followed by its opaque base packet.
    typedef POLY_GT3 _TmdEnvTrianglePair[TMD_GT3_ENV_PACKET_COUNT];

    STATIC_ASSERT(OFFSET_OF(POLY_GT3, u2) - OFFSET_OF(POLY_GT3, u1) == TMD_GT3_ENV_CORNER_STRIDE_BYTES &&
                      OFFSET_OF(POLY_GT3, p1) - OFFSET_OF(POLY_GT3, code) == TMD_GT3_ENV_CORNER_STRIDE_BYTES &&
                      OFFSET_OF(POLY_GT3, p2) - OFFSET_OF(POLY_GT3, p1) == TMD_GT3_ENV_CORNER_STRIDE_BYTES,
                  tmdGt3EnvCornerStride);
    STATIC_ASSERT(OFFSET_OF(POLY_GT3, u0) + (TMD_GT3_ENV_CORNER_COUNT - 1) * TMD_GT3_ENV_CORNER_STRIDE_BYTES == OFFSET_OF(POLY_GT3, u2) &&
                      OFFSET_OF(POLY_GT3, code) + (TMD_GT3_ENV_CORNER_COUNT - 1) * TMD_GT3_ENV_CORNER_STRIDE_BYTES == OFFSET_OF(POLY_GT3, p2),
                  tmdGt3EnvCornerCount);
    STATIC_ASSERT(OFFSET_OF(POLY_GT3, code) + TMD_GT3_ENV_CORNER_COUNT * TMD_GT3_ENV_CORNER_STRIDE_BYTES <= sizeof(_TmdEnvTrianglePair) &&
                      OFFSET_OF(POLY_GT3, u0) + TMD_GT3_ENV_CORNER_COUNT * TMD_GT3_ENV_CORNER_STRIDE_BYTES <= sizeof(_TmdEnvTrianglePair),
                  tmdGt3EnvCornerCursorBounds);

    _TmdEnvTrianglePair* packetPair;
    s32*                 gteResultDestination;
    u32                  invalidDepthMask;
    s32                  packetWordCount;
    s32                  baseCommand;
    const DisplayState*  displayState;
    const u16*           depthRefs;
    s32                  depthOrPageMarkers;
    u32                  depthByteOffset;
    const s32*           vertexDepths;
    u8*                  pageMarker;
    u8*                  textureU;
    s32                  cornerIndex;
    s32                  texturePage;

    packetPair = (_TmdEnvTrianglePair*)workspace->preXformWrite;
    if (workspace->elemCount-- > 0) {
        gteResultDestination = &workspace->gteResult;
        invalidDepthMask     = TMD_VERTEX_DEPTH_INVALID;
        packetWordCount      = (sizeof((*packetPair)[0]) - sizeof((*packetPair)[0].tag)) / sizeof(u32);
        baseCommand          = TMD_GT3_ENV_BASE_COMMAND;
        displayState         = &gDisplayState;
        do {
            depthRefs = (const u16*)elements;
            // Cull from the environment packet's projected winding.
            _tmdStoreTexturedTriangleFacing(&(*packetPair)[0], gteResultDestination);
            if (workspace->gteResult > 0) {
                vertexDepths       = workspace->szTable;
                depthByteOffset    = depthRefs[0] & TMD_GT3_ENV_DEPTH_BYTE_OFFSET_MASK;
                depthOrPageMarkers = vertexDepths[depthByteOffset / sizeof(*vertexDepths)];
                if ((depthOrPageMarkers & invalidDepthMask) == 0) {
                    gte_ldSZ1(depthOrPageMarkers);
                    depthByteOffset    = depthRefs[1] & TMD_GT3_ENV_DEPTH_BYTE_OFFSET_MASK;
                    depthOrPageMarkers = vertexDepths[depthByteOffset / sizeof(*vertexDepths)];
                    if ((depthOrPageMarkers & invalidDepthMask) == 0) {
                        gte_ldSZ2(depthOrPageMarkers);
                        depthByteOffset    = depthRefs[2] & TMD_GT3_ENV_DEPTH_BYTE_OFFSET_MASK;
                        depthOrPageMarkers = vertexDepths[depthByteOffset / sizeof(*vertexDepths)];
                        if ((depthOrPageMarkers & invalidDepthMask) == 0) {
                            gte_ldSZ3(depthOrPageMarkers);
                            gte_avsz3();
                            // Projection leaves second-page markers in the colour groups' high bytes.
                            depthOrPageMarkers = (*packetPair)[0].code | (*packetPair)[0].p1 | (*packetPair)[0].p2;
                            if (depthOrPageMarkers == 0) {
                                texturePage = TMD_GT3_ENV_FIRST_TEXTURE_PAGE;
                            } else {
                                // Use the pair byte view: even the final cursor increments stay in bounds.
                                pageMarker  = (u8*)packetPair + OFFSET_OF(POLY_GT3, code);
                                cornerIndex = 0;
                                textureU    = (u8*)packetPair + OFFSET_OF(POLY_GT3, u0);
                                do {
                                    if (*pageMarker == 0) {
                                        if ((s8)*textureU < 0) {
                                            *textureU -= TMD_GT3_ENV_PAGE_U_DISPLACEMENT;
                                        } else {
                                            *textureU = 0;
                                        }
                                    }
                                    textureU += TMD_GT3_ENV_CORNER_STRIDE_BYTES;
                                    cornerIndex++;
                                    pageMarker += TMD_GT3_ENV_CORNER_STRIDE_BYTES;
                                } while (cornerIndex < TMD_GT3_ENV_CORNER_COUNT);
                                texturePage = TMD_GT3_ENV_SECOND_TEXTURE_PAGE;
                            }
                            (*packetPair)[0].tpage = texturePage;
                            // Stamp both commands; head insertion puts the base before its environment layer.
                            setlen(&(*packetPair)[0], packetWordCount);
                            setcode(&(*packetPair)[0], TMD_GT3_ENV_LAYER_COMMAND);
                            setlen(&(*packetPair)[1], packetWordCount);
                            setcode(&(*packetPair)[1], baseCommand);
                            gte_stotz(gteResultDestination);
                            addPrim(&workspace->ot[(((u32)workspace->gteResult << displayState->otDepthShift) >>
                                                    TMD_GT3_ENV_OT_INDEX_SHIFT) &
                                                   (GPU_ORDERING_TABLE_DEPTH_BYTE_MASK / sizeof(*workspace->ot))],
                                    &(*packetPair)[0]);
                            addPrim(&workspace->ot[(((u32)workspace->gteResult << displayState->otDepthShift) >>
                                                    TMD_GT3_ENV_OT_INDEX_SHIFT) &
                                                   (GPU_ORDERING_TABLE_DEPTH_BYTE_MASK / sizeof(*workspace->ot))],
                                    &(*packetPair)[1]);
                        }
                    }
                }
            }
            // Culled elements consume both packet slots too.
            packetPair++;
            elements += workspace->elemStride;
        } while (workspace->elemCount-- > 0);
    }
    workspace->preXformWrite = (u8*)packetPair;
    return elements;
}

u32* tmdDrawStreamPrimGt4PreXformEnvLayer(TmdStreamWorkspace* workspace, s32 objectFlags, u32* elements)
{
    enum {
        TMD_GT4_ENV_DEPTH_BYTE_OFFSET_MASK = 0xFFFC, // Keep reference bits 15..2; discarded low bits have no proven role
        TMD_GT4_ENV_TEXTURE_DEPTH_DIRECT   = 2,      // getTPage selector: one 16-bit VRAM word per texel, no CLUT
        TMD_GT4_ENV_FIRST_TEXTURE_PAGE     = getTPage(TMD_GT4_ENV_TEXTURE_DEPTH_DIRECT, GPU_BLEND_ADD, 448, 256),
        TMD_GT4_ENV_SECOND_TEXTURE_PAGE    = getTPage(TMD_GT4_ENV_TEXTURE_DEPTH_DIRECT, GPU_BLEND_ADD, 576, 256),
        TMD_GT4_ENV_PAGE_U_DISPLACEMENT    = 128,  // Texels between the two overlapping environment-page origins
        TMD_GT4_ENV_BASE_COMMAND           = 0x3C, // Opaque Gouraud textured quad; vertex colours modulate the texture
        TMD_GT4_ENV_LAYER_COMMAND          = 0x3E, // Colour-modulated semitransparent quad; page selects additive blending
        TMD_GT4_ENV_CORNER_COUNT           = 4,
        TMD_GT4_ENV_PACKET_COUNT           = 2,    // Environment layer first, opaque model-texture base second
        TMD_GT4_ENV_CORNER_STRIDE_BYTES    = OFFSET_OF(POLY_GT4, u1) - OFFSET_OF(POLY_GT4, u0),
        TMD_GT4_ENV_OT_INDEX_SHIFT         = 4     // Scaled OTZ units to four-byte OT tags; following mask wraps to 1024 tags
    };
    /// One element's environment packet followed by its opaque base packet.
    typedef POLY_GT4 _TmdEnvQuadPair[TMD_GT4_ENV_PACKET_COUNT];

    STATIC_ASSERT(OFFSET_OF(POLY_GT4, u2) - OFFSET_OF(POLY_GT4, u1) == TMD_GT4_ENV_CORNER_STRIDE_BYTES &&
                      OFFSET_OF(POLY_GT4, u3) - OFFSET_OF(POLY_GT4, u2) == TMD_GT4_ENV_CORNER_STRIDE_BYTES &&
                      OFFSET_OF(POLY_GT4, p1) - OFFSET_OF(POLY_GT4, code) == TMD_GT4_ENV_CORNER_STRIDE_BYTES &&
                      OFFSET_OF(POLY_GT4, p2) - OFFSET_OF(POLY_GT4, p1) == TMD_GT4_ENV_CORNER_STRIDE_BYTES &&
                      OFFSET_OF(POLY_GT4, p3) - OFFSET_OF(POLY_GT4, p2) == TMD_GT4_ENV_CORNER_STRIDE_BYTES,
                  tmdGt4EnvCornerStride);
    STATIC_ASSERT(OFFSET_OF(POLY_GT4, code) + TMD_GT4_ENV_CORNER_COUNT * TMD_GT4_ENV_CORNER_STRIDE_BYTES <= sizeof(_TmdEnvQuadPair) &&
                      OFFSET_OF(POLY_GT4, u0) + TMD_GT4_ENV_CORNER_COUNT * TMD_GT4_ENV_CORNER_STRIDE_BYTES <= sizeof(_TmdEnvQuadPair),
                  tmdGt4EnvCornerCursorBounds);

    _TmdEnvQuadPair*    packetPair;
    s32*                gteResultDestination;
    u32                 invalidDepthMask;
    s32                 packetWordCount;
    s32                 baseCommand;
    const DisplayState* displayState;
    const u16*          depthRefs;
    s32                 depthOrPageMarkers;
    u32                 depthByteOffset;
    const s32*          vertexDepths;
    u8*                 pageMarker;
    u8*                 textureU;
    s32                 cornerIndex;
    s32                 texturePage;

    packetPair = (_TmdEnvQuadPair*)workspace->preXformWrite;
    if (workspace->elemCount-- > 0) {
        gteResultDestination = &workspace->gteResult;
        invalidDepthMask     = TMD_VERTEX_DEPTH_INVALID;
        packetWordCount      = (sizeof((*packetPair)[0]) - sizeof((*packetPair)[0].tag)) / sizeof(u32);
        baseCommand          = TMD_GT4_ENV_BASE_COMMAND;
        displayState         = &gDisplayState;
        do {
            depthRefs = (const u16*)elements;
            // Test halves 0/1/2 and 1/2/3 with their opposite accepted windings.
            _tmdStoreTexturedQuadFirstTriangleFacing(&(*packetPair)[0], gteResultDestination);
            gte_ldSXYP(GPU_PRIMITIVE_XY_WORD(&(*packetPair)[0], 3));
            gte_nclip();
            if (workspace->gteResult <= 0) {
                // Fold the first half away in both packets, even if the second is culled too.
                GPU_PRIMITIVE_XY_WORD(&(*packetPair)[0], 0) = GPU_PRIMITIVE_XY_WORD(&(*packetPair)[0], 1);
                GPU_PRIMITIVE_XY_WORD(&(*packetPair)[1], 0) = GPU_PRIMITIVE_XY_WORD(&(*packetPair)[1], 1);
                gte_stopz(gteResultDestination);
                if (workspace->gteResult >= 0) {
                    goto next;
                }
            } else {
                gte_stopz(gteResultDestination);
                if (workspace->gteResult >= 0) {
                    GPU_PRIMITIVE_XY_WORD(&(*packetPair)[0], 3) = GPU_PRIMITIVE_XY_WORD(&(*packetPair)[0], 2);
                    GPU_PRIMITIVE_XY_WORD(&(*packetPair)[1], 3) = GPU_PRIMITIVE_XY_WORD(&(*packetPair)[1], 2);
                }
            }
            // All four original depths contribute, including a folded-away corner.
            vertexDepths       = workspace->szTable;
            depthByteOffset    = depthRefs[0] & TMD_GT4_ENV_DEPTH_BYTE_OFFSET_MASK;
            depthOrPageMarkers = vertexDepths[depthByteOffset / sizeof(*vertexDepths)];
            if ((depthOrPageMarkers & invalidDepthMask) == 0) {
                gte_ldSZ0(depthOrPageMarkers);
                depthByteOffset    = depthRefs[1] & TMD_GT4_ENV_DEPTH_BYTE_OFFSET_MASK;
                depthOrPageMarkers = vertexDepths[depthByteOffset / sizeof(*vertexDepths)];
                if ((depthOrPageMarkers & invalidDepthMask) == 0) {
                    gte_ldSZ1(depthOrPageMarkers);
                    depthByteOffset    = depthRefs[2] & TMD_GT4_ENV_DEPTH_BYTE_OFFSET_MASK;
                    depthOrPageMarkers = vertexDepths[depthByteOffset / sizeof(*vertexDepths)];
                    if ((depthOrPageMarkers & invalidDepthMask) == 0) {
                        gte_ldSZ2(depthOrPageMarkers);
                        depthByteOffset    = depthRefs[3] & TMD_GT4_ENV_DEPTH_BYTE_OFFSET_MASK;
                        depthOrPageMarkers = vertexDepths[depthByteOffset / sizeof(*vertexDepths)];
                        if ((depthOrPageMarkers & invalidDepthMask) == 0) {
                            gte_ldSZ3(depthOrPageMarkers);
                            gte_avsz4();
                            // Projection leaves second-page markers in the colour groups' high bytes.
                            depthOrPageMarkers = (*packetPair)[0].code | (*packetPair)[0].p1 | (*packetPair)[0].p2 | (*packetPair)[0].p3;
                            if (depthOrPageMarkers == 0) {
                                texturePage = TMD_GT4_ENV_FIRST_TEXTURE_PAGE;
                            } else {
                                // The complete pair byte view also contains the final cursor advances.
                                pageMarker  = (u8*)packetPair + OFFSET_OF(POLY_GT4, code);
                                cornerIndex = 0;
                                textureU    = (u8*)packetPair + OFFSET_OF(POLY_GT4, u0);
                                do {
                                    if (*pageMarker == 0) {
                                        if ((s8)*textureU < 0) {
                                            *textureU -= TMD_GT4_ENV_PAGE_U_DISPLACEMENT;
                                        } else {
                                            *textureU = 0;
                                        }
                                    }
                                    textureU += TMD_GT4_ENV_CORNER_STRIDE_BYTES;
                                    cornerIndex++;
                                    pageMarker += TMD_GT4_ENV_CORNER_STRIDE_BYTES;
                                } while (cornerIndex < TMD_GT4_ENV_CORNER_COUNT);
                                texturePage = TMD_GT4_ENV_SECOND_TEXTURE_PAGE;
                            }
                            (*packetPair)[0].tpage = texturePage;
                            setlen(&(*packetPair)[0], packetWordCount);
                            setcode(&(*packetPair)[0], TMD_GT4_ENV_LAYER_COMMAND);
                            gte_stotz(gteResultDestination);
                            setlen(&(*packetPair)[1], packetWordCount);
                            setcode(&(*packetPair)[1], baseCommand);
                            // Head insertion draws the opaque base before its additive environment layer.
                            addPrim(&workspace->ot[((u32)workspace->gteResult << displayState->otDepthShift) >> TMD_GT4_ENV_OT_INDEX_SHIFT &
                                                   (GPU_ORDERING_TABLE_DEPTH_BYTE_MASK / sizeof(*workspace->ot))],
                                    &(*packetPair)[0]);
                            addPrim(&workspace->ot[((u32)workspace->gteResult << displayState->otDepthShift) >> TMD_GT4_ENV_OT_INDEX_SHIFT &
                                                   (GPU_ORDERING_TABLE_DEPTH_BYTE_MASK / sizeof(*workspace->ot))],
                                    &(*packetPair)[1]);
                        }
                    }
                }
            }
        next:
            // Culled elements consume both packet slots too.
            packetPair++;
            elements += workspace->elemStride;
        } while (workspace->elemCount-- > 0);
    }
    workspace->preXformWrite = (u8*)packetPair;
    return elements;
}

u32* tmdDrawStreamPrimGt3PreXformOffsetLayer(TmdStreamWorkspace* workspace, s32 objectFlags, u32* elements)
{
    enum {
        /// Extracts an aligned depth-cache byte offset from an offset-layer triangle's corner reference.
        ///
        /// Applied to each of the element's first three u16 values: retain
        /// bits 15..2 and discard bits 1..0, whose role is unproven. Results
        /// span 0..65532 bytes and are divided by `sizeof(*vertexDepths)` to
        /// index four-byte `szTable` entries. The draw walk supplies 1024
        /// entries: valid byte offsets are 0..4092, and each selected depth
        /// must be initialized by an earlier projection in that walk.
        /// This mask checks neither
        /// bounds nor validity; `TMD_VERTEX_DEPTH_INVALID` is tested in the
        /// selected cache entry, independently of the reference's low bits.
        TMD_GT3_OFFSET_LAYER_DEPTH_BYTE_OFFSET_MASK = 0xFFFC,
        /// GPU command byte for the opaque, colour-modulated Gouraud-textured base triangle.
        ///
        /// Bits 1 and 0 are clear: semitransparency is disabled and the existing
        /// per-corner colours modulate the texture. Written to the second
        /// `POLY_GT3` in each pair, using the model's base page/CLUT offsets.
        /// Ordering-table head insertion draws it before the blended layer.
        /// The command is fixed, independent of `objectFlags`.
        TMD_GT3_OFFSET_LAYER_BASE_COMMAND = 0x34,
        /// GPU command byte for the colour-modulated, semitransparent offset-layer triangle.
        ///
        /// Bit 1 enables semitransparency; bit 0 stays clear so the existing lit
        /// vertex colours modulate the texture. The prebuilt packet's `tpage`
        /// supplies the blend mode. Written to the first `POLY_GT3` in each
        /// offset-layer/base pair; ordering-table head insertion draws it after
        /// the opaque base. The command is fixed, independent of `objectFlags`.
        TMD_GT3_OFFSET_LAYER_SEMI_TRANS_COMMAND = 0x36,
        /// Number of `POLY_GT3` packet slots consumed by one offset-layer triangle element.
        ///
        /// Slot 0 holds the semitransparent offset-textured layer; slot 1 holds
        /// the opaque model-textured base. Defines the pair's storage stride,
        /// including both DMA tags; culled elements still consume both slots.
        TMD_GT3_OFFSET_LAYER_PACKETS_PER_ELEMENT = 2,
        /// Right shift converting an offset-layer triangle's scaled GTE OTZ to an OT tag index.
        ///
        /// Applied after the u32 left shift by `gDisplayState.otDepthShift`
        /// (0..3): sixteen scaled depth units select one four-byte tag.
        /// Combines the depth-to-byte-offset right shift by two with the
        /// bytes-to-tags conversion. The following mask is
        /// `GPU_ORDERING_TABLE_DEPTH_BYTE_MASK / sizeof(*workspace->ot)`;
        /// it keeps scaled-depth bits 4..13, wrapping to indices 0..1023.
        /// The unsigned cast preserves unsigned shift arithmetic; the result
        /// counts tags, not bytes, and does not clamp depth. Both packets use
        /// this index relative to `workspace->ot`, already displaced by the
        /// model's signed tag offset. The selected table must contain that
        /// entry; neither the shift nor the mask checks its storage bounds.
        TMD_GT3_OFFSET_LAYER_OT_INDEX_SHIFT = 4
    };
    /// One element's offset-textured layer followed by its opaque base.
    typedef POLY_GT3 _TmdOffsetLayerTrianglePair[TMD_GT3_OFFSET_LAYER_PACKETS_PER_ELEMENT];

    _TmdOffsetLayerTrianglePair* packetPair;
    s32*                         gteResultDestination;
    u32                          invalidDepthMask;
    s32                          packetWordCount;
    s32                          baseCommand;
    const DisplayState*          displayState;
    const u16*                   depthRefs;
    s32                          cachedDepth;
    u32                          depthByteOffset;
    const s32*                   vertexDepths;

    packetPair = (_TmdOffsetLayerTrianglePair*)workspace->preXformWrite;
    if (workspace->elemCount-- > 0) {
        gteResultDestination = &workspace->gteResult;
        invalidDepthMask     = TMD_VERTEX_DEPTH_INVALID;
        packetWordCount      = (sizeof((*packetPair)[0]) - sizeof((*packetPair)[0].tag)) / sizeof(u32);
        baseCommand          = TMD_GT3_OFFSET_LAYER_BASE_COMMAND;
        displayState         = &gDisplayState;
        do {
            depthRefs = (const u16*)elements;
            // Cull both packets from the layer's projected winding.
            _tmdStoreTexturedTriangleFacing(&(*packetPair)[0], gteResultDestination);
            if (workspace->gteResult > 0) {
                // Only initialized, valid cached depths may contribute to AVSZ3.
                vertexDepths    = workspace->szTable;
                depthByteOffset = depthRefs[0] & TMD_GT3_OFFSET_LAYER_DEPTH_BYTE_OFFSET_MASK;
                cachedDepth     = vertexDepths[depthByteOffset / sizeof(*vertexDepths)];
                if ((cachedDepth & invalidDepthMask) == 0) {
                    gte_ldSZ1(cachedDepth);
                    depthByteOffset = depthRefs[1] & TMD_GT3_OFFSET_LAYER_DEPTH_BYTE_OFFSET_MASK;
                    cachedDepth     = vertexDepths[depthByteOffset / sizeof(*vertexDepths)];
                    if ((cachedDepth & invalidDepthMask) == 0) {
                        gte_ldSZ2(cachedDepth);
                        depthByteOffset = depthRefs[2] & TMD_GT3_OFFSET_LAYER_DEPTH_BYTE_OFFSET_MASK;
                        cachedDepth     = vertexDepths[depthByteOffset / sizeof(*vertexDepths)];
                        if ((cachedDepth & invalidDepthMask) == 0) {
                            gte_ldSZ3(cachedDepth);
                            gte_avsz3();
                            setlen(&(*packetPair)[0], packetWordCount);
                            setcode(&(*packetPair)[0], TMD_GT3_OFFSET_LAYER_SEMI_TRANS_COMMAND);
                            gte_stotz(gteResultDestination);
                            setlen(&(*packetPair)[1], packetWordCount);
                            setcode(&(*packetPair)[1], baseCommand);
                            gte_stotz(gteResultDestination);
                            // Head insertion makes the opaque base draw before the blended layer.
                            addPrim(&workspace->ot[((u32)workspace->gteResult << displayState->otDepthShift) >> TMD_GT3_OFFSET_LAYER_OT_INDEX_SHIFT &
                                                   (GPU_ORDERING_TABLE_DEPTH_BYTE_MASK / sizeof(*workspace->ot))],
                                    &(*packetPair)[0]);
                            addPrim(&workspace->ot[((u32)workspace->gteResult << displayState->otDepthShift) >> TMD_GT3_OFFSET_LAYER_OT_INDEX_SHIFT &
                                                   (GPU_ORDERING_TABLE_DEPTH_BYTE_MASK / sizeof(*workspace->ot))],
                                    &(*packetPair)[1]);
                        }
                    }
                }
            }
            packetPair++;
            elements += workspace->elemStride;
        } while (workspace->elemCount-- > 0);
    }
    workspace->preXformWrite = (u8*)packetPair;
    return elements;
}

u32* tmdDrawStreamPrimGt4PreXformOffsetLayer(TmdStreamWorkspace* workspace, s32 objectFlags, u32* elements)
{
    enum {
        /// Extracts an aligned depth-cache byte offset from an offset-layer quad's corner reference.
        ///
        /// Applied to each of the element's first four u16 values: retain
        /// bits 15..2 and discard bits 1..0, whose role is unproven. Results
        /// span 0..65532 bytes and are divided by `sizeof(*vertexDepths)` to
        /// index four-byte `szTable` entries. The draw walk supplies 1024
        /// entries: valid byte offsets are 0..4092, and each selected depth
        /// must be initialized by an earlier projection in that walk.
        /// This mask checks neither bounds nor validity; `TMD_VERTEX_DEPTH_INVALID`
        /// is tested in the selected cache entry, independently of the reference's
        /// low bits.
        TMD_GT4_OFFSET_LAYER_DEPTH_BYTE_OFFSET_MASK = 0xFFFC,
        /// GPU command byte for the opaque, colour-modulated Gouraud-textured base quad.
        ///
        /// Bits 1 and 0 are clear: semitransparency is disabled and the existing
        /// per-corner colours modulate the texture. Written to the second
        /// `POLY_GT4` in each pair, using the model's base page/CLUT offsets.
        /// Ordering-table head insertion draws it before the blended layer.
        /// The command is fixed, independent of `objectFlags`.
        TMD_GT4_OFFSET_LAYER_BASE_COMMAND = 0x3C,
        /// GPU command byte for the colour-modulated, semitransparent offset-layer quad.
        ///
        /// Bit 1 enables semitransparency; bit 0 stays clear so the existing lit
        /// vertex colours modulate the texture. The prebuilt packet's `tpage`
        /// supplies the blend mode. Written to the first `POLY_GT4` in each
        /// offset-layer/base pair; ordering-table head insertion draws it after
        /// the opaque base. The command is fixed, independent of `objectFlags`.
        TMD_GT4_OFFSET_LAYER_SEMI_TRANS_COMMAND = 0x3E,
        /// Number of `POLY_GT4` packet slots consumed by one offset-layer quad element.
        ///
        /// Slot 0 holds the semitransparent offset-textured layer; slot 1 holds
        /// the opaque model-textured base. Defines the pair's storage stride,
        /// including both DMA tags; culled elements still consume both slots.
        TMD_GT4_OFFSET_LAYER_PACKETS_PER_ELEMENT = 2,
        /// Right shift converting an offset-layer quad's scaled GTE OTZ to an OT tag index.
        ///
        /// Applied after the u32 left shift by `gDisplayState.otDepthShift`
        /// (0..3): sixteen scaled depth units select one four-byte tag.
        /// Combines the depth-to-byte-offset right shift by two with the
        /// bytes-to-tags conversion. The following mask is
        /// `GPU_ORDERING_TABLE_DEPTH_BYTE_MASK / sizeof(*workspace->ot)`;
        /// it keeps scaled-depth bits 4..13, wrapping to indices 0..1023.
        /// The unsigned cast preserves unsigned shift arithmetic; the result
        /// counts tags, not bytes, and does not clamp depth. Both packets use
        /// this index relative to `workspace->ot`, already displaced by the
        /// model's signed tag offset. The selected table must contain that
        /// entry; neither the shift nor the mask checks its storage bounds.
        TMD_GT4_OFFSET_LAYER_OT_INDEX_SHIFT = 4
    };
    /// One element's offset-textured layer followed by its opaque base.
    typedef POLY_GT4 _TmdOffsetLayerQuadPair[TMD_GT4_OFFSET_LAYER_PACKETS_PER_ELEMENT];

    _TmdOffsetLayerQuadPair* packetPair;
    s32*                     gteResultDestination;
    u32                      invalidDepthMask;
    s32                      packetWordCount;
    s32                      baseCommand;
    const DisplayState*      displayState;
    const u16*               depthRefs;
    s32                      cachedDepth;
    u32                      depthByteOffset;
    const s32*               vertexDepths;

    packetPair = (_TmdOffsetLayerQuadPair*)workspace->preXformWrite;
    if (workspace->elemCount-- > 0) {
        gteResultDestination = &workspace->gteResult;
        invalidDepthMask     = TMD_VERTEX_DEPTH_INVALID;
        packetWordCount      = (sizeof((*packetPair)[0]) - sizeof((*packetPair)[0].tag)) / sizeof(u32);
        baseCommand          = TMD_GT4_OFFSET_LAYER_BASE_COMMAND;
        displayState         = &gDisplayState;
        do {
            depthRefs = (const u16*)elements;
            // Accept a positive first half immediately; otherwise require a negative second half.
            _tmdStoreTexturedQuadFirstTriangleFacing(&(*packetPair)[0], gteResultDestination);
            if (workspace->gteResult > 0) {
                goto draw;
            }
            gte_ldSXYP(GPU_PRIMITIVE_XY_WORD(&(*packetPair)[0], 3));
            gte_nclip();
            gte_stopz(gteResultDestination);
            if (workspace->gteResult < 0) {
            draw:
                // Only initialized, valid cached depths may contribute to AVSZ4.
                vertexDepths    = workspace->szTable;
                depthByteOffset = depthRefs[0] & TMD_GT4_OFFSET_LAYER_DEPTH_BYTE_OFFSET_MASK;
                cachedDepth     = vertexDepths[depthByteOffset / sizeof(*vertexDepths)];
                if ((cachedDepth & invalidDepthMask) == 0) {
                    gte_ldSZ0(cachedDepth);
                    depthByteOffset = depthRefs[1] & TMD_GT4_OFFSET_LAYER_DEPTH_BYTE_OFFSET_MASK;
                    cachedDepth     = vertexDepths[depthByteOffset / sizeof(*vertexDepths)];
                    if ((cachedDepth & invalidDepthMask) == 0) {
                        gte_ldSZ1(cachedDepth);
                        depthByteOffset = depthRefs[2] & TMD_GT4_OFFSET_LAYER_DEPTH_BYTE_OFFSET_MASK;
                        cachedDepth     = vertexDepths[depthByteOffset / sizeof(*vertexDepths)];
                        if ((cachedDepth & invalidDepthMask) == 0) {
                            gte_ldSZ2(cachedDepth);
                            depthByteOffset = depthRefs[3] & TMD_GT4_OFFSET_LAYER_DEPTH_BYTE_OFFSET_MASK;
                            cachedDepth     = vertexDepths[depthByteOffset / sizeof(*vertexDepths)];
                            if ((cachedDepth & invalidDepthMask) == 0) {
                                gte_ldSZ3(cachedDepth);
                                gte_avsz4();
                                setlen(&(*packetPair)[0], packetWordCount);
                                setcode(&(*packetPair)[0], TMD_GT4_OFFSET_LAYER_SEMI_TRANS_COMMAND);
                                gte_stotz(gteResultDestination);
                                setlen(&(*packetPair)[1], packetWordCount);
                                setcode(&(*packetPair)[1], baseCommand);
                                gte_stotz(gteResultDestination);
                                // Head insertion makes the opaque base draw before the blended layer.
                                addPrim(&workspace->ot[((u32)workspace->gteResult << displayState->otDepthShift) >>
                                                           TMD_GT4_OFFSET_LAYER_OT_INDEX_SHIFT &
                                                       (GPU_ORDERING_TABLE_DEPTH_BYTE_MASK / sizeof(*workspace->ot))],
                                        &(*packetPair)[0]);
                                addPrim(&workspace->ot[((u32)workspace->gteResult << displayState->otDepthShift) >>
                                                           TMD_GT4_OFFSET_LAYER_OT_INDEX_SHIFT &
                                                       (GPU_ORDERING_TABLE_DEPTH_BYTE_MASK / sizeof(*workspace->ot))],
                                        &(*packetPair)[1]);
                            }
                        }
                    }
                }
            }
            packetPair++;
            elements += workspace->elemStride;
        } while (workspace->elemCount-- > 0);
    }
    workspace->preXformWrite = (u8*)packetPair;
    return elements;
}

const CVECTOR gGpColorGrey   = { 0x80, 0x80, 0x80, 0 };
const CVECTOR Gp_ColorOrange = { 0xFF, 0xA0, 0x60, 0 };
const CVECTOR gGpColorWhite  = { 0xFF, 0xFF, 0xFF, 0 };

/// Projects and lights paired grey corners with reduced environment UVs on the layer.
///
/// Reads a four-halfword prefix per element at `elemStride` u32-word intervals;
/// initial `elemCount` is 0..65535. `objectFlags` is ignored. Geometry references
/// mask low three bits and must address complete SVECTORs; vertexRef >> 3 must
/// fit the 1024-entry depth cache. Layer/base destinations are word-aligned
/// colour-group byte offsets in the first packet region, requiring respectively
/// ten and eight bytes. No full-packet or full-element extent is implied.
///
/// GTE projection, light, colour and rotation state must be set. Each corner
/// receives the same XY and independently lit neutral-grey material RGB/code.
/// Only the layer receives U/V: (screen >> 4) + 32 + (rotated normal >> 8),
/// preserving signed halfword intermediates and truncating the final bytes.
/// Adjacent equal vertex references reuse projection; failures publish FLAG
/// and invalid cached depth but still write both destinations. No OT linking.
///
/// Returns the advanced word cursor and consumes positive counts to -1; zero
/// leaves the count unchanged. Packet cursors stay fixed. Workspace scratch and
/// GTE state change; storage is borrowed for the call and no pointer is retained.
/// No current stream resolver selects this retained private routine.
static u32* _tmdXformStreamVertsGreyEnvLayerReduced(TmdStreamWorkspace* workspace, s32 objectFlags, u32* elements)
{
    s32                           previousVertexRef;
    s32                           initialElementCount;
    u32                           vertexRef;
    const _TmdEnvLayerCornerRefs* cornerRefs;
    CVECTOR                       baseMaterial;
    CVECTOR                       layerMaterial;
    u8*                           packetDestination;

    baseMaterial        = gGpColorGrey;
    layerMaterial       = gGpColorGrey;
    initialElementCount = workspace->elemCount;
    if (initialElementCount == 0) {
        return elements;
    }
    previousVertexRef    = TMD_ENV_MAP_NO_PREVIOUS_VERTEX;
    workspace->elemCount = initialElementCount + previousVertexRef;
    if (initialElementCount > 0) {
        do {
            cornerRefs = (const _TmdEnvLayerCornerRefs*)elements;
            vertexRef  = cornerRefs->vertexByteRef;
            // Reuse projection only for adjacent identical vertex references.
            if (vertexRef != previousVertexRef) {
                gte_ldv0((const u8*)workspace->verts + (vertexRef & TMD_ENV_MAP_GEOMETRY_BYTE_OFFSET_MASK));
                gte_rtps();
                gte_stsz(&workspace->gteResult);
                gte_stflg(&workspace->gteFlag);
                if (workspace->gteFlag & TMD_GTE_ERROR_FLAG) {
                    workspace->gteResult |= TMD_VERTEX_DEPTH_INVALID;
                }
                workspace->szTable[cornerRefs->vertexByteRef >> TMD_ENV_MAP_VERTEX_INDEX_SHIFT] = workspace->gteResult;
            }
            previousVertexRef = cornerRefs->vertexByteRef;
            packetDestination = workspace->preXformWrite + cornerRefs->layerColorByteOffset + (OFFSET_OF(POLY_GT3, x0) - OFFSET_OF(POLY_GT3, r0));
            gte_stsxy(packetDestination);
            packetDestination = workspace->preXformWrite + cornerRefs->baseColorByteOffset + (OFFSET_OF(POLY_GT3, x0) - OFFSET_OF(POLY_GT3, r0));
            gte_stsxy(packetDestination);
            gte_stsxy(&workspace->texCoord);
            gte_ldv0((const u8*)workspace->normals + (cornerRefs->normalByteRef & TMD_ENV_MAP_GEOMETRY_BYTE_OFFSET_MASK));
            gte_ldrgb(&layerMaterial);
            gte_nccs();
            gte_strgb(workspace->preXformWrite + cornerRefs->layerColorByteOffset);
            gte_ldrgb(&baseMaterial);
            gte_nccs();
            gte_strgb(workspace->preXformWrite + cornerRefs->baseColorByteOffset);
            gte_rtv0();
            // Preserve the signed halfword intermediates before truncating layer U/V.
            workspace->texCoord.vx = (workspace->texCoord.vx >> TMD_ENV_MAP_REDUCED_SCREEN_SHIFT) + TMD_ENV_MAP_REDUCED_CENTER;
            workspace->texCoord.vy = (workspace->texCoord.vy >> TMD_ENV_MAP_REDUCED_SCREEN_SHIFT) + TMD_ENV_MAP_REDUCED_CENTER;
            gte_stsv(&workspace->elemNormal);
            workspace->texCoord.vx                                              += workspace->elemNormal.vx >> TMD_ENV_MAP_REDUCED_NORMAL_SHIFT;
            workspace->texCoord.vy                                              += workspace->elemNormal.vy >> TMD_ENV_MAP_REDUCED_NORMAL_SHIFT;
            packetDestination                                                    = workspace->preXformWrite + cornerRefs->layerColorByteOffset;
            packetDestination[OFFSET_OF(POLY_GT3, u0) - OFFSET_OF(POLY_GT3, r0)] = (u8)workspace->texCoord.vx;
            packetDestination                                                    = workspace->preXformWrite + cornerRefs->layerColorByteOffset;
            packetDestination[OFFSET_OF(POLY_GT3, v0) - OFFSET_OF(POLY_GT3, r0)] = (u8)workspace->texCoord.vy;
            elements                                                            += workspace->elemStride;
        } while (workspace->elemCount-- > 0);
    }
    return elements;
}

static u32* func_8009AA5C(TmdStreamWorkspace* ws, s32 arg1, u32* arg2)
{
    CVECTOR col;

    col = Gp_ColorOrange;
    gte_ldrgb(&col);
    return _tmdXformStreamVertsEnvMapLitReduced(ws, arg2);
}

/// Projects and lights corners with a blend-scaled, full-screen environment map.
///
/// The four-halfword prefix and word-stride/count, geometry, depth-cache and
/// borrowed-storage requirements are `_tmdXformStreamVertsEnvMapLitReduced`'s.
/// `objectFlags` is ignored. XY and RGB offsets remain independent; this path
/// also writes the byte immediately preceding XY as a temporary 0/1 page marker.
/// Require that byte plus XY/U/V to fit the first packet region. GTE projection,
/// light, colour and rotation state must be initialized; material is white.
///
/// Below 4096, lit RGB blends toward neutral grey with the object's
/// 12-fraction-bit colorBlend. Scales the rotated normal with GPF12 using
/// colorBlend >> 9, then subtracts it from signed screen XY + (160,120).
/// Negative U/V clamp to zero; U >= 256 rebases by 128 and clamps to 191,
/// setting the second-page marker, while V clamps to 239. Does not link an OT.
///
/// Adjacent equal references reuse projection; new failures publish FLAG and
/// invalid cached depth but still write XY/RGB/UV. Returns the advanced word
/// cursor, consumes positive counts to -1, and leaves packet cursors fixed.
/// Zero count is unchanged but material is loaded first. Workspace scratch and
/// GTE state change; no borrowed pointer is retained. No current stream resolver
/// selects this retained private routine.
static u32* _tmdXformStreamVertsEnvMapLit(TmdStreamWorkspace* workspace, s32 objectFlags, u32* elements)
{
    enum {
        TMD_ENV_MAP_SCREEN_CENTER_X        = 160,
        TMD_ENV_MAP_SCREEN_CENTER_Y        = 120,
        TMD_ENV_MAP_NORMAL_BLEND_SHIFT     = 9,
        TMD_ENV_MAP_FIRST_PAGE_U_LIMIT     = 256,
        TMD_ENV_MAP_PAGE_U_DISPLACEMENT    = 128,
        TMD_ENV_MAP_SECOND_PAGE_U_LIMIT    = 192,
        TMD_ENV_MAP_V_LIMIT                = 240,
        TMD_ENV_MAP_FIRST_PAGE_MARKER      = 0,
        TMD_ENV_MAP_SECOND_PAGE_MARKER     = 1,
        TMD_ENV_MAP_XY_TO_U_BYTES          = OFFSET_OF(POLY_GT3, u0) - OFFSET_OF(POLY_GT3, x0),
        TMD_ENV_MAP_V_TO_PAGE_MARKER_BYTES = OFFSET_OF(POLY_GT3, v0) - OFFSET_OF(POLY_GT3, code)
    };
    /// Blends one aligned RGB group toward neutral grey; caller gates blend < 4096.
    ///
    /// Arguments must be side-effect-free workspace/record/reference pointers and
    /// a writable cursor lvalue. The cursor is assigned after the first blend
    /// load; the object's 12-fraction-bit weight is read separately for each
    /// term. Evaluates workspace, refs and cursor repeatedly, captures no locals,
    /// clobbers GTE interpolation state and writes RGB. This multi-statement
    /// expansion requires an enclosing compound block.
#define TMD_BLEND_ENV_MAP_COLOR_TO_GREY(workspace, refs, cursor, reference)      \
    gte_lddp((workspace)->obj->shading.colorBlend);                              \
    (cursor) = (workspace)->preXformWrite + (refs)->rgbByteOffset;               \
    gte_ldcv(cursor);                                                            \
    gte_gpf12();                                                                 \
    gte_lddp(TMD_OBJECT_COLOR_BLEND_ONE - (workspace)->obj->shading.colorBlend); \
    gte_ldcv(reference);                                                         \
    gte_gpl12();                                                                 \
    gte_stcv(cursor)

    s32                         previousVertexRef;
    s32                         initialElementCount;
    u32                         vertexRef;
    const _TmdEnvMapCornerRefs* cornerRefs;
    CVECTOR                     materialColor;
    CVECTOR                     referenceColor;
    u8*                         packetDestination;
    u8*                         colorDestination;
    s16*                        screenComponent;
    SVECTOR*                    rotatedNormal;
    s32                         secondPage;
    s32                         textureComponent;

    materialColor  = gGpColorWhite;
    referenceColor = gGpColorGrey;
    gte_ldrgb(&materialColor);
    initialElementCount = workspace->elemCount;
    if (initialElementCount == 0) {
        return elements;
    }
    previousVertexRef    = TMD_ENV_MAP_NO_PREVIOUS_VERTEX;
    workspace->elemCount = initialElementCount + previousVertexRef;
    if (initialElementCount > 0) {
        do {
            cornerRefs = (const _TmdEnvMapCornerRefs*)elements;
            vertexRef  = cornerRefs->vertexByteRef;
            // Reuse projection only for adjacent identical vertex references.
            if (vertexRef != previousVertexRef) {
                gte_ldv0((const u8*)workspace->verts + (vertexRef & TMD_ENV_MAP_GEOMETRY_BYTE_OFFSET_MASK));
                gte_rtps();
                gte_stsz(&workspace->gteResult);
                gte_stflg(&workspace->gteFlag);
                if (workspace->gteFlag & TMD_GTE_ERROR_FLAG) {
                    workspace->gteResult |= TMD_VERTEX_DEPTH_INVALID;
                }
                workspace->szTable[cornerRefs->vertexByteRef >> TMD_ENV_MAP_VERTEX_INDEX_SHIFT] = workspace->gteResult;
            }
            previousVertexRef = cornerRefs->vertexByteRef;
            gte_stsxy(workspace->preXformWrite + cornerRefs->xyByteOffset);
            gte_stsxy(&workspace->texCoord);
            gte_ldv0((const u8*)workspace->normals + (cornerRefs->normalByteRef & TMD_ENV_MAP_GEOMETRY_BYTE_OFFSET_MASK));
            gte_nccs();
            gte_rtv0();
            gte_stsv(&workspace->elemNormal);
            elements += workspace->elemStride;
            gte_strgb(workspace->preXformWrite + cornerRefs->rgbByteOffset);
            // Fade lit colour toward the grey reference before deriving texture displacement.
            if (workspace->obj->shading.colorBlend < TMD_OBJECT_COLOR_BLEND_ONE) {
                TMD_BLEND_ENV_MAP_COLOR_TO_GREY(workspace, cornerRefs, colorDestination, &referenceColor);
            }
            // Store U/V and the page marker separately; keep the cursor increments in order.
            screenComponent   = &workspace->texCoord.vx;
            secondPage        = TMD_ENV_MAP_FIRST_PAGE_MARKER;
            packetDestination = workspace->preXformWrite + cornerRefs->xyByteOffset + TMD_ENV_MAP_XY_TO_U_BYTES;
            rotatedNormal     = &workspace->elemNormal;
            gte_lddp(workspace->obj->shading.colorBlend >> TMD_ENV_MAP_NORMAL_BLEND_SHIFT);
            gte_ldsv(rotatedNormal);
            gte_gpf12();
            gte_stsv(rotatedNormal);
            textureComponent  = *screenComponent + TMD_ENV_MAP_SCREEN_CENTER_X;
            textureComponent -= rotatedNormal->vx;
            if (textureComponent < 0) {
                textureComponent = secondPage;
            } else if (textureComponent >= TMD_ENV_MAP_FIRST_PAGE_U_LIMIT) {
                textureComponent -= TMD_ENV_MAP_PAGE_U_DISPLACEMENT;
                secondPage        = TMD_ENV_MAP_SECOND_PAGE_MARKER;
                if (textureComponent >= TMD_ENV_MAP_SECOND_PAGE_U_LIMIT) {
                    textureComponent = TMD_ENV_MAP_SECOND_PAGE_U_LIMIT - 1;
                }
            }
            *packetDestination = textureComponent;
            textureComponent   = *++screenComponent + TMD_ENV_MAP_SCREEN_CENTER_Y;
            textureComponent  -= rotatedNormal->vy;
            packetDestination++;
            if (textureComponent < 0) {
                textureComponent = 0;
            } else if (textureComponent >= TMD_ENV_MAP_V_LIMIT) {
                textureComponent = TMD_ENV_MAP_V_LIMIT - 1;
            }
            *packetDestination                                     = textureComponent;
            packetDestination[-TMD_ENV_MAP_V_TO_PAGE_MARKER_BYTES] = secondPage;
        } while (workspace->elemCount-- > 0);
    }
    return elements;
#undef TMD_BLEND_ENV_MAP_COLOR_TO_GREY
}

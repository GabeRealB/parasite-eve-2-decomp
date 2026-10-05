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

static inline u32* _gpPreXformEnvMapLit(TmdStreamWorkspace* ws, u32* arg2);

static __inline__ void _actorRenderRefreshCoord(GfxCoord* coord, s32 stamp, s32 parity, GfxCoord* root);

static u32* func_8009A804(TmdStreamWorkspace* ws, s32 arg1, u32* arg2);

static u32* func_8009AA5C(TmdStreamWorkspace* ws, s32 arg1, u32* arg2);

static u32* func_8009AC58(TmdStreamWorkspace* ws, s32 arg1, u32* arg2);

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

static inline u32* _gpPreXformEnvMapLit(TmdStreamWorkspace* ws, u32* arg2)
{
    s32  prev;
    s32  count;
    u32  idx;
    u16* rec;
    u8*  dest;

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
            dest = ws->preXformWrite + rec[2];
            gte_stsxy(dest);
            gte_stsxy(&ws->texCoord);
            gte_ldv0((u8*)ws->normals + (rec[1] & 0xFFF8));
            gte_nccs();
            gte_rtv0();
            ws->texCoord.vx = (ws->texCoord.vx >> 4) + 0x20;
            ws->texCoord.vy = (ws->texCoord.vy >> 4) + 0x20;
            gte_stsv(&ws->elemNormal);
            ws->texCoord.vx += ws->elemNormal.vx >> 8;
            ws->texCoord.vy += ws->elemNormal.vy >> 8;
            dest             = ws->preXformWrite + rec[2];
            dest[4]          = (u8)ws->texCoord.vx;
            dest             = ws->preXformWrite + rec[2];
            dest[5]          = (u8)ws->texCoord.vy;
            arg2            += ws->elemStride;
            gte_strgb(ws->preXformWrite + rec[3]);
        } while (ws->elemCount-- > 0);
    }
    return arg2;
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

TmdObject* Gp_AttachTmd(Task* task, TmdSource* src)
{
    TmdObject*   node;
    TmdListNode* last;
    TmdListNode* list;

    node = tmdCreateModel(src, 0);
    if (node != NULL) {
        list            = &gTmdList;
        last            = list->prev;
        node->link.next = last->next;
        last->next      = &node->link;
        node->link.prev = last;
        list->prev      = &node->link;
        task->extra.tmd = node;
        task->bodyKind  = TASK_BODY_TMD;
    }
    return node;
}

ModelObjectCoordBody* gpAttachDisp2d(Task* task)
{
    ModelObjectCoordBody* node;
    TmdListNode*          last;
    TmdListNode*          list;
    GfxCoord*             coord;

    node = memCalloc(sizeof(*node), 0);
    if (node != NULL) {
        coord         = &node->ownedCoord;
        node->coord   = coord;
        node->field_C = 1;
        coord->parent = &gGfxViewCoord;
        gfxSetRotIdentity(&coord->coord);
        coord->coord.t[2]     = 0;
        coord->coord.t[1]     = 0;
        coord->coord.t[0]     = 0;
        coord->param.rot.vz   = 0;
        coord->param.rot.vy   = 0;
        coord->param.rot.vx   = 0;
        coord->composeStamp   = GRAPHICS_COORD_DIRTY;
        list                  = &gModelObjectCoordBodyList;
        last                  = list->prev;
        node->link.next       = last->next;
        last->next            = &node->link;
        node->link.prev       = last;
        list->prev            = &node->link;
        task->extra.coordBody = node;
        task->bodyKind        = TASK_BODY_COORD;
    } else {
        printf("new_disp_2d ----> NULL\n");
    }
    return node;
}

TmdObject* Gp_AttachTmdFlags(Task* task, TmdSource* src, s32 flags)
{
    TmdObject*   node;
    TmdListNode* last;
    TmdListNode* list;

    node = tmdCreateModel(src, flags);
    if (node != NULL) {
        list            = &gTmdList;
        last            = list->prev;
        node->link.next = last->next;
        last->next      = &node->link;
        node->link.prev = last;
        list->prev      = &node->link;
        task->extra.tmd = node;
        task->bodyKind  = TASK_BODY_TMD;
    }
    return node;
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
    Task_CallExit(_gModelObjectTemporaryDrawTask);
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

    task = Task_GetActiveList()->next;
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

static u32* func_8009A804(TmdStreamWorkspace* ws, s32 arg1, u32* arg2)
{
    s32     prev;
    s32     count;
    u32     idx;
    u16*    rec;
    CVECTOR col;
    CVECTOR col2;
    u8*     dest;

    col   = gGpColorGrey;
    col2  = gGpColorGrey;
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
            dest = ws->preXformWrite + rec[2] + 4;
            gte_stsxy(dest);
            dest = ws->preXformWrite + rec[3] + 4;
            gte_stsxy(dest);
            gte_stsxy(&ws->texCoord);
            gte_ldv0((u8*)ws->normals + (rec[1] & 0xFFF8));
            gte_ldrgb(&col2);
            gte_nccs();
            gte_strgb(ws->preXformWrite + rec[2]);
            gte_ldrgb(&col);
            gte_nccs();
            gte_strgb(ws->preXformWrite + rec[3]);
            gte_rtv0();
            ws->texCoord.vx = (ws->texCoord.vx >> 4) + 0x20;
            ws->texCoord.vy = (ws->texCoord.vy >> 4) + 0x20;
            gte_stsv(&ws->elemNormal);
            ws->texCoord.vx += ws->elemNormal.vx >> 8;
            ws->texCoord.vy += ws->elemNormal.vy >> 8;
            dest             = ws->preXformWrite + rec[2];
            dest[8]          = (u8)ws->texCoord.vx;
            dest             = ws->preXformWrite + rec[2];
            dest[9]          = (u8)ws->texCoord.vy;
            arg2            += ws->elemStride;
        } while (ws->elemCount-- > 0);
    }
    return arg2;
}

static u32* func_8009AA5C(TmdStreamWorkspace* ws, s32 arg1, u32* arg2)
{
    CVECTOR col;

    col = Gp_ColorOrange;
    gte_ldrgb(&col);
    return _gpPreXformEnvMapLit(ws, arg2);
}

static u32* func_8009AC58(TmdStreamWorkspace* ws, s32 arg1, u32* arg2)
{
    s32      prev;
    s32      count;
    u32      idx;
    u16*     rec;
    CVECTOR  col;
    CVECTOR  col2;
    u8*      dest;
    u8*      cptr;
    s16*     xy;
    SVECTOR* sv;
    s32      page;
    s32      uv;

    col  = gGpColorWhite;
    col2 = gGpColorGrey;
    gte_ldrgb(&col);
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
            gte_stsxy(ws->preXformWrite + rec[2]);
            gte_stsxy(&ws->texCoord);
            gte_ldv0((u8*)ws->normals + (rec[1] & 0xFFF8));
            gte_nccs();
            gte_rtv0();
            gte_stsv(&ws->elemNormal);
            arg2 += ws->elemStride;
            gte_strgb(ws->preXformWrite + rec[3]);
            if (ws->obj->shading.colorBlend < TMD_OBJECT_COLOR_BLEND_ONE) {
                gte_lddp(ws->obj->shading.colorBlend);
                cptr = ws->preXformWrite + rec[3];
                gte_ldcv(cptr);
                gte_gpf12();
                gte_lddp(TMD_OBJECT_COLOR_BLEND_ONE - ws->obj->shading.colorBlend);
                gte_ldcv(&col2);
                gte_gpl12();
                gte_stcv(cptr);
            }
            xy   = &ws->texCoord.vx;
            page = 0;
            dest = ws->preXformWrite + rec[2] + 4;
            sv   = &ws->elemNormal;
            gte_lddp(ws->obj->shading.colorBlend >> 9);
            gte_ldsv(sv);
            gte_gpf12();
            gte_stsv(sv);
            uv  = *xy + 0xA0;
            uv -= sv->vx;
            if (uv < 0) {
                uv = page;
            } else if (uv >= 0x100) {
                uv  -= 0x80;
                page = 1;
                if (uv >= 0xC0) {
                    uv = 0xBF;
                }
            }
            *dest = uv;
            uv    = *++xy + 0x78;
            uv   -= sv->vy;
            dest++;
            if (uv < 0) {
                uv = 0;
            } else if (uv >= 0xF0) {
                uv = 0xEF;
            }
            *dest    = uv;
            dest[-6] = page;
        } while (ws->elemCount-- > 0);
    }
    return arg2;
}

#include "gameplay/model_objects.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/gtemac.h>
#include <psyq/inline_c.h>
#include <psyq/stdio.h>

#include "gte.h"
#include "types.h"

#include "gameplay/actor_render.h"
#include "actor_render.h"
#include "model_objects.h"

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

/// Stores the signed screen-space area of a flat quad's first triangle.
///
/// Reads word-aligned packed XY pairs 0..2 from `packet`, in signed pixel
/// coordinates. `facingArea` addresses one word-aligned writable s32 and
/// receives NCLIP's signed double area in square pixels; positive values
/// accept this half. Leaves vertices 0..2 in SXY0..SXY2, so pushing vertex 3
/// next tests vertices 1..3. All coordinate reads precede the result store;
/// the buffers may overlap. Performs no projection and clobbers MAC0/FLAG.
static inline void _tmdStoreFlatQuadFirstTriangleFacing(const POLY_F4* packet, s32* facingArea)
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

    node = Tmd_Create(src, 0);
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

    node = Tmd_Create(src, flags);
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
    enum {
        // Sixteen scaled OTZ units per tag, before wrapping to the OT's ten index bits.
        TMD_F4_PRE_XFORM_OT_INDEX_SHIFT = 4
    };
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

u32* gpDrawStreamPrimGt3PreXformFixedLayer(TmdStreamWorkspace* ws, s32 flags, u32* stream)
{
    POLY_GT3*     poly;
    s32*          opz;
    u32           clipMask;
    s32           len;
    s32           code;
    DisplayState* ds;
    u16*          rec;
    s32           sz;
    s32           idx;
    s32*          szTable;
    u8*           flagp;
    u8*           up;
    s32           i;
    s32           tpage;

    poly = (POLY_GT3*)ws->preXformWrite;
    if (ws->elemCount-- > 0) {
        opz      = &ws->gteResult;
        clipMask = TMD_VERTEX_DEPTH_INVALID;
        len      = 9;
        code     = 0x34;
        ds       = &gDisplayState;
        do {
            rec = (u16*)stream;
            gte_ldSXYP(GPU_PRIMITIVE_XY_WORD(poly, 0));
            gte_ldSXYP(GPU_PRIMITIVE_XY_WORD(poly, 1));
            gte_ldSXYP(GPU_PRIMITIVE_XY_WORD(poly, 2));
            gte_nclip();
            gte_stopz(opz);
            if (ws->gteResult > 0) {
                szTable = ws->szTable;
                idx     = rec[0] & 0xFFFC;
                sz      = szTable[(u32)idx / sizeof(*szTable)];
                if ((sz & clipMask) == 0) {
                    gte_ldSZ1(sz);
                    idx = rec[1] & 0xFFFC;
                    sz  = szTable[(u32)idx / sizeof(*szTable)];
                    if ((sz & clipMask) == 0) {
                        gte_ldSZ2(sz);
                        idx = rec[2] & 0xFFFC;
                        sz  = szTable[(u32)idx / sizeof(*szTable)];
                        if ((sz & clipMask) == 0) {
                            gte_ldSZ3(sz);
                            gte_avsz3();
                            // code, p1 and p2 each follow a vertex colour and hold that vertex's flag.
                            sz = poly->code | poly->p1 | poly->p2;
                            if (sz == 0) {
                                tpage = 0x137;
                            } else {
                                flagp = &poly->code;
                                i     = 0;
                                up    = &poly->u0;
                                do {
                                    if (*flagp == 0) {
                                        if ((s8)*up < 0) {
                                            *up += 0x80;
                                        } else {
                                            *up = 0;
                                        }
                                    }
                                    up += 0xC;
                                    i++;
                                    flagp += 0xC;
                                } while (i < 3);
                                tpage = 0x139;
                            }
                            poly->tpage = tpage;
                            setlen(poly, len);
                            setcode(poly, 0x36);
                            setlen(&poly[1], len);
                            setcode(&poly[1], code);
                            gte_stotz(opz);
                            addPrim(&ws->ot[((u32)ws->gteResult << ds->otDepthShift) >> 4 & 0x3FF], poly);
                            addPrim(&ws->ot[((u32)ws->gteResult << ds->otDepthShift) >> 4 & 0x3FF], &poly[1]);
                        }
                    }
                }
            }
            poly   += 2;
            stream += ws->elemStride;
        } while (ws->elemCount-- > 0);
    }
    ws->preXformWrite = (u8*)poly;
    return stream;
}

u32* gpDrawStreamPrimGt4PreXformLayer(TmdStreamWorkspace* ws, s32 flags, u32* stream)
{
    POLY_GT4*     poly;
    s32*          opz;
    u32           clipMask;
    s32           len;
    s32           code;
    DisplayState* ds;
    u16*          rec;
    s32           sz;
    s32           idx;
    s32*          szTable;
    u8*           flagp;
    u8*           up;
    s32           i;
    s32           tpage;

    poly = (POLY_GT4*)ws->preXformWrite;
    if (ws->elemCount-- > 0) {
        opz      = &ws->gteResult;
        clipMask = TMD_VERTEX_DEPTH_INVALID;
        len      = 12;
        code     = 0x3C;
        ds       = &gDisplayState;
        do {
            rec = (u16*)stream;
            gte_ldSXYP(GPU_PRIMITIVE_XY_WORD(poly, 0));
            gte_ldSXYP(GPU_PRIMITIVE_XY_WORD(poly, 1));
            gte_ldSXYP(GPU_PRIMITIVE_XY_WORD(poly, 2));
            gte_nclip();
            gte_stopz(opz);
            gte_ldSXYP(GPU_PRIMITIVE_XY_WORD(poly, 3));
            gte_nclip();
            if (ws->gteResult <= 0) {
                GPU_PRIMITIVE_XY_WORD(poly, 0)     = GPU_PRIMITIVE_XY_WORD(poly, 1);
                GPU_PRIMITIVE_XY_WORD(&poly[1], 0) = GPU_PRIMITIVE_XY_WORD(&poly[1], 1);
                gte_stopz(opz);
                if (ws->gteResult >= 0) {
                    goto next;
                }
            } else {
                gte_stopz(opz);
                if (ws->gteResult >= 0) {
                    GPU_PRIMITIVE_XY_WORD(poly, 3)     = GPU_PRIMITIVE_XY_WORD(poly, 2);
                    GPU_PRIMITIVE_XY_WORD(&poly[1], 3) = GPU_PRIMITIVE_XY_WORD(&poly[1], 2);
                }
            }
            szTable = ws->szTable;
            idx     = rec[0] & 0xFFFC;
            sz      = szTable[(u32)idx / sizeof(*szTable)];
            if ((sz & clipMask) == 0) {
                gte_ldSZ0(sz);
                idx = rec[1] & 0xFFFC;
                sz  = szTable[(u32)idx / sizeof(*szTable)];
                if ((sz & clipMask) == 0) {
                    gte_ldSZ1(sz);
                    idx = rec[2] & 0xFFFC;
                    sz  = szTable[(u32)idx / sizeof(*szTable)];
                    if ((sz & clipMask) == 0) {
                        gte_ldSZ2(sz);
                        idx = rec[3] & 0xFFFC;
                        sz  = szTable[(u32)idx / sizeof(*szTable)];
                        if ((sz & clipMask) == 0) {
                            gte_ldSZ3(sz);
                            gte_avsz4();
                            // code, p1, p2 and p3 each follow a vertex colour and hold that vertex's flag.
                            sz = poly->code | poly->p1 | poly->p2 | poly->p3;
                            if (sz == 0) {
                                tpage = 0x137;
                            } else {
                                flagp = &poly->code;
                                i     = 0;
                                up    = &poly->u0;
                                do {
                                    if (*flagp == 0) {
                                        if ((s8)*up < 0) {
                                            *up += 0x80;
                                        } else {
                                            *up = 0;
                                        }
                                    }
                                    up += 0xC;
                                    i++;
                                    flagp += 0xC;
                                } while (i < 4);
                                tpage = 0x139;
                            }
                            poly->tpage = tpage;
                            setlen(poly, len);
                            setcode(poly, 0x3E);
                            gte_stotz(opz);
                            setlen(&poly[1], len);
                            setcode(&poly[1], code);
                            addPrim(&ws->ot[((u32)ws->gteResult << ds->otDepthShift) >> 4 & 0x3FF], poly);
                            addPrim(&ws->ot[((u32)ws->gteResult << ds->otDepthShift) >> 4 & 0x3FF], &poly[1]);
                        }
                    }
                }
            }
        next:
            poly   += 2;
            stream += ws->elemStride;
        } while (ws->elemCount-- > 0);
    }
    ws->preXformWrite = (u8*)poly;
    return stream;
}

u32* gpDrawStreamPrimGt3PreXformOffsetLayer(TmdStreamWorkspace* ws, s32 flags, u32* stream)
{
    POLY_GT3*     poly;
    POLY_GT3*     xy;
    s32*          opz;
    u32           clipMask;
    s32           len;
    s32           code;
    DisplayState* ds;
    u16*          rec;
    s32           sz;
    s32           idx;
    s32*          szTable;

    poly = (POLY_GT3*)ws->preXformWrite;
    if (ws->elemCount-- > 0) {
        opz      = &ws->gteResult;
        clipMask = TMD_VERTEX_DEPTH_INVALID;
        len      = 9;
        code     = 0x34;
        ds       = &gDisplayState;
        do {
            xy  = poly + 1;
            rec = (u16*)stream;
            gte_ldSXYP(GPU_PRIMITIVE_XY_WORD(&xy[-1], 0));
            gte_ldSXYP(GPU_PRIMITIVE_XY_WORD(&xy[-1], 1));
            gte_ldSXYP(GPU_PRIMITIVE_XY_WORD(&xy[-1], 2));
            gte_nclip();
            gte_stopz(opz);
            if (ws->gteResult > 0) {
                szTable = ws->szTable;
                idx     = rec[0] & 0xFFFC;
                sz      = szTable[(u32)idx / sizeof(*szTable)];
                if ((sz & clipMask) == 0) {
                    gte_ldSZ1(sz);
                    idx = rec[1] & 0xFFFC;
                    sz  = szTable[(u32)idx / sizeof(*szTable)];
                    if ((sz & clipMask) == 0) {
                        gte_ldSZ2(sz);
                        idx = rec[2] & 0xFFFC;
                        sz  = szTable[(u32)idx / sizeof(*szTable)];
                        if ((sz & clipMask) == 0) {
                            gte_ldSZ3(sz);
                            gte_avsz3();
                            setlen(&xy[-1], len);
                            setcode(&xy[-1], 0x36);
                            gte_stotz(opz);
                            setlen(xy, len);
                            setcode(xy, code);
                            gte_stotz(opz);
                            addPrim(&ws->ot[((u32)ws->gteResult << ds->otDepthShift) >> 4 & 0x3FF], poly);
                            addPrim(&ws->ot[((u32)ws->gteResult << ds->otDepthShift) >> 4 & 0x3FF], xy);
                        }
                    }
                }
            }
            poly   += 2;
            stream += ws->elemStride;
        } while (ws->elemCount-- > 0);
    }
    ws->preXformWrite = (u8*)poly;
    return stream;
}

u32* gpDrawStreamPrimGt4PreXformOffsetLayer(TmdStreamWorkspace* ws, s32 flags, u32* stream)
{
    POLY_GT4*     poly;
    POLY_GT4*     xy;
    s32*          opz;
    u32           clipMask;
    s32           len;
    s32           code;
    DisplayState* ds;
    u16*          rec;
    s32           sz;
    s32           idx;
    s32*          szTable;

    poly = (POLY_GT4*)ws->preXformWrite;
    if (ws->elemCount-- > 0) {
        opz      = &ws->gteResult;
        clipMask = TMD_VERTEX_DEPTH_INVALID;
        len      = 12;
        code     = 0x3C;
        ds       = &gDisplayState;
        do {
            xy  = poly + 1;
            rec = (u16*)stream;
            gte_ldSXYP(GPU_PRIMITIVE_XY_WORD(&xy[-1], 0));
            gte_ldSXYP(GPU_PRIMITIVE_XY_WORD(&xy[-1], 1));
            gte_ldSXYP(GPU_PRIMITIVE_XY_WORD(&xy[-1], 2));
            gte_nclip();
            gte_stopz(opz);
            if (ws->gteResult > 0) {
                goto draw;
            }
            gte_ldSXYP(GPU_PRIMITIVE_XY_WORD(&xy[-1], 3));
            gte_nclip();
            gte_stopz(opz);
            if (ws->gteResult < 0) {
            draw:
                szTable = ws->szTable;
                idx     = rec[0] & 0xFFFC;
                sz      = szTable[(u32)idx / sizeof(*szTable)];
                if ((sz & clipMask) == 0) {
                    gte_ldSZ0(sz);
                    idx = rec[1] & 0xFFFC;
                    sz  = szTable[(u32)idx / sizeof(*szTable)];
                    if ((sz & clipMask) == 0) {
                        gte_ldSZ1(sz);
                        idx = rec[2] & 0xFFFC;
                        sz  = szTable[(u32)idx / sizeof(*szTable)];
                        if ((sz & clipMask) == 0) {
                            gte_ldSZ2(sz);
                            idx = rec[3] & 0xFFFC;
                            sz  = szTable[(u32)idx / sizeof(*szTable)];
                            if ((sz & clipMask) == 0) {
                                gte_ldSZ3(sz);
                                gte_avsz4();
                                setlen(&xy[-1], len);
                                setcode(&xy[-1], 0x3E);
                                gte_stotz(opz);
                                setlen(xy, len);
                                setcode(xy, code);
                                gte_stotz(opz);
                                addPrim(&ws->ot[((u32)ws->gteResult << ds->otDepthShift) >> 4 & 0x3FF], poly);
                                addPrim(&ws->ot[((u32)ws->gteResult << ds->otDepthShift) >> 4 & 0x3FF], xy);
                            }
                        }
                    }
                }
            }
            poly   += 2;
            stream += ws->elemStride;
        } while (ws->elemCount-- > 0);
    }
    ws->preXformWrite = (u8*)poly;
    return stream;
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

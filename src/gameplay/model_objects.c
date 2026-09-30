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

static inline u32* _gpPreXformEnvMapLit(TmdScratchModelBlock* ws, u32* arg2);

static __inline__ void _gpRefreshCoord(GfxCoord* coord, s32 stamp, s32 parity, GfxCoord* root);

static u32* func_8009A804(TmdScratchModelBlock* ws, s32 arg1, u32* arg2);

static u32* func_8009AA5C(TmdScratchModelBlock* ws, s32 arg1, u32* arg2);

static u32* func_8009AC58(TmdScratchModelBlock* ws, s32 arg1, u32* arg2);

static inline u32* _gpPreXformEnvMapLit(TmdScratchModelBlock* ws, u32* arg2)
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
                if (ws->gteFlag & 0x80000000) {
                    ws->gteResult |= 0x80000000;
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

/// Visits a coordinate after its ancestors and refreshes its composed matrix when stale.
///
/// The supplied root is excluded. `gte_RotTransLV` preserves the full signed
/// translation range while the rotation is composed with the GTE.
static __inline__ void _gpRefreshCoord(GfxCoord* coord, s32 stamp, s32 parity, GfxCoord* root)
{
    GfxCoord* parent;

    // Visit ancestors before composing; the parity bit records visits, not rebuilds.
    parent              = coord->parent;
    coord->composeStamp = (coord->composeStamp << 1) >> 1;
    if (parent == root) {
        if (coord->composeStamp == GRAPHICS_COORD_DIRTY) {
            coord->workm        = coord->coord;
            coord->composeStamp = stamp;
        }
    } else {
        if ((parent->composeStamp == GRAPHICS_COORD_DIRTY) || ((parent->composeStamp >> 31) != parity)) {
            _gpUpdateCoordTree(parent, stamp, parity, root);
        }
        if (coord->composeStamp < (parent->composeStamp & GRAPHICS_COORD_STAMP_MASK)) {
            gte_CompMatrix(&parent->workm, &coord->coord, &coord->workm);
            coord->composeStamp = stamp;
            gte_SetRotMatrix(&parent->workm);
            gte_ldclmv(&coord->coord.m[0][0]);
            gte_rtir();
            gte_stclmv(&coord->workm.m[0][0]);
            gte_ldclmv(&coord->coord.m[0][1]);
            gte_rtir();
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
        coord->composeStamp |= GRAPHICS_COORD_PARITY_BIT;
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
        task->spawnType = TASK_BODY_TMD;
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
        task->spawnType       = TASK_BODY_DISP2D;
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
        task->spawnType = TASK_BODY_TMD;
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
        prevSlot = &gTmdList.prev;
    } else {
        prevSlot = &next->prev;
    }
    prev       = node->prev;
    *prevSlot  = prev;
    prev->next = node->next;
}

void gpFreeTmd(TmdObject* obj)
{
    if (obj->buffer != NULL) {
        memFreeFromHeap(obj->buffer, 1);
        obj->buffer = NULL;
    }
    memFree(obj);
}

void modelObjectUnlinkDisp2d(TmdListNode* node)
{
    TmdListNode*  next;
    TmdListNode** prevSlot;
    TmdListNode*  prev;

    next = node->next;
    if (next == NULL) {
        prevSlot = &gModelObjectCoordBodyList.prev;
    } else {
        prevSlot = &next->prev;
    }
    prev       = node->prev;
    *prevSlot  = prev;
    prev->next = node->next;
}

void gpFreeDisp2d(ModelObjectCoordBody* node)
{
    memFree(node);
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

void _gpUpdateCoordTree(GfxCoord* coord, s32 stamp, s32 parity, GfxCoord* root)
{
    _gpRefreshCoord(coord, stamp, parity, root);
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
            switch (task->spawnType) {
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
                case TASK_BODY_DISP2D:
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

u32* gpDrawStreamPrimF4PreXform(TmdScratchModelBlock* ws, s32 flags, u32* stream)
{
    POLY_F4*      poly;
    s32*          opz;
    DisplayState* ds;
    u32           clipMask;
    u16*          rec;
    s32           sz;
    s32           idx;
    s32*          szTable;

    poly = (POLY_F4*)ws->preXformWrite;
    if (ws->elemCount-- > 0) {
        opz      = &ws->gteResult;
        clipMask = 0x80000000;
        ds       = &gDisplayState;
        do {
            rec = (u16*)stream;
            gte_ldSXYP(PRIM_XY_WORD(poly, 0));
            gte_ldSXYP(PRIM_XY_WORD(poly, 1));
            gte_ldSXYP(PRIM_XY_WORD(poly, 2));
            gte_nclip();
            gte_stopz(opz);
            if (ws->gteResult > 0) {
                goto draw;
            }
            gte_ldSXYP(PRIM_XY_WORD(poly, 3));
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
                                gte_stotz(opz);
                                gte_stotz(opz);
                                addPrim(&ws->ot[((u32)ws->gteResult << ds->otDepthShift) >> 4 & 0x3FF], poly);
                            }
                        }
                    }
                }
            }
            poly++;
            stream += ws->elemStride;
        } while (ws->elemCount-- > 0);
    }
    ws->preXformWrite = (u8*)poly;
    return stream;
}

u32* gpDrawStreamPrimF3PreXform(TmdScratchModelBlock* ws, s32 flags, u32* stream)
{
    POLY_F3*      poly;
    s32*          opz;
    DisplayState* ds;
    u32           clipMask;
    u16*          rec;
    s32           sz;
    s32           idx;
    s32*          szTable;

    poly = (POLY_F3*)ws->preXformWrite;
    if (ws->elemCount-- > 0) {
        opz      = &ws->gteResult;
        clipMask = 0x80000000;
        ds       = &gDisplayState;
        do {
            rec = (u16*)stream;
            gte_ldSXYP(PRIM_XY_WORD(poly, 0));
            gte_ldSXYP(PRIM_XY_WORD(poly, 1));
            gte_ldSXYP(PRIM_XY_WORD(poly, 2));
            gte_nclip();
            gte_stopz(opz);
            if (ws->gteResult < 0) {
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
                            gte_avsz3();
                            gte_stotz(opz);
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
    ws->preXformWrite = (u8*)poly;
    return stream;
}

u32* gpDrawStreamPrimGt3PreXformFixedLayer(TmdScratchModelBlock* ws, s32 flags, u32* stream)
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
        clipMask = 0x80000000;
        len      = 9;
        code     = 0x34;
        ds       = &gDisplayState;
        do {
            rec = (u16*)stream;
            gte_ldSXYP(PRIM_XY_WORD(poly, 0));
            gte_ldSXYP(PRIM_XY_WORD(poly, 1));
            gte_ldSXYP(PRIM_XY_WORD(poly, 2));
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

u32* gpDrawStreamPrimGt4PreXformLayer(TmdScratchModelBlock* ws, s32 flags, u32* stream)
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
        clipMask = 0x80000000;
        len      = 12;
        code     = 0x3C;
        ds       = &gDisplayState;
        do {
            rec = (u16*)stream;
            gte_ldSXYP(PRIM_XY_WORD(poly, 0));
            gte_ldSXYP(PRIM_XY_WORD(poly, 1));
            gte_ldSXYP(PRIM_XY_WORD(poly, 2));
            gte_nclip();
            gte_stopz(opz);
            gte_ldSXYP(PRIM_XY_WORD(poly, 3));
            gte_nclip();
            if (ws->gteResult <= 0) {
                PRIM_XY_WORD(poly, 0)     = PRIM_XY_WORD(poly, 1);
                PRIM_XY_WORD(&poly[1], 0) = PRIM_XY_WORD(&poly[1], 1);
                gte_stopz(opz);
                if (ws->gteResult >= 0) {
                    goto next;
                }
            } else {
                gte_stopz(opz);
                if (ws->gteResult >= 0) {
                    PRIM_XY_WORD(poly, 3)     = PRIM_XY_WORD(poly, 2);
                    PRIM_XY_WORD(&poly[1], 3) = PRIM_XY_WORD(&poly[1], 2);
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

u32* gpDrawStreamPrimGt3PreXformOffsetLayer(TmdScratchModelBlock* ws, s32 flags, u32* stream)
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
        clipMask = 0x80000000;
        len      = 9;
        code     = 0x34;
        ds       = &gDisplayState;
        do {
            xy  = poly + 1;
            rec = (u16*)stream;
            gte_ldSXYP(PRIM_XY_WORD(&xy[-1], 0));
            gte_ldSXYP(PRIM_XY_WORD(&xy[-1], 1));
            gte_ldSXYP(PRIM_XY_WORD(&xy[-1], 2));
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

u32* gpDrawStreamPrimGt4PreXformOffsetLayer(TmdScratchModelBlock* ws, s32 flags, u32* stream)
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
        clipMask = 0x80000000;
        len      = 12;
        code     = 0x3C;
        ds       = &gDisplayState;
        do {
            xy  = poly + 1;
            rec = (u16*)stream;
            gte_ldSXYP(PRIM_XY_WORD(&xy[-1], 0));
            gte_ldSXYP(PRIM_XY_WORD(&xy[-1], 1));
            gte_ldSXYP(PRIM_XY_WORD(&xy[-1], 2));
            gte_nclip();
            gte_stopz(opz);
            if (ws->gteResult > 0) {
                goto draw;
            }
            gte_ldSXYP(PRIM_XY_WORD(&xy[-1], 3));
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

static u32* func_8009A804(TmdScratchModelBlock* ws, s32 arg1, u32* arg2)
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
                if (ws->gteFlag & 0x80000000) {
                    ws->gteResult |= 0x80000000;
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

static u32* func_8009AA5C(TmdScratchModelBlock* ws, s32 arg1, u32* arg2)
{
    CVECTOR col;

    col = Gp_ColorOrange;
    gte_ldrgb(&col);
    return _gpPreXformEnvMapLit(ws, arg2);
}

static u32* func_8009AC58(TmdScratchModelBlock* ws, s32 arg1, u32* arg2)
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
                if (ws->gteFlag & 0x80000000) {
                    ws->gteResult |= 0x80000000;
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

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
#include "gameplay/display.h"
#include "model_objects.h"

#include "main/display.h"
#include "main/gfx.h"
#include "main/mem.h"
#include "main/task.h"
#include "main/tmd.h"

extern TmdListHead Gp_TmdListStash;

extern TmdListHead Gp_TmdListAltStash;

extern Task* Gp_TmdStashTask;

extern const CVECTOR Gp_ColorOrange;

static inline u32* _gpPreXformEnvMapLit(TmdScratchModelBlock* ws, u32* arg2);

/// Brings one coordinate up to date for the current pass, as
/// `_gpUpdateCoordTree` describes: the body that function and the draw passes
/// share, inlined into each. The high-precision translation is composed with
/// `gte_RotTransLV`.
static __inline__ void _gpRefreshCoord(GpCoord* coord, s32 stamp, s32 parity, GpCoord* root);

static void Gp_StashTmdLists(void);

static void Gp_RestoreTmdLists(void);

static Task* Gp_FindTaskByCoord(GpCoord* arg0);

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

TmdListHead Gp_TmdListStash    = { NULL, NULL };
TmdListHead Gp_TmdListAltStash = { NULL, NULL };
Task*       Gp_TmdStashTask    = NULL;

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

/// Brings one coordinate up to date for the current pass, as
/// `_gpUpdateCoordTree` describes: the body that function and the draw passes
/// share, inlined into each. The high-precision translation is composed with
/// `gte_RotTransLV`.
static __inline__ void _gpRefreshCoord(GpCoord* coord, s32 stamp, s32 parity, GpCoord* root)
{
    GpCoord* parent;

    parent     = coord->sub;
    coord->flg = (coord->flg << 1) >> 1;
    if (parent == root) {
        if (coord->flg == 0) {
            coord->workm = coord->coord;
            coord->flg   = stamp;
        }
    } else {
        if ((parent->flg == 0) || ((parent->flg >> 31) != parity)) {
            _gpUpdateCoordTree(parent, stamp, parity, root);
        }
        if (coord->flg < (parent->flg & 0x7FFFFFFF)) {
            gte_CompMatrix(&parent->workm, &coord->coord, &coord->workm);
            coord->flg = stamp;
            gte_SetRotMatrix(&parent->workm);
            gte_ldclmv(&coord->coord.m[0][0]);
            gte_rtir();
            gte_stclmv(&coord->workm.m[0][0]);
            gte_ldclmv(&coord->coord.m[0][1]);
            gte_rtir();
            coord->flg = stamp;
            gte_stclmv(&coord->workm.m[0][1]);
            gte_ldclmv(&coord->coord.m[0][2]);
            gte_rtir();
            gte_SetTransVector(parent->workm.t);
            gte_stclmv(&coord->workm.m[0][2]);
            gte_RotTransLV(coord->coord.t, coord->workm.t);
        }
    }
    if (parity != 0) {
        coord->flg |= 0x80000000;
    }
}

TmdObject* Gp_AttachTmd(Task* task, TmdSource* src)
{
    TmdObject*   node;
    TmdListHead* last;
    TmdListHead* list;

    node = Tmd_Create(src, 0);
    if (node != NULL) {
        list            = &gTmdList;
        last            = list->prev;
        node->link.next = last->next;
        last->next      = &node->link;
        node->link.prev = last;
        list->prev      = &node->link;
        task->extra.tmd = node;
        task->spawnType = 1;
    }
    return node;
}

GpDisp2d* gpAttachDisp2d(Task* task)
{
    GpDisp2d*    node;
    TmdListHead* last;
    TmdListHead* list;
    GpCoord*     coord;

    node = memCalloc(0x60, 0);
    if (node != NULL) {
        coord         = &node->coord;
        node->coords  = coord;
        node->field_C = 1;
        coord->sub    = &gGfxViewCoord;
        gfxSetRotIdentity(&coord->coord);
        coord->coord.t[2]   = 0;
        coord->coord.t[1]   = 0;
        coord->coord.t[0]   = 0;
        coord->param.rot.vz = 0;
        coord->param.rot.vy = 0;
        coord->param.rot.vx = 0;
        coord->flg          = 0;
        list                = &gTmdDisp2dList;
        last                = list->prev;
        node->link.next     = last->next;
        last->next          = &node->link;
        node->link.prev     = last;
        list->prev          = &node->link;
        task->extra.disp2d  = node;
        task->spawnType     = 2;
    } else {
        printf("new_disp_2d ----> NULL\n");
    }
    return node;
}

TmdObject* Gp_AttachTmdFlags(Task* task, TmdSource* src, s32 flags)
{
    TmdObject*   node;
    TmdListHead* last;
    TmdListHead* list;

    node = Tmd_Create(src, flags);
    if (node != NULL) {
        list            = &gTmdList;
        last            = list->prev;
        node->link.next = last->next;
        last->next      = &node->link;
        node->link.prev = last;
        list->prev      = &node->link;
        task->extra.tmd = node;
        task->spawnType = 1;
    }
    return node;
}

void gpUnlinkTmd(TmdListHead* node)
{
    TmdListHead*  next;
    TmdListHead** pp;
    TmdListHead*  prev;

    next = node->next;
    if (next == NULL) {
        pp = &gTmdList.prev;
    } else {
        pp = &next->prev;
    }
    prev       = node->prev;
    *pp        = prev;
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

void gpUnlinkDisp2d(TmdListHead* node)
{
    TmdListHead*  next;
    TmdListHead** pp;
    TmdListHead*  prev;

    next = node->next;
    if (next == NULL) {
        pp = &gTmdDisp2dList.prev;
    } else {
        pp = &next->prev;
    }
    prev       = node->prev;
    *pp        = prev;
    prev->next = node->next;
}

void gpFreeDisp2d(GpDisp2d* node)
{
    memFree(node);
}

static void Gp_StashTmdLists(void)
{
    Gp_TmdListStash     = gTmdList;
    Gp_TmdListAltStash  = gTmdDisp2dList;
    gTmdList.next       = NULL;
    gTmdList.prev       = &gTmdList;
    gTmdDisp2dList.next = NULL;
    gTmdDisp2dList.prev = &gTmdDisp2dList;
    Gp_TmdStashTask     = Task_Spawn(0, 0x1A, 0, 0);
}

static void Gp_RestoreTmdLists(void)
{
    Task_CallExit(Gp_TmdStashTask);
    gTmdList       = Gp_TmdListStash;
    gTmdDisp2dList = Gp_TmdListAltStash;
}

/// Brings a coordinate's world matrix up to date, and its ancestors' with it.
///
/// The chain of `sub` links is walked to `root` and composed on the way back
/// down, so a coordinate's world matrix is its parent's world matrix multiplied
/// by its own local one. Each coordinate records in `flg` the stamp of the pass
/// that last rebuilt it, and is rebuilt when that stamp is older than its
/// parent's — which is what clearing `flg` asks for. A coordinate whose `flg` is
/// clear at the walk's end has never been composed and takes its local matrix
/// as its world matrix.
///
/// `parity` is the value written to the top bit of `flg`; it differs from one
/// pass to the next, which is what tells the walk an ancestor has already been
/// reached in this one. `root` ends the walk: the composed matrices are left
/// relative to it, and its own world matrix is neither updated nor folded in,
/// because the caller that passes one applies that transformation itself.
/// `NULL` stops at the top of the chain.
void _gpUpdateCoordTree(GpCoord* coord, s32 stamp, s32 parity, GpCoord* root)
{
    _gpRefreshCoord(coord, stamp, parity, root);
}

static Task* Gp_FindTaskByCoord(GpCoord* arg0)
{
    Task*      task;
    TmdObject* extra;
    GpCoord*   coord;
    u32        i;
    s32        found;
    u32        count;

    task = Task_GetActiveList()->next;
    if (task != NULL) {
        do {
            found = 0;
            switch (task->spawnType) {
                case 1:
                    extra = task->extra.tmd;
                    count = extra->partCount;
                    coord = extra->coords;
                    for (i = 0; i < count; i++) {
                        if (coord == arg0) {
                            found = 1;
                            break;
                        }
                        coord++;
                    }
                    break;
                case 2:
                    extra = task->extra.tmd;
                    coord = extra->coords;
                    if (coord == arg0) {
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
            if (ws->obj->lightLevel < 0x1000) {
                gte_lddp(ws->obj->lightLevel);
                cptr = ws->preXformWrite + rec[3];
                gte_ldcv(cptr);
                gte_gpf12();
                gte_lddp(0x1000 - ws->obj->lightLevel);
                gte_ldcv(&col2);
                gte_gpl12();
                gte_stcv(cptr);
            }
            xy   = &ws->texCoord.vx;
            page = 0;
            dest = ws->preXformWrite + rec[2] + 4;
            sv   = &ws->elemNormal;
            gte_lddp(ws->obj->lightLevel >> 9);
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

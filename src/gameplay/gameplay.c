#include "common.h"

#include <psyq/inline_c.h>
#include "gte.h"
#include "main/gfxgte.h"
#include <psyq/gtemac.h>
#include <psyq/memory.h>
#include <psyq/rand.h>
#include <psyq/stdio.h>

#include "gameplay/1A8.h"
#include "gameplay/1BC.h"
#include "gameplay/268.h"
#include "gameplay/3688.h"
#include "gameplay/3A34.h"
#include "gameplay/3CD8.h"
#include "gameplay/4CC.h"
#include "gameplay/D4.h"
#include "gameplay/gameplay.h"
#include "main/display.h"
#include "main/fs.h"
#include "main/gamemain.h"
#include "main/gfx.h"
#include "main/mc.h"
#include "main/mem.h"
#include "main/pad.h"
#include "main/session.h"
#include "main/sound.h"
#include "main/stage.h"
#include "main/stream.h"
#include "main/task.h"
#include "main/text.h"
#include "main/tmd.h"
#include "main/ui.h"
#include "main/wipsys.h"

extern u8           Gp_StrItemObtained[]; // "Item obtained!"
extern u8           Gp_StrBonusItem[];    // "Bonus item!!"
extern s32          Gp_ItemGrantCooldown;
extern McItemScan   D_8010CA2C;
extern UiObjectDesc D_8010CA40;
extern UiObjectDesc D_8010CA78[];
extern UiObjectDesc D_8010D6D8;
extern UiObjectDesc D_80185000;
extern TaskDesc     D_8010CAB0;
extern TaskDesc     D_8010CABC;
extern TaskDesc     D_8010D1FC;
extern TmdListHead  Gp_TmdListStash;
extern s32          D_80114A24;
extern s32          D_80114A34;
extern u16          D_8007A39C;
extern TmdListHead  Gp_TmdListAltStash;
extern Task*        Gp_TmdStashTask;
/// The coordinate a world-matrix update was most recently run for.
///
/// The entry points that refresh a coordinate record it before the ancestor
/// chain is walked, and nothing ever reads it: what the recorded coordinate is
/// kept for is not established.
extern GpCoord* _gGpCurCoord;
extern CVECTOR  D_80114BA4;
extern u16      D_80114BB0[];
extern RECT     D_80114BD0;
extern CVECTOR  D_80114BA8;
extern u8       Gp_DebugAttachLevels[];
extern s32      Pad_MaskConfirm;
extern s32      Pad_MaskCancel;
extern s16      D_80114C40;
extern DR_STP   D_80114C50;
extern s32      D_80115724;

extern McItemRec* Gp_SelItemRec;
extern const char D_8009388C[];
extern const char D_80093890[];
extern const char D_80093894[];
extern const char D_80093898[];
extern const char D_800938AC[];

void Gp_DrawItemIcon(UiObject* arg0, s32 arg1, s32 arg2, s32 arg3, s32 arg4);

void func_80108874(void);
void func_800A57B0(GpIdMapC* arg0);
void Gp_UseItemTask(GpIdMapC* arg0);
s32  func_800A2104(GpIdMapC* arg0, s32 arg1, s32 arg2);
s32  func_800A7550(void);

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

static void _gpUpdateCoordTree(GpCoord* coord, s32 stamp, s32 parity,
                               GpCoord* root);
void        func_800A4904(s32 arg0);
void        Gp_DrawAimCircle(s32 arg0, s32 arg1, s32 arg2, s32 arg3);
void        Gp_InitSlot18(s32 arg0, s32 arg1, s32 arg2, s32 arg3);
void        func_800A5574(s32 arg0, s32 arg1, s32 arg2, s32 arg3);
void        func_800A7824(s32 arg0, s32 arg1, s32 arg2);
void        Gp_DrawHudNumbers(s32 arg0, s32 arg1, s32 arg2, s32 arg3, s32 arg4);
s32         Gp_SpawnViewCoordTask(GpCoord* arg0, VECTOR* arg1);
void        Gp_FinishLoadWait(Task* task);
void        func_807150F8(s32 arg0);
void        func_80715198(void);

/// Working state of a relative transform between two coordinate frames, carved
/// from the scratch arena that `G_SCRATCH_HEAD` heads.
///
/// `rot` is the source frame's rotation transposed, so that multiplying a
/// matrix by it yields that matrix's orientation relative to the source;
/// `delta` is the target origin relative to the source, which the same
/// rotation turns into the destination translation.
typedef struct {
    MATRIX rot;   // source frame's rotation, transposed
    VECTOR delta; // target origin minus source origin
} _GpRelMatScratch;
STATIC_ASSERT_SIZEOF(_GpRelMatScratch, 0x30);

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

/// Brings every coordinate the draw passes use up to date for this frame: the
/// 2D displays' single coordinates, then each model's part coordinates, and
/// advances the frame stamp the next pass will compare against.
static __inline__ void _gpRefreshAllCoords(void)
{
    TmdObject* node;
    GpCoord*   coord;
    s32        stamp;
    s32        parity;
    u32        i;

    stamp  = D_80071210 & 0x7FFFFFFF;
    parity = D_80071210 & 1;
    for (node = PARENT_OF(gTmdDisp2dList.next, TmdObject, link); node != NULL;
         node = PARENT_OF(node->link.next, TmdObject, link)) {
        _gpRefreshCoord(node->coords, stamp, parity, NULL);
    }
    for (node = PARENT_OF(gTmdList.next, TmdObject, link); node != NULL;
         node = PARENT_OF(node->link.next, TmdObject, link)) {
        coord = node->coords;
        for (i = 0; i < node->partCount; i++) {
            _gpRefreshCoord(coord, stamp, parity, NULL);
            coord++;
        }
    }
    D_80071210 += 1;
}

/// Refreshes every coordinate for this frame, then draws the models the
/// flagged pass draws.
void Gp_DrawActorTmdFlagged(GpuOtBuf* arg0)
{
    _gpRefreshAllCoords();
    Tmd_DrawFlaggedNodes(PARENT_OF(gTmdList.next, TmdObject, link));
}

/// Refreshes every coordinate for this frame, then draws the active models.
void Gp_DrawActorTmdActive(GpuOtBuf* arg0)
{
    _gpRefreshAllCoords();
    Tmd_DrawActiveNodes(PARENT_OF(gTmdList.next, TmdObject, link));
}

void Gp_UpdateCoord(GpCoord* arg0)
{
    _gGpCurCoord = arg0;
    _gpUpdateCoordTree(arg0, D_80071210 & 0x7FFFFFFF, D_80071210 & 1, 0);
}

void Gp_UpdateCoordEx(GpCoord* arg0, GpCoord* arg1)
{
    if (arg0->sub == NULL) {
        _gGpCurCoord = arg0;
        _gpUpdateCoordTree(arg0, D_80071210 & 0x7FFFFFFF, D_80071210 & 1, 0);
        Gp_WorldToLocal(&gGfxViewCoord.workm, &arg0->workm, &arg0->coord);
    } else {
        _gpUpdateCoordTree(arg0, D_80071210 & 0x7FFFFFFF, D_80071210 & 1, arg1);
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
        printf("new_disp_2d ----> NULL
");
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
static void _gpUpdateCoordTree(GpCoord* coord, s32 stamp, s32 parity, GpCoord* root)
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

void Gp_DrawDisp2dOt(void)
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
    u8*           szTable;

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
                szTable = (u8*)ws->szTable;
                idx     = rec[0] & 0xFFFC;
                sz      = *(s32*)(idx + (s32)szTable);
                if ((sz & clipMask) == 0) {
                    gte_ldSZ0(sz);
                    idx = rec[1] & 0xFFFC;
                    sz  = *(s32*)(idx + (s32)szTable);
                    if ((sz & clipMask) == 0) {
                        gte_ldSZ1(sz);
                        idx = rec[2] & 0xFFFC;
                        sz  = *(s32*)(idx + (s32)szTable);
                        if ((sz & clipMask) == 0) {
                            gte_ldSZ2(sz);
                            idx = rec[3] & 0xFFFC;
                            sz  = *(s32*)(idx + (s32)szTable);
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
    u8*           szTable;

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
                szTable = (u8*)ws->szTable;
                idx     = rec[0] & 0xFFFC;
                sz      = *(s32*)(idx + (s32)szTable);
                if ((sz & clipMask) == 0) {
                    gte_ldSZ0(sz);
                    idx = rec[1] & 0xFFFC;
                    sz  = *(s32*)(idx + (s32)szTable);
                    if ((sz & clipMask) == 0) {
                        gte_ldSZ1(sz);
                        idx = rec[2] & 0xFFFC;
                        sz  = *(s32*)(idx + (s32)szTable);
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
    u8*           szTable;
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
                szTable = (u8*)ws->szTable;
                idx     = rec[0] & 0xFFFC;
                sz      = *(s32*)(idx + (s32)szTable);
                if ((sz & clipMask) == 0) {
                    gte_ldSZ1(sz);
                    idx = rec[1] & 0xFFFC;
                    sz  = *(s32*)(idx + (s32)szTable);
                    if ((sz & clipMask) == 0) {
                        gte_ldSZ2(sz);
                        idx = rec[2] & 0xFFFC;
                        sz  = *(s32*)(idx + (s32)szTable);
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
    u8*           szTable;
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
            szTable = (u8*)ws->szTable;
            idx     = rec[0] & 0xFFFC;
            sz      = *(s32*)(idx + (s32)szTable);
            if ((sz & clipMask) == 0) {
                gte_ldSZ0(sz);
                idx = rec[1] & 0xFFFC;
                sz  = *(s32*)(idx + (s32)szTable);
                if ((sz & clipMask) == 0) {
                    gte_ldSZ1(sz);
                    idx = rec[2] & 0xFFFC;
                    sz  = *(s32*)(idx + (s32)szTable);
                    if ((sz & clipMask) == 0) {
                        gte_ldSZ2(sz);
                        idx = rec[3] & 0xFFFC;
                        sz  = *(s32*)(idx + (s32)szTable);
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
    u8*           szTable;

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
                szTable = (u8*)ws->szTable;
                idx     = rec[0] & 0xFFFC;
                sz      = *(s32*)(idx + (s32)szTable);
                if ((sz & clipMask) == 0) {
                    gte_ldSZ1(sz);
                    idx = rec[1] & 0xFFFC;
                    sz  = *(s32*)(idx + (s32)szTable);
                    if ((sz & clipMask) == 0) {
                        gte_ldSZ2(sz);
                        idx = rec[2] & 0xFFFC;
                        sz  = *(s32*)(idx + (s32)szTable);
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
    u8*           szTable;

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
                szTable = (u8*)ws->szTable;
                idx     = rec[0] & 0xFFFC;
                sz      = *(s32*)(idx + (s32)szTable);
                if ((sz & clipMask) == 0) {
                    gte_ldSZ0(sz);
                    idx = rec[1] & 0xFFFC;
                    sz  = *(s32*)(idx + (s32)szTable);
                    if ((sz & clipMask) == 0) {
                        gte_ldSZ1(sz);
                        idx = rec[2] & 0xFFFC;
                        sz  = *(s32*)(idx + (s32)szTable);
                        if ((sz & clipMask) == 0) {
                            gte_ldSZ2(sz);
                            idx = rec[3] & 0xFFFC;
                            sz  = *(s32*)(idx + (s32)szTable);
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

/// Neutral grey (128,128,128) material colour.
///
/// The pre-transformed primitives that carry no colour of their own are shaded
/// with it, and one whose object is dimmed by `lightLevel` decays toward it as
/// the level falls, so it is both the flat material colour and the unlit end
/// of the shading range.
static const CVECTOR gGpColorGrey   = { 0x80, 0x80, 0x80, 0 };
static const CVECTOR Gp_ColorOrange = { 0xFF, 0xA0, 0x60, 0 };
/// The base colour a lit primitive is computed from when the lighting alone
/// should decide its colour: white, the identity of the GTE's colour multiply.
///
/// A `CVECTOR`, so the whole colour is loaded into the GTE at once. Colour
/// computations start from a copy of it and overwrite the channels they derive.
static const CVECTOR gGpColorWhite = { 0xFF, 0xFF, 0xFF, 0 };

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

/// Projects each vertex of a record run into the pre-transform buffer, with a
/// texture coordinate derived from its screen position offset by its rotated
/// normal, and a colour lit from the colour already loaded into the GTE.
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
    DVECTOR* xy;
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
            xy = &ws->texCoord;
            TOUCH_REG(xy);
            page = 0;
            dest = ws->preXformWrite + rec[2] + 4;
            sv   = &ws->elemNormal;
            gte_lddp(ws->obj->lightLevel >> 9);
            gte_ldsv(sv);
            gte_gpf12();
            gte_stsv(sv);
            uv  = ws->texCoord.vx + 0xA0;
            uv -= ws->elemNormal.vx;
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
            uv    = xy->vy + 0x78;
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

u32* func_8009AF90(TmdScratchModelBlock* ws, s32 arg1, u32* arg2)
{
    s32      prev;
    s32      count;
    u32      idx;
    u16*     rec;
    CVECTOR  col;
    u8*      dest;
    u8*      rgb;
    DVECTOR* sxy;
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
                if (ws->gteFlag & 0x80000000) {
                    ws->gteResult |= 0x80000000;
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
            // Blend the primitive colour towards the grey reference by the
            // object's light level when the level is below 1.0 (0x1000).
            dp = ws->obj->lightLevel;
            if (dp < 0x1000) {
                gte_lddp(dp);
                rgb = ws->preXformWrite + rec[2];
                gte_ldcv(rgb);
                gte_gpf12();
                gte_lddp(0x1000 - dp);
                gte_ldcv(&col);
                gte_gpl12();
                gte_stcv(rgb);
            }
            sxy = &ws->texCoord;
            TOUCH_REG(sxy);
            flag = 0;
            dest = ws->preXformWrite + rec[2] + 8;
            sv   = &ws->elemNormal;
            gte_lddp(ws->obj->lightLevel >> 9);
            gte_ldsv(sv);
            gte_gpf12();
            gte_stsv(sv);
            // dest[8]/dest[9] are the U/V pair; dest[3] (dest[-6] once dest has
            // been advanced onto the V byte) is the primitive's code byte, set
            // when U had to be pulled back onto the second texture page.
            x  = ws->texCoord.vx + 0xA0;
            x -= ws->elemNormal.vx;
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
            dest++;
            x  = sxy->vy + 0x78;
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

u32* gpXformStreamVertsOffsetLayer(TmdScratchModelBlock* ws, s32 flags, u32* stream)
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
    val    = ws->obj->lightLevel >> 5;
    inv    = 0x80 - val;
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
                if (ws->gteFlag & 0x80000000) {
                    ws->gteResult |= 0x80000000;
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

u32* func_8009B500(TmdScratchModelBlock* ws, s32 arg1, u32* arg2)
{
    POLY_GT3* poly;
    u16*      rec;
    u8*       verts;
    u8*       norms;
    CVECTOR   col;
    SVECTOR*  sv;
    DVECTOR*  sxy;
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
                    if (ws->obj->lightLevel < 0x1000) {
                        gte_lddp(ws->obj->lightLevel);
                        rgb = &poly[0].r0;
                        gte_ldcv(rgb);
                        gte_gpf12();
                        gte_lddp(0x1000 - ws->obj->lightLevel);
                        gte_ldcv(&col);
                        gte_gpl12();
                        gte_stcv(rgb);
                        gte_lddp(ws->obj->lightLevel);
                        rgb = &poly[0].r1;
                        gte_ldcv(rgb);
                        gte_gpf12();
                        gte_lddp(0x1000 - ws->obj->lightLevel);
                        gte_ldcv(&col);
                        gte_gpl12();
                        gte_stcv(rgb);
                        gte_lddp(ws->obj->lightLevel);
                        rgb = &poly[0].r2;
                        gte_ldcv(rgb);
                        gte_gpf12();
                        gte_lddp(0x1000 - ws->obj->lightLevel);
                        gte_ldcv(&col);
                        gte_gpl12();
                        gte_stcv(rgb);
                    }

                    /* Environment-map UVs: each vertex's rotated normal, scaled by the
                     * light level, offsets its screen position into the reflection
                     * texture. A U past the first page wraps onto the second one and
                     * is flagged in the pad byte after that vertex's colour. */
                    gte_rtv0();
                    gte_stsv(&ws->elemNormal);
                    combined = 0;
                    sxy      = (DVECTOR*)&poly[0].x0;
                    dest     = &poly[0].u0;
                    flag     = 0;
                    sv       = &ws->elemNormal;
                    gte_lddp(ws->obj->lightLevel >> 9);
                    gte_ldsv(sv);
                    gte_gpf12();
                    gte_stsv(sv);
                    x  = poly[0].x0 + 0xA0;
                    x -= ws->elemNormal.vx;
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
                    dest++;
                    x  = sxy->vy + 0x78;
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
                    sxy = (DVECTOR*)&poly[0].x1;
                    TOUCH_REG(sxy);
                    dest = &poly[0].u1;
                    flag = 0;
                    sv   = &ws->elemNormal;
                    gte_lddp(ws->obj->lightLevel >> 9);
                    gte_ldsv(sv);
                    gte_gpf12();
                    gte_stsv(sv);
                    x  = poly[0].x1 + 0xA0;
                    x -= ws->elemNormal.vx;
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
                    dest++;
                    x  = sxy->vy + 0x78;
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
                    sxy = (DVECTOR*)&poly[0].x2;
                    TOUCH_REG(sxy);
                    dest = &poly[0].u2;
                    flag = 0;
                    sv   = &ws->elemNormal;
                    gte_lddp(ws->obj->lightLevel >> 9);
                    gte_ldsv(sv);
                    gte_gpf12();
                    gte_stsv(sv);
                    x  = poly[0].x2 + 0xA0;
                    x -= ws->elemNormal.vx;
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
                    dest++;
                    x  = sxy->vy + 0x78;
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
                    addPrim((u_long*)(((((u32)ws->gteResult << gDisplayState.otDepthShift) >> 2) & 0xFFC) + (s32)ws->ot), &poly[0]);
                    addPrim((u_long*)(((((u32)ws->gteResult << gDisplayState.otDepthShift) >> 2) & 0xFFC) + (s32)ws->ot), &poly[1]);
                }
            }
            poly += 2;
            arg2 += ws->elemStride;
        } while (ws->elemCount-- > 0);
    }
    ws->primWrite = (u8*)poly;
    return arg2;
}

u32* gpDrawStreamPrimGt3OffsetLayer(TmdScratchModelBlock* ws, s32 flags, u32* stream)
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
    val    = ws->obj->lightLevel >> 5;
    inv    = 0x80 - val;
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
                    poly[0].tag = (poly[0].tag & maskHi) | (*(u_long*)(((((u32)ws->gteResult << ds->otDepthShift) >> 2) & 0xFFC) + (s32)ws->ot) & mask);
                    *(u_long*)(((((u32)ws->gteResult << ds->otDepthShift) >> 2) & 0xFFC) + (s32)ws->ot) =
                        (*(u_long*)(((((u32)ws->gteResult << ds->otDepthShift) >> 2) & 0xFFC) + (s32)ws->ot) & maskHi) | ((u32)&poly[0] & mask);
                    poly[1].tag = (poly[1].tag & maskHi) | (*(u_long*)(((((u32)ws->gteResult << ds->otDepthShift) >> 2) & 0xFFC) + (s32)ws->ot) & mask);
                    *(u_long*)(((((u32)ws->gteResult << ds->otDepthShift) >> 2) & 0xFFC) + (s32)ws->ot) =
                        (*(u_long*)(((((u32)ws->gteResult << ds->otDepthShift) >> 2) & 0xFFC) + (s32)ws->ot) & maskHi) | ((u32)&poly[1] & mask);
                }
            }
            poly   += 2;
            stream += ws->elemStride;
        } while (ws->elemCount-- > 0);
    }
    ws->primWrite = (u8*)poly;
    return stream;
}

u32* gpDrawStreamPrimGt4OffsetLayer(TmdScratchModelBlock* ws, s32 flags, u32* stream)
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
    val    = ws->obj->lightLevel >> 5;
    inv    = 0x80 - val;
    col.b  = val;
    col.g  = val;
    col.r  = val;
    col2.b = inv;
    col2.g = inv;
    col2.r = inv;
    if (ws->elemCount-- > 0) {
        flg      = &ws->gteFlag;
        clipMask = 0x80000000;
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
                        poly[0].tag = (poly[0].tag & maskHi) | (*(u_long*)(((((u32)ws->gteResult << ds->otDepthShift) >> 2) & 0xFFC) + (s32)ws->ot) & mask);
                        *(u_long*)(((((u32)ws->gteResult << ds->otDepthShift) >> 2) & 0xFFC) + (s32)ws->ot) =
                            (*(u_long*)(((((u32)ws->gteResult << ds->otDepthShift) >> 2) & 0xFFC) + (s32)ws->ot) & maskHi) | ((u32)&poly[0] & mask);
                        poly[1].tag = (poly[1].tag & maskHi) | (*(u_long*)(((((u32)ws->gteResult << ds->otDepthShift) >> 2) & 0xFFC) + (s32)ws->ot) & mask);
                        *(u_long*)(((((u32)ws->gteResult << ds->otDepthShift) >> 2) & 0xFFC) + (s32)ws->ot) =
                            (*(u_long*)(((((u32)ws->gteResult << ds->otDepthShift) >> 2) & 0xFFC) + (s32)ws->ot) & maskHi) | ((u32)&poly[1] & mask);
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

u32* func_8009C414(TmdScratchModelBlock* ws, s32 arg1, u32* arg2)
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
            if ((ws->gteFlag & 0x80000000) == 0) {
                gte_nclip();
                gte_stopz(opz);
                gte_stsxy3_gt4(&poly[0]);
                gte_stsxy3_gt4(&poly[1]);
                gte_ldv0((u8*)ws->verts + (rec[3] & 0xFFF8));
                gte_rtps();
                gte_stflg(flg);
                if ((ws->gteFlag & 0x80000000) == 0) {
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
                     * light level, offsets its screen position into the reflection
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
                    gte_lddp(ws->obj->lightLevel >> 9);
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
                    gte_lddp(ws->obj->lightLevel >> 9);
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
                    gte_lddp(ws->obj->lightLevel >> 9);
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
                    gte_lddp(ws->obj->lightLevel >> 9);
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
                    if (ws->obj->lightLevel < 0x1000) {
                        gte_lddp(ws->obj->lightLevel);
                        rgb = &poly[0].r0;
                        gte_ldcv(rgb);
                        gte_gpf12();
                        gte_lddp(0x1000 - ws->obj->lightLevel);
                        gte_ldcv(&col);
                        gte_gpl12();
                        gte_stcv(rgb);

                        gte_lddp(ws->obj->lightLevel);
                        rgb = &poly[0].r1;
                        gte_ldcv(rgb);
                        gte_gpf12();
                        gte_lddp(0x1000 - ws->obj->lightLevel);
                        gte_ldcv(&col);
                        gte_gpl12();
                        gte_stcv(rgb);

                        gte_lddp(ws->obj->lightLevel);
                        rgb = &poly[0].r2;
                        gte_ldcv(rgb);
                        gte_gpf12();
                        gte_lddp(0x1000 - ws->obj->lightLevel);
                        gte_ldcv(&col);
                        gte_gpl12();
                        gte_stcv(rgb);

                        gte_lddp(ws->obj->lightLevel);
                        rgb3 = &poly[0].r3;
                        gte_ldcv(rgb3);
                        gte_gpf12();
                        gte_lddp(0x1000 - ws->obj->lightLevel);
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
                    addPrim((u_long*)(((((u32)ws->gteResult << gDisplayState.otDepthShift) >> 2) & 0xFFC) + (s32)ws->ot), &poly[0]);
                    addPrim((u_long*)(((((u32)ws->gteResult << gDisplayState.otDepthShift) >> 2) & 0xFFC) + (s32)ws->ot), &poly[1]);
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

u32* gpDrawStreamPrimGt3ElemColor(TmdScratchModelBlock* ws, s32 flags, u32* stream)
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
                    if (ws->obj->flags & 2) {
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

u32* gpDrawStreamPrimGt4ElemColor(TmdScratchModelBlock* ws, s32 flags, u32* stream)
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
        clipMask = 0x80000000;
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
                        if (ws->obj->flags & 2) {
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

u32* func_8009D388(TmdScratchModelBlock* arg0, s32 arg1, u32* arg2)
{
    TmdScratchModelBlock* ws;
    POLY_FT3*             poly;
    s32*                  opz;
    DisplayState*         ds;
    u16*                  rec;
    u8*                   verts;

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

u32* func_8009D518(TmdScratchModelBlock* arg0, s32 arg1, u32* arg2)
{
    TmdScratchModelBlock* ws;
    POLY_FT4*             poly;
    s32*                  opz;
    DisplayState*         ds;
    u32                   clipMask;
    s32*                  flg;
    u16*                  rec;
    u8*                   verts;

    ws   = arg0;
    poly = (POLY_FT4*)ws->primWrite;
    if (ws->elemCount-- > 0) {
        flg      = &ws->gteFlag;
        clipMask = 0x80000000;
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

u32* func_8009D718(TmdScratchModelBlock* arg0, s32 arg1, u32* arg2)
{
    TmdScratchModelBlock* ws;
    POLY_GT4*             poly;
    s32*                  opz;
    DisplayState*         ds;
    u32                   clipMask;
    s32*                  flg;
    u16*                  rec;
    u8*                   verts;

    ws   = arg0;
    poly = (POLY_GT4*)ws->primWrite;
    if (ws->elemCount-- > 0) {
        flg      = &ws->gteFlag;
        clipMask = 0x80000000;
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

u32* func_8009D900(TmdScratchModelBlock* arg0, s32 arg1, u32* arg2)
{
    TmdScratchModelBlock* ws;
    POLY_F4*              poly;
    s32*                  opz;
    DisplayState*         ds;
    u32                   clipMask;
    s32*                  flg;
    u16*                  rec;
    u8*                   verts;

    ws   = arg0;
    poly = (POLY_F4*)ws->primWrite;
    if (ws->elemCount-- > 0) {
        flg      = &ws->gteFlag;
        clipMask = 0x80000000;
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

u32* func_8009DB00(TmdScratchModelBlock* arg0, s32 arg1, u32* arg2)
{
    TmdScratchModelBlock* ws;
    POLY_F3*              poly;
    s32*                  opz;
    DisplayState*         ds;
    u32                   clipMask;
    s32*                  flg;
    u16*                  rec;
    u8*                   verts;

    ws   = arg0;
    poly = (POLY_F3*)ws->primWrite;
    if (ws->elemCount-- > 0) {
        flg      = &ws->gteFlag;
        clipMask = 0x80000000;
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

u32* func_8009DCB8(TmdScratchModelBlock* arg0, s32 arg1, u32* arg2)
{
    TmdScratchModelBlock* ws;
    POLY_FT3*             poly;
    s32*                  opz;
    DisplayState*         ds;
    u16*                  rec;
    u8*                   verts;

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

u32* func_8009DE48(TmdScratchModelBlock* arg0, s32 arg1, u32* arg2)
{
    TmdScratchModelBlock* ws;
    POLY_FT4*             poly;
    s32*                  opz;
    DisplayState*         ds;
    u32                   clipMask;
    s32*                  flg;
    u16*                  rec;
    u8*                   verts;

    ws   = arg0;
    poly = (POLY_FT4*)ws->primWrite;
    if (ws->elemCount-- > 0) {
        flg      = &ws->gteFlag;
        clipMask = 0x80000000;
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

u32* func_8009E048(TmdScratchModelBlock* arg0, s32 arg1, u32* arg2)
{
    TmdScratchModelBlock* ws;
    POLY_G3*              poly;
    s32*                  opz;
    DisplayState*         ds;
    u16*                  rec;
    u8*                   verts;

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

u32* func_8009E274(TmdScratchModelBlock* arg0, s32 arg1, u32* arg2)
{
    TmdScratchModelBlock* ws;
    POLY_G3*              poly;
    s32*                  opz;
    DisplayState*         ds;
    u16*                  rec;
    u8*                   verts;

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

u32* func_8009E4A0(TmdScratchModelBlock* arg0, s32 arg1, u32* arg2)
{
    TmdScratchModelBlock* ws;
    POLY_G4*              poly;
    s32*                  opz;
    DisplayState*         ds;
    u32                   clipMask;
    s32*                  flg;
    u16*                  rec;
    u8*                   verts;

    ws   = arg0;
    poly = (POLY_G4*)ws->primWrite;
    if (ws->elemCount-- > 0) {
        flg      = &ws->gteFlag;
        clipMask = 0x80000000;
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

u32* gpDrawStreamPrimG4CornerColorsSemiTrans(TmdScratchModelBlock* ws, s32 flags, u32* stream)
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
        clipMask = 0x80000000;
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

u32* gpXformStreamVertsUnlit(TmdScratchModelBlock* ws, s32 flags, u32* stream)
{
    s32  prev;
    s32  count;
    u32  idx;
    u16* rec;

    prev  = -1;
    count = ws->elemCount;
    if (count == 0) {
        return stream;
    }
    TOUCH_REG(prev);
    ws->elemCount = count + prev;
    if (count > 0) {
        do {
            rec = (u16*)stream;
            idx = rec[0];
            if (idx != prev) {
                gte_ldv0((u8*)ws->verts + (idx & 0xFFF8));
                gte_rtps();
                gte_stsz(&ws->gteResult);
                if (ws->gteFlag & 0x80000000) {
                    ws->gteResult |= 0x80000000;
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

u32* gpStreamPrimGt3PreXform(TmdScratchModelBlock* ws, s32 flags, u32* stream)
{
    POLY_GT3* poly;

    poly = (POLY_GT3*)ws->preXformWrite;
    if (ws->elemCount-- > 0) {
        do {
            PRIM_UV_CLUT_WORD(poly)  = stream[2];
            PRIM_UV_TPAGE_WORD(poly) = stream[3];
            *(u16*)&poly->u2         = (u16)stream[4];
            poly->tpage             += ws->tpage;
            poly->clut              += ws->clut;
            poly++;
            stream += ws->elemStride;
        } while (ws->elemCount-- > 0);
    }
    ws->preXformWrite = (u8*)poly;
    return stream;
}

u32* gpStreamPrimGt4PreXform(TmdScratchModelBlock* ws, s32 flags, u32* stream)
{
    POLY_GT4* poly;

    poly = (POLY_GT4*)ws->preXformWrite;
    if (ws->elemCount-- > 0) {
        do {
            PRIM_UV_CLUT_WORD(poly)  = stream[2];
            PRIM_UV_TPAGE_WORD(poly) = stream[3];
            *(u16*)&poly->u2         = (u16)stream[4];
            *(u16*)&poly->u3         = ((u16*)&stream[4])[1];
            poly->tpage             += ws->tpage;
            poly->clut              += ws->clut;
            poly++;
            stream += ws->elemStride;
        } while (ws->elemCount-- > 0);
    }
    ws->preXformWrite = (u8*)poly;
    return stream;
}

u32* gpStreamPrimF4PreXform(TmdScratchModelBlock* ws, s32 flags, u32* stream)
{
    POLY_F4* poly;
    s32      color;

    poly = (POLY_F4*)ws->preXformWrite;
    if (ws->elemCount-- > 0) {
        do {
            color = stream[2];
            setlen(poly, 5);
            PRIM_COLOR_WORD(poly, 0) = color;
            setcode(poly, 0x28);
            poly++;
            stream += ws->elemStride;
        } while (ws->elemCount-- > 0);
    }
    ws->preXformWrite = (u8*)poly;
    return stream;
}

u32* gpStreamPrimF3PreXform(TmdScratchModelBlock* ws, s32 flags, u32* stream)
{
    POLY_F3* poly;
    s32      color;

    poly = (POLY_F3*)ws->preXformWrite;
    if (ws->elemCount-- > 0) {
        do {
            color = stream[2];
            setlen(poly, 4);
            PRIM_COLOR_WORD(poly, 0) = color;
            setcode(poly, 0x20);
            poly++;
            stream += ws->elemStride;
        } while (ws->elemCount-- > 0);
    }
    ws->preXformWrite = (u8*)poly;
    return stream;
}

u32* gpStreamPrimGt3(TmdScratchModelBlock* ws, s32 flags, u32* stream)
{
    POLY_GT3* poly;

    poly = (POLY_GT3*)ws->primWrite;
    if (ws->elemCount-- > 0) {
        do {
            PRIM_UV_CLUT_WORD(poly)  = stream[3];
            PRIM_UV_TPAGE_WORD(poly) = stream[4];
            *(u16*)&poly->u2         = (u16)stream[5];
            poly->tpage             += ws->tpage;
            poly->clut              += ws->clut;
            poly++;
            stream += ws->elemStride;
        } while (ws->elemCount-- > 0);
    }
    ws->primWrite = (u8*)poly;
    return stream;
}

u32* gpStreamPrimGt4(TmdScratchModelBlock* ws, s32 flags, u32* stream)
{
    POLY_GT4* poly;

    poly = (POLY_GT4*)ws->primWrite;
    if (ws->elemCount-- > 0) {
        do {
            PRIM_UV_CLUT_WORD(poly)  = stream[4];
            PRIM_UV_TPAGE_WORD(poly) = stream[5];
            *(u16*)&poly->u2         = (u16)stream[6];
            *(u16*)&poly->u3         = ((u16*)&stream[6])[1];
            poly->tpage             += ws->tpage;
            poly->clut              += ws->clut;
            poly++;
            stream += ws->elemStride;
        } while (ws->elemCount-- > 0);
    }
    ws->primWrite = (u8*)poly;
    return stream;
}

u32* gpStreamPrimGt3ElemColor(TmdScratchModelBlock* ws, s32 flags, u32* stream)
{
    POLY_GT3* poly;

    poly = (POLY_GT3*)ws->primWrite;
    if (ws->elemCount-- > 0) {
        do {
            PRIM_UV_CLUT_WORD(poly)  = stream[4];
            PRIM_UV_TPAGE_WORD(poly) = stream[5];
            *(u16*)&poly->u2         = (u16)stream[6];
            poly->tpage             += ws->tpage;
            poly->clut              += ws->clut;
            poly++;
            stream += ws->elemStride;
        } while (ws->elemCount-- > 0);
    }
    ws->primWrite = (u8*)poly;
    return stream;
}

u32* gpStreamPrimGt3CornerColors(TmdScratchModelBlock* ws, s32 flags, u32* stream)
{
    POLY_GT3* poly;

    poly = (POLY_GT3*)ws->primWrite;
    if (ws->elemCount-- > 0) {
        do {
            PRIM_UV_CLUT_WORD(poly)  = stream[6];
            PRIM_UV_TPAGE_WORD(poly) = stream[7];
            *(u16*)&poly->u2         = (u16)stream[8];
            poly->tpage             += ws->tpage;
            poly->clut              += ws->clut;
            poly++;
            stream += ws->elemStride;
        } while (ws->elemCount-- > 0);
    }
    ws->primWrite = (u8*)poly;
    return stream;
}

u32* gpStreamPrimGt4ElemColor(TmdScratchModelBlock* ws, s32 flags, u32* stream)
{
    POLY_GT4* poly;

    poly = (POLY_GT4*)ws->primWrite;
    if (ws->elemCount-- > 0) {
        do {
            PRIM_UV_CLUT_WORD(poly)  = stream[5];
            PRIM_UV_TPAGE_WORD(poly) = stream[6];
            *(u16*)&poly->u2         = (u16)stream[7];
            *(u16*)&poly->u3         = ((u16*)&stream[7])[1];
            poly->tpage             += ws->tpage;
            poly->clut              += ws->clut;
            poly++;
            stream += ws->elemStride;
        } while (ws->elemCount-- > 0);
    }
    ws->primWrite = (u8*)poly;
    return stream;
}

u32* gpStreamPrimGt4CornerColors(TmdScratchModelBlock* ws, s32 flags, u32* stream)
{
    POLY_GT4* poly;

    poly = (POLY_GT4*)ws->primWrite;
    if (ws->elemCount-- > 0) {
        do {
            PRIM_UV_CLUT_WORD(poly)  = stream[8];
            PRIM_UV_TPAGE_WORD(poly) = stream[9];
            *(u16*)&poly->u2         = (u16)stream[10];
            *(u16*)&poly->u3         = ((u16*)&stream[10])[1];
            poly->tpage             += ws->tpage;
            poly->clut              += ws->clut;
            poly++;
            stream += ws->elemStride;
        } while (ws->elemCount-- > 0);
    }
    ws->primWrite = (u8*)poly;
    return stream;
}

u32* gpStreamPrimGt3OneNormal(TmdScratchModelBlock* ws, s32 flags, u32* stream)
{
    POLY_GT3* poly;

    poly = (POLY_GT3*)ws->primWrite;
    if (ws->elemCount-- > 0) {
        do {
            PRIM_UV_CLUT_WORD(poly)  = stream[2];
            PRIM_UV_TPAGE_WORD(poly) = stream[3];
            *(u16*)&poly->u2         = (u16)stream[4];
            poly->tpage             += ws->tpage;
            poly->clut              += ws->clut;
            poly++;
            stream += ws->elemStride;
        } while (ws->elemCount-- > 0);
    }
    ws->primWrite = (u8*)poly;
    return stream;
}

u32* gpStreamPrimGt4OneNormal(TmdScratchModelBlock* ws, s32 flags, u32* stream)
{
    POLY_GT4* poly;

    poly = (POLY_GT4*)ws->primWrite;
    if (ws->elemCount-- > 0) {
        do {
            PRIM_UV_CLUT_WORD(poly)  = stream[3];
            PRIM_UV_TPAGE_WORD(poly) = stream[4];
            *(u16*)&poly->u2         = (u16)stream[5];
            *(u16*)&poly->u3         = ((u16*)&stream[5])[1];
            poly->tpage             += ws->tpage;
            poly->clut              += ws->clut;
            poly++;
            stream += ws->elemStride;
        } while (ws->elemCount-- > 0);
    }
    ws->primWrite = (u8*)poly;
    return stream;
}

u32* gpStreamPrimGt4Unlit(TmdScratchModelBlock* ws, s32 flags, u32* stream)
{
    POLY_GT4* poly;
    s32       color;

    poly = (POLY_GT4*)ws->primWrite;
    if (ws->elemCount-- > 0) {
        do {
            PRIM_COLOR_WORD(poly, 0) = stream[2];
            PRIM_COLOR_WORD(poly, 1) = stream[3];
            PRIM_COLOR_WORD(poly, 2) = stream[4];
            color                    = stream[5];
            setlen(poly, 12);
            setcode(poly, 0x3E);
            PRIM_COLOR_WORD(poly, 3) = color;
            PRIM_UV_CLUT_WORD(poly)  = stream[6];
            PRIM_UV_TPAGE_WORD(poly) = stream[7];
            *(u16*)&poly->u2         = (u16)stream[8];
            *(u16*)&poly->u3         = ((u16*)&stream[8])[1];
            poly->tpage             += ws->tpage;
            poly->clut              += ws->clut;
            poly++;
            stream += ws->elemStride;
        } while (ws->elemCount-- > 0);
    }
    ws->primWrite = (u8*)poly;
    return stream;
}

u32* gpStreamPrimFt3(TmdScratchModelBlock* ws, s32 flags, u32* stream)
{
    POLY_FT3* poly;

    poly = (POLY_FT3*)ws->primWrite;
    if (ws->elemCount-- > 0) {
        do {
            PRIM_UV_CLUT_WORD(poly)  = stream[2];
            PRIM_UV_TPAGE_WORD(poly) = stream[3];
            *(u16*)&poly->u2         = (u16)stream[4];
            poly->tpage             += ws->tpage;
            poly->clut              += ws->clut;
            poly++;
            stream += ws->elemStride;
        } while (ws->elemCount-- > 0);
    }
    ws->primWrite = (u8*)poly;
    return stream;
}

u32* gpStreamPrimFt4(TmdScratchModelBlock* ws, s32 flags, u32* stream)
{
    POLY_FT4* poly;

    poly = (POLY_FT4*)ws->primWrite;
    if (ws->elemCount-- > 0) {
        do {
            PRIM_UV_CLUT_WORD(poly)  = stream[2];
            PRIM_UV_TPAGE_WORD(poly) = stream[3];
            *(u16*)&poly->u2         = (u16)stream[4];
            *(u16*)&poly->u3         = ((u16*)&stream[4])[1];
            poly->tpage             += ws->tpage;
            poly->clut              += ws->clut;
            poly++;
            stream += ws->elemStride;
        } while (ws->elemCount-- > 0);
    }
    ws->primWrite = (u8*)poly;
    return stream;
}

u32* gpStreamPrimF4(TmdScratchModelBlock* ws, s32 flags, u32* stream)
{
    POLY_F4* poly;
    s32      color;

    poly = (POLY_F4*)ws->primWrite;
    if (ws->elemCount-- > 0) {
        do {
            color = stream[2];
            setlen(poly, 5);
            PRIM_COLOR_WORD(poly, 0) = color;
            setcode(poly, 0x28);
            poly++;
            stream += ws->elemStride;
        } while (ws->elemCount-- > 0);
    }
    ws->primWrite = (u8*)poly;
    return stream;
}

u32* gpStreamPrimF3(TmdScratchModelBlock* ws, s32 flags, u32* stream)
{
    POLY_F3* poly;
    s32      color;

    poly = (POLY_F3*)ws->primWrite;
    if (ws->elemCount-- > 0) {
        do {
            color = stream[2];
            setlen(poly, 4);
            PRIM_COLOR_WORD(poly, 0) = color;
            setcode(poly, 0x20);
            poly++;
            stream += ws->elemStride;
        } while (ws->elemCount-- > 0);
    }
    ws->primWrite = (u8*)poly;
    return stream;
}

u32* gpStreamPrimGt3OffsetLayer(TmdScratchModelBlock* ws, s32 flags, u32* stream)
{
    POLY_GT3* poly;
    s32       tpage;
    s32       tmp;

    poly = (POLY_GT3*)ws->primWrite;
    if (ws->elemCount-- > 0) {
        do {
            PRIM_UV_CLUT_WORD(poly)  = stream[3];
            PRIM_UV_TPAGE_WORD(poly) = stream[4];
            *(u16*)&poly->u2         = (u16)stream[5];
            poly->tpage             += (s8)ws->obj->tpageOffset;
            tmp                      = ws->obj->clutOffset;
            tpage                    = poly->tpage;
            tpage                   |= 0x20;
            poly->tpage              = tpage;
            poly->clut              += (s8)tmp << 6;
            poly++;
            PRIM_UV_CLUT_WORD(poly)  = stream[3];
            PRIM_UV_TPAGE_WORD(poly) = stream[4];
            *(u16*)&poly->u2         = (u16)stream[5];
            poly->tpage             += ws->tpage;
            poly->clut              += ws->clut;
            poly++;
            stream += ws->elemStride;
        } while (ws->elemCount-- > 0);
    }
    ws->primWrite = (u8*)poly;
    return stream;
}

u32* gpStreamPrimGt3Base(TmdScratchModelBlock* ws, s32 flags, u32* stream)
{
    POLY_GT3* poly;

    poly = (POLY_GT3*)ws->primWrite;
    if (ws->elemCount-- > 0) {
        do {
            poly++;
            PRIM_UV_CLUT_WORD(poly)  = stream[3];
            PRIM_UV_TPAGE_WORD(poly) = stream[4];
            *(u16*)&poly->u2         = (u16)stream[5];
            poly->tpage             += ws->tpage;
            poly->clut              += ws->clut;
            poly++;
            stream += ws->elemStride;
        } while (ws->elemCount-- > 0);
    }
    ws->primWrite = (u8*)poly;
    return stream;
}

u32* gpStreamPrimGt4OffsetLayer(TmdScratchModelBlock* ws, s32 flags, u32* stream)
{
    POLY_GT4* poly;
    s32       tpage;
    s32       tmp;

    poly = (POLY_GT4*)ws->primWrite;
    if (ws->elemCount-- > 0) {
        do {
            PRIM_UV_CLUT_WORD(poly)  = stream[4];
            PRIM_UV_TPAGE_WORD(poly) = stream[5];
            *(u16*)&poly->u2         = (u16)stream[6];
            *(u16*)&poly->u3         = ((u16*)&stream[6])[1];
            poly->tpage             += (s8)ws->obj->tpageOffset;
            tmp                      = ws->obj->clutOffset;
            tpage                    = poly->tpage;
            tpage                   |= 0x20;
            poly->tpage              = tpage;
            poly->clut              += (s8)tmp << 6;
            poly++;
            PRIM_UV_CLUT_WORD(poly)  = stream[4];
            PRIM_UV_TPAGE_WORD(poly) = stream[5];
            *(u16*)&poly->u2         = (u16)stream[6];
            *(u16*)&poly->u3         = ((u16*)&stream[6])[1];
            poly->tpage             += ws->tpage;
            poly->clut              += ws->clut;
            poly++;
            stream += ws->elemStride;
        } while (ws->elemCount-- > 0);
    }
    ws->primWrite = (u8*)poly;
    return stream;
}

u32* gpStreamPrimGt4Base(TmdScratchModelBlock* ws, s32 flags, u32* stream)
{
    POLY_GT4* poly;

    poly = (POLY_GT4*)ws->primWrite;
    if (ws->elemCount-- > 0) {
        do {
            poly++;
            PRIM_UV_CLUT_WORD(poly)  = stream[4];
            PRIM_UV_TPAGE_WORD(poly) = stream[5];
            *(u16*)&poly->u2         = (u16)stream[6];
            *(u16*)&poly->u3         = ((u16*)&stream[6])[1];
            poly->tpage             += ws->tpage;
            poly->clut              += ws->clut;
            poly++;
            stream += ws->elemStride;
        } while (ws->elemCount-- > 0);
    }
    ws->primWrite = (u8*)poly;
    return stream;
}

u32* gpStreamPrimGt3PreXformFixedLayer(TmdScratchModelBlock* ws, s32 flags, u32* stream)
{
    POLY_GT3* poly;

    poly = (POLY_GT3*)ws->preXformWrite;
    if (ws->elemCount-- > 0) {
        do {
            poly->tpage = 0x3F;
            poly->clut  = 0x3C10;
            poly++;
            PRIM_UV_CLUT_WORD(poly)  = stream[2];
            PRIM_UV_TPAGE_WORD(poly) = stream[3];
            *(u16*)&poly->u2         = (u16)stream[4];
            poly->tpage             += ws->tpage;
            poly->clut              += ws->clut;
            poly++;
            stream += ws->elemStride;
        } while (ws->elemCount-- > 0);
    }
    ws->preXformWrite = (u8*)poly;
    return stream;
}

u32* gpStreamPrimGt4PreXformLayer(TmdScratchModelBlock* ws, s32 flags, u32* stream)
{
    POLY_GT4* poly;

    poly = (POLY_GT4*)ws->preXformWrite;
    if (ws->elemCount-- > 0) {
        do {
            poly->tpage = 0x3F;
            poly->clut  = 0x3C10;
            poly++;
            PRIM_UV_CLUT_WORD(poly)  = stream[2];
            PRIM_UV_TPAGE_WORD(poly) = stream[3];
            *(u16*)&poly->u2         = (u16)stream[4];
            *(u16*)&poly->u3         = ((u16*)&stream[4])[1];
            poly->tpage             += ws->tpage;
            poly->clut              += ws->clut;
            poly++;
            stream += ws->elemStride;
        } while (ws->elemCount-- > 0);
    }
    ws->preXformWrite = (u8*)poly;
    return stream;
}

u32* gpStreamPrimGt3PreXformOffsetLayer(TmdScratchModelBlock* ws, s32 flags, u32* stream)
{
    POLY_GT3* poly;
    s32       tpage;
    s32       tmp;

    poly = (POLY_GT3*)ws->preXformWrite;
    if (ws->elemCount-- > 0) {
        do {
            PRIM_UV_CLUT_WORD(poly)  = stream[2];
            PRIM_UV_TPAGE_WORD(poly) = stream[3];
            *(u16*)&poly->u2         = (u16)stream[4];
            poly->tpage             += (s8)ws->obj->tpageOffset;
            tmp                      = ws->obj->clutOffset;
            tpage                    = poly->tpage;
            tpage                   |= 0x20;
            poly->tpage              = tpage;
            poly->clut              += (s8)tmp << 6;
            poly++;
            PRIM_UV_CLUT_WORD(poly)  = stream[2];
            PRIM_UV_TPAGE_WORD(poly) = stream[3];
            *(u16*)&poly->u2         = (u16)stream[4];
            poly->tpage             += ws->tpage;
            poly->clut              += ws->clut;
            poly++;
            stream += ws->elemStride;
        } while (ws->elemCount-- > 0);
    }
    ws->preXformWrite = (u8*)poly;
    return stream;
}

u32* gpStreamPrimGt4PreXformOffsetLayer(TmdScratchModelBlock* ws, s32 flags, u32* stream)
{
    POLY_GT4* poly;
    s32       tpage;
    s32       tmp;

    poly = (POLY_GT4*)ws->preXformWrite;
    if (ws->elemCount-- > 0) {
        do {
            PRIM_UV_CLUT_WORD(poly)  = stream[2];
            PRIM_UV_TPAGE_WORD(poly) = stream[3];
            *(u16*)&poly->u2         = (u16)stream[4];
            *(u16*)&poly->u3         = ((u16*)&stream[4])[1];
            poly->tpage             += (s8)ws->obj->tpageOffset;
            tmp                      = ws->obj->clutOffset;
            tpage                    = poly->tpage;
            tpage                   |= 0x20;
            poly->tpage              = tpage;
            poly->clut              += (s8)tmp << 6;
            poly++;
            PRIM_UV_CLUT_WORD(poly)  = stream[2];
            PRIM_UV_TPAGE_WORD(poly) = stream[3];
            *(u16*)&poly->u2         = (u16)stream[4];
            *(u16*)&poly->u3         = ((u16*)&stream[4])[1];
            poly->tpage             += ws->tpage;
            poly->clut              += ws->clut;
            poly++;
            stream += ws->elemStride;
        } while (ws->elemCount-- > 0);
    }
    ws->preXformWrite = (u8*)poly;
    return stream;
}

u32* gpStreamPrimG4(TmdScratchModelBlock* ws, s32 flags, u32* stream)
{
    u8* prims;
    s32 stride;

    prims = ws->primWrite;
    if (ws->elemCount-- > 0) {
        stride = ws->elemStride;
        do {
            stream += stride;
            prims  += 0x24;
        } while (ws->elemCount-- > 0);
    }
    ws->primWrite = prims;
    return stream;
}

u32* gpStreamPrimG3(TmdScratchModelBlock* ws, s32 flags, u32* stream)
{
    u8* prims;
    s32 stride;

    prims = ws->primWrite;
    if (ws->elemCount-- > 0) {
        stride = ws->elemStride;
        do {
            stream += stride;
            prims  += 0x1C;
        } while (ws->elemCount-- > 0);
    }
    ws->primWrite = prims;
    return stream;
}

static u32* func_8009FCDC(TmdScratchModelBlock* arg0, s32 arg1, u32* arg2)
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

static u32* func_8009FD28(TmdScratchModelBlock* arg0, s32 arg1, u32* arg2)
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

void Gp_ApplyPadReplay(s32 arg0, PadScratch* arg1)
{
    u16 temp_v0;
    u16 temp_v1;
    s32 offset;

    if (arg0 == 1) {
        func_807150F8(1);
        return;
    }

    offset = (s32)Gp_ReplayCursor - (s32)D_8005C374;
    if (gDisplayState.demoScene == 0x10) {
        offset = (s32)Gp_ReplayCursor + 0x7F9FFF00;
    }
    if (offset <= 0x17FDF) {
        if (GameMain_HaltFlags != 0) {
            arg1->buttons = Gp_ReplayButtons;
            return;
        }
        temp_v1 = Gp_ReplayCursor[0];
        if (temp_v1 != Gp_ReplayButtons) {
            Gp_ReplayButtons    = temp_v1;
            Gp_ReplayFramesLeft = Gp_ReplayCursor[1];
        }
        if (arg1->buttons & 0x800) {
            arg1->buttons        = Gp_ReplayButtons | 0x800;
            Wip_SysFlags.field_4 = 1;
        } else {
            arg1->buttons = Gp_ReplayButtons;
        }
        temp_v0             = Gp_ReplayFramesLeft - 1;
        Gp_ReplayFramesLeft = temp_v0;
        if (!(temp_v0 & 0xFFFF)) {
            u16* next = Gp_ReplayCursor + 2;

            Gp_ReplayButtons = 0xFFFF;
            Gp_ReplayCursor  = next;
            if (*next == 0xFFFF) {
                Wip_SysFlags.field_4    = 0;
                Pad_RemapState->field_8 = 0;
            }
        }
    } else {
        Pad_RemapState->field_8 = 0;
    }
}

static void Gp_InitPlayClock(Task* task)
{
    GpIdMap30*    rec;
    DisplayState* ds;

    Gp_UpdatePadInput();
    gGameSession->field_5E = 1;
    rec                    = memCalloc(0x30, 0);
    if (rec == NULL) {
        taskKill(task);
        return;
    }
    Gp_ResetHudFx(&rec->extra);
    GameMain_SetFrameTiming(1);
    task->work   = (TaskIdMap*)rec;
    rec->field_0 = Mc_SaveData[0].playTime / 60;
    rec->field_4 = Mc_SaveData[0].playTime % 60;
    ds           = &gDisplayState;
    rec->field_8 = ds->gameTick;
    func_800B25B0();
    if (ds->demoScene != 0) {
        srand(1);
        ds->animFrame            = 0;
        gDisplayState.frameCount = 0;
        Gp_LcgState              = 0;
        ds->gameTick             = 0;
        ds->loopCount            = 0;
        ds->vsyncCount           = 0;
        ds->field_10             = 0;
        if (ds->demoScene == 0x10) {
            Gp_ReplayCursor = (u16*)0x80600E4C;
        } else {
            Gp_ReplayCursor = (u16*)((u8*)D_8005C374 + 0xD4C);
        }
        Gp_ReplayButtons        = 0xFFFF;
        Gp_ReplayFramesLeft     = 1;
        Pad_RemapState->field_8 = -1;
    } else if (Pad_RemapState->field_9 == 1) {
        func_80715198();
    }
    task->state++;
}

static void Gp_TickPlayClock(Task* task)
{
    TextDrawReq   req;
    u8            buf[0x20];
    GpIdMap30*    rec;
    McSaveData*   save;
    PlayerStatus* cfg;
    GameSession*  session;
    s32           one;
    s32           temp;
    s32           companion;

    rec = (GpIdMap30*)task->work;
    cfg = &Player_Status;
    Gp_UpdatePadInput();

    temp         = gDisplayState.gameTick;
    D_8005ED68  += temp - rec->field_8;
    rec->field_8 = temp;
    if (D_8005ED68 >= 0xE10) {
        McSaveData* p;
        D_8005ED68 -= 0xE10;
        p           = &Mc_SaveData[0];
        if (p->playTime <= 0xEA5E) {
            p->playTime++;
            rec->field_4++;
            if (rec->field_4 >= 0x3C) {
                rec->field_4 -= 0x3C;
                rec->field_0++;
            }
        } else {
            p->playTime  = 0xEA5F;
            rec->field_0 = 0x3E7;
            rec->field_4 = 0x3B;
        }
    }

    save = &Mc_SaveData[0];
    one  = 1;
    if (save->demoScene == one) {
        req.x          = -0x96;
        req.y          = 0x64;
        req.otIndex    = 4;
        req.field_8    = 0x502008;
        req.glyphTable = 2;
        req.centerMode = 0;
        req.field_E    = one;
        func_8002E53C(&req, Text_ItoaUnsigned(buf, rec->field_0));
        func_8002E53C(&req, ":");
        func_8002E53C(&req, func_8002F44C(buf, rec->field_4, 2));
        func_8002E53C(&req, "'");
        func_8002E53C(&req, func_8002F44C(buf, D_8005ED68 / 60, 2));
        Pad_CheckButtons(one, one, 0x100);
    }

    if (gGameSession->suppressDeathChecks == 0) {
        if (cfg->hp > 0) {
            companion = save->companionType;
            if (companion == one) {
                if (save->companionHp <= 0) {
                    goto block_hp;
                }
            }
            if (companion != 3) {
                goto block_normal;
            }
            if (save->companionHp > 0) {
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
        Gp_StateC08.field_3 = 0;
        func_800A7DE0();
        Gp_PulseState1C80();
        session = gGameSession;
        if (session->restartMode != 3) {
            Gp_LcgState           = Gp_LcgState * 5 + 0x71357911;
            session->deathVariant = ((u32)Gp_LcgState >> 16 & 1) + 1;
            SndEvt_EnqueueType7(0x20000000, 8);
            SndBank_SetEnableFlags(0, 0x20000000);
            CdCmd_EnqueueLoadFile(9, ((u8)gGameSession->deathVariant + 0x1D) & 0xFF, 3);
        }

    block_companion: {
        McSaveData* p;
        p = &Mc_SaveData[0];
        if (p->companionHp <= 0) {
            if (gGameSession->eventState != 0) {
                p->companionHp = 1;
                return;
            }
            Gp_StateC08.field_3 = 0;
            func_800A7DE0();
            Gp_PulseState1C80();
            companion = p->companionType;
            if (companion == 1) {
                gGameSession->restartMode  = companion;
                Gp_LcgState                = Gp_LcgState * 5 + 0x71357911;
                gGameSession->deathVariant = ((u32)Gp_LcgState >> 16 & 1) + 1;
                SndEvt_EnqueueType7(0x20000000, 8);
                SndBank_SetEnableFlags(0, 0x20000000);
                CdCmd_EnqueueLoadFile(9, ((u8)gGameSession->deathVariant + 0x20) & 0xFF, 3);
                companion = p->companionType;
            }
            if (companion == 3) {
                gGameSession->restartMode = 4;
            }
        }
    }
        Display_AcquireRef();
        task->killCountdown  = gGameSession->deathRestartDelay;
        Wip_SysFlags.field_1 = 1;
        task->state++;
        return;
    }

block_normal:
    if (gGameSession->restartMode == 0xFF) {
        Display_AcquireRef();
        gGameSession->deathVariant = 1;
        task->state++;
    } else {
        Gp_HudTask(&rec->extra);
    }
}

static void Gp_RestartSessionTask(Task* arg0)
{
    RECT          rect;
    DisplayState* ds;
    GameSession*  session;
    CdCmdQueue*   queue;
    s32           flag;

    queue = &CdCmd_Queue;
    Gp_StartAreaBgm(&arg0->killCountdown);
    arg0->spawnArg1 += 0xA;
    if (arg0->spawnArg1 < 0x100) {
        return;
    }
    if (gGameSession->restartMode != 3) {
        SetDispMask(0);
    }
    SndEvt_EnqueueType2(0, 8);
    SndEvt_EnqueueType7(0x80000000, 0x78);
    SndEvt_EnqueueType7(0x60010001, 0x78);
    flag            = 0xFF;
    arg0->spawnArg1 = flag;
    Pad_SetCooldown(0);
    Game_ClearPtrSlots();
    ds               = &gDisplayState;
    ds->stopTaskWalk = 1;
    Task_ResetDefaultList();
    Gpu_ClearOTag(0);
    Gpu_ClearOTag(1);
    Mem_Init();
    CdCmd_ActivatePhase1();
    session          = gGameSession;
    queue->field_20A = 1;
    if (session->restartMode != 3) {
        rect.w = 0x140;
        rect.y = 0;
        rect.x = 0;
        rect.h = 0x200;
        ClearImage(&rect, 0, 0, 0);
        DrawSync(0);
        ds->at100.flags.imageSource = 0;
    }
    memset(&gGameSession->at4.loc.view, 0, 8);
    Mem_ConfigureAuxHeap(0, 0);
    if (gGameSession->restartMode == flag) {
        Gpu_PrimHeapSize   = 0xB000;
        GActiveAuxHeapSize = 0x30000;
        Gpu_PrimHeapBase   = (size_t)((u8*)Fs_ImgBuffers - 0x35800);
        gMemActiveAuxHeap  = (u8*)Fs_ImgBuffers - 0xA800;
    }
    Mem_Init();
    Mem_InitAux();
    if (gGameSession->restartMode != flag) {
        CdCmd_SetupMdecBuffers();
    }
    Task_SpawnFromTable(&D_8010D1FC, 0, 0, 0);
}

void Gp_EndingTask(Task* arg0)
{
    GameSession* session;
    GpEndWork*   work;
    GpSndParam*  pair;

    if (arg0->state == 0) {
        work                = arg0->spawnArg2;
        work->field_4       = 1;
        arg0->killCountdown = 0x1E;
        if ((GP_LOC_WORD(gGameSession->at4.loc) & GP_LOC_STAGE_AREA) == GP_LOC_KEY(4, 48, 0, 0)) {
            arg0->killCountdown = 0x5A;
        }
        SndEvt_EnqueueType6(0xB, 0, 0);
        Gp_SpawnScript18((s32)&D_80114A24, (s32)&D_80114A34);
        Gp_SetCurAreaFlag4();
    } else if (arg0->state == 1) {
        session = gGameSession;
        if (!(session->flowFlags & 1)) {
            pair          = (GpSndParam*)&D_8007A39C;
            pair->field_0 = 0;
            pair->field_2 = 0;
            if ((session->flowFlags & 4) == 0) {
                Task_SpawnFromTable(&D_80062774, 0, 2, 0);
            } else {
                Task_SpawnFromTable(&D_80062774, 0, 3, 0);
            }
        } else {
            gStageMusicLoadState = 0xFF;
        }
    } else {
        goto countdown;
    }
    arg0->state++;
countdown:
    arg0->killCountdown--;
    if (arg0->killCountdown <= 0) {
        if (gStageMusicLoadState == 0xFF) {
            taskKill(arg0);
            Stage_SetEndingFlag();
        }
    }
}

const TaskFuncTable6 Gp_PlayClockStates = { {
    Gp_InitPlayClock,
    Gp_TickPlayClock,
    Gp_PlayClockState2,
    Gp_PlayClockState3,
    Gp_RestartSessionTask,
    taskKill,
} };

static const char Gp_StrBattleResult[] = "Battle Result";
static const char Gp_StrTotal[]        = "Total";
const char        Gp_StrHP[]           = "HP";
const char        Gp_StrMP[]           = "MP";
static const char Gp_StrBP[]           = "BP";
static const char Gp_StrEXP[]          = "EXP";
const char        Gp_StrItem[]         = "Item";

void func_800A087C(Task* arg0)
{
    u8            buf[0x20];
    TextDrawReq   req1;
    TextDrawReq   req2;
    TextDrawReq   req3;
    TextDrawReq   req4;
    TextDrawReq   req5;
    TextDrawReq   req6;
    TextDrawReq   req7;
    TextDrawReq   req8;
    TextDrawReq   req9;
    TextDrawReq   req10;
    TextDrawReq   req11;
    UiObject*     obj;
    PlayerStatus* cfg;
    s32           col;
    s32           step;
    s32           color;
    s32           color2;
    s32           y;
    s32           top;
    s32           h;
    s32           tx;
    u16           add;

    cfg = &Player_Status;
    obj = arg0->spawnArg2;
    if (arg0->state == 0) {
        if (arg0->spawnArg1 == 0) {
            D_80114BE2 = 0;
            D_80114BE4 = 0;
            D_80114BDC = Gp_StateF0.field_C;
            D_80114BDE = Gp_StateF0.field_8;
            D_80114BE0 = Gp_StateF0.field_10;
            if (func_800B9D80(0x8000) != 0) {
                D_80114BE4 = ((u32)(Gp_StateF0.field_10 - 1) >> 2) + 1;
                if (D_80114BE4 >= 100) {
                    D_80114BE4 = 99;
                }
            }
            if (func_800B9D80(0x1000) != 0) {
                add        = (u16)Gp_StateF0.field_10;
                D_80114BE2 = add;
                cfg->hp   += add;
                if (cfg->hp >= cfg->hpMax) {
                    cfg->hp = cfg->hpMax;
                }
            }
        } else {
            D_80114BDE = 0;
            D_80114BDC = -10;
            D_80114BE0 = 1;
            D_80114BE2 = 0;
            D_80114BE4 = 0;
        }
        cfg->bp += D_80114BDC;
        if (cfg->bp > 999999) {
            cfg->bp = 999999;
        }
        if (cfg->bp < 0) {
            cfg->bp = 0;
        }
        cfg->exp += D_80114BDE;
        if (cfg->exp > 999999) {
            cfg->exp = 999999;
        }
        cfg->mp += D_80114BE0 + D_80114BE4;
        if (cfg->mp > cfg->mpMax) {
            cfg->mp = cfg->mpMax;
        }
        arg0->killCountdown = 0;
        arg0->state++;
    }

    Ui_DrawTitle((UiPanel*)obj, (char*)Gp_StrBattleResult);
    if (arg0->killCountdown < 500) {
        arg0->killCountdown++;
    }

    col   = 0;
    step  = 0xE;
    color = 0x606060;

    top             = (s16)obj->field_18;
    tx              = obj->baseX - 4;
    req1.x          = (s16)obj->field_1E + tx;
    req1.y          = obj->baseY + top + 5;
    req1.otIndex    = obj->drawOrder + 1;
    req1.field_8    = color;
    req1.glyphTable = 5;
    req1.centerMode = 2;
    req1.field_E    = 1;
    func_8002E53C(&req1, Gp_StrTotal);

    Ui_DrawHBar((UiPanel*)obj, obj->field_1C, (s16)obj->field_1E, top + 9);
    Ui_DrawVBar((UiPanel*)obj, top + 0xC, (s16)obj->field_1A, 0x1C);

    h = (s16)obj->field_1A;
    y = h - 2;
    if (D_80114BE2 > 0) {
        y               = h - 1;
        req2.x          = obj->field_1C + (obj->baseX + 6);
        req2.y          = (s16)(obj->baseY - 2) + y;
        req2.otIndex    = obj->drawOrder + 1;
        req2.field_8    = color;
        req2.glyphTable = 5;
        req2.centerMode = 0;
        req2.field_E    = 1;
        func_8002E53C(&req2, Gp_StrHP);
        step = 0xA;

        req3.x          = obj->baseX + col;
        req3.y          = obj->baseY + y;
        req3.otIndex    = obj->drawOrder + 1;
        req3.field_8    = color;
        req3.glyphTable = 0;
        req3.centerMode = 2;
        req3.field_E    = 3;
        func_8002E53C(&req3, Text_ItoaUnsigned(buf, D_80114BE2));
        y -= 0xA;
    }

    req4.x          = obj->field_1C + (obj->baseX + 6);
    req4.y          = (s16)(obj->baseY - 2) + y;
    req4.otIndex    = obj->drawOrder + 1;
    req4.field_8    = color;
    req4.glyphTable = 5;
    req4.centerMode = 0;
    req4.field_E    = 1;
    func_8002E53C(&req4, Gp_StrMP);

    req5.x          = obj->baseX + col;
    req5.y          = obj->baseY + y;
    req5.otIndex    = obj->drawOrder + 1;
    req5.field_8    = color;
    req5.glyphTable = 0;
    req5.centerMode = 2;
    req5.field_E    = 3;
    func_8002E53C(&req5, Text_ItoaUnsigned(buf, D_80114BE0));

    if (D_80114BE4 > 0) {
        buf[0] = '+';
        Text_ItoaUnsigned(&buf[1], D_80114BE4);
        req6.x          = obj->baseX + col;
        req6.y          = obj->baseY + y;
        req6.otIndex    = obj->drawOrder + 1;
        req6.field_8    = color;
        req6.glyphTable = 0;
        req6.centerMode = 0;
        req6.field_E    = 3;
        func_8002E53C(&req6, buf);
    }

    y              -= step;
    req6.x          = obj->field_1C + (obj->baseX + 6);
    req6.y          = (s16)(obj->baseY - 2) + y;
    req6.otIndex    = obj->drawOrder + 1;
    req6.field_8    = color;
    req6.glyphTable = 5;
    req6.centerMode = 0;
    req6.field_E    = 1;
    func_8002E53C(&req6, Gp_StrBP);

    if (D_80114BDC < 0) {
        req7.x          = obj->baseX + col;
        req7.y          = obj->baseY + y;
        req7.otIndex    = obj->drawOrder + 1;
        req7.field_8    = 0xD287F;
        req7.glyphTable = 0;
        req7.centerMode = 2;
        req7.field_E    = 3;
        func_8002E53C(&req7, Text_ItoaSigned(buf, D_80114BDC));
    } else {
        req7.x          = obj->baseX + col;
        req7.y          = obj->baseY + y;
        req7.otIndex    = obj->drawOrder + 1;
        req7.field_8    = color;
        req7.glyphTable = 0;
        req7.centerMode = 2;
        req7.field_E    = 3;
        func_8002E53C(&req7, Text_ItoaUnsigned(buf, D_80114BDC));
    }

    y              -= step;
    color2          = 0x606060;
    req7.x          = obj->field_1C + (obj->baseX + 6);
    req7.y          = (s16)(obj->baseY - 2) + y;
    req7.otIndex    = obj->drawOrder + 1;
    req7.field_8    = color2;
    req7.glyphTable = 5;
    req7.centerMode = 0;
    req7.field_E    = 1;
    func_8002E53C(&req7, Gp_StrEXP);

    req8.x          = obj->baseX + col;
    req8.y          = obj->baseY + y;
    req8.otIndex    = obj->drawOrder + 1;
    req8.field_8    = color2;
    req8.glyphTable = 0;
    req8.centerMode = 2;
    req8.field_E    = 3;
    func_8002E53C(&req8, Text_ItoaUnsigned(buf, D_80114BDE));

    y   = (s16)obj->field_1A - 2;
    col = (s16)obj->field_1E - 2;
    if (D_80114BE2 > 0) {
        y = (s16)obj->field_1A - 1;
        if (arg0->killCountdown >= 0x8D) {
            req9.x          = obj->baseX + col;
            req9.y          = obj->baseY + y;
            req9.otIndex    = obj->drawOrder + 1;
            req9.field_8    = color2;
            req9.glyphTable = 0;
            req9.centerMode = 2;
            req9.field_E    = 3;
            func_8002E53C(&req9, Text_ItoaUnsigned(buf, cfg->hp));
        }
        y -= step;
    }
    if (arg0->killCountdown >= 0x6F) {
        req9.x          = obj->baseX + col;
        req9.y          = obj->baseY + y;
        req9.otIndex    = obj->drawOrder + 1;
        req9.field_8    = 0x606060;
        req9.glyphTable = 0;
        req9.centerMode = 2;
        req9.field_E    = 3;
        func_8002E53C(&req9, Text_ItoaUnsigned(buf, cfg->mp));
    }
    y -= step;
    if (arg0->killCountdown >= 0x51) {
        req10.x          = obj->baseX + col;
        req10.y          = obj->baseY + y;
        req10.otIndex    = obj->drawOrder + 1;
        req10.field_8    = 0x606060;
        req10.glyphTable = 0;
        req10.centerMode = 2;
        req10.field_E    = 3;
        func_8002E53C(&req10, Text_ItoaUnsigned(buf, cfg->bp));
    }
    y -= step;
    if (arg0->killCountdown >= 0x33) {
        req11.x          = obj->baseX + col;
        req11.y          = obj->baseY + y;
        req11.otIndex    = obj->drawOrder + 1;
        req11.field_8    = 0x606060;
        req11.glyphTable = 0;
        req11.centerMode = 2;
        req11.field_E    = 3;
        func_8002E53C(&req11, Text_ItoaUnsigned(buf, cfg->exp));
    }

    if (obj->status == 1) {
        if (Pad_CheckButtons(0, 1, Pad_MaskConfirm | Pad_MaskCancel) != 0) {
            obj->field_2E = 6;
        }
    }
}

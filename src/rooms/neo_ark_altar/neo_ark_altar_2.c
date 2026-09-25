#include "common.h"

#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/inline_c.h>

#include "gameplay/3CD8.h"
#include "gameplay/3FB8.h"
#include "gameplay/D4.h"
#include "gameplay/gameplay.h"
#include "main/display.h"
#include "main/fs.h"
#include "main/gameflag.h"
#include "main/gameflow.h"
#include "main/gfx.h"
#include "main/mem.h"
#include "main/pad.h"
#include "main/session.h"
#include "main/sound.h"
#include "main/stream.h"
#include "main/task.h"
#include "rooms/neo_ark_altar.h"

/// One 12-byte altar floor tile: `x` / `z` are the tile's low corner in world
/// units, `w` / `d` its size along X and Z, and `id` the number the tile
/// carries. A table scanned by `id` ends at an entry whose `id` is -1.
typedef struct NeoArkAltarTile {
    /* 0x0 */ s16 x;
    /* 0x2 */ s16 z;
    /* 0x4 */ s16 w;
    /* 0x6 */ s16 d;
    /* 0x8 */ s16 id;
    /* 0xA */ u16 pad;
} NeoArkAltarTile;
STATIC_ASSERT_SIZEOF(NeoArkAltarTile, 0xC);

/// Per-task work block of the altar task, allocated zeroed as 0x10 bytes by
/// `func_neo_ark_altar_8017ED60` and parked in `Task::work`. `field_0` keeps
/// the task spawned from `D_neo_ark_altar_8017EFC0` once the altar sequence
/// completes. `field_8` is the tile the player currently stands on
/// (`func_neo_ark_altar_8017EC34` of the player coordinate), `field_6` the
/// value recorded on the previous frame, `field_C` the tile whose wall is
/// currently raised and `field_A` that wall's height; `field_E` carries the
/// new tile to `func_neo_ark_altar_8017E260`.
typedef struct NeoArkAltarWork {
    /* 0x0 */ Task* field_0;
    /* 0x4 */ u8    pad_4[0x2];
    /* 0x6 */ s16   field_6;
    /* 0x8 */ s16   field_8;
    /* 0xA */ s16   field_A;
    /* 0xC */ s16   field_C;
    /* 0xE */ s16   field_E;
} NeoArkAltarWork;
STATIC_ASSERT_SIZEOF(NeoArkAltarWork, 0x10);

extern s8 D_8007106B;

extern NeoArkAltarTile D_neo_ark_altar_8017F014[];

extern s16 D_neo_ark_altar_801800AC;
extern s16 D_neo_ark_altar_801800AE;
extern s16 D_neo_ark_altar_801800B0[];

extern s16 D_neo_ark_altar_8017F050[];
extern s16 D_neo_ark_altar_8017F068[];

extern NeoArkAltarTile D_neo_ark_altar_8017EFD8[];
extern GpAreaApplyRec  D_neo_ark_altar_8018007C[];

void func_neo_ark_altar_8017E658(SVECTOR* p0, SVECTOR* p1, SVECTOR* p2, SVECTOR* p3);
s16  func_neo_ark_altar_8017EC34(NeoArkAltarTile* table, s16 x, s16 z);
s16  func_neo_ark_altar_8017E260(Task* task);
void func_neo_ark_altar_8017E92C(s16 arg0, s32 arg1);

void func_neo_ark_altar_8017DA40(Task* arg0)
{
    u8          slotParam[4];
    GameLoc     key;
    s16         slot;
    CdCmdQueue* queue;
    Task*       task;

    task  = arg0;
    queue = &CdCmd_Queue;
    switch (task->state) {
        case 0:
            goto L_case0;
        case 1:
            goto L_case1;
        case 2:
            goto L_case2;
        case 3:
            goto L_case3;
        case 4:
            goto L_case4;
        case 5:
            goto L_case5;
    }
    return;

L_case0:
    SetDispMask(0);
    Mem_AllocAuxWithImages(1);
    goto advance;

L_case1:
    key = gGameSession->at4;
    if (task->spawnArg1 == 0) {
        key.loc.view = 0x64;
    } else if (task->spawnArg1 == 1) {
        key.loc.view = 0x65;
    } else {
        key.loc.view = 0x66;
    }
    slot = Stream_FindSlot(key.raw.data, 0, 0);
    {
        s32 cmd;
        s32 zero;
        u8* p;
        cmd  = 0x61;
        zero = 0;
        p    = slotParam;
        SOFT_TOUCH_REG4(cmd, zero, p, slot);
        slotParam[0] = slot;
        CdCmd_Enqueue(cmd, zero, p);
    }
    goto advance;

L_case2:
    if (queue->field_1FA == 0) {
        return;
    }
    SetDispMask(1);
    goto advance;

L_case3:
    if (CdCmd_IsIdle() & 0xFFFF) {
        SetDispMask(0);
        goto advance;
    }
    if (Pad_CheckFlag800() == 0) {
        return;
    }
    SetDispMask(0);
    CdCmd_ActivatePhase1();
    goto advance;

L_case4:
    if ((CdCmd_IsIdle() & 0xFFFF) == 0) {
        return;
    }
    Stream_ResetRestoreState();
advance:
    task->state = task->state + 1;
    return;

L_case5:
    if ((Stream_RestoreAfterLoad(0, 1) & 0xFFFF) == 0) {
        return;
    }
    taskKill(task);
    Display_ResetHeapWrapper();
}

/// Entry 0 of `D_neo_ark_altar_8017EFC0`: spawns that table's entry 1 (the
/// streaming task `func_neo_ark_altar_8017DA40`) with an ordering table,
/// passing on this task's `spawnArg1`, sets `D_8007106B`, calls
/// `Gp_SpawnViewTasks` and ends.
void func_neo_ark_altar_8017DBF0(Task* arg0)
{
    Display_SpawnWithOt(D_neo_ark_altar_8017EFC0, 1, arg0->spawnArg1, 0);
    D_8007106B = 1;
    Gp_SpawnViewTasks();
    taskKill(arg0);
}

/// Steps the altar's switch state `D_neo_ark_altar_801800AE` by one per call,
/// in the direction `arg0` (the 0xD9 game-flag nibble) selects: 0 sets
/// `field_4` of the second command in `rec[3]` and `rec[6]` and counts up
/// towards 6, 1 clears them and counts down towards 0; any other value, or a
/// state already at the end of its range, changes nothing. Each step rewrites
/// the six commands after the first in `rec[4]` so exactly one holds 0: index
/// `6 - s`, where `s` is the lower of the old and new states.
void func_neo_ark_altar_8017DC40(s32 arg0)
{
    GpAreaKey* sess;
    GpSprtRec* rec;
    GpSprtCmd* cmd;

    sess  = &gGameSession->at4.loc;
    rec   = Gp_SprtTables[sess->stage - 1][0].field_0[sess->area - 1];
    arg0 &= 0xFF;
    if (arg0 == 0) {
        cmd            = rec[3].field_4;
        cmd[1].field_4 = 1;
        cmd            = rec[6].field_4;
        cmd[1].field_4 = 1;
        switch (D_neo_ark_altar_801800AE) {
            case 0:
                cmd                      = rec[4].field_4;
                cmd[1].field_4           = 1;
                cmd[2].field_4           = 1;
                cmd[3].field_4           = 1;
                cmd[4].field_4           = 1;
                cmd[5].field_4           = 1;
                cmd[6].field_4           = 0;
                D_neo_ark_altar_801800AE = 1;
                break;
            case 1:
                cmd                      = rec[4].field_4;
                cmd[1].field_4           = 1;
                cmd[2].field_4           = 1;
                cmd[3].field_4           = 1;
                cmd[4].field_4           = 1;
                cmd[5].field_4           = 0;
                cmd[6].field_4           = 1;
                D_neo_ark_altar_801800AE = 2;
                break;
            case 2:
                cmd                      = rec[4].field_4;
                cmd[1].field_4           = 1;
                cmd[2].field_4           = 1;
                cmd[3].field_4           = 1;
                cmd[4].field_4           = 0;
                cmd[5].field_4           = 1;
                cmd[6].field_4           = 1;
                D_neo_ark_altar_801800AE = 3;
                break;
            case 3:
                cmd                      = rec[4].field_4;
                cmd[1].field_4           = 1;
                cmd[2].field_4           = 1;
                cmd[3].field_4           = 0;
                cmd[4].field_4           = 1;
                cmd[5].field_4           = 1;
                cmd[6].field_4           = 1;
                D_neo_ark_altar_801800AE = 4;
                break;
            case 4:
                cmd                      = rec[4].field_4;
                cmd[1].field_4           = 1;
                cmd[2].field_4           = 0;
                cmd[3].field_4           = 1;
                cmd[4].field_4           = 1;
                cmd[5].field_4           = 1;
                cmd[6].field_4           = 1;
                D_neo_ark_altar_801800AE = 5;
                break;
            case 5:
                cmd                      = rec[4].field_4;
                cmd[1].field_4           = 0;
                cmd[2].field_4           = 1;
                cmd[3].field_4           = 1;
                cmd[4].field_4           = 1;
                cmd[5].field_4           = 1;
                cmd[6].field_4           = 1;
                D_neo_ark_altar_801800AE = 6;
                break;
        }
    } else if (arg0 == 1) {
        cmd            = rec[3].field_4;
        cmd[1].field_4 = 0;
        cmd            = rec[6].field_4;
        cmd[1].field_4 = 0;
        switch (D_neo_ark_altar_801800AE) {
            case 6:
                cmd                      = rec[4].field_4;
                cmd[1].field_4           = 0;
                cmd[2].field_4           = 1;
                cmd[3].field_4           = 1;
                cmd[4].field_4           = 1;
                cmd[5].field_4           = 1;
                cmd[6].field_4           = 1;
                D_neo_ark_altar_801800AE = 5;
                break;
            case 5:
                cmd                      = rec[4].field_4;
                cmd[1].field_4           = 1;
                cmd[2].field_4           = 0;
                cmd[3].field_4           = 1;
                cmd[4].field_4           = 1;
                cmd[5].field_4           = 1;
                cmd[6].field_4           = 1;
                D_neo_ark_altar_801800AE = 4;
                break;
            case 4:
                cmd                      = rec[4].field_4;
                cmd[1].field_4           = 1;
                cmd[2].field_4           = 1;
                cmd[3].field_4           = 0;
                cmd[4].field_4           = 1;
                cmd[5].field_4           = 1;
                cmd[6].field_4           = 1;
                D_neo_ark_altar_801800AE = 3;
                break;
            case 3:
                cmd                      = rec[4].field_4;
                cmd[1].field_4           = 1;
                cmd[2].field_4           = 1;
                cmd[3].field_4           = 1;
                cmd[4].field_4           = 0;
                cmd[5].field_4           = 1;
                cmd[6].field_4           = 1;
                D_neo_ark_altar_801800AE = 2;
                break;
            case 2:
                cmd                      = rec[4].field_4;
                cmd[1].field_4           = 1;
                cmd[2].field_4           = 1;
                cmd[3].field_4           = 1;
                cmd[4].field_4           = 1;
                cmd[5].field_4           = 0;
                cmd[6].field_4           = 1;
                D_neo_ark_altar_801800AE = 1;
                break;
            case 1:
                cmd                      = rec[4].field_4;
                cmd[1].field_4           = 1;
                cmd[2].field_4           = 1;
                cmd[3].field_4           = 1;
                cmd[4].field_4           = 1;
                cmd[5].field_4           = 1;
                cmd[6].field_4           = 0;
                D_neo_ark_altar_801800AE = 0;
                break;
        }
    }
}

/// Altar state 2: records the tile the player walks onto and, while
/// `func_neo_ark_altar_8017E260` reports the altar sequence has matched, raises
/// the wall of the tile the player stands on. `func_neo_ark_altar_8017EC34`
/// resolves the player coordinate to a tile id, which is pushed onto
/// `D_neo_ark_altar_801800B0` whenever it changes; the returned sequence state
/// picks the sound and area record set for the frame and, at 3, arms
/// `var_s2`, which raises the matching tile by half the remaining distance to
/// 0xBB8 per frame. With no tile raised, `field_A` instead decays by a quarter
/// towards 0 while `field_C` still names a valid tile.
void func_neo_ark_altar_8017DF0C(Task* task)
{
    NeoArkAltarWork* work;
    GpCoord*         coord;
    Task*            actor;
    s32              prev;
    s16              cur;
    s16              level;
    s32              i;
    s32              grow;
    s16              found;

    work          = (NeoArkAltarWork*)task->work;
    actor         = *Gp_ActorSlots;
    work->field_6 = work->field_8;
    grow          = 0;
    coord         = actor->extra.tmd->coords;
    cur           = func_neo_ark_altar_8017EC34(D_neo_ark_altar_8017EFD8, (s16)coord->coord.t[0], (s16)coord->coord.t[2]);
    prev          = work->field_6;
    work->field_8 = cur;
    if (cur != prev && prev == 0) {
        work->field_E                                      = cur;
        D_neo_ark_altar_801800B0[D_neo_ark_altar_801800AC] = work->field_8;
        D_neo_ark_altar_801800AC                           = (u16)D_neo_ark_altar_801800AC + 1;
    } else {
        work->field_E = 0;
    }
    switch (func_neo_ark_altar_8017E260(task)) {
        case 1:
            GameFlag_SetNibble(0xDC, 1);
            GameFlag_SetNibble(0x1B7, 0);
            SndEvt_EnqueueType7(0x55140003, 0);
            SndEvt_EnqueueType6(0x55140007, 0, 0);
            Gp_RunCapCmd1(1);
            Gp_ApplyAreaRecs(D_neo_ark_altar_8018007C);
            break;
        case 2:
            GameFlag_SetNibble(0xDD, 1);
            task->state = 3;
            break;
        case 3:
            grow = 1;
            break;
    }
    found = 0;
    for (i = 0; i < 4; i++) {
        if (grow == 1 && (work->field_8 - 1) == i) {
            work->field_C = i;
            /* Two dead stores: loop.c only keeps the `grow == 1` constant
               inside the loop (and so in a caller-saved register, remade in
               the back-edge delay slot) while the loop holds 30 RTL insns.
               At the 28 this body otherwise compiles to it is hoisted, which
               costs an extra saved register and an 8-byte frame. */
            level         = 0;
            level         = 1;
            level         = (u16)work->field_A + ((0xBB8 - work->field_A) >> 1);
            work->field_A = level;
            func_neo_ark_altar_8017E92C((s16)i, level);
            found = 1;
        }
    }
    if (found == 0 && (u16)work->field_C < 4U) {
        level         = (u16)work->field_A + ((-work->field_A) >> 2);
        work->field_A = level;
        if (level >= 0xB) {
            func_neo_ark_altar_8017E92C(work->field_C, level);
        }
    }
}

/// Altar state 0: gates the wall sprites of the current view's record on game
/// flag 0xD9 and resets the altar's work area. The switch state written to
/// `D_neo_ark_altar_801800AE` is 6 while the flag is clear, 0 otherwise; the
/// six sprite commands reached through `rec[3]` / `rec[6]` / `rec[4]` are
/// skipped (1) or linked (0) to match, and the 17 halfwords at
/// `D_neo_ark_altar_801800B0` are cleared for `func_neo_ark_altar_8017E260`.
void func_neo_ark_altar_8017E148(void)
{
    GpAreaKey* sess;
    GpSprtRec* rec;
    GpSprtCmd* cmd;
    s32        i;

    sess = &gGameSession->at4.loc;
    rec  = Gp_SprtTables[sess->stage - 1][0].field_0[sess->area - 1];
    if (GameFlag_GetNibble(0xD9) == 0) {
        cmd                      = rec[3].field_4;
        cmd[1].field_4           = 1;
        cmd                      = rec[6].field_4;
        cmd[1].field_4           = 1;
        cmd                      = rec[4].field_4;
        cmd[1].field_4           = 0;
        cmd[2].field_4           = 1;
        cmd[3].field_4           = 1;
        cmd[4].field_4           = 1;
        cmd[5].field_4           = 1;
        cmd[6].field_4           = 1;
        D_neo_ark_altar_801800AE = 6;
    } else {
        cmd                      = rec[3].field_4;
        cmd[1].field_4           = 0;
        cmd                      = rec[6].field_4;
        cmd[1].field_4           = 0;
        cmd                      = rec[4].field_4;
        cmd[1].field_4           = 1;
        cmd[2].field_4           = 1;
        cmd[3].field_4           = 1;
        cmd[4].field_4           = 1;
        cmd[5].field_4           = 1;
        cmd[6].field_4           = 0;
        D_neo_ark_altar_801800AE = 0;
    }
    D_neo_ark_altar_801800AC = 0;
    for (i = 0x10; i >= 0; i--) {
        D_neo_ark_altar_801800B0[i] = 0;
    }
}

s16 func_neo_ark_altar_8017E260(Task* task)
{
    NeoArkAltarWork* work;
    s32              i;
    s32              bad1;
    s32              bad2;

    bad1 = 0;
    work = (NeoArkAltarWork*)task->work;
    bad2 = 0;
    if (D_neo_ark_altar_801800AC == 0) {
        return 0;
    }
    if (GameFlag_GetNibble(0xDC) == 0) {
        for (i = 0; i < D_neo_ark_altar_801800AC; i++) {
            if (D_neo_ark_altar_8017F050[i] != D_neo_ark_altar_801800B0[i]) {
                goto fail1;
            }
            if (i == 11) {
                return 1;
            }
        }
    } else {
    fail1:
        bad1 = 1;
    }
    if (work->field_E != 0) {
        if (D_neo_ark_altar_8017F050[D_neo_ark_altar_801800AC - 1] != D_neo_ark_altar_801800B0[D_neo_ark_altar_801800AC - 1] || bad1 == 1) {
            if (work->field_E == 1) {
                SndEvt_EnqueueType6(0x55140008, 0, 0);
            }
            if (work->field_E == 2) {
                SndEvt_EnqueueType6(0x55140009, 0, 0);
            }
            if (work->field_E == 3) {
                SndEvt_EnqueueType6(0x5514000A, 0, 0);
            }
            if (work->field_E == 4) {
                SndEvt_EnqueueType6(0x5514000B, 0, 0);
            }
        } else {
            if (work->field_E == 1) {
                SndEvt_EnqueueType6(0x55140001, 0, 0);
            }
            if (work->field_E == 2) {
                SndEvt_EnqueueType6(0x55140000 | work->field_E, 0, 0);
            }
            if (work->field_E == 3) {
                SndEvt_EnqueueType6(0x55140000 | work->field_E, 0, 0);
            }
            if (work->field_E == 4) {
                SndEvt_EnqueueType6(0x55140000 | work->field_E, 0, 0);
            }
        }
    }
    if (GameFlag_GetNibble(0xDD) == 0) {
        for (i = 0; i < D_neo_ark_altar_801800AC; i++) {
            if (D_neo_ark_altar_8017F068[i] != D_neo_ark_altar_801800B0[i]) {
                goto fail2;
            }
            if (i == 15) {
                SndEvt_EnqueueType6(0x5514000C, 0, 0);
                return 2;
            }
        }
    } else {
    fail2:
        bad2 = 1;
    }
    if (work->field_E != 0) {
        if (D_neo_ark_altar_8017F068[D_neo_ark_altar_801800AC - 1] != D_neo_ark_altar_801800B0[D_neo_ark_altar_801800AC - 1] || bad2 == 1) {
            if (work->field_E == 1) {
                SndEvt_EnqueueType6(0x55140008, 0, 0);
            }
            if (work->field_E == 2) {
                SndEvt_EnqueueType6(0x55140009, 0, 0);
            }
            if (work->field_E == 3) {
                SndEvt_EnqueueType6(0x5514000A, 0, 0);
            }
            if (work->field_E == 4) {
                SndEvt_EnqueueType6(0x5514000B, 0, 0);
            }
        } else {
            if (work->field_E == 1) {
                SndEvt_EnqueueType6(0x55140001, 0, 0);
            }
            if (work->field_E == 2) {
                SndEvt_EnqueueType6(0x55140000 | work->field_E, 0, 0);
            }
            if (work->field_E == 3) {
                SndEvt_EnqueueType6(0x55140000 | work->field_E, 0, 0);
            }
            if (work->field_E == 4) {
                SndEvt_EnqueueType6(0x55140000 | work->field_E, 0, 0);
            }
        }
    }
    if (bad1 == 1 && bad2 == bad1) {
        func_neo_ark_altar_8017E148();
        return 0;
    }
    return 3;
}

/// Draws one side of a raised altar tile as 32 horizontal strips. `p0` and
/// `p1` are the side's top corners and `p2` the corner below `p0`; only the
/// height difference `p2 - p0` is used, split into 32 equal steps, and `p3` is
/// not read. Each strip that projects without a clipping error becomes a
/// Gouraud quad whose top edge is shade `c` and bottom edge `c - 6`, linked at
/// its projected depth together with a 0xE100002A draw-mode packet; the shade
/// only steps down for strips that are drawn.
void func_neo_ark_altar_8017E658(SVECTOR* p0, SVECTOR* p1, SVECTOR* p2, SVECTOR* p3)
{
    SVECTOR  v0;
    SVECTOR  v1;
    SVECTOR  v2;
    SVECTOR  v3;
    s32      sxy0;
    s32      sxy1;
    s32      sxy2;
    s32      sxy3;
    s32      p;
    s32      flag;
    s32      otz;
    s16      step;
    s32      i;
    u8       c;
    u8       c2;
    POLY_G4* poly;
    DR_MODE* dr;

    c     = 0xC0;
    step  = (p2->vy - p0->vy) / 32;
    v0.vx = p0->vx;
    v0.vy = p0->vy;
    v0.vz = p0->vz;
    v1.vx = p1->vx;
    v1.vy = p1->vy;
    v1.vz = p1->vz;
    v2.vx = p0->vx;
    v2.vy = p0->vy + step;
    v2.vz = p0->vz;
    v3.vx = p1->vx;
    v3.vy = p1->vy + step;
    v3.vz = p1->vz;
    for (i = 0; i < 32; i++) {
        otz    = RotTransPers4(&v0, &v1, &v2, &v3, &sxy0, &sxy1, &sxy2, &sxy3, &p, &flag);
        v0.vy  = v2.vy;
        v2.vy += step;
        v1.vy  = v3.vy;
        v3.vy += step;
        if (flag >= 0) {
            poly           = (POLY_G4*)gGpuPrimCursor;
            gGpuPrimCursor = (u8*)(poly + 1);
            setlen(poly, 8);
            setcode(poly, 0x3A);
            PRIM_XY_WORD(poly, 0) = sxy0;
            PRIM_XY_WORD(poly, 1) = sxy1;
            PRIM_XY_WORD(poly, 2) = sxy2;
            PRIM_XY_WORD(poly, 3) = sxy3;
            c2                    = c - 6;
            poly->r0              = c;
            poly->g0              = c;
            poly->b0              = c;
            poly->r1              = c;
            poly->g1              = c;
            poly->b1              = c;
            poly->r2              = c2;
            poly->g2              = c2;
            poly->b2              = c2;
            poly->r3              = c2;
            poly->g3              = c2;
            poly->b3              = c2;
            addPrim((u_long*)(((((u32)otz << gDisplayState.otDepthShift) >> 2) & 0xFFC) + (s32)gGpuCurrentOt), poly);
            dr             = (DR_MODE*)gGpuPrimCursor;
            gGpuPrimCursor = (u8*)(dr + 1);
            setlen(dr, 1);
            dr->code[0] = 0xE100002A;
            addPrim((u_long*)(((((u32)otz << gDisplayState.otDepthShift) >> 2) & 0xFFC) + (s32)gGpuCurrentOt), dr);
            c = c2;
        }
    }
}

/// Walls in one altar tile. The view matrix is re-derived from
/// `gGfxViewCoord` and `Gfx_ViewWorldMtx` pushed into the GTE first, then each
/// of the tile's four sides goes to `func_neo_ark_altar_8017E658` as its two
/// corners at the floor height `y0` and at `y0 - arg1`, so `arg1` is how far a
/// side drops below the tile. The sides walk the tile rectangle
/// `(x, z) -> (x + w, z) -> (x + w, z + d) -> (x, z + d)` as `arg0` selects
/// the tile in the table.
void func_neo_ark_altar_8017E92C(s16 arg0, s32 arg1)
{
    NeoArkAltarTile* tile;
    NeoArkAltarTile* base;
    SVECTOR          p0;
    SVECTOR          p1;
    SVECTOR          p2;
    SVECTOR          p3;
    s16              y0;
    s16              y1;

    base = D_neo_ark_altar_8017F014;
    y0   = -0x1086;

    gGfxViewCoord.flg = 0;
    Gp_UpdateCoord(&gGfxViewCoord);

    gte_SetRotMatrix(&Gfx_ViewWorldMtx);
    gte_SetTransMatrix(&Gfx_ViewWorldMtx);

    tile = &base[arg0];
    y1   = y0 - arg1;

    p0.vx = tile->x;
    p0.vy = y0;
    p0.vz = tile->z;
    p1.vx = tile->x + tile->w;
    p1.vy = y0;
    p1.vz = tile->z;
    p2.vx = tile->x;
    p2.vy = y1;
    p2.vz = tile->z;
    p3.vx = tile->x + tile->w;
    p3.vy = y1;
    p3.vz = tile->z;
    func_neo_ark_altar_8017E658(&p0, &p1, &p2, &p3);

    p0.vx = tile->x + tile->w;
    p0.vy = y0;
    p0.vz = tile->z;
    p1.vx = tile->x + tile->w;
    p1.vy = y0;
    p1.vz = tile->z + tile->d;
    p2.vx = tile->x + tile->w;
    p2.vy = y1;
    p2.vz = tile->z;
    p3.vx = tile->x + tile->w;
    p3.vy = y1;
    p3.vz = tile->z + tile->d;
    func_neo_ark_altar_8017E658(&p0, &p1, &p2, &p3);

    p0.vx = tile->x;
    p0.vy = y0;
    p0.vz = tile->z + tile->d;
    p1.vx = tile->x + tile->w;
    p1.vy = y0;
    p1.vz = tile->z + tile->d;
    p2.vx = tile->x;
    p2.vy = y1;
    p2.vz = tile->z + tile->d;
    p3.vx = tile->x + tile->w;
    p3.vy = y1;
    p3.vz = tile->z + tile->d;
    func_neo_ark_altar_8017E658(&p0, &p1, &p2, &p3);

    p0.vx = tile->x;
    p0.vy = y0;
    p0.vz = tile->z;
    p1.vx = tile->x;
    p1.vy = y0;
    p1.vz = tile->z + tile->d;
    p2.vx = tile->x;
    p2.vy = y1;
    p2.vz = tile->z;
    p3.vx = tile->x;
    p3.vy = y1;
    p3.vz = tile->z + tile->d;
    func_neo_ark_altar_8017E658(&p0, &p1, &p2, &p3);
}

/// Returns the `id` of the first tile in `table` whose rectangle contains
/// `(x, z)`, edges inclusive, or 0 when none does. The scan ends at the entry
/// whose `id` is -1.
s16 func_neo_ark_altar_8017EC34(NeoArkAltarTile* table, s16 x, s16 z)
{
    for (; table->id != -1; table++) {
        if (table->x <= x && x <= table->x + table->w && table->z <= z && z <= table->z + table->d) {
            return table->id;
        }
    }
    return 0;
}

void func_neo_ark_altar_8017ED60(Task* task);
void func_neo_ark_altar_8017EDBC(Task* task);
void func_neo_ark_altar_8017EDF8(Task* task);
void func_neo_ark_altar_8017EE30(Task* task);
void func_neo_ark_altar_8017EE90(Task* task);
void func_neo_ark_altar_8017EF00(Task* task);
void func_neo_ark_altar_8017EF34(Task* task);

/// State handlers of the altar task, dispatched by
/// `func_neo_ark_altar_8017ECE0` off `Task::state`: allocation and set-up,
/// a short wait, the tile sequence (`func_neo_ark_altar_8017DF0C`), then, once
/// the sequence completes, a fade-out, a spawn from `D_neo_ark_altar_8017EFC0`
/// and a view change before control returns to the tile sequence.
const TaskFuncTable8 D_neo_ark_altar_8017D648 = {
    func_neo_ark_altar_8017ED60,
    func_neo_ark_altar_8017EDBC,
    func_neo_ark_altar_8017DF0C,
    func_neo_ark_altar_8017EDF8,
    func_neo_ark_altar_8017EE30,
    func_neo_ark_altar_8017EE90,
    func_neo_ark_altar_8017EF00,
    func_neo_ark_altar_8017EF34,
};

void func_neo_ark_altar_8017ECE0(Task* arg0)
{
    TaskFuncTable8 sp = D_neo_ark_altar_8017D648;

    sp.funcs[arg0->state](arg0);
}

void func_neo_ark_altar_8017ED60(Task* arg0)
{
    NeoArkAltarWork* work;

    work       = memCalloc(0x10, 0);
    arg0->work = work;
    if (work == NULL) {
        taskKill(arg0);
        return;
    }
    func_neo_ark_altar_8017E148();
    arg0->killCountdown = 0;
    arg0->state         = (s32)(arg0->state + 1);
}

void func_neo_ark_altar_8017EDBC(Task* arg0)
{
    arg0->killCountdown = arg0->killCountdown + 1;
    if (arg0->killCountdown >= 3) {
        arg0->state = (s32)(arg0->state + 1);
    }
}

void func_neo_ark_altar_8017EDF8(Task* arg0)
{
    Gp_MsgPlayerWeapon(0);
    arg0->killCountdown = 0;
    arg0->state         = (s32)(arg0->state + 1);
}

void func_neo_ark_altar_8017EE30(Task* arg0)
{
    u8 temp_a0;

    arg0->killCountdown = arg0->killCountdown + 6;
    if (arg0->killCountdown >= 0x100) {
        arg0->killCountdown = 0xFF;
        arg0->state         = (s32)(arg0->state + 1);
    }
    temp_a0 = (u8)arg0->killCountdown;
    Fade_DrawOverlay(temp_a0, temp_a0, temp_a0, 2);
}

void func_neo_ark_altar_8017EE90(Task* arg0)
{
    NeoArkAltarWork* work;

    work = arg0->work;
    Gp_MsgPlayer3F3(0);
    gGameSession->hideHud = 1;
    work->field_0         = Task_SpawnFromTable(D_neo_ark_altar_8017EFC0, 0, 2, 0);
    arg0->state           = (s32)(arg0->state + 1);
}

void func_neo_ark_altar_8017EF00(Task* arg0)
{
    s16* viewDirty;

    /* Through a pointer rather than as a member: a member store is struct
       memory, which the scheduler lets the store to `Mc_SaveData.at4.loc.room` pass, and the
       original keeps the two in source order. */
    viewDirty                  = &gGameSession->viewDirty;
    *viewDirty                 = 1;
    Mc_SaveData.at4.loc.room   = 2;
    gGameSession->at4.loc.room = 2;
    arg0->state                = (s32)(arg0->state + 1);
}

void func_neo_ark_altar_8017EF34(Task* arg0)
{
    SetDispMask(1);
    Gp_MsgPlayer3F3(1);
    Gp_MsgPlayerWeapon(1);
    gGameSession->hideHud = 0;
    arg0->state           = 2;
}

void func_neo_ark_altar_8017EF84(void)
{
}

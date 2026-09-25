#include "common.h"

#include "gameplay/1BC.h"
#include "gameplay/268.h"
#include "gameplay/3A34.h"
#include "gameplay/3CD8.h"
#include "gameplay/3FB8.h"
#include "gameplay/D4.h"
#include "gameplay/gameplay.h"

#include "main/display.h"
#include "main/gameflag.h"
#include "main/mc.h"
#include "main/mem.h"
#include "main/session.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/tmd.h"
#include "rooms/acropolis_patio.h"
#include "rooms/room.h"
#include "rooms/room_common.h"

#include <psyq/inline_c.h>
#include "gte.h"
#include <psyq/libgpu.h>
#include <psyq/libgs.h>
#include <psyq/libgte.h>

typedef struct {
    /* 0x0 */ u16 field_0;
    /* 0x2 */ u8  field_2;
    /* 0x3 */ u8  field_3;
    /* 0x4 */ u8  field_4;
    /* 0x5 */ u8  field_5;
    /* 0x6 */ u16 field_6;
} AcropolisPatioMsg8;

extern s8       D_8007272D;
extern s16      D_80071076;
extern u8       D_8007216D;
extern TaskDesc D_acropolis_patio_801802BC;
extern TaskDesc D_acropolis_patio_80182800;
extern s32      D_acropolis_patio_8018028C;
extern s32      D_acropolis_patio_80180428;
extern s32      D_acropolis_patio_80180440;
extern s32      D_acropolis_patio_8018044C;
extern s32      D_acropolis_patio_8018046C;
extern Task*    D_acropolis_patio_80187060;
extern s32      D_acropolis_patio_80180DEC;
extern s32      D_acropolis_patio_80180EDC;
extern u8       D_acropolis_patio_80187064;
extern u8       D_acropolis_patio_80187065;
extern s32      D_acropolis_patio_80180484;
extern s32      D_acropolis_patio_801806AC;
extern s32      D_acropolis_patio_8018082C;
extern s32      D_acropolis_patio_80180C64;
extern s32      D_acropolis_patio_8018280C;
extern s32      D_acropolis_patio_80182BE4;

/// The 14 anchor points of the patio's fountain spray, in the room object's own
/// space. The first three double as the jitter centres for the mist burst.
extern SVECTOR D_acropolis_patio_80182DDC[14];

/// Per-anchor camera-view mask, one bit per 1-based `GameSession::at4.loc.view`
/// view: anchor `i` only draws while the room is being seen from a view its
/// mask names.
extern u16 D_acropolis_patio_80182E4C[14];

void func_acropolis_patio_8017D5EC(Task* arg0);
void func_acropolis_patio_8017DF7C(Task* task);

/// State table of the room's three-state task dispatcher
/// (`func_acropolis_patio_8017DF8C`): the entry tick, an idle state, then
/// `taskKill`.
const TaskFuncTable3 D_acropolis_patio_8017D5C4 = {
    { func_acropolis_patio_8017D5EC, func_acropolis_patio_8017DF7C, taskKill },
};

/// Room entry task tick. Publishes the room's own record at
/// `Task::msgTable` / pointer slot 7, then re-issues the messages the room's
/// actors need for the current point in the story: the first visit
/// (`GameFlag_GetNibble(0) < 2`) arms the two hotspots and spawns the arrival
/// cutscene, and the second-visit branches replace them according to
/// `gGameSession::at4.loc.place`.
void func_acropolis_patio_8017D5EC(Task* arg0)
{
    GpCmdArg msg;
    s32      temp;

    arg0->msgTable = &D_acropolis_patio_8018028C;
    Game_SetPtrSlot(arg0, 7);
    if (GameFlag_GetNibble(0) < 2) {
        if (D_8007216D == 1) {
            Gp_DispatchMsg((Task*)Gp_LookupSlot4(0), 0x7D4, (s32)&D_acropolis_patio_80180428, 0);
            Gp_DispatchMsg((Task*)Gp_LookupSlot4(0), 0x7DB, (s32)&D_acropolis_patio_8018044C, 0);
            Gp_DispatchMsg((Task*)Gp_LookupSlot4(0), 0x7D5, 1, 0);
            D_acropolis_patio_80187060 = Task_SpawnFromTable(&D_acropolis_patio_801802BC, 2, 0, 0);
        }
        temp = Gp_LookupSlot4(1);
        if (temp != 0) {
            Gp_DispatchMsg((Task*)temp, 0x7D4, (s32)&D_acropolis_patio_8018046C, 0);
        }
    }
    if ((gGameSession->at4.loc.place == 1) && (GameFlag_GetNibble(0x21) < 2) && (GameFlag_GetNibble(0x21) < 2)) {
        temp = Gp_LookupSlot4(1);
        if (temp != 0) {
            Gp_DispatchMsg((Task*)temp, 0x7DB, (s32)&D_acropolis_patio_80180440, 0);
        }
    }
    if ((gGameSession->at4.loc.place == 2) && (GameFlag_GetNibble(0x26) == 0)) {
        msg.from.loc.stage = 1;
        msg.from.loc.area  = 3;
        msg.command        = 0;
        Gp_DispatchMsg((Task*)Gp_LookupSlot4(2), 0x7DB, (s32)&msg, 0);
        Gp_DispatchMsg((Task*)Gp_LookupSlot4(3), 0x7DB, (s32)&msg, 0);
    }
    arg0->state = arg0->state + 1;
}

s32 func_acropolis_patio_8017D7D0(s32 arg0, s32 arg1, AcropolisPatioMsg8* arg2, AcropolisPatioMsg8* arg3)
{
    s32 var_v0;
    u16 temp_s1;

    *arg3 = *arg2;
    if (arg2->field_0 == 8) {
        if ((GameFlag_GetNibble(9) & 2) && (arg2->field_5 == 0)) {
            arg3->field_3 = 2;
        }
    }
    if (arg2->field_0 == 4) {
        if (GameFlag_GetNibble(8) < 2) {
            var_v0 = 0;
            if (arg2->field_5 == 0) {
                Gp_RunCapCmd1(3);
                GameFlag_SetNibble(8, 1);
                Gp_SetNibbleIf(arg2->field_6, 2);
                return 0;
            }
            return var_v0;
        }
        if (GameFlag_GetNibble(0) == 2) {
            var_v0 = 2;
            if (arg2->field_5 == 0) {
                if (GameFlag_GetNibble(0x23) == 0) {
                    func_800E8634((s32)&D_acropolis_patio_80180DEC, 0, (s32)&D_acropolis_patio_80180EDC);
                    GameFlag_SetNibble(0x23, 1);
                    return 2;
                }
                Gp_RunCapCmd1(8);
                return 2;
            }
            return var_v0;
        }
        if (GameFlag_GetNibble(8) == 2) {
            var_v0 = 2;
            if (arg2->field_5 == 0) {
                Task_SpawnFromTable(&D_acropolis_patio_801802BC, 1, 0, 0);
                Gp_SetItemSeenBit(0x101, 1);
                D_acropolis_patio_80187064 = arg2->field_2;
                D_acropolis_patio_80187065 = arg2->field_3;
                return 2;
            }
            return var_v0;
        }
        goto block_17;
    }
block_17:
    if ((arg2->field_0 == 8) && (GameFlag_GetNibble(0) < 5)) {
        var_v0 = 0;
        if (arg2->field_5 == 0) {
            Gp_SetNibbleIf(arg2->field_6, 2);
            Gp_RunCapCmd1(4);
            return 0;
        }
        return var_v0;
    }
    if ((arg2->field_5 == 0) && (GameFlag_GetNibble(0x21) == 3)) {
        GameFlag_SetNibble(0x21, 4);
    }
    temp_s1 = arg2->field_0;
    var_v0  = 1;
    if (temp_s1 == 4) {
        var_v0 = 1;
        if (arg2->field_5 == 0) {
            if (GameFlag_GetNibble(0) >= 3) {
                arg3->field_3 = (s8)temp_s1;
            }
            var_v0 = 1;
            if (GameFlag_GetNibble(0) == 2) {
                arg3->field_3 = 3;
                var_v0        = 1;
            }
        }
    }
    return var_v0;
}

void func_acropolis_patio_8017DA5C(Task* task)
{
    s32 state;

    state = task->state;
    switch (state) {
        case 0:
            Gp_MsgPlayerWeapon(0);
            Gp_RunCapCmd1(3);
            goto advance;
        case 1:
            task->state = 2;
            return;
        case 2:
            if (Gp_GetCapEventKey() == state) {
                GameFlag_SetNibble(8, 3);
                SndEvt_EnqueueType6(0x51030004, 0, 0);
            advance:
                task->state = task->state + 1;
                return;
            }
            Gp_MsgPlayerWeapon(1);
            goto kill;
        case 3:
            if (SndVoice_HasActiveId(0x51030004) != 0) {
                return;
            }
            SndEvt_EnqueueType7(0x80000000, 0);
            Mc_SaveData.at4.loc.area = 4;
            D_80071076               = 1;
            Mc_SaveData.at4.loc.warp = D_acropolis_patio_80187064;
            Mc_SaveData.at4.loc.room = D_acropolis_patio_80187065;
            Task_Spawn(0, 0x11, 0, 0);
        kill:
            taskKill(task);
            return;
    }
}

void func_acropolis_patio_8017DBAC(s32 arg0, s32 arg1, AcropolisPatioMsg8* arg2)
{
    u8 state;

    if ((arg2->field_2 == 0) && (GameFlag_GetNibble(0x21) < 2)) {
        GameFlag_SetNibble(0x21, 3);
        func_800E8634((s32)&D_acropolis_patio_80180484, 0, (s32)&D_acropolis_patio_801806AC);
        D_8007272D              = 3;
        gGameSession->flowFlags = 0xC1;
    }
    if ((arg2->field_2 == 1) && (GameFlag_GetNibble(0x21) == 3) &&
        (Gp_DispatchMsg((Task*)Gp_LookupSlot4(1), 0x7D6, 0, 0) == 0)) {
        GameFlag_SetNibble(0x21, 4);
        func_800E8634((s32)&D_acropolis_patio_8018082C, 0, (s32)&D_acropolis_patio_80180C64);
    }
    state = arg2->field_2;
    if ((state == 2) && (GameFlag_GetNibble(0x26) == 0) && (GameFlag_GetNibble(0) == state)) {
        GameFlag_SetNibble(0x26, 1);
        func_800E8634((s32)&D_acropolis_patio_8018280C, 0, (s32)&D_acropolis_patio_80182BE4);
    }
}
s32 func_acropolis_patio_8017DCE4(s32 arg0, s32 arg1, s32 arg2)
{
    s32 var_v0;

    if (arg2 == 1) {
        Gp_SpawnIfCapIdle(1, 0);
    }
    var_v0 = 2;
    if (arg2 == 2) {
        var_v0 = GameFlag_GetNibble(0) < 2;
        if (var_v0 != 0) {
            Gp_SpawnIfCapIdle(2, 0);
            var_v0 = 0;
        }
    }
    return var_v0;
}

s32 func_acropolis_patio_8017DD44(void)
{
    return 0;
}

s32 func_acropolis_patio_8017DD4C(s32 arg0, s32 arg1, s32 arg2)
{
    if (arg2 == 3) {
        SndEvt_EnqueueType6(0x51030000 | 3, 0, 0);
    }
    return 0;
}
void func_acropolis_patio_8017DD80(Task* task)
{
    s32 state;

    state = task->state;
    switch (state) {
        case 0:
            Gp_RunCapCmd1(6);
            task->state = task->state + 1;
            return;
        case 1:
            task->state = 2;
            return;
        case 2:
            if (Gp_GetCapEventKey() == 1) {
                GameFlag_SetNibble(0x15, 1);
            }
            taskKill(task);
            return;
    }
}

void func_acropolis_patio_8017DE2C(Task* task)
{
    ApLookAtWork work;
    Task*        target;
    s32          offset;

    target                = gameGetPtrSlot(3);
    work.coord.coord.t[0] = -0x1F40;
    work.coord.coord.t[1] = 0;
    work.coord.coord.t[2] = 0x384;

    switch (task->state) {
        case 0:
            task->spawnArg1 = 0;
            task->state     = task->state + 1;
            return;
        case 1:
            return;
        case 2:
            func_800B0CF4(target, &work.coord, 0x200, 0x100, 0x1000);
            return;
        case 3:
            task->spawnArg1 = 0;
            func_800B0CF4(target, &work.coord, 0x200, 0x100, 0x1000);
            task->state = task->state + 1;
            return;
        case 4:
            offset          = task->spawnArg1 + 0x32;
            task->spawnArg1 = offset;
            if (offset >= 0x1001) {
                task->spawnArg1 = 0x1000;
            }
            work.coord.coord.t[2] -= task->spawnArg1;
            func_800B0CF4(target, &work.coord, 0x200, 0x100, 0x1000);
            return;
    }
}

void func_acropolis_patio_8017DF38(s32 arg0)
{
    D_acropolis_patio_80187060->state = arg0;
}

void func_acropolis_patio_8017DF48(void)
{
    gGameSession->at4.loc.room = D_8007216D = 2;
    gGameSession->roomObjsDirty             = 1;
}
void func_acropolis_patio_8017DF70(u8 arg0)
{
    Gp_StateF0.field_4 = arg0;
}

INCLUDE_RODATA("rooms/nonmatchings/acropolis_patio/acropolis_patio", D_acropolis_patio_8017D5E8);

void func_acropolis_patio_8017DF7C(Task* task)
{
    char pad[0x10];
}

void func_acropolis_patio_8017DF8C(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_acropolis_patio_8017D5C4;
    sp.funcs[task->state](task);
}

void func_acropolis_patio_8017DFE4(s32 arg0)
{
    if (arg0 != 0) {
        Gp_StateF0.field_4 = 0;
        Gp_ArmStateF0(1);
        return;
    }
    Gp_StateF0.field_4 = 1;
}
void func_acropolis_patio_8017E024(void)
{
    Task_SpawnFromTable(&D_acropolis_patio_80182800, 0, 0, 0);
}

/// Slow left turn-in-place: nudges the player's facing angle by -0x80 each
/// frame for 0x10 frames, wrapping it back into [-0x800, 0x800), then kills
/// itself. Any state other than 0 or 1 kills the task immediately.
void func_acropolis_patio_8017E054(Task* task)
{
    GameActor* actor;
    s16        angle;

    actor = gameGetPtrSlot(3)->work;

    switch (task->state) {
        case 0:
            task->killCountdown = 0x10;
            task->state++;
            /* fallthrough */
        case 1:
            angle = actor->field_52 - 0x80;
            if (angle < -0x800) {
                angle = actor->field_52 + 0xF80;
            }
            actor->field_52 = angle;
            task->killCountdown--;
            if (task->killCountdown > 0) {
                break;
            }
            /* fallthrough */
        default:
            taskKill(task);
            break;
    }
}

/// Lights the patio fountain: a one-shot burst that seeds every jet and its
/// mist, then leaves the task idle for the rest of the room.
///
/// The 14 anchors of `D_acropolis_patio_80182DDC` are handed to `Gp_SpawnEff`
/// as three runs of effect 0x60087, each run differing only in the high bits of
/// the spawn argument - `0x03000200` for the three main jets, `0x02000000` for
/// the next four and a plain `0x100` for the remaining seven - so the anchor
/// index rides in the low byte and the flags pick the jet's size and blend.
///
/// The three main jets then get three puffs of mist each (effect 0x6008F).
/// Every puff re-uses the task's own `GpEffWork.move` triple as a scratch
/// offset: three 11-bit LCG draws centred on 0x400 give a `+/-0x400` jitter,
/// which is added to the jet's anchor before the spawn reads it. The work block
/// is scratch, not state - each spawn copies the vector out immediately - so
/// all nine puffs share it.
void func_acropolis_patio_8017E100(Task* task)
{
    GpCoord*   objCoord;
    GpEffWork* work;
    s32        i;
    s32        j;

    work     = (GpEffWork*)task->spawnArg2;
    objCoord = task->extra.tmd->coords;

    if (task->state == 0) {
        for (i = 0; i < 3; i++) {
            Gp_SpawnEff(0x60087, objCoord, i + 0x03000200, &D_acropolis_patio_80182DDC[i]);
        }
        for (i = 3; i < 7; i++) {
            Gp_SpawnEff(0x60087, objCoord, i + 0x02000000, &D_acropolis_patio_80182DDC[i]);
        }
        for (i = 7; i < 0xE; i++) {
            Gp_SpawnEff(0x60087, objCoord, i + 0x100, &D_acropolis_patio_80182DDC[i]);
        }
        task->state++;
        for (i = 0; i < 3; i++) {
            for (j = 0; j < 3; j++) {
                Gp_LcgState    = Gp_LcgState * 5 + 0x71357911;
                work->move.vx  = 0x400 - (((u32)Gp_LcgState >> 16) & 0x7FF);
                Gp_LcgState    = Gp_LcgState * 5 + 0x71357911;
                work->move.vy  = 0x400 - (((u32)Gp_LcgState >> 16) & 0x7FF);
                Gp_LcgState    = Gp_LcgState * 5 + 0x71357911;
                work->move.vz  = 0x400 - (((u32)Gp_LcgState >> 16) & 0x7FF);
                work->move.vx += D_acropolis_patio_80182DDC[i].vx;
                work->move.vy += D_acropolis_patio_80182DDC[i].vy;
                work->move.vz += D_acropolis_patio_80182DDC[i].vz;
                Gp_SpawnEff(0x6008F, objCoord, i, &work->move);
            }
        }
    }
}

/// Draws one frame of a flickering sprite at the task's own coordinate frame.
/// Nothing is drawn once `Gp_State1C->eventState` reaches 4, nor for a camera
/// view whose bit is clear in the anchor mask `D_acropolis_patio_80182E4C`,
/// indexed by the low nibble of `Task::spawnArg1`.
///
/// On the first frame the task unpacks the rest of `spawnArg1` into its effect
/// work block - the sprite's half extent from bits 16-27 (0x280 when those bits
/// are clear), its animation column from bits 8-9, and that column's grey level
/// from `D_acropolis_patio_8017D5E8` - and keeps only the anchor index. Every
/// frame it projects the coordinate's translation through `GsWSMATRIX` into a
/// 0x14-byte `G_SCRATCH_HEAD` block and, at `otz` 0x11 or further, queues one
/// semi-transparent `POLY_FT4` on tpage 0x2B whose half extent is
/// `width * 39 / otz`, so the sprite shrinks with distance. The grey steps by
/// 0x10 on the parity of `DisplayState::animFrame`, which is the flicker.
void func_acropolis_patio_8017E324(Task* task)
{
    void**            scratch;
    RoomShaftScratch* block;
    GpEffWork*        work;
    GpCoord*          coord;
    POLY_FT4*         prim;
    DisplayState*     ds;
    s32               rgb;
    s32               flip;
    s16               xy;

    work  = (GpEffWork*)task->spawnArg2;
    coord = task->extra.tmd->coords;
    if (Gp_State1C->eventState < 4 &&
        ((D_acropolis_patio_80182E4C[task->spawnArg1 & 0xF] >> ((u8)gGameSession->at4.loc.view - 1)) & 1)) {
        Gp_UpdateCoord(coord);
        scratch = (void**)G_SCRATCH_HEAD;
        SCRATCH_PUSH_BYTES_AT(scratch, 0x14);
        block = (RoomShaftScratch*)*scratch;
        if (task->state == 0) {
            ApGreyLevels levels = D_acropolis_patio_8017D5E8;

            if (task->spawnArg1 & 0xFFF0000) {
                work->scale = (task->spawnArg1 >> 16) & 0xFFF;
            } else {
                work->scale = 0x280;
            }
            work->angle     = (task->spawnArg1 >> 8) & 3;
            task->spawnArg1 = task->spawnArg1 & 0xF;
            work->period    = levels.level[work->angle];
            task->state     = task->state + 1;
        }
        block->vec.vx = (u16)coord->workm.t[0];
        block->vec.vy = (u16)coord->workm.t[1];
        block->vec.vz = (u16)coord->workm.t[2];
        gte_SetTransMatrix(&GsWSMATRIX);
        gte_SetRotMatrix(&GsWSMATRIX);
        gte_ldv0(&block->vec);
        gte_rtps();
        prim           = (POLY_FT4*)gGpuPrimCursor;
        gGpuPrimCursor = prim + 1;
        setlen(prim, 9);
        setcode(prim, 0x2C);
        gte_stsxy(&block->sx);
        gte_stszotz(&block->otz);
        if (block->otz >= 0x11) {
            ds   = &gDisplayState;
            flip = (u8)ds->animFrame;
            SOFT_BARRIER();
            rgb         = (u8)work->period;
            prim->tpage = 0x2B;
            rgb        += (flip & 1) << 4;
            prim->r0    = rgb;
            prim->g0    = rgb;
            prim->b0    = rgb;
            setSemiTrans(prim, 1);
            setClut(prim, work->angle * 16, 0x10E);
            prim->u0 = work->angle * 0x28;
            prim->v0 = 0;
            prim->u1 = work->angle * 0x28 + 0x27;
            prim->v1 = 0;
            prim->u2 = work->angle * 0x28;
            prim->v2 = 0x27;
            prim->u3 = work->angle * 0x28 + 0x27;
            prim->v3 = 0x27;

            block->halfWidth = (work->scale * 0x27) / block->otz;
            xy               = block->sx - (u16)block->halfWidth;
            prim->x2         = xy;
            prim->x0         = xy;
            xy               = block->sx + (u16)block->halfWidth;
            prim->x3         = xy;
            prim->x1         = xy;
            xy               = block->sy - (u16)block->halfWidth;
            prim->y1         = xy;
            prim->y0         = xy;
            xy               = block->sy + (u16)block->halfWidth;
            prim->y3         = xy;
            prim->y2         = xy;
            addPrim((u_long*)(((((u32)block->otz << ds->otDepthShift) >> 2) & 0xFFC) + (s32)gGpuCurrentOt), prim);
        }
        SCRATCH_POP_BYTES(0x14);
    }
}

/// Draws and drifts one puff of the fountain's mist for the current frame.
///
/// The puff only exists for the camera views its anchor's mask in
/// `D_acropolis_patio_80182E4C` names, and the whole draw stops once
/// `Gp_State1C->eventState` reaches 4 (the room is fading out).
///
/// `GpEffWork::index` is the puff's mode and the per-frame step in
/// `GpEffWork.move` is its velocity. In drift mode (0) the velocity is
/// re-rolled every frame as `0x10 - rand[0,0x1F]` per axis, a random walk
/// centred just above zero, and a 1-in-60 draw flips the puff into gather
/// mode. In gather mode (non-zero) the velocity is instead re-aimed at the
/// jet's own anchor once every fourth frame - the normalised direction from
/// the puff to the anchor, scaled by `GPF` at `dp = 0x20` - and jittered by
/// `+/-8` per axis every frame, with a 1-in-120 draw returning it to drift.
/// The velocity is then added to the effect coordinate's translation.
///
/// The result is projected through `GsWSMATRIX` into a 0xC-byte scratch frame
/// and drawn as a single grey `TILE_1` whose level is a fresh `rand[0,0xC0)`,
/// so the mist shimmers; the tile is dropped entirely inside `otz` 0x11.
void func_acropolis_patio_8017E730(Task* task)
{
    GpEffWork*       work;
    GpCoord*         coord;
    RoomMoteScratch* sc;
    SVECTOR*         dir;
    SVECTOR*         anchors;
    TILE_1*          prim;
    u32              level;

    work  = task->spawnArg2;
    coord = task->extra.tmd->coords;
    if (Gp_State1C->eventState < 4 &&
        ((D_acropolis_patio_80182E4C[task->spawnArg1] >> ((u8)gGameSession->at4.loc.view - 1)) & 1)) {
        sc = (RoomMoteScratch*)SCRATCH_PUSH_BYTES(0xC);
        Gp_UpdateCoord(coord);
        if (task->state == 0) {
            Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
            work->move.vx = 0x10 - (((u32)Gp_LcgState >> 16) & 0x1F);
            Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
            work->move.vy = 0x10 - (((u32)Gp_LcgState >> 16) & 0x1F);
            Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
            work->move.vz = 0x10 - (((u32)Gp_LcgState >> 16) & 0x1F);
            task->state++;
        }
        if (work->index != 0) {
            Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
            if ((((u32)Gp_LcgState >> 16) & 3) == 0) {
                anchors       = D_acropolis_patio_80182DDC;
                dir           = &work->move;
                work->move.vx = (u16)anchors[task->spawnArg1].vx -
                                (u16)coord->coord.t[0];
                work->move.vy = (u16)anchors[task->spawnArg1].vy -
                                (u16)coord->coord.t[1];
                work->move.vz = (u16)anchors[task->spawnArg1].vz -
                                (u16)coord->coord.t[2];
                VectorNormalSS(dir, dir);
                gte_lddp(0x20);
                gte_ldsv(dir);
                gte_gpf12();
                gte_stsv(dir);
            }
            Gp_LcgState    = Gp_LcgState * 5 + 0x71357911;
            work->move.vx -= (((u32)Gp_LcgState >> 16) & 0xF) - 8;
            Gp_LcgState    = Gp_LcgState * 5 + 0x71357911;
            work->move.vy -= (((u32)Gp_LcgState >> 16) & 0xF) - 8;
            Gp_LcgState    = Gp_LcgState * 5 + 0x71357911;
            work->move.vz -= (((u32)Gp_LcgState >> 16) & 0xF) - 8;
            Gp_LcgState    = Gp_LcgState * 5 + 0x71357911;
            if ((u16)(((u32)Gp_LcgState >> 16) % 0x78) == 0) {
                work->index = 0;
            }
        } else {
            Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
            work->move.vx = 0x10 - (((u32)Gp_LcgState >> 16) & 0x1F);
            Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
            work->move.vy = 0x10 - (((u32)Gp_LcgState >> 16) & 0x1F);
            Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
            work->move.vz = 0x10 - (((u32)Gp_LcgState >> 16) & 0x1F);
            Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
            if ((u16)(((u32)Gp_LcgState >> 16) % 0x3C) == 0) {
                work->index = 1;
            }
        }
        coord->coord.t[0] += work->move.vx;
        coord->coord.t[1] += work->move.vy;
        coord->coord.t[2] += work->move.vz;
        coord->flg         = 0;
        sc->vec.vx         = (u16)coord->workm.t[0];
        sc->vec.vy         = (u16)coord->workm.t[1];
        sc->vec.vz         = (u16)coord->workm.t[2];
        gte_SetTransMatrix(&GsWSMATRIX);
        gte_SetRotMatrix(&GsWSMATRIX);
        gte_ldv0(&sc->vec);
        gte_rtps();
        prim           = (TILE_1*)gGpuPrimCursor;
        gGpuPrimCursor = prim + 1;
        setTile1(prim);
        gte_stsxy(&prim->x0);
        gte_stszotz(&sc->otz);
        if (sc->otz >= 0x11) {
            Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
            level       = (u32)Gp_LcgState >> 16;
            level      %= 0xC0;
            prim->r0    = level;
            prim->g0    = level;
            prim->b0    = level;
            addPrim((u_long*)(((((u32)sc->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC) +
                              (s32)gGpuCurrentOt),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 0, sc->otz);
        }
        SCRATCH_POP_BYTES(0xC);
    }
}

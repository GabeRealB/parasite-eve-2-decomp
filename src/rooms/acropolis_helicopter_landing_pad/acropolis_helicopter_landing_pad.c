#include "common.h"

#include "gameplay/1A8.h"
#include "gameplay/1BC.h"
#include "gameplay/268.h"
#include "gameplay/3CD8.h"
#include "gameplay/D4.h"
#include "main/display.h"
#include "main/gameflag.h"
#include "main/mc.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/tmd.h"
#include "rooms/room_common.h"
#include "main/gfx.h"
#include "rooms/acropolis_helicopter_landing_pad.h"

extern s16 D_80071076;
extern s32 D_acropolis_helicopter_landing_pad_80184D9C;
extern s32 D_acropolis_helicopter_landing_pad_80184124;
extern s32 D_acropolis_helicopter_landing_pad_801844B4;
extern s32 D_acropolis_helicopter_landing_pad_80184E0C;
extern s32 D_acropolis_helicopter_landing_pad_80187F84;
extern s16 D_acropolis_helicopter_landing_pad_80187F7C;
/// Turn-to-heading task state: current unwrapped yaw and signed step.
extern s32 D_acropolis_helicopter_landing_pad_80187F74;
extern s32 D_acropolis_helicopter_landing_pad_80187F78;

/// Main-executable globals with no module header yet: `D_80071075` gates the
/// phase advance, `D_80114C12` is the cutscene/among-us mode flag.
extern u8         D_80071075;
extern s8         D_80114C12;
extern TaskDesc   D_acropolis_helicopter_landing_pad_80184E68;
extern GpMsgEntry D_acropolis_helicopter_landing_pad_80182328[];
extern GsF_LIGHT  D_acropolis_helicopter_landing_pad_80182340[3];
/// Per-camera-view visibility table indexed by `(u8)Game_Session->field_4`:
/// a non-zero byte keeps the enemy model visible in that view.
extern s8 D_acropolis_helicopter_landing_pad_80182370[];

void func_acropolis_helicopter_landing_pad_8017D7B0(Task* task);
s32  func_acropolis_helicopter_landing_pad_8017D8E8(Task* task, s32 msgId, RoomPlacement* placement, s32 arg3);
void func_acropolis_helicopter_landing_pad_8017E618(s32 arg0, s32 arg1);

/// Three `Gp_SpawnScript18` argument pairs used by the state timeline in
/// `func_acropolis_helicopter_landing_pad_8017DE78`, one pair per phase.
extern s32 D_acropolis_helicopter_landing_pad_80187D40;
extern s32 D_acropolis_helicopter_landing_pad_80187D48;
extern s32 D_acropolis_helicopter_landing_pad_80187D50;
extern s32 D_acropolis_helicopter_landing_pad_80187D60;
extern s32 D_acropolis_helicopter_landing_pad_80187D68;
extern s32 D_acropolis_helicopter_landing_pad_80187D78;

extern RoomPlacement D_acropolis_helicopter_landing_pad_80182394;
extern RoomPlacement D_acropolis_helicopter_landing_pad_801823AC;

/// State-0 entry of the room's enemy task: allocates the 0x54-byte work block
/// into `Task::idMap`, marks the model (`field_E = 8`, clears bit 0x80 of
/// `field_C`), runs the placement setup and installs the message table.
void func_acropolis_helicopter_landing_pad_8017D658(Task* task)
{
    TmdObject* obj = task->extra;
    void*      mem;

    mem = Mem_Calloc(0x54, false);
    if (mem == NULL) {
        Gp_EnemyTaskExit(task);
        return;
    }
    task->idMap   = mem;
    obj->field_E  = 8;
    obj->field_C &= 0xFF7F;
    func_acropolis_helicopter_landing_pad_8017D7B0(task);
    task->field_24      = D_acropolis_helicopter_landing_pad_80182328;
    task->killCountdown = 0;
    task->state         = task->state + 1;
}

/// Per-frame update of the enemy task's model. While the `field_50` countdown
/// armed by the 0x7D3 handler is running, the model's coordinate translation
/// is stepped by the work block's three velocity words and marked dirty; the
/// countdown is clamped at zero once it expires. When `Game_Session->field_4D`
/// is set, the model is hidden (bit 0x80 of `field_C`) in every camera view
/// whose entry in the per-view table is zero and shown again otherwise.
void func_acropolis_helicopter_landing_pad_8017D6E0(Task* task)
{
    AhlpEnemyWork* work  = (AhlpEnemyWork*)task->idMap;
    RoomCoord*     coord = (RoomCoord*)((TmdObject*)task->extra)->field_8;
    TmdObject*     obj   = task->extra;
    s16            n;

    n = --work->field_50;
    if (n >= 0) {
        coord->coord.t[0] += work->field_0;
        coord->coord.t[1] += work->field_4;
        coord->coord.t[2] += work->field_8;
        coord->flg         = 0;
    } else {
        work->field_50 = 0;
    }
    if (Game_Session->field_4D != 0) {
        if (D_acropolis_helicopter_landing_pad_80182370[(u8)Game_Session->field_4] != 0) {
            obj->field_C &= 0xFF7F;
        } else {
            obj->field_C |= 0x80;
        }
    }
}

/// Points the model's light / colour matrices at the work block's own copies
/// and loads the room's three flat lights into them.
void func_acropolis_helicopter_landing_pad_8017D7B0(Task* task)
{
    AhlpEnemyWork* work = (AhlpEnemyWork*)task->idMap;
    TmdObject*     obj  = task->extra;
    GsF_LIGHT*     light;
    s32            i;

    obj->field_1C = &work->lightMtx;
    obj->field_20 = &work->colorMtx;
    for (i = 0, light = D_acropolis_helicopter_landing_pad_80182340; i < 3; i++, light++) {
        Gfx_SetFlatLight(i, light, &work->lightMtx, &work->colorMtx);
    }
}

/// Msg 0x7D3 handler: repositions the model by `msg->phase`. Phases 0 and 1
/// take the first / second placement, arm the 0x78 countdown and reset the
/// work block's first three words with the step set to -0x19 / +0x19; phase 2
/// returns to the first placement and clears the countdown.
s32 func_acropolis_helicopter_landing_pad_8017D824(Task* task, s32 msgId, AhlpMsg7D3* msg)
{
    AhlpEnemyWork* work = (AhlpEnemyWork*)task->idMap;

    switch (msg->phase) {
        case 0:
            func_acropolis_helicopter_landing_pad_8017D8E8(task, 0, &D_acropolis_helicopter_landing_pad_80182394, 0);
            work->field_50 = 0x78;
            work->field_0  = 0;
            work->field_4  = -0x19;
            work->field_8  = 0;
            break;
        case 1:
            func_acropolis_helicopter_landing_pad_8017D8E8(task, 0, &D_acropolis_helicopter_landing_pad_801823AC, 0);
            work->field_50 = 0x78;
            work->field_0  = 0;
            work->field_4  = 0x19;
            work->field_8  = 0;
            break;
        case 2:
            func_acropolis_helicopter_landing_pad_8017D8E8(task, 0, &D_acropolis_helicopter_landing_pad_80182394, 0);
            work->field_50 = 0;
            break;
    }
    return 0;
}

/// Msg 0x7D4 handler, also called directly by the 0x7D3 handler. Places the
/// task's model at `placement`: copies the position onto the coordinate's
/// translation, the Euler angles onto its rotation, rebuilds the rotation
/// matrix and marks the coordinate dirty.
s32 func_acropolis_helicopter_landing_pad_8017D8E8(Task* task, s32 msgId, RoomPlacement* placement, s32 arg3)
{
    RoomCoord* coord;

    coord             = (RoomCoord*)((TmdObject*)task->extra)->field_8;
    coord->coord.t[0] = placement->pos.vx;
    coord->coord.t[1] = placement->pos.vy;
    coord->coord.t[2] = placement->pos.vz;
    coord->rot.vx     = placement->rot.vx;
    coord->rot.vy     = placement->rot.vy;
    coord->rot.vz     = placement->rot.vz;
    RotMatrix(&coord->rot, &coord->coord);
    coord->flg = 0;
    return 0;
}

INCLUDE_RODATA("rooms/nonmatchings/acropolis_helicopter_landing_pad/acropolis_helicopter_landing_pad", D_acropolis_helicopter_landing_pad_8017D5C0);

INCLUDE_ASM("rooms/nonmatchings/acropolis_helicopter_landing_pad/acropolis_helicopter_landing_pad", func_acropolis_helicopter_landing_pad_8017D964);

/// Per-frame phase tick of the helipad script. In phase 1 it posts msg 0x7D6
/// to slot 4; once that is refused and no cutscene/among-us mode
/// (`D_80114C12`) or blocker (`D_80071075`) is active it advances to phase 2,
/// starts the second script block and queues sound 0xA2. Room 5 of the
/// session raises `D_acropolis_helicopter_landing_pad_80184E0C`; a cleared
/// `Game_Session->field_1` resets `D_acropolis_helicopter_landing_pad_80187F84`.
void func_acropolis_helicopter_landing_pad_8017D9BC(void)
{
    s32 phase = D_acropolis_helicopter_landing_pad_80184D9C;

    if (phase == 1) {
        if (Gp_DispatchMsg(Gp_LookupSlot4(0), 0x7D6, 0, 0) == 0) {
            if ((D_80114C12 != phase) && (D_80071075 == 0)) {
                D_acropolis_helicopter_landing_pad_80184D9C = 2;
                func_800E8634((s32)&D_acropolis_helicopter_landing_pad_80184124, 0,
                              (s32)&D_acropolis_helicopter_landing_pad_801844B4);
                func_800E3FAC(0xA2, 8);
            }
        }
    }
    if ((u8)Game_Session->field_4 == 5) {
        D_acropolis_helicopter_landing_pad_80184E0C = 1;
    }
    if (Game_Session->field_1 == 0) {
        D_acropolis_helicopter_landing_pad_80187F84 = 0;
    }
}

INCLUDE_RODATA("rooms/nonmatchings/acropolis_helicopter_landing_pad/acropolis_helicopter_landing_pad", D_acropolis_helicopter_landing_pad_8017D5E4);

INCLUDE_ASM("rooms/nonmatchings/acropolis_helicopter_landing_pad/acropolis_helicopter_landing_pad", func_acropolis_helicopter_landing_pad_8017DA9C);

/// Frame-counted timeline task: three phases, each spawning the enemy task
/// (`func_..._8017E618`) and, a few frames later, a script-18 pair; kills
/// itself at frame 0x280.
void func_acropolis_helicopter_landing_pad_8017DE78(Task* task)
{
    switch (task->state) {
        case 0xC8:
        case 0x0:
            func_acropolis_helicopter_landing_pad_8017E618(0xF, 2);
            break;
        case 0x7:
        case 0xCF:
            Gp_SpawnScript18((s32)&D_acropolis_helicopter_landing_pad_80187D68, (s32)&D_acropolis_helicopter_landing_pad_80187D78);
            break;
        case 0x19A:
            func_acropolis_helicopter_landing_pad_8017E618(0x16, 3);
            break;
        case 0x1A5:
            Gp_SpawnScript18((s32)&D_acropolis_helicopter_landing_pad_80187D50, (s32)&D_acropolis_helicopter_landing_pad_80187D60);
            break;
        case 0x208:
            func_acropolis_helicopter_landing_pad_8017E618(0x1E, 4);
            break;
        case 0x217:
            Gp_SpawnScript18((s32)&D_acropolis_helicopter_landing_pad_80187D40, (s32)&D_acropolis_helicopter_landing_pad_80187D48);
            break;
        case 0x280:
            Task_Kill(task);
            break;
    }
    task->state += 1;
}

void func_acropolis_helicopter_landing_pad_8017DFCC(Task* arg0)
{
    s32 temp_v1;

    temp_v1 = arg0->state;
    switch (temp_v1) {
        case 0:
            Gp_FillPlayerHpMp();
            Gp_ClearCollectedBit(0x101);
            Gp_ClearCollectedBit(0x102);
            Gp_SetItemSeenBit(0x102, 1);
            GameFlag_SetNibble(3, 0);
            GameFlag_SetNibble(0x155, 7);
            Task_SpawnFromTable(&D_acropolis_helicopter_landing_pad_80184E68, 0, 0, 0);
            arg0->state = (s32)(arg0->state + 1);
            return;
        case 1:
            arg0->state = 2;
            return;
        case 2:
            SndEvt_EnqueueType7(0x80000000, 0);
            Mc_SaveData.field_5C5 = 1;
            Mc_SaveData.field_7   = 1;
            Mc_SaveData.field_6   = 0x12;
            Mc_SaveData.field_8   = 1;
            Mc_SaveData.field_5   = 1;
            D_80071076            = 1;
            Task_Spawn(0, 0x11, 0, 0);
            Display_ReleaseRef();
            Task_Kill(arg0);
            return;
    }
}

/// Turn-to-heading task: rotates the player actor's yaw
/// (`GameActor.field_52`, masked to 12 bits) to `spawnArg1` in `0x100` steps
/// along the shorter direction. State 0 picks the unwrapped start angle
/// (`yaw`, or `yaw +/- 0x1000` when that is closer to the target) and the
/// step sign; state 1 steps, clamps onto the target and kills the task.
///
/// `tmp` carries three unrelated values (the first abs distance, the
/// "wrapped is closer" flag and state 1's new yaw), and `target` is re-read
/// in state 1: both are what puts the `slt` result and the state-1 sum in
/// the same registers as the original.
void func_acropolis_helicopter_landing_pad_8017E0F8(Task* arg0)
{
    GameActor* actor = (GameActor*)((Task*)Game_GetPtrSlot(3))->idMap;
    s32        wrapped;
    s32        tmp;
    s32        dist;
    s32        target;
    s32        cur;

    switch (arg0->state) {
        case 0:
            D_acropolis_helicopter_landing_pad_80187F74 = actor->field_52 & 0xFFF;
            if (arg0->spawnArg1 < D_acropolis_helicopter_landing_pad_80187F74) {
                wrapped = D_acropolis_helicopter_landing_pad_80187F74 - 0x1000;
            } else {
                wrapped = D_acropolis_helicopter_landing_pad_80187F74 + 0x1000;
            }
            target = arg0->spawnArg1;
            cur    = D_acropolis_helicopter_landing_pad_80187F74;
            tmp    = wrapped - target;
            if (tmp < 0) {
                tmp = -tmp;
            }
            dist = cur - target;
            if (dist < 0) {
                dist = -dist;
            }
            tmp = tmp < dist;
            if (tmp) {
                D_acropolis_helicopter_landing_pad_80187F74 = wrapped;
            }
            if (arg0->spawnArg1 > D_acropolis_helicopter_landing_pad_80187F74) {
                D_acropolis_helicopter_landing_pad_80187F78 = 0x100;
            } else {
                D_acropolis_helicopter_landing_pad_80187F78 = -0x100;
            }
            arg0->state++;
            break;
        case 1:
            tmp                                         = D_acropolis_helicopter_landing_pad_80187F74 + D_acropolis_helicopter_landing_pad_80187F78;
            D_acropolis_helicopter_landing_pad_80187F74 = tmp;
            if (D_acropolis_helicopter_landing_pad_80187F78 > 0) {
                target = arg0->spawnArg1;
                if (target < tmp) {
                    D_acropolis_helicopter_landing_pad_80187F74 = target;
                    Task_Kill(arg0);
                }
            }
            if (D_acropolis_helicopter_landing_pad_80187F78 < 0) {
                if (D_acropolis_helicopter_landing_pad_80187F74 < arg0->spawnArg1) {
                    D_acropolis_helicopter_landing_pad_80187F74 = arg0->spawnArg1;
                    Task_Kill(arg0);
                }
            }
            actor->field_52 = D_acropolis_helicopter_landing_pad_80187F74;
            break;
    }
}

/// Helipad rotor / lift task: swings the player model's coord part 4 about X
/// by `-angle * 0x60 / 0x1000` and updates it. State 1 ramps
/// `D_acropolis_helicopter_landing_pad_80187F7C` up to 0x1000 (then state 2),
/// state 2 ramps it back to 0 (then state 0), state 0 resets it.
///
/// The extra locals are dead: the original wrote `dir` and reserved the
/// other aggregates (0x50 bytes of frame) for code that no longer runs, and
/// the constant stores survive in the binary.
void func_acropolis_helicopter_landing_pad_8017E270(Task* task)
{
    GsCOORDINATE2* coord;
    SVECTOR        unusedA;
    VECTOR         unusedB;
    VECTOR         dir;
    MATRIX         unusedM;
    SVECTOR        unusedC;

    coord  = &((TmdObject*)((Task*)Game_GetPtrSlot(3))->extra)->field_8[4];
    dir.vx = -0x249;
    dir.vy = 0;
    dir.vz = 0xB8;

    switch (task->state) {
        case 0:
            D_acropolis_helicopter_landing_pad_80187F7C = 0;
            break;
        case 1:
            Gfx_RotMatrixX(&coord->coord, -(D_acropolis_helicopter_landing_pad_80187F7C * 0x60) / 0x1000, 0);
            coord->flg = 0;
            Gp_UpdateCoord(coord);
            D_acropolis_helicopter_landing_pad_80187F7C += 0x190;
            if (D_acropolis_helicopter_landing_pad_80187F7C > 0x1000) {
                D_acropolis_helicopter_landing_pad_80187F7C = 0x1000;
                task->state                                 = 2;
            }
            break;
        case 2:
            Gfx_RotMatrixX(&coord->coord, -(D_acropolis_helicopter_landing_pad_80187F7C * 0x60) / 0x1000, 0);
            coord->flg = 0;
            Gp_UpdateCoord(coord);
            D_acropolis_helicopter_landing_pad_80187F7C -= 0x190;
            if (D_acropolis_helicopter_landing_pad_80187F7C < 0) {
                D_acropolis_helicopter_landing_pad_80187F7C = 0;
                task->state                                 = 0;
            }
            break;
    }
}

/// Message 0x13EE handler: copies the requested `GpSaveLoc` to `dst`. For a
/// warp into stage 0xF it consults the room's phase
/// (`D_acropolis_helicopter_landing_pad_80184D9C`): phase 0 queues sound
/// event 0x1E and refuses the warp (returns 1); phase 2 starts cap slot 9
/// first. `field_5` set skips the side effect either way.
s32 func_acropolis_helicopter_landing_pad_8017E3F0(Task* task, s32 msgId, GpSaveLoc* src, GpSaveLoc* dst)
{
    *dst = *src;
    if (*(u16*)src == 0xF) {
        if (D_acropolis_helicopter_landing_pad_80184D9C == 0) {
            if (src->field_5 == 0) {
                SndEvt_EnqueueType7(-1, 0x1E);
            }
            return 1;
        }
        if (D_acropolis_helicopter_landing_pad_80184D9C == 2) {
            if (src->field_5 == 0) {
                Gp_StartCapSlot(9, 1, 0);
            }
            return 0;
        }
        return 0;
    }
    return 1;
}

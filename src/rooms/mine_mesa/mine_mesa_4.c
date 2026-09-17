#include "common.h"
#include "gameplay/1BC.h"
#include "gameplay/D4.h"
#include "gameplay/gameplay.h"
#include "main/display.h"
#include "main/mem.h"
#include "main/task.h"
#include "rooms/room_common.h"

extern u8       D_801156F9;
extern s8       D_8007106B;
extern TaskDesc D_mine_mesa_80181990;

/// The mesa's run: one `SVECTOR` position per frame, sent as a `RoomPlacement`.
extern SVECTOR D_mine_mesa_80184184[];

extern Task* D_mine_mesa_80189B58;
extern Task* RoomsShared8018459cTask;

/// Head-aim record `func_mine_mesa_8017E2A4` allocates and parks in
/// `Task::idMap`, handed straight to `func_800B17D4` as its `arg2`: the yaw and
/// pitch clamps that function widens against the head's current pose, and the
/// `rate` fraction of the remaining angle this overlay ramps one 0x100 step per
/// frame.
///
/// The field roles are `GpHeadAim`'s, but the two readings of the record are
/// not the same size. This overlay allocates 12 bytes where `GpHeadAim` is 10,
/// and the other `func_800B17D4` callers that build the record the same way --
/// `func_mine_mesa_8017E15C`, `func_actor_361100_801627D4` and
/// `func_actor_450200_80131FA8` -- also allocate 12, so 12 is the record's size
/// and gameplay's 10 is the most `func_800B17D4` alone can see of it.
///
/// `rate` is `u16` here because the body reads it as an unsigned halfword and
/// reinterprets the stored value as `s16` for the clamp, which is what the
/// `lhu` / `sll` / `sra` sequence in the ROM says.
typedef struct MineMesaHeadAim {
    /* 0x0 */ s16  yawLimit;
    /* 0x2 */ s16  pitchLimit;
    /* 0x4 */ u16  rate;
    /* 0x6 */ s16  lastPitch;
    /* 0x8 */ s8   inited;
    /* 0x9 */ byte pad_9[0x3];
} MineMesaHeadAim;
STATIC_ASSERT_SIZEOF(MineMesaHeadAim, 0xC);

void func_mine_mesa_8017E024(Task* arg0)
{
    Display_SpawnWithOt(&D_mine_mesa_80181990, 1, 0, 0);
    D_8007106B = 1;
    Gp_SpawnViewTasks();
    Task_Kill(arg0);
}

/// Walks the mesa one step along `D_mine_mesa_80184184` per frame: sends slot 3
/// that entry as a `RoomPlacement` -- the table position with x pulled back
/// 0x64 and z pushed out 0xC8 -- and advances `killCountdown`. At 0x2E the mesa
/// has finished its run, and the task kills itself; the session's overlay-wait
/// gate cuts the run short the same way.
void func_mine_mesa_8017E074(Task* arg0)
{
    RoomPlacement rec;

    if (arg0->killCountdown >= 0x2E || Game_Session->field_5F != 0) {
        Task_Kill(arg0);
        return;
    }
    rec.pos.vx  = D_mine_mesa_80184184[arg0->killCountdown].vx;
    rec.pos.vy  = D_mine_mesa_80184184[arg0->killCountdown].vy;
    rec.pos.vz  = D_mine_mesa_80184184[arg0->killCountdown].vz;
    rec.pos.vx -= 0x64;
    rec.pos.vz += 0xC8;
    rec.rot.vx  = 0;
    rec.rot.vy  = 0x311;
    rec.rot.vz  = 0;
    arg0->killCountdown++;
    Gp_DispatchMsg(Game_GetPtrSlot(3), 0x3E9, (s32)&rec, 0);
}

/// Head-aim state of the mesa's run task, run only while `D_801156F9` is clear:
/// a missing slot-3 or slot-0xA task parks the state machine on -1. State 0
/// allocates the `MineMesaHeadAim` record into `Task::idMap` and seeds its
/// clamps to 0x300 yaw and 0x200 pitch; state 1 ramps its `rate` up toward
/// 0x1000 while `Task::spawnArg1` is set and back down toward 0 while it is
/// not, then hands the record to `func_800B17D4` between the slot-3 task whose
/// head turns and the slot-0xA task it turns toward -- the mirror of
/// `func_mine_mesa_8017E2A4`, which looks from slot 0xA. Every other state
/// kills the task and clears `RoomsShared8018459cTask`, and a state-0 NULL
/// allocation falls out of its own `if` into that same kill.
void func_mine_mesa_8017E15C(Task* arg0)
{
    Task*            turner;
    Task*            looker;
    MineMesaHeadAim* aim;
    s32              state;
    u16              rateUp;
    u16              rateDown;

    turner = Game_GetPtrSlot(3);
    looker = Game_GetPtrSlot(0xA);
    if (D_801156F9 == 0) {
        if ((turner == NULL) || (looker == NULL)) {
            arg0->state = -1;
        }
        state = arg0->state;
        switch (state) {
            case 0:
                aim = Mem_Calloc(sizeof(MineMesaHeadAim), false);
                if (aim != NULL) {
                    arg0->idMap     = (TaskIdMap*)aim;
                    aim->yawLimit   = 0x300;
                    aim->pitchLimit = 0x200;
                    arg0->state++;
                        /* fallthrough */
                    case 1:
                        aim = (MineMesaHeadAim*)arg0->idMap;
                        if (arg0->spawnArg1 != 0) {
                            rateUp    = aim->rate + 0x100;
                            aim->rate = rateUp;
                            if ((s16)rateUp >= 0x1001) {
                                aim->rate = 0x1000;
                            }
                        } else {
                            rateDown  = aim->rate - 0x100;
                            aim->rate = rateDown;
                            if ((s16)rateDown < 0) {
                                aim->rate = 0;
                            }
                        }
                        func_800B17D4(turner, looker, (GpHeadAim*)aim);
                        return;
                }
                /* fallthrough */
            default:
                Task_Kill(arg0);
                RoomsShared8018459cTask = NULL;
                break;
        }
    }
}

/// Head-aim state of the mesa's tracked task, run only while `D_801156F9` is
/// clear: a missing `Game_GetPtrSlot(0xA)` task parks the state machine on -1.
/// State 0 allocates the `MineMesaHeadAim` record into `Task::idMap` and seeds
/// its clamps to 0x300 yaw and 0x100 pitch; state 1 ramps its `rate` up toward
/// 0x1000 while `Task::spawnArg1` is set and back down toward 0 while it is
/// not, then hands the record to `func_800B17D4` between the
/// `Game_GetPtrSlot(0xA)` task whose head turns and the slot-3 task it turns
/// toward -- the reverse of `func_mine_mesa_8017E15C` and of
/// `func_actor_450200_80131FA8`, which look from slot 3. Every other state
/// kills the task and clears `D_mine_mesa_80189B58`, and a state-0 NULL
/// allocation falls out of its own `if` into that same kill.
void func_mine_mesa_8017E2A4(Task* arg0)
{
    Task*            looker;
    MineMesaHeadAim* aim;
    u16              rate;

    looker = Game_GetPtrSlot(0xA);
    if (D_801156F9 == 0) {
        if (looker == NULL) {
            arg0->state = -1;
        }
        switch (arg0->state) {
            case 0:
                aim = Mem_Calloc(sizeof(MineMesaHeadAim), false);
                if (aim != NULL) {
                    arg0->idMap     = (TaskIdMap*)aim;
                    aim->yawLimit   = 0x300;
                    aim->pitchLimit = 0x100;
                    arg0->state++;
                        /* fallthrough */
                    case 1:
                        aim = (MineMesaHeadAim*)arg0->idMap;
                        if (arg0->spawnArg1 != 0) {
                            rate      = aim->rate + 0x100;
                            aim->rate = rate;
                            if ((s16)rate >= 0x1001) {
                                aim->rate = 0x1000;
                            }
                        } else {
                            rate      = aim->rate - 0x100;
                            aim->rate = rate;
                            if ((s16)rate < 0) {
                                aim->rate = 0;
                            }
                        }
                        func_800B17D4(looker, Game_GetPtrSlot(3), (GpHeadAim*)aim);
                        return;
                }
                /* fallthrough */
            default:
                Task_Kill(arg0);
                D_mine_mesa_80189B58 = NULL;
                break;
        }
    }
}

/// State machine of the mesa's flare: a full-screen semi-transparent tile
/// whose colour is `Task::killCountdown`, dimmed 8 a frame from state 2 until
/// it goes negative and the task kills itself; the other states are the run-in
/// (0 seeds the countdown at 0xFF, 1 waits out `Task::spawnArg1`) and anything
/// else kills the task outright. The tile and a `DR_TPAGE` for it are carved
/// off `Gpu_PrimCursor` and linked into `Gpu_CurrentOt[3]` every frame,
/// including the frames the switch kills the task on -- only the colours differ
/// there, since `r`/`g`/`b` are read before the switch.
///
/// This is `func_actor_503500_80132990` minus its `D_801153F4` gate and minus
/// the `Game_Session->field_5F != 0` term of its state-1 test; the tile packet
/// itself is built byte-for-byte the same way.
void func_mine_mesa_8017E3E0(Task* arg0)
{
    TILE*     tile;
    DR_TPAGE* dr;
    u8        r, g, b;

    r = g = b = arg0->killCountdown;
    switch (arg0->state) {
        case 0:
            arg0->killCountdown = 0xFF;
            arg0->state++;
            break;
        case 1:
            if (--arg0->spawnArg1 < 0) {
                arg0->state++;
            }
            break;
        case 2:
            arg0->killCountdown -= 8;
            if (arg0->killCountdown < 0) {
                Task_Kill(arg0);
            }
            break;
        default:
            Task_Kill(arg0);
            break;
    }
    tile           = (TILE*)Gpu_PrimCursor;
    Gpu_PrimCursor = (DR_TPAGE*)(tile + 1);
    setTile(tile);
    SetSemiTrans(tile, 1);
    tile->x0 = -160;
    tile->y0 = -120;
    tile->w  = 320;
    tile->h  = 240;
    setRGB0(tile, r, g, b);
    addPrim(Gpu_CurrentOt + 3, tile);
    dr             = Gpu_PrimCursor;
    Gpu_PrimCursor = dr + 1;
    setDrawTPage(dr, 1, 0, getTPage(0, 2, 320, 0));
    addPrim(Gpu_CurrentOt + 3, dr);
}

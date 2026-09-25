#include "common.h"

#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/abs.h>
#include <psyq/inline_c.h>
#include "gte.h"

#include "gameplay/1A8.h"
#include "gameplay/1BC.h"
#include "gameplay/3A34.h"
#include "gameplay/3CD8.h"
#include "gameplay/3FB8.h"
#include "gameplay/D4.h"
#include "gameplay/gameplay.h"
#include "main/display.h"
#include "main/fs.h"
#include "main/gfx.h"
#include "main/mc.h"
#include "main/mem.h"
#include "main/pad.h"
#include "main/session.h"
#include "main/sound.h"
#include "main/stream.h"
#include "main/task.h"
#include "main/tmd.h"
#include "main/unknown_syms.h"
#include "main/wipsys.h"
#include "rooms/room.h"
#include "rooms/room_common.h"

/// Scratch block for the plaza's eight-quad glow. `vec` holds the coordinate
/// origin, `sx` / `sy` its projected screen position, and `half` the radius
/// scaled by inverse depth. The draw task takes 0x14 bytes from G_SCRATCH_HEAD.
typedef struct AcropolisPlazaGlowScratch {
    /* 0x00 */ s32     otz;
    /* 0x04 */ s32     half;
    /* 0x08 */ SVECTOR vec;
    /* 0x10 */ s16     sx;
    /* 0x12 */ s16     sy;
} AcropolisPlazaGlowScratch;
STATIC_ASSERT_SIZEOF(AcropolisPlazaGlowScratch, 0x14);

/// Projected beam vertices and glow parameters in a 0x60-byte scratch block.
typedef struct AcropolisPlazaBeamScratch {
    /* 0x00 */ s32     screen[7];
    /* 0x1C */ s32     otz;
    /* 0x20 */ s32     half;
    /* 0x24 */ SVECTOR vec[7];
    /* 0x5C */ s16     sx;
    /* 0x5E */ s16     sy;
} AcropolisPlazaBeamScratch;
STATIC_ASSERT_SIZEOF(AcropolisPlazaBeamScratch, 0x60);

/// Flare projection and two inverse-depth radii in a 0x4C-byte scratch block.
typedef struct AcropolisPlazaFlareScratch {
    /* 0x00 */ u8      pad_0[0x14];
    /* 0x14 */ s32     otz;
    /* 0x18 */ s32     half;
    /* 0x1C */ s32     inner;
    /* 0x20 */ SVECTOR vec;
    /* 0x28 */ u8      pad_28[0x20];
    /* 0x48 */ s16     sx;
    /* 0x4A */ s16     sy;
} AcropolisPlazaFlareScratch;
STATIC_ASSERT_SIZEOF(AcropolisPlazaFlareScratch, 0x4C);

/// Identified tail of the beam task's spawnArg2: sweep angle, length and falloff.
typedef struct AcropolisPlazaBeamWork {
    /* 0x00 */ u8  pad[0x24];
    /* 0x24 */ s16 yaw;
    /* 0x26 */ s16 depth;
    /* 0x28 */ s16 spread;
} AcropolisPlazaBeamWork;

/// Spawn argument the plaza's scene task (`func_acropolis_plaza_8017DFE0`)
/// reads once in state 0: `view` seeds both `CdCmd_Queue.field_1EE` and
/// `field_1EA`, and a non-zero `noStream` skips the opening stream request
/// altogether.
typedef struct AcropolisPlazaSceneArg {
    /* 0x0 */ u16 view;
    /* 0x2 */ u16 noStream;
} AcropolisPlazaSceneArg;

/// Work block the plaza's scene task (`func_acropolis_plaza_8017DFE0`) keeps at
/// `Task::work`; it is a 0x34 allocation, distinct from the sequence task's
/// `AcropolisPlazaWork`. `pos` is the world position
/// `func_acropolis_plaza_8017DD90` centres its vertex box on; it is refreshed
/// every frame from the plaza's per-view position table, and `distX` is how far
/// that point sits ahead of the camera in X. `fwd` / `back` are the two "the
/// player has walked past the edge of this shot" flags the task recomputes each
/// frame from `distX`, `prev*` their values from the previous frame, and
/// `field_2E` selects the 10- or 40-frame stream step used when re-seeking.
typedef struct AcropolisPlazaSceneWork {
    /* 0x00 */ MATRIX*    mtx;
    /* 0x04 */ Task*      slot3;
    /* 0x08 */ GameActor* actor;
    /* 0x0C */ VECTOR3    pos;
    /* 0x18 */ s32        distX;
    /* 0x1C */ byte       pad_1C[0x2];
    /* 0x1E */ s16        prevSide;
    /* 0x20 */ s16        streamLen;
    /* 0x22 */ s16        fwd;
    /* 0x24 */ s16        back;
    /* 0x26 */ u16        baseView;
    /* 0x28 */ byte       pad_28[0x2];
    /* 0x2A */ s16        prevFwd;
    /* 0x2C */ s16        prevBack;
    /* 0x2E */ s16        field_2E;
    /* 0x30 */ byte       pad_30[0x4];
} AcropolisPlazaSceneWork;
STATIC_ASSERT_SIZEOF(AcropolisPlazaSceneWork, 0x34);

/// Work block the plaza's sequence task (`func_acropolis_plaza_80180054`)
/// allocates with `Mem_Malloc(0x28, 0)` and parks in `Task::work` -- that slot
/// is not a `TaskIdMap` here. State 0 caches the slot-3 task in `slot3` and the
/// task it spawns from entry 5 of the room's table in `field_C`; state 3 spawns
/// entry 1 into `field_8`, handing it `&field_10` as its spawn argument. The
/// halfwords from 0x1E on are the per-emitter "already playing" flags
/// `func_acropolis_plaza_8017F770` tests and sets, one per ambience voice
/// started by `func_acropolis_plaza_8017F9EC`.
///
/// 0x14..0x1D belong to `func_acropolis_plaza_8017FB50`, the scene stepper:
/// `step` is its own state machine, `evtId`/`evtKind`/`evtSub` latch the
/// pending `GpObj4C` event `Gp_TakePendingObj4C` hands it, `streamFrame`
/// snapshots `CdCmd_Queue.field_1EE` when the event arrives, and `variant`
/// counts how many times the entry-6 scene has run (capped at 2) so each pass
/// spawns it with the next `Task_SpawnFromTable` arg2.
typedef struct AcropolisPlazaWork {
    /* 0x00 */ byte  pad_0[0x4];
    /* 0x04 */ Task* slot3;
    /* 0x08 */ Task* field_8;
    /* 0x0C */ Task* field_C;
    /* 0x10 */ s16   field_10;
    /* 0x12 */ s16   field_12;
    /* 0x14 */ u16   step;
    /* 0x16 */ u16   evtId;
    /* 0x18 */ u8    evtKind;
    /* 0x19 */ u8    evtSub;
    /* 0x1A */ u16   streamFrame;
    /* 0x1C */ u16   variant;
    /* 0x1E */ u16   sfxState1E;
    /* 0x20 */ u16   sfxState20;
    /* 0x22 */ u16   sfxState22;
    /* 0x24 */ u16   sfxState24;
    /* 0x26 */ byte  pad_26[0x2];
} AcropolisPlazaWork;
STATIC_ASSERT_SIZEOF(AcropolisPlazaWork, 0x28);

/// Work block the plaza's opening sequence (`func_acropolis_plaza_8017ECF8`)
/// allocates with `Mem_Malloc(8, 0)` and parks in `Task::work` -- that slot is
/// not a `TaskIdMap` here. `slot3` caches the slot-3 task every message in the
/// sequence is addressed to; `timer` is the frame counter the waiting states
/// step (0x3D frames in state 7, 0xB in state 11, 2 in state 12).
typedef struct AcropolisPlazaOpeningWork {
    /* 0x0 */ Task* slot3;
    /* 0x4 */ s16   timer;
    /* 0x6 */ byte  pad_6[0x2];
} AcropolisPlazaOpeningWork;
STATIC_ASSERT_SIZEOF(AcropolisPlazaOpeningWork, 0x8);

/// The one scratch buffer the plaza's opening sequence shares between its area
/// lookup and its last stream request. `key` is the location key states 6 and 8
/// build from `gGameSession` before walking the nested area records for the
/// 0x6C room, and `slot` is the CD stream-slot triple state 13 hands to
/// `CdCmd_Enqueue(0x71, ...)`; the task only ever has one of them in flight.
typedef union AcropolisPlazaOpeningBuf {
    /* 0x0 */ GpAreaKey key;
    /* 0x0 */ u8        slot[4];
} AcropolisPlazaOpeningBuf;

/// Work block the plaza's warp task (`func_acropolis_plaza_8017E7E4`) allocates
/// with `Mem_Malloc(8, 0)` and parks in `Task::work` -- that slot is not a
/// `TaskIdMap` here. It only caches the slot-3 task every message in the
/// sequence (0x3F2 place, 0x3EE warp, 0x3F0 poll) is addressed to; the
/// trailing four bytes are zeroed by `Mem_Set` and never read.
typedef struct AcropolisPlazaWarpWork {
    /* 0x0 */ Task* slot3;
    /* 0x4 */ byte  pad_4[0x4];
} AcropolisPlazaWarpWork;

/// Work block the plaza's cutscene tasks reach through `Task::spawnArg2`.
/// Every one of them (`func_acropolis_plaza_8017E7E4`, `..._8017E9A8`,
/// `..._8017F48C`, `..._8017F620`) runs the same handoff once `CdCmd_IsIdle`
/// reports the stream has finished: latch `CdCmd_Queue.field_1EE` into
/// `field_1A`, kill the task at `task`, and (in `..._8017F620`) run the
/// capture command named by `capCmd`. Only those three fields are identified,
/// so this declaration is deliberately partial.
typedef struct AcropolisPlazaCutWork {
    /* 0x00 */ byte  pad_0[0x8];
    /* 0x08 */ Task* task;
    /* 0x0C */ byte  pad_C[0xC];
    /* 0x18 */ s8    capCmd;
    /* 0x19 */ byte  pad_19[0x1];
    /* 0x1A */ u16   field_1A;
} AcropolisPlazaCutWork;

/// The 0x14-byte weapon record msg 0x3E8 takes, seen at its offset inside
/// `AcropolisPlazaTailMsg`: the plaza's opening tail builds it eight bytes into
/// the shared buffer, which is what makes that buffer 0x1C rather than 0x18
/// bytes long.
typedef struct AcropolisPlazaWeaponMsg {
    /* 0x00 */ byte      pad_0[0x8];
    /* 0x08 */ GpAnimArg rec;
} AcropolisPlazaWeaponMsg;
STATIC_ASSERT_SIZEOF(AcropolisPlazaWeaponMsg, 0x1C);

/// The one scratch buffer `func_acropolis_plaza_8017E9A8` builds its late
/// payloads in. The task only ever has one of them in flight, so all three
/// views share a single frame slot, and the union is what makes that sharing
/// explicit: `slot` is the CD stream-slot triple handed to
/// `CdCmd_Enqueue(0x72, ...)` in state 3, `weapon.rec` the record msg 0x3E8
/// takes in state 5, and `place` the position + Euler rotation the 0x3E9
/// placement that follows it takes.
typedef union AcropolisPlazaTailMsg {
    /* 0x0 */ u8                      slot[4];
    /* 0x0 */ AcropolisPlazaWeaponMsg weapon;
    /* 0x0 */ GpXformArg              place;
} AcropolisPlazaTailMsg;
STATIC_ASSERT_SIZEOF(AcropolisPlazaTailMsg, 0x1C);

extern s32 D_80070F70;
extern s16 D_acropolis_plaza_801987E0[];

extern s8       D_8007106B;
extern s16      D_80071076;
extern TaskDesc D_acropolis_plaza_80183824[];

/// Gate `func_acropolis_plaza_8017FB50` applies to a pending `GpObj4C` event
/// whose id has the sign bit clear; a main-executable global with no module
/// header yet.
extern u8 D_80073BAC;

/// The three scene `GpObj4A` nodes the plaza unlinks: `..._801991F0` when the
/// opening stream hands over, and `..._801991A4` / `..._8019923C` depending on
/// which event kind ended the scene.
extern GpObj4A D_acropolis_plaza_801991A4;
extern GpObj4A D_acropolis_plaza_801991F0;
extern GpObj4A D_acropolis_plaza_8019923C;

extern void Stage_RequestFromAreaTable(s32 arg0);
extern void Stage_RequestMidiFromMap(s32 arg0);

/// Main-executable globals with no module header yet: `D_80073BA9` is the
/// equipped-weapon index the slot-3 msg 0x3E8 record is keyed on, and
/// `D_8007218A` picks which of the two weapon-id bases that record uses.
extern u8 D_80073BA9;
extern s8 D_8007218A;

/// Script block the plaza hands to slot 3 as msg 0x3F4 entry 0xB; it lives in
/// the main executable, not in this overlay.
extern s32 D_801797FC;

/// The block `func_acropolis_plaza_8017E9A8` runs once its stream reports in.
extern u8 D_acropolis_plaza_80182B24[];

/// The pair of blocks `func_acropolis_plaza_8017E7E4` hands to `func_800E8634`
/// once the streamed scene it waits on has finished.
extern u8 D_acropolis_plaza_80182734[];
extern u8 D_acropolis_plaza_80182A34[];

/// The pair of blocks the opening sequence hands to `func_800E8634` in state 4,
/// and the two it runs on its own with `func_800E8614` in states 10 and 14.
extern u8 D_acropolis_plaza_80182C90[];
extern u8 D_acropolis_plaza_80182F18[];
extern u8 D_acropolis_plaza_801830DC[];
extern u8 D_acropolis_plaza_801834B4[];

/// The three blocks `func_acropolis_plaza_8017F48C` picks between with
/// `Task::spawnArg1` before handing one to `func_800E8614`.
extern u8 D_acropolis_plaza_80183554[];
extern u8 D_acropolis_plaza_8018365C[];
extern u8 D_acropolis_plaza_80183764[];

/// Two four-vertex quads facing each other across the plaza's scene object:
/// one at x - 0xBB8, one at x + 0x7D0, each spanning y .. y + 0x3E8 and
/// z - 0x1000 .. z + 0x3000. The second quad's vertices run in the opposite
/// z order, flipping its winding.
extern SVECTOR D_acropolis_plaza_80198B90[8];

/// The plaza's view tables, one per camera set. Each entry is a *pair* of
/// `GpViewRec`s -- the two shots the stream alternates between -- indexed by
/// `CdCmd_Queue.field_1EE - 1`, so a table row is 0x48 bytes.
extern GpViewRec D_acropolis_plaza_801838B8[][2];
extern GpViewRec D_acropolis_plaza_8018A938[][2];
extern GpViewRec D_acropolis_plaza_8018CAFC[][2];
extern GpViewRec D_acropolis_plaza_8018F530[][2];
extern GpViewRec D_acropolis_plaza_8018F9B4[][2];

/// The plaza's camera table: one world position per stream view, indexed by
/// `CdCmd_Queue.field_1EE - 1`.
extern VECTOR3 D_acropolis_plaza_801907C4[];

/// Ambient-effect anchor points, one `SVECTOR` per effect slot. The plaza's
/// three effect bursts index this table with the same slot number they pass to
/// `Gp_SpawnEff`, so entries 1-6, 7-0xA and 0xC-0x12 belong to the 0x60098,
/// 0x60099 and 0x60096 flavours respectively. The block runs well past entry
/// 0x12, so this declaration is left unsized.
extern SVECTOR D_acropolis_plaza_80198820[];

/// Per-frame service step for the plaza's streamed cutscene commands.
///
/// Only runs while the slot `CdCmd_Queue.readIdx` selects holds one of the
/// stream opcodes 0x71..0x73; the entry packs the stream slot in `idB0` and a
/// 16-bit argument in `idB1:idB2`. Step 0 waits for `CdCmd_PollStatus`: status
/// 0 keeps waiting, status 2 flushes the drive first, and status 1 (or 2)
/// promotes a 0x72 entry to 0x71 -- clearing the MDEC strip counters -- kicks
/// the decoder, primes `func_8001FAE0` and advances to step 1. Step 1 polls
/// `func_8001FAE0` every frame and retires the command once it reports done.
void func_acropolis_plaza_8017D6D4(void)
{
    CdCmdQueue* q;
    CdCmdEntry* e;
    s16         slot;
    s16         arg;
    s32         cmd;

    q    = &CdCmd_Queue;
    e    = &q->entries[q->readIdx];
    cmd  = e->cmd;
    slot = (s8)e->idB0;
    arg  = e->idB2 | (e->idB1 << 8);

    if (cmd != 0) {
        if (cmd >= 0) {
            if (cmd < 0x74) {
                if (cmd >= 0x71) {
                    switch (q->step) {
                        case 0:
                            switch ((s16)CdCmd_PollStatus(0, 0)) {
                                case 0:
                                    break;
                                case 2:
                                    CdFlush();
                                    /* fallthrough */
                                case 1:
                                    if (q->entries[q->readIdx].cmd == 0x72) {
                                        D_8005EAEC                 = 0;
                                        D_8005EAEE                 = 0;
                                        q->entries[q->readIdx].cmd = 0x71;
                                    }
                                    Stream_KickDecode(slot & 0xFFFF);
                                    if (q->entries[q->readIdx].cmd == 0x71) {
                                        func_8001FAE0(0, arg);
                                    } else if (q->entries[q->readIdx].cmd == 0x73) {
                                        func_8001FAE0(1, q->field_48);
                                    }
                                    q->step++;
                                    /* fallthrough */
                                default:
                                    goto poll;
                            }
                            break;
                        case 1:
                        poll:
                            if (q->entries[q->readIdx].cmd == 0x71) {
                                if (func_8001FAE0(0, arg) != 0) {
                                    CdCmd_AdvanceRead();
                                }
                            } else if (q->entries[q->readIdx].cmd == 0x73) {
                                if (func_8001FAE0(1, q->field_48) != 0) {
                                    CdCmd_AdvanceRead();
                                }
                            }
                            break;
                    }
                }
            }
        }
    }
}

/// Fade the plaza to white and tear the task down.
///
/// State 0 allocates the `OverlayFadeWork` ramp at `Task::work` and
/// zeroes it; a failed allocation kills the task outright. State 1 runs every
/// frame: it links a semi-transparent full-screen `TILE` (`-0xA0,-0x78`,
/// `0x140x0xF0`) plus the `0xE1000240` `DR_TPAGE` into `gGpuCurrentOt[-16]`,
/// tinting the tile `r`/`g`/`r`, then steps all three channels by
/// `Task::spawnArg1`. Once `r` saturates past 0xFF the screen is fully covered,
/// so the task blanks the display and kills itself.
void func_acropolis_plaza_8017D8AC(Task* arg0)
{
    OverlayFadeWork* fade;
    OverlayFadeWork* alloc;
    u8               r;
    u8               g;
    TILE*            tile;
    DR_TPAGE*        dr;

    fade = (OverlayFadeWork*)arg0->work;
    switch (arg0->state) {
        case 0:
            alloc      = (OverlayFadeWork*)Mem_Malloc(8, 0);
            arg0->work = (TaskIdMap*)alloc;
            if (alloc == NULL) {
                goto kill;
            }
            fade         = alloc;
            fade->b      = 0;
            fade->g      = 0;
            fade->r      = 0;
            arg0->state += 1;
            /* fallthrough */
        case 1:
            r              = fade->r;
            g              = fade->g;
            tile           = (TILE*)gGpuPrimCursor;
            gGpuPrimCursor = tile + 1;
            setlen(tile, 3);
            setcode(tile, 0x62);
            tile->r0 = r;
            tile->g0 = g;
            tile->b0 = r;
            tile->x0 = -0xA0;
            tile->y0 = -0x78;
            tile->w  = 0x140;
            tile->h  = 0xF0;
            addPrim(gGpuCurrentOt - 16, tile);

            dr             = gGpuPrimCursor;
            gGpuPrimCursor = dr + 1;
            setlen(dr, 1);
            dr->code[0] = 0xE1000240;
            addPrim(gGpuCurrentOt - 16, dr);

            fade->r += (u16)arg0->spawnArg1;
            fade->g += (u16)arg0->spawnArg1;
            fade->b += (u16)arg0->spawnArg1;
            if (fade->r >= 0x100) {
                SetDispMask(0);
            kill:
                taskKill(arg0);
            }
            break;
    }
}

/// Fade the plaza up from white, then tear the task down.
///
/// State 0 allocates the `OverlayFadeWork` ramp at `Task::work` and
/// saturates all three channels at 0xFF; a failed allocation kills the task
/// outright. State 1 runs every frame: it links a semi-transparent full-screen
/// `TILE` (`-0xA0,-0x78`, `0x140x0xF0`) plus the `0xE1000240` `DR_TPAGE` into
/// `gGpuCurrentOt[-16]`, tinting the tile `r`/`g`/`r`, then steps all three
/// channels down by `Task::spawnArg1`. Once `r` drops below 0 the screen is
/// fully clear, so the task kills itself.
void func_acropolis_plaza_8017DA58(Task* arg0)
{
    OverlayFadeWork* fade;
    OverlayFadeWork* alloc;
    u8               r;
    u8               g;
    TILE*            tile;
    DR_TPAGE*        dr;

    fade = (OverlayFadeWork*)arg0->work;
    switch (arg0->state) {
        case 0:
            alloc      = (OverlayFadeWork*)Mem_Malloc(8, 0);
            arg0->work = (TaskIdMap*)alloc;
            if (alloc == NULL) {
                goto kill;
            }
            fade         = alloc;
            fade->b      = 0xFF;
            fade->g      = 0xFF;
            fade->r      = 0xFF;
            arg0->state += 1;
            /* fallthrough */
        case 1:
            r              = fade->r;
            g              = fade->g;
            tile           = (TILE*)gGpuPrimCursor;
            gGpuPrimCursor = tile + 1;
            setlen(tile, 3);
            setcode(tile, 0x62);
            tile->r0 = r;
            tile->g0 = g;
            tile->b0 = r;
            tile->x0 = -0xA0;
            tile->y0 = -0x78;
            tile->w  = 0x140;
            tile->h  = 0xF0;
            addPrim(gGpuCurrentOt - 16, tile);

            dr             = gGpuPrimCursor;
            gGpuPrimCursor = dr + 1;
            setlen(dr, 1);
            dr->code[0] = 0xE1000240;
            addPrim(gGpuCurrentOt - 16, dr);

            fade->r -= (u16)arg0->spawnArg1;
            fade->g -= (u16)arg0->spawnArg1;
            fade->b -= (u16)arg0->spawnArg1;
            if (fade->r < 0) {
            kill:
                taskKill(arg0);
            }
            break;
    }
}

void func_acropolis_plaza_8017DBFC(Task* arg0)
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
        case 6:
            goto L_case6;
    }
    return;

L_case0:
    SetDispMask(0);
    Mem_AllocAuxWithImages(1);
    goto advance;

L_case1:
    key          = gGameSession->at4;
    key.loc.view = 0x64;
    slot         = Stream_FindSlot(key.raw.data, 0, 0);
    slotParam[0] = slot;
    CdCmd_Enqueue(0x61, 0, slotParam);
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
        SOFT_BARRIER();
        goto advance;
    }
    if (Pad_CheckFlag800() == 0) {
        return;
    }
    CdCmd_ActivatePhase1();
    goto advance;

L_case4:
    if ((CdCmd_IsIdle() & 0xFFFF) == 0) {
        return;
    }
    SetDispMask(0);
    goto advance;

L_case5:
    Stream_ResetRestoreState();
advance:
    task->state = task->state + 1;
    return;

L_case6:
    if ((Stream_RestoreAfterLoad(1, 0) & 0xFFFF) == 0) {
        return;
    }
    taskKill(task);
    Display_ResetHeapWrapper();
}

/// Rebuilds the eight box vertices in `D_acropolis_plaza_80198B90` around the
/// scene work's world position.
void func_acropolis_plaza_8017DD90(Task* arg0)
{
    AcropolisPlazaSceneWork* work = (AcropolisPlazaSceneWork*)arg0->work;
    s32                      x    = work->pos.vx;
    s32                      y    = work->pos.vy;
    s32                      z    = work->pos.vz;
    s16                      near = x - 0xBB8;
    s16                      top;
    s16                      left;
    s16                      right;
    s16                      far;

    D_acropolis_plaza_80198B90[0].vx = near;
    D_acropolis_plaza_80198B90[1].vx = near;
    D_acropolis_plaza_80198B90[2].vx = near;
    D_acropolis_plaza_80198B90[3].vx = near;

    top   = y + 0x3E8;
    left  = z - 0x1000;
    right = z + 0x3000;
    far   = x + 0x7D0;

    D_acropolis_plaza_80198B90[0].vy = y;
    D_acropolis_plaza_80198B90[1].vy = y;
    D_acropolis_plaza_80198B90[2].vy = top;
    D_acropolis_plaza_80198B90[3].vy = top;

    D_acropolis_plaza_80198B90[0].vz = left;
    D_acropolis_plaza_80198B90[1].vz = right;
    D_acropolis_plaza_80198B90[2].vz = left;
    D_acropolis_plaza_80198B90[3].vz = right;

    D_acropolis_plaza_80198B90[4].vx = far;
    D_acropolis_plaza_80198B90[5].vx = far;
    D_acropolis_plaza_80198B90[6].vx = far;
    D_acropolis_plaza_80198B90[7].vx = far;

    D_acropolis_plaza_80198B90[4].vy = y;
    D_acropolis_plaza_80198B90[5].vy = y;
    D_acropolis_plaza_80198B90[6].vy = top;
    D_acropolis_plaza_80198B90[7].vy = top;

    D_acropolis_plaza_80198B90[4].vz = right;
    D_acropolis_plaza_80198B90[5].vz = left;
    D_acropolis_plaza_80198B90[6].vz = right;
    D_acropolis_plaza_80198B90[7].vz = left;
}

/// Applies the plaza camera for view set `arg0`.
///
/// Sets 0..3 all share the opening table and pick within a row directly:
/// while `field_1FA` is clear the row's first shot is used, otherwise
/// `field_1F2` steps forward or back from it depending on `field_1F0`.
/// Sets 4..7 (and any out-of-range value, which leaves the table whatever the
/// caller left in place) instead index the table flat, one shot per step, and
/// clamp the backwards walk at the start of the table.
void func_acropolis_plaza_8017DE24(s32 arg0)
{
    CdCmdQueue* q = &CdCmd_Queue;
    GpViewRec(*tbl)[2];
    GpViewRec* view;
    s16        idx;

    switch ((u16)arg0) {
        case 0:
        case 1:
        case 2:
        case 3:
            tbl = D_acropolis_plaza_801838B8;
            if (q->field_1FA == 0) {
                s32 pair = q->field_1EE - 1;
                view     = tbl[pair];
            } else if (q->field_1F0 == 0) {
                s32 pair = q->field_1EE - 1;
                view     = tbl[pair] + q->field_1F2;
            } else {
                s32 pair = q->field_1EE - 1;
                view     = tbl[pair] - q->field_1F2;
            }
            break;
        case 4:
            tbl = D_acropolis_plaza_8018A938;
            goto common;
        case 5:
            tbl = D_acropolis_plaza_8018CAFC;
            goto common;
        case 6:
            tbl = D_acropolis_plaza_8018F530;
            goto common;
        case 7:
            tbl = D_acropolis_plaza_8018F9B4;
        default:
        common:
            if (q->field_1FA == 0) {
                s32 pair = q->field_1EE - 1;
                view     = tbl[pair];
            } else {
                if (q->field_1F0 == 0) {
                    idx = ((q->field_1EE - 1) * 2) + q->field_1F2 + 1;
                } else {
                    idx = ((q->field_1EE - 1) * 2) - q->field_1F2 - 1;
                    if (idx < 0) {
                        idx = 0;
                    }
                }
                view = *tbl + idx;
            }
            break;
    }
    Gp_ApplyView(view);
}

/// Recomputes the scene's two "the player has walked off this shot" flags from
/// how far the shot's anchor sits ahead of the camera. Past 0xC8 the player has
/// gone forward, before -0x14 they have gone back, and the first view has no
/// shot behind it so the backwards flag never fires there. A `field_958` of 3
/// (the player running) picks the 2 variant of either flag; anything else
/// picks 1.
static __inline__ void plaza_updateEdgeFlags(AcropolisPlazaSceneWork* w)
{
    CdCmdQueue* cq   = &CdCmd_Queue;
    s32         dist = w->distX;

    w->fwd  = 0;
    w->back = 0;
    if (dist >= 0xC9) {
        if ((u16)w->actor->field_958 == 3) {
            w->fwd = 2;
        } else {
            w->fwd = 1;
        }
    } else if (dist < -0x14) {
        if (cq->field_1EE != 1) {
            if ((u16)w->actor->field_958 == 3) {
                w->back = 2;
            } else {
                w->back = 1;
            }
        }
    }
}

/// The plaza's scene task: it plays the room's pre-rendered camera stream and
/// re-seeks it whenever the player walks past the end of the current shot.
///
/// State 0 allocates the work block, seeds `CdCmd_Queue` from the spawn
/// argument and (unless the argument suppresses it) asks for the opening
/// stream; state 1 caches the slot-3 task once the CD is idle and turns the
/// display on. State 2 is the running state: it refreshes the edge flags, and
/// when one fires it enqueues a 0x71 seek to the neighbouring view -- forwards
/// from `baseView`, backwards from `baseView + 1` -- with the target frame
/// packed into the request's last two bytes. States 3..5 wait for that seek to
/// land and return to state 2, and state 6 is the parked state the task sits in
/// while `field_1F6` blocks seeking, leaving on the first flag that matches the
/// current view.
///
/// `loMask` holds 0xFF in a local on purpose: masking with a literal lets GCC
/// fold the `andi` into the byte store, and the original build keeps it.
void func_acropolis_plaza_8017DFE0(Task* task)
{
    u8                       slot[4];
    s32                      frameOfs;
    u32                      seekFrame;
    u32                      openFrame;
    s32                      loMask = 0xFF;
    s32                      side;
    TaskIdMap*               block;
    CdCmdQueue*              q;
    AcropolisPlazaSceneWork* work;
    AcropolisPlazaSceneArg*  arg;
    Task*                    slot3;
    u16                      view;
    u16                      startView;

    q    = &CdCmd_Queue;
    work = (AcropolisPlazaSceneWork*)task->work;

    if (task->state != 0) {
        switch (q->field_1F8) {
            case 0:
            case 1:
            case 2:
            case 3:
                work->pos.vx = D_acropolis_plaza_801907C4[q->field_1EE - 1].vx;
                work->pos.vy = D_acropolis_plaza_801907C4[q->field_1EE - 1].vy;
                work->pos.vz = D_acropolis_plaza_801907C4[q->field_1EE - 1].vz;
                work->distX  = work->pos.vx - work->mtx->t[0];
                break;
        }
    }

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
        case 6:
            goto L_case6;
    }
    goto L_tail;

L_case0:
    block      = (TaskIdMap*)Mem_Malloc(0x34, 0);
    task->work = block;
    if (block == NULL) {
        taskKill(task);
        return;
    }
    Mem_Set(block, 0, 0x34);
    arg            = (AcropolisPlazaSceneArg*)task->spawnArg2;
    work           = (AcropolisPlazaSceneWork*)task->work;
    startView      = arg->view;
    q->field_1F8   = 0;
    q->field_1EE   = startView;
    q->field_1EA   = startView;
    work->prevSide = 0;
    q->field_1F0   = 0;
    q->field_1FA   = 0;
    q->field_1E8   = 0;
    if (((AcropolisPlazaSceneArg*)task->spawnArg2)->noStream == 0) {
        slot[0]   = Stream_FindSlot(&gGameSession->at4.loc.view, q->field_1F8, 0);
        frameOfs  = (q->field_1EA - 1) * 10;
        openFrame = frameOfs & 0xFFFF;
        slot[1]   = openFrame >> 8;
        slot[2]   = openFrame & loMask;
        CdCmd_Enqueue(0x72, 0, slot);
    } else {
        q->field_1F6 = 0;
    }
    ((AcropolisPlazaSceneWork*)task->work)->mtx = Player_Status.coordMtx;
    work->field_2E                              = 1;
    task->state                                 = task->state + 1;
    goto L_tail;

L_case1:
    if ((CdCmd_IsIdle() & 0xFFFF) == 0) {
        goto L_tail;
    }
    slot3       = gameGetPtrSlot(3);
    work->slot3 = slot3;
    work->actor = (GameActor*)slot3->work;
    SetDispMask(1);
    task->state = task->state + 1;
    goto L_tail;

L_case2:
    work->streamLen = Stream_GetSlotField1A(q->field_1F8);
    if (q->field_1F6 != 0) {
        task->state = 6;
        goto L_tail;
    }
    plaza_updateEdgeFlags((AcropolisPlazaSceneWork*)task->work);
    if (work->fwd != 0) {
        work->prevSide = q->field_1F0;
        q->field_1F0   = 0;
    }
    if (work->back != 0) {
        work->prevSide = q->field_1F0;
        q->field_1F0   = 1;
    }
    if (work->fwd == 0 && work->back == 0) {
        goto L_tail;
    }
    q->field_1E8 = 1;
    side         = q->field_1F0;
    if (side == work->prevSide) {
        if (side == 0) {
            if (work->fwd != 1) {
                goto L_enqueue;
            }
            view = work->baseView;
        L_eqShared:
            q->field_1F8 = view;
            if (work->field_2E == 2) {
                frameOfs = q->field_1EA * 40;
                goto L_eqSetFlag;
            }
        } else {
            if (side != 1) {
                goto L_enqueue;
            }
            if (work->back != side) {
                goto L_enqueue;
            }
            view = work->baseView + 1;
            goto L_eqShared;
        }
        frameOfs = q->field_1EA * 10;
    L_eqSetFlag:
        work->field_2E = 1;
    } else {
        if (side == 0) {
            if (work->fwd != 1) {
                goto L_enqueue;
            }
            q->field_1F8 = work->baseView;
            if (work->field_2E == 2) {
                frameOfs = (work->streamLen - q->field_1EA) * 40;
            } else {
                frameOfs = (work->streamLen - q->field_1EA) * 10;
            }
            work->field_2E = 1;
        } else if (side == 1) {
            if (work->back != side) {
                goto L_enqueue;
            }
            q->field_1F8 = work->baseView + 1;
            if (work->field_2E == 2) {
                frameOfs = (work->streamLen - q->field_1EA) * 40;
            } else {
                frameOfs = (work->streamLen - q->field_1EA) * 10;
            }
            work->field_2E = 1;
        }
    }
L_enqueue:
    slot[0]   = Stream_FindSlot(&gGameSession->at4.loc.view, q->field_1F8, 0);
    seekFrame = frameOfs & 0xFFFF;
    slot[1]   = seekFrame >> 8;
    slot[2]   = seekFrame;
    CdCmd_Enqueue(0x71, 0, slot);
    q->field_1FA = 0;
    task->state  = task->state + 1;
    goto L_tail;

L_case3:
    if (q->field_1FA != 0) {
        task->state = task->state + 1;
    }
    goto L_tail;

L_case4:
    plaza_updateEdgeFlags((AcropolisPlazaSceneWork*)task->work);
    if (q->field_1F6 != 0) {
        task->state = 6;
        goto L_tail;
    }
    if ((work->fwd == 0 && q->field_1F0 == 0) || (work->back == 0 && q->field_1F0 == 1) ||
        work->fwd != work->prevFwd || work->back != work->prevBack) {
        q->field_1E8 = 0;
        task->state  = task->state + 1;
    }
    goto L_tail;

L_case5:
    plaza_updateEdgeFlags((AcropolisPlazaSceneWork*)task->work);
    if ((CdCmd_IsIdle() & 0xFFFF) == 0) {
        goto L_tail;
    }
    task->state = 2;
    goto L_case2;

L_case6:
    plaza_updateEdgeFlags((AcropolisPlazaSceneWork*)task->work);
    switch (q->field_1F8) {
        case 0:
        case 2:
            if (work->back == 0) {
                goto L_tail;
            }
            break;
        case 1:
        case 3:
            if (work->fwd == 0) {
                goto L_tail;
            }
            break;
        default:
            goto L_tail;
    }
    q->field_1F6 = 0;
    task->state  = 2;

L_tail:
    if (q->field_1FA != 0) {
        func_acropolis_plaza_8017DD90(task);
        func_acropolis_plaza_8017DE24(q->field_1F8);
    }
    work->prevBack = work->back;
    work->prevFwd  = work->fwd;
}

/// Five-state warp sequence. State 0 allocates the work block, caches the
/// slot-3 task in it and places the player at (0x3804, 0, 0xFC8) with msg
/// 0x3F2; states 1 and 2 wait for slot 3 to go idle (msg 0x3F0), state 1
/// following up with the 0xD55 warp (msg 0x3EE). State 3 waits for the stream
/// to finish, latches `CdCmd_Queue.field_1EE` into the cutscene work block,
/// kills the task it names and runs `func_800E8634`; state 4 kills this task
/// once the session is out of its transition.
void func_acropolis_plaza_8017E7E4(Task* task)
{
    GpXformArg              place;
    GpXformArg              warp;
    CdCmdQueue*             q    = &CdCmd_Queue;
    AcropolisPlazaWarpWork* work = (AcropolisPlazaWarpWork*)task->work;
    AcropolisPlazaWarpWork* newWork;

    switch (task->state) {
        case 0:
            newWork    = Mem_Malloc(8, 0);
            task->work = (TaskIdMap*)newWork;
            if (newWork == NULL) {
                taskKill(task);
                return;
            }
            Mem_Set(newWork, 0, 8);
            ((AcropolisPlazaWarpWork*)task->work)->slot3 = gameGetPtrSlot(3);
            place.pos.vx                                 = 0x3804;
            place.pos.vy                                 = 0;
            place.pos.vz                                 = 0xFC8;
            Gp_DispatchMsg(((AcropolisPlazaWarpWork*)task->work)->slot3, 0x3F2, (s32)&place, 0);
            task->state = task->state + 1;
            return;
        case 1:
            if (Gp_DispatchMsg(work->slot3, 0x3F0, 0, 0) != 0) {
                return;
            }
            warp.rot.vy = 0xD55;
            Gp_DispatchMsg(((AcropolisPlazaWarpWork*)task->work)->slot3, 0x3EE, (s32)&warp, 0);
            task->state = task->state + 1;
            return;
        case 2:
            if (Gp_DispatchMsg(work->slot3, 0x3F0, 0, 0) != 0) {
                return;
            }
            task->state = task->state + 1;
            return;
        case 3:
            if (CdCmd_IsIdle() == 0) {
                return;
            }
            ((AcropolisPlazaCutWork*)task->spawnArg2)->field_1A = q->field_1EE;
            taskKill(((AcropolisPlazaCutWork*)task->spawnArg2)->task);
            func_800E8634((s32)D_acropolis_plaza_80182734, 1, (s32)D_acropolis_plaza_80182A34);
            task->state = task->state + 1;
            return;
        case 4:
            if (gGameSession->eventState == 0) {
                Task_RequestKill(task, 0);
            }
            return;
    }
}

/// Seven-state opening sequence for the plaza's streamed scene, and the only
/// caller of every payload `AcropolisPlazaTailMsg` describes. State 0 allocates
/// the work block, caches the slot-3 task in it and places the player at
/// (0xF6E, 0, 0x2328) with msg 0x3F2; states 1 and 2 wait for slot 3 to go idle
/// (msg 0x3F0), following up with the 0xD55 warp (msg 0x3EE) and then the
/// `D_801797FC` script (msg 0x3F4). State 3 waits for the CD queue, latches
/// `CdCmd_Queue.field_1EE` into the cutscene work block, kills the task it
/// names and starts the scene's stream (`CdCmd_Enqueue(0x72, ...)`); state 4
/// waits for the stream to report in and runs `D_acropolis_plaza_80182B24`.
/// State 5 waits out 0x60 frames, republishes the player's weapon to slot 3
/// (msg 0x3E8) and warps the player onto the slot-3 model's own coordinate
/// frame with a 0x3E9 placement; state 6 releases slot 3 (msg 0x3F1) and asks
/// to be killed. States 5 and 6 also step the room's per-frame work
/// (`func_acropolis_plaza_8017DE24(4)`), which the earlier states skip.
void func_acropolis_plaza_8017E9A8(Task* task)
{
    GpXformArg              place;
    GpXformArg              warp;
    GpAnimArg               script;
    AcropolisPlazaTailMsg   buf;
    GpAnimArg*              rec;
    CdCmdQueue*             q    = &CdCmd_Queue;
    AcropolisPlazaWarpWork* work = (AcropolisPlazaWarpWork*)task->work;
    AcropolisPlazaWarpWork* newWork;
    GsCOORDINATE2*          coord;
    s32                     weaponId;
    s32                     id;

    switch (task->state) {
        case 0:
            newWork    = Mem_Malloc(8, 0);
            task->work = (TaskIdMap*)newWork;
            if (newWork == NULL) {
                taskKill(task);
                return;
            }
            Mem_Set(newWork, 0, 8);
            ((AcropolisPlazaWarpWork*)task->work)->slot3 = gameGetPtrSlot(3);
            place.pos.vx                                 = 0xF6E;
            place.pos.vy                                 = 0;
            place.pos.vz                                 = 0x2328;
            Gp_DispatchMsg(((AcropolisPlazaWarpWork*)task->work)->slot3, 0x3F2, (s32)&place, 0);
            task->state = task->state + 1;
            return;
        case 1:
            if (Gp_DispatchMsg(work->slot3, 0x3F0, 0, 0) != 0) {
                return;
            }
            warp.rot.vy = 0xD55;
            Gp_DispatchMsg(((AcropolisPlazaWarpWork*)task->work)->slot3, 0x3EE, (s32)&warp, 0);
            task->state = task->state + 1;
            return;
        case 2:
            if (Gp_DispatchMsg(work->slot3, 0x3F0, 0, 0) != 0) {
                return;
            }
            script.animBlock.ptr = &D_801797FC;
            script.field_4       = 0xB;
            script.field_8       = 0;
            script.field_C       = 0;
            script.field_10      = 1;
            Gp_DispatchMsg(work->slot3, 0x3F4, (s32)&script, 0);
            task->state = task->state + 1;
            return;
        case 3:
            if (CdCmd_IsIdle() == 0) {
                return;
            }
            ((AcropolisPlazaCutWork*)task->spawnArg2)->field_1A = q->field_1EE;
            taskKill(((AcropolisPlazaCutWork*)task->spawnArg2)->task);
            q->field_1EE = 1;
            q->field_1EA = 1;
            q->field_1F8 = 2;
            buf.slot[0]  = Stream_FindSlot(&gGameSession->at4.loc.view, 2, 0);
            buf.slot[1]  = 0;
            buf.slot[2]  = 0;
            CdCmd_Enqueue(0x72, 0, buf.slot);
            q->field_1E8 = 1;
            task->state  = task->state + 1;
            return;
        case 4:
            if (q->field_1FA == 0) {
                return;
            }
            func_800E8614((s32)D_acropolis_plaza_80182B24, 1);
            task->state = task->state + 1;
            return;
        case 5:
            if (q->field_1EA >= 0x60) {
                rec                            = &buf.weapon.rec;
                weaponId                       = D_80073BA9;
                id                             = (D_8007218A == 1) ? weaponId + 1 : weaponId + 0x22;
                buf.weapon.rec.animBlock.index = id;
                rec->field_4                   = 1;
                buf.weapon.rec.field_8         = 0;
                rec->field_C                   = 0xA;
                buf.weapon.rec.field_10        = 0;
                Gp_DispatchMsg(gameGetPtrSlot(3), 0x3E8, (s32)&buf.weapon.rec, 0);

                coord            = ((TmdObject*)((AcropolisPlazaWarpWork*)task->work)->slot3->extra)->coords;
                buf.place.pos.vx = coord->coord.t[0];
                buf.place.pos.vy = coord->coord.t[1];
                buf.place.pos.vz = coord->coord.t[2];
                buf.place.rot.vz = 0;
                buf.place.rot.vx = 0;
                buf.place.rot.vy = 0xEAA;
                Gp_DispatchMsg(((AcropolisPlazaWarpWork*)task->work)->slot3, 0x3E9, (s32)&buf.place, 0);
                task->state = task->state + 1;
            }
            break;
        case 6:
            if (CdCmd_IsIdle() != 0) {
                Gp_DispatchMsg(work->slot3, 0x3F1, 0, 0);
                Task_RequestKill(task, 0);
            }
            break;
        default:
            return;
    }
    func_acropolis_plaza_8017DE24(4);
}

/// Sixteen-state opening sequence for the plaza's long streamed scene, and the
/// counterpart to `func_acropolis_plaza_8017E9A8` for the rest of it. States 0
/// to 2 allocate the work block, cache the slot-3 task in it, place the player
/// at (0x3DE, 0, 0x33FE) with msg 0x3F2 and warp them with a 0x1000 heading
/// (msg 0x3EE), waiting on msg 0x3F0 in between. State 3 kills the cutscene
/// block's task and starts stream slot 4; state 4 runs
/// `D_acropolis_plaza_80182C90` / `..._80182F18` once the CD queue reports in.
/// State 5 waits out the session transition and starts stream slot 5, unless
/// `GameSession::evtSkipped` says to skip the scene, in which case it blanks the
/// display and jumps straight to state 8.
///
/// States 6 and 8 both look the room's own work object up by location: they
/// build a `GpAreaKey` from `gGameSession`, walk the nested area records for
/// the 0x6C entry and pack that index into the id `Gp_FindWorkById` matches.
/// State 6 releases slot 3 (msg 0x3F1), re-places the player at
/// (0x3DE, 0, 0x439E) and hands the room a 0x7D3 record; state 8 sends it 0x7D7
/// and rebuilds the graphics state (`Gpu_ResetGraphAndOt`, the aux heap from
/// `GameSession::at4.loc.stage` / `at4.loc.area`, `Tmd_AllocMissingBuffers`). State 7
/// waits 0x3D frames, playing 0x51050003 at frame 0x1E and spawning table entry
/// 7 at the end.
///
/// States 9 to 12 restart stream slot 3, run `D_acropolis_plaza_801830DC`,
/// spawn table entry 8 after 0xB frames and re-enable the display. State 13 is
/// the exit: the start button (`Pad_CheckFlag800`) skips to state 15, otherwise
/// it requests the map's own MIDI and starts the closing stream, state 14 runs
/// `D_acropolis_plaza_801834B4`, and state 15 releases slot 3 and kills the
/// task. Every state from 7 on also steps the room's per-frame work.
void func_acropolis_plaza_8017ECF8(Task* task)
{
    GpXformArg                 place;
    GpXformArg                 warp;
    u8                         slot[4];
    GpXformArg                 placeBack;
    GpAnimArg                  roomRec;
    AcropolisPlazaOpeningBuf   buf;
    CdCmdQueue*                q    = &CdCmd_Queue;
    AcropolisPlazaOpeningWork* work = (AcropolisPlazaOpeningWork*)task->work;
    AcropolisPlazaOpeningWork* newWork;
    GpAreaKey*                 sessionKey;
    GpAreaPlace*               entry;
    s32                        idx;

    switch (task->state) {
        case 0:
            newWork    = Mem_Malloc(8, 0);
            task->work = (TaskIdMap*)newWork;
            if (newWork == NULL) {
                taskKill(task);
                return;
            }
            Mem_Set(newWork, 0, 8);
            ((AcropolisPlazaOpeningWork*)task->work)->slot3 = gameGetPtrSlot(3);
            place.pos.vx                                    = 0x3DE;
            place.pos.vy                                    = 0;
            place.pos.vz                                    = 0x33FE;
            Gp_DispatchMsg(((AcropolisPlazaOpeningWork*)task->work)->slot3, 0x3F2, (s32)&place, 0);
            task->state = task->state + 1;
            return;
        case 1:
            if (Gp_DispatchMsg(work->slot3, 0x3F0, 0, 0) != 0) {
                return;
            }
            warp.rot.vy = 0x1000;
            Gp_DispatchMsg(((AcropolisPlazaOpeningWork*)task->work)->slot3, 0x3EE, (s32)&warp, 0);
            task->state = task->state + 1;
            return;
        case 2:
            if (Gp_DispatchMsg(work->slot3, 0x3F0, 0, 0) != 0) {
                return;
            }
            task->state = task->state + 1;
            return;
        case 3:
            if (CdCmd_IsIdle() == 0) {
                return;
            }
            taskKill(((AcropolisPlazaCutWork*)task->spawnArg2)->task);
            q->field_1EE = 1;
            q->field_1EA = 1;
            q->field_1F8 = 4;
            slot[0]      = Stream_FindSlot(&gGameSession->at4.loc.view, 4, 0);
            slot[1]      = 0;
            slot[2]      = 0;
            CdCmd_Enqueue(0x72, 0, slot);
            q->field_1E8 = 1;
            task->state  = task->state + 1;
            return;
        case 4:
            if (CdCmd_IsIdle() != 0) {
                func_800E8634((s32)D_acropolis_plaza_80182C90, 1, (s32)D_acropolis_plaza_80182F18);
                task->state = task->state + 1;
                return;
            }
            func_acropolis_plaza_8017DE24(6);
            return;
        case 5:
            if (gGameSession->eventState != 0) {
                return;
            }
            if (gGameSession->evtSkipped != 0) {
                SetDispMask(0);
                task->state = 8;
                return;
            }
            q->field_1EE = 1;
            q->field_1EA = 1;
            q->field_1F8 = 5;
            slot[0]      = Stream_FindSlot(&gGameSession->at4.loc.view, 5, 0);
            slot[1]      = 0;
            slot[2]      = 0;
            CdCmd_Enqueue(0x72, 0, slot);
            q->field_1E8 = 1;
            task->state  = task->state + 1;
            return;
        case 6:
            if (q->field_1FA == 0) {
                return;
            }
            Gp_DispatchMsg(work->slot3, 0x3F1, 1, 0);
            placeBack.pos.vx = 0x3DE;
            placeBack.pos.vy = 0;
            placeBack.pos.vz = 0x439E;
            Gp_DispatchMsg(
                ((AcropolisPlazaOpeningWork*)task->work)->slot3, 0x3F2, (s32)&placeBack, 0);
            roomRec.animBlock.index = 1;
            roomRec.field_4         = 8;
            roomRec.field_8         = 0;
            roomRec.field_C         = 0xA;
            roomRec.field_10        = 0;
            sessionKey              = &gGameSession->at4.loc;
            buf.key.stage           = sessionKey->stage;
            buf.key.area            = sessionKey->area;
            buf.key.room            = gGameSession->sprtVariant;
            buf.key.view            = gGameSession->at4.loc.view;
            buf.key.place           = sessionKey->place;
            entry                   = (GpAreaPlace*)Gp_GetNestedAreaRec(&buf.key)->field_0;
            idx                     = 0;
            /* `for (;;)` with a `goto` out: a `break` here makes GCC copy the
               first exit test into the loop preheader and the walk stops
               matching. */
            if (entry->entryId != 0xFF) {
                for (;;) {
                    if (entry->entryId == 0x6C) {
                        goto found6;
                    }
                    entry++;
                    idx++;
                    if (entry->entryId == 0xFF) {
                        goto found6;
                    }
                }
            }
        found6:
            Gp_DispatchMsg(
                (Task*)Gp_FindWorkById((idx << 12) | (sessionKey->stage << 8) |
                                       sessionKey->area)
                    ->field_0,
                0x7D3, (s32)&roomRec, 0);
            task->state = task->state + 1;
            work->timer = 0;
            return;
        case 7:
            if (work->timer == 0x1E) {
                SndEvt_EnqueueType6(0x51050003, 0, 0);
            }
            work->timer = work->timer + 1;
            if (work->timer >= 0x3D) {
                Task_SpawnFromTable(D_acropolis_plaza_80183824, 7, 9, 0);
                task->state = task->state + 1;
            }
            func_acropolis_plaza_8017DE24(7);
            return;
        case 8:
            if (CdCmd_IsIdle() != 0) {
                sessionKey    = &gGameSession->at4.loc;
                buf.key.stage = sessionKey->stage;
                buf.key.area  = sessionKey->area;
                buf.key.room  = gGameSession->sprtVariant;
                buf.key.view  = gGameSession->at4.loc.view;
                buf.key.place = sessionKey->place;
                entry         = (GpAreaPlace*)Gp_GetNestedAreaRec(&buf.key)->field_0;
                idx           = 0;
                if (entry->entryId != 0xFF) {
                    for (;;) {
                        if (entry->entryId == 0x6C) {
                            goto found8;
                        }
                        entry++;
                        idx++;
                        if (entry->entryId == 0xFF) {
                            goto found8;
                        }
                    }
                }
            found8:
                Gp_DispatchMsg(
                    (Task*)Gp_FindWorkById((idx << 12) | (sessionKey->stage << 8) |
                                           sessionKey->area)
                        ->field_0,
                    0x7D7, 1, 0);
                Gp_DispatchMsg(work->slot3, 0x3F3, 2, 0);
                Gpu_ResetGraphAndOt();
                Mem_ConfigureAuxHeap(gGameSession->at4.loc.stage, gGameSession->at4.loc.area);
                Mem_SetActiveAuxHeap(1);
                Tmd_AllocMissingBuffers();
                SndEvt_EnqueueTypeB(0x51050005, 0x26);
                task->state = task->state + 1;
                return;
            }
            func_acropolis_plaza_8017DE24(7);
            return;
        case 9:
            q->field_1EE = 1;
            q->field_1EA = 1;
            q->field_1F8 = 3;
            slot[0]      = Stream_FindSlot(&gGameSession->at4.loc.view, 3, 0);
            slot[1]      = 0;
            slot[2]      = 0;
            CdCmd_Enqueue(0x72, 0, slot);
            q->field_1E8 = 0;
            task->state  = task->state + 1;
            /* fallthrough */
        case 10:
            if (CdCmd_IsIdle() == 0) {
                return;
            }
            func_800E8614((s32)D_acropolis_plaza_801830DC, 1);
            work->timer = 0;
            task->state = task->state + 1;
            return;
        case 11:
            work->timer = work->timer + 1;
            if (work->timer >= 0xB) {
                SndEvt_EnqueueType6(0x5105000B, 0, 0);
                Task_SpawnFromTable(D_acropolis_plaza_80183824, 8, 8, 0);
                work->timer = 0;
                task->state = task->state + 1;
            }
            return;
        case 12:
            work->timer = work->timer + 1;
            if (work->timer >= 2) {
                SetDispMask(1);
                task->state = task->state + 1;
            }
            return;
        case 13:
            if (Pad_CheckFlag800() != 0) {
                Stage_RequestMidiFromMap(0xA);
                CdCmd_ActivatePhase1();
                task->state = 0xF;
            } else if (gGameSession->eventState == 0) {
                Stage_RequestMidiFromMap(0x1E0);
                q->field_1EE = 1;
                q->field_1EA = 1;
                q->field_1F8 = 3;
                buf.slot[0]  = Stream_FindSlot(&gGameSession->at4.loc.view, 3, 0);
                buf.slot[1]  = 0;
                buf.slot[2]  = 0;
                CdCmd_Enqueue(0x71, 0, buf.slot);
                q->field_1E8 = 1;
                task->state  = task->state + 1;
            }
            func_acropolis_plaza_8017DE24(5);
            return;
        case 14:
            if (q->field_1FA != 0) {
                func_800E8614((s32)D_acropolis_plaza_801834B4, 1);
                task->state = task->state + 1;
            }
            func_acropolis_plaza_8017DE24(5);
            return;
        case 15:
            if (CdCmd_IsIdle() != 0) {
                SndEvt_EnqueueType7(0x51050002, 0xB4);
                Gp_DispatchMsg(work->slot3, 0x3F1, 1, 0);
                Task_RequestKill(task, 0);
            }
            func_acropolis_plaza_8017DE24(5);
            return;
        default:
            return;
    }
}

/// Three-state cutscene tail: state 0 republishes the player's weapon to slot
/// 3 (msg 0x3E8), state 1 waits for the streamed scene to finish -- latching
/// `CdCmd_Queue.field_1EE` into the work block, killing the block's task and
/// running the block `spawnArg1` names -- and state 2 kills this task once the
/// session is out of its transition.
void func_acropolis_plaza_8017F48C(Task* task)
{
    GpAnimArg   rec;
    CdCmdQueue* q = &CdCmd_Queue;
    s32         state;
    s32         weaponId;
    s32         id;

    state = task->state;
    switch (state) {
        case 0:
            weaponId            = D_80073BA9;
            id                  = (D_8007218A == 1) ? weaponId + 1 : weaponId + 0x22;
            rec.animBlock.index = id;
            rec.field_4         = 1;
            rec.field_8         = 0;
            rec.field_C         = 0xA;
            rec.field_10        = 0;
            Gp_DispatchMsg(gameGetPtrSlot(3), 0x3E8, (s32)&rec, 0);
            task->state = task->state + 1;
            break;
        case 1:
            if (CdCmd_IsIdle() != 0) {
                ((AcropolisPlazaCutWork*)task->spawnArg2)->field_1A = q->field_1EE;
                taskKill(((AcropolisPlazaCutWork*)task->spawnArg2)->task);
                switch (task->spawnArg1) {
                    case 0:
                        func_800E8614((s32)D_acropolis_plaza_80183554, 1);
                        break;
                    case 1:
                        func_800E8614((s32)D_acropolis_plaza_8018365C, 1);
                        break;
                    case 2:
                        func_800E8614((s32)D_acropolis_plaza_80183764, 1);
                        break;
                }
                task->state = task->state + 1;
            }
            break;
        case 2:
            if (gGameSession->eventState == 0) {
                Task_RequestKill(task, 0);
            }
            break;
    }
}

/// Three-state cutscene tail: state 0 republishes the player's weapon to slot
/// 3 (msg 0x3E8), state 1 waits for the streamed scene to finish and hands
/// control back -- latching `CdCmd_Queue.field_1EE` into the work block, killing
/// the block's task and running its capture command -- and state 2 releases
/// slot 3 (msg 0x3F1) and kills itself.
void func_acropolis_plaza_8017F620(Task* task)
{
    GpAnimArg   rec;
    CdCmdQueue* q = &CdCmd_Queue;
    s32         weaponId;
    s32         id;

    switch (task->state) {
        case 0:
            weaponId            = D_80073BA9;
            id                  = (D_8007218A == 1) ? weaponId + 1 : weaponId + 0x22;
            rec.animBlock.index = id;
            rec.field_4         = 1;
            rec.field_8         = 0;
            rec.field_C         = 0xA;
            rec.field_10        = 0;
            Gp_DispatchMsg(gameGetPtrSlot(3), 0x3E8, (s32)&rec, 0);
            task->state = task->state + 1;
            break;
        case 1:
            if (CdCmd_IsIdle() != 0) {
                ((AcropolisPlazaCutWork*)task->spawnArg2)->field_1A = q->field_1EE;
                taskKill(((AcropolisPlazaCutWork*)task->spawnArg2)->task);
                Gp_RunCapCmd1(((AcropolisPlazaCutWork*)task->spawnArg2)->capCmd);
                task->state = task->state + 1;
            }
            break;
        case 2:
            Gp_DispatchMsg(gameGetPtrSlot(3), 0x3F1, 1, 0);
            Task_RequestKill(task, 0);
            break;
    }
}

/// Ambience voice driver: starts the voice named by `sndId` the first time
/// `state` is clear, then tracks `CdCmd_Queue.field_1EE` between `fadeIn` and
/// `fadeOut` to ramp its volume.
void func_acropolis_plaza_8017F770(u16 fadeIn, u16 fadeOut, u16 hold, u16* state, s32 sndId, u16 mode)
{
    CdCmdQueue* q     = &CdCmd_Queue;
    u16*        frame = &CdCmd_Queue.field_1EE;
    u32         pos;
    u8          vol;

    pos = q->field_1EE;
    if (pos >= fadeIn && pos <= fadeOut) {
        COMPILER_BARRIER();
        if (*state == 0) {
            SndEvt_EnqueueType6(sndId, 0, 0x7F);
            SndEvt_EnqueueTypeB(sndId, 0);
            *state = 1;
            return;
        }
        if (mode == 0) {
            if (pos < hold) {
                vol = ((*frame - fadeIn) * 0x7F) / (hold - fadeIn);
            } else {
                vol = ((fadeOut - *frame) * 0x7F) / (fadeOut - hold);
            }
        } else if (mode == 1) {
            if (pos < hold) {
                vol = ((*frame - fadeIn) * 0x17D) / ((hold - fadeIn) * 4);
            } else {
                vol = ((fadeOut - *frame) * 0x17D) / ((fadeOut - hold) * 4);
            }
        } else if (pos < 0x54U) {
            vol = (((0x54 - *frame) * 0x7F) / 332) + 0x5F;
        } else {
            vol = ((0x82 - *frame) * 0x17D) / 184;
        }
        SndEvt_EnqueueTypeB(sndId, vol);
        return;
    }
    if (*state != 0) {
        SndEvt_EnqueueType7(sndId, 0);
        *state = 0;
    }
}

/// Ambience driver for the plaza's streamed scene, stepped by
/// `CdCmd_Queue.field_1F8`. While the stream is at 0/1 it keeps the four
/// looping voices alive (`func_acropolis_plaza_8017F770` starts a voice the
/// first time its slot flag is clear and ramps it afterwards); at 2 it fades
/// the crowd loop out against the stream frame counter, holding full volume
/// (0x7F) over frames 0x1F..0x54 and sliding down over 127/120ths of the
/// distance to the nearer end outside that window.
void func_acropolis_plaza_8017F9EC(Task* task)
{
    CdCmdQueue*         q     = &CdCmd_Queue;
    volatile u16*       frame = &CdCmd_Queue.field_1EE;
    AcropolisPlazaWork* work  = (AcropolisPlazaWork*)task->work;
    s32                 pos;
    s32                 vol;

    switch (CdCmd_Queue.field_1F8) {
        case 0:
        case 1:
            func_acropolis_plaza_8017F770(1, 0x320, 1, &work->sfxState1E, 0x51050005, 0);
            func_acropolis_plaza_8017F770(1, 0x82, 1, &work->sfxState22, 0x51050002, 2);
            func_acropolis_plaza_8017F770(0xF, 0x8E, 0x50, &work->sfxState24, 0x51050004, 0);
            func_acropolis_plaza_8017F770(0xC8, 0x172, 0x140, &work->sfxState20, 0x51050001, 1);
            break;
        case 2:
            pos = q->field_1EE;
            if ((u32)(pos - 0x1F) < 0x36U) {
                vol = 0x7F;
            } else if (pos < 0x1EU) {
                vol = ((*frame * 0x7F) / 120) + 0x5F;
            } else {
                vol = (((0x73 - *frame) * 0x7F) / 120) + 0x5F;
            }
            SndEvt_EnqueueTypeB(0x51050001, vol & 0xFF);
            break;
    }
}

/// Steps the plaza's streamed scene, returning zero while it is still running.
///
/// Seven steps driven by the pending `GpObj4C` event `Gp_TakePendingObj4C`
/// reports. `ready` is that event's "take it" flag, qualified by `D_80073BAC`
/// so an event that arrives with the id's sign bit clear is only acted on when
/// that global is set. Steps 0 and 2 latch the event into the work block and
/// pick a table entry from its kind byte; steps 1, 3 and 4..6 wait on the task
/// the previous step spawned (`Task_PollKill`) and respawn the entry-1 stream
/// watcher over `field_10`. Step 3 is the only exit: it unlinks the scene's
/// `GpObj4A` and returns 1 when the latched kind is 2.
u16 func_acropolis_plaza_8017FB50(Task* task)
{
    CdCmdQueue*         q    = &CdCmd_Queue;
    AcropolisPlazaWork* work = (AcropolisPlazaWork*)task->work;
    u16                 evtId;
    u8                  evtKind;
    u8                  evtSub;
    s32                 killed0;
    s32                 killed1;
    s32                 killed2;
    s16                 ready;
    u16                 step;
    s32                 kind;
    u32                 latchedKind;
    u16                 latchedKind16;

    ready = Gp_TakePendingObj4C(&evtId, &evtKind, &evtSub);
    if (!((s16)evtId & 0x8000) && (ready != 0)) {
        ready = D_80073BAC != 0;
    }

    switch (work->step) {
        case 0:
            if (ready != 0) {
                work->evtId   = evtId;
                work->evtKind = evtKind;
                work->evtSub  = evtSub;
                if ((s8)evtKind == 1) {
                    work->field_C =
                        Task_SpawnFromTable(D_acropolis_plaza_80183824, 4, 0, (s32)work);
                    work->step = work->step + 1;
                    goto running;
                } else if ((s8)evtKind >= 6) {
                    work->field_C =
                        Task_SpawnFromTable(D_acropolis_plaza_80183824, 9, 0, (s32)work);
                    work->step = 5;
                }
            }
            return 0;
        case 1:
            if (Task_PollKill(work->field_C, &killed0) != 0) {
                work->field_12 = 1;
                work->field_10 = work->streamFrame;
                work->field_8 =
                    Task_SpawnFromTable(D_acropolis_plaza_80183824, 1, 0, (s32)&work->field_10);
                Gp_UnlinkObj4A(0, &D_acropolis_plaza_801991F0);
                work->step = work->step + 1;
            }
            return 0;
        case 2:
            if (ready != 0) {
                work->evtId       = evtId;
                work->evtKind     = evtKind;
                work->evtSub      = evtSub;
                work->streamFrame = q->field_1EE;
                kind              = (s8)evtKind;
                if (kind == 0) {
                    work->field_C =
                        Task_SpawnFromTable(D_acropolis_plaza_80183824, 2, 0, (s32)work);
                    work->step = work->step + 1;
                    goto running;
                } else if (kind == 2) {
                    work->field_C =
                        Task_SpawnFromTable(D_acropolis_plaza_80183824, 3, 0, (s32)work);
                    work->step = work->step + 1;
                    goto running;
                } else if (kind == 3) {
                    if (work->variant == 0) {
                        work->field_C =
                            Task_SpawnFromTable(D_acropolis_plaza_80183824, 6, 0, (s32)work);
                        work->step = 4;
                    } else if (work->variant == 1) {
                        work->field_C =
                            Task_SpawnFromTable(D_acropolis_plaza_80183824, 6, 1, (s32)work);
                        work->step = 4;
                    } else {
                        work->field_C =
                            Task_SpawnFromTable(D_acropolis_plaza_80183824, 6, 2, (s32)work);
                        work->step = 4;
                    }
                } else if (kind >= 6) {
                    work->field_C =
                        Task_SpawnFromTable(D_acropolis_plaza_80183824, 9, 0, (s32)work);
                    work->step = 6;
                }
            }
            return 0;
        case 3:
            if (Task_PollKill(work->field_C, &killed1) != 0) {
                /* The kind byte is tested as an unsigned short, so it is
                   sign-extended and narrowed again at each comparison; routing
                   both tests through one variable folds the pair away. */
                latchedKind   = work->evtKind;
                latchedKind16 = (s8)latchedKind;
                if (latchedKind16 == 0) {
                    Gp_UnlinkObj4A(0, &D_acropolis_plaza_801991A4);
                } else if ((u16)(s8)latchedKind == 2) {
                    Gp_UnlinkObj4A(0, &D_acropolis_plaza_8019923C);
                }
                work->step = work->step - 1;
                if ((s8)work->evtKind == 2) {
                    return 1;
                }
                work->field_12 = 1;
                work->field_10 = work->streamFrame;
                work->field_8 =
                    Task_SpawnFromTable(D_acropolis_plaza_80183824, 1, 0, (s32)&work->field_10);
            }
            return 0;
        case 4:
        case 5:
        case 6:
            if (Task_PollKill(work->field_C, &killed2) != 0) {
                work->field_12 = 1;
                work->field_10 = work->streamFrame;
                work->field_8 =
                    Task_SpawnFromTable(D_acropolis_plaza_80183824, 1, 0, (s32)&work->field_10);
                step = work->step;
                if (step == 5) {
                    work->step = 0;
                } else {
                    if (step != 6) {
                        if (work->variant < 2) {
                            work->variant = work->variant + 1;
                        }
                    }
                    work->step = 2;
                }
            }
            return 0;
    }
running:
    return 0;
}

/// Draws the cinematic letterbox: two black 0x140x0x18 `TILE` bars spanning the
/// full screen width at the top (y -0x78) and bottom (y 0x60), linked into
/// `gGpuCurrentOt[3]`.
void func_acropolis_plaza_8017FF18(void)
{
    TILE* tile;

    tile           = (TILE*)gGpuPrimCursor;
    gGpuPrimCursor = tile + 1;
    SetTile(tile);
    tile->b0 = 0;
    tile->g0 = 0;
    tile->r0 = 0;
    tile->x0 = -0xA0;
    tile->y0 = -0x78;
    tile->w  = 0x140;
    tile->h  = 0x18;
    addPrim(gGpuCurrentOt + 3, tile);

    tile           = (TILE*)gGpuPrimCursor;
    gGpuPrimCursor = tile + 1;
    SetTile(tile);
    tile->b0 = 0;
    tile->g0 = 0;
    tile->r0 = 0;
    tile->x0 = -0xA0;
    tile->y0 = 0x60;
    tile->w  = 0x140;
    tile->h  = 0x18;
    addPrim(gGpuCurrentOt + 3, tile);
}

/// Six-state opening sequence for the plaza. State 0 fades in the room
/// (`func_800E9BDC`), applies the plaza view, allocates the sequence work block
/// and spawns entries 5 and 0xB of the room's table around
/// `Gp_KillPlayerEffs`; states 1 and 2 idle. State 3 pins the camera override
/// to (0x370, 0x370, 0x370), tells slot 6 to start (msg 0xFA4), spawns the
/// stream watcher (entry 1) and the entry-8 actor, and arms
/// `CdCmd_Queue.field_244`. State 4 runs the ambience driver until
/// `func_acropolis_plaza_8017FB50` reports the scene is over; state 5 records
/// the next stage in the save block, disarms `field_244` and hands off to the
/// stage-load task.
void func_acropolis_plaza_80180054(Task* task)
{
    CdCmdQueue*         q    = &CdCmd_Queue;
    AcropolisPlazaWork* work = (AcropolisPlazaWork*)task->work;
    AcropolisPlazaWork* newWork;
    SVECTOR             vec;

    switch (task->state) {
        case 0:
            func_800E9BDC(3, 0x9DF);
            Gp_ApplyView(D_acropolis_plaza_801838B8[0]);
            newWork    = (AcropolisPlazaWork*)Mem_Malloc(0x28, 0);
            task->work = (TaskIdMap*)newWork;
            if (newWork == NULL) {
                taskKill(task);
                return;
            }
            Mem_Set(newWork, 0, 0x28);
            ((AcropolisPlazaWork*)task->work)->slot3 = gameGetPtrSlot(3);
            ((AcropolisPlazaWork*)task->work)->field_C =
                Task_SpawnFromTable(D_acropolis_plaza_80183824, 5, 0, 0);
            Gp_KillPlayerEffs();
            Task_SpawnFromTable(D_acropolis_plaza_80183824, 0xB, 0, 0);
            task->state = task->state + 1;
            return;
        case 1:
        case 2:
            task->state = task->state + 1;
            return;
        case 3:
            vec.vx = 0x370;
            vec.vy = 0x370;
            vec.vz = 0x370;
            Gp_SetOverrideVec(&vec);
            Gp_DispatchMsg(gameGetPtrSlot(6), 0xFA4, 0, 0);
            work->field_12 = 0;
            work->field_10 = 0;
            work->field_8  = Task_SpawnFromTable(D_acropolis_plaza_80183824, 1, 0, (s32)&work->field_10);
            Stage_RequestFromAreaTable(0);
            Task_SpawnFromTable(D_acropolis_plaza_80183824, 8, 6, 0);
            q->field_244 = 1;
            task->state  = task->state + 1;
            return;
        case 4:
            func_acropolis_plaza_8017F9EC(task);
            if (func_acropolis_plaza_8017FB50(task) == 0) {
                return;
            }
            task->state = task->state + 1;
            return;
        case 5:
            Mc_SaveData.at4.loc.stage = 1;
            Mc_SaveData.at4.loc.warp  = 1;
            Mc_SaveData.at4.loc.area  = 0x11;
            Mc_SaveData.at4.loc.room  = 1;
            D_80071076                = 1;
            Gp_EnqueueHeldWeaponCd();
            SndEvt_EnqueueType7(0x80000000, 0);
            Task_Spawn(0, 0x11, 0, 0);
            q->field_244 = 0;
            taskKill(task);
            return;
    }
}

void func_acropolis_plaza_80180270(Task* arg0)
{
    Display_SpawnWithOt(D_acropolis_plaza_80183824, 0xA, 0, 0);
    D_8007106B = 1;
    Gp_SpawnViewTasks();
    taskKill(arg0);
}

void func_acropolis_plaza_801802C0(Task* task)
{
    // Work spans both passes; s4 is reused for transient draw state.
    GpCoord64*                       entry;
    GpPointLight*                    light;
    GsCOORDINATE2*                   coord;
    GsCOORDINATE2*                   lightCoord;
    register AcropolisPlazaBeamWork* work asm("s5");
    u8 *                             head, *raw;
    u16                              vz;
    AcropolisPlazaBeamScratch*       blk;
    SVECTOR*                         point;
    POLY_G3*                         tri;
    POLY_G4*                         prim;
    s32                              i;
    u32                              brightness;
    u16                              red, green, blue;
    s32                              slot, pulse;
    u32                              pulse2;
    s16                              spread, depthVal;
    u16                              yaw;

    slot       = task->spawnArg1;
    entry      = &Gp_RoomCoords[slot & 7];
    light      = &entry->data.light;
    coord      = ((TmdObject*)task->extra)->coords;
    work       = (AcropolisPlazaBeamWork*)task->spawnArg2;
    lightCoord = &light->head.u.coord;
    if (task->state == 0) {
        work->yaw   = (slot & 1) << 11;
        task->state = task->state + 1;
    }
    Gfx_RotMatrixY(&coord->coord, work->yaw, 1);
    coord->flg = 0;
    Gp_UpdateCoord(coord);
    head = *(void**)G_SCRATCH_HEAD;
    raw  = head - 0x60;
    SOFT_TOUCH_REG(raw);
    blk                     = (AcropolisPlazaBeamScratch*)raw;
    blk->vec[0].vx          = *(u16*)&coord->workm.t[0];
    blk->vec[0].vy          = *(u16*)&coord->workm.t[1];
    vz                      = *(u16*)&coord->workm.t[2];
    *(void**)G_SCRATCH_HEAD = blk;
    blk->vec[0].vz          = vz;
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&((AcropolisPlazaBeamScratch*)(head - 0x60))->vec[0]);
    gte_rtps();
    gte_stsxy(&((AcropolisPlazaBeamScratch*)(head - 0x60))->sx);
    gte_stszotz(&((AcropolisPlazaBeamScratch*)(head - 0x60))->otz);
    entry->framesLeft = 0;
    if (blk->otz >= 0x11) {
        if (__builtin_abs(blk->sx) < 0xC0 && __builtin_abs(blk->sy) < 0x98) {
            Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
            brightness  = ((Gp_LcgState >> 16) & 0x7F) | 0x80;
            if (task->spawnArg1 < 5) {
                if (work->yaw - 0x800 >= 0) {
                    if (work->yaw - 0x800 <= 0x200) {
                        work->depth = 0x200;
                        goto depth_done1;
                    }
                    goto depth_deep1;
                }
                if (0x800 - work->yaw <= 0x200) {
                    work->depth = 0x200;
                    goto depth_done1;
                }
            depth_deep1:
                work->depth = 0x800;
            depth_done1:;
                red   = brightness >> 1;
                green = brightness >> 1;
                blue  = brightness;
            } else {
                work->depth = ABS(work->yaw - 0x800) < 0x600 ? 0x800 : 0x200;
                red         = brightness;
                green       = red >> 1;
                blue        = red >> 1;
            }
            depthVal               = work->depth;
            spread                 = depthVal != 0x800 ? 0 : (yaw = work->yaw, work->yaw - 0x800 >= 0 ? (yaw - 0x800) * 4 : (depthVal - yaw) * 4);
            work->spread           = spread;
            lightCoord->coord.t[0] = coord->coord.t[0];
            lightCoord->coord.t[1] = coord->coord.t[1];
            lightCoord->coord.t[2] = coord->coord.t[2];
            lightCoord->flg        = 0;
            entry->framesLeft      = 2;
            light->inner           = 0x600;
            light->outer           = work->spread + 0x600;
            light->head.r          = red << 4;
            light->head.g          = green << 4;
            light->head.b          = blue << 4;
            blk->vec[1].vx         = -0x200;
            blk->vec[1].vy         = 0;
            blk->vec[1].vz         = work->depth;
            blk->vec[2].vx         = 0x200;
            blk->vec[2].vy         = 0;
            blk->vec[2].vz         = work->depth;
            blk->vec[3].vx         = -0x400;
            blk->vec[3].vy         = 0;
            blk->vec[3].vz         = 0x400;
            blk->vec[4].vx         = 0x400;
            blk->vec[4].vy         = 0;
            blk->vec[4].vz         = 0x400;
            blk->vec[5].vx         = -0x200;
            blk->vec[5].vy         = 0;
            blk->vec[5].vz         = 0x100;
            blk->vec[6].vx         = 0x200;
            blk->vec[6].vy         = 0;
            blk->vec[6].vz         = 0x100;
            for (i = 1; i < 7; i++) {
                gte_SetRotMatrix(&coord->workm);
                gte_ldv0(&blk->vec[i]);
                gte_rtv0();
                gte_stsv(&blk->vec[i]);
                // Add the field offset last to keep this pointer separate from the GTE address.
                point           = ((AcropolisPlazaBeamScratch*)((SVECTOR*)blk + i))->vec;
                blk->vec[i].vx += *(u16*)&coord->workm.t[0];
                point->vy      += *(u16*)&coord->workm.t[1];
                point->vz      += *(u16*)&coord->workm.t[2];
            }
            gte_SetRotMatrix(&GsWSMATRIX);
            for (i = 1; i < 7; i++) {
                gte_ldv0(&blk->vec[i]);
                gte_rtps();
                gte_stsxy(&blk->screen[i]);
            }
            i              = 0;
            red            = (s32)(red << 16) >> 18;
            green          = (s32)(green << 16) >> 18;
            blue           = (s32)(blue << 16) >> 18;
            tri            = (POLY_G3*)gGpuPrimCursor;
            gGpuPrimCursor = tri + 1;
            setPolyG3(tri);
            setRGB0(tri, (s16)red * 3, (s16)green * 3, (s16)blue * 3);
            setRGB1(tri, 0, 0, 0);
            setRGB2(tri, 0, 0, 0);
            tri->x0 = blk->sx;
            tri->y0 = blk->sy;
            tri->x1 = (u16)blk->screen[1];
            tri->y1 = (blk->screen[1] >> 16);
            tri->x2 = (u16)blk->screen[2];
            tri->y2 = (blk->screen[2] >> 16);
            addPrim((u_long*)(((((u32)blk->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC) + (s32)gGpuCurrentOt), tri);
            Gp_AddTpageShift((P_TAG*)tri, 1, blk->otz);
            for (; i < 2; i++) {
                prim           = (POLY_G4*)gGpuPrimCursor;
                gGpuPrimCursor = prim + 1;
                setPolyG4(prim);
                setRGB0(prim, 0, 0, 0);
                setRGB1(prim, (s16)red * 2, (s16)green * 2, (s16)blue * 2);
                setRGB2(prim, 0, 0, 0);
                setRGB3(prim, 0, 0, 0);
                prim->x0 = (u16)blk->screen[i + 1];
                prim->y0 = (blk->screen[i + 1] >> 16);
                prim->x1 = blk->sx;
                prim->y1 = blk->sy;
                prim->x2 = (u16)blk->screen[i + 3];
                prim->y2 = (blk->screen[i + 3] >> 16);
                prim->x3 = (u16)blk->screen[i + 5];
                prim->y3 = (blk->screen[i + 5] >> 16);
                addPrim((u_long*)(((((u32)blk->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC) + (s32)gGpuCurrentOt), prim);
                Gp_AddTpageShift((P_TAG*)prim, 1, blk->otz);
            }
            blk->half = 0xC000 / blk->otz;
            red     <<= 1;
            green   <<= 1;
            blue    <<= 1;
            for (i = 0; i < 0x10; i += 2) {
                prim           = (POLY_G4*)gGpuPrimCursor;
                gGpuPrimCursor = prim + 1;
                setPolyG4(prim);
                setRGB0(prim, 0, 0, 0);
                setRGB1(prim, 0, 0, 0);
                setRGB2(prim, red, green, blue);
                setRGB3(prim, 0, 0, 0);
                prim->x0 = blk->sx + ((blk->half * D_acropolis_plaza_801987E0[i + 4]) >> 12);
                prim->y0 = blk->sy + ((blk->half * D_acropolis_plaza_801987E0[i]) >> 12);
                prim->x1 = blk->sx + ((blk->half * D_acropolis_plaza_801987E0[i + 5]) >> 12);
                prim->y1 = blk->sy + ((blk->half * D_acropolis_plaza_801987E0[i + 1]) >> 12);
                prim->x2 = blk->sx;
                prim->y2 = blk->sy;
                prim->x3 = blk->sx + ((blk->half * D_acropolis_plaza_801987E0[i + 6]) >> 12);
                prim->y3 = blk->sy + ((blk->half * D_acropolis_plaza_801987E0[i + 2]) >> 12);
                addPrim((u_long*)(((((u32)blk->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC) + (s32)gGpuCurrentOt), prim);
                Gp_AddTpageShift((P_TAG*)prim, 1, blk->otz);
            }
        }
    }
    Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
    brightness  = ((Gp_LcgState >> 16) & 0x7F) | 0x80;
    if (task->spawnArg1 < 5) {
        if (work->yaw - 0x800 >= 0) {
            if (work->yaw - 0x800 <= 0x300) {
                work->depth = 0x200;
                goto depth_done2;
            }
            goto depth_deep2;
        }
        if (0x800 - work->yaw <= 0x300) {
            work->depth = 0x200;
            goto depth_done2;
        }
    depth_deep2:
        work->depth = 0x800;
    depth_done2:;
        pulse = brightness << 16;
        red   = pulse >> 20;
        green = pulse >> 20;
        blue  = (u32)pulse >> 18;
    } else {
        work->depth = ABS(work->yaw - 0x800) < 0x500 ? 0x800 : 0x200;
        pulse2      = brightness << 16;
        red         = (u32)pulse2 >> 18;
        green       = (s32)pulse2 >> 20;
        blue        = (s32)pulse2 >> 20;
    }
    if (work->depth == 0x800) {
        blk->vec[0].vx = 0;
        blk->vec[0].vy = 0;
        blk->vec[0].vz = 0xE00;
        gte_SetRotMatrix(&coord->workm);
        gte_ldv0(&blk->vec[0]);
        gte_rtv0();
        gte_stsv(&blk->vec[0]);
        blk->vec[0].vx += *(u16*)&coord->workm.t[0];
        blk->vec[0].vy += *(u16*)&coord->workm.t[1];
        blk->vec[0].vz += *(u16*)&coord->workm.t[2];
        gte_SetTransMatrix(&GsWSMATRIX);
        gte_SetRotMatrix(&GsWSMATRIX);
        gte_ldv0(&blk->vec[0]);
        gte_rtps();
        gte_stsxy(&blk->sx);
        gte_stszotz(&blk->otz);
        if (blk->otz >= 0x11) {
            if (__builtin_abs(blk->sx) < 0xC0 && __builtin_abs(blk->sy) < 0x98) {
                blk->half = 0x10000 / blk->otz;
                for (i = 0; i < 0x10; i += 2) {
                    prim           = (POLY_G4*)gGpuPrimCursor;
                    gGpuPrimCursor = prim + 1;
                    setPolyG4(prim);
                    setRGB0(prim, 0, 0, 0);
                    setRGB1(prim, 0, 0, 0);
                    setRGB2(prim, red, green, blue);
                    setRGB3(prim, 0, 0, 0);
                    prim->x0 = blk->sx + ((blk->half * D_acropolis_plaza_801987E0[i + 4]) >> 12);
                    prim->y0 = blk->sy + ((blk->half * D_acropolis_plaza_801987E0[i]) >> 12);
                    prim->x1 = blk->sx + ((blk->half * D_acropolis_plaza_801987E0[i + 5]) >> 12);
                    prim->y1 = blk->sy + ((blk->half * D_acropolis_plaza_801987E0[i + 1]) >> 12);
                    prim->x2 = blk->sx;
                    prim->y2 = blk->sy;
                    prim->x3 = blk->sx + ((blk->half * D_acropolis_plaza_801987E0[i + 6]) >> 12);
                    prim->y3 = blk->sy + ((blk->half * D_acropolis_plaza_801987E0[i + 2]) >> 12);
                    addPrim((u_long*)(((((u32)blk->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC) + (s32)gGpuCurrentOt), prim);
                    Gp_AddTpageShift((P_TAG*)prim, 1, blk->otz);
                }
            }
        }
    }
    work->yaw               = (work->yaw - 0x80) & 0xFFF;
    *(void**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD + 0x60;
}

void func_acropolis_plaza_801811D0(Task* task)
{
    GsCOORDINATE2*              coord;
    AcropolisPlazaBeamWork*     work;
    u8*                         head;
    register u8*                raw asm("v0");
    u16                         vz;
    AcropolisPlazaFlareScratch* blk;
    POLY_G4*                    prim;
    s32                         i;
    s16                         brightness;
    u16                         red, green, blue;
    s32                         yawInit;
    s32                         pulse;
    u32                         pulse2;
    s16                         level;
    coord = ((TmdObject*)task->extra)->coords;
    work  = (AcropolisPlazaBeamWork*)task->spawnArg2;
    if (task->state == 0) {
        yawInit     = (task->spawnArg1 & 1) << 11;
        work->yaw   = yawInit;
        task->state = task->state + 1;
    }
    Gfx_RotMatrixY(&coord->coord, work->yaw, 1);
    coord->flg = 0;
    Gp_UpdateCoord(coord);
    head                    = *(void**)G_SCRATCH_HEAD;
    raw                     = head - 0x4C;
    blk                     = (AcropolisPlazaFlareScratch*)raw;
    blk->vec.vx             = *(u16*)&coord->workm.t[0];
    blk->vec.vy             = *(u16*)&coord->workm.t[1];
    vz                      = *(u16*)&coord->workm.t[2];
    *(void**)G_SCRATCH_HEAD = blk;
    blk->vec.vz             = vz;
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&((AcropolisPlazaFlareScratch*)(head - 0x4C))->vec);
    gte_rtps();
    gte_stsxy(&((AcropolisPlazaFlareScratch*)(head - 0x4C))->sx);
    gte_stszotz(&((AcropolisPlazaFlareScratch*)(head - 0x4C))->otz);
    if (blk->otz >= 0x11) {
        if (__builtin_abs(blk->sx) < 0xC0 && __builtin_abs(blk->sy) < 0x98) {
            pulse = D_80070F70 * 8 + task->spawnArg1 * 0xC0;
            if (pulse & 0x80) {
                level = 0x7F - (pulse & 0x7F);
            } else {
                level = pulse & 0x78;
            }
            brightness = level;
            if (task->spawnArg1 < 9) {
                red   = brightness >> 2;
                green = brightness >> 2;
                blue  = brightness;
            } else {
                red   = brightness;
                green = (s16)red >> 2;
                blue  = (s16)red >> 2;
            }
            blk->half = 0xA000 / blk->otz;
            for (i = 0; i < 0x10; i += 2) {
                prim           = (POLY_G4*)gGpuPrimCursor;
                gGpuPrimCursor = prim + 1;
                setPolyG4(prim);
                setRGB0(prim, 0, 0, 0);
                setRGB1(prim, 0, 0, 0);
                setRGB2(prim, red, green, blue);
                setRGB3(prim, 0, 0, 0);
                prim->x0 = blk->sx + ((blk->half * D_acropolis_plaza_801987E0[i + 4]) >> 12);
                prim->y0 = blk->sy + ((blk->half * D_acropolis_plaza_801987E0[i]) >> 12);
                prim->x1 = blk->sx + ((blk->half * D_acropolis_plaza_801987E0[i + 5]) >> 12);
                prim->y1 = blk->sy + ((blk->half * D_acropolis_plaza_801987E0[i + 1]) >> 12);
                prim->x2 = blk->sx;
                prim->y2 = blk->sy;
                prim->x3 = blk->sx + ((blk->half * D_acropolis_plaza_801987E0[i + 6]) >> 12);
                prim->y3 = blk->sy + ((blk->half * D_acropolis_plaza_801987E0[i + 2]) >> 12);
                addPrim((u_long*)(((((u32)blk->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC) + (s32)gGpuCurrentOt), prim);
                Gp_AddTpageShift((P_TAG*)prim, 1, blk->otz);
            }
            blk->half  = 0x8000 / blk->otz;
            blk->inner = 0x1800 / blk->otz;
            red      <<= 1;
            green    <<= 1;
            blue     <<= 1;
            for (i = 3; i < 0x10; i += 8) {
                prim           = (POLY_G4*)gGpuPrimCursor;
                gGpuPrimCursor = prim + 1;
                setPolyG4(prim);
                setRGB0(prim, 0, 0, 0);
                setRGB1(prim, 0, 0, 0);
                setRGB2(prim, red, green, blue);
                setRGB3(prim, 0, 0, 0);
                prim->x0 = blk->sx + ((blk->inner * D_acropolis_plaza_801987E0[i]) >> 11);
                prim->y0 = blk->sy + ((blk->inner * D_acropolis_plaza_801987E0[i + 12]) >> 11);
                prim->x1 = blk->sx + ((blk->half * D_acropolis_plaza_801987E0[i + 4]) >> 11);
                prim->y1 = blk->sy + ((blk->half * D_acropolis_plaza_801987E0[i]) >> 11);
                prim->x2 = blk->sx;
                prim->y2 = blk->sy;
                prim->x3 = blk->sx + ((blk->inner * D_acropolis_plaza_801987E0[i + 8]) >> 11);
                prim->y3 = blk->sy + ((blk->inner * D_acropolis_plaza_801987E0[i + 4]) >> 11);
                addPrim((u_long*)(((((u32)blk->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC) + (s32)gGpuCurrentOt), prim);
                Gp_AddTpageShift((P_TAG*)prim, 1, blk->otz);
                prim           = (POLY_G4*)gGpuPrimCursor;
                gGpuPrimCursor = prim + 1;
                setPolyG4(prim);
                setRGB0(prim, 0, 0, 0);
                setRGB1(prim, 0, 0, 0);
                setRGB2(prim, red, green, blue);
                setRGB3(prim, 0, 0, 0);
                prim->x0 = blk->sx + ((blk->inner * D_acropolis_plaza_801987E0[i + 4]) >> 12);
                prim->y0 = blk->sy + ((blk->inner * D_acropolis_plaza_801987E0[i]) >> 12);
                prim->x1 = blk->sx + ((blk->half * D_acropolis_plaza_801987E0[i + 8]) >> 12);
                prim->y1 = blk->sy + ((blk->half * D_acropolis_plaza_801987E0[i + 4]) >> 12);
                prim->x2 = blk->sx;
                prim->y2 = blk->sy;
                prim->x3 = blk->sx + ((blk->inner * D_acropolis_plaza_801987E0[i + 12]) >> 12);
                prim->y3 = blk->sy + ((blk->inner * D_acropolis_plaza_801987E0[i + 8]) >> 12);
                addPrim((u_long*)(((((u32)blk->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC) + (s32)gGpuCurrentOt), prim);
                Gp_AddTpageShift((P_TAG*)prim, 1, blk->otz);
            }
        }
    }
    Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
    brightness  = ((Gp_LcgState >> 16) & 0x7F) | 0x80;
    if (task->spawnArg1 < 9) {
        work->depth = ABS(work->yaw - 0x800) > 0x300 ? 0x800 : 0x200;
        pulse       = brightness << 16;
        red         = pulse >> 20;
        green       = pulse >> 20;
        blue        = pulse >> 18;
    } else {
        work->depth = ABS(work->yaw - 0x800) < 0x500 ? 0x800 : 0x200;
        pulse2      = brightness << 16;
        red         = (s32)pulse2 >> 18;
        green       = (s32)pulse2 >> 20;
        blue        = (s32)pulse2 >> 20;
    }
    if (work->depth == 0x800) {
        blk->vec.vx = 0;
        blk->vec.vy = 0;
        blk->vec.vz = 0xE00;
        gte_SetRotMatrix(&coord->workm);
        gte_ldv0(&blk->vec);
        gte_rtv0();
        gte_stsv(&blk->vec);
        blk->vec.vx += *(u16*)&coord->workm.t[0];
        blk->vec.vy += *(u16*)&coord->workm.t[1];
        blk->vec.vz += *(u16*)&coord->workm.t[2];
        gte_SetTransMatrix(&GsWSMATRIX);
        gte_SetRotMatrix(&GsWSMATRIX);
        gte_ldv0(&blk->vec);
        gte_rtps();
        gte_stsxy(&blk->sx);
        gte_stszotz(&blk->otz);
        if (blk->otz >= 0x11) {
            if (__builtin_abs(blk->sx) < 0xC0 && __builtin_abs(blk->sy) < 0x98) {
                blk->half = 0x10000 / blk->otz;
                for (i = 0; i < 0x10; i += 2) {
                    prim           = (POLY_G4*)gGpuPrimCursor;
                    gGpuPrimCursor = prim + 1;
                    setPolyG4(prim);
                    setRGB0(prim, 0, 0, 0);
                    setRGB1(prim, 0, 0, 0);
                    setRGB2(prim, red, green, blue);
                    setRGB3(prim, 0, 0, 0);
                    prim->x0 = blk->sx + ((blk->half * D_acropolis_plaza_801987E0[i + 4]) >> 12);
                    prim->y0 = blk->sy + ((blk->half * D_acropolis_plaza_801987E0[i]) >> 12);
                    prim->x1 = blk->sx + ((blk->half * D_acropolis_plaza_801987E0[i + 5]) >> 12);
                    prim->y1 = blk->sy + ((blk->half * D_acropolis_plaza_801987E0[i + 1]) >> 12);
                    prim->x2 = blk->sx;
                    prim->y2 = blk->sy;
                    prim->x3 = blk->sx + ((blk->half * D_acropolis_plaza_801987E0[i + 6]) >> 12);
                    prim->y3 = blk->sy + ((blk->half * D_acropolis_plaza_801987E0[i + 2]) >> 12);
                    addPrim((u_long*)(((((u32)blk->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC) + (s32)gGpuCurrentOt), prim);
                    Gp_AddTpageShift((P_TAG*)prim, 1, blk->otz);
                }
            }
        }
    }
    work->yaw               = (work->yaw - 0x80) & 0xFFF;
    *(void**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD + 0x4C;
}

void func_acropolis_plaza_80182054(Task* task)
{
    GsCOORDINATE2*             coord;
    u8 *                       head, *raw;
    u16                        vz;
    AcropolisPlazaGlowScratch* blk;
    POLY_G4*                   prim;
    s32                        i, pulse;
    s16                        level;
    s32                        brightness, shade0, shade1;
    s16                        red, green, blue;

    coord = ((TmdObject*)task->extra)->coords;
    Gp_UpdateCoord(coord);
    head = *(void**)G_SCRATCH_HEAD;
    raw  = head - 0x14;
    SOFT_TOUCH_REG(raw);
    blk                     = (AcropolisPlazaGlowScratch*)raw;
    blk->vec.vx             = *(u16*)&coord->workm.t[0];
    blk->vec.vy             = *(u16*)&coord->workm.t[1];
    vz                      = *(u16*)&coord->workm.t[2];
    *(void**)G_SCRATCH_HEAD = blk;
    blk->vec.vz             = vz;
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&((AcropolisPlazaGlowScratch*)(head - 0x14))->vec);
    gte_rtps();
    gte_stsxy(&((AcropolisPlazaGlowScratch*)(head - 0x14))->sx);
    gte_stszotz(&blk->otz);
    if (((AcropolisPlazaGlowScratch*)(head - 0x14))->otz >= 0x11) {
        if (__builtin_abs(blk->sx) < 0xC0 && __builtin_abs(blk->sy) < 0x98) {
            if (task->spawnArg1 < 0x10) {
                Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
                brightness  = (((Gp_LcgState >> 16) & 0x3F) + 0x80) << 16;
                shade0      = brightness >> 17;
                red         = shade0;
                green       = shade0;
                blue        = (u32)brightness >> 18;
                blk->half   = 0xC000 / ((AcropolisPlazaGlowScratch*)(head - 0x14))->otz;
            } else {
                pulse = D_80070F70 * 6;
                if (pulse & 0x80) {
                    level = 0x7F - (pulse & 0x7F);
                } else {
                    level = pulse & 0x7E;
                }
                red       = level;
                shade1    = (s32)(red << 16) >> 18;
                green     = shade1;
                blue      = shade1;
                blk->half = 0x8000 / blk->otz;
            }
            for (i = 0; i < 0x10; i += 2) {
                prim           = (POLY_G4*)gGpuPrimCursor;
                gGpuPrimCursor = prim + 1;
                setPolyG4(prim);
                setRGB0(prim, 0, 0, 0);
                setRGB1(prim, 0, 0, 0);
                setRGB2(prim, red, green, blue);
                setRGB3(prim, 0, 0, 0);
                prim->x0 = blk->sx + ((blk->half * D_acropolis_plaza_801987E0[i + 4]) >> 12);
                prim->y0 = blk->sy + ((blk->half * D_acropolis_plaza_801987E0[i]) >> 12);
                prim->x1 = blk->sx + ((blk->half * D_acropolis_plaza_801987E0[i + 5]) >> 12);
                prim->y1 = blk->sy + ((blk->half * D_acropolis_plaza_801987E0[i + 1]) >> 12);
                prim->x2 = blk->sx;
                prim->y2 = blk->sy;
                prim->x3 = blk->sx + ((blk->half * D_acropolis_plaza_801987E0[i + 6]) >> 12);
                prim->y3 = blk->sy + ((blk->half * D_acropolis_plaza_801987E0[i + 2]) >> 12);
                addPrim((u_long*)(((((u32)blk->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC) + (s32)gGpuCurrentOt), prim);
                Gp_AddTpageShift((P_TAG*)prim, 1, blk->otz);
            }
        }
    }
    *(void**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD + 0x14;
}

/// Plaza ambient-effect spawner. On its first frame only, it fires three bursts
/// of `Gp_SpawnEff` against the task's own coordinate frame - seven 0x60096
/// effects on slots 0xC-0x12, four 0x60099 on slots 7-0xA and six 0x60098 on
/// slots 1-6 - each anchored at the matching entry of
/// `D_acropolis_plaza_80198820`. Every later frame is a no-op.
void func_acropolis_plaza_8018251C(Task* task)
{
    GsCOORDINATE2* coord;
    s32            i;

    coord = ((TmdObject*)task->extra)->coords;
    if (task->state == 0) {
        for (i = 0xC; i < 0x13; i++) {
            Gp_SpawnEff(0x60096, coord, i, &D_acropolis_plaza_80198820[i]);
        }
        for (i = 7; i < 0xB; i++) {
            Gp_SpawnEff(0x60099, coord, i, &D_acropolis_plaza_80198820[i]);
        }
        for (i = 1; i < 7; i++) {
            Gp_SpawnEff(0x60098, coord, i, &D_acropolis_plaza_80198820[i]);
        }
        task->state = task->state + 1;
    }
}

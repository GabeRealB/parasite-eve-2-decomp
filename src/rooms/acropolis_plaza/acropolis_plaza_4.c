#include "common.h"
#include "gameplay/1A8.h"
#include "gameplay/3A34.h"
#include "gameplay/3CD8.h"
#include "gameplay/3FB8.h"
#include "gameplay/D4.h"
#include "gameplay/gameplay.h"
#include "main/display.h"
#include "main/fs.h"
#include "main/mc.h"
#include "main/mem.h"
#include "main/session.h"
#include "main/sound.h"
#include "main/task.h"
#include "rooms/acropolis_plaza.h"

extern s8       D_8007106B;
extern s16      D_80071076;
extern TaskDesc D_acropolis_plaza_80183824;

/// View the plaza's opening sequence applies before it spawns anything.
extern GpViewRec D_acropolis_plaza_801838B8[][2];

extern void Stage_RequestFromAreaTable(s32 arg0);

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

/// Per-frame room work stepped by the plaza's cutscene tails, in
/// `acropolis_plaza_3`.
extern void func_acropolis_plaza_8017DE24(s32 arg0);

/// The pair of blocks `func_acropolis_plaza_8017E7E4` hands to `func_800E8634`
/// once the streamed scene it waits on has finished.
extern u8 D_acropolis_plaza_80182734[];
extern u8 D_acropolis_plaza_80182A34[];

/// The three blocks `func_acropolis_plaza_8017F48C` picks between with
/// `Task::spawnArg1` before handing one to `func_800E8614`.
extern u8 D_acropolis_plaza_80183554[];
extern u8 D_acropolis_plaza_8018365C[];
extern u8 D_acropolis_plaza_80183764[];

/// Five-state warp sequence. State 0 allocates the work block, caches the
/// slot-3 task in it and places the player at (0x3804, 0, 0xFC8) with msg
/// 0x3F2; states 1 and 2 wait for slot 3 to go idle (msg 0x3F0), state 1
/// following up with the 0xD55 warp (msg 0x3EE). State 3 waits for the stream
/// to finish, latches `CdCmd_Queue.field_1EE` into the cutscene work block,
/// kills the task it names and runs `func_800E8634`; state 4 kills this task
/// once the session is out of its transition.
void func_acropolis_plaza_8017E7E4(Task* task)
{
    GpMsg3EE                place;
    GpMsg3EE                warp;
    CdCmdQueue*             q    = &CdCmd_Queue;
    AcropolisPlazaWarpWork* work = (AcropolisPlazaWarpWork*)task->idMap;
    AcropolisPlazaWarpWork* newWork;

    switch (task->state) {
        case 0:
            newWork     = Mem_Malloc(8, 0);
            task->idMap = (TaskIdMap*)newWork;
            if (newWork == NULL) {
                Task_Kill(task);
                return;
            }
            Mem_Set(newWork, 0, 8);
            ((AcropolisPlazaWarpWork*)task->idMap)->slot3 = Game_GetPtrSlot(3);
            place.field_0                                 = 0x3804;
            place.field_4                                 = 0;
            place.field_8                                 = 0xFC8;
            Gp_DispatchMsg(((AcropolisPlazaWarpWork*)task->idMap)->slot3, 0x3F2, (s32)&place, 0);
            task->state = task->state + 1;
            return;
        case 1:
            if (Gp_DispatchMsg(work->slot3, 0x3F0, 0, 0) != 0) {
                return;
            }
            warp.field_12 = 0xD55;
            Gp_DispatchMsg(((AcropolisPlazaWarpWork*)task->idMap)->slot3, 0x3EE, (s32)&warp, 0);
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
            Task_Kill(((AcropolisPlazaCutWork*)task->spawnArg2)->task);
            func_800E8634((s32)D_acropolis_plaza_80182734, 1, (s32)D_acropolis_plaza_80182A34);
            task->state = task->state + 1;
            return;
        case 4:
            if (Game_Session->field_1 == 0) {
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
    GpMsg3EE                place;
    GpMsg3EE                warp;
    GpRec14                 script;
    AcropolisPlazaTailMsg   buf;
    GpRec14*                rec;
    CdCmdQueue*             q    = &CdCmd_Queue;
    AcropolisPlazaWarpWork* work = (AcropolisPlazaWarpWork*)task->idMap;
    AcropolisPlazaWarpWork* newWork;
    GsCOORDINATE2*          coord;
    s32                     weaponId;
    s32                     id;

    switch (task->state) {
        case 0:
            newWork     = Mem_Malloc(8, 0);
            task->idMap = (TaskIdMap*)newWork;
            if (newWork == NULL) {
                Task_Kill(task);
                return;
            }
            Mem_Set(newWork, 0, 8);
            ((AcropolisPlazaWarpWork*)task->idMap)->slot3 = Game_GetPtrSlot(3);
            place.field_0                                 = 0xF6E;
            place.field_4                                 = 0;
            place.field_8                                 = 0x2328;
            Gp_DispatchMsg(((AcropolisPlazaWarpWork*)task->idMap)->slot3, 0x3F2, (s32)&place, 0);
            task->state = task->state + 1;
            return;
        case 1:
            if (Gp_DispatchMsg(work->slot3, 0x3F0, 0, 0) != 0) {
                return;
            }
            warp.field_12 = 0xD55;
            Gp_DispatchMsg(((AcropolisPlazaWarpWork*)task->idMap)->slot3, 0x3EE, (s32)&warp, 0);
            task->state = task->state + 1;
            return;
        case 2:
            if (Gp_DispatchMsg(work->slot3, 0x3F0, 0, 0) != 0) {
                return;
            }
            script.field_0  = (s32)&D_801797FC;
            script.field_4  = 0xB;
            script.field_8  = 0;
            script.field_C  = 0;
            script.field_10 = 1;
            Gp_DispatchMsg(work->slot3, 0x3F4, (s32)&script, 0);
            task->state = task->state + 1;
            return;
        case 3:
            if (CdCmd_IsIdle() == 0) {
                return;
            }
            ((AcropolisPlazaCutWork*)task->spawnArg2)->field_1A = q->field_1EE;
            Task_Kill(((AcropolisPlazaCutWork*)task->spawnArg2)->task);
            q->field_1EE = 1;
            q->field_1EA = 1;
            q->field_1F8 = 2;
            buf.slot[0]  = Stream_FindSlot(&Game_Session->field_4, 2, 0);
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
                rec                     = &buf.weapon.rec;
                weaponId                = D_80073BA9;
                id                      = (D_8007218A == 1) ? weaponId + 1 : weaponId + 0x22;
                buf.weapon.rec.field_0  = id;
                rec->field_4            = 1;
                buf.weapon.rec.field_8  = 0;
                rec->field_C            = 0xA;
                buf.weapon.rec.field_10 = 0;
                Gp_DispatchMsg(Game_GetPtrSlot(3), 0x3E8, (s32)&buf.weapon.rec, 0);

                coord            = ((TmdObject*)((AcropolisPlazaWarpWork*)task->idMap)->slot3->extra)->field_8;
                buf.place.pos.vx = coord->coord.t[0];
                buf.place.pos.vy = coord->coord.t[1];
                buf.place.pos.vz = coord->coord.t[2];
                buf.place.rot.vz = 0;
                buf.place.rot.vx = 0;
                buf.place.rot.vy = 0xEAA;
                Gp_DispatchMsg(((AcropolisPlazaWarpWork*)task->idMap)->slot3, 0x3E9, (s32)&buf.place, 0);
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

INCLUDE_ASM("rooms/nonmatchings/acropolis_plaza/acropolis_plaza_4", func_acropolis_plaza_8017ECF8);

/// Three-state cutscene tail: state 0 republishes the player's weapon to slot
/// 3 (msg 0x3E8), state 1 waits for the streamed scene to finish -- latching
/// `CdCmd_Queue.field_1EE` into the work block, killing the block's task and
/// running the block `spawnArg1` names -- and state 2 kills this task once the
/// session is out of its transition.
void func_acropolis_plaza_8017F48C(Task* task)
{
    GpRec14     rec;
    CdCmdQueue* q = &CdCmd_Queue;
    s32         state;
    s32         weaponId;
    s32         id;

    state = task->state;
    switch (state) {
        case 0:
            weaponId     = D_80073BA9;
            id           = (D_8007218A == 1) ? weaponId + 1 : weaponId + 0x22;
            rec.field_0  = id;
            rec.field_4  = 1;
            rec.field_8  = 0;
            rec.field_C  = 0xA;
            rec.field_10 = 0;
            Gp_DispatchMsg(Game_GetPtrSlot(3), 0x3E8, (s32)&rec, 0);
            task->state = task->state + 1;
            break;
        case 1:
            if (CdCmd_IsIdle() != 0) {
                ((AcropolisPlazaCutWork*)task->spawnArg2)->field_1A = q->field_1EE;
                Task_Kill(((AcropolisPlazaCutWork*)task->spawnArg2)->task);
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
            if (Game_Session->field_1 == 0) {
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
    GpRec14     rec;
    CdCmdQueue* q = &CdCmd_Queue;
    s32         weaponId;
    s32         id;

    switch (task->state) {
        case 0:
            weaponId     = D_80073BA9;
            id           = (D_8007218A == 1) ? weaponId + 1 : weaponId + 0x22;
            rec.field_0  = id;
            rec.field_4  = 1;
            rec.field_8  = 0;
            rec.field_C  = 0xA;
            rec.field_10 = 0;
            Gp_DispatchMsg(Game_GetPtrSlot(3), 0x3E8, (s32)&rec, 0);
            task->state = task->state + 1;
            break;
        case 1:
            if (CdCmd_IsIdle() != 0) {
                ((AcropolisPlazaCutWork*)task->spawnArg2)->field_1A = q->field_1EE;
                Task_Kill(((AcropolisPlazaCutWork*)task->spawnArg2)->task);
                Gp_RunCapCmd1(((AcropolisPlazaCutWork*)task->spawnArg2)->capCmd);
                task->state = task->state + 1;
            }
            break;
        case 2:
            Gp_DispatchMsg(Game_GetPtrSlot(3), 0x3F1, 1, 0);
            Task_RequestKill(task, 0);
            break;
    }
}

/// Ambience voice driver: starts the voice named by `sndId` the first time
/// `state` is clear, then tracks `CdCmd_Queue.field_1EE` between `fadeIn` and
/// `fadeOut` to ramp its volume.
void func_acropolis_plaza_8017F770(u16 fadeIn, u16 fadeOut, u16 hold, u16* state, s32 sndId, u16 mode);

INCLUDE_ASM("rooms/nonmatchings/acropolis_plaza/acropolis_plaza_4", func_acropolis_plaza_8017F770);

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
    AcropolisPlazaWork* work  = (AcropolisPlazaWork*)task->idMap;
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
u16 func_acropolis_plaza_8017FB50(Task* task);

INCLUDE_ASM("rooms/nonmatchings/acropolis_plaza/acropolis_plaza_4", func_acropolis_plaza_8017FB50);

/// Draws the cinematic letterbox: two black 0x140x0x18 `TILE` bars spanning the
/// full screen width at the top (y -0x78) and bottom (y 0x60), linked into
/// `Gpu_CurrentOt[3]`.
void func_acropolis_plaza_8017FF18(void)
{
    TILE* tile;

    tile           = (TILE*)Gpu_PrimCursor;
    Gpu_PrimCursor = (DR_TPAGE*)(tile + 1);
    SetTile(tile);
    tile->b0 = 0;
    tile->g0 = 0;
    tile->r0 = 0;
    tile->x0 = -0xA0;
    tile->y0 = -0x78;
    tile->w  = 0x140;
    tile->h  = 0x18;
    addPrim(Gpu_CurrentOt + 3, tile);

    tile           = (TILE*)Gpu_PrimCursor;
    Gpu_PrimCursor = (DR_TPAGE*)(tile + 1);
    SetTile(tile);
    tile->b0 = 0;
    tile->g0 = 0;
    tile->r0 = 0;
    tile->x0 = -0xA0;
    tile->y0 = 0x60;
    tile->w  = 0x140;
    tile->h  = 0x18;
    addPrim(Gpu_CurrentOt + 3, tile);
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
    AcropolisPlazaWork* work = (AcropolisPlazaWork*)task->idMap;
    AcropolisPlazaWork* newWork;
    SVECTOR             vec;

    switch (task->state) {
        case 0:
            func_800E9BDC(3, 0x9DF);
            Gp_ApplyView(D_acropolis_plaza_801838B8[0]);
            newWork     = (AcropolisPlazaWork*)Mem_Malloc(0x28, 0);
            task->idMap = (TaskIdMap*)newWork;
            if (newWork == NULL) {
                Task_Kill(task);
                return;
            }
            Mem_Set(newWork, 0, 0x28);
            ((AcropolisPlazaWork*)task->idMap)->slot3 = Game_GetPtrSlot(3);
            ((AcropolisPlazaWork*)task->idMap)->field_C =
                Task_SpawnFromTable(&D_acropolis_plaza_80183824, 5, 0, 0);
            Gp_KillPlayerEffs();
            Task_SpawnFromTable(&D_acropolis_plaza_80183824, 0xB, 0, 0);
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
            Gp_DispatchMsg(Game_GetPtrSlot(6), 0xFA4, 0, 0);
            work->field_12 = 0;
            work->field_10 = 0;
            work->field_8  = Task_SpawnFromTable(&D_acropolis_plaza_80183824, 1, 0, (s32)&work->field_10);
            Stage_RequestFromAreaTable(0);
            Task_SpawnFromTable(&D_acropolis_plaza_80183824, 8, 6, 0);
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
            Mc_SaveData.field_7 = 1;
            Mc_SaveData.field_8 = 1;
            Mc_SaveData.field_6 = 0x11;
            Mc_SaveData.field_5 = 1;
            D_80071076          = 1;
            Gp_EnqueueHeldWeaponCd();
            SndEvt_EnqueueType7(0x80000000, 0);
            Task_Spawn(0, 0x11, 0, 0);
            q->field_244 = 0;
            Task_Kill(task);
            return;
    }
}

void func_acropolis_plaza_80180270(Task* arg0)
{
    Display_SpawnWithOt(&D_acropolis_plaza_80183824, 0xA, 0, 0);
    D_8007106B = 1;
    Gp_SpawnViewTasks();
    Task_Kill(arg0);
}

INCLUDE_ASM("rooms/nonmatchings/acropolis_plaza/acropolis_plaza_4", func_acropolis_plaza_801802C0);

INCLUDE_ASM("rooms/nonmatchings/acropolis_plaza/acropolis_plaza_4", func_acropolis_plaza_801811D0);

INCLUDE_ASM("rooms/nonmatchings/acropolis_plaza/acropolis_plaza_4", func_acropolis_plaza_80182054);

/// Ambient-effect anchor points, one `SVECTOR` per effect slot. The plaza's
/// three effect bursts index this table with the same slot number they pass to
/// `Gp_SpawnEff`, so entries 1-6, 7-0xA and 0xC-0x12 belong to the 0x60098,
/// 0x60099 and 0x60096 flavours respectively. The block runs well past entry
/// 0x12, so this declaration is left unsized.
extern SVECTOR D_acropolis_plaza_80198820[];

/// Plaza ambient-effect spawner. On its first frame only, it fires three bursts
/// of `Gp_SpawnEff` against the task's own coordinate frame - seven 0x60096
/// effects on slots 0xC-0x12, four 0x60099 on slots 7-0xA and six 0x60098 on
/// slots 1-6 - each anchored at the matching entry of
/// `D_acropolis_plaza_80198820`. Every later frame is a no-op.
void func_acropolis_plaza_8018251C(Task* task)
{
    GsCOORDINATE2* coord;
    s32            i;

    coord = ((TmdObject*)task->extra)->field_8;
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

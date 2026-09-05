#include "common.h"
#include "gameplay/1A8.h"
#include "gameplay/3CD8.h"
#include "gameplay/D4.h"
#include "gameplay/gameplay.h"
#include "main/display.h"
#include "main/fs.h"
#include "main/mem.h"
#include "main/session.h"
#include "main/task.h"
#include "rooms/acropolis_plaza.h"

extern s8       D_8007106B;
extern TaskDesc D_acropolis_plaza_80183824;

/// Main-executable globals with no module header yet: `D_80073BA9` is the
/// equipped-weapon index the slot-3 msg 0x3E8 record is keyed on, and
/// `D_8007218A` picks which of the two weapon-id bases that record uses.
extern u8 D_80073BA9;
extern s8 D_8007218A;

/// The pair of blocks `func_acropolis_plaza_8017E7E4` hands to `func_800E8634`
/// once the streamed scene it waits on has finished.
extern u8 D_acropolis_plaza_80182734[];
extern u8 D_acropolis_plaza_80182A34[];

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

INCLUDE_ASM("rooms/nonmatchings/acropolis_plaza/acropolis_plaza_4", func_acropolis_plaza_8017E9A8);

INCLUDE_ASM("rooms/nonmatchings/acropolis_plaza/acropolis_plaza_4", func_acropolis_plaza_8017ECF8);

INCLUDE_ASM("rooms/nonmatchings/acropolis_plaza/acropolis_plaza_4", func_acropolis_plaza_8017F48C);

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

INCLUDE_ASM("rooms/nonmatchings/acropolis_plaza/acropolis_plaza_4", func_acropolis_plaza_8017F770);

INCLUDE_ASM("rooms/nonmatchings/acropolis_plaza/acropolis_plaza_4", func_acropolis_plaza_8017F9EC);

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

INCLUDE_ASM("rooms/nonmatchings/acropolis_plaza/acropolis_plaza_4", func_acropolis_plaza_80180054);

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

INCLUDE_ASM("rooms/nonmatchings/acropolis_plaza/acropolis_plaza_4", func_acropolis_plaza_8018251C);

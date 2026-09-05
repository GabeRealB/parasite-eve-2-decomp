#include "common.h"
#include "gameplay/3CD8.h"
#include "gameplay/D4.h"
#include "gameplay/gameplay.h"
#include "main/display.h"
#include "main/fs.h"
#include "main/task.h"
#include "rooms/acropolis_plaza.h"

extern s8       D_8007106B;
extern TaskDesc D_acropolis_plaza_80183824;

/// Main-executable globals with no module header yet: `D_80073BA9` is the
/// equipped-weapon index the slot-3 msg 0x3E8 record is keyed on, and
/// `D_8007218A` picks which of the two weapon-id bases that record uses.
extern u8 D_80073BA9;
extern s8 D_8007218A;

/// Two four-vertex quads facing each other across the plaza's scene object:
/// one at x - 0xBB8, one at x + 0x7D0, each spanning y .. y + 0x3E8 and
/// z - 0x1000 .. z + 0x3000. The second quad's vertices run in the opposite
/// z order, flipping its winding.
extern SVECTOR D_acropolis_plaza_80198B90[8];

/// Rebuilds the eight box vertices in `D_acropolis_plaza_80198B90` around the
/// scene work's world position.
void func_acropolis_plaza_8017DD90(Task* arg0)
{
    AcropolisPlazaWork* work = (AcropolisPlazaWork*)arg0->idMap;
    s32                 x    = work->pos.vx;
    s32                 y    = work->pos.vy;
    s32                 z    = work->pos.vz;
    s16                 near = x - 0xBB8;
    s16                 top;
    s16                 left;
    s16                 right;
    s16                 far;

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

INCLUDE_RODATA("rooms/nonmatchings/acropolis_plaza/acropolis_plaza_3", D_acropolis_plaza_8017D5E0);

INCLUDE_ASM("rooms/nonmatchings/acropolis_plaza/acropolis_plaza_3", func_acropolis_plaza_8017DE24);

INCLUDE_ASM("rooms/nonmatchings/acropolis_plaza/acropolis_plaza_3", func_acropolis_plaza_8017DFE0);

INCLUDE_ASM("rooms/nonmatchings/acropolis_plaza/acropolis_plaza_3", func_acropolis_plaza_8017E7E4);

INCLUDE_ASM("rooms/nonmatchings/acropolis_plaza/acropolis_plaza_3", func_acropolis_plaza_8017E9A8);

INCLUDE_ASM("rooms/nonmatchings/acropolis_plaza/acropolis_plaza_3", func_acropolis_plaza_8017ECF8);

INCLUDE_ASM("rooms/nonmatchings/acropolis_plaza/acropolis_plaza_3", func_acropolis_plaza_8017F48C);

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

INCLUDE_ASM("rooms/nonmatchings/acropolis_plaza/acropolis_plaza_3", func_acropolis_plaza_8017F770);

INCLUDE_ASM("rooms/nonmatchings/acropolis_plaza/acropolis_plaza_3", func_acropolis_plaza_8017F9EC);

INCLUDE_ASM("rooms/nonmatchings/acropolis_plaza/acropolis_plaza_3", func_acropolis_plaza_8017FB50);

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

INCLUDE_ASM("rooms/nonmatchings/acropolis_plaza/acropolis_plaza_3", func_acropolis_plaza_80180054);

void func_acropolis_plaza_80180270(Task* arg0)
{
    Display_SpawnWithOt(&D_acropolis_plaza_80183824, 0xA, 0, 0);
    D_8007106B = 1;
    Gp_SpawnViewTasks();
    Task_Kill(arg0);
}

INCLUDE_ASM("rooms/nonmatchings/acropolis_plaza/acropolis_plaza_3", func_acropolis_plaza_801802C0);

INCLUDE_ASM("rooms/nonmatchings/acropolis_plaza/acropolis_plaza_3", func_acropolis_plaza_801811D0);

INCLUDE_ASM("rooms/nonmatchings/acropolis_plaza/acropolis_plaza_3", func_acropolis_plaza_80182054);

INCLUDE_ASM("rooms/nonmatchings/acropolis_plaza/acropolis_plaza_3", func_acropolis_plaza_8018251C);

#include "common.h"

#include "gameplay/1BC.h"
#include "gameplay/3A34.h"
#include "gameplay/3CD8.h"
#include "gameplay/D4.h"
#include "gameplay/gameplay.h"

#include "main/mem.h"
#include "main/session.h"
#include "main/task.h"
#include "main/tmd.h"

#include <psyq/libgs.h>
#include <psyq/libgte.h>

extern GsCOORDINATE2 Gfx_ViewCoord;
extern SVECTOR       D_acropolis_west_elevator_hall_8017D5EC;
extern SVECTOR       D_acropolis_west_elevator_hall_8017D5F4;
extern GpMsgEntry    D_acropolis_west_elevator_hall_801849CC[];
extern GpMsgEntry    D_acropolis_west_elevator_hall_801849F4[];
extern TaskDesc      D_acropolis_west_elevator_hall_80184568[];
extern Task*         RoomsShared8017f470Cars[];
/// The lift bay's two 256-entry RGB555 CLUTs and the blend destination:
/// `...80184A04` is the unlit base palette, `...80184C04` the lit one and
/// `...80184E04` the blended result that `...80185004` uploads to VRAM.
extern u16      D_acropolis_west_elevator_hall_80184A04[];
extern u16      D_acropolis_west_elevator_hall_80184C04[];
extern u16      D_acropolis_west_elevator_hall_80184E04[];
extern GpImgRec D_acropolis_west_elevator_hall_80185004[];
/// Scratch state of the west-elevator-hall lift task, stored at `Task::idMap`.
/// `func_acropolis_west_elevator_hall_8017F64C` allocates it with
/// `Mem_Calloc(4, 0)`, so the size below is the allocation and not a guess.
typedef struct {
    /* 0x0 */ s32 field_0;
} AwehElevatorState;

void func_acropolis_west_elevator_hall_8017F568(Task* arg0)
{
    arg0->field_24 = D_acropolis_west_elevator_hall_801849CC;
    Game_SetPtrSlot(arg0, 7);
    RoomsShared8017f470Cars[0] =
        Task_SpawnFromTable(D_acropolis_west_elevator_hall_80184568, 0, 0, -1);
    RoomsShared8017f470Cars[1] =
        Task_SpawnFromTable(D_acropolis_west_elevator_hall_80184568, 1, 0, 1);
    arg0->state = (s32)(arg0->state + 1);
}

INCLUDE_ASM("rooms/nonmatchings/acropolis_west_elevator_hall/acropolis_west_elevator_hall_3", func_acropolis_west_elevator_hall_8017F5F4);

/// Second state of the elevator task: allocates its scratch block, parks the
/// car model at its starting position and parents it to the room's view
/// coordinate system.
void func_acropolis_west_elevator_hall_8017F64C(Task* task)
{
    TmdObject*         extra;
    GsCOORDINATE2*     coord;
    AwehElevatorState* work;

    extra = (TmdObject*)task->extra;
    coord = extra->field_8;
    work  = (AwehElevatorState*)Mem_Calloc(sizeof(AwehElevatorState), 0);
    if (work == NULL) {
        Task_Kill(task);
        return;
    }
    task->idMap       = (TaskIdMap*)work;
    work->field_0     = 0;
    extra->field_C    = 0;
    coord->sub        = &Gfx_ViewCoord;
    coord->coord.t[0] = -1000;
    coord->coord.t[1] = -20;
    coord->coord.t[2] = 0x974;
    coord->flg        = 0;
    task->state++;
}

/// Fourth state of the elevator task: drives the car along its shaft from the
/// task's per-frame step, clamps the travel to [0, 0x2D0], and refreshes the
/// model's world matrix and lighting from the resulting position.
void func_acropolis_west_elevator_hall_8017F6F0(Task* task)
{
    VECTOR             pos;
    TmdObject*         extra;
    GsCOORDINATE2*     coord;
    AwehElevatorState* work;

    work  = (AwehElevatorState*)task->idMap;
    extra = (TmdObject*)task->extra;
    coord = extra->field_8;

    work->field_0 += task->spawnArg1 * 0x14;
    if (work->field_0 < 0) {
        work->field_0 = 0;
    }
    if (work->field_0 >= 0x2D1) {
        work->field_0 = 0x2D0;
    }
    coord->coord.t[0] = (work->field_0 * (s32)task->spawnArg2) - 1000;
    if ((u8)Game_Session->field_4 == 5) {
        extra->field_C = 0;
    } else {
        extra->field_C = 0x80;
    }
    coord->flg = 0;
    Gp_UpdateCoord(coord);
    pos.vx = coord->workm.t[0];
    pos.vy = coord->workm.t[1];
    pos.vz = coord->workm.t[2];
    func_800D7A9C(extra, &pos, 0, 3);
}

/// Third state of the elevator task: on the two session phases that use it,
/// spawns the lift's ambient effects around the room's coordinate system.
void func_acropolis_west_elevator_hall_8017F7D4(Task* task)
{
    SVECTOR        pos;
    SVECTOR        altPos;
    GsCOORDINATE2* coord;

    coord = ((TmdObject*)task->extra)->field_8;
    switch (task->state) {
        case 0:
            task->field_24 = D_acropolis_west_elevator_hall_801849F4;
            Game_SetPtrSlot(task, 5);
            Task_Spawn(1, 0x25, 0, 0);
            Task_Spawn(1, 0x25, 1, 0);
            task->state = task->state + 1;
            return;
        case 1:
            if ((u8)Game_Session->field_4 == 2) {
                pos = D_acropolis_west_elevator_hall_8017D5EC;
                Gp_SpawnEff(0x6001F, coord, 0x1804, &pos);
                pos.vx = -0x1800;
                pos.vy = -0x4F0;
                pos.vz = -0x600;
                Gp_SpawnEff(0x6001F, coord, 0x803, &pos);
                pos.vx = -0x1800;
                pos.vy = -0x4F0;
                pos.vz = -0x2C0;
                Gp_SpawnEff(0x6001F, coord, 0x803, &pos);
            }
            if ((u8)Game_Session->field_4 == 5) {
                altPos = D_acropolis_west_elevator_hall_8017D5F4;
                Gp_SpawnEff(0x60025, coord, 0, &altPos);
            }
            return;
    }
}

/// Per-frame update of the lift bay's lighting: ramps `GpEffWork::field_24`
/// from 0 to 0x1000 in 0x800 steps, re-blending the bay CLUT towards its lit
/// palette on every step it takes, and latching `field_26` once the ramp is
/// full. On every session phase but 5 the CLUT is then blended straight back
/// to the unlit palette and the effect's work object is released, so only
/// phase 5 keeps the lit bay on screen.
void func_acropolis_west_elevator_hall_8017F990(Task* task)
{
    GpEffWork* work;
    s32        i;
    s32        blend;

    work  = (GpEffWork*)task->spawnArg2;
    blend = 0;
    if (work->field_26 == 0) {
        work->field_24 = work->field_24 + 0x800;
        if (work->field_24 == 0x1000) {
            work->field_26 = 1;
        }
        blend = 1;
    }

    if (blend != 0) {
        for (i = 0; i < 0x100; i += 0x10) {
            Gp_BlendRgb555Clut(&D_acropolis_west_elevator_hall_80184C04[i],
                               &D_acropolis_west_elevator_hall_80184A04[i], work->field_24,
                               &D_acropolis_west_elevator_hall_80184E04[i]);
        }
        Gp_LoadImages(D_acropolis_west_elevator_hall_80185004);
    }

    if ((u8)Game_Session->field_4 != 5) {
        for (i = 0; i < 0x100; i += 0x10) {
            Gp_BlendRgb555Clut(&D_acropolis_west_elevator_hall_80184C04[i],
                               &D_acropolis_west_elevator_hall_80184A04[i], 0,
                               &D_acropolis_west_elevator_hall_80184E04[i]);
        }
        Gp_LoadImages(D_acropolis_west_elevator_hall_80185004);
        Gp_ReleaseState1CMem(work, task);
    }
}

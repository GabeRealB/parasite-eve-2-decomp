#include "common.h"

#include "main/gfx.h"
#include "main/mem.h"
#include "main/task.h"
#include "main/tmd.h"

#include "gameplay/3A34.h"
#include "gameplay/D4.h"

#include "actors/actor_341900.h"

extern GpMsgEntry D_actor_341900_80163A38[];

extern void func_80143490(s32 arg0);
extern s32  D_80144A74;
extern s32  D_80144A7C;

INCLUDE_ASM("actors/nonmatchings/actor_341900/actor_341900", func_actor_341900_80161E58);

INCLUDE_ASM("actors/nonmatchings/actor_341900/actor_341900", func_actor_341900_80161FD0);

/// Turns the model's world translation into the light/colour matrix pair the
/// actor draws with, allocating that pair on the first frame.
void func_actor_341900_80162200(Task* arg0)
{
    TmdObject*           extra;
    TmdObject*           mdl;
    Actor341900ColorMtx* mtx;
    VECTOR               pos;

    if (arg0->state == 0) {
        extra       = (TmdObject*)arg0->extra;
        mtx         = (Actor341900ColorMtx*)Mem_Malloc(0x44, 0);
        arg0->idMap = (TaskIdMap*)mtx;
        if (mtx == NULL) {
            Task_Kill(arg0);
        } else {
            Mem_Set(mtx, 0, 0x44);
            mtx->field_40                           = (Task*)arg0->spawnArg2;
            extra->field_C                          = 0;
            ((TmdObject*)arg0->extra)->field_8->sub = &Gfx_ViewCoord;
            extra->field_1C                         = &mtx->light;
            extra->field_20                         = &mtx->color;
            extra->field_E                          = 0x1F;
            arg0->field_24                          = D_actor_341900_80163A38;
            Task_Reparent(mtx->field_40, arg0);
        }
        arg0->state++;
    }

    mdl    = (TmdObject*)arg0->extra;
    pos.vx = ((TmdObject*)arg0->extra)->field_8->workm.t[0];
    pos.vy = ((TmdObject*)arg0->extra)->field_8->workm.t[1];
    pos.vz = ((TmdObject*)arg0->extra)->field_8->workm.t[2];
    func_800D7A9C(mdl, &pos, 0, 3);
}

INCLUDE_ASM("actors/nonmatchings/actor_341900/actor_341900", func_actor_341900_80162330);

/// Attaches the actor to the bone its spawn record names, copies that record's
/// offset onto the part's coordinate, inherits the spawner's colour flag and
/// pushes the part's translation through the draw matrix.
void func_actor_341900_801625B4(Task* arg0)
{
    Actor341900TaskWork* work = (Actor341900TaskWork*)arg0->idMap;
    TmdObject*           extra;
    TmdObject*           mdl;
    GsCOORDINATE2*       coord;
    VECTOR               pos;

    if (arg0->state == 0) {
        func_actor_341900_80162330(arg0);
        work = (Actor341900TaskWork*)arg0->idMap;

        extra      = (TmdObject*)arg0->extra;
        coord      = extra->field_8;
        coord->sub = &((TmdObject*)((Task*)work->field_248)->extra)
                          ->field_8[D_actor_341900_80163A98[arg0->spawnArg1].field_6];
        coord->coord.t[0] = D_actor_341900_80163A98[arg0->spawnArg1].field_0;
        coord->coord.t[1] = D_actor_341900_80163A98[arg0->spawnArg1].field_2;
        coord->coord.t[2] = D_actor_341900_80163A98[arg0->spawnArg1].field_4;
        coord->flg        = 0;
        arg0->state++;
    }

    ((TmdObject*)arg0->extra)->field_C =
        ((TmdObject*)((Task*)work->field_248)->extra)->field_C;

    mdl    = (TmdObject*)arg0->extra;
    pos.vx = ((TmdObject*)arg0->extra)->field_8[1].workm.t[0];
    pos.vy = ((TmdObject*)arg0->extra)->field_8[1].workm.t[1];
    pos.vz = ((TmdObject*)arg0->extra)->field_8[1].workm.t[2];
    func_800D7A9C(mdl, &pos, 0, 3);
}

/// Per-state body of the actor task. State 0 publishes the part's draw
/// matrix, state 1 watches the work block's frame counter for the two frames
/// that respawn the actor's script, and every state but 0 then refreshes the
/// three child tasks and pushes the model's third coordinate, the actor's own
/// world position, through the draw matrix.
void func_actor_341900_80162708(Task* arg0)
{
    Actor341900TaskWork* work;
    TmdObject*           mdl;
    VECTOR               pos;
    s32                  frame;

    work = (Actor341900TaskWork*)arg0->idMap;
    switch (arg0->state) {
        case 0:
            func_actor_341900_80162330(arg0);
            ((TmdObject*)arg0->extra)->field_8->sub = &Gfx_ViewCoord;
            arg0->state++;
            return;
        case 1:
            if (work->field_254 == arg0->state) {
                frame = work->field_66 & 0x3FF;
                if ((frame == 0x12) && (work->field_230 != frame)) {
                    Task_Reparent(arg0,
                                  Gp_SpawnScript18((s32)&D_80144A74, (s32)&D_80144A7C));
                    func_80143490(3);
                }
                frame = work->field_66 & 0x3FF;
                if ((frame == 0x18) && (work->field_230 != frame)) {
                    Task_Reparent(arg0,
                                  Gp_SpawnScript18((s32)&D_80144A74, (s32)&D_80144A7C));
                    func_80143490(3);
                }
                work->field_230 = work->field_66 & 0x3FF;
            }
            work = (Actor341900TaskWork*)arg0->idMap;
            break;
    }

    work = (Actor341900TaskWork*)arg0->idMap;
    func_actor_341900_80161E58(arg0, 8);
    func_actor_341900_80161E58(work->field_24C, 4);
    func_actor_341900_80161E58(work->field_250, 4);

    mdl    = (TmdObject*)arg0->extra;
    pos.vx = ((TmdObject*)arg0->extra)->field_8[1].workm.t[0];
    pos.vy = ((TmdObject*)arg0->extra)->field_8[1].workm.t[1];
    pos.vz = ((TmdObject*)arg0->extra)->field_8[1].workm.t[2];
    func_800D7A9C(mdl, &pos, 0, 3);
}

INCLUDE_RODATA("actors/nonmatchings/actor_341900/actor_341900", D_actor_341900_80161E20);

INCLUDE_ASM("actors/nonmatchings/actor_341900/actor_341900", func_actor_341900_801628B8);

INCLUDE_ASM("actors/nonmatchings/actor_341900/actor_341900", func_actor_341900_80162AD4);

INCLUDE_ASM("actors/nonmatchings/actor_341900/actor_341900", func_actor_341900_80162EFC);

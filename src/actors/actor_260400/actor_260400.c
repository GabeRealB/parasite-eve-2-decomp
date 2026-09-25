#include "common.h"

#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>
#include <psyq/inline_c.h>
#include "gte.h"

#include "actors/actor_260400.h"

#include "gameplay/1BC.h"
#include "gameplay/268.h"
#include "gameplay/3A34.h"
#include "gameplay/3CD8.h"
#include "gameplay/D4.h"
#include "gameplay/areaplace.h"
#include "main/gameflag.h"
#include "main/gfx.h"
#include "main/mem.h"
#include "main/session.h"
#include "main/task.h"
#include "main/tmd.h"

extern s32      D_actor_260400_8014C6C0;
extern s32      D_actor_260400_8014C788;
extern s32      D_actor_260400_8014CF38;
extern s32      D_actor_260400_8014D118;
extern s32      D_actor_260400_8014D208;
extern s32      D_actor_260400_8014D340;
extern s32      D_actor_260400_8014D4A8;
extern s32      D_actor_260400_8014D610;
extern u8       D_actor_260400_80154BE8[];
extern TaskDesc D_actor_260400_80154C18[];
extern u8       D_actor_260400_80154C30[];

extern u8 D_80072729;

/// Steps the task's model `amount` units along its facing (the coordinate
/// matrix's z column, normalised and scaled on the GTE), using a scratch-pad
/// vector; skipped while `D_80072729` is 1.
static __inline__ void Actor260400_MoveForward(Task* task, s16 amount)
{
    GsCOORDINATE2* coord;
    SVECTOR*       head;
    SVECTOR*       vec;

    coord = ((TmdObject*)task->extra)->coords;
    if (D_80072729 != 1) {
        head                       = *(SVECTOR**)G_SCRATCH_HEAD;
        vec                        = head - 1;
        *(SVECTOR**)G_SCRATCH_HEAD = vec;
        Gfx_MatrixCol2(&coord->coord, vec);
        VectorNormalSS(vec, vec);
        gte_lddp(amount);
        gte_ldsv(vec);
        gte_gpf12();
        gte_stsv(vec);
        coord->coord.t[0]          += head[-1].vx;
        coord->coord.t[1]          += vec->vy;
        coord->coord.t[2]          += vec->vz;
        coord->flg                  = 0;
        *(SVECTOR**)G_SCRATCH_HEAD += 1;
    }
}

void func_actor_260400_80149E38(void)
{
    switch (GameFlag_GetNibble(0xE3)) {
        case 0:
            func_800E8634((s32)&D_actor_260400_8014C788, 0, (s32)&D_actor_260400_8014CF38);
            GameFlag_SetNibble(0xE3, 1);
            break;
        case 1:
            if ((Gp_GetCurBit2Flag(4) == 1) || (Gp_GetCurBit2Flag(5) == 1)) {
                func_800E8614((s32)&D_actor_260400_8014D118, 0);
            } else {
                func_800E8614((s32)&D_actor_260400_8014D208, 0);
                GameFlag_SetNibble(0xE3, 2);
            }
            break;
        case 2:
            func_800E8614((s32)&D_actor_260400_8014D340, 0);
            GameFlag_SetNibble(0xE3, 3);
            break;
        case 3:
            func_800E8614((s32)&D_actor_260400_8014D4A8, 0);
            GameFlag_SetNibble(0xE3, 4);
            break;
        case 4:
            func_800E8614((s32)&D_actor_260400_8014D610, 0);
            break;
    }
}

void func_actor_260400_80149F5C(s32 arg0)
{
    if (arg0 != 0) {
        Gp_CapFile = 0;
        Gp_LoadCapFile(1);
        func_800E6D4C(0x340, 0);
        return;
    }
    Gp_ResetCap();
}

void func_actor_260400_80149FA4(void)
{
    s32 slot;

    slot = Gp_LookupSlot4(0);
    if (slot != 0) {
        Gp_DispatchMsg((Task*)slot, 0x7D4, (s32)&D_actor_260400_8014C6C0, 0);
    }
}

/// Spawn routine (state 0 of `func_actor_260400_8014A550`): allocates the work
/// block and publishes it in `D_actor_260400_80154C70` and the task's `work`
/// slot, binds the model to the view and hands it the block's light and colour
/// matrices, publishes the task in `D_actor_260400_80154C74`, relights the
/// model from a point 0x320 above its translation and binds the animation
/// stream. It then starts the helper task and textures the helper's model from
/// the area placement record the spawning enemy names, before running the
/// first update with the reset mode 2 / id 1 it seeds.
void func_actor_260400_80149FE0(GpEnemy* enemy, Task* task)
{
    VECTOR         vec;
    GpAreaKey      key;
    GsCOORDINATE2* coord;
    TmdObject*     obj;
    TmdObject*     model;
    Task*          spawned;
    GpAreaPlace*   place;
    s32            idx;
    u32            raw;
    GpAreaKey*     sessionKey;
    GpAreaKey*     keyPtr;
    void*          work;

    obj                     = task->extra;
    coord                   = obj->coords;
    work                    = memCalloc(0x4F8, 0);
    D_actor_260400_80154C70 = work;
    task->work              = work;
    if (work == NULL) {
        Gp_DestroyEnemy(enemy, task);
        return;
    }
    task->exitCallback      = func_actor_260400_8014A630;
    coord->sub              = &gGfxViewCoord;
    enemy->field_4          = &coord->coord;
    enemy->field_48         = 0;
    enemy->node.targeted    = 0;
    enemy->node.flags       = 1;
    obj->otOffset           = 1;
    obj->flags              = 0;
    obj->lightMtx           = &D_actor_260400_80154C70->light;
    obj->colorMtx           = &D_actor_260400_80154C70->color;
    vec.vx                  = coord->workm.t[0];
    vec.vy                  = coord->workm.t[1] - 0x320;
    D_actor_260400_80154C74 = task;
    vec.vz                  = coord->workm.t[2];
    func_800D7A9C(obj, &vec, 0, 3);
    func_800B3F84(&D_actor_260400_80154C70->anim, D_actor_260400_80154C30, obj,
                  D_actor_260400_80154C70->poses, D_actor_260400_80154C70->slots);
    D_actor_260400_80154C70->field_4B8 = 1;
    D_actor_260400_80154C70->field_4B4 = 2;
    spawned                            = Task_SpawnFromTable(D_actor_260400_80154C18, 1, 8, 0);
    if (spawned != NULL) {
        D_actor_260400_80154C70->field_4F0 = spawned;
        sessionKey                         = (GpAreaKey*)&gGameSession->at4.loc;
        model                              = spawned->extra;
        raw                                = ((GpEnemy*)task->spawnArg2)->placeKey;
        key.stage                          = sessionKey->stage;
        key.area                           = sessionKey->area;
        key.room                           = sessionKey->room;
        keyPtr                             = &key;
        TOUCH_REG(keyPtr);
        key.view = sessionKey->view;
        idx      = raw >> 12;
        Gp_SyncAreaKeyIndex(keyPtr);
        place        = (GpAreaPlace*)((idx << 4) + (s32)Gp_GetNestedAreaRec(&key)->field_0);
        model->tpage = place->tpage;
        model->clut  = place->clut;
        if (model->buffer != NULL) {
            tmdProcessStream(model);
            tmdProcessStream(model);
        }
    }
    D_actor_260400_80154C70->field_4EA = 0;
    D_actor_260400_80154C70->field_4EC = 0;
    D_actor_260400_80154C70->field_4F4 = 0;
    task->msgTable                     = D_actor_260400_80154BE8;
    func_actor_260400_8014A200(task);
    task->state++;
}

/// Per-frame update: reset modes 1 and 2 run their one-shot reseed (blended or
/// plain) and switch to mode 3. In mode 3 the walking animations 2, 0xE and 0xF
/// step the model forward while `field_4EA` counts down, by a distance the
/// approach mode in `D_actor_260400_80154C78` picks, and blend into animation
/// 0xD with reset argument 10 when the walk ends; animation 3 turns the model
/// while `field_4EC` counts down. Mode 3 then ticks the animation.
void func_actor_260400_8014A200(Task* task)
{
    GsCOORDINATE2*   coord = ((TmdObject*)task->extra)->coords;
    Actor260400Work* work  = (Actor260400Work*)task->work;

    if (D_actor_260400_80154C70->field_4B4 == 1) {
        func_actor_260400_8014A888();
        D_actor_260400_80154C70->field_4B4 = 3;
    } else if (D_actor_260400_80154C70->field_4B4 == 2) {
        func_actor_260400_8014A7F8();
        D_actor_260400_80154C70->field_4B4 = 3;
    } else if (D_actor_260400_80154C70->field_4B4 == 3) {
        if (work->field_4B8 == 0xE || work->field_4B8 == 2 || work->field_4B8 == 0xF) {
            if (work->field_4EA != 0) {
                switch (D_actor_260400_80154C78) {
                    case 0:
                        Actor260400_MoveForward(task, 0x3C);
                        break;
                    case 1:
                        Actor260400_MoveForward(task, -0xF);
                        break;
                    case 2:
                        Actor260400_MoveForward(task, 0x19);
                        break;
                }
                if (--work->field_4EA == 0) {
                    work->field_4B4         = 1;
                    D_actor_260400_80154BE4 = 10;
                    work->field_4B8         = 0xD;
                }
            }
        }
        if (work->field_4B8 == 3 && work->field_4EC != 0) {
            work->yaw += 0x33;
            Gfx_RotMatrixY(&coord->coord, (s16)work->yaw, 1);
            coord->flg = 0;
            work->field_4EC--;
        }
        func_actor_260400_8014A7AC();
    }
}

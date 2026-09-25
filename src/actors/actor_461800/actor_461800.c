#include "common.h"

#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>
#include <psyq/inline_c.h>
#include "gte.h"

#include "actors/actor.h"
#include "actors/actors_shared_8013411c.h"
#include "gameplay/1BC.h"
#include "gameplay/268.h"
#include "gameplay/3A34.h"
#include "gameplay/3CD8.h"
#include "gameplay/3E9C.h"
#include "gameplay/D4.h"
#include "gameplay/gameplay.h"
#include "main/fs.h"
#include "main/gameflag.h"
#include "main/gfx.h"
#include "main/mc.h"
#include "main/mem.h"
#include "main/session.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/tmd.h"

extern Actor461800Work* D_actor_461800_80143894;

/// The task the first variant's work block above belongs to, published by
/// `func_actor_461800_80132390` alongside it.
extern Task* D_actor_461800_80143898;

extern Actor151000Work* D_actor_461800_801438A0;

/// The second variant's task, published by its spawn routine
/// `func_actor_461800_8013307C` so the handlers can reach its model.
extern Task* D_actor_461800_801438A4;

/* Scratchpad stack pointer, initialised by GameMain (see src/main/gamemain.c). */

/// `func_800B4114` is deliberately declared locally with a signed `arg2`; see
/// the note in `include/gameplay/1BC.h`.
void func_800B4114(GpAnimCtx* arg0, s32 arg1, s32 arg2, s32 arg3, s32 arg4);

extern Task* D_actor_461800_80133EB8;

extern Task*    D_actor_461800_80133EB4;
extern TaskDesc D_actor_461800_80133EBC;

extern u8       D_actor_461800_80139F5C[];
extern TaskDesc D_actor_461800_80139F8C;
extern u8       D_actor_461800_80139FB0[];

extern s16 D_80071076;
extern s8  D_8007218B;
extern s32 D_8017DC54;
extern s32 D_actor_461800_80143884;
extern s32 D_actor_461800_80143888;
extern s32 D_actor_461800_8014388C;
extern s32 D_actor_461800_80143890;

extern u8 D_actor_461800_801437BC[];
extern u8 D_actor_461800_801437F8[];

/// Reset argument the first variant forwards to every reseeded slot.
extern s16 D_actor_461800_80139F58;

/// Reset argument the second variant forwards to every reseeded slot.
extern s16 D_actor_461800_801437B8;

/// Approach mode the last `func_actor_461800_80132F44` call selected.
extern s16 D_actor_461800_8014389C;

/// Approach mode the last `func_actor_461800_80133A3C` call selected.
extern s16 D_actor_461800_801438A8;

void func_actor_461800_80132660(Task* task);
void func_actor_461800_80132A0C(GpEnemy* enemy, Task* task);
void func_actor_461800_80132A90(Task* task);
void func_actor_461800_80132AD8(Task* task);
void func_actor_461800_80132C28(void);
void func_actor_461800_80132C74(void);
void func_actor_461800_80132D04(void);
void func_actor_461800_801331E4(Task* task);
void func_actor_461800_801335B0(GpEnemy* enemy, Task* task);
void func_actor_461800_80133634(Task* task);
void func_actor_461800_8013365C(Task* task);
void func_actor_461800_80133724(void);
void func_actor_461800_80133770(void);
void func_actor_461800_8013380C(void);
void func_actor_461800_80133B98(Task* task);

void func_actor_461800_80131E38(Task* task)
{
    switch (task->state) {
        case 0:
            D_actor_461800_80143884 = 0x10;
            break;
        case 1:
            if (D_actor_461800_80143888 != 0) {
                if (D_actor_461800_80143884 < 0x10) {
                    D_actor_461800_80143884++;
                } else {
                    D_actor_461800_80143888 = 0;
                }
            } else if (D_actor_461800_80143884 > 0) {
                D_actor_461800_80143884--;
            } else {
                D_actor_461800_80143888 = 1;
            }
            if (D_actor_461800_8014388C != 0) {
                if (D_actor_461800_80143890 < 0x13) {
                    D_actor_461800_80143890++;
                } else {
                    D_actor_461800_8014388C = 0;
                }
            } else if (D_actor_461800_80143890 > 0) {
                D_actor_461800_80143890--;
            } else {
                D_actor_461800_8014388C = 1;
            }
            break;
        case 2:
            if (D_actor_461800_80143884 < 0x10) {
                D_actor_461800_80143884++;
            }
            if (D_actor_461800_80143890 < 0x13) {
                D_actor_461800_80143890++;
            }
            if (D_actor_461800_80143884 == 0x10 && D_actor_461800_80143890 == 0x13) {
                task->state = 3;
            }
            break;
        case 3:
            if (!(task->killCountdown & 3)) {
                if (D_actor_461800_80143884 > 0) {
                    D_actor_461800_80143884--;
                }
                if (D_actor_461800_80143890 > 0) {
                    D_actor_461800_80143890--;
                }
            }
            break;
        case 4:
            Display_ClampField126(0);
            D_8017DC54 = -1;
            return;
    }
    task->killCountdown++;
    D_8017DC54 = D_actor_461800_80143884 / 3 + 3;
}

/// Full-screen fade overlay: `Task::state` picks the ramp (0 snaps it to 90,
/// 1 clears it, 2 counts down, 3 counts up) held in `Task::killCountdown`, which
/// then scales a grey semi-transparent `TILE` linked with its `DR_TPAGE` into
/// OT slot 5.
void func_actor_461800_80132048(Task* task)
{
    TILE*     tile;
    DR_TPAGE* dr;
    u8        c;

    switch (task->state) {
        case 1:
            task->killCountdown = 0;
            break;
        case 3:
            if (task->killCountdown < 90) {
                task->killCountdown++;
            }
            break;
        case 2:
            if (task->killCountdown > 0) {
                task->killCountdown--;
            }
            break;
        case 0:
            task->killCountdown = 90;
            break;
    }
    tile           = (TILE*)gGpuPrimCursor;
    gGpuPrimCursor = tile + 1;
    c              = (task->killCountdown * 0xFF) / 90;
    setlen(tile, 3);
    setcode(tile, 0x62);
    tile->x0 = -0xA0;
    tile->y0 = -0x80;
    tile->w  = 0x140;
    tile->h  = 0x100;
    tile->r0 = c;
    tile->g0 = c;
    tile->b0 = c;
    addPrim(gGpuCurrentOt + 5, tile);

    dr             = gGpuPrimCursor;
    gGpuPrimCursor = dr + 1;
    setlen(dr, 1);
    dr->code[0] = 0xE1000240;
    addPrim(gGpuCurrentOt + 5, dr);
}

void func_actor_461800_801321DC(s32 arg0)
{
    if (arg0 < 0) {
        if (D_actor_461800_80133EB4 == NULL) {
            D_actor_461800_80133EB4 = Task_SpawnFromTable(&D_actor_461800_80133EBC, 0, 0, 0);
        }
    } else {
        D_actor_461800_80133EB4->state = arg0;
    }
}

void func_actor_461800_8013223C(s32 arg0)
{
    if (arg0 < 0) {
        if (D_actor_461800_80133EB8 == NULL) {
            D_actor_461800_80133EB8 = Task_SpawnFromTable(&D_actor_461800_80133EBC, 1, 0, 0);
        }
    } else {
        D_actor_461800_80133EB8->state = arg0;
    }
}

/// Exit path taken when the player leaves through this actor: two flag awards
/// first, then one of two endings depending on whether the two event flags have
/// been seen. With neither seen the session bails out (`field_128` / `field_12E`
/// are the stage-load sentinels); otherwise the save header is primed and the
/// boot loader started, with the stream RNG restored behind it. Skipped whole
/// when `D_8007218B` (the current screen id) is 9.
void func_actor_461800_8013229C(void)
{
    if (D_8007218B != 9) {
        if (GameFlag_GetNibble(0xEA) == 2) {
            Gp_SetCollectedBit(0x130);
        }
        if (GameFlag_GetNibble(0x113) != 0) {
            Gp_SetCollectedBit(0x12F);
        }
        if (GameFlag_GetNibble(0x112) == 0 && GameFlag_GetNibble(0x113) == 0) {
            gGameSession->restartMode = 0xFF;
            gGameSession->field_12E   = 0xF;
            return;
        }
        Mc_SaveData.at4.loc.stage = 4;
        Mc_SaveData.at4.loc.area  = 0x24;
        Mc_SaveData.at4.loc.warp  = 1;
        Mc_SaveData.at4.loc.room  = 1;
        D_80071076                = 1;
        Task_Spawn(0, 0x11, 0, 0);
        Fs_BeginBootLoad(&Mc_SaveData.at4.loc.view, 0);
        Gp_RestoreStreamRng();
    }
}

/// Spawn tick of the first actor variant: allocates the work block, hangs the
/// model off the view, seeds the animation context and starts the two helper
/// tasks. Each helper takes its texture page and CLUT row from the nested area
/// record the actor's spawn index selects, and is streamed twice once its aux
/// buffer exists.
void func_actor_461800_80132390(GpEnemy* enemy, Task* task)
{
    VECTOR         vec;
    GpAreaKey      key;
    GpAreaKey*     keyp;
    GsCOORDINATE2* coord;
    TmdObject*     obj;
    u8             areaByte0;
    u8             areaByte1;
    u32            raw1, index1;
    Task*          spawned1;
    TmdObject*     model1;
    GpAreaPlace*   entry1;
    GpAreaKey*     sessionKey1;
    u32            raw2, index2;
    Task*          spawned2;
    TmdObject*     model2;
    GpAreaPlace*   entry2;
    GpAreaKey*     sessionKey2;

    obj        = task->extra.tmd;
    coord      = obj->coords;
    task->work = (TaskIdMap*)(D_actor_461800_80143894 = memCalloc(0x4F8, false));
    if (D_actor_461800_80143894 == NULL) {
        Gp_DestroyEnemy(enemy, task);
        return;
    }
    task->exitCallback           = func_actor_461800_80132A90;
    coord->sub                   = &gGfxViewCoord;
    enemy->field_4               = &coord->coord;
    enemy->field_48              = 0;
    enemy->node.state.b.targeted = 0;
    enemy->node.state.b.flags    = 1;
    obj->otOffset                = 1;
    obj->flags                   = 0;
    obj->lightMtx                = &D_actor_461800_80143894->light;
    obj->colorMtx                = &D_actor_461800_80143894->color;
    vec.vx                       = coord->workm.t[0];
    vec.vy                       = coord->workm.t[1] - 0x320;
    D_actor_461800_80143898      = task;
    vec.vz                       = coord->workm.t[2];
    func_800D7A9C(obj, &vec, 0, 3);
    func_800B3F84(&D_actor_461800_80143894->rig.anim, D_actor_461800_80139FB0, obj,
                  D_actor_461800_80143894->rig.poses, D_actor_461800_80143894->rig.slots);
    D_actor_461800_80143894->st.animId = 1;
    D_actor_461800_80143894->st.state  = 2;

    spawned1 = Task_SpawnFromTable(&D_actor_461800_80139F8C, 1, 8, 0);
    if (spawned1 != NULL) {
        D_actor_461800_80143894->helper1 = spawned1;
        sessionKey1                      = (GpAreaKey*)&gGameSession->at4.loc;
        raw1                             = ((GpEnemy*)task->spawnArg2)->placeKey;
        model1                           = spawned1->extra.tmd;
        key.stage                        = sessionKey1->stage;
        key.area                         = sessionKey1->area;
        areaByte1                        = sessionKey1->room;
        SOFT_BARRIER();
        keyp = &key;
        TOUCH_REG(keyp);
        key.room  = areaByte1;
        areaByte0 = gGameSession->at4.loc.view;
        index1    = raw1 >> 12;
        key.view  = areaByte0;
        Gp_SyncAreaKeyIndex(keyp);
        entry1        = (GpAreaPlace*)((index1 * 0x10) + (s32)Gp_GetNestedAreaRec(&key)->field_0);
        model1->tpage = entry1->tpage;
        model1->clut  = entry1->clut;
        if (model1->buffer != NULL) {
            tmdProcessStream(model1);
            tmdProcessStream(model1);
        }
    }

    spawned2 = Task_SpawnFromTable(&D_actor_461800_80139F8C, 2, 0xC, 0);
    if (spawned2 != NULL) {
        D_actor_461800_80143894->helper2 = spawned2;
        sessionKey2                      = (GpAreaKey*)&gGameSession->at4.loc;
        raw2                             = ((GpEnemy*)task->spawnArg2)->placeKey;
        model2                           = spawned2->extra.tmd;
        key.stage                        = sessionKey2->stage;
        key.area                         = sessionKey2->area;
        areaByte1                        = sessionKey2->room;
        SOFT_BARRIER();
        keyp = &key;
        TOUCH_REG(keyp);
        key.room  = areaByte1;
        areaByte0 = gGameSession->at4.loc.view;
        index2    = raw2 >> 12;
        key.view  = areaByte0;
        Gp_SyncAreaKeyIndex(keyp);
        entry2        = (GpAreaPlace*)((index2 * 0x10) + (s32)Gp_GetNestedAreaRec(&key)->field_0);
        model2->tpage = entry2->tpage;
        model2->clut  = entry2->clut;
        if (model2->buffer != NULL) {
            tmdProcessStream(model2);
            tmdProcessStream(model2);
        }
    }

    D_actor_461800_80143894->st.travel  = 0;
    D_actor_461800_80143894->turnFrames = 0;
    task->msgTable                      = D_actor_461800_80139F5C;
    func_actor_461800_80132660(task);
    task->state++;
}

/// Per-frame update of the first variant: modes 1 and 2 run their one-shot
/// setup and switch to mode 3; mode 3 walks the model while `st.travel` counts
/// down (distance picked by `D_actor_461800_8014389C`), turns it while
/// `turnFrames` counts down in animation 3, then ticks the animation.
void func_actor_461800_80132660(Task* task)
{
    GsCOORDINATE2*   coord = task->extra.tmd->coords;
    Actor461800Work* work  = (Actor461800Work*)task->work;

    if (D_actor_461800_80143894->st.state == 1) {
        func_actor_461800_80132D04();
        D_actor_461800_80143894->st.state = 3;
    } else if (D_actor_461800_80143894->st.state == 2) {
        func_actor_461800_80132C74();
        D_actor_461800_80143894->st.state = 3;
    } else if (D_actor_461800_80143894->st.state == 3) {
        if (work->st.animId == 0xE || work->st.animId == 2 || work->st.animId == 0xF) {
            if (work->st.travel != 0) {
                switch (D_actor_461800_8014389C) {
                    case 0:
                        actorMoveModelForward(task, 0x3C);
                        break;
                    case 1:
                        actorMoveModelForward(task, -0xF);
                        break;
                    case 2:
                        actorMoveModelForward(task, 0x19);
                        break;
                }
                if (--work->st.travel == 0) {
                    work->st.state          = 1;
                    D_actor_461800_80139F58 = 10;
                    work->st.animId         = 0xD;
                }
            }
        }
        if (work->st.animId == 3 && work->turnFrames != 0) {
            work->st.yaw += 0x33;
            Gfx_RotMatrixY(&coord->coord, work->st.yaw, 1);
            coord->flg = 0;
            work->turnFrames--;
        }
        func_actor_461800_80132C28();
    }
}

/// Two-state dispatcher: publishes the task's work block in
/// `D_actor_461800_80143894` on the way through, then calls the handler its
/// state selects.
void func_actor_461800_801329B0(Task* task)
{
    void (*fns[2])(GpEnemy*, Task*) = {
        func_actor_461800_80132390,
        func_actor_461800_80132A0C,
    };

    D_actor_461800_80143894 = (Actor461800Work*)task->work;
    fns[task->state](task->spawnArg2, task);
}

/// Second state of the first variant's task: refreshes the model root's world
/// matrix, relights the model from a point 0x320 above its translation, then
/// runs the per-frame update and draws the ground shadow.
void func_actor_461800_80132A0C(GpEnemy* enemy, Task* task)
{
    TmdObject*     obj;
    GsCOORDINATE2* coord;
    VECTOR         vec;

    obj   = task->extra.tmd;
    coord = obj->coords;
    Gp_UpdateCoord(coord);
    vec.vx = coord->workm.t[0];
    vec.vy = coord->workm.t[1] - 0x320;
    vec.vz = coord->workm.t[2];
    func_800D7A9C(obj, &vec, 0, 3);
    func_actor_461800_80132660(task);
    func_actor_461800_80132AD8(task);
}

/// `Task::exitCallback` of the first variant: hands the task's `GpEnemy`
/// (parked in `Task::spawnArg2` by the spawn descriptor) back to
/// `Gp_DestroyEnemy`, then kills the two helper tasks the spawn routine
/// started.
void func_actor_461800_80132A90(Task* task)
{
    Actor461800Work* work = (Actor461800Work*)task->work;

    Gp_DestroyEnemy(task->spawnArg2, task);
    taskKill(work->helper1);
    taskKill(work->helper2);
}

/// Draws the ground shadow quad under the model root, unless the model is
/// hidden (`flags & 0x80`) or has no buffer yet. The root's world translation
/// is staged in a scratchpad `VECTOR3`, and the quad's brightness follows the
/// room's current ground shade.
void func_actor_461800_80132AD8(Task* task)
{
    TmdObject*     obj;
    GsCOORDINATE2* coord;
    VECTOR3*       vec;

    obj   = task->extra.tmd;
    coord = obj->coords;
    if (!(obj->flags & 0x80) && obj->buffer != NULL) {
        vec     = (VECTOR3*)SCRATCH_PUSH_BYTES(0x18);
        vec->vx = coord->workm.t[0];
        vec->vy = coord->workm.t[1];
        vec->vz = coord->workm.t[2];
        Gp_DrawEffGroundQuad(vec, 0x200, Gp_State1C->groundShade);
        SCRATCH_POP_BYTES(0x18);
    }
}

/// State handler of the actor's model task: the spawn tick hangs the task's own
/// coordinate frame off the actor's part `spawnArg1` and steps to state 1, and
/// every later tick hands that part's world translation, dropped by 0x320 in y,
/// to `func_800D7A9C` for the part colour matrix.
void func_actor_461800_80132B74(Task* task)
{
    TmdObject*     extra = task->extra.tmd;
    GsCOORDINATE2* coord = extra->coords;
    GsCOORDINATE2* parts = D_actor_461800_80143898->extra.tmd->coords;
    GsCOORDINATE2* part  = parts + task->spawnArg1;
    VECTOR         vec;

    switch (task->state) {
        case 0:
            coord->flg   = 0;
            extra->flags = 0;
            coord->sub   = part;
            task->state++;
            break;
        case 1:
            vec.vx = parts->workm.t[0];
            vec.vy = parts->workm.t[1] - 0x320;
            vec.vz = parts->workm.t[2];
            func_800D7A9C(extra, &vec, 0, 3);
            break;
    }
}

/// Ticks animation slots 1..0x13 of the actor's animation context.
void func_actor_461800_80132C28(void)
{
    s32 i;

    i = 1;
    do {
        Gp_AnimTickIndex(&D_actor_461800_80143894->rig.anim, i);
        i++;
    } while (i < 0x14);
}

/// Marks animation slots 1..0x13 reset-pending and reseeds each of them from
/// the current animation id, then records that id as the one now playing.
void func_actor_461800_80132C74(void)
{
    s32 i;

    i = 1;
    do {
        D_actor_461800_80143894->rig.slots[i].rate = 1;
        Gp_AnimResetSlot(&D_actor_461800_80143894->rig.anim, i, D_actor_461800_80143894->st.animId);
        i++;
    } while (i < 0x14);
    D_actor_461800_80143894->st.appliedAnimId = D_actor_461800_80143894->st.animId;
}

/// Reseeds animation slots 1..0x13 from the current animation id and records
/// that id as the one now playing.
void func_actor_461800_80132D04(void)
{
    s32 i;

    i = 1;
    do {
        func_800B4114(&D_actor_461800_80143894->rig.anim, i, D_actor_461800_80143894->st.animId, 0,
                      D_actor_461800_80139F58);
        i++;
    } while (i < 0x14);
    D_actor_461800_80143894->st.appliedAnimId = D_actor_461800_80143894->st.animId;
}

/// Applies an animation preset: the id is copied into the work block, the reset
/// mode is picked by the preset's blend flag and the reset argument is either
/// taken from the preset or left at 2, then the whole slot array is re-seeded.
/// Only the six known animation ids are accepted; anything else leaves the work
/// block untouched and reports the failure.
s32 func_actor_461800_80132D84(Task* task, s32 arg1, GpAnimArg* preset, s32 arg3)
{
    if (preset->field_4 < 6) {
        D_actor_461800_80143894->st.animId = preset->field_4;
        if (preset->field_8 != 0) {
            D_actor_461800_80143894->st.state = 1;
            D_actor_461800_80139F58           = preset->field_C;
        } else {
            D_actor_461800_80143894->st.state = 2;
        }
        D_actor_461800_80143894->st.field_6 = 0;
        func_actor_461800_80132660(D_actor_461800_80143898);
        return 0;
    }
    return -1;
}

/// Applies a `Tmd_Create` flag word to the three model objects this actor owns:
/// the one on its own task and the two helper tasks' models in the work block.
/// `arg2 & 1` picks the base value -- 0x80 normally, 0 when set -- and
/// `arg2 & 2` ORs bit 0x4 in on top of it.
s32 func_actor_461800_80132E14(Task* arg0, s32 arg1, s32 arg2)
{
    TmdObject* own    = D_actor_461800_80143898->extra.tmd;
    TmdObject* first  = D_actor_461800_80143894->helper1->extra.tmd;
    TmdObject* second = D_actor_461800_80143894->helper2->extra.tmd;

    if (arg2 & 1) {
        own->flags    = 0;
        first->flags  = 0;
        second->flags = 0;
    } else {
        own->flags    = 0x80;
        first->flags  = 0x80;
        second->flags = 0x80;
    }
    if (arg2 & 2) {
        own->flags    |= 4;
        first->flags  |= 4;
        second->flags |= 4;
    }
    return 0;
}

/// Seeds the task's `TmdObject` coordinate frame from `placement`: only the yaw
/// is used, remembered in the work block and applied with `Gfx_RotMatrixY`,
/// then the three longs become the coordinate's translation.
s32 func_actor_461800_80132EA4(Task* task, s32 arg1, GpXformArg* placement)
{
    GsCOORDINATE2* coord;
    u16            yaw;

    coord                           = task->extra.tmd->coords;
    D_actor_461800_80143894->st.yaw = yaw = placement->rot.vy;
    Gfx_RotMatrixY(&coord->coord, (s16)yaw, 1);
    coord->coord.t[0] = placement->pos.vx;
    coord->coord.t[1] = placement->pos.vy;
    coord->coord.t[2] = placement->pos.vz;
    coord->flg        = 0;
    return 0;
}

s32 func_actor_461800_80132F20(Task* arg0, s32 arg1, GpCmdArg* arg2, s32 arg3)
{
    if (arg2->command == 0) {
        D_actor_461800_80143894->turnFrames = 0x14;
    }
    return 0;
}

/// Turns the model to face `target` -- away from it in mode 1 -- and stores the
/// per-step distance: the planar distance over 60 steps in mode 0, 15 in
/// mode 1 and 25 otherwise.
s32 func_actor_461800_80132F44(Task* task, s32 arg1, VECTOR* target, s32 mode)
{
    GsCOORDINATE2*   coord;
    Actor461800Work* work;
    s32              dx;
    s32              dz;
    s32              steps;
    s32              dist;
    s32              angle;

    coord                   = task->extra.tmd->coords;
    work                    = (Actor461800Work*)task->work;
    D_actor_461800_8014389C = mode;
    dx                      = target->vx - coord->coord.t[0];
    dz                      = target->vz - coord->coord.t[2];
    angle                   = ratan2(dx, dz);
    work->st.yaw            = angle;
    if (D_actor_461800_8014389C == 1) {
        work->st.yaw = angle + 0x800;
    }
    Gfx_RotMatrixY(&coord->coord, work->st.yaw, 1);
    dist  = SquareRoot0(dx * dx + dz * dz);
    steps = 0x19;
    switch (D_actor_461800_8014389C) {
        case 0:
            steps = 0x3C;
            break;
        case 1:
            steps = 0xF;
            break;
        case 2:
            break;
    }
    work->st.travel = dist / steps;
    return 0;
}

void func_actor_461800_8013307C(GpEnemy* enemy, Task* task)
{
    VECTOR         vec;
    GsCOORDINATE2* coord;
    TmdObject*     obj;

    obj        = task->extra.tmd;
    coord      = obj->coords;
    task->work = (TaskIdMap*)(D_actor_461800_801438A0 = memCalloc(0x4C0, false));
    if (D_actor_461800_801438A0 == NULL) {
        Gp_DestroyEnemy(enemy, task);
        return;
    }
    task->exitCallback           = func_actor_461800_80133634;
    coord->sub                   = &gGfxViewCoord;
    enemy->field_4               = &coord->coord;
    enemy->field_48              = 0;
    enemy->node.state.b.targeted = 0;
    enemy->node.state.b.flags    = 1;
    obj->otOffset                = 1;
    obj->lightMtx                = &D_actor_461800_801438A0->light;
    obj->colorMtx                = &D_actor_461800_801438A0->color;
    vec.vx                       = coord->workm.t[0];
    vec.vy                       = coord->workm.t[1] - 0x320;
    D_actor_461800_801438A4      = task;
    vec.vz                       = coord->workm.t[2];
    func_800D7A9C(obj, &vec, 0, 3);
    func_800B3F84(&D_actor_461800_801438A0->rig.anim, D_actor_461800_801437F8, obj,
                  D_actor_461800_801438A0->rig.poses, D_actor_461800_801438A0->rig.slots);
    D_actor_461800_801438A0->st.animId  = 1;
    D_actor_461800_801438A0->st.state   = 2;
    D_actor_461800_801438A0->st.travel  = 0;
    D_actor_461800_801438A0->turnFrames = 0;
    D_actor_461800_801438A0->stepRec    = 0;
    D_actor_461800_801438A0->footsteps  = 0;
    task->msgTable                      = D_actor_461800_801437BC;
    func_actor_461800_801331E4(task);
    task->state++;
}

/// Per-frame update of the second variant: modes 1 and 2 run their one-shot
/// setup and switch to mode 3 for the next frame; mode 3 walks the model while `st.travel` counts
/// down (distance picked by `D_actor_461800_801438A8`), turns it while
/// `turnFrames` counts down in animation 3, then ticks the animation.
void func_actor_461800_801331E4(Task* task)
{
    GsCOORDINATE2*   coord = task->extra.tmd->coords;
    Actor151000Work* work  = (Actor151000Work*)task->work;

    if (D_actor_461800_801438A0->st.state == 1) {
        func_actor_461800_8013380C();
        D_actor_461800_801438A0->st.state = 3;
    } else if (D_actor_461800_801438A0->st.state == 2) {
        func_actor_461800_80133770();
        D_actor_461800_801438A0->st.state = 3;
    } else if (D_actor_461800_801438A0->st.state == 3) {
        if (work->st.animId == 0xE || work->st.animId == 2 || work->st.animId == 0xF) {
            if (work->st.travel != 0) {
                switch (D_actor_461800_801438A8) {
                    case 0:
                        actorMoveModelForward(task, 0x3C);
                        break;
                    case 1:
                        actorMoveModelForward(task, -0xF);
                        break;
                    case 2:
                        actorMoveModelForward(task, 0x19);
                        break;
                }
                if (--work->st.travel == 0) {
                    work->st.state          = 1;
                    D_actor_461800_801437B8 = 10;
                    work->st.animId         = 0xD;
                }
            }
        }
        if (work->st.animId == 3 && work->turnFrames != 0) {
            work->st.yaw += 0x33;
            Gfx_RotMatrixY(&coord->coord, work->st.yaw, 1);
            coord->flg = 0;
            work->turnFrames--;
        }
        func_actor_461800_80133724();
        if (work->footsteps != 0) {
            func_actor_461800_8013365C(task);
        }
    }
}

/// Two-state dispatcher whose handler table is built on the stack, publishing
/// the task's work block in `D_actor_461800_801438A0` on the way through so the
/// rest of the overlay can reach it without the task.
void func_actor_461800_80133554(Task* task)
{
    void (*fns[2])(GpEnemy*, Task*) = {
        func_actor_461800_8013307C,
        func_actor_461800_801335B0,
    };

    D_actor_461800_801438A0 = (Actor151000Work*)task->work;
    fns[task->state](task->spawnArg2, task);
}

/// Second state of the second variant's task: refreshes the model root's world
/// matrix, relights the model from a point 0x320 above its translation, then
/// runs the per-frame update and draws the ground shadow.
void func_actor_461800_801335B0(GpEnemy* enemy, Task* task)
{
    TmdObject*     obj;
    GsCOORDINATE2* coord;
    VECTOR         vec;

    obj   = task->extra.tmd;
    coord = obj->coords;
    Gp_UpdateCoord(coord);
    vec.vx = coord->workm.t[0];
    vec.vy = coord->workm.t[1] - 0x320;
    vec.vz = coord->workm.t[2];
    func_800D7A9C(obj, &vec, 0, 3);
    func_actor_461800_801331E4(task);
    func_actor_461800_80133B98(task);
}

/// `Task::exitCallback` of the second variant: hands the task's `GpEnemy`
/// (parked in `Task::spawnArg2` by the spawn descriptor) back to
/// `Gp_DestroyEnemy`.
void func_actor_461800_80133634(Task* task)
{
    Gp_DestroyEnemy(task->spawnArg2, task);
}

/// Footstep sounds of the second variant: whenever animation slot 1 rolls onto
/// a new record whose flags nibble is 0x10 or 0x20, plays the matching step
/// sound, panned and attenuated from the model's second coordinate. The record
/// is latched in `stepRec` so each one fires once.
void func_actor_461800_8013365C(Task* task)
{
    Actor151000Work* work;
    GsCOORDINATE2*   obj;
    GpAnimRec*       rec;
    s32              kind;
    s32              id;
    s32              pan;

    work = (Actor151000Work*)task->work;
    obj  = task->extra.tmd->coords + 1;
    rec  = Gp_AnimGetRec(&work->rig.anim, &work->rig.slots[1]);
    if (rec == NULL || rec == work->stepRec) {
        return;
    }
    work->stepRec = rec;
    kind          = rec->flags & 0x30;
    if (kind != 0x10 && kind != 0x20) {
        return;
    }
    id = 0x1000000F;
    if (kind == 0x10) {
        id = 0x10000010;
    }
    id += 0x64;
    pan = (s8)Gp_GetObjPan(obj);
    SndEvt_EnqueueType6(id, pan, (s8)gpGetObjDepth(obj));
}

/// Ticks animation slots 1..0x12 of the second variant's animation context.
void func_actor_461800_80133724(void)
{
    s32 i;

    i = 1;
    do {
        Gp_AnimTickIndex(&D_actor_461800_801438A0->rig.anim, i);
        i++;
    } while (i < 0x13);
}

/// Restarts animation slots 1..0x12 from `st.animId`, flagging each slot's
/// `field_9` before the reset so it replays from the top, and latches that id
/// into `st.appliedAnimId` as the one now playing.
void func_actor_461800_80133770(void)
{
    s32 i;

    D_actor_461800_801438A0->stepRec = 0;
    i                                = 1;
    do {
        D_actor_461800_801438A0->rig.slots[i].rate = 1;
        Gp_AnimResetSlot(&D_actor_461800_801438A0->rig.anim, i, D_actor_461800_801438A0->st.animId);
        i++;
    } while (i < 0x13);
    D_actor_461800_801438A0->st.appliedAnimId = D_actor_461800_801438A0->st.animId;
}

/// Reseeds animation slots 1..0x12 from `st.animId` and latches that id into
/// `st.appliedAnimId` as the one now playing.
void func_actor_461800_8013380C(void)
{
    s32 i;

    D_actor_461800_801438A0->stepRec = 0;
    i                                = 1;
    do {
        func_800B4114(&D_actor_461800_801438A0->rig.anim, i, D_actor_461800_801438A0->st.animId, 0,
                      D_actor_461800_801437B8);
        i++;
    } while (i < 0x13);
    D_actor_461800_801438A0->st.appliedAnimId = D_actor_461800_801438A0->st.animId;
}

/// Applies an animation preset to the second variant's work block: the id is
/// copied in, the reset mode is picked by the preset's blend flag and the reset
/// argument is either taken from the preset or left at 2, then the work block's
/// animation is restarted through `func_actor_461800_801331E4`. Only the ids
/// this variant owns are accepted; anything else leaves the work block
/// untouched and reports the failure.
s32 func_actor_461800_80133898(Task* task, s32 arg1, GpAnimArg* preset, s32 arg3)
{
    if (preset->field_4 < 0x23) {
        D_actor_461800_801438A0->st.animId = preset->field_4;
        if (preset->field_8 != 0) {
            D_actor_461800_801438A0->st.state = 1;
            D_actor_461800_801437B8           = preset->field_C;
        } else {
            D_actor_461800_801438A0->st.state = 2;
        }
        D_actor_461800_801438A0->st.field_6 = 0;
        func_actor_461800_801331E4(D_actor_461800_801438A4);
        return 0;
    }
    return -1;
}

/// Visibility message of the second variant: applies `arg2` to the model of
/// the task published in `D_actor_461800_801438A4` - bit 0 selects
/// `TmdObject.flags` 0 (shown) vs 0x80 (hidden), bit 1 ORs in 0x4.
s32 func_actor_461800_80133928(Task* task, s32 arg1, s32 arg2)
{
    TmdObject* obj;

    obj = D_actor_461800_801438A4->extra.tmd;
    if (arg2 & 1) {
        obj->flags = 0;
    } else {
        obj->flags = 0x80;
    }
    if (arg2 & 2) {
        obj->flags |= 4;
    }
    return 0;
}

/// Seeds the task's `TmdObject` coordinate frame from `placement`: only the yaw
/// is used, remembered in the work block and applied with `Gfx_RotMatrixY`,
/// then the three longs become the coordinate's translation.
s32 func_actor_461800_80133970(Task* task, s32 arg1, GpXformArg* placement)
{
    GsCOORDINATE2* coord;
    u16            yaw;

    coord                           = task->extra.tmd->coords;
    D_actor_461800_801438A0->st.yaw = yaw = placement->rot.vy;
    Gfx_RotMatrixY(&coord->coord, (s16)yaw, 1);
    coord->coord.t[0] = placement->pos.vx;
    coord->coord.t[1] = placement->pos.vy;
    coord->coord.t[2] = placement->pos.vz;
    coord->flg        = 0;
    return 0;
}

/// Message handler: the message id selects how the second work block is
/// reseeded -- 0 arms the reset argument, 1 remembers the id in the byte the
/// seeding loop reads. Anything else does nothing.
s32 func_actor_461800_801339EC(Task* task, s32 arg1, GpCmdArg* msg, s32 arg3)
{
    s32 id;

    id = msg->command;
    switch (id) {
        case 0:
            D_actor_461800_801438A0->turnFrames = 0x14;
            break;
        case 1:
            D_actor_461800_801438A0->footsteps = id;
            break;
    }
    return 0;
}

/// Turns the model to face `target` -- away from it in mode 1 -- and stores the
/// per-step distance in the second work block: the planar distance over 60
/// steps in mode 0, 15 in mode 1 and 25 in mode 2.
s32 func_actor_461800_80133A3C(Task* task, s32 arg1, VECTOR* target, s32 mode)
{
    GsCOORDINATE2*   coord;
    Actor151000Work* work;
    s32              dx;
    s32              dz;
    s32              steps;
    s32              dist;
    s32              angle;

    coord                   = task->extra.tmd->coords;
    work                    = (Actor151000Work*)task->work;
    D_actor_461800_801438A8 = mode;
    dx                      = target->vx - coord->coord.t[0];
    dz                      = target->vz - coord->coord.t[2];
    angle                   = ratan2(dx, dz);
    work->st.yaw            = angle;
    if (D_actor_461800_801438A8 == 1) {
        work->st.yaw = angle + 0x800;
    }
    Gfx_RotMatrixY(&coord->coord, work->st.yaw, 1);
    dist = SquareRoot0(dx * dx + dz * dz);
    switch (D_actor_461800_801438A8) {
        case 0:
            steps = 0x3C;
            break;
        case 1:
            steps = 0xF;
            break;
        case 2:
            steps = 0x19;
            break;
    }
    work->st.travel = dist / steps;
    return 0;
}

/// Draws the ground shadow quad under the model root, unless the model is
/// hidden (`flags & 0x80`) or has no buffer yet. The root's world translation
/// is staged in a scratchpad `VECTOR3`, and the quad's brightness follows the
/// room's current ground shade.
void func_actor_461800_80133B98(Task* task)
{
    TmdObject*     obj;
    GsCOORDINATE2* coord;
    VECTOR3*       vec;

    obj   = task->extra.tmd;
    coord = obj->coords;
    if (!(obj->flags & 0x80) && obj->buffer != NULL) {
        vec     = (VECTOR3*)SCRATCH_PUSH_BYTES(0x18);
        vec->vx = coord->workm.t[0];
        vec->vy = coord->workm.t[1];
        vec->vz = coord->workm.t[2];
        Gp_DrawEffGroundQuad(vec, 0x200, Gp_State1C->groundShade);
        SCRATCH_POP_BYTES(0x18);
    }
}

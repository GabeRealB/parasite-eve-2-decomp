#include "common.h"

#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>
#include <psyq/inline_c.h>
#include "gte.h"

#include "actors/actor_461800_move.h"
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

/// Per-actor work block for the `actor_461800` overlay.
///
/// The spawn routine `func_actor_461800_80132390` allocates it with
/// `memCalloc(0x4F8, 0)` and stores the pointer both here (in
/// `D_actor_461800_80143894`) and in the task's `Task::work` slot (0x1C),
/// which is not a `TaskIdMap` in this overlay -- so the size below is the
/// allocation and not a guess. Every other function in the overlay reaches the
/// block through the global.
///
/// `anim` is the animation context `func_800B4114` walks, `field_4B8` the
/// animation id the slots are seeded with, `field_4B6` the copy of it kept for
/// change detection and `field_4EC` the reset argument handed to
/// `func_800B4114`. The tail holds the two helper tasks the exit callback
/// `func_actor_461800_80132A90` kills on teardown.
typedef struct Actor461800Work {
    /* 0x000 */ MATRIX     light; // model light matrix (`TmdObject::lightMtx`)
    /* 0x020 */ MATRIX     color; // model colour matrix (`TmdObject::colorMtx`)
    /* 0x040 */ GpAnimCtx  anim;
    /* 0x054 */ GpAnimSlot slots[0x14];
    /* 0x374 */ byte       pad_374[0x140];
    /* 0x4B4 */ s16        field_4B4; // reset mode `func_actor_461800_80132D84` selects (1 or 2)
    /* 0x4B6 */ s16        field_4B6; // copy of `field_4B8`, kept for change detection
    /* 0x4B8 */ s16        field_4B8; // animation id the slots are seeded with
    /* 0x4BA */ s16        field_4BA; // cleared by `func_actor_461800_80132D84` before the reseed
    /* 0x4BC */ byte       pad_4BC[0x2A];
    /* 0x4E6 */ s16        field_4E6; // yaw seeding the root coordinate
    /* 0x4E8 */ byte       pad_4E8[0x2];
    /* 0x4EA */ s16        field_4EA; // distance to the target over the step count
    /* 0x4EC */ s16        field_4EC; // reset argument handed to `func_800B4114`
    /* 0x4EE */ byte       pad_4EE[0x2];
    /* 0x4F0 */ Task*      field_4F0; // first helper task the spawn starts
    /* 0x4F4 */ Task*      field_4F4; // second helper task
} Actor461800Work;
STATIC_ASSERT_SIZEOF(Actor461800Work, 0x4F8);

extern Actor461800Work* D_actor_461800_80143894;

/// The task the first variant's work block above belongs to, published by
/// `func_actor_461800_80132390` alongside it.
extern Task* D_actor_461800_80143898;

/// Work block of the overlay's second actor variant. `func_actor_461800_8013307C`
/// allocates it with `memCalloc(0x4C0, 0)` and stores the pointer in
/// `D_actor_461800_801438A0` and in the task's `Task::work` slot, the same pair
/// of places the first variant's block goes -- so the size below is the
/// allocation, not a guess. The animation context sits at the same 0x40, but
/// this variant keeps its change-detection pair at 0x47E / 0x480 where
/// `Actor461800Work` has 0x4B6 / 0x4B8.
typedef struct Actor461800Work2 {
    /* 0x000 */ MATRIX     light;
    /* 0x020 */ MATRIX     color;
    /* 0x040 */ GpAnimCtx  anim;
    /* 0x054 */ GpAnimSlot slots[0x14];
    /* 0x374 */ byte       pad_374[0x108];
    /* 0x47C */ s16        field_47C; // 1 or 2, picked by the message at 80133898
    /* 0x47E */ s16        field_47E; // copy of `field_480`, kept for change detection
    /* 0x480 */ s16        field_480; // animation id the slots are seeded with
    /* 0x482 */ s16        field_482;
    /* 0x484 */ byte       pad_484[0x2A];
    /* 0x4AE */ s16        field_4AE;
    /* 0x4B0 */ byte       pad_4B0[0x2];
    /* 0x4B2 */ s16        field_4B2;
    /* 0x4B4 */ s16        field_4B4; // reset argument the message handler writes
    /* 0x4B6 */ byte       pad_4B6[0x2];
    /* 0x4B8 */ GpAnimRec* field_4B8; // last `Gp_AnimGetRec` record the footstep check saw
    /* 0x4BC */ u8         field_4BC;
    /* 0x4BD */ byte       pad_4BD[0x3];
} Actor461800Work2;
STATIC_ASSERT_SIZEOF(Actor461800Work2, 0x4C0);

extern Actor461800Work2* D_actor_461800_801438A0;

/// The second variant's task, published by its spawn routine
/// `func_actor_461800_8013307C` so the handlers can reach its model.
extern Task* D_actor_461800_801438A4;

/// Message payload the state handlers take as `Gp_DispatchMsg`'s `arg2`.
typedef struct Actor461800Msg {
    /* 0x0 */ byte pad_0[2];
    /* 0x2 */ u16  field_2;
} Actor461800Msg;

/// Animation preset the overlay's "play animation" message handlers apply to
/// their work block: `field_4` is the animation id, `field_8` picks the reset
/// path -- 1 for the blended `func_800B4114` reseed, 2 for a plain one -- and
/// `field_C` becomes the reset argument the reseed forwards. The id range each
/// handler accepts is that variant's own: `func_actor_461800_80132D84` takes
/// the first variant's six ids, `func_actor_461800_80133898` the second
/// variant's 0x23.
typedef struct Actor461800AnimPreset {
    /* 0x00 */ s32 field_0;
    /* 0x04 */ s32 field_4;
    /* 0x08 */ s32 field_8;
    /* 0x0C */ s32 field_C;
} Actor461800AnimPreset;

/* Scratchpad stack pointer, initialised by GameMain (see src/main/gamemain.c). */
#define SCRATCH_SP (*(u32*)0x1F8003FC)

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

    obj        = task->extra;
    coord      = obj->coords;
    task->work = (TaskIdMap*)(D_actor_461800_80143894 = memCalloc(0x4F8, false));
    if (D_actor_461800_80143894 == NULL) {
        Gp_DestroyEnemy(enemy, task);
        return;
    }
    task->exitCallback      = func_actor_461800_80132A90;
    coord->sub              = &gGfxViewCoord;
    enemy->field_4          = &coord->coord;
    enemy->field_48         = 0;
    enemy->node.targeted    = 0;
    enemy->node.flags       = 1;
    obj->otOffset           = 1;
    obj->flags              = 0;
    obj->lightMtx           = &D_actor_461800_80143894->light;
    obj->colorMtx           = &D_actor_461800_80143894->color;
    vec.vx                  = coord->workm.t[0];
    vec.vy                  = coord->workm.t[1] - 0x320;
    D_actor_461800_80143898 = task;
    vec.vz                  = coord->workm.t[2];
    func_800D7A9C(obj, &vec, 0, 3);
    func_800B3F84(&D_actor_461800_80143894->anim, D_actor_461800_80139FB0, obj,
                  D_actor_461800_80143894->pad_374, D_actor_461800_80143894->slots);
    D_actor_461800_80143894->field_4B8 = 1;
    D_actor_461800_80143894->field_4B4 = 2;

    spawned1 = Task_SpawnFromTable(&D_actor_461800_80139F8C, 1, 8, 0);
    if (spawned1 != NULL) {
        D_actor_461800_80143894->field_4F0 = spawned1;
        sessionKey1                        = (GpAreaKey*)&gGameSession->at4.loc;
        raw1                               = ((GpEnemy*)task->spawnArg2)->placeKey;
        model1                             = spawned1->extra;
        key.stage                          = sessionKey1->stage;
        key.area                           = sessionKey1->area;
        areaByte1                          = sessionKey1->room;
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
        D_actor_461800_80143894->field_4F4 = spawned2;
        sessionKey2                        = (GpAreaKey*)&gGameSession->at4.loc;
        raw2                               = ((GpEnemy*)task->spawnArg2)->placeKey;
        model2                             = spawned2->extra;
        key.stage                          = sessionKey2->stage;
        key.area                           = sessionKey2->area;
        areaByte1                          = sessionKey2->room;
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

    D_actor_461800_80143894->field_4EA = 0;
    D_actor_461800_80143894->field_4EC = 0;
    task->msgTable                     = D_actor_461800_80139F5C;
    func_actor_461800_80132660(task);
    task->state++;
}

/// Per-frame update of the first variant: modes 1 and 2 run their one-shot
/// setup and switch to mode 3; mode 3 walks the model while `field_4EA` counts
/// down (distance picked by `D_actor_461800_8014389C`), turns it while
/// `field_4EC` counts down in animation 3, then ticks the animation.
void func_actor_461800_80132660(Task* task)
{
    GsCOORDINATE2*   coord = ((TmdObject*)task->extra)->coords;
    Actor461800Work* work  = (Actor461800Work*)task->work;

    if (D_actor_461800_80143894->field_4B4 == 1) {
        func_actor_461800_80132D04();
        D_actor_461800_80143894->field_4B4 = 3;
    } else if (D_actor_461800_80143894->field_4B4 == 2) {
        func_actor_461800_80132C74();
        D_actor_461800_80143894->field_4B4 = 3;
    } else if (D_actor_461800_80143894->field_4B4 == 3) {
        if (work->field_4B8 == 0xE || work->field_4B8 == 2 || work->field_4B8 == 0xF) {
            if (work->field_4EA != 0) {
                switch (D_actor_461800_8014389C) {
                    case 0:
                        Actor461800_MoveForward(task, 0x3C);
                        break;
                    case 1:
                        Actor461800_MoveForward(task, -0xF);
                        break;
                    case 2:
                        Actor461800_MoveForward(task, 0x19);
                        break;
                }
                if (--work->field_4EA == 0) {
                    work->field_4B4         = 1;
                    D_actor_461800_80139F58 = 10;
                    work->field_4B8         = 0xD;
                }
            }
        }
        if (work->field_4B8 == 3 && work->field_4EC != 0) {
            work->field_4E6 += 0x33;
            Gfx_RotMatrixY(&coord->coord, work->field_4E6, 1);
            coord->flg = 0;
            work->field_4EC--;
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

    obj   = task->extra;
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
    taskKill(work->field_4F0);
    taskKill(work->field_4F4);
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

    obj   = (TmdObject*)task->extra;
    coord = obj->coords;
    if (!(obj->flags & 0x80) && obj->buffer != NULL) {
        vec     = (VECTOR3*)(SCRATCH_SP -= 0x18);
        vec->vx = coord->workm.t[0];
        vec->vy = coord->workm.t[1];
        vec->vz = coord->workm.t[2];
        Gp_DrawEffGroundQuad(vec, 0x200, Gp_State1C->groundShade);
        SCRATCH_SP += 0x18;
    }
}

/// State handler of the actor's model task: the spawn tick hangs the task's own
/// coordinate frame off the actor's part `spawnArg1` and steps to state 1, and
/// every later tick hands that part's world translation, dropped by 0x320 in y,
/// to `func_800D7A9C` for the part colour matrix.
void func_actor_461800_80132B74(Task* task)
{
    TmdObject*     extra = task->extra;
    GsCOORDINATE2* coord = extra->coords;
    GsCOORDINATE2* parts = ((TmdObject*)D_actor_461800_80143898->extra)->coords;
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
        Gp_AnimTickIndex(&D_actor_461800_80143894->anim, i);
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
        D_actor_461800_80143894->slots[i].rate = 1;
        Gp_AnimResetSlot(&D_actor_461800_80143894->anim, i, D_actor_461800_80143894->field_4B8);
        i++;
    } while (i < 0x14);
    D_actor_461800_80143894->field_4B6 = D_actor_461800_80143894->field_4B8;
}

/// Reseeds animation slots 1..0x13 from the current animation id and records
/// that id as the one now playing.
void func_actor_461800_80132D04(void)
{
    s32 i;

    i = 1;
    do {
        func_800B4114(&D_actor_461800_80143894->anim, i, D_actor_461800_80143894->field_4B8, 0,
                      D_actor_461800_80139F58);
        i++;
    } while (i < 0x14);
    D_actor_461800_80143894->field_4B6 = D_actor_461800_80143894->field_4B8;
}

/// Applies an animation preset: the id is copied into the work block, the reset
/// mode is picked by the preset's blend flag and the reset argument is either
/// taken from the preset or left at 2, then the whole slot array is re-seeded.
/// Only the six known animation ids are accepted; anything else leaves the work
/// block untouched and reports the failure.
s32 func_actor_461800_80132D84(Task* task, s32 arg1, Actor461800AnimPreset* preset, s32 arg3)
{
    if (preset->field_4 < 6) {
        D_actor_461800_80143894->field_4B8 = preset->field_4;
        if (preset->field_8 != 0) {
            D_actor_461800_80143894->field_4B4 = 1;
            D_actor_461800_80139F58            = preset->field_C;
        } else {
            D_actor_461800_80143894->field_4B4 = 2;
        }
        D_actor_461800_80143894->field_4BA = 0;
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
    TmdObject* own    = D_actor_461800_80143898->extra;
    TmdObject* first  = D_actor_461800_80143894->field_4F0->extra;
    TmdObject* second = D_actor_461800_80143894->field_4F4->extra;

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
s32 func_actor_461800_80132EA4(Task* task, s32 arg1, ActorShared8013411cPlacement* placement)
{
    GsCOORDINATE2* coord;
    u16            yaw;

    coord                              = ((TmdObject*)task->extra)->coords;
    D_actor_461800_80143894->field_4E6 = yaw = placement->rot.vy;
    Gfx_RotMatrixY(&coord->coord, (s16)yaw, 1);
    coord->coord.t[0] = placement->pos.vx;
    coord->coord.t[1] = placement->pos.vy;
    coord->coord.t[2] = placement->pos.vz;
    coord->flg        = 0;
    return 0;
}

s32 func_actor_461800_80132F20(Task* arg0, s32 arg1, Actor461800Msg* arg2, s32 arg3)
{
    if (arg2->field_2 == 0) {
        D_actor_461800_80143894->field_4EC = 0x14;
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

    coord                   = ((TmdObject*)task->extra)->coords;
    work                    = (Actor461800Work*)task->work;
    D_actor_461800_8014389C = mode;
    dx                      = target->vx - coord->coord.t[0];
    dz                      = target->vz - coord->coord.t[2];
    angle                   = ratan2(dx, dz);
    work->field_4E6         = angle;
    if (D_actor_461800_8014389C == 1) {
        work->field_4E6 = angle + 0x800;
    }
    Gfx_RotMatrixY(&coord->coord, work->field_4E6, 1);
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
    work->field_4EA = dist / steps;
    return 0;
}

void func_actor_461800_8013307C(GpEnemy* enemy, Task* task)
{
    VECTOR         vec;
    GsCOORDINATE2* coord;
    TmdObject*     obj;

    obj        = task->extra;
    coord      = obj->coords;
    task->work = (TaskIdMap*)(D_actor_461800_801438A0 = memCalloc(0x4C0, false));
    if (D_actor_461800_801438A0 == NULL) {
        Gp_DestroyEnemy(enemy, task);
        return;
    }
    task->exitCallback      = func_actor_461800_80133634;
    coord->sub              = &gGfxViewCoord;
    enemy->field_4          = &coord->coord;
    enemy->field_48         = 0;
    enemy->node.targeted    = 0;
    enemy->node.flags       = 1;
    obj->otOffset           = 1;
    obj->lightMtx           = &D_actor_461800_801438A0->light;
    obj->colorMtx           = &D_actor_461800_801438A0->color;
    vec.vx                  = coord->workm.t[0];
    vec.vy                  = coord->workm.t[1] - 0x320;
    D_actor_461800_801438A4 = task;
    vec.vz                  = coord->workm.t[2];
    func_800D7A9C(obj, &vec, 0, 3);
    func_800B3F84(&D_actor_461800_801438A0->anim, D_actor_461800_801437F8, obj,
                  &D_actor_461800_801438A0->slots[0x13], D_actor_461800_801438A0->slots);
    D_actor_461800_801438A0->field_480 = 1;
    D_actor_461800_801438A0->field_47C = 2;
    D_actor_461800_801438A0->field_4B2 = 0;
    D_actor_461800_801438A0->field_4B4 = 0;
    D_actor_461800_801438A0->field_4B8 = 0;
    D_actor_461800_801438A0->field_4BC = 0;
    task->msgTable                     = D_actor_461800_801437BC;
    func_actor_461800_801331E4(task);
    task->state++;
}

/// Per-frame update of the second variant: modes 1 and 2 run their one-shot
/// setup and switch to mode 3 for the next frame; mode 3 walks the model while `field_4B2` counts
/// down (distance picked by `D_actor_461800_801438A8`), turns it while
/// `field_4B4` counts down in animation 3, then ticks the animation.
void func_actor_461800_801331E4(Task* task)
{
    GsCOORDINATE2*    coord = ((TmdObject*)task->extra)->coords;
    Actor461800Work2* work  = (Actor461800Work2*)task->work;

    if (D_actor_461800_801438A0->field_47C == 1) {
        func_actor_461800_8013380C();
        D_actor_461800_801438A0->field_47C = 3;
    } else if (D_actor_461800_801438A0->field_47C == 2) {
        func_actor_461800_80133770();
        D_actor_461800_801438A0->field_47C = 3;
    } else if (D_actor_461800_801438A0->field_47C == 3) {
        if (work->field_480 == 0xE || work->field_480 == 2 || work->field_480 == 0xF) {
            if (work->field_4B2 != 0) {
                switch (D_actor_461800_801438A8) {
                    case 0:
                        Actor461800_MoveForward(task, 0x3C);
                        break;
                    case 1:
                        Actor461800_MoveForward(task, -0xF);
                        break;
                    case 2:
                        Actor461800_MoveForward(task, 0x19);
                        break;
                }
                if (--work->field_4B2 == 0) {
                    work->field_47C         = 1;
                    D_actor_461800_801437B8 = 10;
                    work->field_480         = 0xD;
                }
            }
        }
        if (work->field_480 == 3 && work->field_4B4 != 0) {
            work->field_4AE += 0x33;
            Gfx_RotMatrixY(&coord->coord, work->field_4AE, 1);
            coord->flg = 0;
            work->field_4B4--;
        }
        func_actor_461800_80133724();
        if (work->field_4BC != 0) {
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

    D_actor_461800_801438A0 = (Actor461800Work2*)task->work;
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

    obj   = task->extra;
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
/// is latched in `field_4B8` so each one fires once.
void func_actor_461800_8013365C(Task* task)
{
    Actor461800Work2* work;
    GsCOORDINATE2*    obj;
    GpAnimRec*        rec;
    s32               kind;
    s32               id;
    s32               pan;

    work = (Actor461800Work2*)task->work;
    obj  = ((TmdObject*)task->extra)->coords + 1;
    rec  = Gp_AnimGetRec(&work->anim, &work->slots[1]);
    if (rec == NULL || rec == work->field_4B8) {
        return;
    }
    work->field_4B8 = rec;
    kind            = rec->flags & 0x30;
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
        Gp_AnimTickIndex(&D_actor_461800_801438A0->anim, i);
        i++;
    } while (i < 0x13);
}

/// Restarts animation slots 1..0x12 from `field_480`, flagging each slot's
/// `field_9` before the reset so it replays from the top, and latches that id
/// into `field_47E` as the one now playing.
void func_actor_461800_80133770(void)
{
    s32 i;

    D_actor_461800_801438A0->field_4B8 = 0;
    i                                  = 1;
    do {
        D_actor_461800_801438A0->slots[i].rate = 1;
        Gp_AnimResetSlot(&D_actor_461800_801438A0->anim, i, D_actor_461800_801438A0->field_480);
        i++;
    } while (i < 0x13);
    D_actor_461800_801438A0->field_47E = D_actor_461800_801438A0->field_480;
}

/// Reseeds animation slots 1..0x12 from `field_480` and latches that id into
/// `field_47E` as the one now playing.
void func_actor_461800_8013380C(void)
{
    s32 i;

    D_actor_461800_801438A0->field_4B8 = 0;
    i                                  = 1;
    do {
        func_800B4114(&D_actor_461800_801438A0->anim, i, (s16)D_actor_461800_801438A0->field_480, 0,
                      D_actor_461800_801437B8);
        i++;
    } while (i < 0x13);
    D_actor_461800_801438A0->field_47E = D_actor_461800_801438A0->field_480;
}

/// Applies an animation preset to the second variant's work block: the id is
/// copied in, the reset mode is picked by the preset's blend flag and the reset
/// argument is either taken from the preset or left at 2, then the work block's
/// animation is restarted through `func_actor_461800_801331E4`. Only the ids
/// this variant owns are accepted; anything else leaves the work block
/// untouched and reports the failure.
s32 func_actor_461800_80133898(Task* task, s32 arg1, Actor461800AnimPreset* preset, s32 arg3)
{
    if (preset->field_4 < 0x23) {
        D_actor_461800_801438A0->field_480 = preset->field_4;
        if (preset->field_8 != 0) {
            D_actor_461800_801438A0->field_47C = 1;
            D_actor_461800_801437B8            = preset->field_C;
        } else {
            D_actor_461800_801438A0->field_47C = 2;
        }
        D_actor_461800_801438A0->field_482 = 0;
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

    obj = (TmdObject*)D_actor_461800_801438A4->extra;
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
s32 func_actor_461800_80133970(Task* task, s32 arg1, ActorShared8013411cPlacement* placement)
{
    GsCOORDINATE2* coord;
    u16            yaw;

    coord                              = ((TmdObject*)task->extra)->coords;
    D_actor_461800_801438A0->field_4AE = yaw = placement->rot.vy;
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
s32 func_actor_461800_801339EC(Task* task, s32 arg1, Actor461800Msg* msg, s32 arg3)
{
    s32 id;

    id = msg->field_2;
    switch (id) {
        case 0:
            D_actor_461800_801438A0->field_4B4 = 0x14;
            break;
        case 1:
            D_actor_461800_801438A0->field_4BC = id;
            break;
    }
    return 0;
}

/// Turns the model to face `target` -- away from it in mode 1 -- and stores the
/// per-step distance in the second work block: the planar distance over 60
/// steps in mode 0, 15 in mode 1 and 25 in mode 2.
s32 func_actor_461800_80133A3C(Task* task, s32 arg1, VECTOR* target, s32 mode)
{
    GsCOORDINATE2*    coord;
    Actor461800Work2* work;
    s32               dx;
    s32               dz;
    s32               steps;
    s32               dist;
    s32               angle;

    coord                   = ((TmdObject*)task->extra)->coords;
    work                    = (Actor461800Work2*)task->work;
    D_actor_461800_801438A8 = mode;
    dx                      = target->vx - coord->coord.t[0];
    dz                      = target->vz - coord->coord.t[2];
    angle                   = ratan2(dx, dz);
    work->field_4AE         = angle;
    if (D_actor_461800_801438A8 == 1) {
        work->field_4AE = angle + 0x800;
    }
    Gfx_RotMatrixY(&coord->coord, work->field_4AE, 1);
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
    work->field_4B2 = dist / steps;
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

    obj   = (TmdObject*)task->extra;
    coord = obj->coords;
    if (!(obj->flags & 0x80) && obj->buffer != NULL) {
        vec     = (VECTOR3*)(SCRATCH_SP -= 0x18);
        vec->vx = coord->workm.t[0];
        vec->vy = coord->workm.t[1];
        vec->vz = coord->workm.t[2];
        Gp_DrawEffGroundQuad(vec, 0x200, Gp_State1C->groundShade);
        SCRATCH_SP += 0x18;
    }
}

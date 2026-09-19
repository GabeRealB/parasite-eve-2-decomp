#include "common.h"

#include "actors/actor_461800.h"
#include "actors/actor_461800_move.h"
#include "actors/actors_shared_80132ecc.h"
#include "gameplay/1BC.h"
#include "gameplay/268.h"
#include "gameplay/D4.h"
#include "main/fs.h"
#include "main/gameflag.h"
#include "main/gfx.h"
#include "main/mc.h"
#include "main/mem.h"
#include "main/session.h"
#include "main/task.h"
#include "main/tmd.h"

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
    tile           = (TILE*)Gpu_PrimCursor;
    Gpu_PrimCursor = (DR_TPAGE*)(tile + 1);
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
    addPrim(Gpu_CurrentOt + 5, tile);

    dr             = Gpu_PrimCursor;
    Gpu_PrimCursor = dr + 1;
    setlen(dr, 1);
    dr->code[0] = 0xE1000240;
    addPrim(Gpu_CurrentOt + 5, dr);
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
    GpCdRec10*     entry1;
    GpAreaKey*     sessionKey1;
    u32            raw2, index2;
    Task*          spawned2;
    TmdObject*     model2;
    GpCdRec10*     entry2;
    GpAreaKey*     sessionKey2;

    obj        = task->extra;
    coord      = obj->coords;
    task->work = (TaskIdMap*)(D_actor_461800_80143894 = memCalloc(0x4F8, false));
    if (D_actor_461800_80143894 == NULL) {
        Gp_DestroyEnemy(enemy, task);
        return;
    }
    task->exitCallback      = ActorsShared80132ecc;
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
        raw1                               = ((GpEnemy*)task->spawnArg2)->field_8;
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
        entry1        = (GpCdRec10*)((index1 * 0x10) + (s32)Gp_GetNestedAreaRec(&key)->field_0);
        model1->tpage = entry1->field_D;
        model1->clut  = entry1->field_E;
        if (model1->buffer != NULL) {
            tmdProcessStream(model1);
            tmdProcessStream(model1);
        }
    }

    spawned2 = Task_SpawnFromTable(&D_actor_461800_80139F8C, 2, 0xC, 0);
    if (spawned2 != NULL) {
        D_actor_461800_80143894->field_4F4 = spawned2;
        sessionKey2                        = (GpAreaKey*)&gGameSession->at4.loc;
        raw2                               = ((GpEnemy*)task->spawnArg2)->field_8;
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
        entry2        = (GpCdRec10*)((index2 * 0x10) + (s32)Gp_GetNestedAreaRec(&key)->field_0);
        model2->tpage = entry2->field_D;
        model2->clut  = entry2->field_E;
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

extern s16 D_actor_461800_80139F58;
extern s16 D_actor_461800_8014389C;

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

INCLUDE_ASM("actors/nonmatchings/actor_461800/actor_461800", func_actor_461800_80132A0C);

#include "common.h"

#include "actors/actor_120300.h"
#include "gameplay/1BC.h"
#include "gameplay/3CD8.h"
#include "gameplay/D4.h"
#include "main/fs.h"
#include "main/mem.h"
#include "main/task.h"
#include "main/tmd.h"
#include "main/gfx.h"

extern s8  D_8007218A;
extern u8  D_80073BA9;
extern s32 D_actor_120300_80140910;
extern s32 D_actor_120300_80140A44;
extern s32 D_actor_120300_80140ACC;
extern s32 D_actor_120300_80140AFC;
extern s32 D_actor_120300_80140B14;
extern s32 D_actor_120300_80140B2C;
extern s32 D_actor_120300_80140B44;
extern s32 D_actor_120300_80140B5C;
extern s32 D_actor_120300_80140B74;
extern s32 D_actor_120300_801416D4;
extern s32 D_actor_120300_801417AC;
extern s32 D_actor_120300_80141884;
extern s32 D_actor_120300_80141A34;

/// Animation id per `Actor120300Work::field_4D4`; -1 skips the restart.
extern s16 D_actor_120300_80140980[];

extern TaskDesc ActorsShared80134898Desc;

/// Ticks slots 1..19 of a task's animation context and, if every one of them
/// then has `GpAnimSlot.flags` bit 0x100 set, re-reads the work block and
/// restarts all twenty slots on the id `D_actor_120300_80140980` selects for
/// `field_4D4`, returning 1; a negative entry or an unset slot returns 0. The
/// gotos reproduce retail's block layout.
s32 func_actor_120300_80131EE0(Task* arg0)
{
    Actor120300Work* work;
    Actor120300Work* animWork;
    u16              anim;
    u16              i;
    u16              done;

    work = (Actor120300Work*)arg0->work;
    for (i = 1; i < 0x14; i++) {
        Gp_AnimTickIndex(&work->anim, i);
    }
    i    = 1;
    done = 1;
    for (; i < 0x14; i++) {
        if (!(work->slots[i].flags & 0x100)) {
            goto fail;
        }
    }
check:
    if (done) {
        if (D_actor_120300_80140980[work->field_4D4] >= 0) {
            anim                = D_actor_120300_80140980[work->field_4D4];
            animWork            = (Actor120300Work*)arg0->work;
            animWork->field_4D4 = anim;
            goto loop;
        fail:
            done = 0;
            goto check;
        loop:
            for (i = 1; i < 0x14; i++) {
                func_800B4114(&animWork->anim, i, anim, 0, 10);
            }
        }
        return 1;
    }
    return 0;
}

/// Spawn tick of a child actor that keeps the model facing the player: state 0
/// allocates the 0x4E4-byte `Actor120300Work` block, parks it in
/// `Task::work`, points the model's light and colour matrices at the block's
/// `field_474` / `field_494`, clears `TmdObject::flags` and anchors the root
/// coordinate `sub` under part 4 of the spawning task's model
/// (`Task::spawnArg2->extra`); a failed allocation kills the task instead of
/// stepping to state 1. The texture page / CLUT row then come from the
/// placement record at the nested area table's `field_0` list whose id matches
/// neither 0xFF (end) nor 0x6A (the skip marker). Every tick after that reads
/// the parent work block's `field_4E0` and primes the colour matrix with the
/// root coordinate's own translation through `func_800D7A9C`, then replaces
/// that translation with the parent scale broadcast over all three axes and
/// folds it in with `ScaleMatrix`.
void func_actor_120300_80132004(Task* arg0)
{
    VECTOR           vec;
    GsCOORDINATE2*   coord;
    TaskIdMap*       map;
    TmdObject*       tmd;
    GpAreaPlace*     place;
    s32              scale;
    s32              kill;
    s32              killCopy;
    u16              scaleRaw;
    TmdObject*       tmd2;
    Actor120300Work* work;
    u8               id;

    if (arg0->state == 0) {
        tmd        = arg0->extra;
        coord      = tmd->coords;
        map        = Mem_Malloc(0x4E4, 0);
        arg0->work = map;
        if (map == NULL) {
            kill = 1;
        } else {
            work = (Actor120300Work*)map;
            Mem_Set(map, 0, 0x4E4);
            coord->sub                       = ((TmdObject*)((Task*)arg0->spawnArg2)->extra)->coords + 4;
            ((TmdObject*)arg0->extra)->flags = 0;
            Tmd_AllocBuffers(tmd);
            kill           = 0;
            tmd->lightMtx  = &work->field_474;
            tmd->colorMtx  = &work->field_494;
            arg0->msgTable = &D_actor_120300_80140A44;
        }
        killCopy = kill;
        TOUCH_REG(killCopy);
        if (killCopy != 0) {
            taskKill(arg0);
            return;
        }
        place = (GpAreaPlace*)Gp_GetNestedAreaRec((GpAreaKey*)&gGameSession->at4.loc)->field_0;
        id    = place->entryId;
        while (id != 0xFF) {
            if (id == 0x6A) {
                break;
            }
            place++;
            id = place->entryId;
        }
        Gp_SetTmdBytes(arg0->extra, ((s8*)place)[0xD], ((s8*)place)[0xE]);
        arg0->state += 1;
    }
    tmd2     = arg0->extra;
    scaleRaw = ((Actor120300Work*)((Task*)arg0->spawnArg2)->work)->field_4E0;
    vec.vx   = tmd2->coords->workm.t[0];
    vec.vy   = ((TmdObject*)arg0->extra)->coords->workm.t[1];
    vec.vz   = ((TmdObject*)arg0->extra)->coords->workm.t[2];
    func_800D7A9C(tmd2, &vec, 0, 3);
    scale  = scaleRaw & 0xFFFF;
    vec.vz = scale;
    vec.vy = scale;
    vec.vx = scale;
    ScaleMatrix(tmd2->colorMtx, &vec);
}

/// Spawn tick of a child actor. State 0 allocates the 0x4E4-byte
/// `Actor120300Work` block, parks it in `Task::work`, points the model's
/// light and colour matrices at the block's `field_474` / `field_494`, clears
/// `TmdObject::flags` and anchors the root coordinate `sub` under part 8 of
/// the spawning task's model (`Task::spawnArg2->extra`). A failed allocation
/// kills the task rather than stepping to state 1.
/// Every later tick reads the parent work block's `field_4E0` and primes the
/// colour matrix with the root coordinate's own translation through
/// `func_800D7A9C`, then replaces that translation with the parent scale
/// broadcast over all three axes and folds it in with `ScaleMatrix`.
void func_actor_120300_801321C8(Task* arg0)
{
    VECTOR           vec;
    GsCOORDINATE2*   coord;
    TaskIdMap*       map;
    TmdObject*       tmd;
    s32              scale;
    s32              kill;
    s32              killCopy;
    u16              scaleRaw;
    TmdObject*       tmd2;
    Actor120300Work* work;

    if (arg0->state == 0) {
        tmd        = arg0->extra;
        coord      = tmd->coords;
        map        = Mem_Malloc(0x4E4, 0);
        arg0->work = map;
        if (map == NULL) {
            kill = 1;
        } else {
            work = (Actor120300Work*)map;
            Mem_Set(map, 0, 0x4E4);
            coord->sub                       = ((TmdObject*)((Task*)arg0->spawnArg2)->extra)->coords + 8;
            ((TmdObject*)arg0->extra)->flags = 0;
            Tmd_AllocBuffers(tmd);
            kill           = 0;
            tmd->lightMtx  = &work->field_474;
            tmd->colorMtx  = &work->field_494;
            arg0->msgTable = &D_actor_120300_80140A44;
        }
        killCopy = kill;
        TOUCH_REG(killCopy);
        if (killCopy != 0) {
            taskKill(arg0);
            return;
        }
        arg0->state += 1;
    }
    tmd2     = arg0->extra;
    scaleRaw = ((Actor120300Work*)((Task*)arg0->spawnArg2)->work)->field_4E0;
    vec.vx   = tmd2->coords->workm.t[0];
    vec.vy   = ((TmdObject*)arg0->extra)->coords->workm.t[1];
    vec.vz   = ((TmdObject*)arg0->extra)->coords->workm.t[2];
    func_800D7A9C(tmd2, &vec, 0, 3);
    scale  = scaleRaw & 0xFFFF;
    vec.vz = scale;
    vec.vy = scale;
    vec.vx = scale;
    ScaleMatrix(tmd2->colorMtx, &vec);
}

INCLUDE_RODATA("actors/nonmatchings/actor_120300/actor_120300", D_actor_120300_80131E20);

INCLUDE_ASM("actors/nonmatchings/actor_120300/actor_120300", func_actor_120300_80132338);

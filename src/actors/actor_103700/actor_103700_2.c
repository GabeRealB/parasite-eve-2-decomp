#include "common.h"

#include "actors/actor_103700.h"
#include "actors/actors_shared_80133c1c.h"
#include "gameplay/3A34.h"
#include "gameplay/3CD8.h"
#include "gameplay/D4.h"
#include "main/session.h"
#include "main/sound.h"
#include "main/wipsys.h"

extern u8 D_80115408[];

void func_actor_103700_801328DC(Task* task)
{
    Actor103700Work* work;
    GsCOORDINATE2*   coord;
    void*            head;
    SVECTOR*         vec;
    s16              angle;
    s32              dist;

    head                    = *(void**)G_SCRATCH_HEAD;
    vec                     = (SVECTOR*)((u8*)head - sizeof(SVECTOR));
    *(void**)G_SCRATCH_HEAD = vec;
    work                    = (Actor103700Work*)task->work;
    coord                   = ((TmdObject*)task->extra)->coords;

    switch (work->field_250) {
        case 0:
            Gp_LcgState        = Gp_LcgState * 5 + 0x71357911;
            work->field_252    = ((Gp_LcgState >> 16) & 0xF) + 20;
            work->field_254    = D_actor_103700_80139D9C[((Actor103700Spawn*)task->spawnArg2)->field_3C->rowIndex];
            Gp_LcgState        = Gp_LcgState * 5 + 0x71357911;
            angle              = (Gp_LcgState >> 16) & 0xFFF;
            Gp_LcgState        = Gp_LcgState * 5 + 0x71357911;
            dist               = (Gp_LcgState >> 16) & 0x1FF;
            work->field_23C.vx = work->field_234.vx + ((dist * rsin(angle)) >> 12);
            Gp_LcgState        = Gp_LcgState * 5 + 0x71357911;
            work->field_23C.vy = work->field_234.vy + ((Gp_LcgState >> 16) & 0x1FF);
            work->field_23C.vz = work->field_234.vz + ((dist * rcos(angle)) >> 12);
            vec->vx            = work->field_23C.vx - coord->coord.t[0];
            vec->vy            = 0;
            vec->vz            = work->field_23C.vz - coord->coord.t[2];
            work->field_244    = ratan2(vec->vx, vec->vz) & 0xFFF;
            work->field_250    = 1;
            break;
        case 1:
            vec->vx = work->field_23C.vx - coord->coord.t[0];
            vec->vz = work->field_23C.vz - coord->coord.t[2];
            if ((s16)SquareRoot0(vec->vx * vec->vx + vec->vz * vec->vz) < 120) {
                work->field_250 = 0;
            }
            break;
    }
    func_actor_103700_801350DC(task, 0, 14);
    func_actor_103700_80135140(task, 20);
    if (ActorsShared80133c1c((ActorShared80133c1c*)task) != 0) {
        work->field_24E      = 3;
        work->field_250      = 0;
        Gp_StateF0.field_19 |= 1;
    }
    *(u32*)G_SCRATCH_HEAD += sizeof(SVECTOR);
}

void func_actor_103700_80132B7C(Task* task)
{
    Actor103700Work* work;
    GsCOORDINATE2*   coord;
    s32              dist;
    s32              i;

    work  = (Actor103700Work*)task->work;
    coord = ((TmdObject*)task->extra)->coords;

    switch (work->field_250) {
        case 0:
            if (ActorsShared80133c1c((ActorShared80133c1c*)task) != 0) {
                Gp_StateF0.field_19 |= 1;
                Gp_LcgState          = Gp_LcgState * 5 + 0x71357911;
                work->field_250      = 1;
                work->field_256      = (Gp_LcgState >> 16) & 0x3F;
            }
            break;
        case 1:
            if ((s16)--work->field_256 <= 0) {
                work->field_250 = 2;
                work->field_256 = 0;
                work->field_248 = 4;
            }
            break;
        case 2:
            for (i = 0; i < 7; i++) {
                if (D_actor_103700_80139E14[i].threshold >= work->field_24C) {
                    coord->coord.t[1] += D_actor_103700_80139E14[i].dy / D_actor_103700_80139E14[i].steps;
                    dist               = D_actor_103700_80139E14[i].dist / D_actor_103700_80139E14[i].steps;
                    coord->coord.t[0] += (rsin(work->field_246) * dist) >> 12;
                    coord->coord.t[2] += (rcos(work->field_246) * dist) >> 12;
                    break;
                }
            }
            if (work->field_24C >= 50) {
                work->field_248      = 1;
                work->field_24E      = 3;
                work->field_250      = 0;
                Gp_StateF0.field_19 |= 1;
            }
            break;
    }
}

void func_actor_103700_80132DA8(Task* task)
{
    Actor103700Work* work;
    GsCOORDINATE2*   coord;
    s32              dist;
    s32              i;

    work  = (Actor103700Work*)task->work;
    coord = ((TmdObject*)task->extra)->coords;

    switch (work->field_250) {
        case 0:
            if (ActorsShared80133c1c((ActorShared80133c1c*)task) != 0) {
                Gp_StateF0.field_19 |= 1;
                Gp_LcgState          = Gp_LcgState * 5 + 0x71357911;
                work->field_250      = 1;
                work->field_256      = (Gp_LcgState >> 16) & 0x3F;
            }
            break;
        case 1:
            if ((s16)--work->field_256 <= 0) {
                work->field_250 = 2;
                work->field_256 = 0;
                work->field_248 = 5;
            }
            break;
        case 2:
            for (i = 0; i < 9; i++) {
                if (D_actor_103700_80139E4C[i].threshold >= work->field_24C) {
                    coord->coord.t[1] += D_actor_103700_80139E4C[i].dy / D_actor_103700_80139E4C[i].steps;
                    dist               = D_actor_103700_80139E4C[i].dist / D_actor_103700_80139E4C[i].steps;
                    coord->coord.t[0] += (rsin(work->field_246) * dist) >> 12;
                    coord->coord.t[2] += (rcos(work->field_246) * dist) >> 12;
                    break;
                }
            }
            if (work->field_24C >= 50) {
                work->field_248      = 1;
                work->field_24E      = 3;
                work->field_250      = 0;
                Gp_StateF0.field_19 |= 1;
            }
            break;
    }
}

void func_actor_103700_80132FD4(Task* task)
{
    Actor103700Work* work;
    GsCOORDINATE2*   coord;
    void*            head;
    SVECTOR*         vec;
    s32              i;
    s32              sound;
    s8               slot;

    coord                   = ((TmdObject*)task->extra)->coords;
    head                    = *(void**)G_SCRATCH_HEAD;
    *(void**)G_SCRATCH_HEAD = (u8*)head - sizeof(SVECTOR);
    work                    = (Actor103700Work*)task->work;
    vec                     = *(void**)G_SCRATCH_HEAD;

    switch (work->field_250) {
        case 0:
            work->field_23C.vx = Player_Status.coordMtx->t[0];
            Gp_LcgState        = Gp_LcgState * 5 + 0x71357911;
            work->field_23C.vy = Player_Status.coordMtx->t[1] - (((Gp_LcgState >> 16) & 0x3FF) + 800);
            work->field_23C.vz = Player_Status.coordMtx->t[2];
            if (work->field_26A == 0) {
                work->field_252 = 5;
            } else {
                work->field_252 = 0;
                work->field_26A = 0;
            }
            work->field_254 = D_actor_103700_80139D9C[((Actor103700Spawn*)task->spawnArg2)->field_3C->rowIndex];
            func_actor_103700_80135140(task, 20);
            if (work->field_246 == work->field_244) {
                work->field_250 = 1;
                work->field_252 = D_actor_103700_80139D3C[((Gp_LcgState = Gp_LcgState * 5 + 0x71357911) >> 16) & 0xF];
                coord           = ((TmdObject*)task->extra)->coords;
                vec->vx         = work->field_23C.vx - coord->coord.t[0];
                vec->vy         = work->field_23C.vy - coord->coord.t[1];
                vec->vz         = work->field_23C.vz - coord->coord.t[2];
                work->field_256 = SquareRoot0(vec->vx * vec->vx + vec->vy * vec->vy + vec->vz * vec->vz) / work->field_252;
                slot            = (((Gp_LcgState = Gp_LcgState * 5 + 0x71357911) >> 16) & 3) + 17;
                for (i = 1; i < 6; i++) {
                    work->slots[i].rate = slot;
                }
                coord = ((TmdObject*)task->extra)->coords;
                sound = ((((Actor103700Spawn*)task->spawnArg2)->field_8 >> 12) << 8) | 0x40250002;
                SndEvt_EnqueueType6(sound, (s8)Gp_GetObjPan((GpObj38*)coord), (s8)Gp_GetObjDepth((GpObj38*)coord));
            }
            break;
        case 1:
            func_actor_103700_80135140(task, 40);
            if ((s16)--work->field_256 <= 0) {
                work->field_250 = 0;
                work->field_256 = 0;
                work->field_26A = 1;
            }
            if (work->field_264 != 0) {
                if (func_actor_103700_80134F50(task) == 0) {
                    func_800FDB18(1, coord, NULL, &work->field_224);
                    work->field_24E = 5;
                } else {
                    work->field_24E = 4;
                }
                work->field_250 = 0;
            }
            if (D_80115408[1] & 2) {
                work->field_24E = 5;
                work->field_248 = 1;
                work->field_250 = 0;
            }
            break;
    }
    func_actor_103700_801350DC(task, 0, 14);
    *(u32*)G_SCRATCH_HEAD += sizeof(SVECTOR);
}

void func_actor_103700_80133370(Task* task)
{
    Actor103700Work* work;
    GpObj38*         obj;
    Task*            player;
    void*            head;
    GpAnimArg*       arg;
    s32              sound;

    work                    = (Actor103700Work*)task->work;
    obj                     = (GpObj38*)((TmdObject*)task->extra)->coords;
    player                  = Game_GetPtrSlot(3);
    head                    = *(void**)G_SCRATCH_HEAD;
    *(void**)G_SCRATCH_HEAD = (u8*)head - 0x1C;
    arg                     = (GpAnimArg*)*(void**)G_SCRATCH_HEAD;

    switch (work->field_250) {
        case 0:
            Gp_DispatchMsg(player, 0x3F9, Gp_PackPair(&D_actor_103700_80139D28, 0), 0);
            func_800FDB18(1, (GsCOORDINATE2*)obj, NULL, &work->field_224);
            Gp_SpawnPadLerp(5, 0xC0, 8);
            sound = ((((Actor103700Spawn*)task->spawnArg2)->field_8 >> 12) << 8) | 0x40250004;
            SndEvt_EnqueueType6(sound, (s8)Gp_GetObjPan(obj), (s8)Gp_GetObjDepth(obj));
            if ((s16)++work->field_26C >= 6) {
                work->field_26C      = 0;
                work->field_250      = 3;
                Gp_StateF0.field_19 |= 2;
            } else {
                work->field_250 = 1;
                work->field_256 = 20;
            }
            break;
        case 1:
            work->field_252 = -20;
            if ((s16)--work->field_256 <= 0) {
                work->field_250 = 2;
            }
            break;
        case 2:
            work->field_23C.vx = Player_Status.coordMtx->t[0];
            Gp_LcgState        = Gp_LcgState * 5 + 0x71357911;
            work->field_23C.vy = Player_Status.coordMtx->t[1] - (((Gp_LcgState >> 16) & 0x3FF) + 800);
            work->field_23C.vz = Player_Status.coordMtx->t[2];
            work->field_252    = D_actor_103700_80139D3C[((Gp_LcgState = Gp_LcgState * 5 + 0x71357911) >> 16) & 0xF];
            work->field_254    = D_actor_103700_80139D9C[((Actor103700Spawn*)task->spawnArg2)->field_3C->rowIndex];
            if (work->field_264 != 0) {
                work->field_250 = 0;
            }
            break;
        case 3:
            arg->field_0  = D_actor_103700_80139F1C;
            arg->field_4  = 2;
            arg->field_8  = 0;
            arg->field_C  = 0;
            arg->field_10 = 1;
            Gp_DispatchMsg(player, 0x3F4, (s32)arg, 0);
            sound = ((((Actor103700Spawn*)task->spawnArg2)->field_8 >> 12) << 8) | 6;
            SndEvt_EnqueueType6(sound, (s8)Gp_GetObjPan(obj), (s8)Gp_GetObjDepth(obj));
            work->field_250 = 4;
            break;
        case 4:
            if (Gp_DispatchMsg(player, 0x3ED, 0, 0) == 0) {
                Gp_DispatchMsg(player, 0x3F1, 0, 0);
                work->field_262      = 0;
                work->field_24E      = 5;
                work->field_250      = 0;
                Gp_StateF0.field_19 &= 1;
            }
            break;
    }
    func_actor_103700_801350DC(task, 1, 14);
    *(u32*)G_SCRATCH_HEAD += 0x1C;
}

void func_actor_103700_801336E8(Task* task)
{
    Actor103700Work* work;
    GpObj38*         obj;
    s32              period;
    s32              i;
    s32              slot;
    s32              sound;

    work   = (Actor103700Work*)task->work;
    obj    = (GpObj38*)((TmdObject*)task->extra)->coords;
    period = 14;

    switch (work->field_250) {
        case 0:
            work->field_250 = 1;
            work->field_256 = 30;
            work->field_258 = D_actor_103700_80139D5C[((Gp_LcgState = Gp_LcgState * 5 + 0x71357911) >> 16) & 0xF];
            work->field_26C = 0;
            slot            = (((Gp_LcgState = Gp_LcgState * 5 + 0x71357911) >> 16) & 3) + 10;
            for (i = 1; i < 6; i++) {
                work->slots[i].rate = slot;
            }
            break;
        case 1:
            work->field_23C.vx = Player_Status.coordMtx->t[0];
            Gp_LcgState        = Gp_LcgState * 5 + 0x71357911;
            work->field_23C.vy = Player_Status.coordMtx->t[1] - (((Gp_LcgState >> 16) & 0x3FF) + 800);
            work->field_23C.vz = Player_Status.coordMtx->t[2];
            work->field_254    = D_actor_103700_80139D9C[((Actor103700Spawn*)task->spawnArg2)->field_3C->rowIndex];
            if ((s16)--work->field_258 > 0) {
                work->field_252 = -50;
            } else {
                work->field_252 = 0;
                func_actor_103700_80135140(task, 20);
            }
            if ((s16)--work->field_256 <= 0) {
                work->field_250 = 2;
                work->field_256 = 0;
                work->field_252 = 0;
            }
            break;
        case 2:
            work->field_23C.vx = Player_Status.coordMtx->t[0];
            Gp_LcgState        = Gp_LcgState * 5 + 0x71357911;
            work->field_23C.vy = Player_Status.coordMtx->t[1] - (((Gp_LcgState >> 16) & 0x1FF) + 800);
            work->field_23C.vz = Player_Status.coordMtx->t[2];
            work->field_254    = D_actor_103700_80139D9C[((Actor103700Spawn*)task->spawnArg2)->field_3C->rowIndex];
            work->field_252    = 5;
            func_actor_103700_80135140(task, 20);
            if ((s16)++work->field_256 >= 91) {
                work->field_256 = 0;
                work->field_24E = 3;
                work->field_250 = 0;
            }
            break;
        case 3:
            work->field_256 = D_actor_103700_80139D7C[((Gp_LcgState = Gp_LcgState * 5 + 0x71357911) >> 16) & 0xF];
            work->field_258 = 15;
            work->field_250 = 4;
            work->field_254 = 0;
            sound           = ((((Actor103700Spawn*)task->spawnArg2)->field_8 >> 12) << 8) | 0x40250003;
            SndEvt_EnqueueType6(sound, (s8)Gp_GetObjPan(obj), (s8)Gp_GetObjDepth(obj));
            break;
        case 4:
            if ((s16)--work->field_258 > 0) {
                work->field_252 = -125;
            } else {
                work->field_252 = 0;
            }
            if ((s16)--work->field_256 <= 0) {
                work->field_256 = 0;
                work->field_24E = 3;
                work->field_250 = 0;
            }
            func_actor_103700_80135140(task, 80);
            period = 21;
            break;
    }
    func_actor_103700_801350DC(task, 0, period);
}

/* Closes this unit's .rodata after the jump table above, so actor_103700_3's
   table starts at 0x80131E94. Nothing reads it. */
const u32 D_actor_103700_80131E90 = 0;

void func_actor_103700_80133AB4(Task* task)
{
    Actor103700Work* work;
    GpObj38*         obj;
    Task*            player;
    void*            head;
    GpAnimArg*       arg;
    s32              sound;
    s32              pan;

    work                    = (Actor103700Work*)task->work;
    obj                     = (GpObj38*)((TmdObject*)task->extra)->coords;
    player                  = Game_GetPtrSlot(3);
    head                    = *(void**)G_SCRATCH_HEAD;
    *(void**)G_SCRATCH_HEAD = (u8*)head - sizeof(GpAnimArg);
    arg                     = (GpAnimArg*)*(void**)G_SCRATCH_HEAD;

    switch (work->field_250) {
        case 0:
            arg->field_0  = D_actor_103700_80139F1C;
            arg->field_4  = 2;
            arg->field_8  = 0;
            arg->field_C  = 0;
            arg->field_10 = 1;
            Gp_DispatchMsg(player, 0x3F4, (s32)arg, 0);
            sound = ((((Actor103700Spawn*)task->spawnArg2)->field_8 >> 12) << 8) | 6;
            pan   = (s8)Gp_GetObjPan(obj);
            SndEvt_EnqueueType6(sound, pan, (s8)Gp_GetObjDepth(obj));
            work->field_250 = 1;
            break;
        case 1:
            if (Gp_DispatchMsg(player, 0x3ED, 0, 0) == 0) {
                Gp_DispatchMsg(player, 0x3F1, 0, 0);
                work->field_262 = 0;
                work->field_24E = 5;
                work->field_250 = 0;
            }
            break;
    }
    *(u32*)G_SCRATCH_HEAD += sizeof(GpAnimArg);
}

#include "common.h"

#include "actors/actor_121300.h"
#include "gameplay/1BC.h"
#include "gameplay/3CD8.h"
#include "gameplay/D4.h"
#include "main/fs.h"
#include "main/gameflow.h"
#include "main/gfx.h"
#include "main/mc.h"
#include "main/mem.h"
#include "main/session.h"
#include "main/task.h"
#include "main/tmd.h"

void func_actor_121300_801326EC(Task* arg0)
{
    Actor121300FadeWork* fade;
    Actor121300FadeWork* alloc;

    fade = (Actor121300FadeWork*)arg0->work;
    switch (arg0->state) {
        case 0:
            alloc      = (Actor121300FadeWork*)Mem_Malloc(8, 0);
            arg0->work = (TaskIdMap*)alloc;
            if (alloc == NULL) {
                taskKill(arg0);
                return;
            }
            fade    = alloc;
            fade->r = 0xFF;
            fade->g = 0xFF;
            fade->b = 0xFF;
            Fade_DrawOverlay((u8)fade->r, (u8)fade->g, (u8)fade->b, 2);
            goto state_inc;
        case 2:
            SetDispMask(1);
        case 1:
            Fade_DrawOverlay((u8)fade->r, (u8)fade->g, (u8)fade->b, 2);
        state_inc:
            arg0->state += 1;
            break;
        case 3:
            Fade_DrawOverlay((u8)fade->r, (u8)fade->g, (u8)fade->b, 2);
            fade->r = (s16)((u16)fade->r - (u16)arg0->spawnArg1);
            fade->g = (s16)((u16)fade->g - (u16)arg0->spawnArg1);
            fade->b = (s16)((u16)fade->b - (u16)arg0->spawnArg1);
            if ((s16)fade->r < 0) {
                taskKill(arg0);
            }
            break;
    }
}

/// `func_800B4114` is declared locally with a signed `arg2`; see `gameplay/1BC.h`.
void func_800B4114(GpAnimCtx* arg0, s32 arg1, s32 arg2, s32 arg3, s32 arg4);

/// Slot re-arm of the cutscene actor: ticks all nineteen animation slots, and
/// once every one of slots 1..18 has `field_10` bit 0x100 set ("finished"),
/// hands them the animation id `D_actor_121300_8013CC18` holds for the current
/// `field_4A0`, blending it in over ten frames.  A negative table entry leaves
/// the slots alone and only the return value follows.  The gotos reproduce
/// retail's block layout.
s32 func_actor_121300_80132818(Task* arg0)
{
    Actor121300Work* work;
    Actor121300Work* ctx;
    u16              i;
    u16              done;
    u16              anim;

    work = (Actor121300Work*)arg0->work;
    for (i = 1; i < 0x13; i++) {
        Gp_AnimTickIndex(&work->anim, i);
    }
    i    = 1;
    done = 1;
    for (; i < 0x13; i++) {
        if (!(work->slots[i].flags & 0x100)) {
            goto fail;
        }
    }
check:
    if (done) {
        if (D_actor_121300_8013CC18[work->field_4A0] >= 0) {
            anim           = D_actor_121300_8013CC18[work->field_4A0];
            ctx            = (Actor121300Work*)arg0->work;
            ctx->field_4A0 = anim;
            goto loop;
        fail:
            done = 0;
            goto check;
        loop:
            for (i = 1; i < 0x13; i++) {
                func_800B4114(&ctx->anim, i, anim, 0, 10);
            }
        }
        return 1;
    }
    return 0;
}

extern s16 D_actor_121300_8013CC04;
extern u16 D_actor_121300_8013D41C;

void func_actor_121300_8013293C(Task* arg0)
{
    Actor121300DebrisWork* work;
    TmdObject*             obj;
    GsCOORDINATE2*         coord;
    VECTOR                 pos;
    Actor121300DebrisWork* alloc;
    s16                    r;
    TmdObject*             tail;

    work  = (Actor121300DebrisWork*)arg0->work;
    obj   = arg0->extra;
    coord = obj->coords;
    if (D_actor_121300_8013D41C == 0) {
        taskKill(arg0);
        return;
    }
    switch (arg0->state) {
        case 0:
            alloc      = (Actor121300DebrisWork*)Mem_Malloc(0x5C, 0);
            arg0->work = (TaskIdMap*)alloc;
            if (alloc == NULL) {
                taskKill(arg0);
                return;
            }
            work = alloc;
            Mem_Set(work, 0, 0x5C);
            coord->sub        = &gGfxViewCoord;
            coord->coord.t[0] = D_actor_121300_8013CC20[arg0->spawnArg1].x;
            coord->coord.t[1] = D_actor_121300_8013CC20[arg0->spawnArg1].y;
            coord->coord.t[2] = D_actor_121300_8013CC20[arg0->spawnArg1].z;
            switch ((u32)arg0->spawnArg2) {
                case 0:
                case 14:
                    break;
                case 1:
                    coord->coord.t[0] += 50;
                    coord->coord.t[1] += 50;
                    break;
                case 2:
                    coord->coord.t[0] -= 50;
                    coord->coord.t[1] += 50;
                    break;
                case 3:
                    coord->coord.t[0] += 50;
                    coord->coord.t[1] -= 50;
                    break;
                case 4:
                    coord->coord.t[0] -= 50;
                    coord->coord.t[1] -= 50;
                    break;
                case 5:
                    coord->coord.t[0] += 80;
                    coord->coord.t[1] += 80;
                    break;
                case 6:
                    coord->coord.t[0] -= 80;
                    coord->coord.t[1] += 80;
                    break;
                case 7:
                    coord->coord.t[0] += 80;
                    coord->coord.t[1] -= 80;
                    break;
                case 8:
                    coord->coord.t[0] -= 80;
                    coord->coord.t[1] -= 80;
                    break;
                case 10:
                    coord->coord.t[0] += 120;
                    coord->coord.t[1] += 120;
                    break;
                case 11:
                    coord->coord.t[0] -= 120;
                    coord->coord.t[1] += 120;
                    break;
                case 12:
                    coord->coord.t[0] += 120;
                    coord->coord.t[1] -= 120;
                    break;
                case 13:
                    coord->coord.t[0] -= 120;
                    coord->coord.t[1] -= 120;
                    break;
            }
            obj->lightMtx = &work->lightMtx;
            obj->colorMtx = &work->colorMtx;

            Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
            if ((Gp_LcgState >> 16) & 1) {
                Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
                r           = ((Gp_LcgState >> 16) + 10) & 7;
            } else {
                Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
                r           = -10 - ((Gp_LcgState >> 16) & 7);
            }
            work->velX  = r;
            Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
            work->velY  = ((Gp_LcgState >> 16) & 3) + 3;

            Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
            if ((Gp_LcgState >> 16) & 1) {
                Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
                r           = ((Gp_LcgState >> 16) + 10) & 7;
            } else {
                Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
                r           = -10 - ((Gp_LcgState >> 16) & 7);
            }
            work->velZ = r;

            Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
            if ((Gp_LcgState >> 16) & 1) {
                Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
                r           = (Gp_LcgState >> 16) & 0x7F;
            } else {
                Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
                r           = -((Gp_LcgState >> 16) & 0x7F);
            }
            work->spinX = r;
            Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
            if ((Gp_LcgState >> 16) & 1) {
                Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
                r           = (Gp_LcgState >> 16) & 0x7F;
            } else {
                Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
                r           = -((Gp_LcgState >> 16) & 0x7F);
            }
            work->spinY = r;
            Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
            if ((Gp_LcgState >> 16) & 1) {
                Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
                r           = (Gp_LcgState >> 16) & 0x7F;
            } else {
                Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
                r           = -((Gp_LcgState >> 16) & 0x7F);
            }
            work->spinZ = r;
            Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
            work->delay = (Gp_LcgState >> 16) & 3;
            arg0->state++;
            break;
        case 1:
            if (work->delay == 0) {
                Tmd_AllocBuffers(obj);
                obj->flags = 0;
                arg0->state++;
            } else {
                work->delay--;
            }
            break;
        case 2:
            work->velY        += D_actor_121300_8013CC04 * 3 / 100;
            coord->coord.t[0] += work->velX * D_actor_121300_8013CC04 / 100;
            coord->coord.t[1] += work->velY * D_actor_121300_8013CC04 / 100;
            coord->coord.t[2] += work->velZ * D_actor_121300_8013CC04 / 100;
            work->rotX        += work->spinX * D_actor_121300_8013CC04 / 100;
            work->rotY        += work->spinY * D_actor_121300_8013CC04 / 100;
            work->rotZ        += work->spinZ * D_actor_121300_8013CC04 / 100;
            Gfx_RotMatrixY(&coord->coord, work->rotY, 1);
            Gfx_RotMatrixX(&coord->coord, work->rotX, 0);
            Gfx_RotMatrixZ(&coord->coord, work->rotZ, 0);
            coord->flg = 0;
            if (coord->coord.t[1] >= -499) {
                taskKill(arg0);
            }
            break;
    }
    tail   = arg0->extra;
    pos.vx = tail->coords->workm.t[0];
    pos.vy = ((TmdObject*)arg0->extra)->coords->workm.t[1];
    pos.vz = ((TmdObject*)arg0->extra)->coords->workm.t[2];
    func_800D7A9C(tail, &pos, 0, 3);
}

/// The `.align 3` pad after `func_actor_121300_8013293C`'s 15-entry jump table.
const s32 D_actor_121300_80131E60 = 0;

/// Spawns the fifteen debris variants for one waypoint, then releases this task.
void func_actor_121300_80133064(Task* task)
{
    Task* dispatch = task;
    void* alloc;

    u32 active = D_actor_121300_8013D41C;

    /* Keep the disabled path in a0. The ra dependency orders the entry
     * copy without fencing the s0 save out of the branch delay slot. */
    __asm__("" : "+r"(dispatch) : : "$31");
    if (active != 0) {
        switch (dispatch->state) {
            case 0:
                alloc          = Mem_Malloc(8, 0);
                dispatch->work = alloc;
                if (alloc != NULL) {
                    Mem_Set(alloc, 0, 8);
                    dispatch->state += 1;
                    return;
                }
                break;
            case 1:
                Task_SpawnFromTable(&ActorsShared80136280Desc, 4, dispatch->spawnArg1, 0);
                Task_SpawnFromTable(&ActorsShared80136280Desc, 5, dispatch->spawnArg1, 1);
                Task_SpawnFromTable(&ActorsShared80136280Desc, 6, dispatch->spawnArg1, 2);
                Task_SpawnFromTable(&ActorsShared80136280Desc, 7, dispatch->spawnArg1, 3);
                Task_SpawnFromTable(&ActorsShared80136280Desc, 8, dispatch->spawnArg1, 4);
                Task_SpawnFromTable(&ActorsShared80136280Desc, 4, dispatch->spawnArg1, 5);
                Task_SpawnFromTable(&ActorsShared80136280Desc, 5, dispatch->spawnArg1, 6);
                Task_SpawnFromTable(&ActorsShared80136280Desc, 6, dispatch->spawnArg1, 7);
                Task_SpawnFromTable(&ActorsShared80136280Desc, 7, dispatch->spawnArg1, 8);
                Task_SpawnFromTable(&ActorsShared80136280Desc, 8, dispatch->spawnArg1, 9);
                Task_SpawnFromTable(&ActorsShared80136280Desc, 4, dispatch->spawnArg1, 0xA);
                Task_SpawnFromTable(&ActorsShared80136280Desc, 5, dispatch->spawnArg1, 0xB);
                Task_SpawnFromTable(&ActorsShared80136280Desc, 6, dispatch->spawnArg1, 0xC);
                Task_SpawnFromTable(&ActorsShared80136280Desc, 7, dispatch->spawnArg1, 0xD);
                Task_SpawnFromTable(&ActorsShared80136280Desc, 8, dispatch->spawnArg1, 0xE);
                break;
            default:
                return;
        }
        taskKill(dispatch);
    } else {
        taskKill(task);
    }
}

#include "common.h"

#include "actors/actor_560800.h"

#include "gameplay/3CD8.h"

#include "main/gfx.h"
#include "main/mem.h"
#include "main/task.h"
#include "main/tmd.h"

#include "gameplay/D4.h"

#include <psyq/abs.h>
#include <psyq/inline_c.h>

/// `rtir` / `mvmva 1, 0, 0, 3, 0` (rtv0). The `inline_c.h` macros of those
/// names assemble to different words, so spell the instructions out.
#define gte_rtir_real() __asm__ volatile("nop; nop; .word 0x4A49E012")
#define gte_rtv0_real() __asm__ volatile("nop; nop; .word 0x4A486012")

extern TaskDesc D_actor_560800_8017575C;

void func_8017F450(GsCOORDINATE2* arg0, s32 arg1, s32 arg2, s32 arg3);
void func_actor_560800_80136AA8(Task* arg0);

/// Swings the model's chain of parts toward the named task's work block: while
/// the pieces are walked 1..5 on the scratchpad stack, each part's X rotation
/// steps by `speed` toward the heading of that block's `world` translation seen
/// from the part's joint, and the joint positions accumulate through the GTE.
/// Part 0's X rotation oscillates on a `D_actor_560800_801752E8` phase. The
/// closing switch drives `field_27E`: close enough in Y/Z starts the dip in
/// `field_24E`, which then returns to zero.
void func_actor_560800_80136AA8(Task* arg0)
{
    Actor560800ChainScratch* top;
    Actor560800ModelWork*    work;
    Actor560800ChainScratch* s;
    Actor560800PartsWork*    target;
    s16                      i;
    s16                      speed;
    s32                      a;

    top  = *(Actor560800ChainScratch**)0x1F8003FC;
    work = (Actor560800ModelWork*)arg0->work;
    s = *(Actor560800ChainScratch**)0x1F8003FC = top - 1;
    target                                     = (Actor560800PartsWork*)work->field_26C->work;
    Mem_Set(s, 0, sizeof(Actor560800ChainScratch));
    Mem_CopyUnaligned(work->rot, s->rot, sizeof(s->rot));
    if (work->field_280 & 1) {
        speed = 2;
        switch (((D_actor_560800_801752E8 + work->field_270) * 2) & 0x300) {
            case 0x0:
            case 0x300:
                s->rot[0].vx += 4;
                s->rot[0].vx %= 0x1000;
                break;
            case 0x100:
            case 0x200:
                s->rot[0].vx -= 4;
                if (s->rot[0].vx < 0) {
                    s->rot[0].vx += 0x1000;
                }
                break;
        }
    } else {
        speed = 1;
        switch (((D_actor_560800_801752E8 + work->field_270) * 2) & 0x700) {
            case 0x0:
            case 0x100:
            case 0x600:
            case 0x700:
                s->rot[0].vx += 2;
                s->rot[0].vx %= 0x1000;
                break;
            case 0x200:
            case 0x300:
            case 0x400:
            case 0x500:
                s->rot[0].vx -= 2;
                if (s->rot[0].vx < 0) {
                    s->rot[0].vx += 0x1000;
                }
                break;
        }
    }
    s->pos.vx = ((TmdObject*)arg0->extra)->coords->sub->coord.t[0] + ((TmdObject*)arg0->extra)->coords->coord.t[0];
    s->pos.vy = ((TmdObject*)arg0->extra)->coords->sub->coord.t[1] + ((TmdObject*)arg0->extra)->coords->coord.t[1];
    s->pos.vz = ((TmdObject*)arg0->extra)->coords->sub->coord.t[2] + ((TmdObject*)arg0->extra)->coords->coord.t[2];
    s->ang.vx = s->rot[0].vx;
    s->ang.vy = s->rot[0].vy;
    s->ang.vz = s->rot[0].vz;
    Gfx_RotMatrixY(&((TmdObject*)arg0->extra)->coords->coord, s->rot[0].vy, 1);
    Gfx_RotMatrixX(&((TmdObject*)arg0->extra)->coords->coord, s->rot[0].vx + 0x400, 0);
    Gfx_RotMatrixZ(&((TmdObject*)arg0->extra)->coords->coord, s->rot[0].vz, 0);
    s->link  = ((TmdObject*)arg0->extra)->coords->coord;
    s->chain = s->link;
    gte_SetRotMatrix(&s->chain);
    for (i = 1; i < 6; i++) {
        gte_ldclmv(&((TmdObject*)arg0->extra)->coords[i].coord);
        gte_rtir_real();
        gte_stclmv(&s->link);
        gte_ldclmv((char*)&((TmdObject*)arg0->extra)->coords[i].coord + 2);
        gte_rtir_real();
        gte_stclmv((char*)&s->link + 2);
        gte_ldclmv((char*)&((TmdObject*)arg0->extra)->coords[i].coord + 4);
        gte_rtir_real();
        gte_stclmv((char*)&s->link + 4);
        s->joint.vx = ((TmdObject*)arg0->extra)->coords[i + 1].coord.t[0];
        s->joint.vy = ((TmdObject*)arg0->extra)->coords[i + 1].coord.t[1];
        s->joint.vz = ((TmdObject*)arg0->extra)->coords[i + 1].coord.t[2];
        gte_SetRotMatrix(&s->link);
        gte_ldv0(&s->joint);
        gte_rtv0_real();
        gte_stsv(&s->joint);
        s->joint.vx += s->pos.vx;
        s->joint.vy += s->pos.vy;
        s->joint.vz += s->pos.vz;
        s->aim.vx    = (s->ang.vx + s->rot[i].vx) % 0x1000;
        s->aim.vy    = (s->ang.vy + s->rot[i].vy) % 0x1000;
        s->aim.vz    = (s->ang.vz + s->rot[i].vz) % 0x1000;
        if (s->rot[0].vy == 0) {
            a = ratan2(s->joint.vy - target->world.t[1], target->world.t[2] - s->joint.vz) % 0x1000;
            if (a < 0) {
                a += 0x1000;
            }
            s->aim.vx -= a;
            if (s->aim.vx < 0) {
                s->aim.vx += 0x1000;
            }
            if (s->aim.vx < 0x800) {
                s->rot[i].vx += speed;
                s->rot[i].vx %= 0x1000;
            } else {
                s->rot[i].vx -= speed;
                if (s->rot[i].vx < 0) {
                    s->rot[i].vx += 0x1000;
                }
            }
        } else {
            a = ratan2(target->world.t[1] - s->joint.vy, target->world.t[2] - s->joint.vz) % 0x1000;
            if (a < 0) {
                a += 0x1000;
            }
            s->aim.vx -= a;
            if (s->aim.vx < 0) {
                s->aim.vx += 0x1000;
            }
            if (s->aim.vx > 0x800) {
                s->rot[i].vx += speed;
                s->rot[i].vx %= 0x1000;
            } else {
                s->rot[i].vx -= speed;
                if (s->rot[i].vx < 0) {
                    s->rot[i].vx += 0x1000;
                }
            }
        }
        Gfx_RotMatrixY(&((TmdObject*)arg0->extra)->coords[i].coord, s->rot[i].vy, 1);
        Gfx_RotMatrixX(&((TmdObject*)arg0->extra)->coords[i].coord, s->rot[i].vx, 0);
        Gfx_RotMatrixZ(&((TmdObject*)arg0->extra)->coords[i].coord, s->rot[i].vz, 0);
        gte_SetRotMatrix(&s->chain);
        gte_ldclmv(&((TmdObject*)arg0->extra)->coords[i].coord);
        gte_rtir_real();
        gte_stclmv(&s->chain);
        gte_ldclmv((char*)&((TmdObject*)arg0->extra)->coords[i].coord + 2);
        gte_rtir_real();
        gte_stclmv((char*)&s->chain + 2);
        gte_ldclmv((char*)&((TmdObject*)arg0->extra)->coords[i].coord + 4);
        gte_rtir_real();
        gte_stclmv((char*)&s->chain + 4);
        s->joint.vx = ((TmdObject*)arg0->extra)->coords[i + 1].coord.t[0];
        s->joint.vy = ((TmdObject*)arg0->extra)->coords[i + 1].coord.t[1];
        s->joint.vz = ((TmdObject*)arg0->extra)->coords[i + 1].coord.t[2];
        gte_SetRotMatrix(&s->chain);
        gte_ldv0(&s->joint);
        gte_rtv0_real();
        gte_stsv(&s->joint);
        s->ang.vx += s->rot[i].vx;
        s->ang.vx %= 0x1000;
        s->ang.vy += s->rot[i].vy;
        s->ang.vy %= 0x1000;
        s->ang.vz += s->rot[i].vz;
        s->ang.vz %= 0x1000;
        s->pos.vx += s->joint.vx;
        s->pos.vy += s->joint.vy;
        s->pos.vz += s->joint.vz;
    }
    Mem_CopyUnaligned(s->rot, work->rot, sizeof(s->rot));
    switch (work->field_27E) {
        case 0:
            if (abs(s->pos.vy - target->world.t[1]) < 300) {
                if (abs(s->pos.vz - target->world.t[2]) < 200) {
                    work->field_27E = 1;
                }
            }
            break;
        case 1:
            work->field_24E -= 20;
            if (work->field_24E < -100) {
                work->field_27E = 2;
            }
            break;
        case 2:
            work->field_24E += 2;
            if (work->field_24E > 0) {
                work->field_24E = 0;
                work->field_27E = 0;
            }
            break;
    }
    *(Actor560800ChainScratch**)0x1F8003FC += 1;
}

/// Sets up the animated model part the spawn argument names: allocates its
/// `Actor560800ModelWork`, hangs it off `Task::work`, points the object's light
/// and colour matrices into it, makes the named task this one's parent and hands
/// the part to `func_800B3F84` with the overlay's animation bank. The three
/// `Gp_LcgState` draws taken along the way seed the handlers' random headings,
/// and the slot count comes from the spawner's `spawnArg1`.
void func_actor_560800_801376E0(Task* arg0)
{
    Actor560800ModelWork* mem;
    Actor560800ModelWork* work;
    TmdObject*            obj;
    GsCOORDINATE2*        coord;
    Task*                 child;

    obj        = (TmdObject*)arg0->extra;
    coord      = obj->coords;
    mem        = (Actor560800ModelWork*)Mem_Malloc(0x28C, 0);
    arg0->work = (TaskIdMap*)mem;
    if (mem == NULL) {
        taskKill(arg0);
        return;
    }
    Mem_Set(mem, 0, 0x28C);
    work            = (Actor560800ModelWork*)arg0->work;
    child           = (Task*)arg0->spawnArg2;
    work->field_26C = child;
    coord->sub      = ((TmdObject*)child->extra)->coords;
    obj->lightMtx   = &work->light;
    obj->colorMtx   = &work->color;
    Task_Reparent(work->field_26C, arg0);
    Gp_LcgState     = Gp_LcgState * 5 + 0x71357911;
    work->field_270 = Gp_LcgState >> 16;
    Gp_LcgState     = Gp_LcgState * 5 + 0x71357911;
    work->field_274 = Gp_LcgState >> 16;
    Gp_LcgState     = Gp_LcgState * 5 + 0x71357911;
    work->field_278 = (Gp_LcgState >> 16) & 0x3FF;
    func_800B3F84(&work->anim, D_actor_560800_801752F0, (GpAnimObj*)obj, work->poseBuf,
                  work->slots);
    work->field_280 = arg0->spawnArg1;
}

/// Per-frame handler of the animated model part `func_actor_560800_801376E0`
/// sets up. State 1 hides the part (`TmdObject::flags` bit 0x80) for the
/// part ids the current view excludes and otherwise runs
/// `func_actor_560800_80136AA8`; state 2 resets all seven animation slots to
/// `field_280`, state 3 ticks them, state 4 copies the coordinates of a part
/// spawned from `D_actor_560800_8017575C` and state 5 kills the task a frame
/// later. Every frame that survives rebuilds the root translation and, while
/// visible, drives `func_8017F450` and the periodic `Gp_SpawnEff`.
void func_actor_560800_80137820(Task* arg0)
{
    Actor560800ModelWork* work;
    TmdObject*            extra;
    GsCOORDINATE2*        coord;
    Actor560800ModelWork* anim;
    Task*                 child;
    s32                   i;
    u32                   tick;
    TmdObject*            obj;
    u32                   state;
    u16                   id;
    SVECTOR               unused; // never touched; only reserves the frame slot

    extra = (TmdObject*)arg0->extra;
    state = arg0->state;
    work  = (Actor560800ModelWork*)arg0->work;
    coord = extra->coords;
    obj   = extra;
    switch (state) {
        case 0:
            func_actor_560800_801376E0(arg0);
            arg0->state++;
            return;
        case 1:
            if (Gp_FindViewIndex(gGameSession->at4.loc.view) == 0x16) {
                switch (work->field_280) {
                    case 1:
                    case 3:
                        obj->flags |= 0x80;
                        return;
                    case 4 ... 0x7FFF:
                        break;
                    default:
                        func_actor_560800_80136AA8(arg0);
                        goto done;
                }
            }
            if (work->field_280 < 8) {
                if (work->field_280 >= 5) {
                    obj->flags |= 0x80;
                    return;
                }
            }
            func_actor_560800_80136AA8(arg0);
            break;
        case 2:
            if (work->field_280 < 4) {
                if (work->field_280 >= 2) {
                    obj->flags |= 0x80;
                    return;
                }
            }
            i    = 1;
            id   = work->field_280;
            anim = (Actor560800ModelWork*)arg0->work;
            do {
                anim->slots[i & 0xFFFF].field_9 = 0x10;
                Gp_AnimResetSlot(&anim->anim, i & 0xFFFF, id);
                i++;
            } while ((u32)(i & 0xFFFF) < 7U);
            arg0->state++;
            break;
        case 3:
            anim = (Actor560800ModelWork*)arg0->work;
            i    = 1;
            do {
                Gp_AnimTickIndex(&anim->anim, i & 0xFFFF);
                i++;
            } while ((u32)(i & 0xFFFF) < 7U);
            for (i = 1; (u32)(i & 0xFFFF) < 7U; i++) {
                if (!(anim->slots[i & 0xFFFF].field_10 & 0x100)) {
                    break;
                }
            }
            break;
        case 4:
            child = Task_SpawnFromTable(&D_actor_560800_8017575C, 3,
                                        ((TmdObject*)D_actor_560800_801757AC->extra)->coords->coord.t[1],
                                        (s32)arg0->spawnArg2);
            if (child == NULL) {
                arg0->state = 1;
                return;
            }
            i = 0;
            do {
                Mem_CopyUnaligned(&((TmdObject*)arg0->extra)->coords[i & 0xFFFF].coord,
                                  &((TmdObject*)child->extra)->coords[i & 0xFFFF].coord, 0x20);
                i++;
            } while ((u32)(i & 0xFFFF) < 7U);
            arg0->killCountdown = 0;
            arg0->state++;
            break;
        case 5:
            if (++arg0->killCountdown >= 2) {
                taskKill(arg0);
                return;
            }
            break;
    }
done:
    coord->coord.t[0] = work->field_254 + work->field_24C;
    coord->coord.t[1] = (s16)work->field_256 + work->field_24E;
    coord->coord.t[2] = work->field_258 + work->field_250;
    coord->flg        = 0;
    if (!(obj->flags & 0x80)) {
        func_8017F450(&((TmdObject*)arg0->extra)->coords[6], work->field_280, 0x100, 0x3C36);
        if (Gp_FindViewIndex(gGameSession->at4.loc.view) != 0x16) {
            tick = D_actor_560800_801752E8 + 1;
            if (!(tick & 0x7F) && ((tick >> 7) & 7) == work->field_280) {
                Gp_SpawnEff(0x601C6, &((TmdObject*)arg0->extra)->coords[2], 0x800, NULL);
            }
        }
    }
}

/// Per-frame handler of the model part. State 0 runs the spawner, parents the
/// root coordinate to `D_actor_560800_801757AC`'s and records its height; state
/// 1 swings parts 3-5 on X/Z by 20 between +-0x154, rebuilds their rotation
/// matrices and sinks the root, killing the task once its height passes 10000.
void func_actor_560800_80137BEC(Task* task)
{
    Actor560800ModelWork* work;
    GsCOORDINATE2*        coord;
    TmdObject*            extra;
    VECTOR                vec;
    s32                   i;
    s32                   j;
    u16                   t286;
    u16                   t288;

    work  = (Actor560800ModelWork*)task->work;
    coord = ((TmdObject*)task->extra)->coords;
    switch (task->state) {
        case 0:
            func_actor_560800_801376E0(task);
            coord->sub                       = ((TmdObject*)D_actor_560800_801757AC->extra)->coords;
            ((TmdObject*)task->extra)->flags = 0;
            work                             = (Actor560800ModelWork*)task->work;
            i                                = 1;
            do {
                work->swingDir[i & 0xFFFF] = 0;
                i                         += 1;
            } while ((u32)(i & 0xFFFF) < 6U);
            work->field_28A = coord->coord.t[1];
            coord->flg      = 0;
            task->state++;
            break;
        case 1:
            i = 3;
            do {
                if (work->swingDir[i & 0xFFFF] & 1) {
                    work->swing[i & 0xFFFF].vx += 20;
                    if (work->swing[i & 0xFFFF].vx >= 0x155) {
                        work->swingDir[i & 0xFFFF] |= 1;
                    }
                } else {
                    work->swing[i & 0xFFFF].vx -= 20;
                    if (work->swing[i & 0xFFFF].vx < -0x154) {
                        work->swingDir[i & 0xFFFF] &= 0xFFFE;
                    }
                }
                if (work->swingDir[i & 0xFFFF] & 2) {
                    work->swing[i & 0xFFFF].vz += 20;
                    if (work->swing[i & 0xFFFF].vz >= 0x155) {
                        work->swingDir[i & 0xFFFF] |= 2;
                    }
                } else {
                    work->swing[i & 0xFFFF].vz -= 20;
                    if (work->swing[i & 0xFFFF].vz < -0x154) {
                        work->swingDir[i & 0xFFFF] &= 0xFFFD;
                    }
                }
                j = i & 0xFFFF;
                Gfx_RotMatrixY(&((TmdObject*)task->extra)->coords[j].coord, work->rot[j].vy, 1);
                Gfx_RotMatrixX(&((TmdObject*)task->extra)->coords[j].coord,
                               work->rot[j].vx + work->swing[j].vx, 0);
                Gfx_RotMatrixZ(&((TmdObject*)task->extra)->coords[j].coord,
                               work->rot[j].vz + work->swing[j].vz, 0);
                i += 1;
            } while ((u32)(i & 0xFFFF) < 6U);
            t286              = work->field_286 + 4;
            t288              = work->field_288 + t286;
            work->field_288   = t288;
            work->field_286   = t286;
            coord->coord.t[1] = work->field_28A + task->spawnArg1 -
                                ((TmdObject*)D_actor_560800_801757AC->extra)->coords->coord.t[1] +
                                (s16)t288;
            coord->flg = 0;
            if (coord->coord.t[1] > 10000) {
                taskKill(task);
                return;
            }
            break;
    }
    extra  = (TmdObject*)task->extra;
    vec.vx = extra->coords->workm.t[0];
    vec.vy = ((TmdObject*)task->extra)->coords->workm.t[1];
    vec.vz = ((TmdObject*)task->extra)->coords->workm.t[2];
    func_800D7A9C(extra, &vec, 0, 3);
}

/// Message handler of the parts task that loads a pose table into all eight
/// parts, chosen by `Actor560800PartsWork::field_46`: 0-2 place each part at
/// its table position and, while `field_48` is set, rebuild its rotation from
/// the table with the child joints reset; 3 resets each part's matrix, hangs
/// it off `Gfx_ViewCoord` and offsets it from the message position; 4 kills
/// parts 4-7, reparents the rest to `D_actor_560800_801757AC`'s model and
/// raises the `D_actor_560800_801752E8` / `801752EC` flags.
void func_actor_560800_80137F58(Task* task, s32 msgId, VECTOR* msg)
{
    Actor560800PartsWork* work;
    u16                   flag;
    Actor560800PartPose*  pose;
    Actor560800ModelWork* part;
    GsCOORDINATE2*        coord;
    Actor560800MatWords*  mat;
    s32                   i;
    s32                   j;

    work = (Actor560800PartsWork*)task->work;
    flag = 0;
    switch (work->field_46) {
        case 0:
            pose = D_actor_560800_80175314;
            break;
        case 1:
            pose = D_actor_560800_801753D4;
            break;
        case 2:
            pose = D_actor_560800_80175494;
            flag = 1;
            break;
        case 3:
            pose = D_actor_560800_80175554;
            i    = 0;
            do {
                if (work->parts[i & 0xFFFF] != NULL) {
                    coord              = ((TmdObject*)work->parts[i & 0xFFFF]->extra)->coords;
                    mat                = (Actor560800MatWords*)&coord->coord;
                    mat->ident.m00_m01 = 0x1000;
                    mat->ident.m02_m10 = 0;
                    mat->ident.m11_m12 = 0x1000;
                    mat->ident.m20_m21 = 0;
                    mat->ident.m22     = 0x1000;
                    coord->sub         = &Gfx_ViewCoord;
                    part               = (Actor560800ModelWork*)work->parts[i & 0xFFFF]->work;
                    part->field_254    = msg->vx + pose->pos.vx;
                    part->field_256    = msg->vy + pose->pos.vy;
                    part->field_258    = msg->vz + pose->pos.vz;
                    part->field_24C    = 0;
                    part->field_24E    = 0;
                    part->field_250    = 0;
                }
                i++;
                pose++;
            } while ((u32)(i & 0xFFFF) < 8U);
            return;
        case 4:
            i = 0;
            do {
                if ((i & 0xFFFF) >= 4U) {
                    taskKill(work->parts[i & 0xFFFF]);
                    work->parts[i & 0xFFFF] = NULL;
                }
                i++;
            } while ((u32)(i & 0xFFFF) < 8U);
            pose = D_actor_560800_80175614;
            i    = 0;
            do {
                if (work->parts[i & 0xFFFF] != NULL) {
                    part            = (Actor560800ModelWork*)work->parts[i & 0xFFFF]->work;
                    coord           = ((TmdObject*)work->parts[i & 0xFFFF]->extra)->coords;
                    coord->sub      = ((TmdObject*)D_actor_560800_801757AC->extra)->coords;
                    part->field_254 = pose->pos.vx;
                    part->field_256 = pose->pos.vy;
                    part->field_258 = pose->pos.vz;
                    Gfx_RotMatrixY(&coord->coord, pose->rot.vy, 1);
                    Gfx_RotMatrixX(&coord->coord, pose->rot.vx + 0x400, 0);
                    Gfx_RotMatrixZ(&coord->coord, pose->rot.vz, 0);
                    part->rot[0].vx = pose->rot.vx;
                    part->rot[0].vy = pose->rot.vy;
                    part->rot[0].vz = pose->rot.vz;
                    part->field_27C = flag;
                    j               = 1;
                    do {
                        Gfx_RotMatrixY(&((TmdObject*)work->parts[i & 0xFFFF]->extra)->coords[j & 0xFFFF].coord, 0, 1);
                        Gfx_RotMatrixX(&((TmdObject*)work->parts[i & 0xFFFF]->extra)->coords[j & 0xFFFF].coord, 0, 0);
                        Gfx_RotMatrixZ(&((TmdObject*)work->parts[i & 0xFFFF]->extra)->coords[j & 0xFFFF].coord, 0, 0);
                        part->rot[j & 0xFFFF].vx = 0;
                        part->rot[j & 0xFFFF].vy = 0;
                        part->rot[j & 0xFFFF].vz = 0;
                        j++;
                    } while ((u32)(j & 0xFFFF) < 7U);
                    coord->flg = 0;
                }
                i++;
                pose++;
            } while ((u32)(i & 0xFFFF) < 8U);
            work->field_46          = 0;
            D_actor_560800_801752E8 = 1;
            D_actor_560800_801752EC = 1;
            return;
    }
    i = 0;
    do {
        if (work->parts[i & 0xFFFF] != NULL) {
            part                                          = (Actor560800ModelWork*)work->parts[i & 0xFFFF]->work;
            coord                                         = ((TmdObject*)work->parts[i & 0xFFFF]->extra)->coords;
            ((TmdObject*)task->extra)->coords->coord.t[0] = msg->vx;
            ((TmdObject*)task->extra)->coords->coord.t[1] = msg->vy;
            ((TmdObject*)task->extra)->coords->coord.t[2] = msg->vz;
            ((TmdObject*)task->extra)->coords->flg        = 0;
            part->field_254                               = pose->pos.vx;
            part->field_256                               = pose->pos.vy;
            part->field_258                               = pose->pos.vz;
            part->field_27C                               = flag;
            if (work->field_48 != 0) {
                Gfx_RotMatrixY(&coord->coord, pose->rot.vy, 1);
                Gfx_RotMatrixX(&coord->coord, pose->rot.vx + 0x400, 0);
                Gfx_RotMatrixZ(&coord->coord, pose->rot.vz, 0);
                part->rot[0].vx = pose->rot.vx;
                part->rot[0].vy = pose->rot.vy;
                part->rot[0].vz = pose->rot.vz;
                j               = 1;
                do {
                    Gfx_RotMatrixY(&((TmdObject*)work->parts[i & 0xFFFF]->extra)->coords[j & 0xFFFF].coord, 0, 1);
                    Gfx_RotMatrixX(&((TmdObject*)work->parts[i & 0xFFFF]->extra)->coords[j & 0xFFFF].coord, 0, 0);
                    Gfx_RotMatrixZ(&((TmdObject*)work->parts[i & 0xFFFF]->extra)->coords[j & 0xFFFF].coord, 0, 0);
                    part->rot[j & 0xFFFF].vx = 0;
                    part->rot[j & 0xFFFF].vy = 0;
                    part->rot[j & 0xFFFF].vz = 0;
                    j++;
                } while ((u32)(j & 0xFFFF) < 7U);
            }
            coord->flg = 0;
        }
        i++;
        pose++;
    } while ((u32)(i & 0xFFFF) < 8U);
    work->field_48 = 0;
    work->field_46 = 0;
}

/// Message handler of the parts task (`D_actor_560800_801756D4`): command 0
/// rebuilds each part's colour matrix from its world translation, 5 and 6 put
/// all eight parts into state 2 / 1, and the rest set this task's state and the
/// `Actor560800PartsWork` halfwords at 0x44-0x4A.
void func_actor_560800_801384EC(Task* task, s32 msgId, Actor560800Msg* msg)
{
    Actor560800PartsWork* work;
    Task*                 part;
    TmdObject*            extra;
    VECTOR                vec;
    s32                   i;

    work = (Actor560800PartsWork*)task->work;
    switch (msg->field_2) {
        case 0:
            i = 0;
            do {
                part = work->parts[i & 0xFFFF];
                if (part != NULL) {
                    extra  = (TmdObject*)part->extra;
                    vec.vx = extra->coords->workm.t[0];
                    vec.vy = ((TmdObject*)part->extra)->coords->workm.t[1];
                    vec.vz = ((TmdObject*)part->extra)->coords->workm.t[2];
                    func_800D7A9C(extra, &vec, 0, 3);
                }
                i += 1;
            } while ((u32)(i & 0xFFFF) < 8U);
            break;
        case 1:
            work->field_46 = 1;
            work->field_48 = 1;
            work->field_4A = 0x83;
            break;
        case 2:
            task->state    = 2;
            work->field_44 = 0;
            break;
        case 3:
            task->state    = 1;
            work->field_46 = 2;
            break;
        case 4:
            work->field_46 = 0;
            work->field_48 = 1;
            break;
        case 5:
            task->state    = 1;
            work->field_46 = 3;
            i              = 0;
            do {
                work->parts[i & 0xFFFF]->state = 2;
                i                             += 1;
            } while ((u32)(i & 0xFFFF) < 8U);
            break;
        case 6:
            task->state    = 1;
            work->field_46 = 4;
            i              = 0;
            do {
                work->parts[i & 0xFFFF]->state = 1;
                i                             += 1;
            } while ((u32)(i & 0xFFFF) < 8U);
            break;
        case 7:
            task->state    = 1;
            work->field_46 = 4;
            work->field_4A = 0x22;
            break;
        case 8:
            task->state = 3;
            break;
    }
}

extern s32  D_80115738;
extern void D_actor_560800_801756D4;
extern u16  D_actor_560800_801756EC[];

/// Handler of the parts task. State 0 allocates its `Actor560800PartsWork`,
/// roots the model at `Gfx_ViewCoord`, reparents the spawner's task, spawns the
/// eight part tasks and swaps `Gp_LcgState` out for a zero seed; state 2 grows
/// each part's `field_256` up to its `D_actor_560800_80175314` limit; state 3
/// bursts effects on the first remaining part, puts it into state 4 and drops
/// it. Every frame the world position follows part 9 of the controller model
/// `field_4A` selects.
void func_actor_560800_801386D4(Task* task)
{
    Actor560800PartsWork* work;
    Actor560800PartsWork* w;
    Actor560800PartsWork* spawned;
    Actor560800PartsWork* grow;
    Actor560800ModelWork* model;
    GsCOORDINATE2*        root;
    GsCOORDINATE2*        partCoord;
    GsCOORDINATE2*        effCoord;
    GsCOORDINATE2*        c;
    Task*                 part;
    SVECTOR               pos;
    s32                   i;
    s32                   n;
    s16                   k;

    work = (Actor560800PartsWork*)task->work;
    switch (task->state) {
        case 0:
            root       = ((TmdObject*)task->extra)->coords;
            w          = (Actor560800PartsWork*)Mem_Malloc(0x4C, 0);
            task->work = (TaskIdMap*)w;
            if (w == NULL) {
                taskKill(task);
            } else {
                root->sub = &Gfx_ViewCoord;
                Mem_Set(task->work, 0, 0x4C);
                i                 = 0;
                spawned           = w;
                spawned->field_40 = (Task*)task->spawnArg2;
                task->msgTable    = &D_actor_560800_801756D4;
                Task_Reparent(spawned->field_40, task);
                do {
                    spawned->parts[i & 0xFFFF] =
                        Task_SpawnFromTable(&D_actor_560800_8017575C, 1, (i & 0xFFFF) + 1, (s32)task);
                    i++;
                } while ((u32)(i & 0xFFFF) < 8U);
                D_actor_560800_801757A8 = Gp_LcgState;
                Gp_LcgState             = 0;
            }
            task->state++;
            break;
        case 1:
            break;
        case 2:
            grow = work;
            n    = 0;
            do {
                part = grow->parts[n & 0xFFFF];
                if (part != NULL) {
                    model             = (Actor560800ModelWork*)part->work;
                    partCoord         = ((TmdObject*)part->extra)->coords;
                    model->field_256 += D_actor_560800_801756EC[n & 0xFFFF];
                    if (D_actor_560800_80175314[n & 0xFFFF].pos.vy < (s16)model->field_256) {
                        model->field_256 = D_actor_560800_80175314[n & 0xFFFF].pos.vy;
                    }
                    partCoord->flg = 0;
                }
                n++;
            } while ((u32)(n & 0xFFFF) < 8U);
            break;
        case 3:
            for (k = 0; k < 8; k++) {
                if (work->parts[k] != NULL) {
                    effCoord = &((TmdObject*)work->parts[k]->extra)->coords[3];
                    Gp_SpawnEff(D_80115738, effCoord, 0x10002380, 0);
                    Gp_SpawnEff(D_80115738, effCoord, 0x04003480, 0);
                    i = 0;
                    do {
                        Gp_SpawnEff(D_80115738, effCoord, 0x02002400, 0);
                        i++;
                        Gp_SpawnEff(0x601B4, effCoord, 0x02202300, 0);
                    } while ((u32)(i & 0xFFFF) < 4U);
                    work->parts[k]->state = 4;
                    work->parts[k]        = NULL;
                    break;
                }
            }
            task->state = 1;
            break;
    }
    w = (Actor560800PartsWork*)task->work;
    if (w->field_4A == 0x83) {
        c = ((TmdObject*)((Actor560800Work*)w->field_40->work)->field_4->extra)->coords;
        Gp_ComposeParentWorld(&c[9], &w->world, &pos);
    } else if (w->field_4A == 0x22) {
        c = ((TmdObject*)((Actor560800Work*)w->field_40->work)->field_C->extra)->coords;
        Gp_ComposeParentWorld(&c[9], &w->world, &pos);
    }
    w->world.t[0] = pos.vx;
    w->world.t[1] = pos.vy - 0x78;
    w->world.t[2] = pos.vz;
}

extern s32 D_actor_560800_801756FC[];
extern s32 D_actor_560800_80175714[];
extern s32 D_actor_560800_8017572C[];

void func_actor_560800_80138A4C(Task* task, s32 msgId, Actor560800Msg* msg)
{
    Actor560800ModelWork* work;
    TmdObject*            extra;
    VECTOR                vec;

    work = (Actor560800ModelWork*)task->work;
    switch (msg->field_2) {
        case 0:
            extra  = (TmdObject*)task->extra;
            vec.vx = extra->coords->workm.t[0];
            vec.vy = ((TmdObject*)task->extra)->coords->workm.t[1];
            vec.vz = ((TmdObject*)task->extra)->coords->workm.t[2];
            func_800D7A9C(extra, &vec, 0, 3);
            break;
        case 1:
            task->state = 1;
            break;
        case 2:
            Gp_DispatchMsg(task, 0x7D5, 1, 0);
            Gp_DispatchMsg(task, 0x7D4, (s32)D_actor_560800_801756FC, 0);
            work->field_278 = 0x1000;
            task->state     = 2;
            break;
        case 3:
            task->state = 3;
            break;
        case 4:
            task->state = 4;
            break;
        case 5:
            Gp_DispatchMsg(task, 0x7D5, 1, 0);
            Gp_DispatchMsg(task, 0x7D4, (s32)D_actor_560800_80175714, 0);
            work->field_278 = 0x1000;
            task->state     = 5;
            break;
        case 6:
            Gp_DispatchMsg(task, 0x7D5, 1, 0);
            Gp_DispatchMsg(task, 0x7D4, (s32)D_actor_560800_8017572C, 0);
            work->field_282 = 0;
            task->state     = 6;
            break;
    }
}

/// Per-frame pulse of the model part: while bit 0 of
/// `D_actor_560800_801752E8` is set it raises `field_286`, which sinks the root
/// coordinate. Once `field_278` has reached 0x800 the second coordinate is reset
/// to identity and scaled on X/Z by `field_278`, which swings between 0x1000 and
/// 0x1800 in steps of 0x32 with `field_27C` as the direction. The dead `w = work`
/// store is what the match needs: see DECOMPILATION_LEARNINGS.md, "birthing".
void func_actor_560800_80138BCC(Task* task)
{
    Actor560800ModelWork* work;
    GsCOORDINATE2*        coord;
    GsCOORDINATE2*        c;
    Actor560800ModelWork* w;
    MATRIX*               m;
    VECTOR                scale;

    work  = (Actor560800ModelWork*)task->work;
    coord = ((TmdObject*)task->extra)->coords;
    if (D_actor_560800_801752E8 & 1) {
        work->field_286++;
    }
    w                  = work;
    coord->coord.t[1] -= work->field_286;
    if (work->field_278 >= 0x800) {
        work->field_27C    = 0;
        w                  = (Actor560800ModelWork*)task->work;
        c                  = ((TmdObject*)task->extra)->coords;
        m                  = &c[1].coord;
        *(s32*)&m->m[0][0] = 0x1000;
        *(s32*)&m->m[0][2] = 0;
        *(s32*)&m->m[1][1] = 0x1000;
        *(s32*)&m->m[2][0] = 0;
        m->m[2][2]         = 0x1000;
        c++;
        if (w->field_27C == 0) {
            w->field_278 -= 0x32;
            if (w->field_278 < 0x1000) {
                w->field_27C = 1;
            }
        } else if (w->field_27C == 1) {
            w->field_278 += 0x32;
            if (w->field_278 > 0x1800) {
                w->field_27C = 0;
            }
        }
        scale.vx = w->field_278;
        scale.vy = 0x1000;
        scale.vz = w->field_278;
        ScaleMatrix(&c->coord, &scale);
    }
    coord->flg = 0;
}

/// Per-frame rise of the model part, driven by `field_282`: phase 0 lifts the
/// root coordinate until it clears -3000, phase 1 keeps lifting while pulsing
/// the second coordinate's X/Z scale in steps of 0x32 until -1200, and phase 2
/// pulses in steps of 0xC8 until `field_278` drops below 0x1000. Phase 3 sinks
/// this part and the one `Actor560800Work::field_C` names together. Each case
/// needs its own matrix pointer: a shared one is set twice, loses sched1's
/// birthing priority, and swaps the `work`/`field_8` loads.
void func_actor_560800_80138D04(Task* task)
{
    Actor560800ModelWork* work;
    GsCOORDINATE2*        coord;
    GsCOORDINATE2*        c;
    GsCOORDINATE2*        other;
    Actor560800ModelWork* w;
    MATRIX*               m;
    MATRIX*               m2;
    VECTOR                scale;
    s32                   one;

    work  = (Actor560800ModelWork*)task->work;
    coord = ((TmdObject*)task->extra)->coords;
    switch (work->field_282) {
        case 0:
            if (coord->coord.t[1] >= -3000) {
                work->field_282++;
            }
            break;
        case 1:
            if (work->field_278 <= 0x1800) {
                work->field_27C    = 1;
                w                  = (Actor560800ModelWork*)task->work;
                c                  = ((TmdObject*)task->extra)->coords;
                m                  = &c[1].coord;
                one                = 0x1000;
                *(s32*)&m->m[0][0] = one;
                *(s32*)&m->m[0][2] = 0;
                *(s32*)&m->m[1][1] = one;
                *(s32*)&m->m[2][0] = 0;
                m->m[2][2]         = one;
                c++;
                if (w->field_27C == 0) {
                    w->field_278 -= 0x32;
                    if (w->field_278 < one) {
                        w->field_27C = 1;
                    }
                } else if (w->field_27C == 1) {
                    w->field_278 += 0x32;
                    if (w->field_278 > 0x1800) {
                        w->field_27C = 0;
                    }
                }
                scale.vx = w->field_278;
                scale.vy = 0x1000;
                scale.vz = w->field_278;
                ScaleMatrix(&c->coord, &scale);
            }
            if (coord->coord.t[1] >= -1200) {
                work->field_282++;
            }
            break;
        case 2:
            if (work->field_278 >= 0x1000) {
                work->field_27C     = 0;
                w                   = (Actor560800ModelWork*)task->work;
                c                   = ((TmdObject*)task->extra)->coords;
                m2                  = &c[1].coord;
                one                 = 0x1000;
                *(s32*)&m2->m[0][0] = one;
                *(s32*)&m2->m[0][2] = 0;
                *(s32*)&m2->m[1][1] = one;
                *(s32*)&m2->m[2][0] = 0;
                m2->m[2][2]         = one;
                c++;
                if (w->field_27C == 0) {
                    w->field_278 -= 0xC8;
                    if (w->field_278 < one) {
                        w->field_27C = 1;
                    }
                } else if (w->field_27C == 1) {
                    w->field_278 += 0xC8;
                    if (w->field_278 > 0x1800) {
                        w->field_27C = 0;
                    }
                }
                scale.vx = w->field_278;
                scale.vy = 0x1000;
                scale.vz = w->field_278;
                ScaleMatrix(&c->coord, &scale);
            } else {
                work->field_282++;
            }
            coord->flg = 0;
            return;
        case 3:
            other              = ((TmdObject*)((Actor560800Work*)((Task*)task->spawnArg2)->work)->field_C->extra)->coords;
            coord->coord.t[1] -= 20;
            other->coord.t[1] -= 20;
            coord->flg         = 0;
            other->flg         = 0;
            return;
    }
    coord->flg         = 0;
    coord->coord.t[1] += 100;
}

/// Per-frame handler of a model task: state 0 allocates its
/// `Actor560800ModelWork`, parents the root coordinate to `Gfx_ViewCoord`,
/// publishes the task as `D_actor_560800_801757AC` and resets the root matrix
/// to identity. States 2/5 lift the root
/// by 5 while pulsing the second coordinate's X/Z scale in steps of 0x32, state
/// 3 by 1 in steps of 0xA; 4 and 6 hand off to `func_actor_560800_80138BCC` /
/// `func_actor_560800_80138D04`. Every state but 0 advances the two frame
/// counters. Each case needs its own matrix pointer (and case 0 its own work
/// pointer): a pointer shared across cases is a global pseudo, so the local
/// 0x1000 constant takes `$v0` from it.
extern void D_actor_560800_80175744;

void func_actor_560800_80138FC8(Task* task)
{
    Actor560800ModelWork* work;
    Actor560800ModelWork* w;
    Actor560800ModelWork* mem;
    TmdObject*            obj;
    GsCOORDINATE2*        coord;
    GsCOORDINATE2*        c;
    MATRIX*               m0;
    MATRIX*               m2;
    MATRIX*               m3;
    MATRIX*               m5;
    VECTOR                scale;
    GsCOORDINATE2*        root;

    switch (task->state) {
        case 0:
            obj        = (TmdObject*)task->extra;
            root       = obj->coords;
            task->work = Mem_Malloc(0x28C, 0);
            if (task->work == NULL) {
                taskKill(task);
            } else {
                Mem_Set(task->work, 0, 0x28C);
                mem            = (Actor560800ModelWork*)task->work;
                root->sub      = &Gfx_ViewCoord;
                mem->field_26C = (Task*)task->spawnArg2;
                obj->lightMtx  = &mem->light;
                obj->colorMtx  = &mem->color;
                Task_Reparent((Task*)task->spawnArg2, task);
                task->msgTable          = &D_actor_560800_80175744;
                D_actor_560800_801757AC = task;
                m0                      = &root->coord;
                *(s32*)&m0->m[0][0]     = 0x1000;
                *(s32*)&m0->m[0][2]     = 0;
                *(s32*)&m0->m[1][1]     = 0x1000;
                *(s32*)&m0->m[2][0]     = 0;
                m0->m[2][2]             = 0x1000;
            }
            task->state++;
            return;
        case 1:
            ((TmdObject*)task->extra)->flags |= 0x80;
            break;
        case 2:
            coord              = ((TmdObject*)task->extra)->coords;
            work               = (Actor560800ModelWork*)task->work;
            coord->coord.t[1] += 5;
            if (work->field_278 <= 0x1800) {
                work->field_27C     = 1;
                c                   = ((TmdObject*)task->extra)->coords;
                w                   = (Actor560800ModelWork*)task->work;
                m2                  = &c[1].coord;
                *(s32*)&m2->m[0][0] = 0x1000;
                *(s32*)&m2->m[0][2] = 0;
                *(s32*)&m2->m[1][1] = 0x1000;
                *(s32*)&m2->m[2][0] = 0;
                m2->m[2][2]         = 0x1000;
                c++;
                if (w->field_27C == 0) {
                    w->field_278 -= 0x32;
                    if (w->field_278 < 0x1000) {
                        w->field_27C = 1;
                    }
                } else if (w->field_27C == 1) {
                    w->field_278 += 0x32;
                    if (w->field_278 > 0x1800) {
                        w->field_27C = 0;
                    }
                }
                scale.vx = w->field_278;
                scale.vy = 0x1000;
                scale.vz = w->field_278;
                ScaleMatrix(&c->coord, &scale);
            }
            coord->flg = 0;
            break;
        case 3:
            coord              = ((TmdObject*)task->extra)->coords;
            work               = (Actor560800ModelWork*)task->work;
            coord->coord.t[1] += 1;
            if (work->field_278 >= 0x800) {
                work->field_27C     = 0;
                c                   = ((TmdObject*)task->extra)->coords;
                w                   = (Actor560800ModelWork*)task->work;
                m3                  = &c[1].coord;
                *(s32*)&m3->m[0][0] = 0x1000;
                *(s32*)&m3->m[0][2] = 0;
                *(s32*)&m3->m[1][1] = 0x1000;
                *(s32*)&m3->m[2][0] = 0;
                m3->m[2][2]         = 0x1000;
                c++;
                if (w->field_27C == 0) {
                    w->field_278 -= 0xA;
                    if (w->field_278 < 0x1000) {
                        w->field_27C = 1;
                    }
                } else if (w->field_27C == 1) {
                    w->field_278 += 0xA;
                    if (w->field_278 > 0x1800) {
                        w->field_27C = 0;
                    }
                }
                scale.vx = w->field_278;
                scale.vy = 0x1000;
                scale.vz = w->field_278;
                ScaleMatrix(&c->coord, &scale);
            }
            coord->flg = 0;
            break;
        case 4:
            func_actor_560800_80138BCC(task);
            break;
        case 5:
            coord              = ((TmdObject*)task->extra)->coords;
            work               = (Actor560800ModelWork*)task->work;
            coord->coord.t[1] += 5;
            if (work->field_278 <= 0x1800) {
                work->field_27C     = 1;
                c                   = ((TmdObject*)task->extra)->coords;
                w                   = (Actor560800ModelWork*)task->work;
                m5                  = &c[1].coord;
                *(s32*)&m5->m[0][0] = 0x1000;
                *(s32*)&m5->m[0][2] = 0;
                *(s32*)&m5->m[1][1] = 0x1000;
                *(s32*)&m5->m[2][0] = 0;
                m5->m[2][2]         = 0x1000;
                c++;
                if (w->field_27C == 0) {
                    w->field_278 -= 0x32;
                    if (w->field_278 < 0x1000) {
                        w->field_27C = 1;
                    }
                } else if (w->field_27C == 1) {
                    w->field_278 += 0x32;
                    if (w->field_278 > 0x1800) {
                        w->field_27C = 0;
                    }
                }
                scale.vx = w->field_278;
                scale.vy = 0x1000;
                scale.vz = w->field_278;
                ScaleMatrix(&c->coord, &scale);
            }
            coord->flg = 0;
            break;
        case 6:
            func_actor_560800_80138D04(task);
            break;
    }
    D_actor_560800_801752E8 += 2;
    D_actor_560800_801752EC += 1;
}

/// Message 0x7D5 handler of the task `D_actor_560800_801756D4` belongs to: the
/// visibility switch `func_actor_560800_801393EC` performs on a single model,
/// applied to every part task its `Actor560800PartsWork` still holds. `arg2` is
/// the sub-command - 1 clears the 0x84 pair of bits in the part's
/// `TmdObject::flags` and 2 sets it, anything else leaves the parts alone.
void func_actor_560800_80139360(Task* task, s32 arg1, s32 arg2)
{
    Actor560800PartsWork* work;
    TmdObject*            obj;
    Task*                 part;
    s32                   i;

    work = (Actor560800PartsWork*)task->work;
    i    = 0;
    do {
        part = work->parts[i & 0xFFFF];
        if (part != NULL) {
            obj = (TmdObject*)part->extra;
            switch (arg2) {
                case 0:
                    break;
                case 1:
                    obj->flags = obj->flags & 0xFF7B;
                    break;
                case 2:
                    obj->flags = obj->flags | 0x84;
                    break;
            }
        }
        i += 1;
    } while ((u32)(i & 0xFFFF) < 8U);
}

void func_actor_560800_801393EC(Task* task, s32 arg1, s32 arg2)
{
    TmdObject* extra;

    extra = (TmdObject*)task->extra;
    switch (arg2) {
        case 0:
            break;
        case 1:
            extra->flags = extra->flags & 0xFF7B;
            return;
        case 2:
            extra->flags = extra->flags | 0x84;
            return;
    }
}

INCLUDE_ASM("actors/nonmatchings/actor_560800/actor_560800_4", func_actor_560800_80139440);

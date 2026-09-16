#include "common.h"

#include "actors/actor_560800.h"

#include "main/mem.h"
#include "main/task.h"
#include "main/tmd.h"

INCLUDE_ASM("actors/nonmatchings/actor_560800/actor_560800_4", func_actor_560800_80136AA8);

/// Sets up the animated model part the spawn argument names: allocates its
/// `Actor560800ModelWork`, hangs it off `Task::idMap`, points the object's light
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

    obj         = (TmdObject*)arg0->extra;
    coord       = obj->field_8;
    mem         = (Actor560800ModelWork*)Mem_Malloc(0x28C, 0);
    arg0->idMap = (TaskIdMap*)mem;
    if (mem == NULL) {
        Task_Kill(arg0);
        return;
    }
    Mem_Set(mem, 0, 0x28C);
    work            = (Actor560800ModelWork*)arg0->idMap;
    child           = (Task*)arg0->spawnArg2;
    work->field_26C = child;
    coord->sub      = ((TmdObject*)child->extra)->field_8;
    obj->field_1C   = &work->light;
    obj->field_20   = &work->color;
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

INCLUDE_ASM("actors/nonmatchings/actor_560800/actor_560800_4", func_actor_560800_80137820);

INCLUDE_ASM("actors/nonmatchings/actor_560800/actor_560800_4", func_actor_560800_80137BEC);

INCLUDE_ASM("actors/nonmatchings/actor_560800/actor_560800_4", func_actor_560800_80137F58);

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

    work = (Actor560800PartsWork*)task->idMap;
    switch (msg->field_2) {
        case 0:
            i = 0;
            do {
                part = work->parts[i & 0xFFFF];
                if (part != NULL) {
                    extra  = (TmdObject*)part->extra;
                    vec.vx = extra->field_8->workm.t[0];
                    vec.vy = ((TmdObject*)part->extra)->field_8->workm.t[1];
                    vec.vz = ((TmdObject*)part->extra)->field_8->workm.t[2];
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

INCLUDE_ASM("actors/nonmatchings/actor_560800/actor_560800_4", func_actor_560800_801386D4);

extern s32 D_actor_560800_801756FC[];
extern s32 D_actor_560800_80175714[];
extern s32 D_actor_560800_8017572C[];

void func_actor_560800_80138A4C(Task* task, s32 msgId, Actor560800Msg* msg)
{
    Actor560800ModelWork* work;
    TmdObject*            extra;
    VECTOR                vec;

    work = (Actor560800ModelWork*)task->idMap;
    switch (msg->field_2) {
        case 0:
            extra  = (TmdObject*)task->extra;
            vec.vx = extra->field_8->workm.t[0];
            vec.vy = ((TmdObject*)task->extra)->field_8->workm.t[1];
            vec.vz = ((TmdObject*)task->extra)->field_8->workm.t[2];
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

    work  = (Actor560800ModelWork*)task->idMap;
    coord = ((TmdObject*)task->extra)->field_8;
    if (D_actor_560800_801752E8 & 1) {
        work->field_286++;
    }
    w                  = work;
    coord->coord.t[1] -= work->field_286;
    if (work->field_278 >= 0x800) {
        work->field_27C    = 0;
        w                  = (Actor560800ModelWork*)task->idMap;
        c                  = ((TmdObject*)task->extra)->field_8;
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
/// birthing priority, and swaps the `idMap`/`field_8` loads.
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

    work  = (Actor560800ModelWork*)task->idMap;
    coord = ((TmdObject*)task->extra)->field_8;
    switch (work->field_282) {
        case 0:
            if (coord->coord.t[1] >= -3000) {
                work->field_282++;
            }
            break;
        case 1:
            if (work->field_278 <= 0x1800) {
                work->field_27C    = 1;
                w                  = (Actor560800ModelWork*)task->idMap;
                c                  = ((TmdObject*)task->extra)->field_8;
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
                w                   = (Actor560800ModelWork*)task->idMap;
                c                   = ((TmdObject*)task->extra)->field_8;
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
            other              = ((TmdObject*)((Actor560800Work*)((Task*)task->spawnArg2)->idMap)->field_C->extra)->field_8;
            coord->coord.t[1] -= 20;
            other->coord.t[1] -= 20;
            coord->flg         = 0;
            other->flg         = 0;
            return;
    }
    coord->flg         = 0;
    coord->coord.t[1] += 100;
}

INCLUDE_ASM("actors/nonmatchings/actor_560800/actor_560800_4", func_actor_560800_80138FC8);

/// Message 0x7D5 handler of the task `D_actor_560800_801756D4` belongs to: the
/// visibility switch `func_actor_560800_801393EC` performs on a single model,
/// applied to every part task its `Actor560800PartsWork` still holds. `arg2` is
/// the sub-command - 1 clears the 0x84 pair of bits in the part's
/// `TmdObject::field_C` and 2 sets it, anything else leaves the parts alone.
void func_actor_560800_80139360(Task* task, s32 arg1, s32 arg2)
{
    Actor560800PartsWork* work;
    TmdObject*            obj;
    Task*                 part;
    s32                   i;

    work = (Actor560800PartsWork*)task->idMap;
    i    = 0;
    do {
        part = work->parts[i & 0xFFFF];
        if (part != NULL) {
            obj = (TmdObject*)part->extra;
            switch (arg2) {
                case 0:
                    break;
                case 1:
                    obj->field_C = obj->field_C & 0xFF7B;
                    break;
                case 2:
                    obj->field_C = obj->field_C | 0x84;
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
            extra->field_C = extra->field_C & 0xFF7B;
            return;
        case 2:
            extra->field_C = extra->field_C | 0x84;
            return;
    }
}

INCLUDE_ASM("actors/nonmatchings/actor_560800/actor_560800_4", func_actor_560800_80139440);

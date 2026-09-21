#include "common.h"

#include "actors/actor_323400.h"
#include "gameplay/3CD8.h"
#include "main/mem.h"
#include "main/session.h"
#include "main/sound.h"

/// Per-frame effect dispatch keyed on `field_82E` and the record each animation
/// slot has reached. A recognised record is handled once: `field_848` remembers,
/// per slot, the record last handled, and meeting it again only clears `reset`.
/// A handled record spawns its effects while the room effect mode is 2 and
/// returns a request word; otherwise the result is 0, after wiping `field_848`
/// when no case claimed a record.
///
/// `steer` is a matching carrier (see `CSE_STEER`); it has no effect.
s32 func_actor_323400_80163448(Task* task, Actor323400Work* work)
{
    SVECTOR vec;
    s32     reset;
    s32     steer;
    reset = 1;
    switch (work->field_82E) {
        case 0: {
            s32 clip = work->slots[9].curRec & 0x3FF;
            s32 old;
            if (clip == 0x58) {
                old = work->field_848[9];
                if (old != clip) {
                    work->field_848[9] = clip;
                    vec.vz             = 0;
                    vec.vx             = 0;
                    vec.vy             = 0x2BC;
                    if (Gp_State1C->roomEffectMode == 2) {
                        Gp_SpawnEff(0x60054, &((TmdObject*)task->extra)->coords[9], 0x80002220, &vec);
                    }
                    return 0x40010002;
                }
                work->field_848[9] = old;
                reset              = 0;
            }
        }
            {
                s32 clip = work->slots[7].curRec & 0x3FF;
                s32 old;
                if (clip == 0x3E) {
                    old = work->field_848[7];
                    if (old != clip) {
                        work->field_848[7] = clip;
                        vec.vz             = 0;
                        vec.vx             = 0;
                        vec.vy             = 0x2BC;
                        if (Gp_State1C->roomEffectMode == 2) {
                            Gp_SpawnEff(0x60054, &((TmdObject*)task->extra)->coords[7], 0x80002220, &vec);
                        }
                        return 0x40010001;
                    }
                    work->field_848[7] = old;
                    reset              = 0;
                }
            }
            {
                s32 clip = work->slots[14].curRec & 0x3FF;
                s32 old;
                if (clip == 0x84) {
                    old = work->field_848[14];
                    if (old != clip) {
                        work->field_848[14] = clip;
                        vec.vz              = 0;
                        vec.vx              = 0;
                        vec.vy              = 0x258;
                        if (Gp_State1C->roomEffectMode == 2) {
                            Gp_SpawnEff(0x60054, &((TmdObject*)task->extra)->coords[14], 0x80002220, &vec);
                        }
                        return 0x40010002;
                    }
                    work->field_848[14] = old;
                    reset               = 0;
                }
            }
            {
                s32 clip = work->slots[17].curRec & 0x3FF;
                s32 old;
                if (clip == 0xA9) {
                    old = work->field_848[17];
                    if (old != clip) {
                        work->field_848[17] = clip;
                        vec.vz              = 0;
                        vec.vx              = 0;
                        vec.vy              = 0x258;
                        if (Gp_State1C->roomEffectMode == 2) {
                            Gp_SpawnEff(0x60054, &((TmdObject*)task->extra)->coords[17], 0x80002220, &vec);
                        }
                        return 0x40010001;
                    }
                    work->field_848[17] = old;
                    reset               = 0;
                }
            }
            break;

        case 10: {
            s32 clip = work->slots[1].curRec & 0x3FF;
            s32 old;
            if (clip == 0x9) {
                old = work->field_848[1];
                if (old != clip) {
                    work->field_848[1] = clip;
                    vec.vz             = 0;
                    vec.vx             = 0;
                    vec.vy             = 0;
                    if (Gp_State1C->roomEffectMode == 2) {
                        Gp_SpawnEff(0x60054, ((TmdObject*)task->extra)->coords, 0x80004A00, &vec);
                    }
                    return 0x40010005;
                }
                work->field_848[1] = old;
                reset              = 0;
            }
        } break;

        case 3: {
            s32 clip = work->slots[1].curRec & 0x3FF;
            s32 old;
            if (clip == 0x4) {
                reset = 0;
                old   = work->field_848[1];
                if (old != clip) {
                    work->field_848[1] = clip;
                    return 0x40010004;
                }
                work->field_848[1] = old;
            }
        }
            {
                s32 clip = work->slots[1].curRec & 0x3FF;
                s32 old;
                if (clip == 0x8) {
                    old = work->field_848[1];
                    if (old != clip) {
                        work->field_848[1] = clip;
                        return 0x40010003;
                    }
                    work->field_848[1] = old;
                    reset              = 0;
                }
            }
            break;

        case 6: {
            s32 clip = work->slots[1].curRec & 0x3FF;
            s32 old;
            if (clip == 0x6) {
                old = work->field_848[1];
                if (old != clip) {
                    work->field_848[1] = clip;
                    vec.vz             = 0;
                    vec.vx             = 0;
                    vec.vy             = 0x2BC;
                    if (Gp_State1C->roomEffectMode == 2) {
                        Gp_SpawnEff(0x60054, &((TmdObject*)task->extra)->coords[9], 0x80003200, &vec);
                    }
                    vec.vz = 0;
                    vec.vx = 0;
                    vec.vy = 0x2BC;
                    if (Gp_State1C->roomEffectMode == 2) {
                        Gp_SpawnEff(0x60054, &((TmdObject*)task->extra)->coords[7], 0x80003200, &vec);
                    }
                    return 0;
                }
                work->field_848[1] = old;
                reset              = 0;
            }
        }
            {
                s32 clip = work->slots[1].curRec & 0x3FF;
                s32 old;
                if (clip == 0xB) {
                    old = work->field_848[1];
                    if (old != clip) {
                        work->field_848[1] = clip;
                        vec.vz             = 0;
                        vec.vx             = 0;
                        vec.vy             = 0x258;
                        if (Gp_State1C->roomEffectMode == 2) {
                            Gp_SpawnEff(0x60054, &((TmdObject*)task->extra)->coords[14], 0x80004480, &vec);
                        }
                        vec.vz = 0;
                        vec.vx = 0;
                        vec.vy = 0x258;
                        if (Gp_State1C->roomEffectMode == 2) {
                            Gp_SpawnEff(0x60054, &((TmdObject*)task->extra)->coords[17], 0x80004480, &vec);
                        }
                        return 0;
                    }
                    work->field_848[1] = old;
                    reset              = 0;
                }
            }
            {
                s32 clip = work->slots[1].curRec & 0x3FF;
                s32 old;
                if (clip == 0xC) {
                    old = work->field_848[1];
                    if (old != clip) {
                        work->field_848[1] = clip;
                        vec.vz             = 0;
                        vec.vx             = 0;
                        vec.vy             = 0x2BC;
                        if (Gp_State1C->roomEffectMode == 2) {
                            Gp_SpawnEff(0x60054, &((TmdObject*)task->extra)->coords[9], 0x80002200, &vec);
                        }
                        CSE_STEER(steer);
                        vec.vz = 0;
                        vec.vx = 0;
                        vec.vy = 0x2BC;
                        if (steer == 0 && Gp_State1C->roomEffectMode == 2) {
                            Gp_SpawnEff(0x60054, &((TmdObject*)task->extra)->coords[7], 0x80002240, &vec);
                        }
                        CSE_STEER(steer);
                        vec.vz = 0;
                        vec.vx = 0;
                        vec.vy = 0x258;
                        if (steer == 0 && Gp_State1C->roomEffectMode == 2) {
                            Gp_SpawnEff(0x60054, &((TmdObject*)task->extra)->coords[14], 0x80003300, &vec);
                        }
                        vec.vz = 0;
                        vec.vx = 0;
                        vec.vy = 0x258;
                        if (Gp_State1C->roomEffectMode == 2) {
                            Gp_SpawnEff(0x60054, &((TmdObject*)task->extra)->coords[17], 0x80003340, &vec);
                        }
                        return 0;
                    }
                    work->field_848[1] = old;
                    reset              = 0;
                }
            }
            {
                s32 clip = work->slots[1].curRec & 0x3FF;
                s32 old;
                if (clip == 0xD) {
                    old = work->field_848[1];
                    if (old != clip) {
                        work->field_848[1] = clip;
                        vec.vz             = 0;
                        vec.vx             = 0;
                        vec.vy             = 0x258;
                        if (Gp_State1C->roomEffectMode == 2) {
                            Gp_SpawnEff(0x60054, &((TmdObject*)task->extra)->coords[14], 0x80002200, &vec);
                        }
                        vec.vz = 0;
                        vec.vx = 0;
                        vec.vy = 0x258;
                        if (Gp_State1C->roomEffectMode == 2) {
                            Gp_SpawnEff(0x60054, &((TmdObject*)task->extra)->coords[17], 0x80002300, &vec);
                        }
                        return 0;
                    }
                    work->field_848[1] = old;
                    reset              = 0;
                }
            }
            break;
    }

    if (reset == 1) {
        Mem_Set(work->field_848, 0, 0x48);
    }
    return 0;
}

INCLUDE_ASM("actors/nonmatchings/actor_323400/actor_323400_2", func_actor_323400_80163B58);

INCLUDE_ASM("actors/nonmatchings/actor_323400/actor_323400_2", func_actor_323400_80163FC8);

void func_actor_323400_801641C4(GpEnemy* enemy, Task* task)
{
    Actor323400Work* work;
    TmdObject*       obj;
    s32              id;
    s32              pan;
    SVECTOR          ofs2;
    SVECTOR          ofs;

    work = (Actor323400Work*)task->work;
    if (work->field_4 != 0) {
        obj               = (TmdObject*)task->extra;
        enemy->node.flags = 1;
        obj->flags        = 0;
        Tmd_AllocBuffers(obj);
        work->field_832 = 0x10;
        work->field_82E = 0xD;
        work->field_828 = 2;
        work->field_83E = 0;
        work->field_840 = 0;
        work->field_6   = 0;
        func_actor_323400_80163B58(task);
        return;
    }
    switch (++work->field_6) {
        case 9: {
            SVECTOR* p = &ofs;
            ofs.vz     = 0;
            p->vx      = 0;
            p->vy      = 0x2BC;
            if (Gp_State1C->roomEffectMode == 2) {
                Gp_SpawnEff(0x60054, &((TmdObject*)task->extra)->coords[9], 0x80002400, p);
            }
            break;
        }
        case 10: {
            SVECTOR* p = &ofs;
            ofs.vz     = 0;
            p->vx      = 0;
            p->vy      = 0x2BC;
            if (Gp_State1C->roomEffectMode == 2) {
                Gp_SpawnEff(0x60054, &((TmdObject*)task->extra)->coords[7], 0x80002400, p);
            }
            id  = ((enemy->placeKey >> 12) << 8) | 0x4001000E;
            pan = (s8)Gp_GetObjPan(((TmdObject*)task->extra)->coords);
            SndEvt_EnqueueType6(id, pan, (s8)gpGetObjDepth(((TmdObject*)task->extra)->coords));
            break;
        }
        case 12: {
            SVECTOR* p = &ofs;
            ofs.vz     = 0;
            p->vx      = 0;
            p->vy      = 0x258;
            if (Gp_State1C->roomEffectMode == 2) {
                Gp_SpawnEff(0x60054, &((TmdObject*)task->extra)->coords[14], 0x80003600, p);
            }
            break;
        }
        case 13: {
            SVECTOR* p = &ofs;
            ofs.vz     = 0;
            p->vx      = 0;
            p->vy      = 0x258;
            if (Gp_State1C->roomEffectMode == 2) {
                Gp_SpawnEff(0x60054, &((TmdObject*)task->extra)->coords[17], 0x80004500, p);
            }
            ofs.vz = 0;
            p->vx  = 0;
            p->vy  = 0x2BC;
            if (Gp_State1C->roomEffectMode == 2) {
                Gp_SpawnEff(0x60054, &((TmdObject*)task->extra)->coords[9], 0x80002480, p);
            }
            ofs2.vy = 0x3E8;
            ofs2.vx = 0;
            ofs2.vz = -0x12C;
            Gp_SpawnEff(0x60054, &((TmdObject*)task->extra)->coords[1], 0x80005900, &ofs2);
            break;
        }
    }
    func_actor_323400_80163B58(task);
    ((TmdObject*)task->extra)->coords->flg = 0;
}

INCLUDE_ASM("actors/nonmatchings/actor_323400/actor_323400_2", func_actor_323400_801644C4);

void func_actor_323400_8016475C(void)
{
}

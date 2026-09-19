#include "common.h"

#include <psyq/inline_c.h>
#include "psyq/abs.h"

#include "actors/actor_401300.h"
#include "actors/actors_shared_80132808.h"
#include "gameplay/1A8.h"
#include "gameplay/D4.h"
#include "gameplay/gameplay.h"
#include "main/gfx.h"
#include "main/mc.h"
#include "main/mem.h"
#include "main/session.h"
#include "main/sound.h"
#include "main/tmd.h"
#include "main/wipsys.h"

extern u8 D_801153F4;
void      Gp_DrawEffGroundQuad(VECTOR3* arg0, s32 arg1, s16 arg2);

/// `gpf 12`; the `inline_c.h` macro of that name assembles to a different word.
#define gte_gpf12_real() __asm__ volatile("nop; nop; .word 0x4B98003D")

/// Declared locally with a signed `arg2`; see the note in `gameplay/1BC.h`.
void func_800B4114(GpAnimCtx* arg0, s32 arg1, s16 arg2, s32 arg3, s32 arg4);

INCLUDE_ASM("actors/nonmatchings/actor_401300/actor_401300", func_actor_401300_801323B0);
s32 func_actor_401300_801323B0(GsCOORDINATE2* coord, u8* arg1, s32 arg2);
s32 func_actor_401300_80132FF4(Actor401300* arg0);

s32 func_actor_401300_80132554(Actor401300* arg0, s32 arg1, Actor401300Event* arg2)
{
    Actor401300Work* work  = arg0->field_1C;
    GpEnemy*         enemy = arg0->field_20;

    work->field_CA8[0] = arg2->b[0];
    work->field_CA8[1] = arg2->b[1];
    work->field_CA8[2] = arg2->b[2];
    if (arg2->w[0] == 0x301) {
        if (arg2->w[1] == 1) {
            work->field_0 = 0x17;
            return 1;
        }
    } else if (arg2->w[0] == 0xB05) {
        switch (arg2->w[1]) {
            case 0:
                work->field_0 = 0;
                return 1;
            case 0xB:
                work->field_0 = 0x23;
                work->field_2 = -1;
                return 1;
            case 0xC:
                if ((enemy->field_8 >> 12) == 0) {
                    work->field_0   = 6;
                    enemy->field_40 = D_actor_401300_80141FA4[0];
                    ((void (*)(s32))Gp_IncStateF0Ref)(0);
                }
                return 1;
        }
    } else if (arg2->w[0] == 0x1D05) {
        switch (arg2->w[1]) {
            case 0:
                work->field_0 = 0;
                return 1;
            case 0xB:
                work->field_0 = 0x23;
                work->field_2 = -1;
                return 1;
        }
    }
    return 0;
}

s32 func_actor_401300_8013267C(GsCOORDINATE2* coord, s16 arg1, s16 arg2)
{
    SVECTOR  v;
    SVECTOR  d;
    VECTOR   e;
    Task*    player;
    s16      angle;
    SVECTOR* pv;
    s32      x;

    player = Game_GetPtrSlot(3);
    d.vx   = ((GpCoordXZ*)((TmdObject*)player->extra)->coords)->field_18 - ((GpCoordXZ*)coord)->field_18;
    d.vy   = (u16)((TmdObject*)player->extra)->coords->coord.t[1] - (u16)coord->coord.t[1];
    d.vz   = ((GpCoordXZ*)((TmdObject*)player->extra)->coords)->field_20 - ((GpCoordXZ*)coord)->field_20;
    angle  = ratan2(d.vx, d.vz) - ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]);
    if (angle < 0) {
    loop_neg:
        if (angle < -0x800) {
            angle += 0x1000;
            goto loop_neg;
        }
    } else {
    loop_pos:
        if (angle > 0x800) {
            angle -= 0x1000;
            goto loop_pos;
        }
    }
    x = angle << 16;
    if (arg2 >= 0) {
        if (abs(x >> 16) > 0x400) {
            return 1;
        }
    } else {
        if (abs(x >> 16) < 0x400) {
            return 1;
        }
    }
    Gfx_MatrixCol2(&coord->coord, &v);
    pv = &v;
    VectorNormalSS(pv, pv);
    gte_lddp(arg2);
    gte_ldsv(pv);
    gte_gpf12_real();
    gte_stsv(pv);
    v.vx += (u16)coord->coord.t[0];
    v.vy += (u16)coord->coord.t[1];
    v.vz += (u16)coord->coord.t[2];
    e.vx  = ((TmdObject*)player->extra)->coords->coord.t[0] - v.vx;
    e.vy  = ((TmdObject*)player->extra)->coords->coord.t[1] - v.vy;
    e.vz  = ((TmdObject*)player->extra)->coords->coord.t[2] - v.vz;
    return SquareRoot0(e.vx * e.vx + e.vy * e.vy + e.vz * e.vz) >= arg1 + 0x96;
}

/// Pushes the root coordinate by a quarter of each kind 0x10000 / 0x30000 record's
/// offset (skipping 0x3000D), walking `recs` until `count` or a zero `key`.
/// The duplicated coordinate update keeps `count`'s sign extension in the loop,
/// as in `Actor01900_Fn03FF8`.
s32 func_actor_401300_80132910(Actor401300* arg0, GpRec18* recs, s16 count)
{
    Actor401300PushScratch* head;
    Actor401300PushScratch* s;
    Actor401300PushScratch* blk;

    if (D_80072729 == 1 || gGameSession->viewReady == 1) {
        return 0;
    }
    arg0->field_2C->coords[1].flg             = 0;
    head                                      = *(Actor401300PushScratch**)G_SCRATCH_HEAD;
    blk                                       = head - 1;
    *(Actor401300PushScratch**)G_SCRATCH_HEAD = blk;
    s                                         = blk;
    Gp_UpdateCoord(&arg0->field_2C->coords[1]);
    s->pos.vx = arg0->field_2C->coords[1].workm.t[0];
    s->pos.vy = arg0->field_2C->coords[1].workm.t[1];
    s->pos.vz = arg0->field_2C->coords[1].workm.t[2];
    s->hit    = 0;
    for (s->i = 0; s->i < count; s->i++) {
        if (recs[s->i].key == 0) {
            s->dist[s->i] = 0x7FFE;
            break;
        }
        s->kind = recs[s->i].key & 0xFFFF0000;
        if ((s->kind == 0x10000 || s->kind == 0x30000) && recs[s->i].key != 0x3000D) {
            if (s->kind == 0x10000) {
                s->hit = 1;
            }
            Gp_MakeDirOffset(&s->pos, (GpDirSrc*)&recs[s->i], &s->offset);
            s->len = s->offset.vx * s->offset.vx + s->offset.vz * s->offset.vz;
            s->len = SquareRoot0(s->len);
            if (s->len >= 0x140) {
                s->offset.vy = 0;
                VectorNormalSS(&s->offset, &s->offset);
                gte_lddp(0x140);
                gte_ldsv(&s->offset);
                __asm__ volatile("nop; nop; .word 0x4B98003D");
                gte_stsv(&s->offset);
                arg0->field_2C->coords->coord.t[0] += s->offset.vx >> 2;
                arg0->field_2C->coords->coord.t[2] += s->offset.vz >> 2;
            } else {
                arg0->field_2C->coords->coord.t[0] += s->offset.vx >> 2;
                arg0->field_2C->coords->coord.t[2] += s->offset.vz >> 2;
            }
            arg0->field_2C->coords->flg = 0;
        }
    }
    *(Actor401300PushScratch**)G_SCRATCH_HEAD += 1;
    return s->hit;
}

void func_actor_401300_80132BE4(GpAreaKey* session, GsCOORDINATE2* coord)
{
    Actor401300HeightClamp* row;
    s32                     offset;
    s32                     lo;
    s16                     i;

    for (i = 0; i < 2; i++) {
        row = &D_actor_401300_801589C8[i];
        if (session->stage == row->field_0 && session->area == row->field_2) {
            lo     = row->lo;
            offset = coord->coord.t[1];
            if (offset < lo) {
                coord->coord.t[1] = lo;
            } else if (row->hi < offset) {
                coord->coord.t[1] = row->hi;
            }
            return;
        }
    }
}

static __inline__ s32 Actor401300_HasHeightClamp(GpAreaKey* session)
{
    Actor401300HeightClamp* row;
    s16                     i;

    for (i = 0; i < 2; i++) {
        row = &D_actor_401300_801589C8[i];
        if (session->stage == row->field_0 && session->area == row->field_2) {
            return 1;
        }
    }
    return 0;
}

s32 func_actor_401300_80132C78(GsCOORDINATE2* coord, GpRec18* rec, s16 arg2, s16 arg3)
{
    Actor401300Delta* head;
    Actor401300Delta* s;
    Actor401300Delta* blk;
    s16               vy;
    SVECTOR*          step;

    if (D_80072729 == 1 || gGameSession->viewReady == 1) {
        return 0;
    }
    head                                = *(Actor401300Delta**)G_SCRATCH_HEAD;
    blk                                 = head - 1;
    *(Actor401300Delta**)G_SCRATCH_HEAD = blk;
    s                                   = blk;
    s->moved                            = 0;
    if (func_800E0C10(rec, &s->delta, arg2, NULL) != 0) {
        s->step.vx = head[-1].delta.vx.w >> 16;
        s->step.vy = s->delta.vy.w >> 16;
        s->step.vz = s->delta.vz.w >> 16;
        if (Actor401300_HasHeightClamp(&gGameSession->at4.loc.view)) {
            vy = s->step.vy;
            if (((vy >= 0) ? vy : -vy) > 0x15E) {
                s->step.vy = (vy <= 0) ? -0x15E : 0x15E;
            }
        }
        coord->coord.t[1] += s->step.vy;
        s->len             = s->step.vx * s->step.vx + s->step.vz * s->step.vz;
        s->len             = SquareRoot0(s->len);
        step               = &s->step;
        if (s->len >= 0xAF) {
            s->step.vy = 0;
            VectorNormalSS(step, step);
            gte_lddp(0xAF);
            gte_ldsv(step);
            __asm__ volatile("nop; nop; .word 0x4B98003D");
            gte_stsv(step);
            coord->coord.t[0] += s->step.vx;
            coord->coord.t[2] += s->step.vz;
        } else {
            coord->coord.t[0] += s->step.vx;
            coord->coord.t[2] += s->step.vz;
        }
        if (s->delta.vx.w & 0xFFFF) {
            if (s->delta.vx.w > 0) {
                coord->coord.t[0]++;
            } else {
                coord->coord.t[0]--;
            }
        }
        if (s->delta.vz.w & 0xFFFF) {
            if (s->delta.vz.w > 0) {
                coord->coord.t[2]++;
            } else {
                coord->coord.t[2]--;
            }
        }
    }
    if (Actor401300_HasHeightClamp(&gGameSession->at4.loc.view)) {
        func_actor_401300_80132BE4(&gGameSession->at4.loc.view, coord);
        coord->coord.t[1] += arg3;
    }
    if (s->delta.vx.w != 0 || s->delta.vz.w != 0) {
        s->moved = 1;
    }
    *(Actor401300Delta**)G_SCRATCH_HEAD += 1;
    return s->moved;
}

INCLUDE_ASM("actors/nonmatchings/actor_401300/actor_401300", func_actor_401300_80132FF4);

void func_actor_401300_80133254(Actor401300* arg0)
{
    s32                  i;
    Actor401300AnimWork* work;

    work = (Actor401300AnimWork*)arg0->field_1C;
    TOUCH_REG(work);

    if (work->field_8A0 != work->field_8A2) {
        for (i = 1; i < 0x13; i++) {
            work->slots[i].rate = work->field_8A6;
            if (i >= 7) {
                if (i < 9) {
                    continue;
                }
            }
            func_800B4114(&work->anim, i, work->field_8A2, 0,
                          D_actor_401300_8015804C[work->field_8A0][work->field_8A2]);
        }
        work->field_8A0 = work->field_8A2;
    }
}

void func_actor_401300_80133324(Actor401300* arg0)
{
    GpAnimPose           pose;
    GpAnimPose           blendPose;
    s16                  weight;
    s16                  i;
    Actor401300AnimWork* work;

    work   = (Actor401300AnimWork*)arg0->field_1C;
    weight = work->field_8B0;
    for (i = 1; i < 0x13; i++) {
        if (i < 0xB) {
            work->blendSlots[i].rate = (u8)work->field_8AE;
            work->slots[i].rate      = (u8)(work->field_8A6 - 3);
            if (i >= 7) {
                if (i < 9) {
                    continue;
                }
            }
            do {
                func_800B3448(&work->anim, i, (s32)&pose, 0);
                func_800B3448(&work->blendAnim, i, (s32)&blendPose, 0);
                Gp_AnimWritePoseCopy(&work->anim, i, &pose, &blendPose, weight, 0x1000 - weight);
            } while (0);
        } else {
            work->slots[i].rate = (u8)(work->field_8A6 - 3);
            Gp_AnimTickIndex(&work->anim, i);
        }
    }
}

/// Returns the sound to play when the current animation (`field_8A2`) reaches
/// one of its cue frames, once per frame reached; `field_8BC` holds the last cue
/// frame seen. The frame is re-read at every use: caching it in a local moves
/// CSE's choice of register for the repeat-frame store.
s32 func_actor_401300_8013346C(Actor401300Work* work)
{
    switch ((s16)(work->field_8A2 - 2)) {
        case 1:
            if ((work->field_5E & 0x3FF) == 0xF) {
                if (work->field_8BC != (work->field_5E & 0x3FF)) {
                    work->field_8BC = work->field_5E & 0x3FF;
                    return 0x400D0004;
                }
                work->field_8BC = work->field_5E & 0x3FF;
                break;
            } else if ((work->field_5E & 0x3FF) == 0x15) {
                if (work->field_8BC != (work->field_5E & 0x3FF)) {
                    work->field_8BC = work->field_5E & 0x3FF;
                    return 0x400D0003;
                }
                work->field_8BC = work->field_5E & 0x3FF;
                break;
            }
            work->field_8BC = 0;
            break;
        case 0:
            if ((work->field_5E & 0x3FF) == 0x11) {
                if (work->field_8BC != (work->field_5E & 0x3FF)) {
                    work->field_8BC = work->field_5E & 0x3FF;
                    return 0x400D0002;
                }
                work->field_8BC = work->field_5E & 0x3FF;
                break;
            } else if ((work->field_5E & 0x3FF) == 0x1A) {
                if (work->field_8BC != (work->field_5E & 0x3FF)) {
                    work->field_8BC = work->field_5E & 0x3FF;
                    return 0x400D0001;
                }
                work->field_8BC = work->field_5E & 0x3FF;
                break;
            }
            work->field_8BC = 0;
            break;
        case 23:
            if ((work->field_5E & 0x3FF) == 0xB) {
                if (work->field_8BC != (work->field_5E & 0x3FF)) {
                    work->field_8BC = work->field_5E & 0x3FF;
                    return 0x400D000C;
                }
                work->field_8BC = work->field_5E & 0x3FF;
                break;
            } else if ((work->field_5E & 0x3FF) == 0xE) {
                if (work->field_8BC != (work->field_5E & 0x3FF)) {
                    work->field_8BC = work->field_5E & 0x3FF;
                    return 0x400D0001;
                }
                work->field_8BC = work->field_5E & 0x3FF;
                break;
            }
            work->field_8BC = 0;
            break;
        case 24:
            if ((work->field_5E & 0x3FF) == 0xB) {
                if (work->field_8BC != (work->field_5E & 0x3FF)) {
                    work->field_8BC = work->field_5E & 0x3FF;
                    return 0x400D000C;
                }
                work->field_8BC = work->field_5E & 0x3FF;
                break;
            }
            work->field_8BC = 0;
            break;
        case 10:
            if ((work->field_5E & 0x3FF) == 0x7) {
                if (work->field_8BC != (work->field_5E & 0x3FF)) {
                    work->field_8BC = work->field_5E & 0x3FF;
                    return 0x400D0005;
                }
                work->field_8BC = work->field_5E & 0x3FF;
                break;
            }
            work->field_8BC = 0;
            break;
        case 32:
            if ((work->field_5E & 0x3FF) == 0x4) {
                if (work->field_8BC != (work->field_5E & 0x3FF)) {
                    work->field_8BC = work->field_5E & 0x3FF;
                    return 0x400D0005;
                }
                work->field_8BC = work->field_5E & 0x3FF;
                break;
            }
            work->field_8BC = 0;
            break;
        case 9:
            if ((work->field_5E & 0x3FF) == 0x5) {
                if (work->field_8BC != (work->field_5E & 0x3FF)) {
                    work->field_8BC = work->field_5E & 0x3FF;
                    return 0x400D0005;
                }
                work->field_8BC = work->field_5E & 0x3FF;
                break;
            }
            work->field_8BC = 0;
            break;
        case 7:
            if ((work->field_5E & 0x3FF) == 0x7) {
                if (work->field_8BC != (work->field_5E & 0x3FF)) {
                    work->field_8BC = work->field_5E & 0x3FF;
                    return 0x400D0006;
                }
                work->field_8BC = work->field_5E & 0x3FF;
                break;
            }
            work->field_8BC = 0;
            break;
        case 30:
            if ((work->field_5E & 0x3FF) == 0xB) {
                if (work->field_8BC != (work->field_5E & 0x3FF)) {
                    work->field_8BC = work->field_5E & 0x3FF;
                    return 0x400D000A;
                }
                work->field_8BC = work->field_5E & 0x3FF;
                break;
            }
            work->field_8BC = 0;
            break;
        case 31:
            if ((work->field_5E & 0x3FF) == 0xD) {
                if (work->field_8BC != (work->field_5E & 0x3FF)) {
                    work->field_8BC = work->field_5E & 0x3FF;
                    return 0x400D000B;
                }
                work->field_8BC = work->field_5E & 0x3FF;
                break;
            }
            work->field_8BC = 0;
            break;
        case 25:
            if ((work->field_5E & 0x3FF) == 0xE) {
                if (work->field_8BC != (work->field_5E & 0x3FF)) {
                    work->field_8BC = work->field_5E & 0x3FF;
                    return 0x400D0004;
                }
                work->field_8BC = work->field_5E & 0x3FF;
                break;
            } else if ((work->field_5E & 0x3FF) == 0x14) {
                if (work->field_8BC != (work->field_5E & 0x3FF)) {
                    work->field_8BC = work->field_5E & 0x3FF;
                    return 0x400D0003;
                }
                work->field_8BC = work->field_5E & 0x3FF;
                break;
            }
            work->field_8BC = 0;
            break;
        case 26:
            if ((work->field_5E & 0x3FF) == 0x12) {
                if (work->field_8BC != 0xE) {
                    work->field_8BC = work->field_5E & 0x3FF;
                    return 0x400D0004;
                }
                work->field_8BC = work->field_5E & 0x3FF;
                break;
            }
            work->field_8BC = 0;
            break;
        case 27:
            if ((work->field_5E & 0x3FF) == 0xD) {
                if (work->field_8BC != (work->field_5E & 0x3FF)) {
                    work->field_8BC = work->field_5E & 0x3FF;
                    return 0x400D0002;
                }
                work->field_8BC = work->field_5E & 0x3FF;
                break;
            } else if ((work->field_5E & 0x3FF) == 0xF) {
                if (work->field_8BC != (work->field_5E & 0x3FF)) {
                    work->field_8BC = work->field_5E & 0x3FF;
                    return 0x400D0001;
                }
                work->field_8BC = work->field_5E & 0x3FF;
                break;
            } else if ((work->field_5E & 0x3FF) == 0x11) {
                if (work->field_8BC != (work->field_5E & 0x3FF)) {
                    work->field_8BC = work->field_5E & 0x3FF;
                    return 0x400D0002;
                }
                work->field_8BC = work->field_5E & 0x3FF;
                break;
            } else if ((work->field_5E & 0x3FF) == 0x14) {
                if (work->field_8BC != (work->field_5E & 0x3FF)) {
                    work->field_8BC = work->field_5E & 0x3FF;
                    return 0x400D0001;
                }
                work->field_8BC = work->field_5E & 0x3FF;
                break;
            }
            work->field_8BC = 0;
            break;
        case 28:
            if ((work->field_5E & 0x3FF) == 0x9) {
                if (work->field_8BC != (work->field_5E & 0x3FF)) {
                    work->field_8BC = work->field_5E & 0x3FF;
                    return 0x400D0012;
                }
                work->field_8BC = work->field_5E & 0x3FF;
                break;
            }
            work->field_8BC = 0;
            break;
    }
    return 0;
}

void func_actor_401300_80133834(Actor401300* arg0, s16 arg1)
{
    SVECTOR* sc;

    sc     = (SVECTOR*)(*(u32*)G_SCRATCH_HEAD -= 8);
    sc->vx = D_actor_401300_801589F8[1].vx +
             ((D_actor_401300_801589F8[0].vx - D_actor_401300_801589F8[1].vx) * (0x200 - arg1)) / 512;
    sc->vy = D_actor_401300_801589F8[1].vy +
             ((D_actor_401300_801589F8[0].vy - D_actor_401300_801589F8[1].vy) * (0x200 - arg1)) / 512;
    sc->vz = D_actor_401300_801589F8[1].vz +
             ((D_actor_401300_801589F8[0].vz - D_actor_401300_801589F8[1].vz) * (0x200 - arg1)) / 512;
    RotMatrix_gte(sc, &arg0->field_2C->coords[7].coord);
    sc->vx = D_actor_401300_80158A08[1].vx +
             ((D_actor_401300_80158A08[0].vx - D_actor_401300_80158A08[1].vx) * (0x200 - arg1)) / 512;
    sc->vy = D_actor_401300_80158A08[1].vy +
             ((D_actor_401300_80158A08[0].vy - D_actor_401300_80158A08[1].vy) * (0x200 - arg1)) / 512;
    sc->vz = D_actor_401300_80158A08[1].vz +
             ((D_actor_401300_80158A08[0].vz - D_actor_401300_80158A08[1].vz) * (0x200 - arg1)) / 512;
    RotMatrix_gte(sc, &arg0->field_2C->coords[8].coord);
    arg0->field_2C->coords[7].flg = 0;
    *(u32*)G_SCRATCH_HEAD        += 8;
    arg0->field_2C->coords[8].flg = 0;
}

extern s32 D_80115738;
extern s32 D_8011574C;

/// Spawns effect `id` on `coord` at the offset (`x`, `y`, `z`).
static __inline__ void Actor401300_SpawnEff(s32 id, GsCOORDINATE2* coord, s32 flags, s16 x, s16 y, s16 z)
{
    SVECTOR pos;

    pos.vx = x;
    pos.vy = y;
    pos.vz = z;
    Gp_SpawnEff(id, coord, flags, &pos);
}

static __inline__ void Actor401300_SpawnEffZero(s32 id, GsCOORDINATE2* coord, s32 flags)
{
    SVECTOR pos;

    pos.vx = pos.vy = pos.vz = 0;
    Gp_SpawnEff(id, coord, flags, &pos);
}

/// `Actor401300_SpawnEff` for an effect id held in a global. Taking the
/// global's address rather than its value is a matching requirement: the `lui`
/// is then evaluated with the arguments and the load itself after them, which
/// is the order the scheduler needs.
static __inline__ void Actor401300_SpawnEffVar(s32* id, GsCOORDINATE2* coord, s32 flags, s16 x, s16 y, s16 z)
{
    SVECTOR pos;

    pos.vx = x;
    pos.vy = y;
    pos.vz = z;
    Gp_SpawnEff(*id, coord, flags, &pos);
}

static __inline__ void Actor401300_SpawnEffZeroVar(s32* id, GsCOORDINATE2* coord, s32 flags)
{
    SVECTOR pos;

    pos.vx = pos.vy = pos.vz = 0;
    Gp_SpawnEff(*id, coord, flags, &pos);
}

/// 1 when coordinate 1's view-space Z is in [-299, 2300): the body of
/// `func_actor_401300_801417F0`, with `Actor401300_TransformToView` written
/// out so `outp` is initialised after `svp`. The `if` that re-tests `ret` keeps
/// jump from folding the result into a bare `sltiu`.
static __inline__ s32 Actor401300_InRange(Actor401300* arg0)
{
    SVECTOR        out;
    SVECTOR        sv;
    VECTOR         vec;
    s32            flag;
    SVECTOR*       svp;
    GsCOORDINATE2* view;
    VECTOR*        vecp;
    s32*           flagp;
    SVECTOR*       outp;
    GsCOORDINATE2* p;
    s32            ret;

    memset(&out, 0, 8);
    p     = &arg0->field_2C->coords[1];
    svp   = &sv;
    outp  = &out;
    view  = &gGfxViewCoord;
    vecp  = &vec;
    flagp = &flag;
    sv.vx = outp->vx;
    sv.vy = outp->vy;
    sv.vz = outp->vz;
loop:
    if (p->sub != NULL) {
        if (p != view) {
            gte_SetTransMatrix(&p->coord);
            gte_SetRotMatrix(&p->coord);
            gte_ldv0(svp);
            __asm__ volatile("nop; nop; .word 0x4A480012");
            gte_stlvnl(vecp);
            gte_stflg(flagp);
            sv.vx = vec.vx;
            sv.vy = vec.vy;
            sv.vz = vec.vz;
            p     = p->sub;
            goto loop;
        }
        outp->vx = sv.vx;
        outp->vy = sv.vy;
        outp->vz = sv.vz;
    }
    ret = (u16)(out.vz + 0x12B) < 0xA27;
    if (ret != 0) {
        ret = 1;
    } else {
        ret = 0;
    }
    return ret;
}

static __inline__ void Actor401300_ResetAnim(Actor401300* arg0)
{
    s32                  i;
    Actor401300AnimWork* work;

    work = (Actor401300AnimWork*)arg0->field_1C;
    for (i = 1; i < 0x13; i++) {
        work->slots[i].rate = work->field_8A6;
        if (i < 7) {
            Gp_AnimResetSlotEx(&work->anim, i, work->field_8A2, i, i);
        } else if (i >= 9) {
            Gp_AnimResetSlotEx(&work->anim, i, work->field_8A2, i - 2, i);
        }
    }
    work->field_8A0 = work->field_8A2;
}

static __inline__ void Actor401300_ResetBlendAnim(Actor401300* arg0)
{
    s32                  i;
    Actor401300AnimWork* work;

    work            = (Actor401300AnimWork*)arg0->field_1C;
    work->field_8AE = 0x30;
    work->field_8B0 = 0x800;
    for (i = 1; i < 0x13; i++) {
        work->slots[i].rate = work->field_8AE;
        if (i < 7) {
            Gp_AnimResetSlotEx(&work->blendAnim, i, work->field_8AC, i, i);
        } else if (i >= 9) {
            Gp_AnimResetSlotEx(&work->blendAnim, i, work->field_8AC, i - 2, i);
        }
    }
}

static __inline__ void Actor401300_TickAnim(Actor401300* arg0)
{
    s32                  i;
    Actor401300AnimWork* work;

    work = (Actor401300AnimWork*)arg0->field_1C;
    for (i = 1; i < 0x13; i++) {
        work->slots[i].rate = work->field_8A6;
        if (i < 7) {
            Gp_AnimTickIndex(&work->anim, i);
        } else if (i >= 9) {
            Gp_AnimTickIndex(&work->anim, i);
        }
    }
}

/// Per-frame animation and effect update: restarts or ticks the animation
/// slots, eases the yaw of coordinates 5/2 and the blend weight, then spawns
/// the current animation's effects and plays its cue sound.
void func_actor_401300_80133A3C(Actor401300* arg0)
{
    s32              i;
    s32              snd;
    s16              yaw;
    s32              inRange;
    Actor401300Work* work;
    GpEnemy*         enemy;

    /* Set here so CSE keeps `inRange` distinct from the helper's result. */
    inRange = 0;
    work    = arg0->field_1C;
    enemy   = arg0->field_20;
    if (work->field_89C == 1) {
        func_actor_401300_80133254(arg0);
        work->field_89C = 3;
        work->field_8A4 = 0;
        work->field_8BC = 0;
    } else if (work->field_89C == 2) {
        Actor401300_ResetAnim(arg0);
        work->field_89C = 3;
        work->field_8A4 = 0;
        work->field_8BC = 0;
    }
    if (work->field_8AA == 2) {
        Actor401300_ResetBlendAnim(arg0);
        work->field_8AA = 3;
    }
    work->field_8A4++;
    if (work->field_89E == 0) {
        Actor401300_TickAnim(arg0);
    } else {
        func_actor_401300_80133324(arg0);
        if (((Actor401300AnimWork*)work)->blendSlots[1].flags & 0x100) {
            work->field_89E = 0;
        }
    }
    if (work->field_8B2 > work->field_8B4) {
        if (work->field_8B2 - work->field_8B4 > 0x100) {
            work->field_8B4 += 0x100;
        } else {
            work->field_8B4 = work->field_8B2;
        }
    } else if (-(work->field_8B2 - work->field_8B4) > 0x100) {
        work->field_8B4 -= 0x100;
    } else {
        work->field_8B4 = work->field_8B2;
    }
    if (work->field_8B4 != 0) {
        yaw = work->field_8B4;
        if (work->field_8B4 > 0x400) {
            yaw = 0x400;
        }
        if (work->field_8B4 < -0x400) {
            yaw = -0x400;
        }
        ActorsShared80132808(&arg0->field_2C->coords[5], (yaw * 2) / 3);
        ActorsShared80132808(&arg0->field_2C->coords[2], yaw / 2);
        arg0->field_2C->coords[5].flg = 0;
        arg0->field_2C->coords[4].flg = 0;
        arg0->field_2C->coords[3].flg = 0;
        arg0->field_2C->coords[2].flg = 0;
    }
    if (work->field_8B8 != work->field_8B6) {
        if (work->field_8B6 < work->field_8B8) {
            work->field_8B8 -= work->field_8BA;
            if (work->field_8B8 < work->field_8B6) {
                work->field_8B8 = work->field_8B6;
            }
        } else {
            work->field_8B8 += work->field_8BA;
            if (work->field_8B6 < work->field_8B8) {
                work->field_8B8 = work->field_8B6;
            }
        }
    }
    func_actor_401300_80133834(arg0, work->field_8B8);
    snd     = func_actor_401300_8013346C(work);
    inRange = Actor401300_InRange(arg0);
    if (inRange == 1) {
        if (work->field_8A2 == 2) {
            if ((u32)gDisplayState.animFrame % 6 == 0) {
                Actor401300_SpawnEffVar(&D_8011574C, &arg0->field_2C->coords[18], 0x40, 0, 0x1C2, -100);
            }
            if ((u32)gDisplayState.animFrame % 6 == 3) {
                Actor401300_SpawnEffVar(&D_8011574C, &arg0->field_2C->coords[15], 0x40, 0, 0x1C2, -100);
            }
        } else if (work->field_8A2 == 3) {
            if ((gDisplayState.animFrame & 1) == inRange) {
                Actor401300_SpawnEffVar(&D_80115738, &arg0->field_2C->coords[18], 0x1202180, 0, 0x1C2, -100);
                Actor401300_SpawnEffVar(&D_8011574C, &arg0->field_2C->coords[18], 0x40, 0, 0x1C2, -100);
            }
            if (!(gDisplayState.animFrame & 1)) {
                Actor401300_SpawnEffVar(&D_80115738, &arg0->field_2C->coords[15], 0x1202180, 0, 0x1C2, -100);
                Actor401300_SpawnEffVar(&D_8011574C, &arg0->field_2C->coords[15], 0x40, 0, 0x1C2, -100);
            }
        } else if (work->field_8A2 == 9 || work->field_8A2 == 25 || work->field_8A2 == 26) {
            if ((u32)gDisplayState.animFrame % 5 == 0) {
                Actor401300_SpawnEffVar(&D_8011574C, &arg0->field_2C->coords[18], 0x40, 0, 0x1C2, -100);
            }
            if ((u32)gDisplayState.animFrame % 6 == 3) {
                Actor401300_SpawnEffVar(&D_8011574C, &arg0->field_2C->coords[15], 0x40, 0, 0x1C2, -100);
            }
        }
        if (snd != 0 && (*(s32*)&gGameSession->at4.loc.view & 0xFFFF0000) == 0x051D0000) {
            switch (snd) {
                case 0x400D0001:
                case 0x400D0003:
                    Actor401300_SpawnEffVar(&D_80115738, &arg0->field_2C->coords[18], 0x1202180, 0, 0x1C2, -100);
                    snd = 0x551D0006;
                    break;
                case 0x400D0002:
                case 0x400D0004:
                    Actor401300_SpawnEffVar(&D_80115738, &arg0->field_2C->coords[15], 0x1202180, 0, 0x1C2, -100);
                    snd = 0x551D0007;
                    break;
                case 0x400D0005:
                case 0x400D000B:
                    Actor401300_SpawnEffZeroVar(&D_80115738, &arg0->field_2C->coords[1], 0x1202180);
                    Actor401300_SpawnEffZeroVar(&D_80115738, &arg0->field_2C->coords[1], 0x1202180);
                    Actor401300_SpawnEffZeroVar(&D_80115738, &arg0->field_2C->coords[1], 0x1202180);
                    snd = 0x551D0005;
                    break;
            }
        }
    }
    if (Gp_State1C->field_A == 2) {
        switch (snd) {
            case 0x400D0001:
            case 0x400D0003:
                Actor401300_SpawnEff(0x60054, &arg0->field_2C->coords[18], 0x800022C0, 0, 0x15E, -100);
                break;
            case 0x400D0002:
            case 0x400D0004:
                Actor401300_SpawnEff(0x60054, &arg0->field_2C->coords[15], 0x800022F0, 0, 0x15E, -100);
                break;
            case 0x400D0005:
            case 0x400D000B:
                Actor401300_SpawnEffZero(0x60054, &arg0->field_2C->coords[1], 0x80004800);
                Actor401300_SpawnEffZero(0x60054, &arg0->field_2C->coords[1], 0x80004800);
                break;
        }
    }
    if (snd != 0) {
        i = snd | ((enemy->field_8 >> 12) << 8);
        SndEvt_EnqueueType6(i, (s8)Gp_GetObjPan((GpObj38*)arg0->field_2C->coords),
                            (s8)Gp_GetObjDepth((GpObj38*)arg0->field_2C->coords));
    }
}

/// Points the model's light and color matrices at the work block's copies.
static __inline__ void Actor401300_BindMatrices(Actor401300* actor)
{
    Actor401300Work* work;
    TmdObject*       obj;

    work          = actor->field_1C;
    obj           = actor->field_2C;
    obj->lightMtx = &work->field_C28;
    obj->colorMtx = &work->field_C48;
}

/// Rebuilds the root coordinate's scaled Y rotation and seeds the combat
/// defaults while the rotation scratch block is still held.
static __inline__ void Actor401300_InitPose(GsCOORDINATE2* coord, Actor401300Work* work)
{
    void*                  top;
    Actor401300RotScratch* blk;
    s16                    ang;
    u16                    m22;

    top                   = (void*)*(u32*)G_SCRATCH_HEAD;
    blk                   = (Actor401300RotScratch*)((u8*)top - 0x34);
    *(u32*)G_SCRATCH_HEAD = (u32)blk;
    ang                   = ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]);
    blk->angle            = ang;
    Gfx_RotMatrixY(&blk->m, ang, 1);
    blk->scale.vz = 0x1964;
    blk->scale.vy = 0x1964;
    blk->scale.vx = 0x1964;
    ScaleMatrix(&blk->m, &blk->scale);
    coord->coord.m[0][0]   = *(u16*)&((Actor401300RotScratch*)((u8*)top - 0x34))->m.m[0][0];
    coord->coord.m[0][1]   = *(u16*)&blk->m.m[0][1];
    coord->coord.m[0][2]   = *(u16*)&blk->m.m[0][2];
    coord->coord.m[1][0]   = *(u16*)&blk->m.m[1][0];
    coord->coord.m[1][1]   = *(u16*)&blk->m.m[1][1];
    coord->coord.m[1][2]   = *(u16*)&blk->m.m[1][2];
    coord->coord.m[2][0]   = *(u16*)&blk->m.m[2][0];
    coord->coord.m[2][1]   = *(u16*)&blk->m.m[2][1];
    m22                    = *(u16*)&blk->m.m[2][2];
    coord->flg             = 0;
    coord->coord.m[2][2]   = m22;
    work->field_D78        = 0;
    work->field_C8A        = 0;
    work->field_CAC        = D_actor_401300_80158914;
    work->field_8B8        = 0x100;
    work->field_8B6        = 0x170;
    work->field_8BA        = 0x20;
    *(u32*)G_SCRATCH_HEAD += 0x34;
}

void func_actor_401300_80134454(GpEnemy* enemy, Actor401300* actor)
{
    SVECTOR              dir;
    VECTOR               pos;
    SVECTOR*             v;
    TmdObject*           obj;
    GsCOORDINATE2*       root;
    Actor401300Work*     work;
    GpObj*               body;
    GpObj*               head;
    Actor401300MatWords* mw;

    root            = actor->field_2C->coords;
    obj             = actor->field_2C;
    work            = memCalloc(0xD7C, 0);
    actor->field_1C = work;
    if (work == NULL) {
        Gp_DestroyEnemy(enemy, (Task*)actor);
        return;
    }
    if (actor->field_36 != 2) {
        ((void (*)(s32))Gp_IncStateF0Ref)(0);
    }
    ((Task*)actor)->exitCallback = func_actor_401300_80141758;
    Actor401300_BindMatrices(actor);
    enemy->field_4     = &actor->field_2C->coords->coord;
    enemy->field_48    = 0;
    enemy->field_1C.vx = 0;
    enemy->field_1C.vy = 0;
    enemy->field_1C.vz = 0;
    enemy->field_18    = &actor->field_2C->coords[2];
    Gp_LinkNode(&enemy->node);
    enemy->node.flags = 1;
    enemy->field_4C   = 0;
    enemy->field_40   = (s16)D_actor_401300_80141FA0.hpMax;
    enemy->field_50   = &D_actor_401300_80141FA0;
    enemy->field_54   = (s32)&work->field_990;
    Gp_AnimInitCtxSlots(&((Actor401300AnimWork*)work)->anim, &D_actor_401300_80158838, obj,
                        ((Actor401300AnimWork*)work)->pad_32C, ((Actor401300AnimWork*)work)->slots);
    Gp_AnimInitCtxSlots(&((Actor401300AnimWork*)work)->blendAnim, &D_actor_401300_80158838, obj,
                        ((Actor401300AnimWork*)work)->pad_768, ((Actor401300AnimWork*)work)->blendSlots);
    work->field_89C = 2;
    work->field_89E = 0;
    work->field_8A2 = 2;
    work->field_8B4 = 0;
    work->field_8B2 = 0;
    work->field_8A8 = 0x10;
    work->field_8A6 = 0x10;
    if ((s16)((enemy->field_8 >> 12) & 1) == 1) {
        work->field_8A8++;
    } else {
        work->field_8A8--;
    }
    func_actor_401300_80133A3C(actor);

    work->field_920.sub        = &gGfxViewCoord;
    mw                         = (Actor401300MatWords*)&work->field_920.coord;
    mw->m00_m01                = 0x1000;
    mw->m02_m10                = 0;
    mw->m11_m12                = 0x1000;
    mw->m20_m21                = 0;
    mw->m22                    = 0x1000;
    work->field_920.coord.t[0] = actor->field_2C->coords->coord.t[0];
    work->field_920.coord.t[1] = actor->field_2C->coords->coord.t[1] - 0x15E;
    work->field_920.coord.t[2] = actor->field_2C->coords->coord.t[2];
    work->field_920.flg        = 0;
    Gp_UpdateCoord(&work->field_920);

    work->field_AB0.ctx.recs = (GpRec18*)&work->field_AD0;
    work->field_AB0.coord    = &work->field_920;
    work->field_AB0.pos.vx   = 0;
    work->field_AB0.pos.vy   = 0;
    work->field_AB0.pos.vz   = 0;
    work->field_AB0.key      = 0x3000D;
    work->field_AB0.radius   = 0x15E;
    work->field_AB0.flags    = 1;
    Gp_LinkObj(2, &work->field_AB0);
    work->field_C88        = 0;
    work->field_AB0.flags |= 0x4000;
    Gp_InitRec18Table(work->field_AB0.ctx.recs, 0xC, 0);

    body           = &work->field_970;
    body->ctx.recs = (GpRec18*)&work->field_990;
    body->key      = 0x30000;
    body->coord    = &gGfxViewCoord;
    body->pos.vx   = 0;
    body->pos.vy   = 0;
    body->pos.vz   = 0;
    body->radius   = 0x280;
    body->flags    = 1;
    Gp_LinkObj(2, body);
    body->flags |= 0x8000;
    Gp_InitRec18Table(body->ctx.recs, 0xC, 0);

    dir.vx         = 0;
    dir.vy         = 0;
    dir.vz         = 0;
    head           = &work->field_BF0;
    head->coord    = &actor->field_2C->coords[3];
    head->ctx.recs = (GpRec18*)&work->pad_C10;
    v              = &dir;
    head->pos.vx   = v->vx;
    head->pos.vy   = v->vy;
    head->pos.vz   = v->vz;
    head->radius   = 0x200;
    head->flags    = 1;
    Gp_LinkObj(3, head);
    Gp_InitRec18Table(head->ctx.recs, 1, 0);

    work->field_16     = 0;
    work->field_C[0].x = actor->field_2C->coords->coord.t[0];
    work->field_C[0].z = actor->field_2C->coords->coord.t[2];
    Gfx_MatrixCol2(&actor->field_2C->coords->coord, v);
    dir.vy = 0;
    VectorNormalSS(v, v);
    gte_lddp(2000);
    gte_ldsv(v);
    __asm__ volatile("nop; nop; .word 0x4B98003D");
    gte_stsv(v);
    work->field_C[1].x = actor->field_2C->coords->coord.t[0] + dir.vx;
    work->field_C[1].z = actor->field_2C->coords->coord.t[2] + dir.vz;

    ((Task*)actor)->msgTable = &D_actor_401300_80158988;
    root->sub                = &gGfxViewCoord;
    root->flg                = 0;
    Gp_UpdateCoord(root);
    pos.vx = root->workm.t[0];
    pos.vy = root->workm.t[1];
    pos.vz = root->workm.t[2];
    Gp_UpdateActorColor(enemy, &pos, 0, 0);

    work->field_910.coord      = &actor->field_2C->coords[1];
    work->field_910.spawnArgLo = 0x300;
    work->field_910.spawnArgHi = 2;
    switch ((u8)actor->field_36) {
        case 2:
            work->field_2   = -1;
            work->field_0   = 0;
            enemy->field_40 = -999;
            break;
        case 4:
            work->field_2 = -1;
            work->field_0 = 0x16;
            break;
        case 0x20:
            work->field_2   = -1;
            work->field_0   = 0x27;
            enemy->field_40 = 0x50;
            break;
        default:
            work->field_2 = -1;
            work->field_0 = 0x18;
            Tmd_AllocBuffers(obj);
            break;
    }
    switch (((Task*)actor)->spawnArg1 & 0xF) {
        case 2:
            work->field_CA0 = D_actor_401300_80141FB0[0].vx;
            work->field_CA2 = D_actor_401300_80141FB0[0].vy;
            work->field_CA4 = D_actor_401300_80141FB0[0].vz;
            break;
        case 1:
            work->field_CA0 = D_actor_401300_80141FB0[2].vx;
            work->field_CA2 = D_actor_401300_80141FB0[2].vy;
            work->field_CA4 = D_actor_401300_80141FB0[2].vz;
            break;
        case 0:
        default:
            work->field_CA0 = D_actor_401300_80141FB0[1].vx;
            work->field_CA2 = D_actor_401300_80141FB0[1].vy;
            work->field_CA4 = D_actor_401300_80141FB0[1].vz;
            break;
    }

    Actor401300_InitPose(actor->field_2C->coords, work);
    ((Task*)actor)->state++;
}

void func_actor_401300_80134BA4(Actor401300* arg0, s16 arg1, s32 arg2)
{
    SVECTOR*         sc;
    s32              mag;
    Actor401300Work* work;

    sc   = (SVECTOR*)(*(u32*)G_SCRATCH_HEAD -= 8);
    mag  = (arg1 >= 0) ? arg1 : -arg1;
    work = arg0->field_1C;
    if (mag < 0x200) {
        Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
        switch ((s32)(Gp_LcgState >> 16) & 3) {
            case 0:
                *sc = D_actor_401300_80158928[0];
                break;
            case 1:
                *sc = D_actor_401300_80158928[1];
                break;
            case 2:
                *sc = D_actor_401300_80158928[2];
                break;
            case 3:
                *sc = D_actor_401300_80158928[3];
                break;
            default:
                *sc = D_actor_401300_80158928[4];
                break;
        }
    } else if (mag > 0x600) {
        Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
        switch ((s32)(Gp_LcgState >> 16) & 2) {
            case 0:
                *sc = D_actor_401300_80158928[5];
                break;
            case 1:
                *sc = D_actor_401300_80158928[6];
                break;
            default:
                *sc = D_actor_401300_80158928[7];
                break;
        }
    } else if (arg1 > 0) {
        Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
        if ((Gp_LcgState >> 16) & 1) {
            *sc = D_actor_401300_80158928[8];
        } else {
            *sc = D_actor_401300_80158928[9];
        }
    } else {
        Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
        if ((Gp_LcgState >> 16) & 1) {
            *sc = D_actor_401300_80158928[10];
        } else {
            *sc = D_actor_401300_80158928[11];
        }
    }
    work->field_910.coord      = &arg0->field_2C->coords[1];
    work->field_910.spawnArgLo = 0x300;
    work->field_910.spawnArgHi = 2;
    func_800FDB18(Gp_GetIdParam1(arg2) & 0xFFFF, &arg0->field_2C->coords[sc->pad], sc, &work->field_910);
    *(u32*)G_SCRATCH_HEAD += 8;
}

/// Wraps a 12-bit angle difference into `[-0x800, 0x800]`.
static __inline__ s16 Actor401300_NormalizeYaw(s16 input)
{
    s16 value = input;
    if (input < 0) {
        while (1) {
            if (value >= -0x800)
                break;
            value += 0x1000;
        }
    } else {
        while (1) {
            if (value <= 0x800)
                break;
            value -= 0x1000;
        }
    }
    return value;
}

/// First `GpRec18` among the twelve at `records` whose id has high word 2,
/// copying its position to `pos`; 0 at the first empty record.
static __inline__ s32 Actor401300_FindHit(SVECTOR* pos, GpRec18* records)
{
    s16 i;

    for (i = 0; i < 12; i++) {
        if (!records[i].key)
            break;
        if ((records[i].key & 0xFFFF0000) == 0x20000) {
            pos->vx = records[i].point.vx;
            pos->vy = records[i].point.vy;
            pos->vz = records[i].point.vz;
            return records[i].key;
        }
    }
    return 0;
}

void func_actor_401300_80134F90(Actor401300* arg0)
{
    PlayerStatus*          config = &Player_Status;
    Actor401300Work*       work;
    GpEnemy*               enemy;
    Actor401300HitScratch* head;
    Actor401300HitScratch* s;
    GsCOORDINATE2*         coord;
    Task*                  player;
    SVECTOR*               dir;
    s16                    z;
    s32                    yaw;
    s32                    dx;
    s32                    dy;
    s32                    dz;
    s32                    deathSound;
    s32                    deathPan;
    s32                    hitSound;
    s32                    hitPan;
    s32                    mag;
    s16                    state;
    s16                    effect;
    u32                    damage;

    enemy = arg0->field_20;
    work  = arg0->field_1C;
    if (enemy->field_40 > 0 && (work->field_0 != 8 || work->field_8A2 != 0x20)) {
        head  = *(Actor401300HitScratch**)G_SCRATCH_HEAD;
        s     = (*(Actor401300HitScratch**)G_SCRATCH_HEAD = head - 1);
        s->id = Actor401300_FindHit(&head[-1].hitPos, (GpRec18*)work->field_990);
        if (s->id == 0) {
            s->id = Actor401300_FindHit(&s->hitPos, (GpRec18*)work->field_AD0);
        }
        if (s->id != 0) {
            work->field_D1C        = 0;
            work->field_D1E        = 0;
            work->field_970.radius = 0x280;
            if (s->id & 0x8000) {
                player       = Game_GetPtrSlot(3);
                s->hitPos.vx = ((TmdObject*)player->extra)->coords->workm.t[0];
                s->hitPos.vy = ((TmdObject*)player->extra)->coords->workm.t[1];
                s->hitPos.vz = ((TmdObject*)player->extra)->coords->workm.t[2];
            }
            arg0->field_2C->coords->flg = 0;
            Gp_UpdateCoord(arg0->field_2C->coords);
            s->dir.vx = s->hitPos.vx - arg0->field_2C->coords->workm.t[0];
            s->dir.vy = s->hitPos.vy - arg0->field_2C->coords->workm.t[1];
            z         = s->hitPos.vz - arg0->field_2C->coords->workm.t[2];
            s->dir.vz = z;
            yaw       = ratan2(s->dir.vx, z);
            coord     = arg0->field_2C->coords;
            s->yaw    = yaw - ratan2(-coord->workm.m[2][0], coord->workm.m[2][2]);
            s->yaw    = Actor401300_NormalizeYaw(s->yaw);
            func_actor_401300_80134BA4(arg0, s->yaw, s->id);
            work->field_8B4 = 0;
            work->field_8B2 = 0;
            s->effect       = -1;
            state           = work->field_0;
            if (state != 0x13 && state != 0x14 && state != 0x25 && state != 0x26 && state != 0x11 && state != 0xF && state != 0x10 &&
                state != 0x27 && state != 4) {
                s->m = arg0->field_2C->coords->coord;
                Gfx_RotMatrixY(&s->m, s->yaw, 0);
                dir = &s->dir;
                Gfx_MatrixCol2(&s->m, dir);
                VectorNormalSS(dir, dir);
                if (work->field_89E == 1) {
                    gte_lddp(-5);
                    gte_ldsv(dir);
                    __asm__ volatile("nop; nop; .word 0x4B98003D");
                    gte_stsv(dir);
                } else {
                    gte_lddp(-10);
                    gte_ldsv(dir);
                    __asm__ volatile("nop; nop; .word 0x4B98003D");
                    gte_stsv(dir);
                }
                arg0->field_2C->coords->coord.t[0] += s->dir.vx;
                arg0->field_2C->coords->coord.t[1] += s->dir.vy;
                arg0->field_2C->coords->coord.t[2] += s->dir.vz;
                arg0->field_2C->coords->flg         = 0;
            }
            dx        = config->coordMtx->t[0] - arg0->field_2C->coords->coord.t[0];
            s->dx     = dx;
            dy        = config->coordMtx->t[1] - arg0->field_2C->coords->coord.t[1];
            s->dy     = dy;
            dz        = config->coordMtx->t[2] - arg0->field_2C->coords->coord.t[2];
            s->dz     = dz;
            s->dist   = SquareRoot0(dx * dx + dy * dy + dz * dz);
            s->damage = Gp_ComputeDamage(s->id, s->dist, 0, 0);
            if (Gp_RollEnemyChance(enemy, s->id, 0) != 0) {
                s->crit    = 1;
                s->effect  = 0;
                s->damage *= 4;
            } else {
                s->crit = 0;
            }
            mag = s->yaw;
            if (mag < 0) {
                mag = -mag;
            }
            if (mag > 0x500) {
                state = work->field_0;
                if (state != 0x13) {
                    if (state != 0x14 && state != 0x11 && state != 0x25 && state != 0x26 && state != 0xF && state != 0x10 && state != 0x27 && state != 4) {
                        damage    = s->damage * 2;
                        s->damage = damage;
                        if (damage != 0) {
                            s->effect = 4;
                        }
                    }
                }
            }
            func_800E2C78((GpObj40*)enemy, s->id, s->damage, 0);
            effect = s->effect;
            if (effect != -1) {
                Gp_SpawnEff(0x6009C, &arg0->field_2C->coords[2], effect, NULL);
            }
            enemy->field_40 -= s->damage;
            func_800DA6E8(&enemy->node, s->damage, 0);
            if (work->field_0 == 0x17) {
                SndEvt_EnqueueType7(0x51030008, 1);
            }
            if ((work->field_0 == 0xC || work->field_0 == 0xD || work->field_0 == 0xE) && config->hp > 0 && work->field_D20 == 1) {
                Gp_DispatchMsg(Game_GetPtrSlot(3), 0x3F1, 0, 0);
            }
            if (enemy->field_40 <= 0) {
                deathSound = ((enemy->field_8 >> 0xC) << 8) | 0x400D0008;
                deathPan   = (s8)Gp_GetObjPan((GpObj38*)arg0->field_2C->coords);
                SndEvt_EnqueueType6(deathSound, deathPan, (s8)Gp_GetObjDepth((GpObj38*)arg0->field_2C->coords));
            } else {
                hitSound = ((enemy->field_8 >> 0xC) << 8) | 0x400D0007;
                hitPan   = (s8)Gp_GetObjPan((GpObj38*)arg0->field_2C->coords);
                SndEvt_EnqueueType6(hitSound, hitPan, (s8)Gp_GetObjDepth((GpObj38*)arg0->field_2C->coords));
            }
            work->field_C88 = Gp_GetIdParam2(s->id);
            switch (Gp_GetIdParam0(s->id) & 0xFFFF) {
                case 4:
                    state = work->field_0;
                    if (state != 0x13 && state != 0x14 && state != 0x25 && state != 0x26 && state != 0x27 && state != 4 && state != 0x11) {
                        mag = s->yaw;
                        if (mag < 0) {
                            mag = -mag;
                        }
                        if (mag < 0x400) {
                            work->field_0 = 0x13;
                        } else {
                            work->field_0 = 0x14;
                        }
                    }
                    break;
                case 0:
                case 5:
                case 6:
                case 7:
                case 8:
                case 9:
                    if (work->field_0 == 4) {
                        work->field_2 = -1;
                    } else if (work->field_0 != 0xF && work->field_0 != 0x10) {
                        if (work->field_0 == 0x13 || work->field_0 == 0x14 || work->field_0 == 0x27 || work->field_0 == 4 || work->field_0 == 0x11) {
                            if (work->field_8A2 == 0xB || work->field_8A2 == 0x17 || work->field_8A2 == 8 || work->field_8A2 == 0xA) {
                                work->field_89E = 1;
                                work->field_8AC = 0xB;
                            } else {
                                work->field_89E = 1;
                                work->field_8AC = 0x22;
                            }
                            work->field_8AA = 2;
                        } else if (s->crit == 1) {
                            if (work->field_0 != 4 && work->field_0 != 0x27 && work->field_0 != 0x11) {
                                work->field_0 = 5;
                            }
                        } else {
                            work->field_8AC = 0xD;
                            work->field_89E = 1;
                            work->field_8AA = 2;
                        }
                    }
                    break;
                case 2:
                    Gp_SetObjFlag2((GpObj5D*)enemy, s->id, 0);
                    state = work->field_0;
                    if (state == 0x11 || state == 0x27 || state == 4) {
                        work->field_0 = 4;
                        work->field_2 = -1;
                    } else {
                        mag = s->yaw;
                        if (mag < 0) {
                            mag = -mag;
                        }
                        if (mag < 0x400) {
                            work->field_0 = 0x13;
                        } else {
                            work->field_0 = 0x14;
                        }
                    }
                    break;
                case 3:
                    state = work->field_0;
                    if (state == 0x18 || state == 0x16 || state == 0x17) {
                        work->field_0 = 5;
                    }
                    Gp_SetObjFlag4((GpObj5C*)enemy, s->id, 0);
                    break;
                case 1:
                    enemy->field_4C &= 0xFE;
                    state            = work->field_0;
                    if (state != 0x13 && state != 0x14 && state != 0x27 && state != 4 && state != 0x25 && state != 0x26 && state != 0x11) {
                        if (state == 0xF && work->field_6 < 0xC) {
                            work->field_0 = 0x25;
                        } else if (work->field_0 == 0x10 && work->field_6 < 0xC) {
                            work->field_0 = 0x26;
                        } else {
                            mag = s->yaw;
                            if (mag < 0) {
                                mag = -mag;
                            }
                            if (mag < 0x400) {
                                work->field_0 = 0x13;
                            } else {
                                work->field_0 = 0x14;
                            }
                        }
                    }
                    break;
            }
        }
        if (enemy->field_4C & 0xC) {
            s->damage = Gp_TickObjFlag4((GpObj5C*)enemy);
            if (Gp_ObjFlag4Expired((GpObj5C*)enemy) != 0) {
                enemy->field_4C &= 0xF3;
            }
            if (s->damage != 0) {
                enemy->field_40 -= s->damage;
                func_800DA6E8(&enemy->node, s->damage, 0);
                if (work->field_0 == 7 || work->field_0 == 0x1E || work->field_0 == 0xB || work->field_0 == 0x1B) {
                    work->field_0 = 5;
                } else if (work->field_0 == 4) {
                    work->field_2 = -1;
                } else {
                    if (work->field_0 == 0x13 || work->field_0 == 0x14 || work->field_0 == 0xF || work->field_0 == 0x10 || work->field_0 == 0x27 || work->field_0 == 0x11) {
                        if (work->field_8A2 == 0xB || work->field_8A2 == 0x17 || work->field_8A2 == 8 || work->field_8A2 == 0xA) {
                            work->field_89E = 1;
                            work->field_8AC = 0xB;
                        } else if (work->field_8A2 == 0x22 || work->field_8A2 == 0x18 || work->field_8A2 == 0x16 || work->field_8A2 == 0xC) {
                            work->field_89E = 1;
                            work->field_8AC = 0x22;
                        } else {
                            work->field_89E = 1;
                            work->field_8AC = 0xD;
                        }
                    } else {
                        work->field_89E = 1;
                        work->field_8AC = 0xD;
                    }
                    work->field_8AA = 2;
                }
            }
        }
        if (enemy->field_40 <= 0) {
            if (s->id != 0) {
                if ((Gp_GetIdParam0(s->id) & 0xFFFF) == 4) {
                    state = work->field_8A2;
                    if (state == 2 || state == 3 || state == 0x1B || state == 0x1C || state == 0x1D) {
                        work->field_0 = 0x28;
                    } else {
                        work->field_0 = 0x1D;
                    }
                } else {
                    state = work->field_0;
                    if (state != 0x13 && state != 0x14 && state != 0x25 && state != 0x26 && state != 4 && state != 0x27 && state != 0x25 && state != 0x26 && state != 0x11) {
                        if (state == 0xF && work->field_6 < 0xC) {
                            work->field_0 = 0x25;
                            work->field_2 = -1;
                        } else if (work->field_0 == 0x10 && work->field_6 < 0xC) {
                            work->field_0 = 0x26;
                            work->field_2 = -1;
                        } else {
                            mag = s->yaw;
                            if (mag < 0) {
                                mag = -mag;
                            }
                            if (mag < 0x400) {
                                work->field_0 = 0x13;
                            } else {
                                work->field_0 = 0x14;
                            }
                        }
                    }
                }
            } else {
                state = work->field_0;
                if (state != 0x13 && state != 0x14 && state != 4 && state != 0x27 && state != 0x25 && state != 0x26 && state != 0x11) {
                    work->field_0 = 0x13;
                }
            }
            if (enemy->field_40 <= 0) {
                enemy->field_40 = 0;
                work->field_C8A = 1;
            }
        }
        *(Actor401300HitScratch**)G_SCRATCH_HEAD += 1;
    }
}

void func_actor_401300_80135DDC(Actor401300* arg0)
{
    Actor401300Work* work  = arg0->field_1C;
    GpEnemy*         enemy = arg0->field_20;
    TmdObject*       tmd;

    if (work->field_4 != 0) {
        tmd               = arg0->field_2C;
        enemy->node.flags = 0;
        tmd->flags        = 0;
        Tmd_AllocBuffers(tmd);
        work->field_89C        = 2;
        work->field_8A6        = 0x10;
        work->field_AB0.flags |= 0x4000;
        if (work->field_8A2 == 11 || work->field_8A2 == 23) {
            work->field_8A2 = 0x17;
        } else if (work->field_8A2 == 12 || work->field_8A2 == 34 || work->field_8A2 == 24) {
            work->field_8A2 = 0x18;
        }
        if ((u16)(work->field_8A2 - 0x17) >= 2) {
            work->field_8A2 = 0x17;
        }
        do {
            func_actor_401300_80133A3C(arg0);
        } while (!(work->field_8A2 == 0x17 && (work->field_5E & 0x3FF) >= 6) &&
                 !(work->field_8A2 == 0x18 && (work->field_5E & 0x3FF) >= 9));
        work->field_8A6 = 0x20;
        return;
    }
    arg0->field_2C->coords->flg = 0;
    work->field_8A6             = work->field_8A6 / 2;
    if (work->field_8A6 == 1) {
        work->field_8A6 = -0x10;
    }
    if (work->field_8A6 == -1) {
        work->field_8A6 = 0x10;
    }
    func_actor_401300_80133A3C(arg0);
    if (Gp_TickObjFlag2((GpObj5D*)enemy) == 1) {
        enemy->field_4C &= ~2;
        work->field_8A6  = 0x10;
        if (arg0->field_36 == 0x20) {
            work->field_0 = 0x27;
        } else {
            work->field_0 = 0x11;
        }
    }
    if (enemy->field_40 <= 0) {
        work->field_0 = 0x15;
    }
}

void func_actor_401300_80135FC4(Actor401300* arg0)
{
    Actor401300Work* work  = arg0->field_1C;
    GpEnemy*         enemy = arg0->field_20;
    s16              i     = 0;
    u16              r;
    TmdObject*       tmd;

    if (work->field_4 != 0) {
        tmd               = arg0->field_2C;
        enemy->node.flags = 0;
        tmd->flags        = 0;
        Tmd_AllocBuffers(tmd);
        work->field_89C        = 2;
        work->field_8A6        = 0x10;
        work->field_8A2        = 0x17;
        work->field_8B6        = 0x40;
        work->field_8B8        = 0x40;
        work->field_8BA        = 0x20;
        work->field_AB0.flags |= 0x4000;
        do {
            func_actor_401300_80133A3C(arg0);
        } while (!(work->field_6C & 0x100) && ++i < 0xFF);
        work->field_8A6 = 0x20;
        return;
    }
    if (++work->field_6 == 0) {
        work->field_89C = 2;
        Gp_LcgState     = Gp_LcgState * 5 + 0x71357911;
        r               = (Gp_LcgState >> 16) % 3;
        switch (r) {
            case 0:
                work->field_8A6 = 0x20;
                break;
            case 1:
                work->field_8A6 = 0x30;
                break;
            case 2:
            default:
                work->field_8A6 = 0x40;
                break;
        }
        func_actor_401300_80133A3C(arg0);
        func_actor_401300_80133A3C(arg0);
        work->field_8A6 = 0x10;
    } else if (work->field_6 > 0) {
        arg0->field_2C->coords->flg = 0;
        work->field_8A6             = work->field_8A6 / 2;
        if (work->field_8A6 == 1) {
            work->field_8A6 = -0x10;
        }
        if (work->field_8A6 == -1) {
            work->field_8A6 = 0x10;
        }
        func_actor_401300_80133A3C(arg0);
    } else if (work->field_89E == 1 || !(work->field_6C & 0x100)) {
        work->field_8A6 = 0x10;
        func_actor_401300_80133A3C(arg0);
    }
    if (work->field_6 >= 7) {
        Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
        work->field_6 = -((Gp_LcgState >> 16) & 0xFF);
    }
    if (enemy->field_40 <= 0) {
        work->field_0 = 0x11;
    }
}

/// Rebuild `coord`'s Y rotation from its current yaw, uniformly scaled by
/// `scale`. Same body as `Actor01900_RescaleYaw`.
static __inline__ void Actor401300_RescaleYaw(GsCOORDINATE2* coord, s16 scale)
{
    void**                 scratch;
    void*                  head;
    Actor401300RotScratch* blk;
    s16                    ang;
    u16                    m22;

    scratch  = (void**)G_SCRATCH_HEAD;
    head     = *scratch;
    blk      = (Actor401300RotScratch*)((u8*)head - 0x34);
    *scratch = blk;

    ang        = ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]);
    blk->angle = ang;
    Gfx_RotMatrixY(&blk->m, ang, 1);
    blk->scale.vz = scale;
    blk->scale.vy = scale;
    blk->scale.vx = scale;
    ScaleMatrix(&blk->m, &blk->scale);

    coord->coord.m[0][0] = *(u16*)&((Actor401300RotScratch*)((u8*)head - 0x34))->m.m[0][0];
    coord->coord.m[0][1] = *(u16*)&blk->m.m[0][1];
    coord->coord.m[0][2] = *(u16*)&blk->m.m[0][2];
    coord->coord.m[1][0] = *(u16*)&blk->m.m[1][0];
    coord->coord.m[1][1] = *(u16*)&blk->m.m[1][1];
    coord->coord.m[1][2] = *(u16*)&blk->m.m[1][2];
    coord->coord.m[2][0] = *(u16*)&blk->m.m[2][0];
    coord->coord.m[2][1] = *(u16*)&blk->m.m[2][1];
    m22                  = *(u16*)&blk->m.m[2][2];
    *scratch             = (u8*)*scratch + 0x34;
    coord->flg           = 0;
    coord->coord.m[2][2] = m22;
}

static __inline__ void Actor401300_ConfigPositionDelta(PlayerStatus* config, GsCOORDINATE2* coord, SVECTOR* pos)
{
    pos->vx = config->coordMtx->t[0] - coord->coord.t[0];
    pos->vy = config->coordMtx->t[1] - coord->coord.t[1];
    pos->vz = config->coordMtx->t[2] - coord->coord.t[2];
}

/// Yaw from the actor's facing to the player, wrapped; `pos` receives the offset.
static __inline__ s16 Actor401300_PositionYaw(Actor401300* actor, SVECTOR* pos, PlayerStatus* config)
{
    GsCOORDINATE2* coord;
    s32            angle;
    Actor401300_ConfigPositionDelta(config, actor->field_2C->coords, pos);
    coord = actor->field_2C->coords;
    angle = ratan2(pos->vx, pos->vz);
    return Actor401300_NormalizeYaw(angle - ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]));
}
void func_actor_401300_80136238(Actor401300* arg0)
{
    Actor401300Work*       work;
    TmdObject*             obj;
    GsCOORDINATE2*         coord;
    Actor401300AimScratch* aim;

    work = arg0->field_1C;
    if (work->field_4 != 0) {
        obj                        = arg0->field_2C;
        arg0->field_20->node.flags = 0;
        obj->flags                 = 0;
        Tmd_AllocBuffers(obj);
        work->field_89C        = 2;
        work->field_8A6        = 0x10;
        work->field_8A2        = 9;
        work->field_89E        = 0;
        work->field_BF0.flags &= 0x7FFF;
        work->field_AB0.flags &= 0xBFFF;
        func_actor_401300_80133A3C(arg0);
        work->field_970.radius = 0x280;
        Gp_ArmStateF0(1);
        work->field_8B6 = 0x200;
        work->field_8BA = 0x20;
        return;
    }
    if (work->field_8B6 == work->field_8B8) {
        if (work->field_8B6 == 0x200) {
            work->field_8BA = 0x80;
            work->field_8B6 = 0x190;
        } else {
            work->field_8B6 = 0x200;
        }
    }
    *(Actor401300AimScratch**)G_SCRATCH_HEAD -= 1;
    aim                                       = *(Actor401300AimScratch**)G_SCRATCH_HEAD;
    arg0->field_2C->coords->flg               = 0;
    if (work->field_6C & 0x100) {
        if (func_actor_401300_80132FF4(arg0) == 1 && (*(s32*)&gGameSession->at4.loc.view & 0xFFFF0000) == 0x051D0000) {
            work->field_0 = 8;
        } else {
            work->field_0 = 7;
        }
    }
    aim->angle      = Actor401300_PositionYaw(arg0, &aim->delta, &Player_Status);
    work->field_8B2 = aim->angle;
    if (aim->angle > 0x10) {
        aim->angle = 0x10;
    }
    if (aim->angle < -0x10) {
        aim->angle = -0x10;
    }
    coord       = arg0->field_2C->coords;
    aim->angle += ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]);
    Gfx_RotMatrixY(&arg0->field_2C->coords->coord, aim->angle, 1);
    Actor401300_RescaleYaw(arg0->field_2C->coords, 0x1964);
    func_actor_401300_80133A3C(arg0);
    *(Actor401300AimScratch**)G_SCRATCH_HEAD += 1;
}

/// `Actor401300_MoveForward` with a zero-amount guard, the X component read
/// back through `head`. Same body as `Actor01900_MoveForward`.
static __inline__ void Actor401300_MoveForwardNonzero(GsCOORDINATE2* coord, s16 amount)
{
    SVECTOR* head;
    SVECTOR* vec;
    SVECTOR* gteVec;

    if (D_80072729 != 1) {
        head                       = *(SVECTOR**)G_SCRATCH_HEAD;
        vec                        = head - 1;
        *(SVECTOR**)G_SCRATCH_HEAD = vec;
        gteVec                     = vec;
        if (amount != 0) {
            SOFT_TOUCH_REG(vec);
            Gfx_MatrixCol2(&coord->coord, vec);
            VectorNormalSS(vec, vec);
            gte_lddp(amount);
            gte_ldsv(gteVec);
            gte_gpf12_real();
            gte_stsv(gteVec);
            coord->coord.t[0] += head[-1].vx;
            coord->coord.t[1] += vec->vy;
            coord->coord.t[2] += vec->vz;
            coord->flg         = 0;
        }
        *(SVECTOR**)G_SCRATCH_HEAD += 1;
    }
}

void func_actor_401300_801365F8(Actor401300* arg0)
{
    Actor401300Work*           work;
    GameActor*                 player;
    PlayerStatus*              config;
    TmdObject*                 obj;
    GsCOORDINATE2*             coord;
    GsCOORDINATE2*             c1;
    GsCOORDINATE2*             c2;
    Actor401300PursuitScratch* head;
    Actor401300PursuitScratch* sc;
    SVECTOR*                   delta;
    s32                        angle;
    s32                        dist;
    s32                        dx;
    s32                        dy;
    s32                        dz;
    s32                        mask;
    u16                        speed;

    config = &Player_Status;
    work   = arg0->field_1C;
    player = (GameActor*)((Task*)Game_GetPtrSlot(3))->work;
    mask   = 0xF0;
    if ((arg0->field_36 & mask) == 0x10) {
        work->field_0 = 0x1E;
        return;
    }
    if (work->field_4 != 0) {
        obj                        = arg0->field_2C;
        arg0->field_20->node.flags = 0;
        obj->flags                 = 0;
        Tmd_AllocBuffers(obj);
        work->field_970.radius = 0x280;
        work->field_89C        = 1;
        SOFT_BARRIER();
        work->field_8A2        = 3;
        speed                  = work->field_8A8;
        work->field_BF0.flags &= 0x7FFF;
        SOFT_BARRIER();
        work->field_89E        = 0;
        work->field_8A6        = speed;
        work->field_AB0.flags |= 0x4000;
        func_actor_401300_80133A3C(arg0);
        work->field_D1C = 0;
        work->field_6   = 0;
        work->field_8   = 0;
        work->field_8B6 = 0x40;
        work->field_8BA = 0x10;
        return;
    }
    if (work->field_8B6 == work->field_8B8) {
        if (work->field_8B6 == 0x40) {
            work->field_8B6 = 0x80;
            work->field_8BA = 0x10;
        } else {
            work->field_8B6 = 0x40;
        }
    }
    work->field_6++;
    head                                         = *(Actor401300PursuitScratch**)G_SCRATCH_HEAD;
    delta                                        = &head[-1].delta;
    c1                                           = arg0->field_2C->coords;
    head[-1].delta.vx                            = Player_Status.coordMtx->t[0] - c1->coord.t[0];
    delta->vy                                    = Player_Status.coordMtx->t[1] - c1->coord.t[1];
    delta->vz                                    = Player_Status.coordMtx->t[2] - c1->coord.t[2];
    *(Actor401300PursuitScratch**)G_SCRATCH_HEAD = head - 1;
    sc                                           = head - 1;
    arg0->field_2C->coords->flg                  = 0;
    func_actor_401300_80133A3C(arg0);
    if (func_actor_401300_80132C78(arg0->field_2C->coords, (GpRec18*)work->field_AD0, 0xC, 0x57) == 0) {
        func_actor_401300_80132910(arg0, (GpRec18*)work->field_990, 0xC);
    }
    c2              = arg0->field_2C->coords;
    angle           = ratan2(head[-1].delta.vx, delta->vz);
    sc->angle       = Actor401300_NormalizeYaw(angle - ratan2(-c2->coord.m[2][0], c2->coord.m[2][2]));
    work->field_8B2 = sc->angle;
    if ((s16)func_actor_401300_8013267C(arg0->field_2C->coords, 0x15E, (work->field_8A8 + 2) * 30 * 1.5f / 18.0f)) {
        Actor401300_MoveForwardNonzero(arg0->field_2C->coords, (work->field_8A8 + 2) * 30 * 1.5f / 18.0f);
    }
    if (sc->angle > 0x30) {
        sc->angle = 0x30;
    } else if (sc->angle < -0x30) {
        sc->angle = -0x30;
    } else {
        sc->dx = dx = config->coordMtx->t[0] - arg0->field_2C->coords->coord.t[0];
        sc->dy = dy = config->coordMtx->t[1] - arg0->field_2C->coords->coord.t[1];
        sc->dz = dz = config->coordMtx->t[2] - arg0->field_2C->coords->coord.t[2];
        dist        = SquareRoot0(dx * dx + dy * dy + dz * dz);
        sc->dist    = dist;
        if (player->field_954 != 2 && work->field_6 >= 0x28) {
            if (dist > 4000) {
                work->field_0 = 0x21;
            } else if (dist > 2000) {
                Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
                if (((Gp_LcgState >> 16) & 0xF) < 5) {
                    work->field_0 = 0x21;
                } else {
                    work->field_0 = 0x22;
                }
            } else if (dist < 1000) {
                Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
                if (((Gp_LcgState >> 16) & 0xF) < 7) {
                    work->field_0 = 0x1F;
                } else {
                    work->field_0 = 0x20;
                }
            }
        }
    }
    coord      = arg0->field_2C->coords;
    sc->angle += ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]);
    Gfx_RotMatrixY(&arg0->field_2C->coords->coord, sc->angle, 1);

    Actor401300_RescaleYaw(arg0->field_2C->coords, 0x1964);
    arg0->field_2C->coords->flg = 0;
    Gp_UpdateCoord(arg0->field_2C->coords);
    *(Actor401300PursuitScratch**)G_SCRATCH_HEAD += 1;
}

/// Step `coord` `amount` units along its local Z axis unless movement is
/// frozen. Same body as `Actor00100_MoveForward`.
static __inline__ void Actor401300_MoveForward(GsCOORDINATE2* coord, s16 amount)
{
    SVECTOR* head;
    SVECTOR* vec;

    if (D_80072729 != 1) {
        head                       = *(SVECTOR**)G_SCRATCH_HEAD;
        vec                        = head - 1;
        *(SVECTOR**)G_SCRATCH_HEAD = vec;
        Gfx_MatrixCol2(&coord->coord, vec);
        VectorNormalSS(vec, vec);
        gte_lddp(amount);
        gte_ldsv(vec);
        gte_gpf12_real();
        gte_stsv(vec);
        coord->coord.t[0]          += head[-1].vx;
        coord->coord.t[1]          += vec->vy;
        coord->coord.t[2]          += vec->vz;
        coord->flg                  = 0;
        *(SVECTOR**)G_SCRATCH_HEAD += 1;
    }
}

static __inline__ void Actor401300_MoveBy(GsCOORDINATE2* coord, s16 amount)
{
    SVECTOR* head;
    SVECTOR* vec;
    SVECTOR* v;

    if (D_80072729 != 1) {
        head                       = *(SVECTOR**)G_SCRATCH_HEAD;
        vec                        = head - 1;
        *(SVECTOR**)G_SCRATCH_HEAD = vec;
        v                          = vec;
        if (amount != 0) {
            Gfx_MatrixCol2(&coord->coord, vec);
            VectorNormalSS(vec, vec);
            gte_lddp(amount);
            gte_ldsv(v);
            gte_gpf12_real();
            gte_stsv(v);
            coord->coord.t[0] += head[-1].vx;
            coord->coord.t[1] += vec->vy;
            coord->coord.t[2] += vec->vz;
            coord->flg         = 0;
        }
        *(SVECTOR**)G_SCRATCH_HEAD += 1;
    }
}

static __inline__ s32 Actor401300_Abs(s32 x)
{
    if (x < 0) {
        x = -x;
    }
    return x;
}

void func_actor_401300_80136CE8(Actor401300* arg0)
{
    Actor401300Work*           work;
    GpEnemy*                   enemy;
    TmdObject*                 obj;
    GsCOORDINATE2*             coord;
    Actor401300PursuitScratch* head;
    Actor401300PursuitScratch* blk;
    Actor401300PursuitScratch* s;
    s32                        z;
    u16                        speed;

    work  = arg0->field_1C;
    enemy = arg0->field_20;
    if (work->field_4 != 0) {
        obj               = arg0->field_2C;
        enemy->node.flags = 0;
        obj->flags        = 0;
        Tmd_AllocBuffers(obj);
        work->field_970.radius = 0x280;
        work->field_89C        = 1;
        SOFT_BARRIER();
        work->field_8A2        = 3;
        speed                  = work->field_8A8;
        work->field_BF0.flags &= 0x7FFF;
        SOFT_BARRIER();
        work->field_89E        = 0;
        work->field_8A6        = speed;
        work->field_AB0.flags |= 0x4000;
        func_actor_401300_80133A3C(arg0);
        work->field_8B6 = 0x40;
        work->field_D1C = 0;
        work->field_6   = 0;
        work->field_8   = 0;
        work->field_8BA = 0x10;
        z               = arg0->field_2C->coords->coord.t[2];
        if (z > 0x1B58) {
            work->home.vx  = -0xB54;
            work->home.vz  = 0x2198;
            work->home.vy  = 0;
            work->home.pad = 0x400;
        } else if (z > 0x1068) {
            work->home.vx  = 0;
            work->home.vy  = 0;
            work->home.vz  = 0x189C;
            work->home.pad = 0;
        } else if (z > 0x384) {
            work->home.vx  = 0x12C0;
            work->home.vy  = 0;
            work->home.vz  = 0x1AF4;
            work->home.pad = 0;
        } else if (z > -0x898) {
            work->home.vx  = 0x12C0;
            work->home.vz  = -0x1388;
            work->home.vy  = 0;
            work->home.pad = 0x800;
        } else {
            work->home.vx  = -0xB4;
            work->home.vy  = 0;
            work->home.vz  = -0x960;
            work->home.pad = 0;
        }
        return;
    }
    if (work->field_8B6 == work->field_8B8) {
        if (work->field_8B6 == 0x40) {
            work->field_8B6 = 0x80;
            work->field_8BA = 0x10;
        } else {
            work->field_8B6 = 0x40;
        }
    }
    head = *(Actor401300PursuitScratch**)G_SCRATCH_HEAD;
    blk  = head - 1;
    work->field_6++;
    *(Actor401300PursuitScratch**)G_SCRATCH_HEAD = blk;
    func_actor_401300_80133A3C(arg0);
    s = blk;
    switch (work->field_8A2) {
        case 3:
            blk->delta.vx = work->home.vx - arg0->field_2C->coords->coord.t[0];
            head[-1].dx   = blk->delta.vx;
            blk->delta.vy = work->home.vy - arg0->field_2C->coords->coord.t[1];
            blk->dy       = blk->delta.vy;
            blk->delta.vz = work->home.vz - arg0->field_2C->coords->coord.t[2];
            blk->dz       = blk->delta.vz;
            blk->dist     = SquareRoot0(head[-1].dx * head[-1].dx + blk->dy * blk->dy + blk->dz * blk->dz);
            if (func_actor_401300_80132C78(arg0->field_2C->coords, (GpRec18*)work->field_AD0, 0xC, 0x57) != 1) {
                func_actor_401300_80132910(arg0, (GpRec18*)work->field_990, 0xC);
            }
            coord           = arg0->field_2C->coords;
            s->angle        = Actor401300_NormalizeYaw(ratan2(head[-1].delta.vx, head[-1].delta.vz) - ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]));
            work->field_8B2 = s->angle;
            if (s->angle > 0x30) {
                s->angle = 0x30;
            } else if (s->angle < -0x30) {
                s->angle = -0x30;
            } else if ((s->dist < 0x898 && Actor401300_Abs(Actor401300_NormalizeYaw(ratan2(-arg0->field_2C->coords->coord.m[2][0], arg0->field_2C->coords->coord.m[2][2]) - work->home.pad)) < 0x200) || work->field_6 > 0xB4) {
                work->field_8A2 = 0x20;
                work->field_89C = 1;
                SndEvt_EnqueueType6(0x551D0008, (s8)Gp_GetObjPan((GpObj38*)arg0->field_2C->coords), (s8)Gp_GetObjDepth((GpObj38*)arg0->field_2C->coords));
                work->field_AB0.flags &= 0x7FFF;
            }
            s->angle += ratan2(-arg0->field_2C->coords->coord.m[2][0], arg0->field_2C->coords->coord.m[2][2]);
            Gfx_RotMatrixY(&arg0->field_2C->coords->coord, s->angle, 1);
            if ((s16)func_actor_401300_8013267C(arg0->field_2C->coords, 0x15E, (s16)((float)((work->field_8A8 + 2) * 30) * 1.5f / 18.0f)) != 0) {
                Actor401300_MoveBy(arg0->field_2C->coords, (s16)((float)((work->field_8A8 + 2) * 30) * 1.5f / 18.0f));
            }
            Actor401300_RescaleYaw(arg0->field_2C->coords, 0x1964);
            arg0->field_2C->coords->flg = 0;
            Gp_UpdateCoord(arg0->field_2C->coords);
            break;
        case 0x20:
            s->angle = ratan2(-arg0->field_2C->coords->coord.m[2][0], arg0->field_2C->coords->coord.m[2][2]);
            Gfx_RotMatrixY(&arg0->field_2C->coords->coord, s->angle, 1);
            Actor401300_MoveForward(arg0->field_2C->coords, 0x12C);
            Actor401300_RescaleYaw(arg0->field_2C->coords, 0x1964);
            arg0->field_2C->coords->flg = 0;
            Gp_UpdateCoord(arg0->field_2C->coords);
            if (work->field_6C & 0x100) {
                work->field_0 = 0;
                Gp_DispatchMsg(Game_GetPtrSlot(7), 0x13F4, enemy->field_40, 0);
            }
            break;
    }
    *(Actor401300PursuitScratch**)G_SCRATCH_HEAD += 1;
}

static __inline__ s32 Actor401300_OutOfRange(SVECTOR* d, s16 r)
{
    u8*                      head;
    Actor401300RangeScratch* blk;
    s32                      ret;

    head                                          = *(u8**)G_SCRATCH_HEAD;
    ((Actor401300RangeScratch*)(head - 0xC))->dx  = d->vx;
    blk                                           = (Actor401300RangeScratch*)(head - 0xC);
    blk->dz                                       = d->vz;
    blk->r                                        = r;
    ((Actor401300RangeScratch*)(head - 0xC))->dx *= ((Actor401300RangeScratch*)(head - 0xC))->dx;
    *(Actor401300RangeScratch**)G_SCRATCH_HEAD    = blk;
    blk->dz                                      *= blk->dz;
    blk->r                                       *= blk->r;
    *(u8**)G_SCRATCH_HEAD                         = head;
    ret                                           = ((Actor401300RangeScratch*)(head - 0xC))->dx + blk->dz >= blk->r;
    return ret;
}

void func_actor_401300_801376E4(Actor401300* arg0)
{
    Actor401300Work*         work;
    TmdObject*               obj;
    GsCOORDINATE2*           coord;
    GsCOORDINATE2*           facing;
    Actor401300ChaseScratch* head;
    Actor401300ChaseScratch* s;

    work = arg0->field_1C;
    if (work->field_4 != 0) {
        head                                       = *(Actor401300ChaseScratch**)G_SCRATCH_HEAD;
        obj                                        = arg0->field_2C;
        *(Actor401300ChaseScratch**)G_SCRATCH_HEAD = head - 1;
        s                                          = head - 1;
        arg0->field_20->node.flags                 = 0;
        obj->flags                                 = 0;
        Tmd_AllocBuffers(obj);
        work->field_970.radius = 0x280;
        work->field_89C        = 1;
        work->field_8A6        = 0x10;
        work->field_8A2        = 3;
        work->field_89E        = 0;
        work->field_8B2        = 0;
        work->field_BF0.flags &= 0x7FFF;
        work->field_AB0.flags |= 0x4000;
        func_actor_401300_80133A3C(arg0);
        Actor401300_ConfigPositionDelta(&Player_Status, arg0->field_2C->coords, &s->delta);
        coord                                       = arg0->field_2C->coords;
        s->turn                                     = Actor401300_NormalizeYaw(ratan2(head[-1].delta.vx, s->delta.vz) - ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]));
        facing                                      = arg0->field_2C->coords;
        s->angle                                    = ratan2(-facing->coord.m[2][0], facing->coord.m[2][2]);
        work->field_C94                             = s->angle;
        work->field_C96                             = s->angle + (u16)s->turn * 2;
        *(Actor401300ChaseScratch**)G_SCRATCH_HEAD += 1;
        return;
    }
    head                                       = *(Actor401300ChaseScratch**)G_SCRATCH_HEAD;
    *(Actor401300ChaseScratch**)G_SCRATCH_HEAD = head - 1;
    s                                          = head - 1;
    func_actor_401300_80133A3C(arg0);
    Actor401300_ConfigPositionDelta(&Player_Status, arg0->field_2C->coords, &s->delta);
    if (work->field_C94 == work->field_C96) {
        if (work->field_D1C < 2 || Actor401300_OutOfRange(&s->delta, 0x384)) {
            work->field_0 = 8;
        } else {
            work->field_0 = 0xB;
        }
    }
    if (work->field_C94 > work->field_C96) {
        work->field_C94 -= 0x89;
        if (work->field_C94 < work->field_C96) {
            work->field_C94 = work->field_C96;
        }
    }
    if (work->field_C94 < work->field_C96) {
        work->field_C94 += 0x89;
        if (work->field_C94 > work->field_C96) {
            work->field_C94 = work->field_C96;
        }
    }
    Gfx_RotMatrixY(&arg0->field_2C->coords->coord, work->field_C94, 1);
    Actor401300_RescaleYaw(arg0->field_2C->coords, 0x1964);
    arg0->field_2C->coords->flg = 0;
    if (work->field_89E == 0) {
        if ((s16)func_actor_401300_8013267C(arg0->field_2C->coords, 0x15E, 0x28) != 0) {
            Actor401300_MoveForward(arg0->field_2C->coords, 0x28);
        }
    } else {
        if ((s16)func_actor_401300_8013267C(arg0->field_2C->coords, 0x15E, 0x14) != 0) {
            Actor401300_MoveForward(arg0->field_2C->coords, 0x14);
        }
    }
    if (func_actor_401300_80132C78(arg0->field_2C->coords, (GpRec18*)work->field_AD0, 0xC, 0x57) != 1) {
        func_actor_401300_80132910(arg0, (GpRec18*)work->field_990, 0xC);
    }
    *(Actor401300ChaseScratch**)G_SCRATCH_HEAD += 1;
}

void func_actor_401300_80137D78(Actor401300* arg0)
{
    Actor401300Work*       work;
    Actor401300AimScratch* head;
    Actor401300AimScratch* aim;
    TmdObject*             obj;
    GsCOORDINATE2*         coord;
    SVECTOR*               dir;
    MATRIX                 mat;
    u16                    angle;
    s32                    kind;

    kind = arg0->field_36;
    work = arg0->field_1C;
    if ((kind & 0xF0) == 0x10) {
        work->field_0 = 0x1E;
        return;
    }
    head                                     = *(Actor401300AimScratch**)G_SCRATCH_HEAD;
    *(Actor401300AimScratch**)G_SCRATCH_HEAD = head - 1;
    aim                                      = head - 1;
    if (work->field_4 != 0) {
        obj                        = arg0->field_2C;
        arg0->field_20->node.flags = 0;
        obj->flags                 = 0;
        Tmd_AllocBuffers(obj);
        work->field_970.radius = 0x140;
        work->field_6          = 0;
        work->field_BF0.flags &= 0x7FFF;
        work->field_AB0.flags |= 0x4000;
        Actor401300_ConfigPositionDelta(&Player_Status, arg0->field_2C->coords, &aim->delta);
        aim->angle = ratan2(head[-1].delta.vx, aim->delta.vz);
        if (work->field_C9C == 0) {
            Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
            if ((Gp_LcgState >> 16) & 1) {
                work->field_C9C = 1;
            } else {
                work->field_C9C = -1;
            }
        }
        if (work->field_C9C == 1) {
            work->field_8A2 = 0x15;
            if (work->field_D1E == 0) {
                angle      = aim->angle + 0x171;
                aim->angle = work->field_CA2 + angle;
            } else {
                aim->angle += work->field_CA2;
            }
            work->field_C9C = -1;
        } else {
            work->field_8A2 = 0x14;
            if (work->field_D1E == 0) {
                angle      = aim->angle - 0x171;
                aim->angle = angle - work->field_CA2;
            } else {
                aim->angle -= work->field_CA2;
            }
            work->field_C9C = 1;
        }
        work->field_89C = 1;
        work->field_8A6 = 0xC;
        work->field_89E = 0;
        func_actor_401300_80133A3C(arg0);
        Gfx_RotMatrixY(&mat, aim->angle, 1);
        dir = &work->field_C8C;
        Gfx_MatrixCol2(&mat, dir);
        VectorNormalSS(dir, dir);
        work->field_C9E = 0xDE;
        work->field_D1E++;
    }
    arg0->field_2C->coords->flg = 0;
    func_actor_401300_80133A3C(arg0);
    arg0->field_2C->coords->flg = 0;
    if (work->field_89E == 0) {
        gte_lddp(work->field_C9E);
        gte_ldsv(&work->field_C8C);
        gte_gpf12_real();
        gte_stsv(aim);
    } else {
        gte_lddp(work->field_C9E >> 1);
        gte_ldsv(&work->field_C8C);
        gte_gpf12_real();
        gte_stsv(aim);
    }
    if ((u32)((u16)work->field_6 - 0xC) < 0xAU) {
        coord              = arg0->field_2C->coords;
        coord->coord.t[0] += aim->delta.vx;
        coord              = arg0->field_2C->coords;
        coord->coord.t[2] += aim->delta.vz;
        func_actor_401300_80132C78(arg0->field_2C->coords, (GpRec18*)work->field_AD0, 0xC, 0x57);
    }
    if (++work->field_6 >= 0x1E) {
        work->field_0 = 7;
    }
    *(Actor401300AimScratch**)G_SCRATCH_HEAD += 1;
}

/// Offset from `coord` to the translation of `m`; `Actor401300_ConfigPositionDelta` for a bare matrix.
static __inline__ void Actor401300_MatrixPositionDelta(MATRIX* m, GsCOORDINATE2* coord, SVECTOR* pos)
{
    pos->vx = m->t[0] - coord->coord.t[0];
    pos->vy = m->t[1] - coord->coord.t[1];
    pos->vz = m->t[2] - coord->coord.t[2];
}

/// `Actor401300_PositionYaw` towards the translation of `m`.
static __inline__ s16 Actor401300_MatrixPositionYaw(Actor401300* actor, SVECTOR* pos, MATRIX* m)
{
    GsCOORDINATE2* coord;
    s32            angle;
    Actor401300_MatrixPositionDelta(m, actor->field_2C->coords, pos);
    coord = actor->field_2C->coords;
    angle = ratan2(pos->vx, pos->vz);
    return Actor401300_NormalizeYaw(angle - ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]));
}

void func_actor_401300_80138160(Actor401300* arg0)
{
    SVECTOR          pos;
    Actor401300Work* work;
    GpEnemy*         enemy;
    GsCOORDINATE2*   coord;
    GameActor*       player;
    PlayerStatus*    config;
    SVECTOR*         p;
    s16              angle;

    enemy  = arg0->field_20;
    work   = arg0->field_1C;
    player = (GameActor*)((Task*)Game_GetPtrSlot(3))->work;
    config = &Player_Status;
    if (work->field_4 != 0) {
        work->field_970.radius = 0x280;
        work->field_BF0.flags &= 0x7FFF;
        work->field_AB0.flags |= 0x4000;
        enemy->node.flags      = 0;
        work->field_89C        = 1;
        work->field_8A6        = 0x10;
        work->field_8A2        = 4;
        func_actor_401300_80133A3C(arg0);
        Gfx_RotMatrixY(&arg0->field_2C->coords->coord, Actor401300_PositionYaw(arg0, &pos, config), 0);
        Actor401300_RescaleYaw(arg0->field_2C->coords, 0x1964);
        pos.vx                      = arg0->field_2C->coords->coord.t[0] - config->coordMtx->t[0];
        pos.vy                      = 0;
        pos.vz                      = arg0->field_2C->coords->coord.t[2] - config->coordMtx->t[2];
        work->field_8B2             = 0;
        work->field_8B4             = 0;
        arg0->field_2C->coords->flg = 0;
        work->field_D1E             = 0;
        work->field_D20             = 0;
    }
    func_actor_401300_80133A3C(arg0);
    if ((work->field_5E & 0x3FF) == 0x10 && player->field_954 != 2) {
        angle = Actor401300_MatrixPositionYaw(arg0, &pos, D_80073B8C);
        if (abs(angle) < 0x10 && !Actor401300_OutOfRange(&pos, 0x44C)) {
            work->field_CAC.field_0 = (s32)&D_actor_401300_801588F0;
            work->field_D00         = 8;
            if (Gp_DispatchMsg(Game_GetPtrSlot(3), 0x3F8, (s32)&work->field_CEC, 0) == 0) {
                work->field_0           = 0xC;
                work->field_D20         = 1;
                work->field_CAC.field_4 = 1;
                Gp_DispatchMsg(Game_GetPtrSlot(3), 0x3FF, (s32)&work->field_CAC, 0);
                work->field_CC0[2] = 0;
                work->field_CC0[1] = 0;
                work->field_CC0[0] = 0;
                work->field_CD0    = 7;
                work->field_CD2    = 1;
                work->field_D22    = 0;
            }
        }
    }
    if (work->field_8A2 == 4 && (work->field_6C & 0x100)) {
        work->field_0 = 7;
    }
    if ((work->field_5E & 0x3FF) > 0x10) {
        p      = &pos;
        pos.vx = arg0->field_2C->coords->coord.t[0] - config->coordMtx->t[0];
        pos.vy = 0;
        pos.vz = arg0->field_2C->coords->coord.t[2] - config->coordMtx->t[2];
        if (!Actor401300_OutOfRange(p, 0x578)) {
            VectorNormalSS(p, p);
            gte_lddp(10);
            gte_ldsv(p);
            gte_gpf12_real();
            gte_stsv(p);
            coord                       = arg0->field_2C->coords;
            coord->coord.t[0]          += pos.vx;
            coord                       = arg0->field_2C->coords;
            coord->coord.t[2]          += pos.vz;
            arg0->field_2C->coords->flg = 0;
        }
    }
}

void func_actor_401300_80138800(Actor401300* arg0)
{
    SVECTOR          dir;
    Actor401300Work* work  = arg0->field_1C;
    GpEnemy*         enemy = arg0->field_20;
    Task*            player;
    SVECTOR*         pdir;

    if (work->field_4 != 0) {
        player                                   = Game_GetPtrSlot(3);
        work->field_970.radius                   = 0x280;
        work->field_BF0.flags                   &= 0x7FFF;
        work->field_AB0.flags                   |= 0x4000;
        enemy->node.flags                        = 0;
        work->field_89C                          = 1;
        work->field_8A6                          = 0x10;
        work->field_8A2                          = 5;
        ((TmdObject*)player->extra)->coords->flg = 0;
        Gp_UpdateCoord(((TmdObject*)player->extra)->coords);
        work->field_CD4.vx = ((TmdObject*)player->extra)->coords->coord.t[0];
        work->field_CD4.vy = ((TmdObject*)player->extra)->coords->coord.t[1];
        work->field_CD4.vz = ((TmdObject*)player->extra)->coords->coord.t[2];
        pdir               = &dir;
        dir.vx             = ((GpCoordXZ*)arg0->field_2C->coords)->field_18 - ((GpCoordXZ*)((TmdObject*)player->extra)->coords)->field_18;
        dir.vy             = 0;
        dir.vz             = ((GpCoordXZ*)arg0->field_2C->coords)->field_20 - ((GpCoordXZ*)((TmdObject*)player->extra)->coords)->field_20;
        VectorNormalSS(pdir, pdir);
        gte_lddp(0x3E8);
        gte_ldsv(pdir);
        gte_gpf12_real();
        gte_stsv(pdir);
        arg0->field_2C->coords->coord.t[0] = ((TmdObject*)player->extra)->coords->coord.t[0] + dir.vx;
        arg0->field_2C->coords->coord.t[2] = ((TmdObject*)player->extra)->coords->coord.t[2] + dir.vz;
        arg0->field_2C->coords->flg        = 0;
        work->field_CE4.vx                 = 0;
        work->field_CE4.vy                 = ratan2(dir.vx, dir.vz);
        work->field_CE4.vz                 = 0;
        Gp_DispatchMsg(player, 0x3E9, (s32)&work->field_CD4, 0);
    }
    func_actor_401300_80133A3C(arg0);
    Gfx_RotMatrixX(&arg0->field_2C->coords[2].coord, -0x80, 0);
    arg0->field_2C->coords[4].flg = 0;
    Gp_UpdateCoord(&arg0->field_2C->coords[2]);
    Gfx_RotMatrixX(&arg0->field_2C->coords[3].coord, -0x80, 0);
    arg0->field_2C->coords[5].flg = 0;
    Gp_UpdateCoord(&arg0->field_2C->coords[3]);
    if (work->field_8A2 == 5 && (work->field_6C & 0x100)) {
        work->field_910.coord      = &arg0->field_2C->coords[1];
        work->field_910.spawnArgLo = 0x300;
        work->field_910.spawnArgHi = 2;
        func_800FDB18(Gp_GetIdParam1(0x1001) & 0xFFFF, &arg0->field_2C->coords[5], NULL, &work->field_910);
        work->field_0 = 0xD;
    }
}

void func_actor_401300_80138B24(Actor401300* arg0)
{
    Actor401300Work* work   = arg0->field_1C;
    GpEnemy*         enemy  = arg0->field_20;
    Task*            player = Game_GetPtrSlot(3);

    if (work->field_4 != 0) {
        work->field_8A6 = 0x10;
        work->field_8A2 = 6;
        work->field_89C = 2;
        if ((s16)Gp_DispatchMsg(Game_GetPtrSlot(3), 0x3F9, Gp_PackObjPair((GpObj50*)enemy, 0), 0) == 1) {
            ((GameActor*)player->work)->field_956 = 0xA;
        }
        work->field_CAC.field_4 = 2;
        Gp_DispatchMsg(Game_GetPtrSlot(3), 0x3FF, (s32)&work->field_CAC, 0);
        work->field_D22 = 0;
    }
    if (work->field_6C & 2) {
        work->field_910.coord      = &arg0->field_2C->coords[1];
        work->field_910.spawnArgLo = 0x300;
        work->field_910.spawnArgHi = 2;
        func_800FDB18(Gp_GetIdParam1(0x1001) & 0xFFFF, &arg0->field_2C->coords[5], NULL, &work->field_910);
        work->field_0 = 0xE;
    }
    work->field_898 = work->field_5E & 0x3FF;
    func_actor_401300_80133A3C(arg0);
    Gfx_RotMatrixX(&arg0->field_2C->coords[2].coord, -0x80, 0);
    arg0->field_2C->coords[4].flg = 0;
    Gp_UpdateCoord(&arg0->field_2C->coords[3]);
    Gfx_RotMatrixX(&arg0->field_2C->coords[3].coord, -0x80, 0);
    arg0->field_2C->coords[5].flg = 0;
    Gp_UpdateCoord(&arg0->field_2C->coords[2]);
}

void func_actor_401300_80138CF8(Actor401300* arg0)
{
    Actor401300Work* work;
    GpEnemy*         enemy;

    work  = arg0->field_1C;
    enemy = arg0->field_20;
    if (work->field_4 != 0) {
        arg0->field_2C->flags  = 0;
        work->field_970.radius = 0x280;
        work->field_BF0.flags &= 0x7FFF;
        work->field_AB0.flags |= 0x4000;
        enemy->node.flags      = 0;
        work->field_89C        = 1;
        work->field_8A2        = 0xA;
        work->field_89E        = 0;
        work->field_8A6        = 0x10;
        work->field_8B4        = 0;
        work->field_8B2        = 0;
        if (enemy->field_40 <= 0) {
            Gp_SetStateF0Byte3(1);
        }
        work->field_8B6        = 0x20;
        work->field_8BA        = 8;
        work->field_970.flags |= 0x4000;
    }
    if (work->field_8A2 == 0xA && (s16)func_actor_401300_8013267C(arg0->field_2C->coords, 0x15E, -0x57) != 0) {
        Actor401300_MoveForward(arg0->field_2C->coords, -0x57);
    }
    func_actor_401300_80133A3C(arg0);
    if (func_actor_401300_801323B0(arg0->field_2C->coords, work->field_990, 0xC) == 0) {
        func_actor_401300_80132C78(arg0->field_2C->coords, (GpRec18*)work->field_AD0, 0xC, 0x57);
    }
    arg0->field_2C->coords->flg = 0;
    if (work->field_6C & 0x100) {
        if (work->field_8A2 == 0xA) {
            work->field_8A2 = 0xB;
            work->field_89C = 2;
            func_actor_401300_80133A3C(arg0);
        }
        if ((work->field_6C & 0x100) && work->field_8A2 == 0xB) {
            work->field_970.flags &= 0xBFFF;
            if (enemy->field_40 <= 0) {
                work->field_0 = 0x15;
            } else if (enemy->field_4C & 2) {
                work->field_0 = 4;
            } else {
                work->field_0 = 0x11;
            }
        }
    }
}

void func_actor_401300_80138FCC(Actor401300* arg0)
{
    Actor401300Work* work;
    GpEnemy*         enemy;

    work  = arg0->field_1C;
    enemy = arg0->field_20;
    if (work->field_4 != 0) {
        arg0->field_2C->flags  = 0;
        work->field_970.radius = 0x280;
        work->field_BF0.flags &= 0x7FFF;
        work->field_AB0.flags |= 0x4000;
        enemy->node.flags      = 0;
        work->field_89C        = 1;
        work->field_8A2        = 0xC;
        work->field_8A6        = 0x10;
        work->field_8B4        = 0;
        work->field_8B2        = 0;
        if (enemy->field_40 <= 0) {
            Gp_SetStateF0Byte3(1);
        }
        work->field_8B6        = 0x20;
        work->field_8BA        = 8;
        work->field_970.flags |= 0x4000;
    }
    func_actor_401300_80133A3C(arg0);
    if (func_actor_401300_801323B0(arg0->field_2C->coords, work->field_990, 0xC) == 0) {
        func_actor_401300_80132C78(arg0->field_2C->coords, (GpRec18*)work->field_AD0, 0xC, 0x57);
    }
    arg0->field_2C->coords->flg = 0;
    if (work->field_6C & 0x100) {
        work->field_970.flags &= 0xBFFF;
        if (enemy->field_40 <= 0) {
            work->field_0 = 0x15;
        } else if (enemy->field_4C & 2) {
            work->field_0 = 4;
        } else {
            work->field_0 = 0x11;
        }
    }
}

/// Rebuild `coord`'s Y rotation from its current yaw, scaled by `xz` on X/Z
/// and `y` on Y. `Actor401300_RescaleYaw` with a separate Y scale.
static __inline__ void Actor401300_RescaleYawXZ(GsCOORDINATE2* coord, s32 xz, s16 y)
{
    void*                  head;
    Actor401300RotScratch* blk;
    s16                    ang;
    u16                    m22;

    head                    = *(void**)G_SCRATCH_HEAD;
    blk                     = (Actor401300RotScratch*)((u8*)head - 0x34);
    *(void**)G_SCRATCH_HEAD = blk;

    ang        = ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]);
    blk->angle = ang;
    Gfx_RotMatrixY(&blk->m, ang, 1);
    blk->scale.vx = xz;
    blk->scale.vy = y;
    blk->scale.vz = xz;
    ScaleMatrix(&blk->m, &blk->scale);

    coord->coord.m[0][0]    = *(u16*)&((Actor401300RotScratch*)((u8*)head - 0x34))->m.m[0][0];
    coord->coord.m[0][1]    = *(u16*)&blk->m.m[0][1];
    coord->coord.m[0][2]    = *(u16*)&blk->m.m[0][2];
    coord->coord.m[1][0]    = *(u16*)&blk->m.m[1][0];
    coord->coord.m[1][1]    = *(u16*)&blk->m.m[1][1];
    coord->coord.m[1][2]    = *(u16*)&blk->m.m[1][2];
    coord->coord.m[2][0]    = *(u16*)&blk->m.m[2][0];
    coord->coord.m[2][1]    = *(u16*)&blk->m.m[2][1];
    m22                     = *(u16*)&blk->m.m[2][2];
    *(void**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD + 0x34;
    coord->flg              = 0;
    coord->coord.m[2][2]    = m22;
}

/// Collapse state: spawns effect 0x600A5 at the actor's view-space position on
/// frame 30, switches the light mode on 30/42, and from frame 26 squashes the
/// root coordinate's Y scale; state 0x24 follows after frame 64.
void func_actor_401300_80139134(Actor401300* arg0)
{
    Actor401300Work*     work;
    GpEnemy*             enemy;
    TmdObject*           obj;
    Actor401300MatWords* w;
    SVECTOR              pos;
    s16                  t;

    work  = arg0->field_1C;
    obj   = arg0->field_2C;
    enemy = arg0->field_20;
    if (work->field_4 != 0) {
        obj->flags            = 0;
        work->field_BF0.flags = (u16)(work->field_BF0.flags & 0x7FFF);
        work->field_AB0.flags = (u16)(work->field_AB0.flags & 0xBFFF);
        enemy->node.flags     = 1;
        work->field_6         = 0;
        work->field_8A6       = 8;
    }
    func_actor_401300_80133A3C(arg0);
    if (work->field_6 <= 0x400) {
        switch (++work->field_6) {
            case 30:
                w          = (Actor401300MatWords*)&work->field_8C0.coord;
                w->m00_m01 = 0x1000;
                w->m02_m10 = 0;
                w->m11_m12 = 0x1000;
                w->m20_m21 = 0;
                w->m22     = 0x1000;
                pos.vx     = 0;
                pos.vy     = 0;
                pos.vz     = 0;
                Actor401300_TransformToView(&arg0->field_2C->coords[2], &pos);
                work->field_8C0.sub        = &gGfxViewCoord;
                work->field_8C0.coord.t[0] = pos.vx;
                work->field_8C0.coord.t[1] = arg0->field_2C->coords->coord.t[1];
                work->field_8C0.coord.t[2] = pos.vz;
                work->field_8C0.flg        = 0;
                Gp_UpdateCoord(&work->field_8C0);
                Gp_SetLightMode(enemy, 1);
                Gp_SpawnEff(0x600A5, &work->field_8C0, 3, NULL);
                break;
            case 48:
                arg0->field_2C->flags = 2;
                break;
            case 42:
                Gp_SetLightMode(enemy, 2);
                break;
            case 64:
                arg0->field_2C->flags = 0x80;
                break;
        }
        t = work->field_6;
        if (t >= 0x1A) {
            Actor401300_RescaleYawXZ(arg0->field_2C->coords, 0x1964, 0x1964 - (t - 0x14) * 16);
        }
        if (work->field_6 > 0x40 && work->field_D20 == 0) {
            work->field_0 = 0x24;
        }
    }
}

void func_actor_401300_80139520(Actor401300* arg0)
{
    Actor401300Work* work;
    GpEnemy*         enemy;
    TmdObject*       obj;
    GsCOORDINATE2*   coord;
    SVECTOR          delta;
    SVECTOR*         d;

    work = arg0->field_1C;
    if (work->field_4 != 0) {
        obj        = arg0->field_2C;
        enemy      = arg0->field_20;
        obj->flags = 0;
        Tmd_AllocBuffers(obj);
        work->field_970.radius = 0x280;
        work->field_BF0.flags &= 0x7FFF;
        work->field_AB0.flags &= 0xBFFF;
        enemy->node.flags      = 0;
        work->field_6          = 0;
        work->field_C68        = work->field_C48;
        work->field_8A2        = 0xE;
        work->field_89C        = 1;
        work->field_8A6        = work->field_8A8;
    }
    if (work->field_6 > 0x960) {
        Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
        if (!((Gp_LcgState >> 16) & 0xF)) {
            return;
        }
    } else {
        work->field_6++;
    }
    coord    = arg0->field_2C->coords;
    d        = &delta;
    delta.vx = D_80073B8C->t[0] - coord->coord.t[0];
    d->vy    = D_80073B8C->t[1] - coord->coord.t[1];
    d->vz    = D_80073B8C->t[2] - coord->coord.t[2];
    if (!Actor401300_OutOfRange(d, 3000)) {
        work->field_0 = 6;
    }
    if (D_801153F2[0] & 1) {
        Gp_ArmStateF0(1);
        work->field_0 = 6;
    }
    func_actor_401300_80133A3C(arg0);
    if (work->field_8A2 == 0xE && (work->field_6C & 2)) {
        Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
        if ((Gp_LcgState >> 16) & 1) {
            work->field_8A2 = 0xF;
            work->field_89C = 1;
            func_actor_401300_80133A3C(arg0);
        }
    }
    if (work->field_8A2 == 0xF && (work->field_6C & 0x100)) {
        work->field_8A2 = 0xE;
        work->field_89C = 1;
        func_actor_401300_80133A3C(arg0);
    }
}

void func_actor_401300_801397F8(Actor401300* arg0)
{
    Actor401300Work* work;
    GpEnemy*         enemy;
    TmdObject*       obj;
    GsCOORDINATE2*   coord;
    SVECTOR          delta;
    SVECTOR*         d;
    s32              sound;
    s32              pan;

    work  = arg0->field_1C;
    enemy = arg0->field_20;
    if (work->field_4 != 0) {
        obj                     = arg0->field_2C;
        D_actor_401300_80158878 = &D_actor_401300_80152BB8;
        work->field_8A2         = 0x10;
        work->field_89C         = 2;
        obj->flags              = 0;
        Tmd_AllocBuffers(obj);
        work->field_970.radius = 0x280;
        work->field_BF0.flags &= 0x7FFF;
        work->field_AB0.flags |= 0x4000;
        enemy->node.flags      = 0;
        work->field_8B4        = 0;
        work->field_8A6        = 0x10;
        work->field_8B2        = 0;
        work->field_6          = 0;
    } else if (work->field_6 == 0) {
        sound = ((enemy->field_8 >> 0xC) << 8) | 0x51030008;
        pan   = (s8)Gp_GetObjPan((GpObj38*)arg0->field_2C->coords);
        SndEvt_EnqueueType6(sound, pan, (s8)Gp_GetObjDepth((GpObj38*)arg0->field_2C->coords));
        work->field_6 = 1;
    }
    func_actor_401300_80133A3C(arg0);
    if ((work->field_5E & 0x3FF) == 4 && work->field_8BC != (work->field_5E & 0x3FF)) {
        work->field_910.coord      = arg0->field_2C->coords + 1;
        work->field_910.spawnArgLo = 0x300;
        work->field_910.spawnArgHi = 2;
        func_800FDB18((u16)Gp_GetIdParam1(0x1001), arg0->field_2C->coords + 5, NULL, &work->field_910);
    }
    work->field_8BC = work->field_5E & 0x3FF;
    coord           = arg0->field_2C->coords;
    d               = &delta;
    delta.vx        = D_80073B8C->t[0] - coord->coord.t[0];
    d->vy           = D_80073B8C->t[1] - coord->coord.t[1];
    d->vz           = D_80073B8C->t[2] - coord->coord.t[2];
    if (!Actor401300_OutOfRange(d, 3000)) {
        SndEvt_EnqueueType7(0x51030008, 1);
        Gp_ArmStateF0(1);
        work->field_0 = 6;
    }
    if (D_801153F2[0] & 1) {
        Gp_ArmStateF0(1);
        work->field_0 = 6;
    }
}

void func_actor_401300_80139AB0(Actor401300* arg0)
{
    Actor401300Work*        work;
    TmdObject*              obj;
    GsCOORDINATE2*          coord;
    Actor401300TurnScratch* s;
    GsCOORDINATE2*          facing;

    work = arg0->field_1C;
    if (work->field_4 != 0) {
        obj                        = arg0->field_2C;
        arg0->field_20->node.flags = 0;
        obj->flags                 = 0;
        Tmd_AllocBuffers(obj);
        work->field_970.radius = 0x280;
        work->field_89C        = 1;
        work->field_8A6        = 0x10;
        work->field_8A2        = 2;
        work->field_8B6        = 0x60;
        work->field_8BA        = 8;
        work->field_89E        = 0;
        work->field_BF0.flags &= 0x7FFF;
        work->field_AB0.flags |= 0x4000;
        func_actor_401300_80133A3C(arg0);
        return;
    }
    if (work->field_8B6 == work->field_8B8) {
        if (work->field_8B6 == 0x60) {
            work->field_8B6 = 0x20;
        } else {
            work->field_8B6 = 0x60;
        }
    }
    *(Actor401300TurnScratch**)G_SCRATCH_HEAD -= 1;
    s                                          = *(Actor401300TurnScratch**)G_SCRATCH_HEAD;
    s->delta.vx                                = work->field_C[work->field_16].x - arg0->field_2C->coords->coord.t[0];
    s->delta.vy                                = 0;
    s->delta.vz                                = work->field_C[work->field_16].z - arg0->field_2C->coords->coord.t[2];
    if (!Actor401300_OutOfRange(&s->delta, 0xA0)) {
        if (work->field_16 == 0) {
            work->field_16 = 1;
        } else {
            work->field_16 = 0;
        }
    }
    func_actor_401300_80133A3C(arg0);
    coord           = arg0->field_2C->coords;
    s->angle        = Actor401300_NormalizeYaw(ratan2(s->delta.vx, s->delta.vz) - ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]));
    work->field_8B2 = s->angle;
    if (s->angle > 0x20) {
        s->angle = 0x20;
    }
    if (s->angle < -0x20) {
        s->angle = -0x20;
    }
    facing    = arg0->field_2C->coords;
    s->angle += ratan2(-facing->coord.m[2][0], facing->coord.m[2][2]);
    Gfx_RotMatrixY(&arg0->field_2C->coords->coord, s->angle, 1);
    Actor401300_RescaleYaw(arg0->field_2C->coords, 0x1964);
    if (work->field_89E == 0) {
        if ((s16)func_actor_401300_8013267C(arg0->field_2C->coords, 0x15E, 0xA) != 0) {
            Actor401300_MoveForward(arg0->field_2C->coords, 0xA);
        }
    }
    if (func_actor_401300_80132C78(arg0->field_2C->coords, (GpRec18*)work->field_AD0, 0xC, 0x57) == 0) {
        func_actor_401300_80132910(arg0, (GpRec18*)work->field_990, 0xC);
    }
    arg0->field_2C->coords->flg = 0;
    Actor401300_ConfigPositionDelta(&Player_Status, arg0->field_2C->coords, &s->delta);
    if (!Actor401300_OutOfRange(&s->delta, 0x7D0)) {
        work->field_0 = 6;
    } else if (!Actor401300_OutOfRange(&s->delta, 0xFA0)) {
        coord    = arg0->field_2C->coords;
        s->angle = Actor401300_NormalizeYaw(ratan2(s->delta.vx, s->delta.vz) - ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]));
        if (ABS(s->angle) < 0x300) {
            work->field_0 = 6;
        }
    }
    if (*(u32*)&Gp_StateF0 & 0xD0000) {
        work->field_0 = 6;
    }
    *(Actor401300TurnScratch**)G_SCRATCH_HEAD += 1;
}

void func_actor_401300_8013A208(Actor401300* arg0)
{
    Actor401300Work*        work;
    GpEnemy*                enemy;
    TmdObject*              obj;
    GsCOORDINATE2*          coord;
    Actor401300TurnScratch* turn;
    u16                     next;

    work = arg0->field_1C;
    if (work->field_4 != 0) {
        enemy           = arg0->field_20;
        obj             = arg0->field_2C;
        work->field_8A2 = 0x12;
        work->field_89C = 1;
        obj->flags      = 0;
        Tmd_AllocBuffers(obj);
        work->field_970.radius = 0x280;
        work->field_BF0.flags &= 0x7FFF;
        work->field_AB0.flags |= 0x4000;
        enemy->node.flags      = 0;
        work->field_8B4        = 0;
        work->field_8A6        = 0x1E;
    }
    *(Actor401300TurnScratch**)G_SCRATCH_HEAD -= 1;
    turn                                       = *(Actor401300TurnScratch**)G_SCRATCH_HEAD;
    turn->angle                                = Actor401300_PositionYaw(arg0, &turn->delta, &Player_Status);
    work->field_8B2                            = turn->angle;
    if (turn->angle > 0x40) {
        turn->angle = 0x40;
    }
    if (turn->angle < -0x40) {
        turn->angle = -0x40;
    }
    coord        = arg0->field_2C->coords;
    turn->angle += ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]);
    Gfx_RotMatrixY(&arg0->field_2C->coords->coord, turn->angle, 1);
    if (func_actor_401300_80132C78(arg0->field_2C->coords, (GpRec18*)work->field_AD0, 0xC, 0x57) == 0) {
        func_actor_401300_80132910(arg0, (GpRec18*)work->field_990, 0xC);
    }
    if ((s16)func_actor_401300_8013267C(arg0->field_2C->coords, 0x15E, work->field_C98) != 0) {
        Actor401300_MoveForwardNonzero(arg0->field_2C->coords, work->field_C98);
    }
    if (work->field_C98 > 0) {
        next            = work->field_C98 - 0xA;
        work->field_C98 = next;
        if ((s16)next < 0) {
            work->field_C98 = 0;
        }
    }
    func_actor_401300_80133A3C(arg0);
    if ((work->field_6C & 0x100) || work->field_C98 == 0) {
        work->field_0 = 9;
    }
    *(Actor401300TurnScratch**)G_SCRATCH_HEAD += 1;
}

void func_actor_401300_8013A5C0(Actor401300* arg0)
{
    Actor401300Work*       work;
    TmdObject*             obj;
    GsCOORDINATE2*         coord;
    Actor401300AimScratch* aim;

    work = arg0->field_1C;
    if (work->field_4 != 0) {
        obj                        = arg0->field_2C;
        arg0->field_20->node.flags = 0;
        obj->flags                 = 0;
        Tmd_AllocBuffers(obj);
        work->field_970.radius = 0x280;
        work->field_89C        = 1;
        work->field_8A6        = 0x16;
        work->field_8A2        = 2;
        work->field_89E        = 0;
        work->field_BF0.flags &= 0x7FFF;
        work->field_AB0.flags |= 0x4000;
        func_actor_401300_80133A3C(arg0);
        return;
    }
    func_actor_401300_80133A3C(arg0);
    *(Actor401300AimScratch**)G_SCRATCH_HEAD -= 1;
    aim                                       = *(Actor401300AimScratch**)G_SCRATCH_HEAD;
    aim->angle                                = Actor401300_PositionYaw(arg0, &aim->delta, &Player_Status);
    work->field_8B2                           = aim->angle;
    if (ABS(aim->angle) <= 0x80 && work->field_8A2 == 2) {
        work->field_8A6 = 0x16;
        work->field_8A2 = 0x11;
        work->field_89C = 1;
        work->field_6   = 0;
        func_actor_401300_80133A3C(arg0);
    }
    if (aim->angle > 0x80) {
        aim->angle = 0x80;
    }
    if (aim->angle < -0x80) {
        aim->angle = -0x80;
    } else {
        aim->angle = aim->angle >> 1;
    }
    coord       = arg0->field_2C->coords;
    aim->angle += ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]);
    Gfx_RotMatrixY(&arg0->field_2C->coords->coord, aim->angle, 1);
    Actor401300_RescaleYaw(arg0->field_2C->coords, 0x1964);
    arg0->field_2C->coords->flg = 0;
    if (work->field_8A2 == 0x11) {
        work->field_6++;
        if ((s16)func_actor_401300_8013267C(arg0->field_2C->coords, 0x15E, -0x10) != 0) {
            Actor401300_MoveForward(arg0->field_2C->coords, -0x10);
        }
        if (func_actor_401300_80132C78(arg0->field_2C->coords, (GpRec18*)work->field_AD0, 0xC, 0x57) == 0) {
            func_actor_401300_80132910(arg0, (GpRec18*)work->field_990, 0xC);
        }
        arg0->field_2C->coords->flg = 0;
        if (work->field_6 >= 0x13) {
            if (work->field_8B2 <= 0) {
                Gfx_RotMatrixY(&arg0->field_2C->coords->coord, 0x4B0, 0);
            } else {
                Gfx_RotMatrixY(&arg0->field_2C->coords->coord, -0x4B0, 0);
            }
            work->field_0 = 7;
        }
    }
    *(Actor401300AimScratch**)G_SCRATCH_HEAD += 1;
}

void func_actor_401300_8013AAE8(Actor401300* arg0)
{
    Actor401300Work*       work;
    TmdObject*             obj;
    GsCOORDINATE2*         coord;
    Actor401300AimScratch* aim;

    work = arg0->field_1C;
    if (work->field_4 != 0) {
        obj                        = arg0->field_2C;
        arg0->field_20->node.flags = 0;
        obj->flags                 = 0;
        Tmd_AllocBuffers(obj);
        work->field_970.radius = 0x280;
        work->field_89C        = 1;
        work->field_8A6        = 0x10;
        work->field_8A2        = 0x13;
        work->field_89E        = 0;
        work->field_BF0.flags &= 0x7FFF;
        work->field_AB0.flags &= 0xBFFF;
        func_actor_401300_80133A3C(arg0);
        work->field_6 = 0;
        return;
    }
    work->field_6++;
    *(Actor401300AimScratch**)G_SCRATCH_HEAD -= 1;
    aim                                       = *(Actor401300AimScratch**)G_SCRATCH_HEAD;
    arg0->field_2C->coords->flg               = 0;
    if ((work->field_6C & 0x100) || work->field_6 >= 0xB) {
        work->field_0 = 0xB;
    }
    aim->angle      = Actor401300_PositionYaw(arg0, &aim->delta, &Player_Status);
    work->field_8B2 = aim->angle;
    if (aim->angle > 0x20) {
        aim->angle = 0x20;
    }
    if (aim->angle < -0x20) {
        aim->angle = -0x20;
    }
    coord       = arg0->field_2C->coords;
    aim->angle += ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]);
    Gfx_RotMatrixY(&arg0->field_2C->coords->coord, aim->angle, 1);
    Actor401300_RescaleYaw(arg0->field_2C->coords, 0x1964);
    func_actor_401300_80133A3C(arg0);
    *(Actor401300AimScratch**)G_SCRATCH_HEAD += 1;
}

void func_actor_401300_8013AE48(Actor401300* arg0)
{
    Actor401300Work*       work;
    TmdObject*             obj;
    GsCOORDINATE2*         coord;
    Actor401300AimScratch* aim;

    work = arg0->field_1C;
    if (work->field_4 != 0) {
        obj                        = arg0->field_2C;
        arg0->field_20->node.flags = 0;
        obj->flags                 = 0;
        Tmd_AllocBuffers(obj);
        work->field_970.radius = 0x280;
        work->field_89C        = 2;
        work->field_8A6        = 8;
        work->field_8A2        = 0x13;
        work->field_89E        = 0;
        work->field_BF0.flags &= 0x7FFF;
        work->field_AB0.flags &= 0xBFFF;
        func_actor_401300_80133A3C(arg0);
        func_actor_401300_80133A3C(arg0);
        work->field_6   = 0;
        work->field_8B4 = 0;
        return;
    }
    work->field_6++;
    *(Actor401300AimScratch**)G_SCRATCH_HEAD -= 1;
    aim                                       = *(Actor401300AimScratch**)G_SCRATCH_HEAD;
    aim->angle                                = Actor401300_PositionYaw(arg0, &aim->delta, &Player_Status);
    if (work->field_8B2 < aim->angle) {
        if (aim->angle - work->field_8B2 > 0x28) {
            work->field_8B2 += 0x28;
        } else {
            work->field_8B2 = aim->angle;
        }
    } else if (work->field_8B2 - aim->angle > 0x28) {
        work->field_8B2 -= 0x28;
    } else {
        work->field_8B2 = aim->angle;
    }
    coord      = arg0->field_2C->coords;
    aim->angle = ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]);
    Gfx_RotMatrixY(&arg0->field_2C->coords->coord, aim->angle, 1);
    Actor401300_RescaleYaw(arg0->field_2C->coords, 0x1964);
    func_actor_401300_80133A3C(arg0);
    if (work->field_6 < 0x32) {
        Gfx_RotMatrixX(&arg0->field_2C->coords[1].coord, 0x40, 0);
        arg0->field_2C->coords[1].flg = 0;
        Gp_UpdateCoord(&arg0->field_2C->coords[1]);
        Gfx_RotMatrixX(&arg0->field_2C->coords[2].coord, 0x80, 0);
        arg0->field_2C->coords[2].flg = 0;
        Gp_UpdateCoord(&arg0->field_2C->coords[2]);
        Gfx_RotMatrixX(&arg0->field_2C->coords[3].coord, 0x80, 0);
        arg0->field_2C->coords[3].flg = 0;
        Gp_UpdateCoord(&arg0->field_2C->coords[3]);
        Gfx_RotMatrixX(&arg0->field_2C->coords[4].coord, 0x80, 0);
        arg0->field_2C->coords[4].flg = 0;
        Gp_UpdateCoord(&arg0->field_2C->coords[4]);
        Gfx_RotMatrixX(&arg0->field_2C->coords[5].coord, 0x100, 0);
        arg0->field_2C->coords[4].flg = 0;
        Gp_UpdateCoord(&arg0->field_2C->coords[4]);
    } else {
        Gfx_RotMatrixX(&arg0->field_2C->coords[1].coord, 0x40 >> ((work->field_6 - 0x31) / 4), 0);
        arg0->field_2C->coords[1].flg = 0;
        Gp_UpdateCoord(&arg0->field_2C->coords[1]);
        Gfx_RotMatrixX(&arg0->field_2C->coords[2].coord, 0x80 >> ((work->field_6 - 0x30) / 4), 0);
        arg0->field_2C->coords[2].flg = 0;
        Gp_UpdateCoord(&arg0->field_2C->coords[2]);
        Gfx_RotMatrixX(&arg0->field_2C->coords[3].coord, 0x80 >> ((work->field_6 - 0x2F) / 4), 0);
        arg0->field_2C->coords[3].flg = 0;
        Gp_UpdateCoord(&arg0->field_2C->coords[3]);
        Gfx_RotMatrixX(&arg0->field_2C->coords[4].coord, 0x80 >> ((work->field_6 - 0x2E) / 4), 0);
        arg0->field_2C->coords[4].flg = 0;
        Gp_UpdateCoord(&arg0->field_2C->coords[4]);
        Gfx_RotMatrixX(&arg0->field_2C->coords[5].coord, 0x100 >> ((work->field_6 - 0x31) / 4), 0);
        arg0->field_2C->coords[4].flg = 0;
        Gp_UpdateCoord(&arg0->field_2C->coords[4]);
        aim->angle = Actor401300_PositionYaw(arg0, &aim->delta, &Player_Status);
        if (aim->angle > 0x24) {
            aim->angle = 0x24;
        } else if (aim->angle < -0x24) {
            aim->angle = -0x24;
        }
        if (ABS(aim->angle) < 0x24) {
            work->field_0 = 7;
        }
        coord       = arg0->field_2C->coords;
        aim->angle += ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]);
        Gfx_RotMatrixY(&arg0->field_2C->coords->coord, aim->angle, 1);
        Actor401300_RescaleYaw(arg0->field_2C->coords, 0x1964);
        arg0->field_2C->coords->flg = 0;
    }
    *(Actor401300AimScratch**)G_SCRATCH_HEAD += 1;
}

/// Tint a freshly spawned effect model from the enemy's area record.
static __inline__ void Actor401300_TintEffect(GpEffWork* eff, GpEnemy* enemy)
{
    GpAreaKey  key;
    GpAreaKey* sessionKey;
    GpAreaKey* keyPtr;
    u8         areaByte0;
    GpAreaRec* rec;
    GpCdRec10* entry;
    TmdObject* model;
    s32        idx;
    u32        raw;

    if (eff != NULL) {
        sessionKey = (GpAreaKey*)&gGameSession->at4.loc;
        raw        = enemy->field_8;
        model      = (TmdObject*)eff->field_0->extra;
        key.stage  = sessionKey->stage;
        key.area   = sessionKey->area;
        key.room   = sessionKey->room;
        areaByte0  = gGameSession->at4.loc.view;
        idx        = raw >> 12;
        SOFT_BARRIER();
        keyPtr = &key;
        TOUCH_REG(keyPtr);
        key.view = areaByte0;
        Gp_SyncAreaKeyIndex(keyPtr);
        rec          = Gp_GetNestedAreaRec(&key);
        entry        = (GpCdRec10*)((idx << 4) + (s32)rec->field_0);
        model->tpage = entry->field_D;
        model->clut  = entry->field_E;
        if (model->buffer != NULL) {
            tmdProcessStream(model);
            tmdProcessStream(model);
        }
    }
}

void func_actor_401300_8013B6E8(Actor401300* arg0)
{
    SVECTOR          vec;
    Actor401300Work* work;
    GpEnemy*         enemy;
    u16              next;

    work  = arg0->field_1C;
    enemy = arg0->field_20;
    if (work->field_4 != 0) {
        arg0->field_2C->flags  = 0x80;
        work->field_970.radius = 0x280;
        work->field_AB0.flags  = (u16)(work->field_AB0.flags & 0xBFFF);
        work->field_BF0.flags  = (u16)(work->field_BF0.flags & 0x7FFF);
        enemy->node.flags      = 1;
        work->field_8B2        = 0;
        work->field_6          = 0U;
        vec.vx                 = 0x64;
        vec.vz                 = 0;
        vec.vy                 = 0;
        Gp_SpawnEff(0x60030, arg0->field_2C->coords + 1, 0x10300, &vec);
    }
    next          = work->field_6 + 1;
    work->field_6 = next;
    if ((s16)next == 3) {
        D_80114B78[0] = &D_actor_401300_80147894;
        vec.vz        = 0x64;
        vec.vy        = 0;
        vec.vx        = 0;
        Actor401300_TintEffect(Gp_SpawnEff(0xA0005, arg0->field_2C->coords + 9, 0x200, &vec), enemy);
    }
    if (work->field_6 == 5) {
        D_80114B78[0] = &D_actor_401300_80147894;
        vec.vy        = 0;
        vec.vx        = 0;
        Actor401300_TintEffect(Gp_SpawnEff(0xA0005, arg0->field_2C->coords + 12, 0x200, &vec), enemy);
    }
    if (work->field_6 == 7) {
        D_80114B78[0] = &D_actor_401300_80148A14;
        Actor401300_TintEffect(Gp_SpawnEff(0xA0005, arg0->field_2C->coords + 1, 0x200, NULL), enemy);
    }
    if (work->field_6 == 8) {
        D_80114B78[0] = &D_actor_401300_80148808;
        Actor401300_TintEffect(Gp_SpawnEff(0xA0005, arg0->field_2C->coords + 3, 0x200, NULL), enemy);
    }
    if (work->field_6 >= 0x3D && work->field_D20 == 0) {
        work->field_0 = 0x24;
    }
}

/// Rebuild `coord`'s Y rotation from its current yaw at unit scale. Same body
/// as `Actor01900_ResetYaw`.
static __inline__ void Actor401300_ResetYaw(GsCOORDINATE2* coord)
{
    void*                  head;
    Actor401300RotScratch* blk;
    s16                    ang;

    head                    = *(void**)G_SCRATCH_HEAD;
    blk                     = (Actor401300RotScratch*)((u8*)head - 0x34);
    *(void**)G_SCRATCH_HEAD = blk;

    ang        = ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]);
    blk->angle = ang;
    Gfx_RotMatrixY(&blk->m, ang, 1);
    blk->scale.vz = 1;
    blk->scale.vy = 1;
    blk->scale.vx = 1;
    ScaleMatrix(&blk->m, &blk->scale);

    coord->coord.m[0][0]    = *(u16*)&blk->m.m[0][0];
    coord->coord.m[0][1]    = *(u16*)&blk->m.m[0][1];
    coord->coord.m[0][2]    = *(u16*)&blk->m.m[0][2];
    coord->coord.m[1][0]    = *(u16*)&blk->m.m[1][0];
    coord->coord.m[1][1]    = *(u16*)&blk->m.m[1][1];
    coord->coord.m[1][2]    = *(u16*)&blk->m.m[1][2];
    coord->coord.m[2][0]    = *(u16*)&blk->m.m[2][0];
    coord->coord.m[2][1]    = *(u16*)&blk->m.m[2][1];
    coord->coord.m[2][2]    = *(u16*)&blk->m.m[2][2];
    coord->flg              = 0;
    *(void**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD + 0x34;
}

void func_actor_401300_8013BB30(Actor401300* arg0)
{
    SVECTOR          vec;
    Actor401300Work* work;
    GpEnemy*         enemy;
    u16              next;
    s16              cur;

    work  = arg0->field_1C;
    enemy = arg0->field_20;
    if (work->field_4 != 0) {
        work->field_970.radius = 0x280;
        work->field_AB0.flags  = (u16)(work->field_AB0.flags | 0x4000);
        work->field_BF0.flags  = (u16)(work->field_BF0.flags & 0x7FFF);
        enemy->node.flags      = 1;
        work->field_8B2        = 0;
        work->field_6          = 0;
        vec.vx                 = 0x64;
        vec.vz                 = 0;
        vec.vy                 = 0;
        work->field_8A2        = 2;
        work->field_89C        = 1;
        work->field_8A6        = 0x10;
        Gp_SpawnEff(0x60030, arg0->field_2C->coords + 1, 0x10300, &vec);
        work->field_6 = 0;
    }
    next          = work->field_6 + 1;
    work->field_6 = next;
    switch (work->field_8A2) {
        case 2:
            if ((s16)next >= 0x10 && (work->field_6C & 2)) {
                work->field_8A2 = 0x23;
                work->field_89C = 2;
                work->field_8A6 = 0x10;
                work->field_89E = 0;
            }
            if ((s16)func_actor_401300_8013267C(arg0->field_2C->coords, 0x15E, 0xA) != 0) {
                Actor401300_MoveForward(arg0->field_2C->coords, 0xA);
            }
            func_actor_401300_801323B0(arg0->field_2C->coords, (GpRec18*)work->field_AD0, 0xC);
            if (work->field_6 == 3) {
                D_80114B78[0] = &D_actor_401300_80148808;
                vec.vz        = 0x64;
                vec.vy        = 0;
                vec.vx        = 0;
                Actor401300_TintEffect(Gp_SpawnEff(0xA0005, arg0->field_2C->coords + 9, 0x200, &vec), enemy);
            }
            if (work->field_6 == 5) {
                D_80114B78[0] = &D_actor_401300_80148A14;
                Actor401300_TintEffect(Gp_SpawnEff(0xA0005, arg0->field_2C->coords + 1, 0x200, NULL), enemy);
            }
            break;
        case 0x23:
            if (!(work->field_6C & 0x100)) {
                work->field_6 = 0;
            }
            switch (work->field_6) {
                case 3:
                    break;
                case 30:
                    Gp_SetLightMode((GpObj4C*)enemy, 1);
                    vec.vx = 0;
                    vec.vy = 0;
                    vec.vz = 0;
                    Actor401300_TransformToView(arg0->field_2C->coords + 2, &vec);
                    work->field_8C0.sub        = &gGfxViewCoord;
                    work->field_8C0.coord.t[0] = vec.vx;
                    work->field_8C0.coord.t[1] = arg0->field_2C->coords->coord.t[1];
                    work->field_8C0.coord.t[2] = vec.vz;
                    work->field_8C0.flg        = 0;
                    Gp_UpdateCoord(&work->field_8C0);
                    Gp_SpawnEff(0x600A5, &work->field_8C0, 2, NULL);
                    break;
                case 48:
                    arg0->field_2C->flags = 2;
                    break;
                case 42:
                    Gp_SetLightMode((GpObj4C*)enemy, 2);
                    break;
                case 64:
                    arg0->field_2C->flags = 0x80;
                    work->field_0         = 0x24;
                    break;
            }
            cur = work->field_6;
            if (cur >= 0x1A) {
                Actor401300_RescaleYawXZ(arg0->field_2C->coords, 0x1964, 0x1964 - (cur - 0x14) * 16);
            }
            break;
    }
    func_actor_401300_80133A3C(arg0);
    Actor401300_ResetYaw(arg0->field_2C->coords + 2);
    Actor401300_ResetYaw(arg0->field_2C->coords + 3);
    Actor401300_ResetYaw(arg0->field_2C->coords + 4);
    Actor401300_ResetYaw(arg0->field_2C->coords + 5);
    Actor401300_ResetYaw(arg0->field_2C->coords + 6);
    Actor401300_ResetYaw(arg0->field_2C->coords + 7);
    Actor401300_ResetYaw(arg0->field_2C->coords + 8);
    Actor401300_ResetYaw(arg0->field_2C->coords + 9);
    Actor401300_ResetYaw(arg0->field_2C->coords + 10);
    Actor401300_ResetYaw(arg0->field_2C->coords + 11);
    Actor401300_ResetYaw(arg0->field_2C->coords + 12);
}

void func_actor_401300_8013CBAC(Actor401300* arg0)
{
    Actor401300Work*       work;
    TmdObject*             obj;
    GsCOORDINATE2*         coord;
    GsCOORDINATE2*         facing;
    GsCOORDINATE2*         root;
    GsCOORDINATE2*         root2;
    PlayerStatus*          config;
    Actor401300AimScratch* head;
    Actor401300AimScratch* aim;
    s16                    yaw;
    s32                    angle;

    work = arg0->field_1C;
    if (work->field_4 != 0) {
        obj                        = arg0->field_2C;
        arg0->field_20->node.flags = 0;
        obj->flags                 = 0;
        Tmd_AllocBuffers(obj);
        work->field_970.radius = 0x280;
        work->field_89C        = 1;
        work->field_8A6        = 0x24;
        work->field_8A2        = 2;
        work->field_89E        = 0;
        work->field_D1C        = 0;
        work->field_BF0.flags &= 0x7FFF;
        work->field_AB0.flags |= 0x4000;
        Gp_ArmStateF0(1);
        work->field_6 = 0;
        work->field_8 = 0;
    }
    work->field_6++;
    head                                     = *(Actor401300AimScratch**)G_SCRATCH_HEAD;
    *(Actor401300AimScratch**)G_SCRATCH_HEAD = head - 1;
    aim                                      = head - 1;
    if (func_actor_401300_80132C78(arg0->field_2C->coords, (GpRec18*)work->field_AD0, 0xC, 0x57) == 0) {
        func_actor_401300_80132910(arg0, (GpRec18*)work->field_990, 0xC);
    }
    config                      = &Player_Status;
    root                        = arg0->field_2C->coords;
    head[-1].delta.vx           = config->coordMtx->t[0] - root->coord.t[0];
    aim->delta.vy               = config->coordMtx->t[1] - root->coord.t[1];
    aim->delta.vz               = config->coordMtx->t[2] - root->coord.t[2];
    arg0->field_2C->coords->flg = 0;
    func_actor_401300_80133A3C(arg0);
    aim->pad_8        = ratan2(-((TmdObject*)((Task*)Game_GetPtrSlot(3))->extra)->coords->coord.m[2][0],
                               ((TmdObject*)((Task*)Game_GetPtrSlot(3))->extra)->coords->coord.m[2][2]);
    root2             = arg0->field_2C->coords;
    head[-1].delta.vx = config->coordMtx->t[0] - root2->coord.t[0];
    aim->delta.vy     = config->coordMtx->t[1] - root2->coord.t[1];
    aim->delta.vz     = config->coordMtx->t[2] - root2->coord.t[2];
    yaw               = ratan2(head[-1].delta.vx, aim->delta.vz) + 0x800;
    aim->pad_A        = yaw;
    aim->pad_A        = Actor401300_NormalizeYaw(yaw);
    coord             = arg0->field_2C->coords;
    angle             = ratan2(aim->delta.vx, aim->delta.vz);
    aim->angle        = Actor401300_NormalizeYaw(angle - ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]));
    work->field_8B2   = aim->angle;
    if (aim->angle < 0x200) {
        if (!Actor401300_OutOfRange(&aim->delta, 0x44C)) {
            work->field_0 = 0xB;
        }
    }
    if (aim->angle > 0x20) {
        aim->angle = 0x20;
    }
    if (aim->angle < -0x20) {
        aim->angle = -0x20;
    }
    facing      = arg0->field_2C->coords;
    aim->angle += ratan2(-facing->coord.m[2][0], facing->coord.m[2][2]);
    Gfx_RotMatrixY(&arg0->field_2C->coords->coord, aim->angle, 1);
    Actor401300_RescaleYaw(arg0->field_2C->coords, 0x1964);
    arg0->field_2C->coords->flg = 0;
    if (work->field_8A2 == 2) {
        if (work->field_89E == 0) {
            if ((s16)func_actor_401300_8013267C(arg0->field_2C->coords, 0x15E, 0x16) != 0) {
                Actor401300_MoveForward(arg0->field_2C->coords, 0x16);
            }
        } else {
            if ((s16)func_actor_401300_8013267C(arg0->field_2C->coords, 0x15E, 5) != 0) {
                Actor401300_MoveForward(arg0->field_2C->coords, 5);
            }
        }
    } else if (work->field_6C & 0x100) {
        work->field_8A2 = 2;
        work->field_89C = 1;
    }
    *(Actor401300AimScratch**)G_SCRATCH_HEAD += 1;
}

void func_actor_401300_8013D2AC(Actor401300* arg0)
{
    Actor401300Work*       work;
    GpEnemy*               enemy;
    TmdObject*             obj;
    GsCOORDINATE2*         coord;
    GsCOORDINATE2*         coord2;
    Actor401300AimScratch* aim;
    s32                    angle;

    work = arg0->field_1C;
    if (work->field_4 != 0) {
        enemy             = arg0->field_20;
        obj               = arg0->field_2C;
        enemy->node.flags = 0;
        obj->flags        = 0;
        Tmd_AllocBuffers(obj);
        work->field_970.radius = 0x280;
        work->field_89C        = 1;
        work->field_8A6        = 0x10;
        work->field_8A2        = 0x19;
        work->field_89E        = 0;
        work->field_BF0.flags &= 0x7FFF;
        work->field_AB0.flags |= 0x4000;
        func_actor_401300_80133A3C(arg0);
        work->field_D1C     = 0;
        work->field_6       = 0;
        work->field_8       = 0;
        work->field_BF0.key = Gp_PackObjPair((GpObj50*)enemy, 0);
        work->field_8B6     = 0x200;
        work->field_8BA     = 0x80;
        return;
    }
    func_actor_401300_80132C78(arg0->field_2C->coords, (GpRec18*)work->field_AD0, 0xC, 0x57);
    if (work->field_6 >= 0x29) {
        work->field_8B6 = 0;
        work->field_8BA = 0x40;
    }
    work->field_6++;
    switch (work->field_6) {
        case 0x19:
            work->field_BF0.flags |= 0x8000;
            break;
        case 0x28:
            work->field_BF0.flags &= 0x7FFF;
            break;
    }
    *(Actor401300AimScratch**)G_SCRATCH_HEAD -= 1;
    aim                                       = *(Actor401300AimScratch**)G_SCRATCH_HEAD;
    Actor401300_ConfigPositionDelta(&Player_Status, arg0->field_2C->coords, &aim->delta);
    arg0->field_2C->coords->flg = 0;
    func_actor_401300_80133A3C(arg0);
    if (work->field_6 < 0xE) {
        coord           = arg0->field_2C->coords;
        angle           = ratan2(aim->delta.vx, aim->delta.vz);
        aim->angle      = Actor401300_NormalizeYaw(angle - ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]));
        work->field_8B2 = aim->angle;
        if (aim->angle > 0x30) {
            aim->angle = 0x30;
        }
        if (aim->angle < -0x30) {
            aim->angle = -0x30;
        }
        coord2      = arg0->field_2C->coords;
        aim->angle += ratan2(-coord2->coord.m[2][0], coord2->coord.m[2][2]);
        Gfx_RotMatrixY(&arg0->field_2C->coords->coord, aim->angle, 1);
        Actor401300_RescaleYaw(arg0->field_2C->coords, 0x1964);
    }
    arg0->field_2C->coords->flg = 0;
    if (work->field_6C & 0x100) {
        work->field_0 = 6;
    }
    *(void**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD + 0x10;
}

void func_actor_401300_8013D6C4(Actor401300* arg0)
{
    Actor401300Work*       work;
    GpEnemy*               enemy;
    TmdObject*             obj;
    GsCOORDINATE2*         coord;
    GsCOORDINATE2*         coord2;
    Actor401300AimScratch* aim;
    s32                    angle;

    work = arg0->field_1C;
    if (work->field_4 != 0) {
        enemy             = arg0->field_20;
        obj               = arg0->field_2C;
        enemy->node.flags = 0;
        obj->flags        = 0;
        Tmd_AllocBuffers(obj);
        work->field_970.radius = 0x280;
        work->field_89C        = 1;
        work->field_8A6        = 0x10;
        work->field_8A2        = 0x1A;
        work->field_89E        = 0;
        work->field_BF0.flags &= 0x7FFF;
        work->field_AB0.flags |= 0x4000;
        func_actor_401300_80133A3C(arg0);
        work->field_D1C     = 0;
        work->field_6       = 0;
        work->field_8       = 0;
        work->field_BF0.key = Gp_PackObjPair((GpObj50*)enemy, 1);
        work->field_8B6     = 0x200;
        work->field_8BA     = 0x80;
        return;
    }
    func_actor_401300_801323B0(arg0->field_2C->coords, work->field_AD0, 0xC);
    if (work->field_6 >= 0x26) {
        work->field_8B6 = 0;
        work->field_8BA = 0x40;
    }
    work->field_6++;
    switch (work->field_6) {
        case 0x15:
            work->field_BF0.flags |= 0x8000;
            break;
        case 0x25:
            work->field_BF0.flags &= 0x7FFF;
            break;
    }
    *(Actor401300AimScratch**)G_SCRATCH_HEAD -= 1;
    aim                                       = *(Actor401300AimScratch**)G_SCRATCH_HEAD;
    Actor401300_ConfigPositionDelta(&Player_Status, arg0->field_2C->coords, &aim->delta);
    arg0->field_2C->coords->flg = 0;
    func_actor_401300_80133A3C(arg0);
    if (work->field_6 < 0xE) {
        coord           = arg0->field_2C->coords;
        angle           = ratan2(aim->delta.vx, aim->delta.vz);
        aim->angle      = Actor401300_NormalizeYaw(angle - ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]));
        work->field_8B2 = aim->angle;
        if (aim->angle > 0x30) {
            aim->angle = 0x30;
        }
        if (aim->angle < -0x30) {
            aim->angle = -0x30;
        }
        coord2      = arg0->field_2C->coords;
        aim->angle += ratan2(-coord2->coord.m[2][0], coord2->coord.m[2][2]);
        Gfx_RotMatrixY(&arg0->field_2C->coords->coord, aim->angle, 1);
        Actor401300_RescaleYaw(arg0->field_2C->coords, 0x1964);
    }
    arg0->field_2C->coords->flg = 0;
    if (work->field_6C & 0x100) {
        work->field_0 = 6;
    }
    *(void**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD + 0x10;
}

/// `Actor401300_RescaleYaw` at 0x1964 on the actor's root coordinate, with
/// the root's `flg` cleared again before the scratch block is released.
static __inline__ void Actor401300_ResetActorYaw(Actor401300* actor)
{
    GsCOORDINATE2*         coord;
    void*                  head;
    Actor401300RotScratch* blk;
    s16                    ang;
    u16                    m22;

    coord                   = actor->field_2C->coords;
    head                    = *(void**)G_SCRATCH_HEAD;
    blk                     = (Actor401300RotScratch*)((u8*)head - 0x34);
    *(void**)G_SCRATCH_HEAD = blk;

    ang        = ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]);
    blk->angle = ang;
    Gfx_RotMatrixY(&blk->m, ang, 1);
    blk->scale.vz = 0x1964;
    blk->scale.vy = 0x1964;
    blk->scale.vx = 0x1964;
    ScaleMatrix(&blk->m, &blk->scale);

    coord->coord.m[0][0]         = *(u16*)&((Actor401300RotScratch*)((u8*)head - 0x34))->m.m[0][0];
    coord->coord.m[0][1]         = *(u16*)&blk->m.m[0][1];
    coord->coord.m[0][2]         = *(u16*)&blk->m.m[0][2];
    coord->coord.m[1][0]         = *(u16*)&blk->m.m[1][0];
    coord->coord.m[1][1]         = *(u16*)&blk->m.m[1][1];
    coord->coord.m[1][2]         = *(u16*)&blk->m.m[1][2];
    coord->coord.m[2][0]         = *(u16*)&blk->m.m[2][0];
    coord->coord.m[2][1]         = *(u16*)&blk->m.m[2][1];
    m22                          = *(u16*)&blk->m.m[2][2];
    coord->flg                   = 0;
    coord->coord.m[2][2]         = m22;
    actor->field_2C->coords->flg = 0;
    *(void**)G_SCRATCH_HEAD      = (u8*)*(void**)G_SCRATCH_HEAD + 0x34;
}

/// Wrapped yaw from `coord`'s facing to the offset (`x`, `z`).
static __inline__ s16 Actor401300_YawTo(GsCOORDINATE2* coord, s16 x, s16 z)
{
    s32 angle;
    angle = ratan2(x, z);
    return Actor401300_NormalizeYaw(angle - ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]));
}

/// Facing yaw of `coord`.
static __inline__ s32 Actor401300_Yaw(GsCOORDINATE2* coord)
{
    return ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]);
}

/// `Actor401300_MoveForward` testing the flag byte through a `McSaveData*`.
static __inline__ void Actor401300_MoveForwardSave(McSaveData* save, GsCOORDINATE2* coord, s16 amount)
{
    SVECTOR* head;
    SVECTOR* vec;

    if ((u8)save->unknown_5C0[1] != 1) {
        head                       = *(SVECTOR**)G_SCRATCH_HEAD;
        vec                        = head - 1;
        *(SVECTOR**)G_SCRATCH_HEAD = vec;
        Gfx_MatrixCol2(&coord->coord, vec);
        VectorNormalSS(vec, vec);
        gte_lddp(amount);
        gte_ldsv(vec);
        gte_gpf12_real();
        gte_stsv(vec);
        coord->coord.t[0]          += head[-1].vx;
        coord->coord.t[1]          += vec->vy;
        coord->coord.t[2]          += vec->vz;
        coord->flg                  = 0;
        *(SVECTOR**)G_SCRATCH_HEAD += 1;
    }
}

void func_actor_401300_8013DADC(Actor401300* arg0)
{
    Actor401300Work*       work;
    Task*                  task;
    GameActor*             player;
    PlayerStatus*          config;
    GpEnemy*               enemy;
    TmdObject*             obj;
    GsCOORDINATE2*         root;
    SVECTOR**              scratch;
    Actor401300AimScratch* head;
    Actor401300AimScratch* aim;
    s16                    amount;
    s16                    ret;
    McSaveData*            save;

    work   = arg0->field_1C;
    task   = Game_GetPtrSlot(3);
    player = (GameActor*)task->work;
    enemy  = arg0->field_20;

    if (work->field_4 != 0) {
        obj               = arg0->field_2C;
        enemy->node.flags = 0;
        obj->flags        = 0;
        Tmd_AllocBuffers(obj);
        work->field_970.radius = 0x280;
        work->field_89C        = 1;
        work->field_8A6        = 0x10;
        work->field_8A2        = 0x1B;
        work->field_89E        = 0;
        work->field_BF0.flags &= 0x7FFF;
        work->field_AB0.flags |= 0x4000;
        func_actor_401300_80133A3C(arg0);
        work->field_D1C = 0;
        work->field_6   = 0;
        work->field_8   = 0;
        work->field_8B6 = 0;
        work->field_8BA = 0x40;
        return;
    }
    scratch = (SVECTOR**)G_SCRATCH_HEAD;
    config  = &Player_Status;
    work->field_6++;
    root              = arg0->field_2C->coords;
    head              = (Actor401300AimScratch*)*scratch;
    head[-1].delta.vx = config->coordMtx->t[0] - root->coord.t[0];
    aim               = (Actor401300AimScratch*)(*scratch = (SVECTOR*)(head - 1));
    aim->delta.vy     = config->coordMtx->t[1] - root->coord.t[1];
    aim->delta.vz     = config->coordMtx->t[2] - root->coord.t[2];
    save              = &Mc_SaveData;
    func_actor_401300_80133A3C(arg0);
    switch (work->field_8A2) {
        case 0x1B:
            if ((aim->pad_E = func_actor_401300_80132C78(arg0->field_2C->coords, (GpRec18*)work->field_AD0, 0xC, 0x57)) != 0 &&
                work->field_6 >= 0x15) {
                work->field_8++;
            } else {
                if (aim->pad_E != 1) {
                    func_actor_401300_80132910(arg0, work->field_990, 0xC);
                }
                work->field_8 = 0;
            }
            if (work->field_8 >= 7) {
                work->field_8A2 = 0x1E;
                work->field_89C = 2;
                work->field_6   = 0;
            }
            work->field_8B2 = 0;
            aim->angle      = Actor401300_YawTo(arg0->field_2C->coords, aim->delta.vx, aim->delta.vz);
            if (work->field_6 >= 0xB) {
                if (abs(aim->angle) < 0x200) {
                    if (!Actor401300_OutOfRange(&aim->delta, 0x7D0)) {
                        work->field_8A2 = 0x1C;
                        work->field_89C = 1;
                        work->field_6   = 0;
                    }
                }
            }
            if (abs(aim->angle) > 0x400) {
                work->field_8A2 = 0x1C;
                work->field_89C = 1;
                work->field_6   = 0;
            }
            if (aim->angle > 6) {
                aim->angle = 6;
            } else if (aim->angle < -6) {
                aim->angle = -6;
            }
            aim->angle += Actor401300_Yaw(arg0->field_2C->coords);
            Gfx_RotMatrixY(&arg0->field_2C->coords->coord, aim->angle, 1);
            if ((s16)func_actor_401300_8013267C(arg0->field_2C->coords, 0x15E, 0x70) != 0) {
                Actor401300_MoveForward(arg0->field_2C->coords, 0x70);
            }
            Actor401300_ResetActorYaw(arg0);
            break;
        case 0x1C:
            if ((aim->pad_E = func_actor_401300_80132C78(arg0->field_2C->coords, (GpRec18*)work->field_AD0, 0xC, 0x57)) != 0 &&
                work->field_6 >= 0x15) {
                work->field_8++;
            } else {
                if (aim->pad_E != 1) {
                    func_actor_401300_80132910(arg0, work->field_990, 0xC);
                }
                work->field_8 = 0;
            }
            if (work->field_8 >= 7) {
                work->field_8A2 = 0x1E;
                work->field_89C = 2;
                work->field_6   = 0;
            }
            aim->angle = Actor401300_YawTo(arg0->field_2C->coords, aim->delta.vx, aim->delta.vz);
            if (work->field_6C & 0x100) {
                work->field_8A2 = 0x1D;
                work->field_89C = 2;
                work->field_6   = 0;
            }
            if (func_actor_401300_80132910(arg0, work->field_990, 0xC) != 0 && player->field_954 != 2 && abs(aim->angle) < 0x100 &&
                enemy->field_40 > 0) {
                work->field_D00 = 0x7F;
                if (Gp_DispatchMsg(Game_GetPtrSlot(3), 0x3F8, (s32)&work->field_CEC, 0) == 0) {
                    Gp_SpawnPadLerp(0x10, 8, 0xFF);
                    work->field_D20         = 1;
                    work->field_CAC.field_0 = (s32)&D_actor_401300_801588F0;
                    work->field_CC0[2]      = 0;
                    work->field_CC0[1]      = 0;
                    work->field_CC0[0]      = 0;
                    work->field_CD0         = 7;
                    work->field_CD2         = 1;
                    aim->delta.vx           = -aim->delta.vx;
                    aim->delta.vy           = -aim->delta.vy;
                    aim->delta.vz           = -aim->delta.vz;
                    aim->angle              = Actor401300_YawTo(((TmdObject*)task->extra)->coords, aim->delta.vx, aim->delta.vz);
                    if (abs(aim->angle) < 0x400) {
                        amount                  = -0x64;
                        work->field_CAC.field_4 = 4;
                        work->field_CE4.vy      = aim->angle + Actor401300_Yaw(((TmdObject*)task->extra)->coords);
                        ret                     = Gp_DispatchMsg(Game_GetPtrSlot(3), 0x3F9, Gp_PackObjPair((GpObj50*)enemy, 4), 0);
                    } else {
                        amount                  = 0x64;
                        work->field_CAC.field_4 = 5;
                        work->field_CE4.vy      = aim->angle + Actor401300_Yaw(((TmdObject*)task->extra)->coords) + 0x800;
                        ret                     = Gp_DispatchMsg(Game_GetPtrSlot(3), 0x3F9, Gp_PackObjPair((GpObj50*)enemy, 5), 0);
                    }
                    if (ret == 1) {
                        player->field_956 = 0xA;
                    }
                    work->field_CD4.vx = ((TmdObject*)task->extra)->coords->coord.t[0];
                    work->field_CD4.vy = ((TmdObject*)task->extra)->coords->coord.t[1];
                    work->field_CD4.vz = ((TmdObject*)task->extra)->coords->coord.t[2];
                    work->field_CE4.vx = 0;
                    work->field_CE4.vz = 0;
                    Gp_DispatchMsg(task, 0x3E9, (s32)&work->field_CD4, 0);
                    Gp_DispatchMsg(Game_GetPtrSlot(3), 0x3FF, (s32)&work->field_CAC, 0);
                    work->field_D22 = 0;
                    Gfx_MatrixCol2(&((TmdObject*)task->extra)->coords->coord, &aim->delta);
                    aim->delta.vy = 0;
                    VectorNormalSS(&aim->delta, &aim->delta);
                    gte_lddp(amount);
                    gte_ldsv(&aim->delta);
                    gte_gpf12_real();
                    gte_stsv(&aim->delta);
                    work->field_CC0[0] = aim->delta.vx;
                    work->field_CC0[1] = 0;
                    work->field_CC0[2] = aim->delta.vz;
                    work->field_CD0    = 7;
                    work->field_CD2    = 1;
                    work->field_8A2    = 0x1E;
                    work->field_89C    = 2;
                    work->field_6      = 0;
                }
            }
            if ((s16)func_actor_401300_8013267C(arg0->field_2C->coords, 0x15E, 0xA8) != 0) {
                Actor401300_MoveForward(arg0->field_2C->coords, 0xA8);
            }
            Actor401300_ResetActorYaw(arg0);
            break;
        case 0x1E:
            if (func_actor_401300_80132C78(arg0->field_2C->coords, (GpRec18*)work->field_AD0, 0xC, 0x57) == 0) {
                func_actor_401300_80132910(arg0, work->field_990, 0xC);
            }
            if (work->field_6 < 8) {
                if ((s16)func_actor_401300_8013267C(arg0->field_2C->coords, 0x15E, -0x79) != 0) {
                    Actor401300_MoveForwardSave(save, arg0->field_2C->coords, -0x79);
                }
            } else if ((u16)(work->field_6 - 8) < 6) {
                if ((s16)func_actor_401300_8013267C(arg0->field_2C->coords, 0x15E, -0x19) != 0) {
                    Actor401300_MoveForwardSave(save, arg0->field_2C->coords, -0x19);
                }
            }
            Actor401300_ResetActorYaw(arg0);
            if (work->field_6C & 0x100) {
                work->field_0 = 6;
            }
            break;
        case 0x1D:
            if (work->field_6C & 0x100) {
                work->field_0 = 6;
            }
            break;
        default:
            work->field_0 = 0x18;
            break;
    }
    *(Actor401300AimScratch**)G_SCRATCH_HEAD += 1;
}

/// `Actor401300_MoveForwardNonzero` testing the same flag byte through a
/// `McSaveData*` (`D_80072729` is `Mc_SaveData.unknown_5C0[1]`).
static __inline__ void Actor401300_MoveForwardNonzeroSave(McSaveData* save, GsCOORDINATE2* coord, s16 amount)
{
    SVECTOR* head;
    SVECTOR* vec;
    SVECTOR* gteVec;

    if ((u8)save->unknown_5C0[1] != 1) {
        head                       = *(SVECTOR**)G_SCRATCH_HEAD;
        vec                        = head - 1;
        *(SVECTOR**)G_SCRATCH_HEAD = vec;
        gteVec                     = vec;
        if (amount != 0) {
            SOFT_TOUCH_REG(vec);
            Gfx_MatrixCol2(&coord->coord, vec);
            VectorNormalSS(vec, vec);
            gte_lddp(amount);
            gte_ldsv(gteVec);
            gte_gpf12_real();
            gte_stsv(gteVec);
            coord->coord.t[0] += head[-1].vx;
            coord->coord.t[1] += vec->vy;
            coord->coord.t[2] += vec->vz;
            coord->flg         = 0;
        }
        *(SVECTOR**)G_SCRATCH_HEAD += 1;
    }
}

void func_actor_401300_8013E930(Actor401300* arg0)
{
    Actor401300Work*         work;
    Task*                    task;
    GameActor*               player;
    PlayerStatus*            config;
    GpEnemy*                 enemy;
    TmdObject*               obj;
    GsCOORDINATE2*           root;
    SVECTOR**                scratch;
    Actor401300LungeScratch* head;
    Actor401300LungeScratch* blk;
    SVECTOR*                 delta;
    SVECTOR*                 vec;
    s16                      cur;
    s16                      amount;
    s16                      ret;
    McSaveData*              save;

    work   = arg0->field_1C;
    task   = Game_GetPtrSlot(3);
    player = (GameActor*)task->work;
    config = &Player_Status;
    save   = &Mc_SaveData;
    enemy  = arg0->field_20;

    if (work->field_4 != 0) {
        obj               = arg0->field_2C;
        enemy->node.flags = 0;
        obj->flags        = 0;
        Tmd_AllocBuffers(obj);
        work->field_970.radius = 0x280;
        work->field_89C        = 1;
        work->field_8A6        = 0x10;
        work->field_8A2        = 0x1F;
        work->field_89E        = 0;
        work->field_BF0.flags &= 0x7FFF;
        work->field_AB0.flags |= 0x4000;
        func_actor_401300_80133A3C(arg0);
        work->field_8B6 = 0x200;
        work->field_D1C = 0;
        work->field_6   = 0;
        work->field_8BA = 0x100;
        work->field_D04 = arg0->field_2C->coords->coord.t[0];
        work->field_D06 = arg0->field_2C->coords->coord.t[1];
        work->field_D08 = arg0->field_2C->coords->coord.t[2];
        return;
    }
    scratch = (SVECTOR**)G_SCRATCH_HEAD;
    work->field_6++;
    root              = arg0->field_2C->coords;
    head              = (Actor401300LungeScratch*)*scratch;
    head[-1].delta.vx = config->coordMtx->t[0] - root->coord.t[0];
    delta             = &head[-1].delta;
    delta->vy         = config->coordMtx->t[1] - root->coord.t[1];
    blk               = (Actor401300LungeScratch*)(*scratch = (SVECTOR*)(head - 1));
    delta->vz         = config->coordMtx->t[2] - root->coord.t[2];
    func_actor_401300_80133A3C(arg0);
    switch (work->field_8A2) {
        case 0x1F:
            work->field_970.radius = 0x280;
            if (func_actor_401300_80132C78(arg0->field_2C->coords, (GpRec18*)work->field_AD0, 0xC, 0x57) == 0) {
                func_actor_401300_80132910(arg0, work->field_990, 0xC);
            }
            work->field_8B2 = 0;
            blk->angle      = Actor401300_YawTo(arg0->field_2C->coords, head[-1].delta.vx, delta->vz);
            if (work->field_6 >= 0xB) {
                work->field_8A2 = 0x20;
                work->field_89C = 1;
                blk->dist.vx    = config->coordMtx->t[0] - arg0->field_2C->coords->coord.t[0];
                blk->dist.vy    = config->coordMtx->t[1] - arg0->field_2C->coords->coord.t[1];
                blk->dist.vz    = config->coordMtx->t[2] - arg0->field_2C->coords->coord.t[2];
                blk->range      = SquareRoot0(blk->dist.vx * blk->dist.vx + blk->dist.vy * blk->dist.vy + blk->dist.vz * blk->dist.vz) + 1000;
                if (blk->range > 5000) {
                    blk->range = 5000;
                } else if (blk->range < 3000) {
                    blk->range = 3000;
                }
                work->field_14  = blk->range / 18;
                work->field_8B6 = 0;
                work->field_6   = 0;
                work->field_8BA = 0x20;
            }
            if (abs(blk->angle) > 0x400) {
                work->field_0 = 7;
            }
            if (blk->angle > 0x10) {
                blk->angle = 0x10;
            } else if (blk->angle < -0x10) {
                blk->angle = -0x10;
            }
            blk->angle += Actor401300_Yaw(arg0->field_2C->coords);
            Gfx_RotMatrixY(&arg0->field_2C->coords->coord, blk->angle, 1);
            Actor401300_ResetActorYaw(arg0);
            break;
        case 0x20:
            work->field_970.radius = 0x140;
            func_actor_401300_80132C78(arg0->field_2C->coords, (GpRec18*)work->field_AD0, 0xC, 0x57);
            if (work->field_6C & 0x100) {
                work->field_8A2 = 0x21;
                work->field_89C = 2;
                work->field_6   = 0;
            }
            if (func_actor_401300_80132910(arg0, work->field_990, 0xC) != 0 && player->field_954 != 2 && work->field_6 >= 8 &&
                enemy->field_40 > 0) {
                work->field_D00 = 0x7F;
                if (Gp_DispatchMsg(Game_GetPtrSlot(3), 0x3F8, (s32)&work->field_CEC, 0) == 0) {
                    Gp_SpawnPadLerp(0x10, 8, 0xFF);
                    SndEvt_EnqueueType6(6, (s8)Gp_GetObjPan((GpObj38*)((TmdObject*)task->extra)->coords),
                                        (s8)Gp_GetObjDepth((GpObj38*)((TmdObject*)task->extra)->coords));
                    work->field_D20         = 1;
                    work->field_CAC.field_0 = (s32)&D_actor_401300_801588F0;
                    blk->delta.vx           = work->field_D04 - ((TmdObject*)task->extra)->coords->coord.t[0];
                    blk->delta.vy           = work->field_D06 - ((TmdObject*)task->extra)->coords->coord.t[1];
                    blk->delta.vz           = work->field_D08 - ((TmdObject*)task->extra)->coords->coord.t[2];
                    blk->angle              = Actor401300_YawTo(((TmdObject*)task->extra)->coords, head[-1].delta.vx, delta->vz);
                    if (abs(blk->angle) < 0x400) {
                        amount                  = -0x46;
                        work->field_CAC.field_4 = 4;
                        work->field_CE4.vy      = blk->angle + Actor401300_Yaw(((TmdObject*)task->extra)->coords);
                        ret                     = Gp_DispatchMsg(Game_GetPtrSlot(3), 0x3F9, Gp_PackObjPair((GpObj50*)enemy, 2), 0);
                    } else {
                        amount                  = 0x46;
                        work->field_CAC.field_4 = 5;
                        work->field_CE4.vy      = blk->angle + Actor401300_Yaw(((TmdObject*)task->extra)->coords) + 0x800;
                        ret                     = Gp_DispatchMsg(Game_GetPtrSlot(3), 0x3F9, Gp_PackObjPair((GpObj50*)enemy, 3), 0);
                    }
                    if (ret == 1) {
                        player->field_956 = 0xA;
                    }
                    work->field_CD4.vx = ((TmdObject*)task->extra)->coords->coord.t[0];
                    work->field_CD4.vy = ((TmdObject*)task->extra)->coords->coord.t[1];
                    work->field_CD4.vz = ((TmdObject*)task->extra)->coords->coord.t[2];
                    work->field_CE4.vx = 0;
                    work->field_CE4.vz = 0;
                    Gp_DispatchMsg(task, 0x3E9, (s32)&work->field_CD4, 0);
                    Gp_DispatchMsg(Game_GetPtrSlot(3), 0x3FF, (s32)&work->field_CAC, 0);
                    work->field_D22 = 0;
                    vec             = &blk->delta;
                    Gfx_MatrixCol2(&((TmdObject*)task->extra)->coords->coord, vec);
                    blk->delta.vy = 0;
                    VectorNormalSS(vec, vec);
                    gte_lddp(amount);
                    gte_ldsv(vec);
                    gte_gpf12_real();
                    gte_stsv(vec);
                    work->field_CC0[0] = blk->delta.vx;
                    work->field_CC0[1] = 0;
                    work->field_CC0[2] = blk->delta.vz;
                    work->field_CD0    = 7;
                    work->field_CD2    = 1;
                }
            }
            if (work->field_D20 == 0) {
                Actor401300_MoveForwardNonzero(arg0->field_2C->coords, work->field_14);
            }
            Actor401300_ResetActorYaw(arg0);
            break;
        case 0x21:
            work->field_970.radius = 0x280;
            if (func_actor_401300_80132C78(arg0->field_2C->coords, (GpRec18*)work->field_AD0, 0xC, 0x57) == 0) {
                cur = work->field_6;
                if (cur < 0x11 && work->field_D20 == 0) {
                    if ((s16)func_actor_401300_8013267C(arg0->field_2C->coords, 0x15E, (s16)(0x54 - cur * 0x54 / 16)) != 0) {
                        Actor401300_MoveForwardNonzeroSave(save, arg0->field_2C->coords, 0x54 - work->field_6 * 0x54 / 16);
                    }
                }
            }
            func_actor_401300_80132910(arg0, work->field_990, 0xC);
            Actor401300_ResetActorYaw(arg0);
            if (work->field_6C & 0x100) {
                work->field_0 = 6;
            }
            break;
        default:
            work->field_0 = 0x18;
            break;
    }
    *(Actor401300LungeScratch**)G_SCRATCH_HEAD += 1;
}

/// Scale `m` uniformly by `scale` (4.12), translation included.
static __inline__ void Actor401300_ScaleMatrix(MATRIX* m, s16 scale)
{
    Actor401300ScaleScratch* head;
    Actor401300ScaleScratch* blk;

    head                                       = *(Actor401300ScaleScratch**)G_SCRATCH_HEAD;
    blk                                        = head - 1;
    *(Actor401300ScaleScratch**)G_SCRATCH_HEAD = blk;
    blk->scale.vz                              = scale;
    blk->scale.vy                              = scale;
    head[-1].scale.vx                          = scale;
    ScaleMatrix(m, &blk->scale);
    blk->trans.vx = m->t[0];
    blk->trans.vy = m->t[1];
    blk->trans.vz = m->t[2];
    gte_lddp(scale);
    gte_ldsv(&blk->trans);
    gte_gpf12_real();
    gte_stsv(&blk->trans);
    m->t[0]                                     = blk->trans.vx;
    m->t[1]                                     = blk->trans.vy;
    *(Actor401300ScaleScratch**)G_SCRATCH_HEAD += 1;
    m->t[2]                                     = blk->trans.vz;
}

void func_actor_401300_8013F628(Actor401300* arg0)
{
    Actor401300Work* work;
    GpEnemy*         enemy;
    TmdObject*       obj;
    PlayerStatus*    config;
    GsCOORDINATE2*   root;
    SVECTOR**        scratch;
    SVECTOR*         head;
    SVECTOR*         vec;
    s16              mod;
    s16              cur;

    work  = arg0->field_1C;
    enemy = arg0->field_20;
    if (work->field_4 != 0) {
        obj = arg0->field_2C;
        Gp_SetLightMode((GpObj4C*)enemy, 0);
        enemy->node.flags = 1;
        obj->flags        = 0;
        Tmd_AllocBuffers(obj);
        work->field_970.radius = 0x280;
        work->field_89C        = 2;
        work->field_8A6        = 0x10;
        work->field_8A2        = 0x20;
        work->field_89E        = 0;
        work->field_BF0.flags &= 0x7FFF;
        work->field_AB0.flags &= 0xBFFF;
        func_actor_401300_80133A3C(arg0);
        work->field_8B6 = 0x200;
        work->field_D1C = 0;
        work->field_6   = 0;
        work->field_8BA = 0x100;
        work->field_D04 = arg0->field_2C->coords->coord.t[0];
        work->field_D06 = arg0->field_2C->coords->coord.t[1];
        work->field_D08 = arg0->field_2C->coords->coord.t[2];
        Actor401300_ScaleMatrix(&work->field_C48, 0);
        return;
    }
    scratch = (SVECTOR**)G_SCRATCH_HEAD;
    config  = &Player_Status;
    work->field_6++;
    root        = arg0->field_2C->coords;
    head        = *scratch;
    head[-1].vx = config->coordMtx->t[0] - root->coord.t[0];
    vec         = head - 1;
    vec->vy     = config->coordMtx->t[1] - root->coord.t[1];
    vec->vz     = config->coordMtx->t[2] - root->coord.t[2];
    *scratch    = head - 3;
    if (work->field_6 < 0x12) {
        Actor401300_ScaleMatrix(&work->field_C48, (work->field_6 << 12) / 30);
        if (gGameSession->at4.loc.area == 0xB) {
            if ((work->field_6 & 7) == 0) {
                Gp_SpawnEff(0x600FB, arg0->field_2C->coords + 3, 0, NULL);
                Gp_SpawnEff(0x600FB, arg0->field_2C->coords + 16, 0, NULL);
                Gp_SpawnEff(0x600FB, arg0->field_2C->coords + 1, 0, NULL);
                Gp_SpawnEff(0x600FB, arg0->field_2C->coords + 18, 0, NULL);
            } else {
                mod = work->field_6 % 8;
                if (mod == 2) {
                    Gp_SpawnEff(0x600FB, arg0->field_2C->coords + 2, 0, NULL);
                    Gp_SpawnEff(0x600FB, arg0->field_2C->coords + 17, 0, NULL);
                    Gp_SpawnEff(0x600FB, arg0->field_2C->coords + 3, 0, NULL);
                    Gp_SpawnEff(0x600FB, arg0->field_2C->coords + 4, 0, NULL);
                } else if (mod == 4) {
                    Gp_SpawnEff(0x600FB, arg0->field_2C->coords + 5, 0, NULL);
                    Gp_SpawnEff(0x600FB, arg0->field_2C->coords + 16, 0, NULL);
                    Gp_SpawnEff(0x600FB, arg0->field_2C->coords + 1, 0, NULL);
                    Gp_SpawnEff(0x600FB, arg0->field_2C->coords + 19, 0, NULL);
                } else if (mod == 6) {
                    Gp_SpawnEff(0x600FB, arg0->field_2C->coords + 17, 0, NULL);
                    Gp_SpawnEff(0x600FB, arg0->field_2C->coords + 16, 0, NULL);
                    Gp_SpawnEff(0x600FB, arg0->field_2C->coords + 5, 0, NULL);
                    Gp_SpawnEff(0x600FB, arg0->field_2C->coords + 18, 0, NULL);
                }
            }
        } else if (gGameSession->at4.loc.area == 0x1D) {
            if ((work->field_6 & 7) == 0) {
                Gp_SpawnEff(0x601C1, arg0->field_2C->coords + 3, 0, NULL);
                Gp_SpawnEff(0x601C1, arg0->field_2C->coords + 16, 0, NULL);
                Gp_SpawnEff(0x601C1, arg0->field_2C->coords + 1, 0, NULL);
                Gp_SpawnEff(0x601C1, arg0->field_2C->coords + 18, 0, NULL);
            } else {
                mod = work->field_6 % 8;
                if (mod == 2) {
                    Gp_SpawnEff(0x601C1, arg0->field_2C->coords + 2, 0, NULL);
                    Gp_SpawnEff(0x601C1, arg0->field_2C->coords + 17, 0, NULL);
                    Gp_SpawnEff(0x601C1, arg0->field_2C->coords + 3, 0, NULL);
                    Gp_SpawnEff(0x601C1, arg0->field_2C->coords + 4, 0, NULL);
                } else if (mod == 4) {
                    Gp_SpawnEff(0x601C1, arg0->field_2C->coords + 5, 0, NULL);
                    Gp_SpawnEff(0x601C1, arg0->field_2C->coords + 16, 0, NULL);
                    Gp_SpawnEff(0x601C1, arg0->field_2C->coords + 1, 0, NULL);
                    Gp_SpawnEff(0x601C1, arg0->field_2C->coords + 19, 0, NULL);
                } else if (mod == 6) {
                    Gp_SpawnEff(0x601C1, arg0->field_2C->coords + 17, 0, NULL);
                    Gp_SpawnEff(0x601C1, arg0->field_2C->coords + 16, 0, NULL);
                    Gp_SpawnEff(0x601C1, arg0->field_2C->coords + 5, 0, NULL);
                    Gp_SpawnEff(0x601C1, arg0->field_2C->coords + 18, 0, NULL);
                }
            }
        }
    }
    func_actor_401300_80133A3C(arg0);
    switch (work->field_8A2) {
        case 0x20:
            work->field_970.radius = 0x500;
            if (work->field_6C & 0x100) {
                work->field_8A2 = 0x21;
                work->field_89C = 2;
                work->field_6   = 0;
            }
            if (work->field_D20 == 0 && (s16)func_actor_401300_8013267C(arg0->field_2C->coords, 0x15E, 0x78) != 0) {
                Actor401300_MoveForward(arg0->field_2C->coords, 0x78);
            }
            Actor401300_ResetActorYaw(arg0);
            break;
        case 0x21:
            work->field_970.radius = 0x280;
            cur                    = work->field_6;
            if (cur < 0x11 && work->field_D20 == 0) {
                if ((s16)func_actor_401300_8013267C(arg0->field_2C->coords, 0x15E, 0x54 - cur * 0x54 / 16) != 0) {
                    Actor401300_MoveForwardNonzero(arg0->field_2C->coords, 0x54 - work->field_6 * 0x54 / 16);
                }
            }
            Actor401300_ResetActorYaw(arg0);
            if (work->field_6C & 0x100) {
                work->field_0 = 6;
            }
            break;
        default:
            work->field_0 = 0x18;
            break;
    }
    *(Actor401300ScaleScratch**)G_SCRATCH_HEAD += 1;
}

void func_actor_401300_80140300(Actor401300* arg0)
{
    Actor401300Work* work;
    GpEnemy*         enemy;

    work  = arg0->field_1C;
    enemy = arg0->field_20;
    if (work->field_4 != 0) {
        arg0->field_2C->flags  = 0;
        work->field_970.radius = 0x280;
        work->field_BF0.flags &= 0x7FFF;
        work->field_AB0.flags |= 0x4000;
        enemy->node.flags      = 0;
        work->field_89C        = 2;
        work->field_8A2        = 0xB;
        work->field_8A6        = 0x10;
        work->field_8B4        = 0;
        work->field_8B2        = 0;
        if (enemy->field_40 <= 0) {
            Gp_SetStateF0Byte3(1);
        }
        work->field_8B6        = 0x40;
        work->field_8B8        = 0xC8;
        work->field_8BA        = 0x40;
        work->field_970.flags |= 0x4000;
    }
    func_actor_401300_80133A3C(arg0);
    if (func_actor_401300_801323B0(arg0->field_2C->coords, work->field_990, 0xC) == 0) {
        func_actor_401300_80132C78(arg0->field_2C->coords, (GpRec18*)work->field_AD0, 0xC, 0x57);
    }
    arg0->field_2C->coords->flg = 0;
    if (work->field_6C & 0x100) {
        work->field_970.flags &= 0xBFFF;
        if (enemy->field_40 <= 0) {
            work->field_0 = 0x15;
        } else if (enemy->field_4C & 2) {
            work->field_0 = 4;
        } else {
            work->field_0 = 0x11;
        }
    }
}

void func_actor_401300_8014046C(Actor401300* arg0)
{
    Actor401300Work* work;
    GpEnemy*         enemy;

    work  = arg0->field_1C;
    enemy = arg0->field_20;
    if (work->field_4 != 0) {
        arg0->field_2C->flags  = 0;
        work->field_970.radius = 0x280;
        work->field_BF0.flags &= 0x7FFF;
        work->field_AB0.flags |= 0x4000;
        enemy->node.flags      = 0;
        work->field_89C        = 2;
        work->field_8A2        = 0x22;
        work->field_8A6        = 0x10;
        work->field_8B4        = 0;
        work->field_8B2        = 0;
        work->field_8B6        = 0x40;
        work->field_8B8        = 0xC8;
        work->field_8BA        = 0x40;
        if (enemy->field_40 <= 0) {
            Gp_SetStateF0Byte3(1);
        }
        work->field_970.flags |= 0x4000;
    }
    func_actor_401300_80133A3C(arg0);
    if (func_actor_401300_801323B0(arg0->field_2C->coords, work->field_990, 0xC) == 0) {
        func_actor_401300_80132C78(arg0->field_2C->coords, (GpRec18*)work->field_AD0, 0xC, 0x57);
    }
    arg0->field_2C->coords->flg = 0;
    if (work->field_6C & 0x100) {
        work->field_970.flags &= 0xBFFF;
        if (enemy->field_40 <= 0) {
            work->field_0 = 0x15;
        } else if (enemy->field_4C & 2) {
            work->field_0 = 4;
        } else {
            work->field_0 = 0x11;
        }
    }
}

// This is a decompilation attempt by the m2c tool.

/// `Actor401300_InRange` with the flag kept apart from the comparison. The
/// dead `ret = cmp` in each arm stops jump from turning the if/else into a
/// store-flag, which would fold `cmp` and `ret` into one register.
static __inline__ s32 Actor401300_InRangeFlag(Actor401300* arg0)
{
    SVECTOR        out;
    SVECTOR        sv;
    VECTOR         vec;
    s32            flag;
    SVECTOR*       svp;
    GsCOORDINATE2* view;
    VECTOR*        vecp;
    s32*           flagp;
    SVECTOR*       outp;
    GsCOORDINATE2* p;
    s32            ret;
    s32            cmp;

    memset(&out, 0, 8);
    p     = &arg0->field_2C->coords[1];
    svp   = &sv;
    outp  = &out;
    view  = &gGfxViewCoord;
    vecp  = &vec;
    flagp = &flag;
    sv.vx = outp->vx;
    sv.vy = outp->vy;
    sv.vz = outp->vz;
loop:
    if (p->sub != NULL) {
        if (p != view) {
            gte_SetTransMatrix(&p->coord);
            gte_SetRotMatrix(&p->coord);
            gte_ldv0(svp);
            __asm__ volatile("nop; nop; .word 0x4A480012");
            gte_stlvnl(vecp);
            gte_stflg(flagp);
            sv.vx = vec.vx;
            sv.vy = vec.vy;
            sv.vz = vec.vz;
            p     = p->sub;
            goto loop;
        }
        outp->vx = sv.vx;
        outp->vy = sv.vy;
        outp->vz = sv.vz;
    }
    cmp = (u16)(out.vz + 0x12B) < 0xA27;
    if (cmp == 0) {
        ret = cmp;
        ret = 0;
    } else {
        ret = cmp;
        ret = 1;
    }
    return ret;
}

/// 1 when the first of `recs` is a kind 0x10000 record.
static __inline__ s32 Actor401300_HasRec10000(GpRec18* recs)
{
    s16 i;

    for (i = 0; i < 1; i++) {
        if (!recs[i].key)
            break;
        if ((recs[i].key & 0xFFFF0000) == 0x10000) {
            return 1;
        }
    }
    return 0;
}

/// Snaps the player's height to the actor's when it is locked (`field_CD0`
/// 7) and has drifted 0x321 or more away.
static __inline__ void Actor401300_SnapPlayerHeight(Actor401300* actor)
{
    Actor401300Work* work;
    Task*            slot;
    GsCOORDINATE2*   playerCoord;
    GsCOORDINATE2*   actorCoord;

    work = actor->field_1C;
    slot = Game_GetPtrSlot(3);
    if ((slot != NULL) && (work->field_CD0 == 7)) {
        playerCoord = ((Actor401300*)slot)->field_2C->coords;
        actorCoord  = actor->field_2C->coords;
        if (abs(playerCoord->coord.t[1] - actorCoord->coord.t[1]) >= 0x321) {
            playerCoord->coord.t[1]                     = actorCoord->coord.t[1];
            ((Actor401300*)slot)->field_2C->coords->flg = 0;
        }
    }
}

static const Actor401300StateTable D_actor_401300_80131F34 = { {
    func_actor_401300_8014192C,
    func_actor_401300_801419B8,
    func_actor_401300_80141A60,
    func_actor_401300_80141B0C,
    func_actor_401300_80135DDC,
    func_actor_401300_80141BC8,
    func_actor_401300_80136238,
    func_actor_401300_801365F8,
    func_actor_401300_80136CE8,
    func_actor_401300_801376E4,
    func_actor_401300_80137D78,
    func_actor_401300_80138160,
    func_actor_401300_80138800,
    func_actor_401300_80138B24,
    (void (*)(Actor401300*))func_actor_401300_80141C80,
    func_actor_401300_80141C88,
    func_actor_401300_80141D50,
    func_actor_401300_80141DF4,
    NULL,
    func_actor_401300_80138CF8,
    func_actor_401300_80138FCC,
    func_actor_401300_80139134,
    func_actor_401300_80139520,
    func_actor_401300_801397F8,
    func_actor_401300_80139AB0,
    func_actor_401300_8013A5C0,
    func_actor_401300_8013A208,
    func_actor_401300_8013AAE8,
    func_actor_401300_8013AE48,
    func_actor_401300_8013B6E8,
    func_actor_401300_8013CBAC,
    func_actor_401300_8013D2AC,
    func_actor_401300_8013D6C4,
    func_actor_401300_8013DADC,
    func_actor_401300_8013E930,
    func_actor_401300_8013F628,
    (void (*)(Actor401300*))func_actor_401300_80141EF8,
    func_actor_401300_80140300,
    func_actor_401300_8014046C,
    func_actor_401300_80135FC4,
    func_actor_401300_8013BB30,
} };

void func_actor_401300_801405DC(GpEnemy* enemy, Actor401300* actor)
{
    VECTOR                  pos;
    Actor401300StateTable   states;
    Actor401300Work*        work;
    Actor401300ViewScratch* scratch;
    Actor401300ViewScratch* head;
    Actor401300*            player;
    PlayerStatus*           config;
    s32                     state;
    s32                     action;

    work   = actor->field_1C;
    player = (Actor401300*)Game_GetPtrSlot(3);
    config = &Player_Status;
    states = D_actor_401300_80131F34;

    actor->field_2C->coords->flg = 0;
    Gp_UpdateCoord(actor->field_2C->coords);
    pos.vx = actor->field_2C->coords->workm.t[0];
    pos.vy = actor->field_2C->coords->workm.t[1];
    pos.vz = actor->field_2C->coords->workm.t[2];
    Gp_UpdateActorColor(enemy, &pos, 0, 0);

    switch (D_801153F4) {
        case 0:
            state = work->field_0;
            if ((state != 0) && (state != 0x24) && (state != 0x15) && (state != 0x1D) && (state != 0x28)) {
                actor->field_2C->flags = 0;
                Gp_DrawEffGroundQuad((VECTOR3*)actor->field_2C->coords->workm.t, 0x280, Gp_State1C->field_8);
                state = work->field_0;
            }
            if ((state == 0x28) && (work->field_8A2 == 2)) {
                Gp_DrawEffGroundQuad((VECTOR3*)actor->field_2C->coords->workm.t, 0x280, Gp_State1C->field_8);
            }
            break;
        case 1:
            state = work->field_0;
            if ((state != 0) && (state != 0x24) && (state != 0x15) && (state != 0x1D) && (state != 0x28)) {
                actor->field_2C->flags = 0;
                Gp_DrawEffGroundQuad((VECTOR3*)actor->field_2C->coords->workm.t, 0x280, Gp_State1C->field_8);
                state = work->field_0;
            }
            if ((state == 0x28) && (work->field_8A2 == 2)) {
                Gp_DrawEffGroundQuad((VECTOR3*)actor->field_2C->coords->workm.t, 0x280, Gp_State1C->field_8);
            }
            Gp_ClearRec18Occupied((GpRec18*)work->field_AD0);
            Gp_ClearRec18Occupied((GpRec18*)work->field_990);
            Gp_ClearRec18Occupied((GpRec18*)work->pad_C10);
            return;
        case 2:
            actor->field_2C->flags = 0x80;
            Gp_ClearRec18Occupied((GpRec18*)work->field_AD0);
            Gp_ClearRec18Occupied((GpRec18*)work->field_990);
            Gp_ClearRec18Occupied((GpRec18*)work->pad_C10);
            return;
    }

    head                                      = *(Actor401300ViewScratch**)G_SCRATCH_HEAD;
    *(Actor401300ViewScratch**)G_SCRATCH_HEAD = head - 1;
    scratch                                   = head - 1;

    if (work->field_C88 > 0) {
        work->field_C88 = (s16)((u16)work->field_C88 - 1);
    } else {
        func_actor_401300_80134F90(actor);
    }
    if (work->field_2 != work->field_0) {
        work->field_4 = 1;
    } else {
        work->field_4 = 0;
    }
    work->field_2 = (u16)work->field_0;
    states.fn[work->field_0](actor);

    state = work->field_0;
    if ((state != 0x15) && (state != 3) && (state != 0) && (state != 0x24) && (state != 0x1D) && (state != 0x28)) {
        scratch->pos.vx = 0;
        scratch->pos.vy = 0;
        scratch->pos.vz = 0;
        Actor401300_TransformToView(actor->field_2C->coords + 1, &scratch->pos);
        work->field_970.pos.vx     = scratch->pos.vx;
        work->field_970.pos.vy     = scratch->pos.vy;
        work->field_970.pos.vz     = scratch->pos.vz;
        work->field_920.coord.t[0] = actor->field_2C->coords->coord.t[0];
        work->field_920.coord.t[1] = actor->field_2C->coords->coord.t[1] - 0x15E;
        work->field_920.coord.t[2] = actor->field_2C->coords->coord.t[2];
        work->field_920.flg        = 0;
        Gp_UpdateCoord(&work->field_920);
        actor->field_2C->coords->flg = 0;
        Gp_UpdateCoord(actor->field_2C->coords);
        state = work->field_0;
    }
    if ((state == 0x15) || (state == 0) || (state == 0x24) || (state == 0x1D) || (state == 0x28)) {
        work->field_970.flags &= 0x7FFF;
        work->field_AB0.flags &= 0x7FFF;
    } else {
        work->field_970.flags |= 0x8000;
        if ((u32)((u16)work->field_0 - 0x21) < 2U) {
            work->field_AB0.flags &= 0x7FFF;
        } else {
            work->field_AB0.flags |= 0x8000;
        }
    }
    if ((Actor401300_HasRec10000((GpRec18*)work->pad_C10) == 1) || (enemy->field_40 <= 0)) {
        work->field_BF0.flags &= 0x7FFF;
    }
    Gp_ClearRec18Occupied((GpRec18*)work->field_AD0);
    Gp_ClearRec18Occupied((GpRec18*)work->field_990);
    Gp_ClearRec18Occupied((GpRec18*)work->pad_C10);

    if (work->field_D20 == 1) {
        state = work->field_0;
        if ((state != 0x15) && (state != 3) && (state != 0) && (state != 0x24) && (state != 0x1D) && (state != 0x28)) {
            Actor401300_SnapPlayerHeight(actor);
        }
        action          = work->field_CAC.field_4;
        work->field_D22 = (u16)(work->field_D22 + 1);
        switch (action) {
            case 0:
            case 1:
            case 2:
            case 3:
                break;
            case 4:
                if ((s16)work->field_D22 == 0xF) {
                    if (Actor401300_InRangeFlag(player) == 1) {
                        SndEvt_EnqueueType6(0x551D0005, (s8)Gp_GetObjPan((GpObj38*)player->field_2C->coords),
                                            (s8)Gp_GetObjDepth((GpObj38*)player->field_2C->coords));
                    } else {
                        SndEvt_EnqueueType6(0x400D0013, (s8)Gp_GetObjPan((GpObj38*)player->field_2C->coords),
                                            (s8)Gp_GetObjDepth((GpObj38*)player->field_2C->coords));
                    }
                    if (Gp_State1C->field_A == 2) {
                        Gp_SpawnEff(0x60054, &player->field_2C->coords[1], 0x80003A00, NULL);
                    }
                }
                if (Gp_DispatchMsg((Task*)player, 0x3FE, (s32)work->field_CC0, 0) == 1) {
                    work->field_CC0[0] = 0;
                    work->field_CC0[1] = 0;
                    work->field_CC0[2] = 0;
                }
                if (work->field_6 >= 10) {
                    work->field_CC0[1] = 0;
                    work->field_CC0[0] = work->field_CC0[0] >> 1;
                    work->field_CC0[2] = work->field_CC0[2] >> 1;
                }
                break;
            case 5:
                if ((s16)work->field_D22 == 0xD) {
                    if (Actor401300_InRangeFlag(player) == 1) {
                        SndEvt_EnqueueType6(0x551D0005, (s8)Gp_GetObjPan((GpObj38*)player->field_2C->coords),
                                            (s8)Gp_GetObjDepth((GpObj38*)player->field_2C->coords));
                    } else {
                        SndEvt_EnqueueType6(0x400D0013, (s8)Gp_GetObjPan((GpObj38*)player->field_2C->coords),
                                            (s8)Gp_GetObjDepth((GpObj38*)player->field_2C->coords));
                    }
                    if (Gp_State1C->field_A == 2) {
                        Gp_SpawnEff(0x60054, &player->field_2C->coords[1], 0x80003A00, NULL);
                    }
                }
                if (Gp_DispatchMsg((Task*)player, 0x3FE, (s32)work->field_CC0, 0) == 1) {
                    work->field_CC0[0] = 0;
                    work->field_CC0[1] = 0;
                    work->field_CC0[2] = 0;
                }
                if (work->field_6 >= 10) {
                    work->field_CC0[1] = 0;
                    work->field_CC0[0] = work->field_CC0[0] >> 1;
                    work->field_CC0[2] = work->field_CC0[2] >> 1;
                }
                break;
            case 6:
            case 7:
                break;
        }
        if (Gp_DispatchMsg(Game_GetPtrSlot(3), 0x3ED, 0, 0) == 0) {
            switch (work->field_CAC.field_4) {
                case 0:
                    break;
                case 1:
                    if (config->hp > 0) {
                        work->field_CAC.field_4 = 2;
                        Gp_DispatchMsg(Game_GetPtrSlot(3), 0x3FF, (s32)&work->field_CAC, 0);
                        work->field_D22 = 0;
                    }
                    break;
                case 2:
                    if (config->hp > 0) {
                        work->field_CAC.field_4 = 3;
                        Gp_DispatchMsg(Game_GetPtrSlot(3), 0x3FF, (s32)&work->field_CAC, 0);
                        work->field_D22 = 0;
                    }
                    break;
                case 4:
                    if (config->hp > 0) {
                        work->field_CAC.field_4 = 6;
                        Gp_DispatchMsg(Game_GetPtrSlot(3), 0x3FF, (s32)&work->field_CAC, 0);
                        work->field_D22 = 0;
                    }
                    break;
                case 5:
                    if (config->hp > 0) {
                        work->field_CAC.field_4 = 7;
                        Gp_DispatchMsg(Game_GetPtrSlot(3), 0x3FF, (s32)&work->field_CAC, 0);
                        work->field_D22 = 0;
                    }
                    break;
                case 3:
                case 6:
                case 7:
                    Gp_DispatchMsg(Game_GetPtrSlot(3), 0x3F1, 0, 0);
                    work->field_D20 = 0;
                    break;
            }
        }
    }
    if ((work->field_C8A == 1) && (work->field_D20 == 0)) {
        Gp_ReleaseStateF0Add((GpObj20E*)actor, 0xD);
        work->field_C8A = 0;
    }
    if ((D_801153F2[1] == 1) && (work->field_0 == 0x18)) {
        work->field_0 = 6;
    }

    scratch->pos.vx = 0;
    scratch->pos.vy = 0;
    scratch->pos.vz = 0;
    Actor401300_TransformToView(actor->field_2C->coords + 2, &scratch->pos);

    work->field_D28[work->field_D78].vx = scratch->pos.vx;
    work->field_D28[work->field_D78].vy = scratch->pos.vy;
    work->field_D28[work->field_D78].vz = scratch->pos.vz;

    *(u8**)G_SCRATCH_HEAD += 0x18;
    work->field_D78        = (u16)work->field_D78 + 1;
    if (work->field_D78 == 7) {
        work->field_D78 = 0;
    }
    if ((u32)((u16)work->field_8A2 - 0x14) < 2U) {
        enemy->field_1C.vx = work->field_D28[work->field_D78].vx;
        enemy->field_1C.vy = work->field_D28[work->field_D78].vy;
        enemy->field_1C.vz = work->field_D28[work->field_D78].vz;
    } else {
        enemy->field_1C.vx = scratch->pos.vx;
        enemy->field_1C.vy = scratch->pos.vy;
        enemy->field_1C.vz = scratch->pos.vz;
    }
    enemy->field_18 = &gGfxViewCoord;
}

void func_actor_401300_8014148C(void)
{
}

INCLUDE_RODATA("actors/nonmatchings/actor_401300/actor_401300", ActorsShared80135df4Table);

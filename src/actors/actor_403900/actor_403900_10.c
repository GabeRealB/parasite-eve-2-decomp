#include "common.h"

#include <psyq/libgte.h>
#include <psyq/inline_c.h>
#include "gte.h"

#include "main/mem.h"
#include "main/session.h"
#include "main/sound.h"
#include "main/wipsys.h"

#include "gameplay/3A34.h"

#include "actors/actor_403900.h"
#include "actors/actors_shared_80132688.h"

/// Picks the damage reaction for the hit just taken, from the enemy's HP and
/// the damage `arg1`: at or below zero HP it silences both queued sound events
/// and enters the death sequence (9, or 10 while `field_6F0` is set); below a
/// tenth of `hpMax` the low-HP sequence (7, or 8 while `field_6F0` is set);
/// otherwise, when `field_6F2` is clear or `field_6EC` set, the flinch sequence
/// 5 for damage below 0x50 and 6 above. A sequence change restarts its state
/// and clears bit 0x8000 of `field_582`; the `field_6F0` variants leave a
/// sequence already held by `field_6F2` running.
void func_actor_403900_801324E8(Actor403900* arg0, s32 arg1)
{
    GpEnemy*         enemy = arg0->field_20;
    s16              hp    = enemy->hp;
    Actor403900Work* work  = arg0->field_1C;
    u32              state = 0;
    s32              max;

    if (hp <= 0) {
        state = 6;
        if (work->field_6F0 == 0) {
            state = 5;
        }
        if (work->field_6B8 != 0) {
            SndEvt_EnqueueType7(work->field_6B8, 1);
            work->field_6B8 = 0;
        }
        if (work->field_6BC != 0) {
            SndEvt_EnqueueType7(work->field_6BC, 1);
            work->field_6BC = 0;
        }
    } else if (max = enemy->param->hpMax, hp < max / 10) {
        state = 4;
        if (work->field_6F0 == 0) {
            state = 3;
        }
    } else if (work->field_6F2 == 0 || work->field_6EC != 0) {
        work->field_6F2 = 0;
        state           = 2;
        if (arg1 < 0x50) {
            state = 1;
        }
    }

    switch (state) {
        case 0:
            break;
        case 1:
            work->field_6CC  = 5;
            work->field_6CE  = 0;
            work->field_582 &= 0x7FFF;
            break;
        case 2:
            work->field_6CC  = 6;
            work->field_6CE  = 0;
            work->field_582 &= 0x7FFF;
            break;
        case 3:
            work->field_6CC  = 7;
            work->field_6CE  = 0;
            work->field_582 &= 0x7FFF;
            break;
        case 4:
            if (work->field_6F2 == 0) {
                work->field_6CC = 8;
                work->field_6CE = 0;
            }
            break;
        case 5:
            work->field_6CC  = 9;
            work->field_6CE  = 0;
            work->field_582 &= 0x7FFF;
            break;
        case 6:
            if (work->field_6F2 == 0) {
                work->field_6CC = 10;
                work->field_6CE = 0;
            }
            break;
    }
}

/// Word after the reaction switch's jump table, closing its object's rodata.
/// Nothing reads it.
const u32 D_actor_403900_80131E68 = 0;

/// Sequence 0xB, the box scan. In state 0 it walks the `field_6FA` boxes at
/// `field_6B4`: a kind-0 box whose radius `field_2` holds the player's planar
/// offset from its centre (`field_4`, `field_6`) moves to state 1 and parks the
/// target position 0x5AA behind the player, raising bit 0x4000 of `field_5BA`
/// and `field_5DA`; a kind-1 box holding the player starts sequence 3 with
/// `field_70E` at 3 and its index in `field_708`. State 1 enters sequence 1
/// (and `field_70E` 1) unless the first record is occupied, clears the target
/// flags and the record, and drops back to state 0.
void func_actor_403900_80132688(Actor403900* arg0)
{
    u8*                         head;
    ActorShared80132688Scratch* sc;
    Actor403900Work*            work;
    Actor403900Coord*           coord;
    s32                         i;

    head                  = *(u8**)G_SCRATCH_HEAD;
    work                  = arg0->field_1C;
    *(u8**)G_SCRATCH_HEAD = head - sizeof(ActorShared80132688Scratch);
    sc                    = (ActorShared80132688Scratch*)(head - sizeof(ActorShared80132688Scratch));
    switch (work->field_6CE) {
        case 0:
            for (i = 0; i < work->field_6FA; i++) {
                switch (work->field_6B4[i].field_0) {
                    case 0:
                        sc->out.vx = work->field_6B4[i].field_4 - Player_Status.coordMtx->t[0];
                        sc->out.vz = work->field_6B4[i].field_6 - Player_Status.coordMtx->t[2];
                        if (SquareRoot0(sc->out.vx * sc->out.vx + sc->out.vz * sc->out.vz) < work->field_6B4[i].field_2) {
                            work->field_6CE = 1;
                            coord           = ((Actor403900*)gameGetPtrSlot(3))->field_2C->field_8;
                            work->field_6E6 = ratan2(coord->field_0.coord.m[0][2], coord->field_0.coord.m[2][2]) & 0xFFF;
                            sc->in.vx       = 0;
                            sc->in.vy       = 0;
                            sc->in.vz       = -0x5AA;
                            gte_SetRotMatrix(&coord->field_0.coord);
                            gte_ldv0(&sc->in);
                            gte_rtv0();
                            gte_stlvnl(&sc->out);
                            work->field_6A4        = Player_Status.coordMtx->t[0] + sc->out.vx;
                            work->field_6A8        = Player_Status.coordMtx->t[1];
                            *(u8**)G_SCRATCH_HEAD += sizeof(ActorShared80132688Scratch);
                            work->field_6AC        = Player_Status.coordMtx->t[2] + sc->out.vz;
                            work->field_5BA       |= 0x4000;
                            work->field_5DA       |= 0x4000;
                            return;
                        }
                        break;
                    case 1:
                        if (work->field_6B4[i].field_8 < Player_Status.coordMtx->t[0] &&
                            Player_Status.coordMtx->t[0] < work->field_6B4[i].field_C &&
                            Player_Status.coordMtx->t[2] < work->field_6B4[i].field_A &&
                            work->field_6B4[i].field_E < Player_Status.coordMtx->t[2]) {
                            work->field_6CC        = 3;
                            work->field_6CE        = 0;
                            work->field_70E        = 3;
                            work->field_708        = i;
                            *(u8**)G_SCRATCH_HEAD += sizeof(ActorShared80132688Scratch);
                            return;
                        }
                        break;
                }
            }
            break;
        case 1:
            if (work->field_5F4.key == 0) {
                work->field_6CC = 1;
                work->field_70E = 1;
            }
            work->field_6CE  = 0;
            work->field_5BA &= 0xBFFF;
            work->field_5DA &= 0xBFFF;
            Gp_ClearRec18Occupied(&work->field_5F4);
            break;
    }
    *(u8**)G_SCRATCH_HEAD += sizeof(ActorShared80132688Scratch);
}

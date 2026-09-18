#include "common.h"

#include "main/mem.h"
#include "main/wipsys.h"

#include "gameplay/3A34.h"

#include "actors/actors_shared_80132688.h"

#include "psyq/inline_c.h"

/// `rtv0`: rotate V0 through the loaded rotation matrix, no translation. The
/// `inline_c.h` macro of that name assembles to a different word.
#define gte_rtv0_real() __asm__ volatile("nop; nop; .word 0x4A486012")

void ActorsShared80132688(ActorShared80132688* arg0)
{
    u8*                         head;
    ActorShared80132688Scratch* sc;
    ActorShared80132688Work*    work;
    GsCOORDINATE2*              coord;
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
                        sc->out.vx = work->field_6B4[i].field_4 - Player_Status.field_4->t[0];
                        sc->out.vz = work->field_6B4[i].field_6 - Player_Status.field_4->t[2];
                        if (SquareRoot0(sc->out.vx * sc->out.vx + sc->out.vz * sc->out.vz) < work->field_6B4[i].field_2) {
                            work->field_6CE = 1;
                            coord           = ((ActorShared80132688*)Game_GetPtrSlot(3))->field_2C->field_8;
                            work->field_6E6 = ratan2(coord->coord.m[0][2], coord->coord.m[2][2]) & 0xFFF;
                            sc->in.vx       = 0;
                            sc->in.vy       = 0;
                            sc->in.vz       = -0x5AA;
                            gte_SetRotMatrix(&coord->coord);
                            gte_ldv0(&sc->in);
                            gte_rtv0_real();
                            gte_stlvnl(&sc->out);
                            work->field_6A4        = Player_Status.field_4->t[0] + sc->out.vx;
                            work->field_6A8        = Player_Status.field_4->t[1];
                            *(u8**)G_SCRATCH_HEAD += sizeof(ActorShared80132688Scratch);
                            work->field_6AC        = Player_Status.field_4->t[2] + sc->out.vz;
                            work->field_5BA       |= 0x4000;
                            work->field_5DA       |= 0x4000;
                            return;
                        }
                        break;
                    case 1:
                        if (work->field_6B4[i].field_8 < Player_Status.field_4->t[0] &&
                            Player_Status.field_4->t[0] < work->field_6B4[i].field_C &&
                            Player_Status.field_4->t[2] < work->field_6B4[i].field_A &&
                            work->field_6B4[i].field_E < Player_Status.field_4->t[2]) {
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
            if (work->field_5F4.field_4 == 0) {
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

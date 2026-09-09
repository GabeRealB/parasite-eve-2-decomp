#include "common.h"

#include "gameplay/3CD8.h"

#include "main/session.h"
#include "main/task.h"
#include "main/tmd.h"

#include <psyq/inline_c.h>
#include <psyq/libgs.h>

#define gte_mvmva_real() __asm__ volatile("nop; nop; .word 0x4A486012")

typedef struct {
    u8 data[12];
} Blk12;

typedef struct {
    s32      field_0;
    SVECTOR* field_4;
    SVECTOR* field_8;
    Blk12*   field_C;
} GolemXfm;

extern s16      D_shelter_b1_golem_freezer_1_8017E6D0;
extern s16      D_shelter_b1_golem_freezer_1_8017E6D2;
extern GolemXfm D_shelter_b1_golem_freezer_1_8017E714;
extern GolemXfm D_shelter_b1_golem_freezer_1_8017E9C0;

void func_shelter_b1_golem_freezer_1_8017D7CC(GsCOORDINATE2* arg0, s16* arg1);

void func_shelter_b1_golem_freezer_1_8017D744(void)
{
    Task* slot   = (Task*)Gp_LookupSlot4(0);
    Task* task   = slot;
    s32   isNull = (slot == NULL);

    if (isNull) {
        task = Game_GetPtrSlot(3);
    }
    if (slot != NULL) {
        if (Game_Session->field_9 == 0x15) {
            D_shelter_b1_golem_freezer_1_8017E6D2 = 0;
        } else {
            D_shelter_b1_golem_freezer_1_8017E6D2 = 0x2710;
        }
    } else {
        D_shelter_b1_golem_freezer_1_8017E6D2 = 0x2710;
    }
    func_shelter_b1_golem_freezer_1_8017D7CC(((TmdObject*)task->extra)->field_8, &D_shelter_b1_golem_freezer_1_8017E6D0);
}

void func_shelter_b1_golem_freezer_1_8017D7CC(GsCOORDINATE2* coord, s16* arg1)
{
    MATRIX    m;
    long      flag;
    s32       i;
    SVECTOR*  d;
    SVECTOR*  s;
    GolemXfm* dst = &D_shelter_b1_golem_freezer_1_8017E9C0;
    GolemXfm* src = &D_shelter_b1_golem_freezer_1_8017E714;

    i = 0;
    do {
        dst->field_4[i].vx = src->field_4[i].vx;
        dst->field_4[i].vy = src->field_4[i].vy;
        dst->field_4[i].vz = src->field_4[i].vz;
        dst->field_C[i]    = src->field_C[i];
        i++;
    } while (i <= 0);

    for (i = 0; i < 4; i++) {
        dst->field_8[i].vx = src->field_8[i].vx;
        dst->field_8[i].vy = src->field_8[i].vy;
        dst->field_8[i].vz = src->field_8[i].vz;
    }

    m = coord->coord;

    if (arg1 != NULL) {
        m.t[0] += arg1[0];
        m.t[1] += arg1[1];
        m.t[2] += arg1[2];
    }

    d = dst->field_4;
    s = src->field_4;
    i = 0;
    do {
        gte_SetRotMatrix(&m);
        gte_ldv0(s);
        s++;
        gte_mvmva_real();
        gte_stsv(d);
        d++;
        i++;
    } while (i <= 0);

    gte_SetRotMatrix(&m);
    gte_SetTransMatrix(&m);
    d = dst->field_8;
    s = src->field_8;
    for (i = 0; i < 4; i++) {
        RotTransSV(s++, d++, &flag);
    }
}

INCLUDE_ASM("rooms/nonmatchings/shelter_b1_golem_freezer_1/shelter_b1_golem_freezer_1_2", func_shelter_b1_golem_freezer_1_8017DA7C);

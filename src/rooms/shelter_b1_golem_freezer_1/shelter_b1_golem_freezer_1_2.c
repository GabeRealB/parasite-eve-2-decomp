#include "common.h"

#include "gameplay/3CD8.h"
#include "gameplay/D4.h"

#include "main/session.h"
#include "main/task.h"
#include "main/tmd.h"
#include "rooms/room_common.h"
#include "rooms/shelter_b1_golem_freezer_1.h"

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
extern SVECTOR  D_shelter_b1_golem_freezer_1_8017E738[];
extern SVECTOR  D_shelter_b1_golem_freezer_1_8017E740[];
extern s32      D_80070F70;
extern u32      Gp_LcgState;

#define GOLEM_RAND() ((Gp_LcgState = Gp_LcgState * 5 + 0x71357911) >> 16)

void func_shelter_b1_golem_freezer_1_8017D7CC(GsCOORDINATE2* arg0, s16* arg1);

void func_shelter_b1_golem_freezer_1_8017D744(void)
{
    Task* slot   = (Task*)Gp_LookupSlot4(0);
    Task* task   = slot;
    s32   isNull = (slot == NULL);

    if (isNull) {
        task = gameGetPtrSlot(3);
    }
    if (slot != NULL) {
        if (gGameSession->at4.loc.place == 0x15) {
            D_shelter_b1_golem_freezer_1_8017E6D2 = 0;
        } else {
            D_shelter_b1_golem_freezer_1_8017E6D2 = 0x2710;
        }
    } else {
        D_shelter_b1_golem_freezer_1_8017E6D2 = 0x2710;
    }
    func_shelter_b1_golem_freezer_1_8017D7CC(((TmdObject*)task->extra)->coords, &D_shelter_b1_golem_freezer_1_8017E6D0);
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

void func_shelter_b1_golem_freezer_1_8017DA7C(void)
{
    SVECTOR pos;
    s32     i;
    s32     ang;
    s32     r;

    if (!(D_80070F70 & 3)) {
        for (i = 0; i < 9; i++) {
            ang    = GOLEM_RAND() & 0xFFF;
            r      = (GOLEM_RAND() & 0x3C0) + 0x40;
            pos.vx = D_shelter_b1_golem_freezer_1_8017E738[i + 2].vx + ((r * rcos(ang)) >> 12);
            pos.vy = -(GOLEM_RAND() & 0xFF);
            pos.vz = D_shelter_b1_golem_freezer_1_8017E738[i + 2].vz + ((r * rsin(ang)) >> 12);
            Gp_SpawnEff(0x601A6, NULL, (GOLEM_RAND() & 0x10FF) + 0x85400, &pos);
        }
    }
    switch (Gp_GetViewIndex() & 0xFF) {
        case 3:
            func_shelter_b1_golem_freezer_1_8017DC5C(D_shelter_b1_golem_freezer_1_8017E738, 0x200, 0x421);
            break;
        case 4:
            func_shelter_b1_golem_freezer_1_8017DC5C(D_shelter_b1_golem_freezer_1_8017E738, 0x200, 0x210);
            break;
        case 2:
        case 5:
            func_shelter_b1_golem_freezer_1_8017DC5C(D_shelter_b1_golem_freezer_1_8017E740, 0x200, 0x421);
            break;
    }
}

#include "common.h"

#include "gameplay/3A34.h"
#include "gameplay/3CD8.h"
#include "main/gameflag.h"
#include "main/mem.h"
#include "main/session.h"
#include "main/task.h"
#include "main/tmd.h"
#include "rooms/dryfield_night_factory.h"

#include <psyq/inline_c.h>
#include <psyq/libgte.h>

#define gte_mvmva_real() __asm__ volatile("nop; nop; .word 0x4A486012")

extern void     func_8004BFF8(s32 angle, MATRIX* matrix);
extern TaskDesc D_dryfield_night_factory_80186DE0[];
extern TaskDesc D_dryfield_night_factory_80186E28[];

/// The two collision-grid templates the factory model's faces are rebuilt
/// from.
extern GpGridParams D_dryfield_night_factory_80186CF0;
extern GpGridParams D_dryfield_night_factory_80186DBC;

INCLUDE_RODATA("rooms/nonmatchings/dryfield_night_factory/dryfield_night_factory", D_dryfield_night_factory_8017D5C4);

INCLUDE_RODATA("rooms/nonmatchings/dryfield_night_factory/dryfield_night_factory", D_dryfield_night_factory_8017D5D0);

INCLUDE_RODATA("rooms/nonmatchings/dryfield_night_factory/dryfield_night_factory", D_dryfield_night_factory_8017D5DC);

/// State 0 of the room's factory model: allocate the work block, seed it from
/// the progress nibble, point the model's coordinate at the seeded position
/// and the light/color matrices at the block's own, then pick the spawn table
/// for this session variant and hand the model to its own state machine.
///
/// The two spawn tables are passed straight to `Task_SpawnFromTable` from each
/// arm rather than through a variable: the argument is then a bare symbol, so
/// the `lui`/`addiu` pair is built in `$a0` itself and `jump2` merges the two
/// arms' identical tails back into one call.
void func_dryfield_night_factory_8017D6F8(Task* task)
{
    NightFactoryWork* work;
    GsCOORDINATE2*    coord;
    TmdObject*        obj;

    obj   = (TmdObject*)task->extra;
    coord = obj->coords;
    work  = memCalloc(0x58, 0);
    if (work == NULL) {
        taskKill(task);
        return;
    }
    task->work     = (TaskIdMap*)work;
    work->field_0  = GameFlag_GetNibble(0x49);
    work->field_16 = -1;
    work->field_17 = -1;
    obj->flags    &= 0xFF7F;
    if (work->field_0 & 1) {
        work->field_10.value = 0x4000000;
        func_8004BFF8(0x4000000, &coord->coord);
    }
    if (work->field_0 & 2) {
        work->field_C.value = 0xFDC60000;
    } else {
        work->field_C.value = 0;
    }
    coord->coord.t[0] = 0xE4C;
    coord->coord.t[1] = work->field_C.part.whole;
    coord->coord.t[2] = 0x1AAE;
    func_dryfield_night_factory_8017FB68(task);
    func_dryfield_night_factory_8017D858(task, 1, 0);
    if (gGameSession->at4.loc.stage == 2) {
        Task_SpawnFromTable(D_dryfield_night_factory_80186E28, 7, 0, (s32)task);
    } else {
        Task_SpawnFromTable(D_dryfield_night_factory_80186DE0, 7, 0, (s32)task);
    }
    task->exitCallback  = func_dryfield_night_factory_8017FB48;
    task->killCountdown = 0;
    task->state++;
}

void func_dryfield_night_factory_8017D858(Task* task, s32 remapFaces, s32 useAltTemplate)
{
    long           flag;
    GsCOORDINATE2* coord;
    MATRIX*        m;
    GpGridParams*  geom;
    GpGridParams*  src;
    SVECTOR*       s;
    SVECTOR*       d;
    GpGridFace*    sf;
    GpGridFace*    df;
    u16*           sv;
    u16*           dv;
    s32            i;
    s32            j;

    coord = ((TmdObject*)task->extra)->coords;
    if (gGameSession->at4.loc.stage == 2) {
        geom = &D_dryfield_night_factory_80187BF8;
    } else {
        geom = &D_dryfield_night_factory_80187BF0;
    }
    if (useAltTemplate != 0) {
        src = &D_dryfield_night_factory_80186DBC;
    } else {
        src = &D_dryfield_night_factory_80186CF0;
    }

    m = &coord->coord;
    s = src->field_4;
    d = geom->field_4 + 2;
    for (i = 0; i < 4; i++) {
        gte_SetRotMatrix(m);
        gte_ldv0(s);
        s++;
        gte_mvmva_real();
        gte_stsv(d);
        d++;
    }

    gte_SetRotMatrix(m);
    gte_SetTransMatrix(m);
    s = src->field_8;
    d = geom->field_8 + 8;
    for (i = 0; i < 8; i++) {
        RotTransSV(s++, d++, &flag);
    }

    if (remapFaces != 0) {
        sf = src->field_C;
        df = geom->field_C + 2;
        for (i = 0; i < 4; i++) {
            j  = 0;
            dv = df->verts;
            sv = sf->verts;
            do {
                *dv++ = *sv++ + 8;
            } while (++j < 4);
            df->field_8 = sf->field_8 + 2;
            df->field_A = sf->field_A;
            df++;
            sf++;
        }
    }
}

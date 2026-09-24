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
#include <psyq/libgs.h>

#define gte_mvmva_real() __asm__ volatile("nop; nop; .word 0x4A486012")

extern TaskFuncTable3 D_dryfield_factory_8017D5DC;

extern TaskDesc D_dryfield_factory_80186DE0[];
extern TaskDesc D_dryfield_factory_80186E28[];

extern GpGridParams D_dryfield_factory_80186D38;
extern GpGridParams D_dryfield_factory_80186E04;
extern GpGridParams D_dryfield_factory_80187BF0;
extern GpGridParams D_dryfield_factory_80187BF8;

extern void Room_Util20(Task* task);
extern void Room_Script16(Task* task);
extern void func_8004BFF8(s32 angle, MATRIX* matrix);

void func_dryfield_factory_8017E140(Task* task, s32 remapFaces, s32 useAltTemplate);

void func_dryfield_factory_8017DF80(void)
{
}

/// Runs the task's current state out of a three-entry table copied onto the
/// stack.
void func_dryfield_factory_8017DF88(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_dryfield_factory_8017D5DC;
    sp.funcs[task->state](task);
}

/// State 0 of the factory model: allocates and seeds its work block from
/// progress nibble 0x49, places the model, rebuilds the collision faces it
/// carries, spawns entry 7 of the session variant's spawn table with the task
/// as its argument and advances.
///
/// Each arm passes its table straight to `Task_SpawnFromTable` rather than
/// through a variable, which lets the two identical call tails merge as in the
/// target.
void func_dryfield_factory_8017DFE0(Task* task)
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
    Room_Util20(task);
    func_dryfield_factory_8017E140(task, 1, 0);
    if (gGameSession->at4.loc.stage == 2) {
        Task_SpawnFromTable(D_dryfield_factory_80186E28, 7, 0, (s32)task);
    } else {
        Task_SpawnFromTable(D_dryfield_factory_80186DE0, 7, 0, (s32)task);
    }
    task->exitCallback  = Room_Script16;
    task->killCountdown = 0;
    task->state++;
}

/// Rebuilds four faces of the room's collision grid from a template, moved
/// into the frame of the task's model: the template normals are rotated into
/// grid normals 2..5 and its corners rotated and translated into corners 8..15.
/// When `remapFaces` is set, the template's four face records are copied into
/// faces 2..5 with their corner and normal indices rebased onto those slots.
void func_dryfield_factory_8017E140(Task* task, s32 remapFaces, s32 useAltTemplate)
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
        geom = &D_dryfield_factory_80187BF8;
    } else {
        geom = &D_dryfield_factory_80187BF0;
    }
    if (useAltTemplate != 0) {
        src = &D_dryfield_factory_80186E04;
    } else {
        src = &D_dryfield_factory_80186D38;
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

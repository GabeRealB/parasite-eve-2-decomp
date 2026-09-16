#include "common.h"

#include "gameplay/3CD8.h"
#include "gameplay/gameplay.h"
#include "main/gfx.h"
#include "main/mem.h"
#include "main/task.h"
#include "rooms/dryfield_dilapidated_house.h"
#include "main/tmd.h"

void func_dryfield_dilapidated_house_80183728(GsCOORDINATE2* coord, s16 arg1, s32 arg2, s16 arg3);
void func_dryfield_dilapidated_house_801815E8(GsCOORDINATE2* coord, s32 arg1);
void func_dryfield_dilapidated_house_8018142C(Task* task);
void func_dryfield_dilapidated_house_80180738(Task* task, SVECTOR* verts);
void func_dryfield_dilapidated_house_801803A4(Task* task, SVECTOR* verts);

extern DdhRoomRec D_dryfield_dilapidated_house_8018669C;

/// Steps the task's 0..0x1000 ramp by 0x44, saturating at 0x1000, and feeds the
/// distance still to run (`0x1000 - ramp`) to the room record's matrix/vertex
/// interpolator. Returns the ramp value, which the caller stores into its
/// `DdhCoordWork`.
s32 func_dryfield_dilapidated_house_80180FD8(Task* task)
{
    s32 ramp;

    ramp = task->killCountdown + 0x44;
    if (ramp >= 0x1001) {
        ramp = 0x1000;
    }
    task->killCountdown = ramp;
    func_dryfield_dilapidated_house_80180A0C(task, &D_dryfield_dilapidated_house_8018669C, 0x1000 - ramp);
    return ramp;
}

/// Rebuilds the work block's `mtx` as the identity, then composes it against
/// the parent model's `GsCOORDINATE2` chain: each node's `coord` rotation is
/// multiplied in, and its translation is rotated by the accumulated matrix and
/// added to `mtx.t`. Steps one coordinate record at a time from the head of the
/// parent's array up to the record this task's own `coord` links with `sub`.
void func_dryfield_dilapidated_house_80181028(Task* task)
{
    VECTOR         vec;
    GsCOORDINATE2* coord;
    DdhCoordWork*  work;
    GsCOORDINATE2* node;
    MATRIX*        mtx;

    coord                = (GsCOORDINATE2*)((TmdObject*)task->extra)->field_8;
    work                 = (DdhCoordWork*)task->idMap;
    node                 = (GsCOORDINATE2*)((TmdObject*)((Task*)task->spawnArg2)->extra)->field_8;
    mtx                  = &work->mtx;
    *(s32*)&work->mtx    = ONE;
    *(s32*)&mtx->m[0][2] = 0;
    *(s32*)&mtx->m[1][1] = ONE;
    *(s32*)&mtx->m[2][0] = 0;
    mtx->m[2][2]         = ONE;
    mtx->t[0]            = 0;
    mtx->t[1]            = 0;
    mtx->t[2]            = 0;
    do {
        ApplyMatrixLV(mtx, (VECTOR*)node->coord.t, &vec);
        mtx->t[0] += vec.vx;
        mtx->t[1] += vec.vy;
        mtx->t[2] += vec.vz;
        MulMatrix0(mtx, &node->coord, mtx);
    } while (node++ != coord->sub);
}

void func_dryfield_dilapidated_house_801810F8(TmdObject* dst, TmdObject* src)
{
    if (!(src->field_C & 0x80)) {
        dst->field_C &= ~0x80;
        return;
    }
    dst->field_C |= 0x80;
}

INCLUDE_ASM("rooms/nonmatchings/dryfield_dilapidated_house/dryfield_dilapidated_house_4", func_dryfield_dilapidated_house_80181134);

/// State 0 of the handler table at `D_dryfield_dilapidated_house_8017D61C`:
/// snapshots the placed model coordinate's matrix into a fresh `DdhModelWork`,
/// seeds its 0x1000 word, marks the model's `TmdObject` hidden (bit 0x80 of
/// `field_C`), re-parents the task that spawned this one under it and advances
/// to state 1.
void func_dryfield_dilapidated_house_8018118C(Task* arg0)
{
    TmdObject*     obj;
    GsCOORDINATE2* coord;
    DdhModelWork*  work;

    obj   = (TmdObject*)arg0->extra;
    coord = obj->field_8;
    work  = (DdhModelWork*)Mem_Malloc(0x24, false);
    if (work == NULL) {
        Task_Kill(arg0);
        return;
    }
    arg0->idMap    = (TaskIdMap*)work;
    work->field_20 = 0x1000;
    work->mtx      = coord->coord;
    obj->field_C  |= 0x80;
    Task_Reparent((Task*)arg0->spawnArg2, arg0);
    arg0->state += 1;
}

void func_dryfield_dilapidated_house_80181264(Task* arg0)
{
    func_dryfield_dilapidated_house_8017EBB8(arg0);
    func_dryfield_dilapidated_house_8017EE58(arg0);
}

/// Converts one axis of a cubic Bezier segment (control points `p0`..`p3`) into
/// the polynomial coefficients of `B(t)`, stored high order first: `t^3`, `t^2`,
/// `t` and the constant term.
void func_dryfield_dilapidated_house_80181290(s32 p0, s32 p1, s32 p2, s32 p3, SVECTOR* coeff)
{
    coeff->vx  = -p0 + (p1 - p2) * 3 + p3;
    coeff->vy  = (p0 + p2) * 3 - p1 * 6;
    coeff->vz  = (-p0 + p1) * 3;
    coeff->pad = p0;
}

INCLUDE_ASM("rooms/nonmatchings/dryfield_dilapidated_house/dryfield_dilapidated_house_4", func_dryfield_dilapidated_house_801812E8);

void func_dryfield_dilapidated_house_80181340(Task* arg0)
{
    GsCOORDINATE2* coord;
    void*          work;

    coord = (GsCOORDINATE2*)((TmdObject*)arg0->extra)->field_8;
    work  = Mem_Malloc(4, false);
    if (work == NULL) {
        Task_Kill(arg0);
        return;
    }
    arg0->idMap = work;
    coord->sub  = (GsCOORDINATE2*)((TmdObject*)((Task*)arg0->spawnArg2)->extra)->field_8;
    Task_Reparent((Task*)arg0->spawnArg2, arg0);
    arg0->exitCallback = func_dryfield_dilapidated_house_8018142C;
    arg0->state       += 1;
}

void func_dryfield_dilapidated_house_801813DC(Task* task)
{
    SVECTOR verts[24];
    s32     sp0;
    s32     sp1;

    func_dryfield_dilapidated_house_8017FAD4(task, verts, &sp0, &sp1);
    func_dryfield_dilapidated_house_8017F568(task, verts, sp0);
    func_dryfield_dilapidated_house_8017F568(task, verts, sp0);
}

void func_dryfield_dilapidated_house_8018142C(Task* arg0)
{
    GsCOORDINATE2* coord;

    coord      = (GsCOORDINATE2*)((TmdObject*)arg0->extra)->field_8;
    coord->sub = &Gfx_ViewCoord;
    Task_Kill(arg0);
}

INCLUDE_ASM("rooms/nonmatchings/dryfield_dilapidated_house/dryfield_dilapidated_house_4", func_dryfield_dilapidated_house_8018145C);

extern s32 D_dryfield_dilapidated_house_80186804[16];

/// State 0 of the handler table at `D_dryfield_dilapidated_house_8017D634`,
/// dispatched by `func_dryfield_dilapidated_house_8018145C`: fills a fresh
/// `DdhAngleStep` with the shared per-part angle table scaled by this task's spawn
/// arg (each wrapped into the 0x4000 angle period), links the model coordinate
/// this task works on to the parent model's coordinate array, and re-parents the
/// task that spawned this one under it.
void func_dryfield_dilapidated_house_801814B4(Task* arg0)
{
    DdhAngleStep*  work;
    GsCOORDINATE2* coord;
    s32            i;

    coord = (GsCOORDINATE2*)((TmdObject*)arg0->extra)->field_8;
    work  = (DdhAngleStep*)Mem_Malloc(0x40, false);
    if (work == NULL) {
        Task_Kill(arg0);
        return;
    }
    arg0->idMap = (TaskIdMap*)work;
    for (i = 0; i < 0x10; i++) {
        work->step[i] = (D_dryfield_dilapidated_house_80186804[i] * arg0->spawnArg1) & 0x3FFF;
    }
    coord->sub = (GsCOORDINATE2*)((TmdObject*)((Task*)arg0->spawnArg2)->extra)->field_8;
    Task_Reparent((Task*)arg0->spawnArg2, arg0);
    arg0->state += 1;
}

void func_dryfield_dilapidated_house_80181584(Task* task)
{
    SVECTOR verts[32];

    func_dryfield_dilapidated_house_80180738(task, verts);
    func_dryfield_dilapidated_house_801803A4(task, verts);
}

void func_dryfield_dilapidated_house_801815B8(Task* arg0)
{
    GsCOORDINATE2* coord;

    coord      = (GsCOORDINATE2*)((TmdObject*)arg0->extra)->field_8;
    coord->sub = &Gfx_ViewCoord;
    Task_Kill(arg0);
}

INCLUDE_ASM("rooms/nonmatchings/dryfield_dilapidated_house/dryfield_dilapidated_house_4", func_dryfield_dilapidated_house_801815E8);

INCLUDE_ASM("rooms/nonmatchings/dryfield_dilapidated_house/dryfield_dilapidated_house_4", func_dryfield_dilapidated_house_80181F08);

INCLUDE_ASM("rooms/nonmatchings/dryfield_dilapidated_house/dryfield_dilapidated_house_4", func_dryfield_dilapidated_house_801823B8);

INCLUDE_ASM("rooms/nonmatchings/dryfield_dilapidated_house/dryfield_dilapidated_house_4", func_dryfield_dilapidated_house_80182744);

INCLUDE_ASM("rooms/nonmatchings/dryfield_dilapidated_house/dryfield_dilapidated_house_4", func_dryfield_dilapidated_house_80182A18);

INCLUDE_ASM("rooms/nonmatchings/dryfield_dilapidated_house/dryfield_dilapidated_house_4", func_dryfield_dilapidated_house_80182F14);

INCLUDE_ASM("rooms/nonmatchings/dryfield_dilapidated_house/dryfield_dilapidated_house_4", func_dryfield_dilapidated_house_801832A8);

INCLUDE_ASM("rooms/nonmatchings/dryfield_dilapidated_house/dryfield_dilapidated_house_4", func_dryfield_dilapidated_house_80183728);

void func_dryfield_dilapidated_house_80183BF8(Task* arg0)
{
    GsCOORDINATE2* coord;
    s32            mask;

    mask  = 1 << (u8)Game_Session->field_4;
    coord = (GsCOORDINATE2*)((TmdObject*)arg0->extra)->field_8;
    if (mask & 0x84A9C) {
        func_dryfield_dilapidated_house_801815E8(coord, 0);
    }
    if (mask & 0x104B98) {
        func_dryfield_dilapidated_house_801815E8(coord, 8);
    }
    if (mask & 0xA55F8) {
        func_dryfield_dilapidated_house_801815E8(coord, 0x10);
    }
}

INCLUDE_ASM("rooms/nonmatchings/dryfield_dilapidated_house/dryfield_dilapidated_house_4", func_dryfield_dilapidated_house_80183C8C);

/// Per-frame handler of the effect family whose work block is `DdhEffWork`
/// (`task->spawnArg2`). While the `Gp_State1C` state word at 0x4 is clear it
/// seeds the ramp (0x80 / 0x100) on the first frame and then, every frame,
/// clears the model coordinate's update flag, refreshes the coordinate and feeds
/// the angle/scale pair to `func_dryfield_dilapidated_house_80183728`, stepping
/// the scale by -8 and the angle by +0x80. Once the scale drops below 9 - and
/// immediately when that state word has already reached 4 - it releases the work
/// block through `Gp_ReleaseState1CMem`.
void func_dryfield_dilapidated_house_80183D5C(Task* arg0)
{
    DdhEffWork*    mem;
    GsCOORDINATE2* coord;
    s16            flag;
    s32            scale;
    s32            angle;

    mem   = arg0->spawnArg2;
    flag  = Gp_State1C->field_4;
    coord = (GsCOORDINATE2*)((TmdObject*)arg0->extra)->field_8;
    if (flag != 0) {
        if (flag >= 4) {
            Gp_ReleaseState1CMem(mem, arg0);
        }
        return;
    }

    if (arg0->state == 0) {
        Gfx_RotMatrixZ(&coord->coord, arg0->spawnArg1, 0);
        coord->flg = 0;
        Gp_UpdateCoord(coord);
        mem->field_24 = 0x80;
        mem->field_26 = 0x100;
        arg0->state   = 1;
    }

    func_dryfield_dilapidated_house_80183728(coord, mem->field_26, 0x100, mem->field_24);
    angle         = (u16)mem->field_26;
    scale         = (u16)mem->field_24;
    angle        += 0x80;
    scale        -= 8;
    mem->field_24 = scale;
    mem->field_26 = angle;
    if ((s16)scale < 9) {
        Gp_ReleaseState1CMem(mem, arg0);
    }
}

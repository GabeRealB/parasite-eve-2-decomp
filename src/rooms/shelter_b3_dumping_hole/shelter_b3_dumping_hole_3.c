#include "common.h"
#include "gameplay/1BC.h"
#include "gameplay/3A34.h"
#include "gameplay/3CD8.h"
#include "gameplay/3FB8.h"
#include "gameplay/D4.h"
#include "gameplay/gameplay.h"
#include "main/display.h"
#include "main/fs.h"
#include "main/gameflag.h"
#include "main/gameflow.h"
#include "main/gfx.h"
#include "main/mem.h"
#include "main/session.h"
#include "main/task.h"
#include "main/tmd.h"
#include <psyq/inline_c.h>
#include <psyq/libgs.h>
#include <psyq/libgte.h>
extern TaskDesc D_shelter_b3_dumping_hole_80188C04;
extern TaskDesc D_shelter_b3_dumping_hole_80188BC8;
extern s16      D_shelter_b3_dumping_hole_8018809C;

typedef struct {
    s32   field_0;
    s32   field_4;
    s32   field_8;
    u8    pad_0C[0x4];
    s16   field_10;
    s16   field_12;
    s16   field_14;
    u8    pad_16[0xE];
    Task* field_24;
    Task* field_28;
    Task* field_2C;
    u16   field_30;
    u16   field_32;
    u16   field_34;
    u8    pad_36[0x2];
    s16   field_38;
    s16   field_3A;
    u8    pad_3C[0x4];
    s16   field_40;
    u16   field_42;
    s16   field_44;
    s16   field_46;
    s16   field_48;
    s16   field_4A;
    u16   field_4C;
} DumpingHoleEntity;

typedef struct {
    u8  pad_0[0x2];
    s16 r;
    s16 g;
    s16 b;
} DumpingHoleFadeWork;

typedef struct {
    u8                 pad_00[0x1C];
    DumpingHoleEntity* field_1C;
} DumpingHoleState;

extern DumpingHoleState* D_shelter_b3_dumping_hole_8018F4A8;
extern TaskFuncTable3    D_shelter_b3_dumping_hole_8017D5C4;

void func_shelter_b3_dumping_hole_8017D9A8(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_shelter_b3_dumping_hole_8017D5C4;
    sp.funcs[task->state](task);
}

INCLUDE_ASM("rooms/nonmatchings/shelter_b3_dumping_hole/shelter_b3_dumping_hole_3", func_shelter_b3_dumping_hole_8017DA00);

typedef struct {
    s16 field_0;
    s16 field_2;
    s16 field_4;
    u8  pad_6[0x2];
    s16 field_8;
    u8  pad_A[0xA];
    s16 field_14;
    s16 field_16;
    s16 field_18;
    u8  pad_1A[0x2];
    s16 field_1C;
    u16 field_1E;
    u16 field_20;
} DumpingHoleAnimWork;

typedef struct {
    s16 field_0;
    s16 field_2;
    s16 field_4;
    s16 field_6;
    s16 field_8;
    s16 field_A;
} DumpingHoleAnimFrame;

extern u32                  Gp_LcgState;
extern s16                  D_shelter_b3_dumping_hole_80188154[];
extern DumpingHoleAnimFrame D_shelter_b3_dumping_hole_801880B8[];

u16 func_shelter_b3_dumping_hole_8017DA00(GsCOORDINATE2* coord, s16 arg1, s16 arg2,
                                          s16 arg3, s16 arg4, s16 arg5, s16 arg6,
                                          s16 arg7, s16 arg8, s16 arg9);

void func_shelter_b3_dumping_hole_8017DCFC(Task* arg0)
{
    DumpingHoleAnimWork* W      = (DumpingHoleAnimWork*)arg0->work;
    GsCOORDINATE2*       coord  = ((TmdObject*)arg0->extra)->coords;
    DumpingHoleEntity*   entity = D_shelter_b3_dumping_hole_8018F4A8->field_1C;

    if (entity->field_42 == 1) {
        taskKill((Task*)arg0);
        return;
    }

    switch (arg0->state) {
        case 0:
            coord->sub        = &gGfxViewCoord;
            coord->coord.t[0] = W->field_0;
            coord->coord.t[1] = W->field_2;
            coord->coord.t[2] = W->field_4;
            arg0->state++;
            return;
        case 1:
            if (entity->field_42 != 2) {
                return;
            }
            W->field_1C = 5;
            W->field_14 = 0;
            W->field_18 = 0;
            Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
            W->field_16 = 0xFFF6 - ((Gp_LcgState >> 16) & 7);
            Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
            W->field_20 = (Gp_LcgState >> 16) & 7;
            arg0->state++;
            return;
        case 2:
            if (W->field_20 == 0) {
                arg0->state = 3;
            } else {
                W->field_20--;
            }
            return;
        case 3: {
            s32 t1e     = W->field_1E + 1;
            W->field_1E = t1e;
            if (D_shelter_b3_dumping_hole_80188154[W->field_1C] < (s16)t1e) {
                *(u16*)&W->field_1C = *(u16*)&W->field_1C + 1;
                W->field_1E         = 0;
                if (*(u16*)&D_shelter_b3_dumping_hole_801880B8[W->field_1C].field_0 == 0xFFFF) {
                    taskKill((Task*)arg0);
                    return;
                }
            }
            break;
        }
        default:
            return;
    }

    coord->coord.t[1] += W->field_16;
    if (func_shelter_b3_dumping_hole_8017DA00(
            coord,
            D_shelter_b3_dumping_hole_801880B8[W->field_1C].field_8,
            D_shelter_b3_dumping_hole_801880B8[W->field_1C].field_A,
            D_shelter_b3_dumping_hole_801880B8[W->field_1C].field_2,
            D_shelter_b3_dumping_hole_801880B8[W->field_1C].field_6,
            D_shelter_b3_dumping_hole_801880B8[W->field_1C].field_0,
            D_shelter_b3_dumping_hole_801880B8[W->field_1C].field_4,
            W->field_8, 0x43C0, 0) != 0) {
        taskKill((Task*)arg0);
        return;
    }
    coord->flg = 0;
}

INCLUDE_ASM("rooms/nonmatchings/shelter_b3_dumping_hole/shelter_b3_dumping_hole_3", func_shelter_b3_dumping_hole_8017DF90);

extern s16 D_shelter_b3_dumping_hole_80188184[];

void func_shelter_b3_dumping_hole_8017E440(Task* arg0)
{
    DumpingHoleAnimWork* work  = (DumpingHoleAnimWork*)arg0->work;
    GsCOORDINATE2*       coord = ((TmdObject*)arg0->extra)->coords;
    SVECTOR              vec;
    s32                  sa1;
    u32                  roll1;
    u32                  roll2;
    s32                  base18;
    s16                  var0;
    s16                  delta;

    if (*(u16*)&D_shelter_b3_dumping_hole_8018F4A8->field_1C->field_48 == 1) {
        taskKill(arg0);
        return;
    }

    switch (arg0->state) {
        case 0:
            coord->sub = &gGfxViewCoord;
            Gp_ComposeParentWorld((GsCOORDINATE2*)arg0->spawnArg2, &coord->coord, &vec);
            coord->coord.t[0] = vec.vx;
            coord->coord.t[1] = vec.vy;
            coord->coord.t[2] = vec.vz;
            arg0->work        = (TaskIdMap*)Mem_Malloc(0x24, 0);
            if (arg0->work == NULL) {
                taskKill(arg0);
                return;
            }
            work = (DumpingHoleAnimWork*)arg0->work;
            Mem_Set(work, 0, 0x24);
            work->field_16 = -0xA;
            work->field_14 = 0;
            work->field_18 = 0;
            work->field_8  = 0x1000;
            work->field_14 = 0;
            roll1          = Gp_LcgState * 5 + 0x71357911;
            work->field_16 = 0xFFF1 - ((roll1 >> 16) & 7);
            sa1            = arg0->spawnArg1;
            Gp_LcgState    = roll1;
            if (sa1 == 0) {
                roll2       = roll1 * 5 + 0x71357911;
                base18      = work->field_18;
                Gp_LcgState = roll2;
                if ((roll2 >> 16) & 1) {
                    Gp_LcgState = roll2 * 5 + 0x71357911;
                    var0        = base18 + ((Gp_LcgState >> 16) & 1);
                } else {
                    Gp_LcgState = roll2 * 5 + 0x71357911;
                    var0        = base18 - ((Gp_LcgState >> 16) & 1);
                }
                work->field_18 = var0;
            } else {
                if (sa1 < 0) {
                    Gp_LcgState = roll1 * 5 + 0x71357911;
                    delta       = *(u16*)&work->field_18 + ((u16)arg0->spawnArg1 - ((Gp_LcgState >> 16) & 1));
                } else {
                    Gp_LcgState = roll1 * 5 + 0x71357911;
                    delta       = *(u16*)&work->field_18 + ((u16)arg0->spawnArg1 + ((Gp_LcgState >> 16) & 1));
                }
                work->field_18 = delta;
            }
            work->field_1C = 0;
            Gp_LcgState    = Gp_LcgState * 5 + 0x71357911;
            work->field_20 = (Gp_LcgState >> 16) & 7;
            arg0->state++;
            return;
        case 1:
            if (work->field_20 == 0) {
                arg0->state = 2;
            } else {
                work->field_20--;
            }
            return;
        case 2: {
            s32 t1e        = work->field_1E + 1;
            work->field_1E = t1e;
            if (D_shelter_b3_dumping_hole_80188184[work->field_1C] < (s16)t1e) {
                *(u16*)&work->field_1C = *(u16*)&work->field_1C + 1;
                work->field_1E         = 0;
                if (*(u16*)&D_shelter_b3_dumping_hole_801880B8[work->field_1C].field_0 == 0xFFFF) {
                    taskKill(arg0);
                    return;
                }
            }
            coord->coord.t[1] += work->field_16;
            coord->coord.t[2] += work->field_18;
            func_shelter_b3_dumping_hole_8017DA00(
                coord,
                D_shelter_b3_dumping_hole_801880B8[work->field_1C].field_8,
                D_shelter_b3_dumping_hole_801880B8[work->field_1C].field_A,
                D_shelter_b3_dumping_hole_801880B8[work->field_1C].field_2,
                D_shelter_b3_dumping_hole_801880B8[work->field_1C].field_6,
                D_shelter_b3_dumping_hole_801880B8[work->field_1C].field_0,
                D_shelter_b3_dumping_hole_801880B8[work->field_1C].field_4,
                work->field_8, 0x43C0, 0);
            coord->flg = 0;
            return;
        }
    }
}

typedef struct {
    s32 field_0;
    s32 field_4;
    s32 field_8;
    u8  pad_C[0x4];
    s16 field_10;
    s16 field_12;
    s16 field_14;
} DumpingHoleCoordCfg;

typedef struct {
    MATRIX field_0;
    MATRIX field_20;
    u16    rotX; // Accumulated rotation about X, advanced by `spinX` each frame
    u16    rotY; // Accumulated rotation about Y, advanced by `spinY` each frame
    u16    rotZ; // Accumulated rotation about Z; advanced but never applied
    u8     pad_46[0x2];
    s16    velX; // Per-frame translation added to the coordinate
    s16    velY;
    s16    velZ;
    u8     pad_4E[0x2];
    s16    spinX; // Per-frame rotation step, chosen at random at launch
    s16    spinY;
    s16    spinZ;
    u8     pad_56[0x2];
    u16    fall; // Downward speed added to `velY`, growing by 5 each frame
    u8     pad_5A[0x2];
} DumpingHoleCoordWork;

void func_shelter_b3_dumping_hole_8017E7DC(Task* arg0)
{
    DumpingHoleCoordWork*         work;
    register TmdObject*           extra asm("s2");
    GsCOORDINATE2*                coord;
    register DumpingHoleCoordCfg* cfg asm("s3");
    VECTOR                        v;
    TmdObject*                    e2;

    extra      = (TmdObject*)arg0->extra;
    cfg        = (DumpingHoleCoordCfg*)arg0->spawnArg2;
    coord      = extra->coords;
    work       = (DumpingHoleCoordWork*)Mem_Malloc(0x5C, 0);
    arg0->work = (TaskIdMap*)work;
    if (work == NULL) {
        taskKill(arg0);
        return;
    }
    Mem_Set(work, 0, 0x5C);
    coord->sub                       = &gGfxViewCoord;
    ((TmdObject*)arg0->extra)->flags = 0;
    Tmd_AllocBuffers(extra);
    extra->lightMtx   = &work->field_0;
    extra->colorMtx   = &work->field_20;
    coord->coord.t[0] = cfg->field_0;
    coord->coord.t[1] = cfg->field_4;
    coord->coord.t[2] = cfg->field_8;
    Gfx_RotMatrixY(&coord->coord, cfg->field_12, 1);
    Gfx_RotMatrixX(&coord->coord, cfg->field_10, 0);
    Gfx_RotMatrixZ(&coord->coord, cfg->field_14, 0);
    coord->flg = 0;
    Task_Reparent((Task*)D_shelter_b3_dumping_hole_8018F4A8, arg0);
    Gp_UpdateCoord(coord);
    e2   = (TmdObject*)arg0->extra;
    v.vx = e2->coords->workm.t[0];
    v.vy = ((TmdObject*)arg0->extra)->coords->workm.t[1];
    v.vz = ((TmdObject*)arg0->extra)->coords->workm.t[2];
    func_800D7A9C(e2, &v, 0, 3);
}

#define gte_rtps_real() __asm__ volatile("nop; nop; .word 0x4A180001")

#define DUMPING_HOLE_RAND() ((s32)((Gp_LcgState = Gp_LcgState * 5 + 0x71357911) >> 16))

/// Stack block for projecting a point through `GsWSMATRIX`: the point, then
/// the screen position and depth the projection writes back.
typedef struct {
    SVECTOR pos;
    s16     sx;
    s16     sy;
    s32     otz;
} DumpingHoleProjection;

/// Debris thrown from the hole. Once the room signals, the piece is projected
/// to the screen: off-screen or behind the camera it is dropped, otherwise it
/// is launched away from the screen centre with a random speed and spin, and
/// then falls under a growing downward speed.
void func_shelter_b3_dumping_hole_8017E94C(Task* arg0)
{
    DumpingHoleCoordWork* work  = (DumpingHoleCoordWork*)arg0->work;
    u16                   flag  = D_shelter_b3_dumping_hole_8018F4A8->field_1C->field_40;
    GsCOORDINATE2*        coord = ((TmdObject*)arg0->extra)->coords;
    GsCOORDINATE2*        c2;
    DumpingHoleProjection p;
    s32                   sx;
    s32                   sy;
    s16                   angle;
    s32                   x;
    s32                   y;
    s32                   z;

    if (flag == 2) {
        taskKill(arg0);
        return;
    }
    switch (arg0->state) {
        case 0:
            func_shelter_b3_dumping_hole_8017E7DC(arg0);
            arg0->state++;
            return;
        case 1:
            if (flag == 1) {
                arg0->state = 2;
            }
            return;
        case 2:
            p.pos.vx = coord->workm.t[0];
            p.pos.vy = coord->workm.t[1];
            p.pos.vz = coord->workm.t[2];
            gte_SetTransMatrix(&GsWSMATRIX);
            gte_SetRotMatrix(&GsWSMATRIX);
            gte_ldv0(&p.pos);
            gte_rtps_real();
            gte_stsxy(&p.sx);
            gte_stszotz(&p.otz);
            sx = p.sx;
            sy = p.sy;
            if (sx < -0xA0) {
                taskKill(arg0);
                return;
            }
            if (sx > 0xA0) {
                taskKill(arg0);
                return;
            }
            if (sy < -0x78) {
                taskKill(arg0);
                return;
            }
            if (sy > 0x78) {
                taskKill(arg0);
                return;
            }
            if (p.otz < 0) {
                taskKill(arg0);
                return;
            }
            angle      = ratan2(sy, sx);
            work->velZ = rcos(angle) * ((DUMPING_HOLE_RAND() & 7) + 0x11) / 4096;
            work->velY = rsin(angle) * ((DUMPING_HOLE_RAND() & 7) + 5) / 4096;
            switch (arg0->spawnArg1) {
                case 0:
                    work->velX = (DUMPING_HOLE_RAND() & 0x1F) + 0x32;
                    break;
                case 1:
                    work->velX = (DUMPING_HOLE_RAND() & 0x1F) + 0x28;
                    break;
                case 2:
                    work->velX = (DUMPING_HOLE_RAND() & 0x1F) + 0x1E;
                    break;
            }
            x = DUMPING_HOLE_RAND() & 0x7F;
            if (DUMPING_HOLE_RAND() & 0x8000) {
                x = -x;
            }
            work->spinX = x;
            y           = DUMPING_HOLE_RAND() & 0x7F;
            if (DUMPING_HOLE_RAND() & 0x8000) {
                y = -y;
            }
            work->spinY = y;
            z           = DUMPING_HOLE_RAND() & 0x7F;
            if (DUMPING_HOLE_RAND() & 0x8000) {
                z = -z;
            }
            work->spinZ = z;
            work->fall  = 0;
            arg0->state++;
            return;
        case 3:
            work->rotX     += work->spinX;
            work->rotY     += work->spinY;
            work->rotZ     += work->spinZ;
            work->fall     += 5;
            c2              = ((TmdObject*)arg0->extra)->coords;
            c2->sub         = &gGfxViewCoord;
            c2->coord.t[0] += work->velX;
            c2->coord.t[1] += work->velY + work->fall;
            c2->coord.t[2] += work->velZ;
            Gfx_RotMatrixY(&c2->coord, (s16)work->rotY, 1);
            Gfx_RotMatrixX(&c2->coord, (s16)work->rotX, 0);
            c2->flg = 0;
            return;
    }
}

/// Payload of message 0x7DA: the current stage and area, and a flag word.
typedef struct {
    u8  stage;
    u8  area;
    s16 field_2;
} DumpingHoleMsg7DA;

extern u8  D_80073BA9;
extern s8  D_8007218A;
extern s32 D_shelter_b3_dumping_hole_8018819C[];
extern u8  D_shelter_b3_dumping_hole_801881CC;

void func_shelter_b3_dumping_hole_8017EDB8(Task* arg0)
{
    DumpingHoleEntity* work = (DumpingHoleEntity*)arg0->work;
    union {
        GpAnimArg         anim;
        DumpingHoleMsg7DA loc;
    } msg;
    u8 area;

    if (work->field_24 != NULL) {
        Gp_DispatchMsg(work->field_24, 0x3ED, 0, 0);
    }
    switch (work->field_30) {
        case 0:
            break;
        case 1:
            switch (work->field_32) {
                case 0:
                    Gp_DispatchMsg(work->field_24, 0x3E9, (s32)D_shelter_b3_dumping_hole_8018819C, 0);
                    Gp_DispatchMsg(work->field_24, 0x3F2, (s32)&D_shelter_b3_dumping_hole_8018819C[6], 0);
                    work->field_32++;
                    return;
                case 1:
                    if (Gp_DispatchMsg(work->field_24, 0x3F0, 0, 0) == 0) {
                        work->field_34 = 0;
                        work->field_32++;
                    }
                    return;
                case 2:
                    if (++work->field_34 < 6) {
                        return;
                    }
                    {
                        DumpingHoleEntity* w2       = (DumpingHoleEntity*)arg0->work;
                        s32                weaponId = D_80073BA9;
                        msg.anim.field_0            = (void*)((D_8007218A == 1) ? weaponId + 1 : weaponId + 0x22);
                        msg.anim.field_4            = 0x2F;
                        msg.anim.field_8            = 1;
                        msg.anim.field_C            = 0xA;
                        msg.anim.field_10           = 0;
                        Gp_DispatchMsg(w2->field_24, 0x3E8, (s32)&msg, 0);
                    }
                    break;
                default:
                    return;
            }
            break;
        case 2: {
            DumpingHoleEntity* w2       = (DumpingHoleEntity*)arg0->work;
            s32                weaponId = D_80073BA9;
            msg.anim.field_0            = (void*)((D_8007218A == 1) ? weaponId + 1 : weaponId + 0x22);
            msg.anim.field_4            = 0x32;
            msg.anim.field_8            = 0;
            msg.anim.field_C            = 0;
            msg.anim.field_10           = 0;
            Gp_DispatchMsg(w2->field_24, 0x3E8, (s32)&msg, 0);
        } break;
        case 3:
            Gp_DispatchMsg(work->field_24, 0x3F3, 2, 0);
            break;
        case 4:
            Gp_DispatchMsg(work->field_24, 0x3F3, 1, 0);
            Gp_DispatchMsg(work->field_24, 0x3E9, (s32)&D_shelter_b3_dumping_hole_801881CC, 0);
            {
                DumpingHoleEntity* w2       = (DumpingHoleEntity*)arg0->work;
                s32                weaponId = D_80073BA9;
                msg.anim.field_0            = (void*)((D_8007218A == 1) ? weaponId + 1 : weaponId + 0x22);
                msg.anim.field_4            = 9;
                msg.anim.field_8            = 0;
                msg.anim.field_C            = 0;
                msg.anim.field_10           = 0;
                Gp_DispatchMsg(w2->field_24, 0x3E8, (s32)&msg, 0);
            }
            break;
        case 5:
            switch (work->field_32) {
                case 0:
                    msg.loc.stage   = gGameSession->at4.loc.stage;
                    area            = gGameSession->at4.loc.area;
                    msg.loc.field_2 = 1;
                    msg.loc.area    = area;
                    Gp_DispatchMsg(gameGetPtrSlot(4), 0x7DA, (s32)&msg, 0x7DB);
                    work->field_34 = 0;
                    work->field_32++;
                    return;
                case 1:
                    if (++work->field_34 < 0x10) {
                        return;
                    }
                    {
                        DumpingHoleEntity* w2       = (DumpingHoleEntity*)arg0->work;
                        s32                weaponId = D_80073BA9;
                        msg.anim.field_0            = (void*)((D_8007218A == 1) ? weaponId + 1 : weaponId + 0x22);
                        msg.anim.field_4            = 0x30;
                        msg.anim.field_8            = 1;
                        msg.anim.field_C            = 0xA;
                        msg.anim.field_10           = 0;
                        Gp_DispatchMsg(w2->field_24, 0x3E8, (s32)&msg, 0);
                    }
                    break;
                default:
                    return;
            }
            break;
        case 6: {
            DumpingHoleEntity* w2       = (DumpingHoleEntity*)arg0->work;
            s32                weaponId = D_80073BA9;
            msg.anim.field_0            = (void*)((D_8007218A == 1) ? weaponId + 1 : weaponId + 0x22);
            msg.anim.field_4            = 0x33;
            msg.anim.field_8            = 1;
            msg.anim.field_C            = 0xA;
            msg.anim.field_10           = 0;
            Gp_DispatchMsg(w2->field_24, 0x3E8, (s32)&msg, 0);
        }
            Gp_DispatchMsg(work->field_24, 0x3FD, 0x20, 0);
            break;
        case 7: {
            DumpingHoleEntity* w2       = (DumpingHoleEntity*)arg0->work;
            s32                weaponId = D_80073BA9;
            msg.anim.field_0            = (void*)((D_8007218A == 1) ? weaponId + 1 : weaponId + 0x22);
            msg.anim.field_4            = 0x31;
            msg.anim.field_8            = 1;
            msg.anim.field_C            = 0xA;
            msg.anim.field_10           = 0;
            Gp_DispatchMsg(w2->field_24, 0x3E8, (s32)&msg, 0);
        } break;
    }
    work->field_30 = 0;
}

INCLUDE_ASM("rooms/nonmatchings/shelter_b3_dumping_hole/shelter_b3_dumping_hole_3", func_shelter_b3_dumping_hole_8017F1B0);

/// Payload of message 0x3F7: a null-terminated table and the number of
/// entries counted in it.
typedef struct {
    s32* table;
    s32  count;
} DumpingHoleMsg3F7;

typedef struct {
    u8 pad_00[0x4A];
    u8 field_4A;
} DumpingHoleFlags;

extern u8               D_80071075;
extern s8               D_80114C12;
extern s8               D_8007272D[];
extern MATRIX*          D_80073B8C;
extern s32              D_shelter_b3_dumping_hole_801880A0[];
extern s32              D_shelter_b3_dumping_hole_80188640;
extern s32              D_shelter_b3_dumping_hole_80188A78;
extern DumpingHoleFlags D_shelter_b3_dumping_hole_8018EF04;

void func_shelter_b3_dumping_hole_8017EDB8(Task* arg0);
void func_shelter_b3_dumping_hole_8017F1B0(Task* arg0);

void func_shelter_b3_dumping_hole_8017F820(Task* arg0)
{
    DumpingHoleEntity* work;
    DumpingHoleEntity* w;
    Task*              t;
    DumpingHoleEntity* w2;
    DumpingHoleMsg3F7  msg;
    GpAnimArg          anim;
    GpAnimArg*         p;
    s32                n;
    s32                weaponId;

    switch (arg0->state) {
        case 0:
            work       = (DumpingHoleEntity*)Mem_Malloc(0x50, 0);
            arg0->work = work;
            if (work == NULL) {
                taskKill(arg0);
            } else {
                Mem_Set(work, 0, 0x50);
                work->field_24                     = gameGetPtrSlot(3);
                D_shelter_b3_dumping_hole_8018F4A8 = (DumpingHoleState*)arg0;
                work->field_28                     = (Task*)Gp_FindWorkById(gGameSession->at4.loc.area | (gGameSession->at4.loc.stage << 8))->field_0;
                work->field_2C                     = (Task*)Gp_FindWorkById((gGameSession->at4.loc.stage << 8) | (u16)(gGameSession->at4.loc.area | 0x1000))->field_0;
                work->field_42                     = 0;
                work->field_40                     = 0;
                work->field_4A                     = 0;
                work->field_48                     = 0;
                work->field_46                     = 0;
            }
            w             = (DumpingHoleEntity*)arg0->work;
            w->field_0    = ((TmdObject*)w->field_28->extra)->coords->coord.t[0];
            t             = w->field_28;
            w->field_4    = ((TmdObject*)t->extra)->coords->coord.t[1];
            w->field_8    = ((TmdObject*)t->extra)->coords->coord.t[2];
            w->field_12   = 0x400;
            w->field_10   = 0;
            w->field_14   = 0;
            D_8007272D[0] = 0xC;
            D_80062735    = 3;
            arg0->state++;
            break;
        case 1:
            if (gGameSession->eventState != 0) {
                break;
            }
            if (Gp_CapBusy() != 0) {
                break;
            }
            if (D_80114C12 == 1 || D_80071075 != 0 || D_80073B8C->t[0] < 0x36B1) {
                break;
            }
            w2 = (DumpingHoleEntity*)arg0->work;
            n  = 0;
            while (D_shelter_b3_dumping_hole_801880A0[n & 0xFFFF] != 0) {
                n += 1;
            }
            msg.table = &D_shelter_b3_dumping_hole_801880A0[0];
            msg.count = n & 0xFFFF;
            Gp_DispatchMsg(w2->field_24, 0x3F7, (s32)&msg, 0);
            weaponId      = D_80073BA9;
            p             = &anim;
            anim.field_0  = (void*)((D_8007218A == 1) ? weaponId + 1 : weaponId + 0x22);
            p->field_4    = 1;
            p->field_8    = 1;
            p->field_C    = 0xA;
            anim.field_10 = 0;
            Gp_DispatchMsg(gameGetPtrSlot(3), 0x3E8, (s32)&anim, 0);
            arg0->state++;
            break;
        case 2:
            func_800E8634((s32)&D_shelter_b3_dumping_hole_80188640, 0, (s32)&D_shelter_b3_dumping_hole_80188A78);
            arg0->state++;
            break;
        case 3:
            if (gGameSession->eventState == 0) {
                D_shelter_b3_dumping_hole_8018EF04.field_4A &= ~0x40;
                taskKill(arg0);
                return;
            }
            func_shelter_b3_dumping_hole_8017EDB8(arg0);
            func_shelter_b3_dumping_hole_8017F1B0(arg0);
            break;
    }
}

s16 func_shelter_b3_dumping_hole_8017FB70(void)
{
    if (gGameSession->at4.loc.room == 2) {
        return 0;
    }
    return D_shelter_b3_dumping_hole_8018809C;
}

void func_shelter_b3_dumping_hole_8017FBA0(Task* arg0)
{
    DumpingHoleFadeWork* fade;
    DumpingHoleFadeWork* alloc;
    DumpingHoleEntity*   ent;

    ent  = D_shelter_b3_dumping_hole_8018F4A8->field_1C;
    fade = (DumpingHoleFadeWork*)arg0->work;
    if (ent->field_4C == 1) {
        taskKill(arg0);
        return;
    }
    switch (arg0->state) {
        case 0:
            alloc      = (DumpingHoleFadeWork*)Mem_Malloc(8, 0);
            arg0->work = (TaskIdMap*)alloc;
            if (alloc == NULL) {
                taskKill(arg0);
                return;
            }
            fade         = alloc;
            fade->b      = 0xFF;
            fade->g      = 0xFF;
            fade->r      = 0xFF;
            arg0->state += 1;
            /* fallthrough */
        case 1:
            Fade_DrawOverlay((u8)fade->r, (u8)fade->g, (u8)fade->r, 2);
            fade->r -= (u16)arg0->spawnArg1;
            fade->g -= (u16)arg0->spawnArg1;
            fade->b -= (u16)arg0->spawnArg1;
            if (fade->r < 0) {
                taskKill(arg0);
            }
            break;
    }
}

INCLUDE_ASM("rooms/nonmatchings/shelter_b3_dumping_hole/shelter_b3_dumping_hole_3", func_shelter_b3_dumping_hole_8017FCA0);

typedef struct {
    u8  pad_00[0xC];
    s16 field_C;
    s16 field_E;
    s16 field_10;
} DumpingHoleSpawnWork;

typedef struct {
    u16 field_0;
    u16 field_2;
    u16 field_4;
} DumpingHoleSpawnArg;

void func_shelter_b3_dumping_hole_8017FCF4(Task* arg0, DumpingHoleSpawnArg* arg1)
{
    Task*                 task;
    DumpingHoleSpawnWork* work;

    task       = Task_SpawnFromTable(&D_shelter_b3_dumping_hole_80188C04, 1, 0, (s32)arg0);
    work       = (DumpingHoleSpawnWork*)Mem_Malloc(0x24, 0);
    task->work = (TaskIdMap*)work;
    if (work == NULL) {
        taskKill(task);
        return;
    }
    Mem_Set(work, 0, 0x24);
    work->field_C  = arg1->field_0;
    work->field_E  = arg1->field_2;
    work->field_10 = arg1->field_4;
}

void func_shelter_b3_dumping_hole_8017FD9C(s32 arg0, s32 arg1)
{
    if ((arg1 << 0x10) == 0) {
        Task_SpawnFromTable(&D_shelter_b3_dumping_hole_80188C04, 3, 0, arg0);
        Task_SpawnFromTable(&D_shelter_b3_dumping_hole_80188C04, 3, -0xA, arg0);
        Task_SpawnFromTable(&D_shelter_b3_dumping_hole_80188C04, 3, 0xA, arg0);
    }
}

void func_shelter_b3_dumping_hole_8017FE10(s32 arg0)
{
    DumpingHoleEntity* p = D_shelter_b3_dumping_hole_8018F4A8->field_1C;
    if (arg0 == 0) {
        p->field_48 = 1;
    }
}

void func_shelter_b3_dumping_hole_8017FE34(void)
{
    Task_SpawnFromTable(&D_shelter_b3_dumping_hole_80188BC8, 1, 9, 0);
}

void func_shelter_b3_dumping_hole_8017FE64(s32 arg0)
{
    DumpingHoleEntity* p = D_shelter_b3_dumping_hole_8018F4A8->field_1C;
    Gp_DispatchMsg(p->field_28, 0x7D5, arg0, 0);
}

void func_shelter_b3_dumping_hole_8017FE9C(s32 arg0)
{
    DumpingHoleEntity* p = D_shelter_b3_dumping_hole_8018F4A8->field_1C;
    Gp_DispatchMsg(p->field_2C, 0x7D5, arg0, 0);
}

void func_shelter_b3_dumping_hole_8017FED4(s16 arg0)
{
    DumpingHoleEntity* p = D_shelter_b3_dumping_hole_8018F4A8->field_1C;
    p->field_30          = arg0;
    p->field_32          = 0;
}

void func_shelter_b3_dumping_hole_8017FEF4(s16 arg0)
{
    DumpingHoleEntity* p = D_shelter_b3_dumping_hole_8018F4A8->field_1C;
    p->field_38          = arg0;
    p->field_3A          = 0;
}

void func_shelter_b3_dumping_hole_8017FF14(void)
{
    register DumpingHoleState*  st asm("s0")  = D_shelter_b3_dumping_hole_8018F4A8;
    register DumpingHoleEntity* ent asm("s1") = st->field_1C;
    DumpingHoleEntity*          ent2;
    s32                         desc[5];

    Gp_DispatchMsg(ent->field_2C, 0x7D5, 2, 0);
    Gp_DispatchMsg(ent->field_24, 0x3F3, 1, 0);
    Gp_DispatchMsg(ent->field_24, 0x3E9, (s32)&D_shelter_b3_dumping_hole_801881CC, 0);
    ent2    = st->field_1C;
    desc[0] = D_80073BA9 + (D_8007218A == 1 ? 1 : 0x22);
    desc[1] = 9;
    desc[2] = 0;
    desc[3] = 0;
    desc[4] = 0;
    Gp_DispatchMsg(ent2->field_24, 0x3E8, (s32)desc, 0);
    ent->field_40 = 2;
    ent->field_46 = 1;
    ent->field_42 = 1;
    ent->field_4C = 1;
    ent->field_44 = 1;
    CdCmd_CancelReplaceAndActivate();
}

void func_shelter_b3_dumping_hole_8017FFF4(void)
{
    CdCmd_EnqueueReplaceOverlay82();
}

void func_shelter_b3_dumping_hole_80180014(void)
{
    CdCmd_EnqueueOverlay81();
}

void func_shelter_b3_dumping_hole_80180034(void)
{
    Gp_RestoreStreamRng();
    CdCmd_CancelReplaceAndActivate();
}

INCLUDE_ASM("rooms/nonmatchings/shelter_b3_dumping_hole/shelter_b3_dumping_hole_3", func_shelter_b3_dumping_hole_8018005C);

INCLUDE_ASM("rooms/nonmatchings/shelter_b3_dumping_hole/shelter_b3_dumping_hole_3", func_shelter_b3_dumping_hole_8018098C);

typedef struct {
    MATRIX lightMtx;
    MATRIX colorMtx;
    u8     pad_40[0x40];
    Task*  field_80;
    Task*  field_84;
    Task*  field_88;
    u8     pad_8C[0x8];
    s16    field_94;
    s16    field_96;
} DumpingHoleEntity4;

typedef struct {
    u8                  pad_00[0x1C];
    DumpingHoleEntity4* field_1C;
} DumpingHoleState4;

typedef struct {
    u8  field_0;
    u8  field_1;
    s16 field_2;
} DumpingHoleDesc7DA;

extern DumpingHoleState4* D_shelter_b3_dumping_hole_8018F4AC;
extern s16                D_shelter_b3_dumping_hole_8018F4B0;

void func_shelter_b3_dumping_hole_80181430(void)
{
    DumpingHoleEntity4* ent;
    DumpingHoleDesc7DA  desc;
    s32                 desc3[5];
    s32*                p3;

    ent = D_shelter_b3_dumping_hole_8018F4AC->field_1C;
    Gp_SetOverrideVec(NULL);
    if (ent->field_88 != NULL) {
        Task_CallExit(ent->field_88);
        ent->field_88 = NULL;
    }
    ent->field_96 = 1;
    Gp_PulseState1C();

    D_shelter_b3_dumping_hole_8018F4B0 = 0;
    desc.field_0                       = gGameSession->at4.loc.stage;
    desc.field_1                       = gGameSession->at4.loc.area;
    desc.field_2                       = 0x13;
    Gp_DispatchMsg(gameGetPtrSlot(4), 0x7DA, (s32)&desc, 0x7DB);

    Display_ClampField126(0);
    Gp_DispatchMsg(ent->field_84, 0x7D5, 1, 0);
    Gp_DispatchMsg(ent->field_80, 0x3F3, 1, 0);

    p3       = desc3;
    desc3[0] = D_80073BA9 + (D_8007218A == 1 ? 1 : 0x22);
    p3[1]    = 1;
    desc3[2] = 0;
    desc3[3] = 0;
    desc3[4] = 0;
    Gp_DispatchMsg(gameGetPtrSlot(3), 0x3E8, (s32)desc3, 0);
    CdCmd_CancelReplaceAndActivate();
}

extern u8  D_80071075;
extern s8  D_80114C12;
extern s32 D_shelter_b3_dumping_hole_8018F4D8;
extern s32 D_shelter_b3_dumping_hole_8018965C;
extern s32 D_shelter_b3_dumping_hole_8018968C;
extern s32 D_shelter_b3_dumping_hole_801899A4;
void       func_shelter_b3_dumping_hole_8018098C(Task* task);
void       func_shelter_b3_dumping_hole_80183218(s32 arg0);

void func_shelter_b3_dumping_hole_80181560(Task* task)
{
    s32                 desc[5];
    TmdObject*          obj;
    TmdObject*          tail;
    DumpingHoleEntity4* work;

    switch (task->state) {
        case 0:
            if (D_80114C12 == 1 || D_80071075 != 0) {
                return;
            }
            obj        = task->extra;
            task->work = memCalloc(0xA0, false);
            if (task->work == NULL) {
                taskKill(task);
            } else {
                ((TmdObject*)task->extra)->coords->sub = &gGfxViewCoord;
                work                                   = task->work;
                Mem_Set(work, 0, 0xA0);
                work->field_80                     = gameGetPtrSlot(3);
                D_shelter_b3_dumping_hole_8018F4AC = (DumpingHoleState4*)task;
                work->field_84                     = (Task*)Gp_FindWorkById(gGameSession->at4.loc.area | (gGameSession->at4.loc.stage << 8))->field_0;
                obj->lightMtx                      = &work->lightMtx;
                obj->colorMtx                      = &work->colorMtx;
                task->msgTable                     = &D_shelter_b3_dumping_hole_8018965C;
                func_shelter_b3_dumping_hole_80183218(0);
            }
            D_shelter_b3_dumping_hole_8018F4D8          = 0;
            ((DumpingHoleEntity4*)task->work)->field_94 = gGameSession->at4.loc.view;
            Gp_MsgPlayerWeapon(0);
            desc[0] = D_80073BA9 + (D_8007218A == 1 ? 1 : 0x22);
            desc[1] = 9;
            desc[2] = 1;
            desc[3] = 0xA;
            desc[4] = 0;
            Gp_DispatchMsg(gameGetPtrSlot(3), 0x3E8, (s32)desc, 0);
            task->state++;
            break;
        case 1:
            D_shelter_b3_dumping_hole_8018809C = 0;
            func_800E8634((s32)&D_shelter_b3_dumping_hole_8018968C, 0, (s32)&D_shelter_b3_dumping_hole_801899A4);
            task->state++;
            break;
        case 2:
            if (gGameSession->eventState == 0) {
                GameFlag_SetNibble(0x11D, 1);
                taskKill(task);
            } else {
                func_shelter_b3_dumping_hole_8018098C(task);
            }
            break;
    }
    tail    = task->extra;
    desc[0] = tail->coords->workm.t[0];
    desc[1] = ((TmdObject*)task->extra)->coords->workm.t[1];
    desc[2] = ((TmdObject*)task->extra)->coords->workm.t[2];
    func_800D7A9C(tail, (VECTOR*)desc, 0, 3);
}

INCLUDE_RODATA("rooms/nonmatchings/shelter_b3_dumping_hole/shelter_b3_dumping_hole_3", D_shelter_b3_dumping_hole_8017D650);

INCLUDE_RODATA("rooms/nonmatchings/shelter_b3_dumping_hole/shelter_b3_dumping_hole_3", D_shelter_b3_dumping_hole_8017D654);

INCLUDE_RODATA("rooms/nonmatchings/shelter_b3_dumping_hole/shelter_b3_dumping_hole_3", D_shelter_b3_dumping_hole_8017D664);

INCLUDE_RODATA("rooms/nonmatchings/shelter_b3_dumping_hole/shelter_b3_dumping_hole_3", D_shelter_b3_dumping_hole_8017D670);

INCLUDE_RODATA("rooms/nonmatchings/shelter_b3_dumping_hole/shelter_b3_dumping_hole_3", D_shelter_b3_dumping_hole_8017D67C);

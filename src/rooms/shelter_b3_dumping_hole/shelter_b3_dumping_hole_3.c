#include "common.h"
#include "gameplay/1BC.h"
#include "gameplay/3A34.h"
#include "gameplay/3CD8.h"
#include "gameplay/D4.h"
#include "gameplay/gameplay.h"
#include "main/display.h"
#include "main/fs.h"
#include "main/gameflow.h"
#include "main/gfx.h"
#include "main/mem.h"
#include "main/session.h"
#include "main/task.h"
#include "main/tmd.h"
extern TaskDesc D_shelter_b3_dumping_hole_80188C04;
extern TaskDesc D_shelter_b3_dumping_hole_80188BC8;
extern s16      D_shelter_b3_dumping_hole_8018809C;

typedef struct {
    u8    pad_00[0x24];
    Task* field_24;
    Task* field_28;
    Task* field_2C;
    s16   field_30;
    s16   field_32;
    u8    pad_34[0x4];
    s16   field_38;
    s16   field_3A;
    u8    pad_3C[0x4];
    s16   field_40;
    u16   field_42;
    s16   field_44;
    s16   field_46;
    s16   field_48;
    u8    pad_4A[0x2];
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
    DumpingHoleAnimWork* W      = (DumpingHoleAnimWork*)arg0->idMap;
    GsCOORDINATE2*       coord  = ((TmdObject*)arg0->extra)->field_8;
    DumpingHoleEntity*   entity = D_shelter_b3_dumping_hole_8018F4A8->field_1C;

    if (entity->field_42 == 1) {
        Task_Kill((Task*)arg0);
        return;
    }

    switch (arg0->state) {
        case 0:
            coord->sub        = &Gfx_ViewCoord;
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
                    Task_Kill((Task*)arg0);
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
        Task_Kill((Task*)arg0);
        return;
    }
    coord->flg = 0;
}

INCLUDE_ASM("rooms/nonmatchings/shelter_b3_dumping_hole/shelter_b3_dumping_hole_3", func_shelter_b3_dumping_hole_8017DF90);

extern s16 D_shelter_b3_dumping_hole_80188184[];

void func_shelter_b3_dumping_hole_8017E440(Task* arg0)
{
    DumpingHoleAnimWork* work  = (DumpingHoleAnimWork*)arg0->idMap;
    GsCOORDINATE2*       coord = ((TmdObject*)arg0->extra)->field_8;
    SVECTOR              vec;
    s32                  sa1;
    u32                  roll1;
    u32                  roll2;
    s32                  base18;
    s16                  var0;
    s16                  delta;

    if (*(u16*)&D_shelter_b3_dumping_hole_8018F4A8->field_1C->field_48 == 1) {
        Task_Kill(arg0);
        return;
    }

    switch (arg0->state) {
        case 0:
            coord->sub = &Gfx_ViewCoord;
            Gp_ComposeParentWorld((GsCOORDINATE2*)arg0->spawnArg2, &coord->coord, &vec);
            coord->coord.t[0] = vec.vx;
            coord->coord.t[1] = vec.vy;
            coord->coord.t[2] = vec.vz;
            arg0->idMap       = (TaskIdMap*)Mem_Malloc(0x24, 0);
            if (arg0->idMap == NULL) {
                Task_Kill(arg0);
                return;
            }
            work = (DumpingHoleAnimWork*)arg0->idMap;
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
                    Task_Kill(arg0);
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
    u8     pad_40[0x1C];
} DumpingHoleCoordWork;

void func_shelter_b3_dumping_hole_8017E7DC(Task* arg0)
{
    DumpingHoleCoordWork*         work;
    register TmdObject*           extra asm("s2");
    GsCOORDINATE2*                coord;
    register DumpingHoleCoordCfg* cfg asm("s3");
    VECTOR                        v;
    TmdObject*                    e2;

    extra       = (TmdObject*)arg0->extra;
    cfg         = (DumpingHoleCoordCfg*)arg0->spawnArg2;
    coord       = extra->field_8;
    work        = (DumpingHoleCoordWork*)Mem_Malloc(0x5C, 0);
    arg0->idMap = (TaskIdMap*)work;
    if (work == NULL) {
        Task_Kill(arg0);
        return;
    }
    Mem_Set(work, 0, 0x5C);
    coord->sub                         = &Gfx_ViewCoord;
    ((TmdObject*)arg0->extra)->field_C = 0;
    Tmd_AllocBuffers(extra);
    extra->field_1C   = &work->field_0;
    extra->field_20   = &work->field_20;
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
    v.vx = e2->field_8->workm.t[0];
    v.vy = ((TmdObject*)arg0->extra)->field_8->workm.t[1];
    v.vz = ((TmdObject*)arg0->extra)->field_8->workm.t[2];
    func_800D7A9C(e2, &v, 0, 3);
}

INCLUDE_ASM("rooms/nonmatchings/shelter_b3_dumping_hole/shelter_b3_dumping_hole_3", func_shelter_b3_dumping_hole_8017E94C);

INCLUDE_ASM("rooms/nonmatchings/shelter_b3_dumping_hole/shelter_b3_dumping_hole_3", func_shelter_b3_dumping_hole_8017EDB8);

INCLUDE_ASM("rooms/nonmatchings/shelter_b3_dumping_hole/shelter_b3_dumping_hole_3", func_shelter_b3_dumping_hole_8017F1B0);

INCLUDE_ASM("rooms/nonmatchings/shelter_b3_dumping_hole/shelter_b3_dumping_hole_3", func_shelter_b3_dumping_hole_8017F820);

s16 func_shelter_b3_dumping_hole_8017FB70(void)
{
    if (Game_Session->field_5 == 2) {
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
    fade = (DumpingHoleFadeWork*)arg0->idMap;
    if (ent->field_4C == 1) {
        Task_Kill(arg0);
        return;
    }
    switch (arg0->state) {
        case 0:
            alloc       = (DumpingHoleFadeWork*)Mem_Malloc(8, 0);
            arg0->idMap = (TaskIdMap*)alloc;
            if (alloc == NULL) {
                Task_Kill(arg0);
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
                Task_Kill(arg0);
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

    task        = Task_SpawnFromTable(&D_shelter_b3_dumping_hole_80188C04, 1, 0, (s32)arg0);
    work        = (DumpingHoleSpawnWork*)Mem_Malloc(0x24, 0);
    task->idMap = (TaskIdMap*)work;
    if (work == NULL) {
        Task_Kill(task);
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

extern s8 D_8007218A;
extern u8 D_80073BA9;
extern u8 D_shelter_b3_dumping_hole_801881CC;

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
    u8    pad_00[0x80];
    Task* field_80;
    Task* field_84;
    Task* field_88;
    u8    pad_8C[0xA];
    s16   field_96;
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
    desc.field_0                       = Game_Session->field_7;
    desc.field_1                       = Game_Session->field_6;
    desc.field_2                       = 0x13;
    Gp_DispatchMsg((Task*)Game_GetPtrSlot(4), 0x7DA, (s32)&desc, 0x7DB);

    Display_ClampField126(0);
    Gp_DispatchMsg(ent->field_84, 0x7D5, 1, 0);
    Gp_DispatchMsg(ent->field_80, 0x3F3, 1, 0);

    p3       = desc3;
    desc3[0] = D_80073BA9 + (D_8007218A == 1 ? 1 : 0x22);
    p3[1]    = 1;
    desc3[2] = 0;
    desc3[3] = 0;
    desc3[4] = 0;
    Gp_DispatchMsg((Task*)Game_GetPtrSlot(3), 0x3E8, (s32)desc3, 0);
    CdCmd_CancelReplaceAndActivate();
}

INCLUDE_ASM("rooms/nonmatchings/shelter_b3_dumping_hole/shelter_b3_dumping_hole_3", func_shelter_b3_dumping_hole_80181560);

INCLUDE_RODATA("rooms/nonmatchings/shelter_b3_dumping_hole/shelter_b3_dumping_hole_3", D_shelter_b3_dumping_hole_8017D650);

INCLUDE_RODATA("rooms/nonmatchings/shelter_b3_dumping_hole/shelter_b3_dumping_hole_3", D_shelter_b3_dumping_hole_8017D654);

INCLUDE_RODATA("rooms/nonmatchings/shelter_b3_dumping_hole/shelter_b3_dumping_hole_3", D_shelter_b3_dumping_hole_8017D664);

INCLUDE_RODATA("rooms/nonmatchings/shelter_b3_dumping_hole/shelter_b3_dumping_hole_3", D_shelter_b3_dumping_hole_8017D670);

INCLUDE_RODATA("rooms/nonmatchings/shelter_b3_dumping_hole/shelter_b3_dumping_hole_3", D_shelter_b3_dumping_hole_8017D67C);

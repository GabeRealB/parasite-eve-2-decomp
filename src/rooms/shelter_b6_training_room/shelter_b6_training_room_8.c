#include "common.h"
#include "gameplay/3CD8.h"
#include "gameplay/3FB8.h"
#include "main/task.h"
#include "main/tmd.h"

extern s32            D_80070F70;
extern u32            Gp_LcgState;
extern GsCOORDINATE2* D_shelter_b6_training_room_80185C94;
extern SVECTOR        D_shelter_b6_training_room_80184334[];

INCLUDE_ASM("rooms/nonmatchings/shelter_b6_training_room/shelter_b6_training_room_8", func_shelter_b6_training_room_8017F8B8);

INCLUDE_ASM("rooms/nonmatchings/shelter_b6_training_room/shelter_b6_training_room_8", func_shelter_b6_training_room_8017FC40);

INCLUDE_ASM("rooms/nonmatchings/shelter_b6_training_room/shelter_b6_training_room_8", func_shelter_b6_training_room_80180530);

INCLUDE_ASM("rooms/nonmatchings/shelter_b6_training_room/shelter_b6_training_room_8", func_shelter_b6_training_room_80180DB4);

INCLUDE_ASM("rooms/nonmatchings/shelter_b6_training_room/shelter_b6_training_room_8", func_shelter_b6_training_room_801811AC);

INCLUDE_ASM("rooms/nonmatchings/shelter_b6_training_room/shelter_b6_training_room_8", func_shelter_b6_training_room_80181368);

void func_shelter_b6_training_room_80181930(Task* task)
{
    GsCOORDINATE2* coord;
    u8             rgb[3];
    u32            shade;

    coord = ((TmdObject*)task->extra)->coords + 1;
    if (Gp_State1C->eventState == 0) {
        D_shelter_b6_training_room_80185C94 = coord;
        shade                               = ((D_80070F70 & 1) << 4) + 0x40;
        rgb[0]                              = shade;
        rgb[1]                              = shade;
        rgb[2]                              = shade >> 1;
        Gp_DrawRing(coord, 0x200, rgb);
        Gp_DrawRing(coord, 0x400, rgb);
        Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
        if (((Gp_LcgState >> 16) & 3) == 0) {
            Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
            Gp_SpawnEff(0x600E0, ((TmdObject*)task->extra)->coords + (((Gp_LcgState >> 16) & 0xF) + 3), 0x10080, NULL);
        }
    }
}

INCLUDE_ASM("rooms/nonmatchings/shelter_b6_training_room/shelter_b6_training_room_8", func_shelter_b6_training_room_80181A3C);

INCLUDE_ASM("rooms/nonmatchings/shelter_b6_training_room/shelter_b6_training_room_8", func_shelter_b6_training_room_80181BAC);

INCLUDE_ASM("rooms/nonmatchings/shelter_b6_training_room/shelter_b6_training_room_8", func_shelter_b6_training_room_80181FDC);

INCLUDE_ASM("rooms/nonmatchings/shelter_b6_training_room/shelter_b6_training_room_8", func_shelter_b6_training_room_8018245C);

INCLUDE_ASM("rooms/nonmatchings/shelter_b6_training_room/shelter_b6_training_room_8", func_shelter_b6_training_room_801825C0);

void func_shelter_b6_training_room_801826E0(Task* task)
{
    GpEffWork*     mem;
    GsCOORDINATE2* coord;

    mem   = task->spawnArg2;
    coord = ((TmdObject*)task->extra)->coords;
    if (Gp_State1C->eventState == 0) {
        mem->age++;
        if (task->state == 0) {
            mem->move.vy = 0x20;
            mem->scale   = 0x80;
            mem->move.vx = 0;
            mem->move.vz = 0;
            task->state  = 1;
        }
        coord->coord.t[1] += mem->move.vy;
        coord->flg         = 0;
        if (mem->age < 60) {
            if (mem->age & 1) {
                mem->index = (mem->index + 1) & 3;
                func_800EB6E8(coord, mem->index, 0x300, 0x80);
                Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
                if (((Gp_LcgState >> 16) & 3) == 0) {
                    Gp_SpawnEff(0x601AD, coord, 0, NULL);
                }
            }
        } else {
            Gp_ReleaseState1CMem(mem, task);
        }
    }
}

void func_shelter_b6_training_room_80182804(Task* task)
{
    GpEffWork* mem;

    mem = task->spawnArg2;
    if (mem->age >= 0x15) {
        Gp_ReleaseState1CMem(mem, task);
        return;
    }
    if (Gp_State1C->eventState == 0) {
        mem->age++;
        Gp_LcgState  = Gp_LcgState * 5 + 0x71357911;
        mem->scale  += ((Gp_LcgState >> 16) & 0x1FF) + 0x200;
        mem->move.vx = D_shelter_b6_training_room_80184334[24].vx + ((rcos(mem->scale) * 1000) >> 12);
        mem->move.vy = D_shelter_b6_training_room_80184334[24].vy - mem->age * 200;
        mem->move.vz = D_shelter_b6_training_room_80184334[24].vz + ((rsin(mem->scale) * 1000) >> 12);
        Gp_SpawnEff(0x601AE, NULL, 0, &mem->move);
    }
}

void func_shelter_b6_training_room_8018294C(Task* task)
{
    if (Gp_State1C->eventState == 0) {
        Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
        if (((Gp_LcgState >> 16) & 7) == 0) {
            Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
            Gp_SpawnEff(0x601AB, ((TmdObject*)task->extra)->coords + ((u16)((Gp_LcgState >> 16) % 18) + 1), 0, NULL);
        }
    }
}

INCLUDE_ASM("rooms/nonmatchings/shelter_b6_training_room/shelter_b6_training_room_8", func_shelter_b6_training_room_80182A14);

INCLUDE_RODATA("rooms/nonmatchings/shelter_b6_training_room/shelter_b6_training_room_8", D_shelter_b6_training_room_8017D638);

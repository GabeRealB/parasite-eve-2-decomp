#include "common.h"
#include "gameplay/3CD8.h"
#include "gameplay/3FB8.h"
#include "gameplay/D4.h"
#include "main/session.h"
#include "main/task.h"
#include "main/tmd.h"

extern s32            D_80070F70;
extern u32            Gp_LcgState;
extern GsCOORDINATE2* D_shelter_b6_training_room_80185C90;
extern GsCOORDINATE2* D_shelter_b6_training_room_80185C94;
extern u16            D_shelter_b6_training_room_80185C98;
extern SVECTOR        D_shelter_b6_training_room_80184334[];

void func_shelter_b6_training_room_80181BAC(GsCOORDINATE2* arg0, s16 arg1, s16 arg2, s16 arg3);
void func_shelter_b6_training_room_80181FDC(GsCOORDINATE2* arg0, GsCOORDINATE2* arg1, s16 arg2, s16 arg3);
void func_shelter_b6_training_room_80181368(GpEffWork* mem, GsCOORDINATE2* coord, s32 arg2);

void func_shelter_b6_training_room_8017F8B8(Task* task)
{
    GpEffWork*     work;
    GsCOORDINATE2* coord;
    u8             rgb[3];

    work  = task->spawnArg2;
    coord = ((TmdObject*)task->extra)->coords;
    if (Gp_State1C->eventState < 4) {
        work->age++;
        switch (task->state) {
            case 0:
                task->state                         = 1;
                work->scale                         = 0;
                work->angle                         = 0x100;
                D_shelter_b6_training_room_80185C90 = NULL;
                work->step                          = 0x80 / task->spawnArg1;
            case 1:
                if (Gp_State1C->eventState != 0) {
                    rgb[0] = (u16)work->scale >> 1;
                    rgb[1] = (u16)work->scale >> 2;
                    rgb[2] = work->scale;
                    Gp_DrawRing(coord, work->angle, rgb);
                    Gp_DrawRing(coord, (s16)((u16)work->angle * 2), rgb);
                    Gp_DrawArc(coord, (s16)((task->spawnArg1 << 5) + 0x300), 0x100, rgb);
                    break;
                }
                work->scale += work->step;
                work->angle += work->step << 3;
                task->spawnArg1--;
                rgb[0] = (u16)work->scale >> 1;
                rgb[1] = (u16)work->scale >> 2;
                rgb[2] = work->scale;
                Gp_DrawRing(coord, work->angle, rgb);
                Gp_DrawRing(coord, (s16)((u16)work->angle * 2), rgb);
                Gp_DrawArc(coord, (s16)((task->spawnArg1 << 5) + 0x300), 0x100, rgb);
                if (task->spawnArg1 == 0) {
                    work->scale                         = 0xFF;
                    task->state                         = 2;
                    work->period                        = 0x600;
                    work->step                          = 0;
                    D_shelter_b6_training_room_80185C90 = coord;
                }
                break;
            case 2:
                if (Gp_State1C->eventState != 0) {
                    rgb[0] = (u16)work->scale >> 1;
                    rgb[1] = (u16)work->scale >> 2;
                    rgb[2] = work->scale;
                    Gp_DrawRing(coord, work->angle, rgb);
                    Gp_DrawRing(coord, (s16)((u16)work->angle * 2), rgb);
                    Gp_DrawArc(coord, 0x300, 0x100, rgb);
                    break;
                }
                if (work->scale >= 9) {
                    rgb[0] = work->scale >> 1;
                    rgb[1] = (u16)work->scale >> 2;
                    rgb[2] = work->scale;
                    Gp_DrawRing(coord, work->angle, rgb);
                    Gp_DrawRing(coord, (s16)((u16)work->angle * 2), rgb);
                    Gp_DrawArc(coord, 0x300, 0x100, rgb);
                    work->scale -= 8;
                    break;
                }
                task->state = 3;
                break;
            case 3:
                break;
            case 4:
                Gp_ReleaseState1CMem(work, task);
                break;
        }
    } else {
        Gp_ReleaseState1CMem(work, task);
    }
}

INCLUDE_ASM("rooms/nonmatchings/shelter_b6_training_room/shelter_b6_training_room_8", func_shelter_b6_training_room_8017FC40);

INCLUDE_ASM("rooms/nonmatchings/shelter_b6_training_room/shelter_b6_training_room_8", func_shelter_b6_training_room_80180530);

void func_shelter_b6_training_room_80180DB4(Task* task)
{
    GpEffWork*     work;
    GsCOORDINATE2* coord;
    GpMtxWords*    rot;
    u8             rgb[3];

    work  = task->spawnArg2;
    coord = ((TmdObject*)task->extra)->coords;
    if (Gp_State1C->eventState < 4) {
        work->age++;
        switch (task->state) {
            case 0:
                rot         = (GpMtxWords*)&coord->coord;
                rot->w0     = 0x1000;
                rot->w1     = 0;
                rot->w2     = 0x1000;
                rot->w3     = 0;
                rot->h4     = 0x1000;
                coord->flg  = 0;
                work->scale = 0;
                work->angle = 0x100;
                work->step  = 0xC0 / task->spawnArg1;
                if (work->step == 0) {
                    work->step = 1;
                }
                task->state = 1;
            case 1:
                if (Gp_State1C->eventState != 0) {
                    rgb[0] = work->scale;
                    rgb[1] = (u16)work->scale >> 1;
                    rgb[2] = (u16)work->scale >> 2;
                    Gp_DrawRing(coord, work->angle, rgb);
                    Gp_DrawRing(coord, (s16)((u16)work->angle * 2), rgb);
                    Gp_DrawArc(coord, (s16)((task->spawnArg1 % 15) * (work->scale << 2)), 0x100, rgb);
                    return;
                }
                work->scale += work->step;
                if (work->scale > 0xC0) {
                    work->scale = 0xC0;
                }
                work->angle = (u16)work->scale * 8 + 0x100;
                task->spawnArg1--;
                rgb[0] = work->scale;
                rgb[1] = (u16)work->scale >> 1;
                rgb[2] = (u16)work->scale >> 2;
                Gp_DrawRing(coord, work->angle, rgb);
                Gp_DrawRing(coord, (s16)((u16)work->angle * 2), rgb);
                Gp_DrawArc(coord, (s16)((task->spawnArg1 % 15) * (work->scale << 2)), 0x100, rgb);
                if (task->spawnArg1 == 0) {
                    work->scale = 0xFF;
                    task->state = 2;
                    Gp_SpawnEff(0x601AA, coord, 0, NULL);
                    Gp_SpawnEff(0x601AA, coord, 1, NULL);
                    Gp_SpawnEff(0x601AA, coord, 2, NULL);
                    work->period = 0x600;
                    work->step   = 0;
                }
                return;
            case 2:
                if (work->scale >= 5) {
                    rgb[0] = work->scale;
                    rgb[1] = (u16)work->scale >> 1;
                    rgb[2] = (u16)work->scale >> 2;
                    Gp_DrawRing(coord, work->angle, rgb);
                    Gp_DrawRing(coord, (s16)((u16)work->angle * 2), rgb);
                    if (Gp_State1C->eventState == 0) {
                        work->scale -= 4;
                    }
                    Gp_DrawFadeQuad(rgb, 1);
                    return;
                }
                break;
            default:
                return;
        }
    }
    Gp_ReleaseState1CMem(work, task);
}

void func_shelter_b6_training_room_801811AC(Task* task)
{
    GpEffWork*     mem;
    GsCOORDINATE2* coord;

    mem   = task->spawnArg2;
    coord = ((TmdObject*)task->extra)->coords;
    if (Gp_State1C->eventState != 0) {
        func_shelter_b6_training_room_80181368(mem, coord, task->spawnArg1);
        if (Gp_State1C->eventState < 4) {
            return;
        }
        goto release;
    }
    mem->age++;
    switch (task->state) {
        case 0:
            mem->scale        = 0x80;
            task->state       = task->spawnArg1 + 1;
            coord->coord.t[1] = 0;
            coord->flg        = 0;
            Gp_UpdateCoord(coord);
            return;
        case 1:
            if (mem->scale < 5) {
                goto release;
            }
            if (mem->period < 0xC00) {
                mem->period += 0xC0;
            } else {
                mem->scale -= 4;
            }
            mem->angle += 0x20;
            mem->step  += 0x18;
            func_shelter_b6_training_room_80181368(mem, coord, task->spawnArg1);
            return;
        case 2:
            if (mem->scale < 4) {
                goto release;
            }
            mem->scale -= 3;
            mem->angle += 0x40;
            mem->step  += 0x18;
            func_shelter_b6_training_room_80181368(mem, coord, task->spawnArg1);
            return;
        case 3:
            if (mem->scale < 5) {
                goto release;
            }
            mem->scale -= 4;
            mem->angle += 0x180;
            mem->step  += 0x18;
            func_shelter_b6_training_room_80181368(mem, coord, task->spawnArg1);
            return;
        case 4:
        release:
            Gp_ReleaseState1CMem(mem, task);
        default:
            return;
    }
}

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

void func_shelter_b6_training_room_80181A3C(Task* task)
{
    GpEffWork*     mem;
    GsCOORDINATE2* coord;

    mem   = task->spawnArg2;
    coord = ((TmdObject*)task->extra)->coords;
    if (Gp_State1C->eventState == 0) {
        mem->age++;
        coord->flg = 0;
        if (task->state == 0) {
            GpMtxWords* rot;
            u32         first;

            rot               = (GpMtxWords*)&coord->coord;
            coord->sub        = mem->parent;
            rot->w0           = 0x1000;
            rot->w1           = 0;
            rot->w2           = 0x1000;
            rot->w3           = 0;
            rot->h4           = 0x1000;
            coord->coord.t[0] = mem->pos.vx;
            Gp_LcgState       = Gp_LcgState * 5 + 0x71357911;
            first             = Gp_LcgState;
            coord->coord.t[1] = mem->pos.vy;
            Gp_LcgState       = Gp_LcgState * 5 + 0x71357911;
            coord->coord.t[2] = mem->pos.vz;
            coord->flg        = 0;
            mem->scale        = ((first >> 16) & 0x1FF) + 0x100;
            mem->angle        = (Gp_LcgState >> 16) & 0xFFF;
            mem->period       = ((Gp_LcgState >> 16) & 0xF) + 6;
            task->state       = 1;
        }
        func_shelter_b6_training_room_80181BAC(coord, mem->age, mem->scale, mem->angle);
        if (mem->age & 1) {
            func_shelter_b6_training_room_80181FDC(coord, D_shelter_b6_training_room_80185C94, mem->age >> 1, mem->scale);
        }
        if (mem->age > mem->period) {
            Gp_ReleaseState1CMem(mem, task);
        }
    }
}

INCLUDE_ASM("rooms/nonmatchings/shelter_b6_training_room/shelter_b6_training_room_8", func_shelter_b6_training_room_80181BAC);

INCLUDE_ASM("rooms/nonmatchings/shelter_b6_training_room/shelter_b6_training_room_8", func_shelter_b6_training_room_80181FDC);

void func_shelter_b6_training_room_8018245C(Task* task)
{
    GpEffWork*     mem;
    GsCOORDINATE2* coord;
    s16            eventState;
    u8             rgb[3];

    mem        = task->spawnArg2;
    eventState = Gp_State1C->eventState;
    coord      = ((TmdObject*)task->extra)->coords;
    if (eventState != 0) {
        if (eventState < 4) {
            return;
        }
        goto release;
    }
    mem->age++;
    if (task->state == 0) {
        mem->scale = 0xC0;
        mem->angle = 0x200;
        D_shelter_b6_training_room_80185C98++;
        task->state     = 1;
        task->spawnArg1 = D_shelter_b6_training_room_80185C98;
    }
    if (task->spawnArg1 != D_shelter_b6_training_room_80185C98) {
        goto release;
    }
    rgb[0]      = mem->scale;
    rgb[1]      = mem->scale;
    rgb[2]      = (u16)mem->scale >> 1;
    mem->angle += 0x18;
    Gp_DrawArc(coord, (s16)(mem->angle * 2), 0, rgb);
    Gp_DrawRing(coord, (s16)((u16)mem->angle * 4), rgb);
    if (mem->age < 9) {
        return;
    }
    mem->scale -= 0x18;
    if (mem->scale < 0x18) {
    release:
        Gp_ReleaseState1CMem(mem, task);
    }
}

void func_shelter_b6_training_room_801825C0(Task* task)
{
    GpEffWork*     mem;
    GsCOORDINATE2* coord;

    mem   = task->spawnArg2;
    coord = ((TmdObject*)task->extra)->coords;
    if (Gp_State1C->eventState == 0) {
        mem->age++;
        if (task->state == 0) {
            mem->move.vx = 0;
            mem->move.vy = 8;
            mem->move.vz = 0;
            Gp_LcgState  = Gp_LcgState * 5 + 0x71357911;
            mem->scale   = ((Gp_LcgState >> 16) & 0xFFF) | 0x1000;
            task->state  = 1;
        }
        coord->coord.t[1] += mem->move.vy;
        coord->flg         = 0;
        if (!(mem->age & 1)) {
            mem->index++;
        }
        if (mem->index < 8) {
            if (mem->age & 1) {
                Gp_DrawFxQuad(coord, mem->index, 0x400, mem->scale);
            }
        } else {
            Gp_ReleaseState1CMem(mem, task);
        }
    }
}

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

void func_shelter_b6_training_room_80182A14(s32 arg0, s32 arg1)
{
    GpAreaKey* sess = &gGameSession->at4.loc;
    GpSprtRec* rec  = Gp_SprtTables[sess->stage - 1]->field_0[sess->area - 1];
    GpSprtCmd* cmd;
    s32        run = arg0 & 0xFF;
    s32        flag;

    if (run == 0) {
        flag = arg1 & 0xFF;
        if (flag == 0) {
            cmd            = rec[2].field_4;
            cmd[2].field_4 = 1;
            cmd            = rec[6].field_4;
            cmd[1].field_4 = 1;
            return;
        }
        if (flag == 1) {
            cmd            = rec[2].field_4;
            cmd[2].field_4 = 0;
            cmd            = rec[6].field_4;
            cmd[1].field_4 = 0;
            return;
        }
    } else if (run == 1) {
        flag = arg1 & 0xFF;
        if (flag == 0) {
            cmd            = rec[2].field_4;
            cmd[1].field_4 = run;
            cmd            = rec[6].field_4;
            cmd[2].field_4 = run;
            return;
        }
        if (flag == run) {
            cmd            = rec[2].field_4;
            cmd[1].field_4 = 0;
            cmd            = rec[6].field_4;
            cmd[2].field_4 = 0;
        }
    }
}

INCLUDE_RODATA("rooms/nonmatchings/shelter_b6_training_room/shelter_b6_training_room_8", D_shelter_b6_training_room_8017D638);

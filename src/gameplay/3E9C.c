#include "common.h"

#include "gameplay/3A34.h"
#include "gameplay/3CD8.h"
#include "gameplay/3FB8.h"
#include "gameplay/gameplay.h"
#include "main/display.h"
#include "main/gfx.h"
#include "main/mem.h"
#include "main/session.h"
#include "main/task.h"

#include <psyq/inline_c.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>
#include <psyq/libgte.h>
#include <psyq/memory.h>

#define gte_rtv0_real()  __asm__ volatile("nop; nop; .word 0x4A486012")
#define gte_rtps_real()  __asm__ volatile("nop; nop; .word 0x4A180001")
#define gte_rtpt_real()  __asm__ volatile("nop; nop; .word 0x4A280030")
#define gte_gpf12_real() __asm__ volatile("nop; nop; .word 0x4B98003D")

extern s32     Gp_LcgState;
extern SVECTOR D_801124DC[];
extern SVECTOR D_801125EC[];
extern SVECTOR D_801126FC[];
extern SVECTOR D_8011280C[];

void Gp_DrawEffSprite6C();
void Gp_DrawEffSprite3B(GsCOORDINATE2* arg0, u16 arg1, s16 arg2, s16 arg3);
void Gp_DrawEffGroundQuad(VECTOR3* arg0, s32 arg1, s16 arg2);
void Gp_DrawEffSprite7C(GsCOORDINATE2* arg0, s32 arg1, u32 arg2);

void Gp_EffCtlTask2B(Task* arg0)
{
    GpEffWork*     mem;
    GsCOORDINATE2* coord;
    GpCoord64*     base;
    GpCoordTail*   slot;
    s32            temp;
    s32            idx;
    s32            t2;
    s32            rng;
    s32            count;

    mem   = arg0->spawnArg2;
    coord = (GsCOORDINATE2*)((TmdObject*)arg0->extra)->coords;
    base  = Gp_RoomCoords;
    slot  = (GpCoordTail*)&base->coord;
    if (Gp_State1C->eventState < 2) {
        mem->age++;
        switch (arg0->state) {
            case 0:
                temp                   = ((GpEffSpawnArg*)&arg0->spawnArg1)->field_2;
                mem->index             = temp;
                arg0->spawnArg1        = (u8)arg0->spawnArg1;
                slot->coord.coord.t[0] = coord->coord.t[0];
                slot->coord.coord.t[1] = coord->coord.t[1];
                t2                     = coord->coord.t[2];
                base->coord.flg        = 0;
                slot->field_50         = 0xC00;
                slot->field_52         = 0xC00;
                slot->field_54         = 0xC00;
                slot->field_58         = 0xFA0;
                slot->field_5C         = 0x12C0;
                slot->coord.coord.t[2] = t2;
                coord->sub             = mem->parent;
                coord->coord.t[0]      = D_801124DC[arg0->spawnArg1].vx;
                coord->coord.t[1]      = D_801124DC[arg0->spawnArg1].vy;
                coord->coord.t[2]      = D_801124DC[arg0->spawnArg1].vz;
                coord->flg             = 0;
                Gp_UpdateCoord(coord);
                switch (arg0->spawnArg1) {
                    case 1:
                    default:
                        rng         = Gp_LcgState * 5 + 0x71357911;
                        Gp_LcgState = rng;
                        Gp_SpawnEff(0x60034, coord, (((u32)rng >> 16) & 0x1FF) | 0x200, 0);
                        idx         = arg0->spawnArg1;
                        arg0->state = 1;
                        Gp_SpawnEff(0x60036, coord, idx, &D_801125EC[idx]);
                        mem->scale    = 4;
                        base->field_0 = 4;
                        break;
                    case 2:
                    case 3:
                        rng         = Gp_LcgState * 5 + 0x71357911;
                        Gp_LcgState = rng;
                        Gp_SpawnEff(0x60034, coord, (((u32)rng >> 16) & 0x1FF) + 0x300, 0);
                        idx         = arg0->spawnArg1;
                        arg0->state = 1;
                        Gp_SpawnEff(0x60036, coord, idx, &D_801125EC[idx]);
                        mem->scale    = 4;
                        base->field_0 = 4;
                        break;
                    case 30:
                    case 31:
                    case 32:
                        arg0->state = 2;
                        rng         = Gp_LcgState * 5 + 0x71357911;
                        Gp_LcgState = rng;
                        Gp_SpawnEff(0x60034, coord, (((u32)rng >> 16) & 0x1FF) + 0x300, 0);
                        idx = arg0->spawnArg1;
                        Gp_SpawnEff(0x60036, coord, idx, &D_801125EC[idx]);
                        mem->scale    = 2;
                        base->field_0 = 2;
                        break;
                    case 5:
                        idx         = arg0->spawnArg1;
                        arg0->state = 1;
                        Gp_SpawnEff(0x60066, coord, idx, &D_801125EC[idx]);
                        mem->scale    = 4;
                        base->field_0 = 0;
                        break;
                    case 33:
                        mem->index  = 1;
                        arg0->state = 1;
                        rng         = Gp_LcgState * 5 + 0x71357911;
                        Gp_LcgState = rng;
                        Gp_SpawnEff(0x60034, coord, (((u32)rng >> 16) & 0x1FF) + 0x300, 0);
                        idx = arg0->spawnArg1;
                        Gp_SpawnEff(0x60066, coord, idx, &D_801125EC[idx]);
                        mem->scale    = 4;
                        base->field_0 = 0;
                        break;
                }
                if (mem->index == 0) {
                    Gp_State1C->burstRequest = 1;
                }
                break;
            case 1:
                rng         = Gp_LcgState * 5 + 0x71357911;
                Gp_LcgState = rng;
                Gp_SpawnEff(0x60035, coord, (((u32)rng >> 16) & 0x1FF) | 0x200, 0);
                arg0->state++;
                break;
        }
        if (slot->field_58 >= 0x191) {
            slot->field_58 -= 0x190;
        }
        count = mem->age;
        if (mem->scale < count) {
            Gp_ReleaseState1CMem(mem, arg0);
        }
    }
}

void Gp_EffCtlTask6A(Task* arg0)
{
    GpEffWork*     mem;
    GsCOORDINATE2* coord;
    GpCoord64*     base;
    GpCoordTail*   slot;
    GpState1C*     st;
    s32            t2;

    base  = Gp_RoomCoords;
    mem   = arg0->spawnArg2;
    coord = (GsCOORDINATE2*)((TmdObject*)arg0->extra)->coords;
    slot  = (GpCoordTail*)&base->coord;
    if (Gp_State1C->eventState < 2) {
        mem->age++;
        switch (arg0->state) {
            case 0:
                slot->coord.coord.t[0] = coord->coord.t[0];
                slot->coord.coord.t[1] = coord->coord.t[1];
                t2                     = coord->coord.t[2];
                base->coord.flg        = 0;
                slot->field_54         = 0xC00;
                slot->field_52         = 0xC00;
                slot->field_50         = 0xC00;
                slot->field_58         = 0xFA0;
                slot->field_5C         = 0x12C0;
                Gp_RoomCoords->field_0 = 4;
                slot->coord.coord.t[2] = t2;
                coord->sub             = mem->parent;
                coord->coord.t[0]      = D_801124DC[arg0->spawnArg1].vx;
                coord->coord.t[1]      = D_801124DC[arg0->spawnArg1].vy;
                coord->coord.t[2]      = D_801124DC[arg0->spawnArg1].vz;
                coord->flg             = 0;
                Gp_UpdateCoord(coord);
                mem->move.vx = 0;
                mem->move.vy = 0;
                Gp_LcgState  = Gp_LcgState * 5 + 0x71357911;
                mem->scale   = ((u32)Gp_LcgState >> 16) & 0x1FF;
                {
                    s32 sh;
                    sh = mem->scale;
                    SOFT_TOUCH_REG(sh);
                    mem->move.vz = -((s16)sh >> 1);
                }
                Gp_SpawnEff(0x60034, coord, mem->scale + 0x600, &mem->move);
                st               = Gp_State1C;
                arg0->state      = 1;
                st->burstRequest = 1;
                break;
            case 1:
                Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
                Gp_SpawnEff(0x60035, coord, (((u32)Gp_LcgState >> 16) & 0x1FF) + 0x11280,
                            (s32)&mem->move);
                Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
                Gp_SpawnEff(0x60035, coord, (((u32)Gp_LcgState >> 16) & 0x1FF) + 0x21280,
                            (s32)&mem->move);
                Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
                Gp_SpawnEff(0x60035, coord, (((u32)Gp_LcgState >> 16) & 0x1FF) + 0x31280,
                            (s32)&mem->move);
                arg0->state++;
                break;
        }
        if (slot->field_58 >= 0x191) {
            slot->field_58 -= 0x190;
        }
        if (mem->age >= 5) {
            Gp_ReleaseState1CMem(mem, arg0);
        }
    }
}

void Gp_EffCtlTask6B(Task* arg0)
{
    GpEffWork*     mem;
    GsCOORDINATE2* coord;
    GpCoord64*     base;
    GpCoordTail*   slot;
    GpState1C*     st;
    s32            temp;
    s32            idx;
    s32            t2;
    s32            count;

    base  = Gp_RoomCoords;
    slot  = (GpCoordTail*)&base->coord;
    mem   = arg0->spawnArg2;
    coord = (GsCOORDINATE2*)((TmdObject*)arg0->extra)->coords;
    st    = Gp_State1C;
    if (st->eventState < 2) {
        mem->age++;
        if (arg0->state == 0) {
            temp                   = ((GpEffSpawnArg*)&arg0->spawnArg1)->field_2;
            mem->index             = temp;
            arg0->spawnArg1        = (u8)arg0->spawnArg1;
            slot->coord.coord.t[0] = coord->coord.t[0];
            slot->coord.coord.t[1] = coord->coord.t[1];
            t2                     = coord->coord.t[2];
            base->coord.flg        = 0;
            slot->field_50         = 0xC00;
            slot->field_52         = 0xC00;
            slot->field_54         = 0xC00;
            slot->field_58         = 0xFA0;
            slot->field_5C         = 0x12C0;
            slot->coord.coord.t[2] = t2;
            coord->sub             = mem->parent;
            coord->coord.t[0]      = D_801124DC[arg0->spawnArg1].vx;
            coord->coord.t[1]      = D_801124DC[arg0->spawnArg1].vy;
            coord->coord.t[2]      = D_801124DC[arg0->spawnArg1].vz;
            coord->flg             = 0;
            Gp_UpdateCoord(coord);
            mem->move.vx = 0;
            mem->move.vy = 0;
            Gp_LcgState  = Gp_LcgState * 5 + 0x71357911;
            mem->scale   = ((u32)Gp_LcgState >> 16) & 0x1FF;
            {
                s32 sh;
                sh = mem->scale;
                SOFT_TOUCH_REG(sh);
                mem->move.vz = -((s16)sh >> 1);
            }
            Gp_SpawnEff(0x60034, coord, mem->scale + 0x380, &mem->move);
            Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
            Gp_SpawnEff(0x60072, coord, (((u32)Gp_LcgState >> 16) & 0x1FF) + 0x380, 0);
            idx         = arg0->spawnArg1;
            arg0->state = 1;
            Gp_SpawnEff(0x60067, coord, idx, &D_801125EC[idx]);
            if (arg0->spawnArg1 == 0x11) {
                mem->scale             = 1;
                Gp_RoomCoords->field_0 = 1;
            } else {
                mem->scale             = 4;
                Gp_RoomCoords->field_0 = 4;
            }
            if (mem->index == 0) {
                Gp_State1C->burstRequest = 1;
            }
        }
        if (slot->field_58 >= 0x191) {
            slot->field_58 -= 0x190;
        }
        count = mem->age;
        if (mem->scale < count) {
            Gp_ReleaseState1CMem(mem, arg0);
        }
    }
}

void func_800ED42C(Task* arg0)
{
    GpEffWork*     mem;
    GsCOORDINATE2* coord;
    GpCoord64*     base;
    GpCoordTail*   slot;
    SVECTOR*       vec;
    s32            temp;
    s32            t2;
    s32            count;
    s32            i;

    mem   = arg0->spawnArg2;
    coord = (GsCOORDINATE2*)((TmdObject*)arg0->extra)->coords;
    base  = Gp_RoomCoords;
    slot  = (GpCoordTail*)&base->coord;
    if (Gp_State1C->eventState < 2) {
        mem->age++;
        switch (arg0->state) {
            case 0:
                temp                   = ((GpEffSpawnArg*)&arg0->spawnArg1)->field_2;
                mem->index             = temp;
                arg0->spawnArg1        = (u8)arg0->spawnArg1;
                slot->coord.coord.t[0] = coord->coord.t[0];
                slot->coord.coord.t[1] = coord->coord.t[1];
                t2                     = coord->coord.t[2];
                base->coord.flg        = 0;
                slot->field_50         = 0xE00;
                slot->field_52         = 0xA00;
                slot->field_54         = 0xA00;
                slot->field_58         = 0xFA0;
                slot->field_5C         = 0x12C0;
                slot->coord.coord.t[2] = t2;
                coord->sub             = mem->parent;
                coord->coord.t[0]      = D_801124DC[arg0->spawnArg1].vx;
                coord->coord.t[1]      = D_801124DC[arg0->spawnArg1].vy;
                coord->coord.t[2]      = D_801124DC[arg0->spawnArg1].vz;
                coord->flg             = 0;
                Gp_UpdateCoord(coord);
                switch (arg0->spawnArg1) {
                    case 1:
                    default:
                        Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
                        mem->scale  = ((u32)Gp_LcgState >> 16) & 0x1FF;
                        if (mem->index == 0xD) {
                            mem->move.vx = 0;
                            mem->move.vy = 0;
                            mem->move.vz = -((s32)((u16)mem->scale << 16) >> 18);
                            Gp_SpawnEff(0x60034, coord, mem->scale + 0x200, &mem->move);
                            for (i = 0; i < 0xC; i++) {
                                Gp_SpawnEff(0x600A4, coord, 0, 0);
                            }
                            for (i = 0; i < 0xC; i++) {
                                Gp_SpawnEff(0x600A3, coord, 0, 0);
                            }
                            mem->scale = 0x18;
                        } else {
                            mem->move.vx = 0;
                            mem->move.vy = 0;
                            mem->move.vz = -((s32)((u16)mem->scale << 16) >> 17);
                            Gp_SpawnEff(0x60034, coord, mem->scale + 0x380, &mem->move);
                            Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
                            Gp_SpawnEff(0x60072, coord, (((u32)Gp_LcgState >> 16) & 0x1FF) + 0x380, 0);
                            if (mem->index == 0xF) {
                                Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
                                Gp_DrawEffSprite6C(coord, (s16)((u16)mem->scale + 0x280),
                                                   ((u32)Gp_LcgState >> 16) & 0xFFF);
                            }
                            mem->scale = 4;
                        }
                        Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
                        Gp_SpawnEff(0x60035, coord, (((u32)Gp_LcgState >> 16) & 0x1FF) + 0x20300, 0);
                        for (i = 0; i < 4; i++) {
                            Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
                            Gp_SpawnEff(0x6006F, coord, (((u32)Gp_LcgState >> 16) & 0x1FF) | 0x200, 0);
                        }
                        arg0->state = 1;
                        break;
                    case 15:
                        Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
                        mem->scale  = ((u32)Gp_LcgState >> 16) & 0x1FF;
                        if (mem->index == 0xD) {
                            mem->move.vx = 0;
                            mem->move.vy = 0;
                            mem->move.vz = -((s32)((u16)mem->scale << 16) >> 18);
                            Gp_SpawnEff(0x60034, coord, mem->scale + 0x200, &mem->move);
                            for (i = 0; i < 0xC; i++) {
                                Gp_SpawnEff(0x600A4, coord, 0, 0);
                            }
                            for (i = 0; i < 0xC; i++) {
                                Gp_SpawnEff(0x600A3, coord, 0, 0);
                            }
                        } else {
                            mem->move.vx = 0;
                            mem->move.vy = 0;
                            mem->move.vz = -((s32)((u16)mem->scale << 16) >> 17);
                            Gp_SpawnEff(0x60034, coord, mem->scale + 0x380, &mem->move);
                            Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
                            Gp_SpawnEff(0x60072, coord, (((u32)Gp_LcgState >> 16) & 0x1FF) + 0x380, 0);
                            if (mem->index == 0xF) {
                                Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
                                Gp_DrawEffSprite6C(coord, (s16)((u16)mem->scale + 0x280),
                                                   ((u32)Gp_LcgState >> 16) & 0xFFF);
                            }
                        }
                        Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
                        Gp_SpawnEff(0x60035, coord, (((u32)Gp_LcgState >> 16) & 0x1FF) + 0x20300, 0);
                        for (i = 0; i < 4; i++) {
                            Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
                            Gp_SpawnEff(0x6006F, coord, (((u32)Gp_LcgState >> 16) & 0x1FF) | 0x200, 0);
                        }
                        Gp_SpawnEff(0x60068, coord, arg0->spawnArg1, &D_801125EC[arg0->spawnArg1]);
                        arg0->state = 2;
                        mem->scale  = 4;
                        break;
                    case 23:
                        Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
                        mem->scale  = ((u32)Gp_LcgState >> 16) & 0x1FF;
                        if (mem->index == 0xD) {
                            mem->move.vx = 0;
                            mem->move.vy = 0;
                            mem->move.vz = (s32)((u16)mem->scale << 16) >> 18;
                            gte_SetTransMatrix(&GsWSMATRIX);
                            gte_SetRotMatrix(&coord->coord);
                            vec = &mem->move;
                            gte_ldv0(vec);
                            gte_rtv0_real();
                            gte_stsv(vec);
                            Gp_SpawnEff(0x60034, coord, mem->scale + 0x200, vec);
                            i = 0;
                            Gfx_RotMatrixX(&coord->coord, 0x400, i);
                            coord->flg = 0;
                            for (; i < 0xC; i++) {
                                Gp_SpawnEff(0x600A4, coord, 0, 0);
                            }
                            for (i = 0; i < 0xC; i++) {
                                Gp_SpawnEff(0x600A3, coord, 0, 0);
                            }
                        } else {
                            mem->move.vx = 0;
                            mem->move.vy = 0;
                            mem->move.vz = (s32)((u16)mem->scale << 16) >> 17;
                            gte_SetTransMatrix(&GsWSMATRIX);
                            gte_SetRotMatrix(&coord->coord);
                            vec = &mem->move;
                            gte_ldv0(vec);
                            gte_rtv0_real();
                            gte_stsv(vec);
                            Gp_SpawnEff(0x60034, coord, mem->scale + 0x380, vec);
                            Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
                            Gp_SpawnEff(0x60072, coord, (((u32)Gp_LcgState >> 16) & 0x1FF) + 0x380, 0);
                            if (mem->index == 0xF) {
                                Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
                                Gp_DrawEffSprite6C(coord, (s16)((u16)mem->scale + 0x280),
                                                   ((u32)Gp_LcgState >> 16) & 0xFFF);
                            }
                            Gfx_RotMatrixX(&coord->coord, 0x400, 0);
                            coord->flg = 0;
                        }
                        Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
                        Gp_SpawnEff(0x60035, coord, (((u32)Gp_LcgState >> 16) & 0x1FF) + 0x20300, 0);
                        for (i = 0; i < 4; i++) {
                            Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
                            Gp_SpawnEff(0x6006F, coord, (((u32)Gp_LcgState >> 16) & 0x1FF) | 0x200, 0);
                        }
                        Gp_SpawnEff(0x60068, mem->parent, arg0->spawnArg1, &D_801125EC[arg0->spawnArg1]);
                        arg0->state = 2;
                        mem->scale  = 4;
                }
                base->field_0            = 4;
                Gp_State1C->burstRequest = 1;
                break;
            case 1:
                if (mem->age == mem->scale) {
                    Gp_SpawnEff(0x60068, coord, arg0->spawnArg1, &D_801125EC[arg0->spawnArg1]);
                    arg0->state = 2;
                }
                break;
        }
        if (slot->field_58 >= 0x191) {
            slot->field_58 -= 0x190;
        }
        count = mem->age;
        if (mem->scale < count) {
            Gp_ReleaseState1CMem(mem, arg0);
        }
    }
}

void Gp_EffCtlTask6C(Task* arg0)
{
    GpEffWork*     mem;
    GsCOORDINATE2* coord;
    GpCoord64*     base;
    GpCoordTail*   slot;
    s32            temp;
    s32            idx;
    s32            t2;
    s32            rng;
    s32            rng2;
    s32            count;
    s32            i;

    mem   = arg0->spawnArg2;
    coord = (GsCOORDINATE2*)((TmdObject*)arg0->extra)->coords;
    base  = Gp_RoomCoords;
    slot  = (GpCoordTail*)&base->coord;
    if (Gp_State1C->eventState < 2) {
        mem->age++;
        switch (arg0->state) {
            case 0:
                temp                   = ((GpEffSpawnArg*)&arg0->spawnArg1)->field_2;
                mem->index             = temp;
                arg0->spawnArg1        = (u8)arg0->spawnArg1;
                slot->coord.coord.t[0] = coord->coord.t[0];
                slot->coord.coord.t[1] = coord->coord.t[1];
                t2                     = coord->coord.t[2];
                base->coord.flg        = 0;
                slot->field_50         = 0xC00;
                slot->field_52         = 0xC00;
                slot->field_54         = 0xC00;
                slot->field_58         = 0xFA0;
                slot->field_5C         = 0x12C0;
                slot->coord.coord.t[2] = t2;
                coord->sub             = mem->parent;
                coord->coord.t[0]      = D_801126FC[arg0->spawnArg1].vx;
                coord->coord.t[1]      = D_801126FC[arg0->spawnArg1].vy;
                coord->coord.t[2]      = D_801126FC[arg0->spawnArg1].vz;
                coord->flg             = 0;
                Gp_UpdateCoord(coord);
                rng         = Gp_LcgState * 5 + 0x71357911;
                idx         = ((u32)rng >> 16) & 0x1FF;
                rng2        = rng * 5 + 0x71357911;
                Gp_LcgState = rng;
                mem->scale  = idx;
                Gp_LcgState = rng2;
                Gp_DrawEffSprite6C(coord, idx | 0x400, ((u32)rng2 >> 16) & 0xFFF, idx);
                for (i = 0; i < 4; i++) {
                    Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
                    Gp_SpawnEff(0x6006F, coord, (((u32)Gp_LcgState >> 16) & 0x1FF) + 0x380, 0);
                    Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
                    Gp_SpawnEff(0x60035, coord, (((u32)Gp_LcgState >> 16) & 0x1FF) + 0x21380, 0);
                }
                switch (arg0->spawnArg1) {
                    case 1:
                    default:
                        arg0->state = 1;
                        mem->scale  = 0x10;
                        break;
                    case 12:
                        arg0->state = 2;
                        mem->scale  = 4;
                        break;
                    case 27:
                        mem->angle  = 0xA;
                        arg0->state = 1;
                        mem->scale  = 0x18;
                        break;
                }
                base->field_0 = 4;
                if (mem->index == 0) {
                    Gp_State1C->burstRequest = 1;
                }
                break;
            case 1:
                if (mem->age == mem->scale) {
                    Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
                    Gp_SpawnEff(0x60035, coord, (((u32)Gp_LcgState >> 16) & 0xFF) + 0x12180,
                                &D_8011280C[arg0->spawnArg1]);
                    Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
                    Gp_SpawnEff(0x60035, coord, (((u32)Gp_LcgState >> 16) & 0xFF) + 0x22180,
                                &D_8011280C[arg0->spawnArg1]);
                    Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
                    Gp_SpawnEff(0x60035, coord, (((u32)Gp_LcgState >> 16) & 0xFF) + 0x32180,
                                &D_8011280C[arg0->spawnArg1]);
                    Gp_SpawnEff(0x60091, coord, arg0->spawnArg1 + mem->angle,
                                &D_8011280C[arg0->spawnArg1]);
                    arg0->state = 2;
                }
                break;
        }
        if (slot->field_58 >= 0x191) {
            slot->field_58 -= 0x190;
        }
        count = mem->age;
        if (mem->scale < count) {
            Gp_ReleaseState1CMem(mem, arg0);
        }
    }
}

void Gp_EffSprTask34(Task* arg0)
{
    GpEffWork*        mem;
    GsCOORDINATE2*    coord;
    void**            scratch;
    u8*               head;
    GpEffBeamScratch* block;
    GpEffBeamScratch* vecp;
    POLY_FT4*         prim;
    s16               flag;
    s32               rng;
    u16               vz;

    mem   = arg0->spawnArg2;
    flag  = Gp_State1C->eventState;
    coord = (GsCOORDINATE2*)((TmdObject*)arg0->extra)->coords;
    if (flag < 2) {
        Gp_UpdateCoord(coord);
        if (mem->age == 0) {
            rng         = Gp_LcgState * 5 + 0x71357911;
            mem->scale  = ((u32)rng >> 16) & 0xFFF;
            Gp_LcgState = rng;
            mem->angle  = ((GpEffSpawnArg*)&arg0->spawnArg1)->field_0;
        }
        scratch                                    = (void**)G_SCRATCH_HEAD;
        head                                       = *scratch;
        ((GpEffBeamScratch*)(head - 0x1C))->vec.vx = *(u16*)&coord->workm.t[0];
        block                                      = (GpEffBeamScratch*)(head - 0x1C);
        block->vec.vy                              = *(u16*)&coord->workm.t[1];
        vz                                         = *(u16*)&coord->workm.t[2];
        *scratch                                   = block;
        block->vec.vz                              = vz;
        vecp                                       = block;
        gte_SetTransMatrix(&GsWSMATRIX);
        gte_SetRotMatrix(&GsWSMATRIX);
        gte_ldv0(&vecp->vec);
        gte_rtps_real();
        gte_stsxy(&((GpEffBeamScratch*)(head - 0x1C))->sxy);
        gte_stflg(&((GpEffBeamScratch*)(head - 0x1C))->flag);
        if (block->flag >= 0) {
            gte_stszotz(&((GpEffBeamScratch*)(head - 0x1C))->otz);
            block->otz     = block->otz + 1;
            prim           = (POLY_FT4*)gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setlen(prim, 9);
            setcode(prim, 0x2F);
            prim->tpage = 0x2A;
            prim->clut  = 0x4340;
            prim->u0    = mem->age * 40;
            prim->v0    = 0xD8;
            prim->u1    = mem->age * 40 + 0x27;
            prim->v1    = 0xD8;
            prim->u2    = mem->age * 40;
            prim->v2    = 0xFF;
            prim->u3    = mem->age * 40 + 0x27;
            prim->v3    = 0xFF;
            block->dx   = (((mem->angle * 23) / block->otz) * rsin(mem->scale)) >> 12;
            block->dy   = (((mem->angle * 23) / block->otz) * rcos(mem->scale)) >> 12;
            prim->x0    = *(u16*)&block->sxy.vx + *(u16*)&block->dx;
            prim->x3    = *(u16*)&block->sxy.vx - *(u16*)&block->dx;
            prim->y0    = *(u16*)&block->sxy.vy - *(u16*)&block->dy;
            prim->y3    = *(u16*)&block->sxy.vy + *(u16*)&block->dy;
            block->dx   = (((mem->angle * 23) / block->otz) * rsin(mem->scale + 0x400)) >> 12;
            block->dy   = (((mem->angle * 23) / block->otz) * rcos(mem->scale + 0x400)) >> 12;
            prim->x1    = *(u16*)&block->sxy.vx + *(u16*)&block->dx;
            prim->x2    = *(u16*)&block->sxy.vx - *(u16*)&block->dx;
            prim->y1    = *(u16*)&block->sxy.vy - *(u16*)&block->dy;
            prim->y2    = *(u16*)&block->sxy.vy + *(u16*)&block->dy;
            addPrim((u_long*)(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC) + (s32)gGpuCurrentOt),
                    prim);
        }
        *scratch = (u8*)*scratch + 0x1C;
        if (Gp_State1C->eventState != 0) {
            return;
        }
        mem->age++;
        if (mem->age < 2) {
            return;
        }
    } else if (flag < 4) {
        return;
    }
    Gp_ReleaseState1CMem(mem, arg0);
}

void Gp_EffSprTask72(Task* arg0)
{
    GpEffWork*        mem;
    GsCOORDINATE2*    coord;
    void**            scratch;
    u8*               head;
    GpEffBeamScratch* block;
    GpEffBeamScratch* vecp;
    POLY_FT4*         prim;
    s16               flag;
    s32               rng;
    u16               vz;

    mem   = arg0->spawnArg2;
    flag  = Gp_State1C->eventState;
    coord = (GsCOORDINATE2*)((TmdObject*)arg0->extra)->coords;
    if (flag < 2) {
        Gp_UpdateCoord(coord);
        if (arg0->state == 0) {
            rng         = Gp_LcgState * 5 + 0x71357911;
            mem->scale  = (((u32)rng >> 16) & 0x800) - 0x200;
            Gp_LcgState = rng;
            mem->angle  = ((GpEffSpawnArg*)&arg0->spawnArg1)->field_0;
            arg0->state = 1;
        }
        scratch                                    = (void**)G_SCRATCH_HEAD;
        head                                       = *scratch;
        ((GpEffBeamScratch*)(head - 0x1C))->vec.vx = *(u16*)&coord->workm.t[0];
        block                                      = (GpEffBeamScratch*)(head - 0x1C);
        block->vec.vy                              = *(u16*)&coord->workm.t[1];
        vz                                         = *(u16*)&coord->workm.t[2];
        *scratch                                   = block;
        block->vec.vz                              = vz;
        vecp                                       = block;
        gte_SetTransMatrix(&GsWSMATRIX);
        gte_SetRotMatrix(&GsWSMATRIX);
        gte_ldv0(&vecp->vec);
        gte_rtps_real();
        gte_stsxy(&((GpEffBeamScratch*)(head - 0x1C))->sxy);
        gte_stflg(&((GpEffBeamScratch*)(head - 0x1C))->flag);
        if (block->flag >= 0) {
            gte_stszotz(&((GpEffBeamScratch*)(head - 0x1C))->otz);
            block->otz     = block->otz + 1;
            prim           = (POLY_FT4*)gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setlen(prim, 9);
            setcode(prim, 0x2F);
            prim->tpage = 0x28;
            prim->clut  = 0x4288;
            prim->u0    = mem->age * 32;
            prim->v0    = 0x38;
            prim->u1    = mem->age * 32 + 0x1F;
            prim->v1    = 0x38;
            prim->u2    = mem->age * 32;
            prim->v2    = 0x57;
            prim->u3    = mem->age * 32 + 0x1F;
            prim->v3    = 0x57;
            block->dx   = (((mem->angle * 31) / block->otz) * rsin(mem->scale)) >> 12;
            block->dy   = (((mem->angle * 31) / block->otz) * rcos(mem->scale)) >> 12;
            prim->x0    = *(u16*)&block->sxy.vx + *(u16*)&block->dx;
            prim->x3    = *(u16*)&block->sxy.vx - *(u16*)&block->dx;
            prim->y0    = *(u16*)&block->sxy.vy - *(u16*)&block->dy;
            prim->y3    = *(u16*)&block->sxy.vy + *(u16*)&block->dy;
            block->dx   = (((mem->angle * 31) / block->otz) * rsin(mem->scale + 0x400)) >> 12;
            block->dy   = (((mem->angle * 31) / block->otz) * rcos(mem->scale + 0x400)) >> 12;
            prim->x1    = *(u16*)&block->sxy.vx + *(u16*)&block->dx;
            prim->x2    = *(u16*)&block->sxy.vx - *(u16*)&block->dx;
            prim->y1    = *(u16*)&block->sxy.vy - *(u16*)&block->dy;
            prim->y2    = *(u16*)&block->sxy.vy + *(u16*)&block->dy;
            addPrim((u_long*)(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC) + (s32)gGpuCurrentOt),
                    prim);
        }
        *scratch = (u8*)*scratch + 0x1C;
        if (Gp_State1C->eventState != 0) {
            return;
        }
        mem->age++;
        if (mem->age < 2) {
            return;
        }
    } else if (flag < 4) {
        return;
    }
    Gp_ReleaseState1CMem(mem, arg0);
}

void Gp_EffLineTaskA3(Task* arg0)
{
    GpEffWork*        mem;
    GsCOORDINATE2*    coord;
    void**            scratch;
    u8*               head;
    GpEffLineScratch* block;
    GpEffLineScratch* vecp;
    LINE_G2*          prim;
    s16               flag;
    s16               c;
    u16               vz;

    mem   = arg0->spawnArg2;
    flag  = Gp_State1C->eventState;
    coord = (GsCOORDINATE2*)((TmdObject*)arg0->extra)->coords;
    if (flag < 2) {
        Gp_UpdateCoord(coord);
        if (arg0->state == 0) {
            Gp_LcgState  = Gp_LcgState * 5 + 0x71357911;
            mem->scale   = ((u32)Gp_LcgState >> 16) % 3 + 1;
            Gp_LcgState  = Gp_LcgState * 5 + 0x71357911;
            mem->move.vx = 0x80 - (((u32)Gp_LcgState >> 16) & 0xFF);
            Gp_LcgState  = Gp_LcgState * 5 + 0x71357911;
            mem->move.vy = (((u32)Gp_LcgState >> 16) & 0x1FF) + 0x200;
            Gp_LcgState  = Gp_LcgState * 5 + 0x71357911;
            mem->move.vz = 0x80 - (((u32)Gp_LcgState >> 16) & 0xFF);
            arg0->state  = 1;
        }
        scratch                                     = (void**)G_SCRATCH_HEAD;
        head                                        = *scratch;
        ((GpEffLineScratch*)(head - 0x20))->vec0.vx = *(u16*)&coord->workm.t[0];
        block                                       = (GpEffLineScratch*)(head - 0x20);
        block->vec0.vy                              = *(u16*)&coord->workm.t[1];
        vz                                          = *(u16*)&coord->workm.t[2];
        *scratch                                    = block;
        block->vec0.vz                              = vz;
        vecp                                        = block;
        gte_SetRotMatrix(&mem->parent->coord);
        gte_ldv0(&mem->move);
        gte_rtv0_real();
        gte_stsv(&((GpEffLineScratch*)(head - 0x20))->vec1);
        gte_SetRotMatrix(&Gfx_ViewWorldMtx);
        gte_ldv0(&((GpEffLineScratch*)(head - 0x20))->vec1);
        gte_rtv0_real();
        gte_stsv(&((GpEffLineScratch*)(head - 0x20))->vec1);
        gte_lddp((mem->age << 11) + 0x1000);
        gte_ldsv(&((GpEffLineScratch*)(head - 0x20))->vec1);
        gte_gpf12_real();
        gte_stsv(&((GpEffLineScratch*)(head - 0x20))->vec1);
        *(u16*)&block->vec1.vx = *(u16*)&block->vec1.vx + *(u16*)&((GpEffLineScratch*)(head - 0x20))->vec0.vx;
        *(u16*)&block->vec1.vy = *(u16*)&block->vec1.vy + *(u16*)&block->vec0.vy;
        *(u16*)&block->vec1.vz = *(u16*)&block->vec1.vz + *(u16*)&block->vec0.vz;
        gte_SetTransMatrix(&GsWSMATRIX);
        gte_SetRotMatrix(&GsWSMATRIX);
        gte_ldv0(&vecp->vec0);
        gte_rtps_real();
        gte_stsxy(&((GpEffLineScratch*)(head - 0x20))->sxy0);
        gte_stflg(&((GpEffLineScratch*)(head - 0x20))->flag);
        if (block->flag >= 0) {
            gte_ldv0(&((GpEffLineScratch*)(head - 0x20))->vec1);
            gte_rtps_real();
            gte_stsxy(&((GpEffLineScratch*)(head - 0x20))->sxy1);
            gte_stflg(&((GpEffLineScratch*)(head - 0x20))->flag);
            if (block->flag >= 0) {
                gte_stszotz(&((GpEffLineScratch*)(head - 0x20))->otz);
                block->otz     = block->otz + 1;
                prim           = (LINE_G2*)gGpuPrimCursor;
                gGpuPrimCursor = prim + 1;
                setlen(prim, 4);
                setcode(prim, 0x50);
                c        = 0xFF - ((u16)mem->age << 6);
                prim->r0 = 0;
                prim->g0 = 0;
                prim->b0 = 0;
                prim->r1 = c;
                prim->g1 = c >> mem->scale;
                prim->b1 = c >> 3;
                prim->x0 = *(u16*)&block->sxy0.vx;
                prim->y0 = *(u16*)&block->sxy0.vy;
                prim->x1 = *(u16*)&block->sxy1.vx;
                prim->y1 = *(u16*)&block->sxy1.vy;
                addPrim((u_long*)(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC) + (s32)gGpuCurrentOt),
                        prim);
                Gp_AddTpageShift((P_TAG*)prim, 1, block->otz);
            }
        }
        *(void**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD + 0x20;
        if (Gp_State1C->eventState != 0) {
            return;
        }
        mem->age++;
        if (mem->age < 4) {
            return;
        }
    } else if (flag < 4) {
        return;
    }
    Gp_ReleaseState1CMem(mem, arg0);
}

void Gp_DrawEffSprite6C(GsCOORDINATE2* arg0, s32 arg1, s32 arg2, s32 arg3)
{
    void**            scratch;
    u8*               head;
    GpEffBeamScratch* block;
    GpEffBeamScratch* vecp;
    register s32      u70 asm("a1");
    POLY_FT4*         prim;
    s32               ang;
    u16               vz;

    scratch                                    = (void**)G_SCRATCH_HEAD;
    head                                       = *scratch;
    ((GpEffBeamScratch*)(head - 0x1C))->vec.vx = *(u16*)&arg0->workm.t[0];
    block                                      = (GpEffBeamScratch*)(head - 0x1C);
    block->vec.vy                              = *(u16*)&arg0->workm.t[1];
    vz                                         = *(u16*)&arg0->workm.t[2];
    *scratch                                   = block;
    block->vec.vz                              = vz;
    vecp                                       = block;
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&vecp->vec);
    gte_rtps_real();
    gte_stsxy(&((GpEffBeamScratch*)(head - 0x1C))->sxy);
    gte_stflg(&((GpEffBeamScratch*)(head - 0x1C))->flag);
    if (block->flag >= 0) {
        gte_stszotz(&((GpEffBeamScratch*)(head - 0x1C))->otz);
        prim           = (POLY_FT4*)gGpuPrimCursor;
        gGpuPrimCursor = prim + 1;
        setlen(prim, 9);
        setcode(prim, 0x2F);
        prim->tpage = 0x29;
        prim->clut  = 0x428B;
        prim->v0    = 0xC8;
        prim->v1    = 0xC8;
        u70         = 0x70;
        prim->u0    = u70;
        prim->u1    = 0xA7;
        prim->u2    = u70;
        prim->v2    = 0xFF;
        prim->u3    = 0xA7;
        prim->v3    = 0xFF;
        block->dx   = ((((s16)arg1 * 55) / block->otz) * rsin((s16)arg2)) >> 12;
        block->dy   = ((((s16)arg1 * 55) / block->otz) * rcos((s16)arg2)) >> 12;
        prim->x0    = *(u16*)&block->sxy.vx + *(u16*)&block->dx;
        prim->x3    = *(u16*)&block->sxy.vx - *(u16*)&block->dx;
        prim->y0    = *(u16*)&block->sxy.vy - *(u16*)&block->dy;
        prim->y3    = *(u16*)&block->sxy.vy + *(u16*)&block->dy;
        ang         = (s16)arg2 + 0x400;
        block->dx   = ((((s16)arg1 * 55) / block->otz) * rsin(ang)) >> 12;
        block->dy   = ((((s16)arg1 * 55) / block->otz) * rcos(ang)) >> 12;
        prim->x1    = *(u16*)&block->sxy.vx + *(u16*)&block->dx;
        prim->x2    = *(u16*)&block->sxy.vx - *(u16*)&block->dx;
        prim->y1    = *(u16*)&block->sxy.vy - *(u16*)&block->dy;
        prim->y2    = *(u16*)&block->sxy.vy + *(u16*)&block->dy;
        addPrim((u_long*)(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC) + (s32)gGpuCurrentOt),
                prim);
    }
    *scratch = (u8*)*scratch + 0x1C;
}

void Gp_EffSprTask35(Task* arg0)
{
    GpEffWork*        mem;
    GsCOORDINATE2*    coord;
    void**            scratch;
    u8*               head;
    GpEffBeamScratch* block;
    GpEffBeamScratch* vecp;
    POLY_FT4*         prim;
    s16               flag;
    s16               val;
    s32               rng;
    s32               t2;
    s32               quot;
    u16               vz;

    mem   = arg0->spawnArg2;
    flag  = Gp_State1C->eventState;
    coord = (GsCOORDINATE2*)((TmdObject*)arg0->extra)->coords;
    if (flag >= 2) {
        if (flag < 4) {
            return;
        }
    } else {
        if (arg0->state == 0) {
            rng         = Gp_LcgState * 5 + 0x71357911;
            mem->scale  = ((u32)rng >> 16) & 0xFFF;
            Gp_LcgState = rng;
            mem->angle  = ((GpEffSpawnArg*)&arg0->spawnArg1)->field_0 & 0xFFF;
            if (arg0->spawnArg1 & 0xF0000) {
                val = (arg0->spawnArg1 >> 16) & 0xF;
            } else {
                val = 1;
            }
            mem->period = val;
            if (arg0->spawnArg1 & 0x1000) {
                Gp_LcgState  = Gp_LcgState * 5 + 0x71357911;
                mem->move.vx = 0x10 - (((u32)Gp_LcgState >> 16) & 0x1F);
                Gp_LcgState  = Gp_LcgState * 5 + 0x71357911;
                mem->move.vy = 0x10 - (((u32)Gp_LcgState >> 16) & 0x1F);
                Gp_LcgState  = Gp_LcgState * 5 + 0x71357911;
                mem->move.vz = 0x10 - (((u32)Gp_LcgState >> 16) & 0x1F);
            }
            arg0->state = 1;
        }
        Gp_UpdateCoord(coord);
        scratch                                    = (void**)G_SCRATCH_HEAD;
        head                                       = *scratch;
        ((GpEffBeamScratch*)(head - 0x1C))->vec.vx = *(u16*)&coord->workm.t[0];
        block                                      = (GpEffBeamScratch*)(head - 0x1C);
        block->vec.vy                              = *(u16*)&coord->workm.t[1];
        vz                                         = *(u16*)&coord->workm.t[2];
        *scratch                                   = block;
        block->vec.vz                              = vz;
        vecp                                       = block;
        gte_SetTransMatrix(&GsWSMATRIX);
        gte_SetRotMatrix(&GsWSMATRIX);
        gte_ldv0(&vecp->vec);
        gte_rtps_real();
        gte_stsxy(&((GpEffBeamScratch*)(head - 0x1C))->sxy);
        gte_stflg(&((GpEffBeamScratch*)(head - 0x1C))->flag);
        if (block->flag >= 0) {
            gte_stszotz(&((GpEffBeamScratch*)(head - 0x1C))->otz);
            prim           = (POLY_FT4*)gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setlen(prim, 9);
            setcode(prim, 0x2F);
            prim->tpage = 0x28;
            prim->clut  = 0x4242;
            quot        = mem->age / mem->period;
            prim->v0    = 0;
            prim->u0    = quot * 24 + 0x30;
            quot        = mem->age / mem->period;
            prim->v1    = 0;
            prim->u1    = quot * 24 + 0x47;
            quot        = mem->age / mem->period;
            prim->v2    = 0x17;
            prim->u2    = quot * 24 + 0x30;
            quot        = mem->age / mem->period;
            prim->v3    = 0x17;
            prim->u3    = quot * 24 + 0x47;
            block->dx   = (((mem->angle * 23) / block->otz) * rsin(mem->scale)) >> 12;
            block->dy   = (((mem->angle * 23) / block->otz) * rcos(mem->scale)) >> 12;
            prim->x0    = *(u16*)&block->sxy.vx + *(u16*)&block->dx;
            prim->x3    = *(u16*)&block->sxy.vx - *(u16*)&block->dx;
            prim->y0    = *(u16*)&block->sxy.vy - *(u16*)&block->dy;
            prim->y3    = *(u16*)&block->sxy.vy + *(u16*)&block->dy;
            block->dx   = (((mem->angle * 23) / block->otz) * rsin(mem->scale + 0x400)) >> 12;
            block->dy   = (((mem->angle * 23) / block->otz) * rcos(mem->scale + 0x400)) >> 12;
            prim->x1    = *(u16*)&block->sxy.vx + *(u16*)&block->dx;
            prim->x2    = *(u16*)&block->sxy.vx - *(u16*)&block->dx;
            prim->y1    = *(u16*)&block->sxy.vy - *(u16*)&block->dy;
            prim->y2    = *(u16*)&block->sxy.vy + *(u16*)&block->dy;
            addPrim((u_long*)(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC) + (s32)gGpuCurrentOt),
                    prim);
        }
        *scratch = (u8*)*scratch + 0x1C;
        if (Gp_State1C->eventState != 0) {
            return;
        }
        coord->coord.t[0] += mem->move.vx;
        coord->coord.t[1] += mem->move.vy;
        t2                 = coord->coord.t[2] + mem->move.vz;
        coord->flg         = 0;
        coord->coord.t[2]  = t2;
        mem->age++;
        if (mem->age <= mem->period * 4 - 1) {
            return;
        }
    }
    Gp_ReleaseState1CMem(mem, arg0);
}

void Gp_EffSprTask6F(Task* arg0)
{
    GpEffWork*        mem;
    GsCOORDINATE2*    coord;
    void**            scratch;
    u8*               head;
    GpEffBeamScratch* block;
    GpEffBeamScratch* vecp;
    POLY_FT4*         prim;
    s16               flag;
    s32               t2;
    s32               quot;
    u16               vz;

    mem   = arg0->spawnArg2;
    flag  = Gp_State1C->eventState;
    coord = (GsCOORDINATE2*)((TmdObject*)arg0->extra)->coords;
    if (flag >= 2) {
        if (flag < 4) {
            return;
        }
    } else {
        Gp_UpdateCoord(coord);
        if (arg0->state == 0) {
            Gp_LcgState  = Gp_LcgState * 5 + 0x71357911;
            mem->scale   = ((u32)Gp_LcgState >> 16) & 0xFFF;
            mem->angle   = ((GpEffSpawnArg*)&arg0->spawnArg1)->field_0 & 0xFFF;
            Gp_LcgState  = Gp_LcgState * 5 + 0x71357911;
            mem->period  = (((u32)Gp_LcgState >> 16) & 1) + 1;
            Gp_LcgState  = Gp_LcgState * 5 + 0x71357911;
            mem->move.vx = 0x10 - (((u32)Gp_LcgState >> 16) & 0x1F);
            Gp_LcgState  = Gp_LcgState * 5 + 0x71357911;
            mem->move.vy = ((u32)Gp_LcgState >> 14) & 0x7C;
            Gp_LcgState  = Gp_LcgState * 5 + 0x71357911;
            mem->move.vz = 0x10 - (((u32)Gp_LcgState >> 16) & 0x1F);
            gte_SetRotMatrix(&mem->parent->coord);
            gte_ldv0(&mem->move);
            gte_rtv0_real();
            gte_stsv(&mem->move);
            arg0->state = 1;
        }
        scratch                                    = (void**)G_SCRATCH_HEAD;
        head                                       = *scratch;
        ((GpEffBeamScratch*)(head - 0x1C))->vec.vx = *(u16*)&coord->workm.t[0];
        block                                      = (GpEffBeamScratch*)(head - 0x1C);
        block->vec.vy                              = *(u16*)&coord->workm.t[1];
        vz                                         = *(u16*)&coord->workm.t[2];
        *scratch                                   = block;
        block->vec.vz                              = vz;
        vecp                                       = block;
        gte_SetTransMatrix(&GsWSMATRIX);
        gte_SetRotMatrix(&GsWSMATRIX);
        gte_ldv0(&vecp->vec);
        gte_rtps_real();
        gte_stsxy(&((GpEffBeamScratch*)(head - 0x1C))->sxy);
        gte_stflg(&((GpEffBeamScratch*)(head - 0x1C))->flag);
        if (block->flag >= 0) {
            gte_stszotz(&((GpEffBeamScratch*)(head - 0x1C))->otz);
            prim           = (POLY_FT4*)gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setlen(prim, 9);
            setcode(prim, 0x2F);
            prim->tpage = 0x28;
            prim->clut  = 0x4253;
            quot        = mem->age / mem->period;
            prim->v0    = 0x18;
            prim->u0    = quot * 32;
            quot        = mem->age / mem->period;
            prim->v1    = 0x18;
            prim->u1    = quot * 32 + 0x1F;
            quot        = mem->age / mem->period;
            prim->v2    = 0x37;
            prim->u2    = quot * 32;
            quot        = mem->age / mem->period;
            prim->v3    = 0x37;
            prim->u3    = quot * 32 + 0x1F;
            block->dx   = (((mem->angle * 31) / block->otz) * rsin(mem->scale)) >> 12;
            block->dy   = (((mem->angle * 31) / block->otz) * rcos(mem->scale)) >> 12;
            prim->x0    = *(u16*)&block->sxy.vx + *(u16*)&block->dx;
            prim->x3    = *(u16*)&block->sxy.vx - *(u16*)&block->dx;
            prim->y0    = *(u16*)&block->sxy.vy - *(u16*)&block->dy;
            prim->y3    = *(u16*)&block->sxy.vy + *(u16*)&block->dy;
            block->dx   = (((mem->angle * 31) / block->otz) * rsin(mem->scale + 0x400)) >> 12;
            block->dy   = (((mem->angle * 31) / block->otz) * rcos(mem->scale + 0x400)) >> 12;
            prim->x1    = *(u16*)&block->sxy.vx + *(u16*)&block->dx;
            prim->x2    = *(u16*)&block->sxy.vx - *(u16*)&block->dx;
            prim->y1    = *(u16*)&block->sxy.vy - *(u16*)&block->dy;
            prim->y2    = *(u16*)&block->sxy.vy + *(u16*)&block->dy;
            addPrim((u_long*)(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC) + (s32)gGpuCurrentOt),
                    prim);
        }
        *scratch = (u8*)*scratch + 0x1C;
        if (Gp_State1C->eventState != 0) {
            return;
        }
        coord->coord.t[0] += mem->move.vx;
        coord->coord.t[1] += mem->move.vy;
        t2                 = coord->coord.t[2] + mem->move.vz;
        coord->flg         = 0;
        coord->coord.t[2]  = t2;
        mem->age++;
        if (mem->age <= mem->period * 8 - 1) {
            return;
        }
    }
    Gp_ReleaseState1CMem(mem, arg0);
}

void Gp_EffModelTask(Task* arg0)
{
    SVECTOR        delta;
    SVECTOR        dir;
    SVECTOR        pos;
    VECTOR         vec;
    VECTOR         tmp;
    TmdObject*     extra;
    GpEffWork*     mem;
    GsCOORDINATE2* coord;
    SVECTOR*       vel;
    s16            flag;
    s32            t2;
    s32            scale;
    u16            tx;
    u16            ty;
    u16            tz;
    s32            dz;

    extra = (TmdObject*)arg0->extra;
    mem   = arg0->spawnArg2;
    flag  = Gp_State1C->eventState;
    coord = (GsCOORDINATE2*)extra->coords;
    if (flag != 0) {
        if (flag < 4) {
            return;
        }
        goto release;
    }
    Gp_UpdateCoord(coord);
    if (arg0->state == 0) {
        extra->flags &= 0xFF7F;
        switch (arg0->spawnArg1) {
            case 1:
            default:
                mem->scale   = 0xD4;
                mem->angle   = 0x14;
                Gp_LcgState  = Gp_LcgState * 5 + 0x71357911;
                mem->move.vx = (((u32)Gp_LcgState >> 16) & 0x3F) + 0x60;
                Gp_LcgState  = Gp_LcgState * 5 + 0x71357911;
                mem->move.vy = 0x10 - (((u32)Gp_LcgState >> 16) & 0x7F);
                Gp_LcgState  = Gp_LcgState * 5 + 0x71357911;
                mem->move.vz = ((u32)Gp_LcgState >> 16) & 0x7F;
                gte_SetRotMatrix(&mem->parent->coord);
                gte_ldv0(&mem->move);
                gte_rtv0_real();
                gte_stsv(&mem->move);
                break;
            case 2:
                mem->scale   = 0x100;
                mem->angle   = 0x14;
                Gp_LcgState  = Gp_LcgState * 5 + 0x71357911;
                mem->move.vx = (((u32)Gp_LcgState >> 16) & 0x3F) + 0x60;
                Gp_LcgState  = Gp_LcgState * 5 + 0x71357911;
                mem->move.vy = 0x10 - (((u32)Gp_LcgState >> 16) & 0x7F);
                Gp_LcgState  = Gp_LcgState * 5 + 0x71357911;
                mem->move.vz = ((u32)Gp_LcgState >> 16) & 0x7F;
                gte_SetRotMatrix(&mem->parent->coord);
                gte_ldv0(&mem->move);
                gte_rtv0_real();
                gte_stsv(&mem->move);
                break;
            case 3:
                mem->scale   = 0x114;
                mem->angle   = 0xF;
                Gp_LcgState  = Gp_LcgState * 5 + 0x71357911;
                mem->move.vx = (((u32)Gp_LcgState >> 16) & 0x3F) + 0x60;
                Gp_LcgState  = Gp_LcgState * 5 + 0x71357911;
                mem->move.vy = 0x10 - (((u32)Gp_LcgState >> 16) & 0x7F);
                Gp_LcgState  = Gp_LcgState * 5 + 0x71357911;
                mem->move.vz = ((u32)Gp_LcgState >> 16) & 0x7F;
                gte_SetRotMatrix(&mem->parent->coord);
                gte_ldv0(&mem->move);
                gte_rtv0_real();
                gte_stsv(&mem->move);
                break;
            case 30:
            case 31:
            case 32:
                mem->scale   = 0x114;
                mem->angle   = 0xA;
                Gp_LcgState  = Gp_LcgState * 5 + 0x71357911;
                mem->move.vx = (((u32)Gp_LcgState >> 16) & 0x3F) + 0x60;
                Gp_LcgState  = Gp_LcgState * 5 + 0x71357911;
                mem->move.vy = 0x10 - (((u32)Gp_LcgState >> 16) & 0x7F);
                Gp_LcgState  = Gp_LcgState * 5 + 0x71357911;
                mem->move.vz = ((u32)Gp_LcgState >> 16) & 0x7F;
                gte_SetRotMatrix(&mem->parent->coord);
                gte_ldv0(&mem->move);
                gte_rtv0_real();
                gte_stsv(&mem->move);
                break;
            case 5:
            case 33:
                mem->scale   = 0xD4;
                mem->angle   = 0x14;
                Gp_LcgState  = Gp_LcgState * 5 + 0x71357911;
                mem->move.vx = (((u32)Gp_LcgState >> 16) & 0x3F) + 0x60;
                Gp_LcgState  = Gp_LcgState * 5 + 0x71357911;
                mem->move.vy = 0x10 - (((u32)Gp_LcgState >> 16) & 0x7F);
                Gp_LcgState  = Gp_LcgState * 5 + 0x71357911;
                mem->move.vz = ((u32)Gp_LcgState >> 16) & 0x7F;
                gte_SetRotMatrix(&mem->parent->coord);
                gte_ldv0(&mem->move);
                gte_rtv0_real();
                gte_stsv(&mem->move);
                break;
            case 9:
                mem->angle   = 0x14;
                Gp_LcgState  = Gp_LcgState * 5 + 0x71357911;
                mem->move.vx = 0x10 - (((u32)Gp_LcgState >> 16) & 0x1F);
                Gp_LcgState  = Gp_LcgState * 5 + 0x71357911;
                mem->move.vy = (((u32)Gp_LcgState >> 16) & 0x3F) + 0x40;
                Gp_LcgState  = Gp_LcgState * 5 + 0x71357911;
                mem->move.vz = 0x10 - (((u32)Gp_LcgState >> 16) & 0x1F);
                Gp_LcgState  = Gp_LcgState * 5 + 0x71357911;
                mem->scale   = (((u32)Gp_LcgState >> 16) & 0x3F) + 0x40;
                gte_SetRotMatrix(&mem->parent->coord);
                gte_ldv0(&mem->move);
                gte_rtv0_real();
                gte_stsv(&mem->move);
                break;
            case 16:
            case 20:
            case 21:
            case 25:
            case 26:
            case 27:
            case 28:
            case 29:
                mem->scale   = 0x114;
                mem->angle   = 0xF;
                Gp_LcgState  = Gp_LcgState * 5 + 0x71357911;
                mem->move.vx = (((u32)Gp_LcgState >> 16) & 0x3F) + 0x60;
                Gp_LcgState  = Gp_LcgState * 5 + 0x71357911;
                mem->move.vy = 0x10 - (((u32)Gp_LcgState >> 16) & 0x7F);
                Gp_LcgState  = Gp_LcgState * 5 + 0x71357911;
                mem->move.vz = ((u32)Gp_LcgState >> 16) & 0x7F;
                gte_SetRotMatrix(&mem->parent->coord);
                gte_ldv0(&mem->move);
                gte_rtv0_real();
                gte_stsv(&mem->move);
                break;
            case 17:
                mem->scale   = 0x114;
                mem->angle   = 5;
                Gp_LcgState  = Gp_LcgState * 5 + 0x71357911;
                mem->move.vx = (((u32)Gp_LcgState >> 16) & 0x3F) + 0x40;
                Gp_LcgState  = Gp_LcgState * 5 + 0x71357911;
                mem->move.vy = 0x10 - (((u32)Gp_LcgState >> 16) & 0x1F);
                Gp_LcgState  = Gp_LcgState * 5 + 0x71357911;
                mem->move.vz = 0xFF80 - (((u32)Gp_LcgState >> 16) & 0x3F);
                gte_SetRotMatrix(&mem->parent->coord);
                gte_ldv0(&mem->move);
                gte_rtv0_real();
                gte_stsv(&mem->move);
                break;
            case 13:
            case 14:
            case 15:
                mem->scale   = 0xBF;
                mem->angle   = 0x14;
                Gp_LcgState  = Gp_LcgState * 5 + 0x71357911;
                mem->move.vx = (((u32)Gp_LcgState >> 16) & 0x3F) + 0x60;
                Gp_LcgState  = Gp_LcgState * 5 + 0x71357911;
                mem->move.vy = 0x10 - (((u32)Gp_LcgState >> 16) & 0x7F);
                Gp_LcgState  = Gp_LcgState * 5 + 0x71357911;
                mem->move.vz = ((u32)Gp_LcgState >> 16) & 0x7F;
                gte_SetRotMatrix(&mem->parent->coord);
                gte_ldv0(&mem->move);
                gte_rtv0_real();
                gte_stsv(&mem->move);
                break;
            case 23:
                mem->scale   = 0xBF;
                mem->angle   = 0x14;
                Gp_LcgState  = Gp_LcgState * 5 + 0x71357911;
                mem->move.vx = 0xFFA0 - (((u32)Gp_LcgState >> 16) & 0x3F);
                Gp_LcgState  = Gp_LcgState * 5 + 0x71357911;
                mem->move.vy = 0x10 - (((u32)Gp_LcgState >> 16) & 0x7F);
                Gp_LcgState  = Gp_LcgState * 5 + 0x71357911;
                mem->move.vz = -(((u32)Gp_LcgState >> 16) & 0x7F);
                memset(&tmp, 0, 0x10);
                tmp.vx = mem->move.vx;
                tmp.vy = mem->move.vy;
                tmp.vz = mem->move.vz;
                vec    = tmp;
                ApplyTransposeMatrixLV(&coord->coord, &vec, &vec);
                mem->move.vx = vec.vx;
                mem->move.vy = vec.vy;
                mem->move.vz = vec.vz;
                break;
            case 11:
                mem->scale   = 0x60;
                mem->angle   = 0x14;
                Gp_LcgState  = Gp_LcgState * 5 + 0x71357911;
                mem->move.vx = 0x10 - (((u32)Gp_LcgState >> 16) & 0x1F);
                Gp_LcgState  = Gp_LcgState * 5 + 0x71357911;
                mem->move.vy = 0x10 - (((u32)Gp_LcgState >> 16) & 0x1F);
                Gp_LcgState  = Gp_LcgState * 5 + 0x71357911;
                mem->move.vz = ((((u32)Gp_LcgState >> 16) & 0x1F) + 0x10);
                gte_SetRotMatrix(&mem->parent->coord);
                gte_ldv0(&mem->move);
                gte_rtv0_real();
                gte_stsv(&mem->move);
                break;
            case 12:
                mem->scale   = 0x80;
                mem->angle   = 0xF;
                Gp_LcgState  = Gp_LcgState * 5 + 0x71357911;
                mem->move.vx = 0x10 - (((u32)Gp_LcgState >> 16) & 0x1F);
                Gp_LcgState  = Gp_LcgState * 5 + 0x71357911;
                mem->move.vy = 0x10 - (((u32)Gp_LcgState >> 16) & 0x1F);
                Gp_LcgState  = Gp_LcgState * 5 + 0x71357911;
                mem->move.vz = 0xFFC0 - (((u32)Gp_LcgState >> 16) & 0x3F);
                gte_SetRotMatrix(&mem->parent->coord);
                gte_ldv0(&mem->move);
                gte_rtv0_real();
                gte_stsv(&mem->move);
                break;
            case 37:
                mem->scale   = 0x60;
                mem->angle   = 0x14;
                Gp_LcgState  = Gp_LcgState * 5 + 0x71357911;
                mem->move.vx = 0x10 - (((u32)Gp_LcgState >> 16) & 0x1F);
                Gp_LcgState  = Gp_LcgState * 5 + 0x71357911;
                mem->move.vy = 0x10 - (((u32)Gp_LcgState >> 16) & 0x1F);
                Gp_LcgState  = Gp_LcgState * 5 + 0x71357911;
                mem->move.vz = 0xFFF0 - (((u32)Gp_LcgState >> 16) & 0x1F);
                gte_SetRotMatrix(&mem->parent->coord);
                gte_ldv0(&mem->move);
                gte_rtv0_real();
                gte_stsv(&mem->move);
                break;
        }
        VectorNormalSS(&mem->move, &mem->move);
        Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
        mem->pos.vx = 0x100 - (((u32)Gp_LcgState >> 16) & 0x1FF);
        Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
        mem->pos.vy = 0x100 - (((u32)Gp_LcgState >> 16) & 0x1FF);
        Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
        mem->pos.vz = 0x100 - (((u32)Gp_LcgState >> 16) & 0x1FF);
        coord->flg  = 0;
        arg0->state = 1;
        Gfx_RotMatrixX(&coord->coord, 0x800, 0);
        return;
    }
    Gfx_RotMatrixXYZ(&coord->coord, &mem->pos, 0);
    MatrixNormal(&coord->coord, &coord->coord);
    gte_lddp(*(u16*)&mem->scale);
    vel = &mem->move;
    gte_ldsv(vel);
    gte_gpf12_real();
    gte_stsv(&delta);
    coord->coord.t[0] += delta.vx;
    coord->coord.t[1] += delta.vy;
    t2                 = coord->coord.t[2] + delta.vz;
    coord->flg         = 0;
    coord->coord.t[2]  = t2;
    gte_SetRotMatrix(&Gfx_ViewWorldMtx);
    gte_ldv0(&delta);
    gte_rtv0_real();
    gte_stsv(&dir);
    tx             = *(u16*)&coord->workm.t[0];
    pos.vx         = tx;
    ty             = *(u16*)&coord->workm.t[1];
    pos.vy         = ty;
    tz             = *(u16*)&coord->workm.t[2];
    *(u16*)&dir.vx = *(u16*)&dir.vx + tx;
    *(u16*)&dir.vy = *(u16*)&dir.vy + ty;
    pos.vz         = tz;
    *(u16*)&dir.vz = *(u16*)&dir.vz + tz;
    if (func_800DE7CC(&dir, &pos, &dir, &pos) == 1) {
        register SVECTOR* r0 asm("a0");
        r0 = vel;
        USE_REG(r0);
        coord->coord.t[0] -= delta.vx;
        coord->coord.t[1] -= delta.vy;
        coord->coord.t[2] -= delta.vz;
        COMPILER_BARRIER();
        {
            u16          t10;
            u16          t11;
            register s32 t12 asm("a1");
            s32          sum;
            t10 = *(volatile u16*)&pos.vx;
            t11 = *(volatile u16*)&mem->move;
            t12 = *(volatile u16*)&mem->move.vy;
            sum = ((s32)(t10 << 16) >> 17) + ((s32)(t11 << 16) >> 17);
            USE_REG(t12);
            mem->move.vx = sum;
            t12        <<= 16;
            t12        >>= 17;
            COMPILER_BARRIER();
            mem->move.vy = *(u16*)&pos.vy + t12;
        }
        dz           = (s32)(*(u16*)&mem->move.vz << 16) >> 17;
        mem->move.vz = ((s32)(*(u16*)&pos.vz << 16) >> 17) + dz;
        VectorNormalSS(vel, vel);
        scale      = (mem->scale * 2) / 3;
        mem->scale = scale;
        gte_lddp(scale);
        gte_ldsv(vel);
        gte_gpf12_real();
        gte_stsv(&delta);
        coord->coord.t[0] += delta.vx;
        coord->coord.t[1] += delta.vy;
        coord->coord.t[2] += delta.vz;
    } else {
        mem->move.vy += 0x180;
    }
    mem->age++;
    if (mem->angle < mem->age) {
        if (gDisplayState.animFrame & 1) {
            extra->flags &= 0xFF7F;
        } else {
            extra->flags |= 0x80;
        }
        if (mem->angle * 2 < mem->age) {
            goto release;
        }
    }
    return;
release:
    Gp_ReleaseState1CMem(mem, arg0);
}

void Gp_EffCtlTask6E(Task* arg0)
{
    GpEffWork*     mem;
    GsCOORDINATE2* coord;
    s32            rng;

    coord = (GsCOORDINATE2*)((TmdObject*)arg0->extra)->coords;
    mem   = arg0->spawnArg2;
    if (arg0->state == 0) {
        if (arg0->spawnArg1 & 0xF0000) {
            mem->scale = (u32)arg0->spawnArg1 >> 16;
        } else {
            mem->scale = 0xC;
        }
        arg0->spawnArg1    = (u16)arg0->spawnArg1;
        coord->sub         = mem->parent;
        coord->coord.t[0]  = D_801125EC[arg0->spawnArg1].vx;
        coord->coord.t[1]  = D_801125EC[arg0->spawnArg1].vy;
        coord->coord.t[2]  = D_801125EC[arg0->spawnArg1].vz;
        coord->coord.t[0] += mem->pos.vx;
        coord->coord.t[1] += mem->pos.vy;
        coord->coord.t[2] += mem->pos.vz;
        coord->flg         = 0;
        Gp_UpdateCoord(coord);
        arg0->state = 1;
        rng         = Gp_LcgState * 5 + 0x71357911;
        Gp_LcgState = rng;
        Gp_SpawnEff(0x60035, coord, (((u32)rng >> 16) & 0x1FF) | 0x11200, 0);
        rng         = Gp_LcgState * 5 + 0x71357911;
        Gp_LcgState = rng;
        Gp_SpawnEff(0x60035, coord, (((u32)rng >> 16) & 0x1FF) | 0x21200, 0);
        rng         = Gp_LcgState * 5 + 0x71357911;
        Gp_LcgState = rng;
        Gp_SpawnEff(0x60035, coord, (((u32)rng >> 16) & 0x1FF) | 0x31200, 0);
    }
    Gp_SpawnEff(0x60091, coord, arg0->spawnArg1, 0);
    mem->age++;
    if (mem->age > mem->scale - 1) {
        Gp_ReleaseState1CMem(mem, arg0);
    }
}

void Gp_EffCtlTask6D(Task* arg0)
{
    GsCOORDINATE2* coord;
    MATRIX*        m;
    void*          mem;
    s32            i;
    s32            one;

    i                    = 0;
    one                  = ONE;
    coord                = (GsCOORDINATE2*)((TmdObject*)arg0->extra)->coords;
    mem                  = arg0->spawnArg2;
    m                    = &coord->coord;
    *(s32*)&coord->coord = one;
    *(s32*)&m->m[0][2]   = 0;
    *(s32*)&m->m[1][1]   = one;
    *(s32*)&m->m[2][0]   = 0;
    m->m[2][2]           = one;
    coord->flg           = 0;
    Gp_UpdateCoord(coord);

    for (; i < 6; i++) {
        Gp_SpawnEff(0x60036, coord, 9, 0);
    }

    Gp_ReleaseState1CMem(mem, arg0);
}

void Gp_EffTileTaskA4(Task* arg0)
{
    GpEffWork*        mem;
    GsCOORDINATE2*    coord;
    GpEffTileScratch* block;
    TILE*             prim;
    s16               c;

    coord                  = (GsCOORDINATE2*)((TmdObject*)arg0->extra)->coords;
    *(u8**)G_SCRATCH_HEAD -= 0x14;
    block                  = (GpEffTileScratch*)*(u8**)G_SCRATCH_HEAD;
    mem                    = arg0->spawnArg2;
    Gp_UpdateCoord(coord);
    if (arg0->state == 0) {
        if (arg0->spawnArg1 != 0) {
            Gp_LcgState  = Gp_LcgState * 5 + 0x71357911;
            mem->move.vx = 0x20 - (((u32)Gp_LcgState >> 16) & 0x3F);
            Gp_LcgState  = Gp_LcgState * 5 + 0x71357911;
            mem->move.vy = 0x20 - (((u32)Gp_LcgState >> 16) & 0x3F);
            Gp_LcgState  = Gp_LcgState * 5 + 0x71357911;
            mem->move.vz = 0x20 - (((u32)Gp_LcgState >> 16) & 0x3F);
        } else {
            Gp_LcgState  = Gp_LcgState * 5 + 0x71357911;
            mem->move.vx = 0x40 - (((u32)Gp_LcgState >> 16) & 0x7F);
            Gp_LcgState  = Gp_LcgState * 5 + 0x71357911;
            mem->move.vy = (((u32)Gp_LcgState >> 16) & 0xFF) + 0x100;
            Gp_LcgState  = Gp_LcgState * 5 + 0x71357911;
            mem->move.vz = 0x40 - (((u32)Gp_LcgState >> 16) & 0x7F);

            gte_SetRotMatrix(&mem->parent->coord);
            gte_ldv0(&mem->move);
            gte_rtv0_real();
            gte_stsv(&mem->move);

            Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
            mem->scale  = ((u32)Gp_LcgState >> 16) % 3 + 1;
            Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
            mem->angle  = (((u32)Gp_LcgState >> 16) & 1) + 1;
        }
        arg0->state = 1;
    }
    coord->coord.t[0] += mem->move.vx;
    coord->coord.t[1] += mem->move.vy;
    coord->coord.t[2] += mem->move.vz;
    coord->flg         = 0;
    block->vec.vx      = *(u16*)&coord->workm.t[0];
    block->vec.vy      = *(u16*)&coord->workm.t[1];
    block->vec.vz      = *(u16*)&coord->workm.t[2];
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&block->vec);
    gte_rtps_real();
    gte_stsxy(&block->sxy);
    gte_stflg(&block->flag);
    if (block->flag >= 0) {
        gte_stszotz(&block->otz);
        block->otz     = block->otz + 1;
        prim           = (TILE*)gGpuPrimCursor;
        gGpuPrimCursor = prim + 1;
        setlen(prim, 3);
        setcode(prim, 0x60);
        c        = 0xFF - (u16)mem->age * 0x10;
        prim->w  = mem->angle;
        prim->h  = mem->angle;
        prim->r0 = c;
        prim->g0 = c >> mem->scale;
        prim->b0 = c >> 3;
        prim->x0 = block->sxy.vx;
        prim->y0 = block->sxy.vy;
        addPrim((u_long*)(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC) + (s32)gGpuCurrentOt),
                prim);
        Gp_AddTpageShift((P_TAG*)prim, 1, block->otz);
    }
    *(u8**)G_SCRATCH_HEAD += 0x14;
    mem->age++;
    if (mem->age >= 8) {
        Gp_ReleaseState1CMem(mem, arg0);
    }
}

void Gp_EffCtlTask3B(Task* arg0)
{
    GpEffWork*     mem;
    GsCOORDINATE2* coord;
    s16            flag;
    s32            i;
    s32            rng;

    mem   = arg0->spawnArg2;
    flag  = Gp_State1C->eventState;
    coord = (GsCOORDINATE2*)((TmdObject*)arg0->extra)->coords;
    if (flag < 2) {
        Gp_UpdateCoord(coord);
        if (arg0->state == 0) {
            rng         = Gp_LcgState * 5 + 0x71357911;
            mem->scale  = ((u32)rng >> 16) & 0xFFF;
            Gp_LcgState = rng;
            if (arg0->spawnArg1 & 0xFFF) {
                mem->angle = ((GpEffSpawnArg*)&arg0->spawnArg1)->field_0 & 0xFFF;
            } else {
                mem->angle = 0x200;
            }
            for (i = 0; i < 6; i++) {
                Gp_SpawnEff(0x600A4, coord, 1, 0);
            }
            arg0->state = 1;
        }
        Gp_DrawEffSprite3B(coord, mem->age, mem->angle, mem->scale);
        if (Gp_State1C->eventState != 0) {
            return;
        }
        mem->age++;
        if (mem->age < 4) {
            return;
        }
    } else if (flag < 4) {
        return;
    }
    Gp_ReleaseState1CMem(mem, arg0);
}

void Gp_DrawEffSprite3B(GsCOORDINATE2* arg0, u16 arg1, s16 arg2, s16 arg3)
{
    void**            scratch;
    u8*               head;
    GpEffBeamScratch* block;
    GpEffBeamScratch* vecp;
    POLY_FT4*         prim;
    s32               uv;
    s32               ang;
    u16               vz;

    scratch                                    = (void**)G_SCRATCH_HEAD;
    head                                       = *scratch;
    ((GpEffBeamScratch*)(head - 0x1C))->vec.vx = *(u16*)&arg0->workm.t[0];
    block                                      = (GpEffBeamScratch*)(head - 0x1C);
    block->vec.vy                              = *(u16*)&arg0->workm.t[1];
    vz                                         = *(u16*)&arg0->workm.t[2];
    *scratch                                   = block;
    block->vec.vz                              = vz;
    vecp                                       = block;
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&vecp->vec);
    gte_rtps_real();
    gte_stsxy(&((GpEffBeamScratch*)(head - 0x1C))->sxy);
    gte_stflg(&((GpEffBeamScratch*)(head - 0x1C))->flag);
    if (block->flag >= 0) {
        gte_stszotz(&((GpEffBeamScratch*)(head - 0x1C))->otz);
        block->otz     = block->otz + 1;
        prim           = (POLY_FT4*)gGpuPrimCursor;
        gGpuPrimCursor = prim + 1;
        setlen(prim, 9);
        setcode(prim, 0x2F);
        prim->tpage = 0x28;
        prim->clut  = 0x4241;
        uv          = arg1 * 0x18;
        prim->u0    = uv - 0x70;
        prim->u2    = uv - 0x70;
        prim->v0    = 0;
        prim->u1    = uv - 0x59;
        prim->v1    = 0;
        prim->v2    = 0x17;
        prim->u3    = uv - 0x59;
        prim->v3    = 0x17;
        block->dx   = ((((s16)arg2 * 23) / block->otz) * rsin((s16)arg3)) >> 12;
        block->dy   = ((((s16)arg2 * 23) / block->otz) * rcos((s16)arg3)) >> 12;
        prim->x0    = *(u16*)&block->sxy.vx + *(u16*)&block->dx;
        prim->x3    = *(u16*)&block->sxy.vx - *(u16*)&block->dx;
        prim->y0    = *(u16*)&block->sxy.vy - *(u16*)&block->dy;
        prim->y3    = *(u16*)&block->sxy.vy + *(u16*)&block->dy;
        ang         = (s16)arg3 + 0x400;
        block->dx   = ((((s16)arg2 * 23) / block->otz) * rsin(ang)) >> 12;
        block->dy   = ((((s16)arg2 * 23) / block->otz) * rcos(ang)) >> 12;
        prim->x1    = *(u16*)&block->sxy.vx + *(u16*)&block->dx;
        prim->x2    = *(u16*)&block->sxy.vx - *(u16*)&block->dx;
        prim->y1    = *(u16*)&block->sxy.vy - *(u16*)&block->dy;
        prim->y2    = *(u16*)&block->sxy.vy + *(u16*)&block->dy;
        addPrim((u_long*)(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC) + (s32)gGpuCurrentOt),
                prim);
    }
    *scratch = (u8*)*scratch + 0x1C;
}

void Gp_EffSprTask5C(Task* arg0)
{
    void**            scratch;
    u8*               head;
    GpEffBeamScratch* block;
    GpEffBeamScratch* vecp;
    GsCOORDINATE2*    coord;
    GpEffWork*        mem;
    POLY_FT4*         prim;
    GpEffUv8*         rec;
    s16               flag;
    s16               scale;
    s16               step;
    s32               rng;
    s32               i;
    s32               n;
    s32               t2;
    s32               tmp;
    u16               vz;

    mem   = arg0->spawnArg2;
    flag  = Gp_State1C->eventState;
    coord = (GsCOORDINATE2*)((TmdObject*)arg0->extra)->coords;
    if (flag >= 2) {
        if (flag < 4) {
            return;
        }
    } else {
        Gp_UpdateCoord(coord);
        if (arg0->state == 0) {
            scale = 0x200;
            if (arg0->spawnArg1 & 0xFFF) {
                scale = ((GpEffSpawnArg*)&arg0->spawnArg1)->field_0 & 0xFFF;
            }
            mem->scale  = scale;
            rng         = Gp_LcgState * 5 + 0x71357911;
            mem->angle  = ((u32)rng >> 16) & 0xFFF;
            Gp_LcgState = rng;
            if (arg0->spawnArg1 & 0xF000) {
                step = (arg0->spawnArg1 >> 12) & 0xF;
            } else {
                step = 2;
            }
            mem->period = step;
            mem->step   = (s32)(*(u16*)&mem->scale << 16) >> 23;
            tmp         = ((GpEffSpawnArgHi*)&arg0->spawnArg1)->field_3;
            mem->index  = tmp & 0xF;
            if (mem->index != 0) {
                Gp_LcgState  = Gp_LcgState * 5 + 0x71357911;
                mem->move.vx = 0x10 - (((u32)Gp_LcgState >> 16) & 0x1F);
                Gp_LcgState  = Gp_LcgState * 5 + 0x71357911;
                mem->move.vy = 0x10 - (((u32)Gp_LcgState >> 16) & 0x1F);
                Gp_LcgState  = Gp_LcgState * 5 + 0x71357911;
                mem->move.vz = 0x10 - (((u32)Gp_LcgState >> 16) & 0x1F);
                gte_lddp(mem->scale << 3);
                gte_ldsv(&mem->move);
                gte_gpf12_real();
                gte_stsv(&mem->move);
                gte_lddp(mem->index << 12);
                gte_ldsv(&mem->move);
                gte_gpf12_real();
                gte_stsv(&mem->move);
                gte_SetRotMatrix(&mem->parent->coord);
                gte_ldv0(&mem->move);
                gte_rtv0_real();
                gte_stsv(&mem->move);
            } else if (!(arg0->spawnArg1 & 0xF0000000)) {
                n = gDisplayState.animFrame & 3;
                i = 0;
                if (n != 0) {
                    do {
                        Gp_SpawnEff(0x6005C, coord, ((s32)(*(u16*)&mem->scale << 16) >> 17) | 0x02001000, 0);
                        i += 1;
                    } while (i < n);
                }
                n = (u32)gDisplayState.animFrame % 3;
                i = 0;
                if (n > 0) {
                    do {
                        Gp_SpawnEff(0x6005C, coord, ((s32)(*(u16*)&mem->scale << 16) >> 17) | 0x01002000, 0);
                        i += 1;
                    } while (i < n);
                }
            }
            arg0->state = 1;
        }
        scratch                                    = (void**)G_SCRATCH_HEAD;
        head                                       = *scratch;
        ((GpEffBeamScratch*)(head - 0x1C))->vec.vx = *(u16*)&coord->workm.t[0];
        block                                      = (GpEffBeamScratch*)(head - 0x1C);
        block->vec.vy                              = *(u16*)&coord->workm.t[1];
        vz                                         = *(u16*)&coord->workm.t[2];
        *scratch                                   = block;
        block->vec.vz                              = vz;
        vecp                                       = block;
        gte_SetTransMatrix(&GsWSMATRIX);
        gte_SetRotMatrix(&GsWSMATRIX);
        gte_ldv0(&vecp->vec);
        gte_rtps_real();
        gte_stsxy(&((GpEffBeamScratch*)(head - 0x1C))->sxy);
        gte_stflg(&((GpEffBeamScratch*)(head - 0x1C))->flag);
        if (block->flag >= 0) {
            gte_stszotz(&((GpEffBeamScratch*)(head - 0x1C))->otz);
            block->otz     = block->otz + 1;
            prim           = (POLY_FT4*)gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setlen(prim, 9);
            setcode(prim, 0x2C);
            rec         = &D_80111E48[mem->age / mem->period];
            prim->code |= 3;
            prim->tpage = 0x29;
            prim->clut  = (rec->clutY << 6) | ((rec->clutX >> 4) & 0x3F);
            prim->u0    = rec->u;
            prim->v0    = rec->v;
            prim->u1    = rec->u + 0x27;
            prim->v1    = rec->v;
            prim->u2    = rec->u;
            prim->v2    = rec->v + 0x27;
            prim->u3    = rec->u + 0x27;
            prim->v3    = rec->v + 0x27;
            block->dx   = ((((s16)mem->scale * 0x27) / block->otz) * rsin(mem->angle)) >> 12;
            block->dy   = ((((s16)mem->scale * 0x27) / block->otz) * rcos(mem->angle)) >> 12;
            prim->x0    = *(u16*)&block->sxy.vx + *(u16*)&block->dx;
            prim->x3    = *(u16*)&block->sxy.vx - *(u16*)&block->dx;
            prim->y0    = *(u16*)&block->sxy.vy - *(u16*)&block->dy;
            prim->y3    = *(u16*)&block->sxy.vy + *(u16*)&block->dy;
            block->dx   = ((((s16)mem->scale * 0x27) / block->otz) * rsin(mem->angle + 0x400)) >> 12;
            block->dy   = ((((s16)mem->scale * 0x27) / block->otz) * rcos(mem->angle + 0x400)) >> 12;
            prim->x1    = *(u16*)&block->sxy.vx + *(u16*)&block->dx;
            prim->x2    = *(u16*)&block->sxy.vx - *(u16*)&block->dx;
            prim->y1    = *(u16*)&block->sxy.vy - *(u16*)&block->dy;
            prim->y2    = *(u16*)&block->sxy.vy + *(u16*)&block->dy;
            addPrim((u_long*)(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC) + (s32)gGpuCurrentOt),
                    prim);
        }
        *scratch = (u8*)*scratch + 0x1C;
        if (Gp_State1C->eventState != 0) {
            return;
        }
        coord->coord.t[0] += mem->move.vx;
        coord->coord.t[1] += mem->move.vy;
        t2                 = coord->coord.t[2] + mem->move.vz;
        coord->flg         = 0;
        coord->coord.t[2]  = t2;
        mem->scale        += mem->step;
        mem->age++;
        if (mem->age <= mem->period * 12 - 1) {
            return;
        }
    }
    Gp_ReleaseState1CMem(mem, arg0);
}

void func_800F289C(Task* arg0)
{
    void**                     scratch;
    u8*                        head;
    GpEffBeamScratch*          block;
    register GpEffBeamScratch* vecp asm("v0");
    GpEffWork*                 mem;
    GsCOORDINATE2*             coord;
    POLY_FT4*                  prim;
    s16                        flag;
    s16                        scale;
    s16                        mode;
    s32                        tmp;
    s32                        i;
    s32                        n;
    s32                        mask;
    s32                        step;
    s32                        step2;
    s32                        mask2;
    register u16               vx asm("v0");
    u16                        vz;

    mem   = arg0->spawnArg2;
    flag  = Gp_State1C->eventState;
    coord = (GsCOORDINATE2*)((TmdObject*)arg0->extra)->coords;
    if (flag >= 2) {
        if (flag < 4) {
            return;
        }
    } else {
        Gp_UpdateCoord(coord);
        if (arg0->state == 0) {
            scale = 0x200;
            if (arg0->spawnArg1 & 0xFFF) {
                scale = ((GpEffSpawnArg*)&arg0->spawnArg1)->field_0 & 0xFFF;
            }
            mem->scale  = scale;
            Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
            mem->angle  = ((u32)Gp_LcgState >> 16) & 0xFFF;
            if (arg0->spawnArg1 & 0xF000) {
                mem->period = (arg0->spawnArg1 >> 12) & 0xF;
            } else {
                mem->period = 1;
            }
            if (arg0->spawnArg1 & 0xFF0000) {
                mem->step = (arg0->spawnArg1 >> 16) & 0xFF;
            } else {
                mem->step = mem->scale >> 8;
            }
            tmp        = ((GpEffSpawnArgHi*)&arg0->spawnArg1)->field_3;
            mode       = tmp & 0xF;
            mem->index = mode;
            if (mode != 0) {
                if (mode == 1) {
                    Gp_LcgState  = Gp_LcgState * 5 + 0x71357911;
                    mem->move.vx = 8 - (((u32)Gp_LcgState >> 16) & 0xF);
                    Gp_LcgState  = Gp_LcgState * 5 + 0x71357911;
                    mem->move.vy = (((u32)Gp_LcgState >> 16) & 0xF) * 3;
                    Gp_LcgState  = Gp_LcgState * 5 + 0x71357911;
                    mem->move.vz = 8 - (((u32)Gp_LcgState >> 16) & 0xF);
                } else {
                    Gp_LcgState  = Gp_LcgState * 5 + 0x71357911;
                    mem->move.vx = 0x10 - (((u32)Gp_LcgState >> 16) & 0x1F);
                    Gp_LcgState  = Gp_LcgState * 5 + 0x71357911;
                    mem->move.vy = 0x10 - (((u32)Gp_LcgState >> 0x10) & 0x1F);
                    Gp_LcgState  = Gp_LcgState * 5 + 0x71357911;
                    mem->move.vz = 0x10 - (((u32)Gp_LcgState >> 0x10) & 0x1F);
                    gte_lddp((mem->scale << 3));
                    gte_ldsv(&mem->move);
                    gte_gpf12_real();
                    gte_stsv(&mem->move);
                    gte_lddp((mem->index << 11));
                    gte_ldsv(&mem->move);
                    gte_gpf12_real();
                    gte_stsv(&mem->move);
                }
                gte_SetRotMatrix(&mem->parent->coord);
                gte_ldv0(&mem->move);
                gte_rtv0_real();
                gte_stsv(&mem->move);
            } else {
                switch (mem->step) {
                    case 1:
                        mem->move.vx = 0;
                        mem->move.vz = 0;
                        Gp_LcgState  = Gp_LcgState * 5 + 0x71357911;
                        mem->move.vy = -(((u32)Gp_LcgState >> 16) & 0xF) - 0x20;
                        break;
                    case 2:
                        Gp_LcgState  = Gp_LcgState * 5 + 0x71357911;
                        mem->move.vx = 0x10 - (((u32)Gp_LcgState >> 16) & 0x1F);
                        Gp_LcgState  = Gp_LcgState * 5 + 0x71357911;
                        mem->move.vy = -(((u32)Gp_LcgState >> 0x10) & 0x1F) - 0x10;
                        Gp_LcgState  = Gp_LcgState * 5 + 0x71357911;
                        mem->move.vz = 0x10 - (((u32)Gp_LcgState >> 0x10) & 0x1F);
                        break;
                    case 3:
                        Gp_LcgState  = Gp_LcgState * 5 + 0x71357911;
                        mem->move.vx = 0x10 - (((u32)Gp_LcgState >> 16) & 0x1F);
                        Gp_LcgState  = Gp_LcgState * 5 + 0x71357911;
                        mem->move.vy = 0x10 - (((u32)Gp_LcgState >> 0x10) & 0x1F);
                        Gp_LcgState  = Gp_LcgState * 5 + 0x71357911;
                        mem->move.vz = 0x10 - (((u32)Gp_LcgState >> 0x10) & 0x1F);
                        gte_lddp((mem->scale << 2));
                        gte_ldsv(&mem->move);
                        gte_gpf12_real();
                        gte_stsv(&mem->move);
                        break;
                }
            }
            if (arg0->spawnArg1 & 0x30000000) {
                n = gDisplayState.animFrame & 3;
                for (i = 0; i < n; i++) {
                    step = mem->scale - (mem->scale >> 2);
                    mask = (arg0->spawnArg1 & 0xC0000000) | 0x6002000;
                    Gp_SpawnEff(0x60070, coord, step | mask, 0);
                }
                n = (u32)gDisplayState.animFrame % 3;
                for (i = 0; i < n; i++) {
                    step2 = mem->scale - (mem->scale >> 2);
                    mask2 = (arg0->spawnArg1 & 0xC0000000) | 0x4003000;
                    Gp_SpawnEff(0x60070, coord, step2 | mask2, 0);
                }
            }
            arg0->state = 1;
        }
        scratch = (void**)G_SCRATCH_HEAD;
        head    = *scratch;
        USE_REG(head);
        vx                                         = *(u16*)&coord->workm.t[0];
        ((GpEffBeamScratch*)(head - 0x1C))->vec.vx = vx;
        vecp                                       = (GpEffBeamScratch*)(head - 0x1C);
        block                                      = vecp;
        block->vec.vy                              = *(u16*)&coord->workm.t[1];
        vz                                         = *(u16*)&coord->workm.t[2];
        *scratch                                   = block;
        block->vec.vz                              = vz;
        gte_SetTransMatrix(&GsWSMATRIX);
        gte_SetRotMatrix(&GsWSMATRIX);
        gte_ldv0(&block->vec);
        gte_rtps_real();
        gte_stsxy(&((GpEffBeamScratch*)(head - 0x1C))->sxy);
        gte_stflg(&((GpEffBeamScratch*)(head - 0x1C))->flag);
        if (block->flag >= 0) {
            gte_stszotz(&((GpEffBeamScratch*)(head - 0x1C))->otz);
            block->otz     = block->otz + 1;
            prim           = (POLY_FT4*)gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setlen(prim, 9);
            setcode(prim, 0x2F);
            if (arg0->spawnArg1 & 0xC0000000) {
                prim->tpage = ((((u32)arg0->spawnArg1 >> 30) - 1) & 3) << 5 | 8;
            } else {
                prim->tpage = 0x28;
            }
            prim->clut = 0x4253;
            prim->u0   = (mem->age / mem->period) << 5;
            prim->v0   = 0x18;
            prim->u1   = ((mem->age / mem->period) << 5) + 0x1F;
            prim->v1   = 0x18;
            prim->u2   = (mem->age / mem->period) << 5;
            prim->v2   = 0x37;
            prim->u3   = ((mem->age / mem->period) << 5) + 0x1F;
            prim->v3   = 0x37;
            block->dx  = (((mem->scale * 0x1F) / block->otz) * rsin(mem->angle)) >> 12;
            block->dy  = (((mem->scale * 0x1F) / block->otz) * rcos(mem->angle)) >> 12;
            prim->x0   = *(u16*)&block->sxy.vx + *(u16*)&block->dx;
            prim->x3   = *(u16*)&block->sxy.vx - *(u16*)&block->dx;
            prim->y0   = *(u16*)&block->sxy.vy - *(u16*)&block->dy;
            prim->y3   = *(u16*)&block->sxy.vy + *(u16*)&block->dy;
            block->dx  = (((mem->scale * 0x1F) / block->otz) * rsin(mem->angle + 0x400)) >> 12;
            block->dy  = (((mem->scale * 0x1F) / block->otz) * rcos(mem->angle + 0x400)) >> 12;
            prim->x1   = *(u16*)&block->sxy.vx + *(u16*)&block->dx;
            prim->x2   = *(u16*)&block->sxy.vx - *(u16*)&block->dx;
            prim->y1   = *(u16*)&block->sxy.vy - *(u16*)&block->dy;
            prim->y2   = *(u16*)&block->sxy.vy + *(u16*)&block->dy;
            addPrim((u_long*)(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC) + (s32)gGpuCurrentOt),
                    prim);
        }
        *(void**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD + 0x1C;
        if (Gp_State1C->eventState != 0) {
            return;
        }
        coord->coord.t[0] += mem->move.vx;
        coord->coord.t[1] += mem->move.vy;
        coord->coord.t[2] += mem->move.vz;
        coord->flg         = 0;
        mem->age++;
        if (mem->age <= mem->period * 8 - 1) {
            return;
        }
    }
    Gp_ReleaseState1CMem(mem, arg0);
}

void Gp_EffSprTask76(Task* arg0)
{
    void**                     scratch;
    u8*                        head;
    register GpEffBeamScratch* vecp asm("v0");
    GpEffBeamScratch*          block;
    GsCOORDINATE2*             coord;
    GpEffWork*                 mem;
    POLY_FT4*                  prim;
    u16                        size;
    s16                        scale;
    s32                        rng;

    coord    = (GsCOORDINATE2*)((TmdObject*)arg0->extra)->coords;
    scratch  = (void**)G_SCRATCH_HEAD;
    head     = *scratch;
    vecp     = (GpEffBeamScratch*)(head - 0x1C);
    block    = vecp;
    *scratch = vecp;
    mem      = arg0->spawnArg2;
    Gp_UpdateCoord(coord);
    ((GpEffBeamScratch*)(head - 0x1C))->vec.vx = *(u16*)&coord->workm.t[0];
    block->vec.vy                              = *(u16*)&coord->workm.t[1];
    block->vec.vz                              = *(u16*)&coord->workm.t[2];
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&block->vec);
    gte_rtps_real();
    gte_stsxy(&((GpEffBeamScratch*)(head - 0x1C))->sxy);
    gte_stflg(&((GpEffBeamScratch*)(head - 0x1C))->flag);
    if (block->flag >= 0) {
        gte_stszotz(&((GpEffBeamScratch*)(head - 0x1C))->otz);
        block->otz     = block->otz + 1;
        prim           = (POLY_FT4*)gGpuPrimCursor;
        gGpuPrimCursor = prim + 1;
        setlen(prim, 9);
        setcode(prim, 0x2C);
        size = Gp_EffSprRecs[mem->age].w;
        if (arg0->state == 0) {
            if (arg0->spawnArg1 & 0xFFF) {
                scale = ((GpEffSpawnArg*)&arg0->spawnArg1)->field_0 & 0xFFF;
            } else {
                scale = 0x200;
            }
            mem->scale  = scale;
            rng         = Gp_LcgState * 5 + 0x71357911;
            Gp_LcgState = rng;
            mem->angle  = ((u32)rng >> 16) & 0xFFF;
            arg0->state = 1;
        }
        prim->code |= 3;
        prim->tpage = ((Gp_EffSprRecs[mem->age].tpageX & 0x3FF) >> 6) | 0x20;
        prim->clut  = (Gp_EffSprRecs[mem->age].clutY << 6) | ((Gp_EffSprRecs[mem->age].clutX >> 4) & 0x3F);
        prim->u0    = Gp_EffSprRecs[mem->age].u;
        prim->v0    = Gp_EffSprRecs[mem->age].v;
        prim->u1    = Gp_EffSprRecs[mem->age].u + size;
        prim->v1    = Gp_EffSprRecs[mem->age].v;
        prim->u2    = Gp_EffSprRecs[mem->age].u;
        prim->v2    = Gp_EffSprRecs[mem->age].v + size;
        prim->u3    = Gp_EffSprRecs[mem->age].u + size;
        prim->v3    = Gp_EffSprRecs[mem->age].v + size;
        block->dx   = ((((s16)size * mem->scale) / block->otz) * rsin(mem->angle)) >> 12;
        block->dy   = ((((s16)size * mem->scale) / block->otz) * rcos(mem->angle)) >> 12;
        prim->x0    = *(u16*)&block->sxy.vx + *(u16*)&block->dx;
        prim->x3    = *(u16*)&block->sxy.vx - *(u16*)&block->dx;
        prim->y0    = *(u16*)&block->sxy.vy - *(u16*)&block->dy;
        prim->y3    = *(u16*)&block->sxy.vy + *(u16*)&block->dy;
        block->dx   = ((((s16)size * mem->scale) / block->otz) * rsin(mem->angle + 0x400)) >> 12;
        block->dy   = ((((s16)size * mem->scale) / block->otz) * rcos(mem->angle + 0x400)) >> 12;
        prim->x1    = *(u16*)&block->sxy.vx + *(u16*)&block->dx;
        prim->x2    = *(u16*)&block->sxy.vx - *(u16*)&block->dx;
        prim->y1    = *(u16*)&block->sxy.vy - *(u16*)&block->dy;
        prim->y2    = *(u16*)&block->sxy.vy + *(u16*)&block->dy;
        addPrim((u_long*)(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC) + (s32)gGpuCurrentOt),
                prim);
    }
    *(void**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD + 0x1C;
    mem->age++;
    if (mem->age >= 4) {
        Gp_ReleaseState1CMem(mem, arg0);
    }
}

void Gp_EffSprTask7C(Task* arg0)
{
    GsCOORDINATE2              hit;
    GpEffWork*                 mem;
    GsCOORDINATE2*             coord;
    void**                     scratch;
    u8*                        head;
    register GpEffBeamScratch* vecp asm("v0");
    GpEffBeamScratch*          block;
    POLY_FT4*                  prim;
    s16                        flag;
    s32                        rng;
    s16                        scale;
    s16                        step;
    s32                        col;
    s32                        tmp;
    u32                        param;
    register u16               vx asm("v0");
    u16                        vz;

    mem   = arg0->spawnArg2;
    flag  = Gp_State1C->eventState;
    coord = (GsCOORDINATE2*)((TmdObject*)arg0->extra)->coords;
    param = 0x80;
    if (flag >= 2) {
        if (flag >= 4) {
            Gp_ReleaseState1CMem(mem, arg0);
        }
        return;
    }
    if (mem->index == 0) {
        scale = 0x200;
        if (arg0->spawnArg1 & 0xFFF) {
            scale = ((GpEffSpawnArg*)&arg0->spawnArg1)->field_0 & 0xFFF;
        }
        mem->scale  = scale;
        rng         = Gp_LcgState * 5 + 0x71357911;
        mem->angle  = ((u32)rng >> 16) & 0xFFF;
        Gp_LcgState = rng;
        if (arg0->spawnArg1 & 0xF000) {
            step = (arg0->spawnArg1 >> 12) & 0xF;
        } else {
            step = 1;
        }
        mem->period  = step;
        Gp_LcgState  = Gp_LcgState * 5 + 0x71357911;
        mem->step    = 0x100 - (((u32)Gp_LcgState >> 16) & 0x1F0);
        Gp_LcgState  = Gp_LcgState * 5 + 0x71357911;
        mem->move.vx = 0x40 - (((u32)Gp_LcgState >> 16) & 0x7F);
        Gp_LcgState  = Gp_LcgState * 5 + 0x71357911;
        mem->move.vy = 0x80 - (((u32)Gp_LcgState >> 16) & 0xFF);
        Gp_LcgState  = Gp_LcgState * 5 + 0x71357911;
        mem->move.vz = 0x40 - (((u32)Gp_LcgState >> 16) & 0x7F);
        mem->index++;
    }
    Gp_UpdateCoord(coord);
    scratch = (void**)G_SCRATCH_HEAD;
    head    = *scratch;
    USE_REG(head);
    vx                                         = *(u16*)&coord->workm.t[0];
    ((GpEffBeamScratch*)(head - 0x1C))->vec.vx = vx;
    vecp                                       = (GpEffBeamScratch*)(head - 0x1C);
    block                                      = vecp;
    block->vec.vy                              = *(u16*)&coord->workm.t[1];
    vz                                         = *(u16*)&coord->workm.t[2];
    *scratch                                   = block;
    block->vec.vz                              = vz;
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&block->vec);
    gte_rtps_real();
    gte_stsxy(&((GpEffBeamScratch*)(head - 0x1C))->sxy);
    gte_stflg(&((GpEffBeamScratch*)(head - 0x1C))->flag);
    if (block->flag >= 0) {
        gte_stszotz(&((GpEffBeamScratch*)(head - 0x1C))->otz);
        block->otz     = block->otz + 1;
        prim           = (POLY_FT4*)gGpuPrimCursor;
        gGpuPrimCursor = prim + 1;
        setlen(prim, 9);
        setcode(prim, 0x2C);
        if (mem->age >= 0x18) {
            col = (0x1F - mem->age) * 16;
            __asm__ volatile("" : "=r"(tmp) : "0"(col));
            param    = (u8)tmp;
            prim->r0 = col;
            prim->g0 = col;
            prim->b0 = col;
        } else {
            setcode(prim, 0x2D);
        }
        prim->tpage = 0x28;
        prim->code |= 2;
        prim->clut  = 0x428A;
        prim->u0    = ((mem->age / mem->period) % 6) * 16;
        prim->v0    = 0x58;
        prim->u1    = ((mem->age / mem->period) % 6) * 16 + 0xF;
        prim->v1    = 0x58;
        prim->u2    = ((mem->age / mem->period) % 6) * 16;
        prim->v2    = 0x67;
        prim->u3    = ((mem->age / mem->period) % 6) * 16 + 0xF;
        prim->v3    = 0x67;
        block->dx   = (((mem->scale * 15) / block->otz) * rsin(mem->angle)) >> 12;
        block->dy   = (((mem->scale * 15) / block->otz) * rcos(mem->angle)) >> 12;
        prim->x0    = *(u16*)&block->sxy.vx + *(u16*)&block->dx;
        prim->x3    = *(u16*)&block->sxy.vx - *(u16*)&block->dx;
        prim->y0    = *(u16*)&block->sxy.vy - *(u16*)&block->dy;
        prim->y3    = *(u16*)&block->sxy.vy + *(u16*)&block->dy;
        block->dx   = (((mem->scale * 15) / block->otz) * rsin(mem->angle + 0x400)) >> 12;
        block->dy   = (((mem->scale * 15) / block->otz) * rcos(mem->angle + 0x400)) >> 12;
        prim->x1    = *(u16*)&block->sxy.vx + *(u16*)&block->dx;
        prim->x2    = *(u16*)&block->sxy.vx - *(u16*)&block->dx;
        prim->y1    = *(u16*)&block->sxy.vy - *(u16*)&block->dy;
        prim->y2    = *(u16*)&block->sxy.vy + *(u16*)&block->dy;
        addPrim((u_long*)(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC) + (s32)gGpuCurrentOt),
                prim);
    }
    *(void**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD + 0x1C;
    if (Gp_State1C->eventState != 0) {
        return;
    }
    coord->coord.t[0] += mem->move.vx;
    coord->coord.t[1] += mem->move.vy;
    coord->coord.t[2] += mem->move.vz;
    coord->flg         = 0;
    Gp_UpdateCoord(coord);
    mem->move.vy += 5;
    mem->angle   += mem->step;
    mem->age++;
    if (mem->age >= 0x1F) {
        Gp_ReleaseState1CMem(mem, arg0);
        return;
    }
    if (Gp_TraceGroundCoord(coord, &hit) == 1) {
        Gp_DrawEffSprite7C(&hit, (s32)(*(u16*)&mem->scale << 16) >> 17, param);
    }
    if (coord->coord.t[1] > hit.coord.t[1]) {
        coord->coord.t[1] -= mem->move.vy * 2;
        mem->move.vy       = -((s32)(*(u16*)&mem->move.vy << 16) >> 17);
        mem->move.vx       = (s32)(*(u16*)&mem->move.vx << 16) >> 17;
        mem->move.vz       = (s32)(*(u16*)&mem->move.vz << 16) >> 17;
    }
}

void func_800F4308(Task* arg0)
{
    u8             rgb[3];
    GpEffWork*     mem;
    GsCOORDINATE2* coord;
    GsCOORDINATE2* roomCoord;
    GpCoord64*     room;
    GpCoordTail*   slot;
    TmdObject*     extra;
    SVECTOR*       vec;
    s16            flag;
    s32            scale11;
    s32            scale12;
    s32            count;
    s32            cond;
    s32            condInc;
    s32            i;
    s32            t2_10;
    s32            t2_11;
    s32            t2_12;
    s32            rng;
    s32            tmp;

    room      = &Gp_RoomCoords[1];
    slot      = (GpCoordTail*)&room->coord;
    roomCoord = &slot->coord;
    extra     = arg0->extra;
    mem       = arg0->spawnArg2;
    flag      = Gp_State1C->eventState;
    coord     = (GsCOORDINATE2*)extra->coords;
    if (flag != 0) {
        cond = flag < 4;
        goto release;
    }
    Gp_UpdateCoord(coord);
    mem->age = (u16)mem->age + 1;
    switch (arg0->spawnArg1) {
        case 10:
            switch (arg0->state) {
                case 0:
                    Gp_LcgState  = Gp_LcgState * 5 + 0x71357911;
                    mem->move.vx = 0x100 - (((u32)Gp_LcgState >> 16) & 0x1FF);
                    Gp_LcgState  = Gp_LcgState * 5 + 0x71357911;
                    mem->move.vy = 0x100 - (((u32)Gp_LcgState >> 16) & 0x1FF);
                    Gp_LcgState  = Gp_LcgState * 5 + 0x71357911;
                    mem->move.vz = 0x100 - (((u32)Gp_LcgState >> 16) & 0x1FF);
                    vec          = &mem->move;
                    Gp_SpawnEff(0x6005C, coord, 0x600, vec);
                    rng = Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
                    Gp_SpawnEff(0x6007C, coord, (((u32)rng >> 16) & 0x3F) | 0x100, vec);
                    rng = Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
                    Gp_SpawnEff(0x6007C, coord, (((u32)rng >> 16) & 0x3F) | 0x100, vec);
                    arg0->state++;
                    break;
                case 1:
                    Gp_LcgState  = Gp_LcgState * 5 + 0x71357911;
                    mem->move.vx = 0x100 - (((u32)Gp_LcgState >> 16) & 0x1FF);
                    Gp_LcgState  = Gp_LcgState * 5 + 0x71357911;
                    mem->move.vy = 0x100 - (((u32)Gp_LcgState >> 16) & 0x1FF);
                    Gp_LcgState  = Gp_LcgState * 5 + 0x71357911;
                    mem->move.vz = 0x100 - (((u32)Gp_LcgState >> 16) & 0x1FF);
                    rng = Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
                    Gp_SpawnEff(0x60070, coord, (((u32)rng >> 16) & 0x1FF) | 0xD0000400,
                                &mem->move);
                    if (mem->age >= 7) {
                        arg0->state++;
                    }
                    break;
                case 2:
                    Gp_LcgState  = Gp_LcgState * 5 + 0x71357911;
                    mem->move.vx = 0x100 - (((u32)Gp_LcgState >> 16) & 0x1FF);
                    Gp_LcgState  = Gp_LcgState * 5 + 0x71357911;
                    mem->move.vy = 0x100 - (((u32)Gp_LcgState >> 16) & 0x1FF);
                    Gp_LcgState  = Gp_LcgState * 5 + 0x71357911;
                    mem->move.vz = 0x100 - (((u32)Gp_LcgState >> 16) & 0x1FF);
                    rng = Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
                    Gp_SpawnEff(0x60070, coord, (((u32)rng >> 16) & 0xFF) | 0x82003400,
                                &mem->move);
                    if (mem->age >= 0xB) {
                        arg0->state++;
                    }
                    break;
            }
            room->field_0         = 0x10;
            count                 = mem->age;
            slot->field_5C        = 0x2580;
            slot->field_50        = 0x1000;
            slot->field_52        = 0xC00;
            slot->field_54        = 0x800;
            slot->field_58        = (0x898 - (count * 0x64)) * 4;
            roomCoord->coord.t[0] = coord->coord.t[0];
            roomCoord->coord.t[1] = coord->coord.t[1];
            t2_10                 = coord->coord.t[2];
            roomCoord->flg        = 0;
            roomCoord->coord.t[2] = t2_10;
            cond                  = mem->age < 0x15;
            goto release;
        case 11:
            switch (arg0->state) {
                case 0:
                    Gp_LcgState  = Gp_LcgState * 5 + 0x71357911;
                    mem->move.vx = 0x100 - (((u32)Gp_LcgState >> 16) & 0x1FF);
                    Gp_LcgState  = Gp_LcgState * 5 + 0x71357911;
                    mem->move.vy = 0x100 - (((u32)Gp_LcgState >> 16) & 0x1FF);
                    Gp_LcgState  = Gp_LcgState * 5 + 0x71357911;
                    mem->move.vz = 0x100 - (((u32)Gp_LcgState >> 16) & 0x1FF);
                    Gp_SpawnEff(0x60076, coord, 0x500, &mem->move);
                    condInc = mem->age < 2;
                    goto maybe11;
                case 1:
                    i = 0;
                    do {
                        Gp_LcgState  = Gp_LcgState * 5 + 0x71357911;
                        mem->move.vx = 0x100 - (((u32)Gp_LcgState >> 16) & 0x1FF);
                        Gp_LcgState  = Gp_LcgState * 5 + 0x71357911;
                        mem->move.vy = 0x100 - (((u32)Gp_LcgState >> 16) & 0x1FF);
                        Gp_LcgState  = Gp_LcgState * 5 + 0x71357911;
                        mem->move.vz = 0x100 - (((u32)Gp_LcgState >> 16) & 0x1FF);
                        rng = Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
                        Gp_SpawnEff(0x60070, coord, (((u32)rng >> 16) & 0x1FF) | 0x82004400,
                                    &mem->move);
                        i += 1;
                    } while (i < 2);
                    condInc = mem->age < 9;
                    goto maybe11;
                case 2:
                    i = 0;
                    do {
                        Gp_LcgState  = Gp_LcgState * 5 + 0x71357911;
                        mem->move.vx = 0x100 - (((u32)Gp_LcgState >> 16) & 0x1FF);
                        Gp_LcgState  = Gp_LcgState * 5 + 0x71357911;
                        mem->move.vy = 0x100 - (((u32)Gp_LcgState >> 16) & 0x1FF);
                        Gp_LcgState  = Gp_LcgState * 5 + 0x71357911;
                        mem->move.vz = 0x100 - (((u32)Gp_LcgState >> 16) & 0x1FF);
                        rng = Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
                        Gp_SpawnEff(0x60070, coord, (((u32)rng >> 16) & 0xFF) | 0xD0000400,
                                    &mem->move);
                        i += 1;
                    } while (i < 2);
                    condInc = mem->age < 0xD;
                maybe11:
                    if (condInc != 0) {
                        break;
                    }
                    arg0->state += 1;
                    break;
            }
            if (mem->scale++ < 8) {
                tmp     = -0x80 - (mem->scale << 4);
                rgb[0]  = tmp;
                rgb[1]  = (rgb[0] * 3) >> 2;
                rgb[2]  = (rgb[0] * 2) / 3;
                scale11 = (mem->scale << 6) + 0x40;
                Gp_DrawArc(coord, (s16)scale11, (s16)scale11, rgb);
            }
            if (mem->age < 4) {
                i = 0;
                do {
                    Gp_SpawnEff(0x60092, coord, 0, 0);
                    i += 1;
                } while (i < 3);
            }
            room->field_0         = 0x10;
            count                 = mem->age;
            slot->field_5C        = 0x2580;
            slot->field_50        = 0xC00;
            slot->field_52        = 0xC00;
            slot->field_54        = 0x800;
            slot->field_58        = (0x898 - (count * 0x64)) * 4;
            roomCoord->coord.t[0] = coord->coord.t[0];
            roomCoord->coord.t[1] = coord->coord.t[1];
            t2_11                 = coord->coord.t[2];
            roomCoord->flg        = 0;
            roomCoord->coord.t[2] = t2_11;
            cond                  = mem->age < 0x15;
            goto release;
        case 12:
            if (arg0->state != 0) {
                if (arg0->state == 1) {
                    goto case12_1;
                }
                goto skip12;
            }
            Gp_LcgState  = Gp_LcgState * 5 + 0x71357911;
            mem->move.vx = 0x100 - (((u32)Gp_LcgState >> 16) & 0x1FF);
            Gp_LcgState  = Gp_LcgState * 5 + 0x71357911;
            mem->move.vy = 0x100 - (((u32)Gp_LcgState >> 16) & 0x1FF);
            Gp_LcgState  = Gp_LcgState * 5 + 0x71357911;
            mem->move.vz = 0x100 - (((u32)Gp_LcgState >> 16) & 0x1FF);
            Gp_SpawnEff(0x60076, coord, 0x500, &mem->move);
            condInc = mem->age < 2;
            goto maybe12;
        case12_1:
            i = 0;
            do {
                Gp_LcgState  = Gp_LcgState * 5 + 0x71357911;
                mem->move.vx = 0x100 - (((u32)Gp_LcgState >> 16) & 0x1FF);
                Gp_LcgState  = Gp_LcgState * 5 + 0x71357911;
                mem->move.vy = 0x100 - (((u32)Gp_LcgState >> 16) & 0x1FF);
                Gp_LcgState  = Gp_LcgState * 5 + 0x71357911;
                mem->move.vz = 0x100 - (((u32)Gp_LcgState >> 16) & 0x1FF);
                rng = Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
                Gp_SpawnEff(0x60070, coord, (((u32)rng >> 16) & 0x1FF) | 0x82004400,
                            &mem->move);
                i += 1;
            } while (i < 2);
            condInc = mem->age < 9;
        maybe12:
            if (condInc == 0) {
                arg0->state += 1;
            }
        skip12:
            if (mem->scale++ < 8) {
                rgb[2]  = (-0x80 - (mem->scale << 4)) * 2;
                rgb[1]  = (rgb[2] & 0xE0) >> 2;
                rgb[0]  = rgb[1];
                scale12 = (mem->scale << 6) + 0x40;
                Gp_DrawArc(coord, (s16)scale12, (s16)scale12, rgb);
                if (gDisplayState.animFrame & 1) {
                    rgb[2] = ~(mem->scale * 0x1F);
                    rgb[1] = rgb[2] >> 2;
                    rgb[0] = rgb[1];
                    Gp_DrawFadeQuad(rgb, 1);
                }
            }
            room->field_0         = 0x10;
            count                 = mem->age;
            slot->field_5C        = 0x2580;
            slot->field_50        = 0x800;
            slot->field_52        = 0xC00;
            slot->field_54        = 0x1000;
            slot->field_58        = (0x898 - (count * 0x64)) * 4;
            roomCoord->coord.t[0] = coord->coord.t[0];
            roomCoord->coord.t[1] = coord->coord.t[1];
            t2_12                 = coord->coord.t[2];
            roomCoord->flg        = 0;
            roomCoord->coord.t[2] = t2_12;
            cond                  = mem->age < 0x15;
            goto release;
        default:
            return;
    }
release:
    if (cond == 0) {
        Gp_ReleaseState1CMem(mem, arg0);
    }
}

void Gp_EffLineTask92(Task* arg0)
{
    GpEffLineScratch* block;
    GpEffWork*        mem;
    GsCOORDINATE2*    coord;
    MATRIX*           m;
    LINE_F2*          prim;
    s16               step;
    s32               rng;
    s32               one;
    s16               val;

    *(void**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD - 0x20;
    coord                   = (GsCOORDINATE2*)((TmdObject*)arg0->extra)->coords;
    block                   = (GpEffLineScratch*)*(void**)G_SCRATCH_HEAD;
    mem                     = arg0->spawnArg2;
    Gp_UpdateCoord(coord);
    if (mem->age == 0) {
        Gp_LcgState  = Gp_LcgState * 5 + 0x71357911;
        mem->move.vx = 0x80 - (((u32)Gp_LcgState >> 16) & 0xFF);
        Gp_LcgState  = Gp_LcgState * 5 + 0x71357911;
        mem->move.vy = 0x80 - (((u32)Gp_LcgState >> 16) & 0xFF);
        Gp_LcgState  = Gp_LcgState * 5 + 0x71357911;
        mem->move.vz = 0x80 - (((u32)Gp_LcgState >> 16) & 0xFF);
        Gp_LcgState  = Gp_LcgState * 5 + 0x71357911;
        step         = 2;
        if ((((u32)Gp_LcgState >> 16) & 3) != 0) {
            step = 1;
        }
        rng                  = Gp_LcgState * 5 + 0x71357911;
        one                  = ONE;
        m                    = &coord->coord;
        mem->angle           = (((u32)rng >> 16) & 1) + 1;
        mem->scale           = step;
        *(s32*)&coord->coord = one;
        *(s32*)&m->m[0][2]   = 0;
        *(s32*)&m->m[1][1]   = one;
        *(s32*)&m->m[2][0]   = 0;
        m->m[2][2]           = one;
        mem->pos.vx          = *(u16*)&coord->workm.t[0];
        mem->pos.vy          = *(u16*)&coord->workm.t[1];
        mem->pos.vz          = *(u16*)&coord->workm.t[2];
        Gp_LcgState          = rng;
    }
    coord->coord.t[0] += mem->move.vx;
    coord->coord.t[1] += mem->move.vy;
    coord->coord.t[2] += mem->move.vz;
    coord->flg         = 0;
    block->vec0.vx     = *(u16*)&coord->workm.t[0];
    block->vec0.vy     = *(u16*)&coord->workm.t[1];
    block->vec0.vz     = *(u16*)&coord->workm.t[2];
    block->vec1.vx     = *(u16*)&mem->pos.vx;
    block->vec1.vy     = *(u16*)&mem->pos.vy;
    block->vec1.vz     = *(u16*)&mem->pos.vz;
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&block->vec0);
    gte_rtps_real();
    gte_stsxy(&block->sxy0);
    gte_stflg(&block->flag);
    if (block->flag >= 0) {
        gte_ldv0(&block->vec1);
        gte_rtps_real();
        gte_stsxy(&block->sxy1);
        gte_stflg(&block->flag);
        if (block->flag >= 0) {
            gte_stszotz(&block->otz);
            prim           = (LINE_F2*)gGpuPrimCursor;
            block->otz     = block->otz + 1;
            gGpuPrimCursor = prim + 1;
            setlen(prim, 3);
            setcode(prim, 0x40);
            val = 0xFF - (mem->age << (6 - mem->scale));
            if (arg0->spawnArg1 != 0) {
                prim->r0 = val >> 3;
                prim->g0 = val >> mem->angle;
                prim->b0 = val;
            } else {
                prim->r0 = val;
                prim->g0 = val >> mem->angle;
                prim->b0 = val >> 3;
            }
            prim->x0 = *(u16*)&block->sxy0.vx;
            prim->y0 = *(u16*)&block->sxy0.vy;
            prim->x1 = *(u16*)&block->sxy1.vx;
            prim->y1 = *(u16*)&block->sxy1.vy;
            addPrim((u_long*)(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC) + (s32)gGpuCurrentOt),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, block->otz);
            mem->pos.vx = *(u16*)&coord->workm.t[0];
            mem->pos.vy = *(u16*)&coord->workm.t[1];
            mem->pos.vz = *(u16*)&coord->workm.t[2];
        }
    }
    *(void**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD + 0x20;
    mem->age++;
    if (mem->age > mem->scale * 8 - 1) {
        Gp_ReleaseState1CMem(mem, arg0);
    }
}

void Gp_EffPolyTask9C(Task* arg0)
{
    GpEffWork*     mem;
    GsCOORDINATE2* coord;
    s16            flag;

    mem   = arg0->spawnArg2;
    flag  = Gp_State1C->eventState;
    coord = (GsCOORDINATE2*)((TmdObject*)arg0->extra)->coords;
    if (flag < 4) {
        if (flag < 2) {
            if (arg0->state == 0) {
                mem->scale  = 0x10;
                mem->angle  = 0x20;
                mem->period = D_8011291C[arg0->spawnArg1].field_0;
                mem->step   = D_8011291C[arg0->spawnArg1].field_2;
                arg0->state++;
            }
            Gp_UpdateCoord(coord);
            mem->scale -= 2;
            mem->angle += mem->step;
            Gp_DrawEffShard(coord, mem->angle, mem->scale, mem->period);
            mem->age++;
            if (mem->age < 8) {
                return;
            }
        } else {
            return;
        }
    }
    Gp_ReleaseState1CMem(mem, arg0);
}

void Gp_DrawEffShard(GsCOORDINATE2* arg0, s16 arg1, s16 arg2, u16 arg3)
{
    void**                     scratch;
    register u8*               head asm("a2");
    GpEffBeamScratch*          block;
    register u16               vx asm("v0");
    register GpEffBeamScratch* vecp asm("v0");
    register s32*              otzp asm("v0");
    POLY_G4*                   quad;
    POLY_G3*                   tri;
    s32                        ang;
    s32                        ang2;
    s32                        rng;
    s32                        base;
    u8                         r;
    u8                         g;
    u8                         b;
    u16                        vz;

    scratch                                    = (void**)G_SCRATCH_HEAD;
    head                                       = *scratch;
    vx                                         = *(u16*)&arg0->workm.t[0];
    ((GpEffBeamScratch*)(head - 0x1C))->vec.vx = vx;
    vecp                                       = (GpEffBeamScratch*)(head - 0x1C);
    block                                      = vecp;
    block->vec.vy                              = *(u16*)&arg0->workm.t[1];
    vz                                         = *(u16*)&arg0->workm.t[2];
    *scratch                                   = block;
    block->vec.vz                              = vz;
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&block->vec);
    gte_rtps_real();
    gte_stsxy(&((GpEffBeamScratch*)(head - 0x1C))->sxy);
    gte_stflg(&((GpEffBeamScratch*)(head - 0x1C))->flag);
    if (block->flag >= 0) {
        otzp = &((GpEffBeamScratch*)(head - 0x1C))->otz;
        gte_stszotz(otzp);
        block->otz++;
        block->dx = (arg1 << 8) / block->otz;
        block->dy = (arg1 << 7) / block->otz;
        r         = arg2 * ((arg3 >> 8) & 0xF);
        g         = arg2 * ((arg3 >> 4) & 0xF);
        b         = arg2 * (arg3 & 0xF);
        ang       = 0;
        do {
            quad           = (POLY_G4*)gGpuPrimCursor;
            gGpuPrimCursor = quad + 1;
            setlen(quad, 8);
            setcode(quad, 0x38);
            quad->r0 = r;
            quad->r1 = r;
            quad->g0 = g;
            quad->b0 = b;
            quad->g1 = g;
            quad->b1 = b;
            quad->r2 = 0;
            quad->g2 = 0;
            quad->b2 = 0;
            quad->r3 = 0;
            quad->g3 = 0;
            quad->b3 = 0;
            quad->x0 = *(u16*)&block->sxy.vx + ((block->dx * rsin(ang)) >> 12);
            ang2     = ang + 0x100;
            quad->y0 = *(u16*)&block->sxy.vy + ((block->dx * rcos(ang)) >> 12);
            quad->x1 = *(u16*)&block->sxy.vx + ((block->dx * rsin(ang2)) >> 12);
            quad->y1 = *(u16*)&block->sxy.vy + ((block->dx * rcos(ang2)) >> 12);
            quad->x2 = *(u16*)&block->sxy.vx + ((block->dy * rsin(ang)) >> 12);
            quad->y2 = *(u16*)&block->sxy.vy + ((block->dy * rcos(ang)) >> 12);
            quad->x3 = *(u16*)&block->sxy.vx + ((block->dy * rsin(ang2)) >> 12);
            quad->y3 = *(u16*)&block->sxy.vy + ((block->dy * rcos(ang2)) >> 12);
            addPrim((u_long*)(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC) + (s32)gGpuCurrentOt),
                    quad);
            ang = ang2;
            Gp_AddTpageShift((P_TAG*)quad, 1, block->otz);
        } while (ang < 0x1000);
        if (arg3 & 0x1000) {
            block->dx = 0x12000 / block->otz;
            block->dy = 0x2400 / block->otz;
            ang       = 0;
            do {
                rng = Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
                tri               = (POLY_G3*)gGpuPrimCursor;
                gGpuPrimCursor    = tri + 1;
                setlen(tri, 6);
                setcode(tri, 0x30);
                tri->r0 = r;
                tri->g0 = g;
                tri->b0 = b;
                tri->r1 = 0;
                tri->g1 = 0;
                tri->b1 = 0;
                tri->r2 = 0;
                tri->g2 = 0;
                tri->b2 = 0;
                base    = ang + (((u32)rng >> 16) & 0x300);
                tri->x0 = *(u16*)&block->sxy.vx;
                tri->y0 = *(u16*)&block->sxy.vy;
                tri->x1 = *(u16*)&block->sxy.vx + ((block->dx * rsin(base)) >> 12) +
                          ((block->dy * rsin(base + 0xC00)) >> 12);
                tri->y1 = *(u16*)&block->sxy.vy + ((block->dx * rcos(base)) >> 12) +
                          ((block->dy * rcos(base + 0xC00)) >> 12);
                tri->x2 = *(u16*)&block->sxy.vx + ((block->dx * rsin(base)) >> 12) +
                          ((block->dy * rsin(base + 0x400)) >> 12);
                tri->y2 = *(u16*)&block->sxy.vy + ((block->dx * rcos(base)) >> 12) +
                          ((block->dy * rcos(base + 0x400)) >> 12);
                addPrim((u_long*)(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC) + (s32)gGpuCurrentOt),
                        tri);
                ang += 0x400;
                Gp_AddTpageShift((P_TAG*)tri, 1, block->otz);
            } while (ang < 0x1000);
        }
    }
    *(void**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD + 0x1C;
}

void Gp_EffSprTask9E(Task* arg0)
{
    register void**   scratch asm("a0");
    register u8*      head asm("v0");
    GpQuadScratch*    block;
    register SVECTOR* v asm("a1");
    s32               i;
    GpEffWork*        mem;
    GsCOORDINATE2*    coord;
    GpQuadCorner*     tbl;
    MATRIX*           m;
    POLY_FT4*         prim;
    s32               scale;
    s32               shade;
    u8                col;

    mem   = arg0->spawnArg2;
    coord = (GsCOORDINATE2*)((TmdObject*)arg0->extra)->coords;
    if (arg0->state == 0) {
        Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
        Gfx_RotMatrixY(&coord->coord, ((u32)Gp_LcgState >> 16) & 0xFFF, 1);
        coord->flg = 0;
        if (arg0->spawnArg1 & 0xFFF) {
            scale = ((GpEffSpawnArg*)&arg0->spawnArg1)->field_0 & 0xFFF;
        } else {
            scale = 0x400;
        }
        mem->scale  = scale;
        mem->angle  = 0x3FF;
        arg0->state = 1;
    }
    Gp_UpdateCoord(coord);

    scratch  = (void**)G_SCRATCH_HEAD;
    head     = (u8*)*scratch - 0x38;
    block    = (GpQuadScratch*)head;
    *scratch = block;
    i        = 0;
    m        = &coord->workm;
    v        = block->vec;
    tbl      = D_80111E38;
    do {
        v->vx = tbl->x * *(u16*)&mem->scale;
        v->vy = 0;
        v->vz = tbl->y * *(u16*)&mem->scale;
        gte_SetRotMatrix(m);
        gte_ldv0(v);
        gte_rtv0_real();
        gte_stsv(v);
        *(u16*)&v->vx = *(u16*)&v->vx + *(u16*)&coord->workm.t[0];
        tbl++;
        *(u16*)&v->vy = *(u16*)&v->vy + *(u16*)&coord->workm.t[1];
        i++;
        *(u16*)&v->vz = *(u16*)&v->vz + *(u16*)&coord->workm.t[2];
        v++;
    } while (i < 4);

    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&block->vec[0]);
    gte_rtps_real();
    shade = -0x80 - (mem->age >> 3);
    col   = shade;
    gte_stsxy(&block->sxy0);
    gte_ldv3(&block->vec[1], &block->vec[2], &block->vec[3]);
    gte_rtpt_real();
    gte_stsxy3(&block->sxy1, &block->sxy2, &block->sxy3);
    gte_stflg(&block->flag);
    if (block->flag >= 0) {
        gte_stszotz(&block->otz);
        block->otz    += 0x80;
        prim           = (POLY_FT4*)gGpuPrimCursor;
        gGpuPrimCursor = prim + 1;
        setlen(prim, 9);
        setcode(prim, 0x2E);
        prim->g0    = col >> 2;
        prim->b0    = col >> 2;
        prim->tpage = 0x29;
        prim->r0    = shade;
        prim->clut  = 0x428C;
        prim->u0    = 0xA8;
        prim->v0    = 0xC8;
        prim->u1    = 0xDF;
        prim->v1    = 0xC8;
        prim->u2    = 0xA8;
        prim->v2    = 0xFF;
        prim->u3    = 0xDF;
        prim->v3    = 0xFF;
        prim->x0    = block->sxy0.vx;
        prim->y0    = block->sxy0.vy;
        prim->x1    = block->sxy1.vx;
        prim->y1    = block->sxy1.vy;
        prim->x2    = block->sxy2.vx;
        prim->y2    = block->sxy2.vy;
        prim->x3    = block->sxy3.vx;
        prim->y3    = block->sxy3.vy;
        addPrim((u_long*)(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC) + (s32)gGpuCurrentOt),
                prim);
    }
    *(void**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD + 0x38;
    mem->age++;
    if (mem->angle < mem->age) {
        Gp_ReleaseState1CMem(mem, arg0);
    }
}

void Gp_EffSprTask54(Task* arg0)
{
    void**            scratch;
    u8*               head;
    GpEffBeamScratch* block;
    GpEffBeamScratch* vecp;
    s16               count;
    s16               step;
    u16               vz;
    GpEffWork*        mem;
    GsCOORDINATE2*    coord;
    POLY_FT4*         prim;

    mem   = arg0->spawnArg2;
    coord = (GsCOORDINATE2*)((TmdObject*)arg0->extra)->coords;
    if (Gp_State1C->eventState >= 2) {
        if (Gp_State1C->eventState >= 4) {
            Gp_ReleaseState1CMem(mem, arg0);
        }
        return;
    }

    Gp_UpdateCoord(coord);
    if (arg0->state == 0) {
        Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
        mem->scale  = ((GpEffSpawnArg*)&arg0->spawnArg1)->field_0 & 0xFFF;
        mem->angle  = ((u32)Gp_LcgState >> 16) & 0xFFF;
        if (arg0->spawnArg1 & 0xF000) {
            count = (arg0->spawnArg1 >> 12) & 0xF;
        } else {
            count = 1;
        }
        mem->period  = count;
        Gp_LcgState  = Gp_LcgState * 5 + 0x71357911;
        mem->move.vx = 0x10 - (((u32)Gp_LcgState >> 16) & 0x1F);
        Gp_LcgState  = Gp_LcgState * 5 + 0x71357911;
        mem->move.vy = -(((u32)Gp_LcgState >> 16) & 0xF);
        Gp_LcgState  = Gp_LcgState * 5 + 0x71357911;
        mem->move.vz = 0x10 - (((u32)Gp_LcgState >> 16) & 0x1F);
        if (arg0->spawnArg1 < 0) {
            if (mem->angle & 1) {
                Gp_SpawnEff(0x60054, coord, ((mem->scale * 3) >> 2) + 0x3000, NULL);
            }
            if (!(mem->angle & 3)) {
                Gp_SpawnEff(0x60054, coord, ((mem->scale * 3) >> 2) + 0x3000, NULL);
            }
        }
        arg0->state = 1;
    }

    scratch                                    = (void**)G_SCRATCH_HEAD;
    head                                       = *scratch;
    ((GpEffBeamScratch*)(head - 0x1C))->vec.vx = *(u16*)&coord->workm.t[0];
    block                                      = (GpEffBeamScratch*)(head - 0x1C);
    block->vec.vy                              = *(u16*)&coord->workm.t[1];
    vz                                         = *(u16*)&coord->workm.t[2];
    *scratch                                   = block;
    block->vec.vz                              = vz;
    vecp                                       = block;
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&vecp->vec);
    gte_rtps_real();
    gte_stsxy(&((GpEffBeamScratch*)(head - 0x1C))->sxy);
    gte_stflg(&((GpEffBeamScratch*)(head - 0x1C))->flag);
    if (block->flag >= 0) {
        gte_stszotz(&((GpEffBeamScratch*)(head - 0x1C))->otz);
        prim           = (POLY_FT4*)gGpuPrimCursor;
        gGpuPrimCursor = prim + 1;
        setlen(prim, 9);
        setcode(prim, 0x2C);
        prim->r0    = 0x68;
        prim->g0    = 0x70;
        prim->b0    = 0x38;
        prim->tpage = 0x28;
        setSemiTrans(prim, 1);
        prim->clut = 0x4253;
        prim->u0   = (mem->age / mem->period) << 5;
        prim->v0   = 0x18;
        prim->u1   = ((mem->age / mem->period) << 5) + 0x1F;
        prim->v1   = 0x18;
        prim->u2   = (mem->age / mem->period) << 5;
        prim->v2   = 0x37;
        prim->u3   = ((mem->age / mem->period) << 5) + 0x1F;
        prim->v3   = 0x37;

        block->dx = ((((s32)mem->scale * 31) / block->otz) * rsin(mem->angle)) >> 12;
        block->dy = ((((s32)mem->scale * 31) / block->otz) * rcos(mem->angle)) >> 12;
        prim->x0  = *(u16*)&block->sxy.vx + *(u16*)&block->dx;
        prim->x3  = *(u16*)&block->sxy.vx - *(u16*)&block->dx;
        prim->y0  = *(u16*)&block->sxy.vy - *(u16*)&block->dy;
        prim->y3  = *(u16*)&block->sxy.vy + *(u16*)&block->dy;
        block->dx = ((((s32)mem->scale * 31) / block->otz) * rsin(mem->angle + 0x400)) >> 12;
        block->dy = ((((s32)mem->scale * 31) / block->otz) * rcos(mem->angle + 0x400)) >> 12;
        prim->x1  = *(u16*)&block->sxy.vx + *(u16*)&block->dx;
        prim->x2  = *(u16*)&block->sxy.vx - *(u16*)&block->dx;
        prim->y1  = *(u16*)&block->sxy.vy - *(u16*)&block->dy;
        prim->y2  = *(u16*)&block->sxy.vy + *(u16*)&block->dy;
        addPrim((u_long*)(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC) + (s32)gGpuCurrentOt),
                prim);
    }
    *scratch = (u8*)*scratch + 0x1C;
    if (Gp_State1C->eventState == 0) {
        coord->coord.t[0] += mem->move.vx;
        coord->coord.t[1] += mem->move.vy;
        coord->coord.t[2] += mem->move.vz;
        coord->flg         = 0;
        step               = mem->age + 1;
        mem->age           = step;
        if (step > (mem->period * 8) - 1) {
            Gp_ReleaseState1CMem(mem, arg0);
        }
    }
}

void Gp_DrawEffSprite7C(GsCOORDINATE2* arg0, s32 arg1, u32 arg2)
{
    void**            scratch;
    register u8*      head asm("v1");
    GpQuadScratch*    block;
    register SVECTOR* v asm("a3");
    s32               i;
    GpQuadCorner*     tbl;
    POLY_FT4*         prim;

    scratch  = (void**)G_SCRATCH_HEAD;
    head     = (u8*)*scratch - 0x38;
    block    = (GpQuadScratch*)head;
    *scratch = head;
    gte_SetTransMatrix(&GsWSMATRIX);
    i   = 0;
    v   = block->vec;
    tbl = D_80111E38;
    do {
        v->vx = tbl->x * arg1;
        v->vy = 0;
        v->vz = tbl->y * arg1;
        gte_SetRotMatrix(&Gfx_ViewWorldMtx);
        gte_ldv0(v);
        gte_rtv0_real();
        gte_stsv(v);
        *(u16*)&v->vx = *(u16*)&v->vx + *(u16*)&arg0->workm.t[0];
        tbl++;
        *(u16*)&v->vy = *(u16*)&v->vy + *(u16*)&arg0->workm.t[1];
        i++;
        *(u16*)&v->vz = *(u16*)&v->vz + *(u16*)&arg0->workm.t[2];
        v++;
    } while (i < 4);

    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&block->vec[0]);
    gte_rtps_real();
    gte_stsxy(&block->sxy0);
    gte_ldv3(&block->vec[1], &block->vec[2], &block->vec[3]);
    gte_rtpt_real();
    gte_stsxy3(&block->sxy1, &block->sxy2, &block->sxy3);
    gte_stflg(&block->flag);
    if (block->flag >= 0) {
        gte_stszotz(&block->otz);
        block->otz++;
        prim           = (POLY_FT4*)gGpuPrimCursor;
        gGpuPrimCursor = prim + 1;
        setlen(prim, 9);
        setcode(prim, 0x2C);
        prim->r0    = arg2 >> 1;
        prim->tpage = 0x29;
        prim->clut  = 0x430F;
        prim->u0    = 0xE0;
        prim->v0    = 0xC8;
        prim->v1    = 0xC8;
        prim->u2    = 0xE0;
        prim->g0    = arg2;
        prim->b0    = arg2;
        prim->u1    = 0xFF;
        prim->v2    = 0xE7;
        prim->u3    = 0xFF;
        prim->v3    = 0xE7;
        setSemiTrans(prim, 1);
        prim->x0 = block->sxy0.vx;
        prim->y0 = block->sxy0.vy;
        prim->x1 = block->sxy1.vx;
        prim->y1 = block->sxy1.vy;
        prim->x2 = block->sxy2.vx;
        prim->y2 = block->sxy2.vy;
        prim->x3 = block->sxy3.vx;
        prim->y3 = block->sxy3.vy;
        addPrim((u_long*)(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC) + (s32)gGpuCurrentOt),
                prim);
    }
    *(void**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD + 0x38;
}

void Gp_DrawEffGroundQuad(VECTOR3* arg0, s32 arg1, s16 arg2)
{
    void**            scratch;
    register u8*      head asm("v1");
    GpQuadScratch*    block;
    register SVECTOR* v asm("a2");
    s32               i;
    GpQuadCorner*     tbl;
    POLY_FT4*         prim;

    if (arg2 >= 0 && Gp_State1C->eventState < 2) {
        scratch  = (void**)G_SCRATCH_HEAD;
        head     = (u8*)*scratch - 0x38;
        block    = (GpQuadScratch*)head;
        *scratch = head;
        gte_SetTransMatrix(&GsWSMATRIX);
        i   = 0;
        v   = block->vec;
        tbl = D_80111E38;
        do {
            v->vx = tbl->x * arg1;
            v->vy = 0;
            v->vz = tbl->y * arg1;
            gte_SetRotMatrix(&Gfx_ViewWorldMtx);
            gte_ldv0(v);
            gte_rtv0_real();
            gte_stsv(v);
            *(u16*)&v->vx = *(u16*)&v->vx + *(u16*)&arg0->vx;
            tbl++;
            *(u16*)&v->vy = *(u16*)&v->vy + *(u16*)&arg0->vy;
            i++;
            *(u16*)&v->vz = *(u16*)&v->vz + *(u16*)&arg0->vz;
            v++;
        } while (i < 4);

        gte_SetRotMatrix(&GsWSMATRIX);
        gte_ldv0(&block->vec[0]);
        gte_rtps_real();
        gte_stsxy(&block->sxy0);
        gte_ldv3(&block->vec[1], &block->vec[2], &block->vec[3]);
        gte_rtpt_real();
        gte_stsxy3(&block->sxy1, &block->sxy2, &block->sxy3);
        gte_stflg(&block->flag);
        if (block->flag >= 0) {
            gte_stszotz(&block->otz);
            prim           = (POLY_FT4*)gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setlen(prim, 9);
            setcode(prim, 0x2C);
            if (arg2 == 0) {
                setcode(prim, 0x2D);
            } else {
                prim->r0 = arg2;
                prim->g0 = arg2;
                prim->b0 = arg2;
            }
            prim->tpage = 0x48;
            prim->clut  = 0x4283;
            prim->u0    = 0xC0;
            prim->v0    = 0x98;
            prim->v1    = 0x98;
            prim->u2    = 0xC0;
            prim->u1    = 0xF7;
            prim->v2    = 0xCF;
            prim->u3    = 0xF7;
            prim->v3    = 0xCF;
            setSemiTrans(prim, 1);
            prim->x0 = block->sxy0.vx;
            prim->y0 = block->sxy0.vy;
            prim->x1 = block->sxy1.vx;
            prim->y1 = block->sxy1.vy;
            prim->x2 = block->sxy2.vx;
            prim->y2 = block->sxy2.vy;
            prim->x3 = block->sxy3.vx;
            prim->y3 = block->sxy3.vy;
            addPrim((u_long*)(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC) + (s32)gGpuCurrentOt),
                    prim);
        }
        *(void**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD + 0x38;
    }
}

void Gp_EffSprTask53(Task* arg0)
{
    VECTOR3        vec;
    Task*          slot;
    GsCOORDINATE2* coord;
    GsCOORDINATE2* parent;

    slot  = gameGetPtrSlot(3);
    coord = (GsCOORDINATE2*)((TmdObject*)arg0->extra)->coords;
    if (slot != NULL) {
        if (arg0->state == 0) {
            parent     = (GsCOORDINATE2*)((TmdObject*)slot->extra)->coords;
            coord->flg = 0;
            coord->sub = parent + 1;
            Gp_UpdateCoord(coord);
            arg0->state = 1;
        } else if (Gp_State1C->groundShade >= 0) {
            if (!(((TmdObject*)slot->extra)->flags & 0x80)) {
                Gp_UpdateCoord(coord);
                if ((s16)func_800EA1A8((VECTOR3*)coord->workm.t, &vec) != 0) {
                    Gp_DrawEffGroundQuad(&vec, 0x1C0, Gp_State1C->groundShade);
                }
            }
        }
    }
}

void Gp_EffAttachTask37(Task* arg0)
{
    SVECTOR        delta;
    SVECTOR        dir;
    SVECTOR        pos;
    VECTOR         scale2;
    VECTOR         scale;
    TmdObject*     extra;
    GpEffWork*     mem;
    GsCOORDINATE2* coord;
    GsCOORDINATE2* player;
    SVECTOR*       rot;
    MATRIX*        mtx;
    s32            state;
    s16            flag;
    s16            trans;
    s32            temp;
    u16            tx;
    u16            ty;
    u16            tz;
    s32            dz;

    extra  = (TmdObject*)arg0->extra;
    mem    = arg0->spawnArg2;
    coord  = (GsCOORDINATE2*)extra->coords;
    player = (GsCOORDINATE2*)((TmdObject*)(gameGetPtrSlot(3))->extra)->coords;
    flag   = Gp_State1C->eventState;
    if (flag != 0) {
        if (flag < 4) {
            return;
        }
        goto release;
    }
    Gp_UpdateCoord(coord);
    mem->age++;
    state = arg0->state;
    switch (state) {
        case 0:
            extra->flags &= 0xFF7F;
            mem->scale    = 0x100;
            if (arg0->spawnArg1 & 0xFFF) {
                temp = ((GpEffSpawnArg*)&arg0->spawnArg1)->field_0 & 0xFFF;
            } else {
                temp = 0x200;
            }
            mem->angle   = temp;
            mem->period  = 0x800;
            Gp_LcgState  = Gp_LcgState * 5 + 0x71357911;
            mem->move.vx = 0x800 - (((u32)Gp_LcgState >> 16) & 0xFFF);
            Gp_LcgState  = Gp_LcgState * 5 + 0x71357911;
            mem->move.vy = 0x400 - (((u32)Gp_LcgState >> 16) % 0xC00);
            Gp_LcgState  = Gp_LcgState * 5 + 0x71357911;
            mem->move.vz = 0x800 - (((u32)Gp_LcgState >> 16) & 0xFFF);
            VectorNormalSS(&mem->move, &mem->move);
            Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
            mem->pos.vx = 0x100 - (((u32)Gp_LcgState >> 16) & 0x1FF);
            Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
            mem->pos.vy = 0x100 - (((u32)Gp_LcgState >> 16) & 0x1FF);
            Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
            mem->pos.vz = 0x100 - (((u32)Gp_LcgState >> 16) & 0x1FF);
            coord->flg  = 0;
            Gp_UpdateCoord(coord);
            arg0->state = 1;
            func_800D7A9C(extra, (VECTOR*)coord->workm.t, 0, 3);
            return;
        case 1:
            mtx = &coord->coord;
            rot = &mem->pos;
            Gfx_RotMatrixXYZ(mtx, rot, 0);
            MatrixNormal(mtx, mtx);
            gte_lddp(*(u16*)&mem->scale);
            gte_ldsv(&mem->move);
            gte_gpf12_real();
            gte_stsv(&delta);
            coord->coord.t[0] += delta.vx;
            coord->coord.t[1] += delta.vy;
            coord->coord.t[2] += delta.vz;
            coord->flg         = 0;
            gte_SetRotMatrix(&Gfx_ViewWorldMtx);
            gte_ldv0(&delta);
            gte_rtv0_real();
            gte_stsv(&dir);
            tx             = *(u16*)&coord->workm.t[0];
            pos.vx         = tx;
            ty             = *(u16*)&coord->workm.t[1];
            pos.vy         = ty;
            tz             = *(u16*)&coord->workm.t[2];
            *(u16*)&dir.vx = *(u16*)&dir.vx + tx;
            *(u16*)&dir.vy = *(u16*)&dir.vy + ty;
            pos.vz         = tz;
            *(u16*)&dir.vz = *(u16*)&dir.vz + tz;
            if (func_800DE7CC(&dir, &pos, &dir, &pos) == state) {
                SVECTOR*          vel;
                register SVECTOR* r0 asm("a0");
                vel = &mem->move;
                r0  = vel;
                USE_REG(r0);
                coord->coord.t[0] -= delta.vx;
                coord->coord.t[1] -= delta.vy;
                coord->coord.t[2] -= delta.vz;
                COMPILER_BARRIER();
                {
                    u16          t10;
                    u16          t11;
                    register s32 t12 asm("a1");
                    s32          sum;
                    t10 = *(volatile u16*)&pos.vx;
                    t11 = *(volatile u16*)&mem->move;
                    t12 = *(volatile u16*)&mem->move.vy;
                    sum = ((s32)(t10 << 16) >> 17) + ((s32)(t11 << 16) >> 17);
                    USE_REG(t12);
                    mem->move.vx = sum;
                    t12        <<= 16;
                    t12        >>= 17;
                    COMPILER_BARRIER();
                    mem->move.vy = *(u16*)&pos.vy + t12;
                }
                dz           = (s32)(*(u16*)&mem->move.vz << 16) >> 17;
                mem->move.vz = ((s32)(*(u16*)&pos.vz << 16) >> 17) + dz;
                VectorNormalSS(vel, vel);
                mem->scale = (s32)(*(u16*)&mem->scale << 16) >> 17;
                gte_lddp(mem->scale);
                gte_ldsv(vel);
                gte_gpf12_real();
                gte_stsv(&delta);
                coord->coord.t[0] += delta.vx;
                coord->coord.t[1] += delta.vy;
                coord->coord.t[2] += delta.vz;
                coord->flg         = 0;
                Gp_UpdateCoord(coord);
                if (!(mem->age & 3)) {
                    func_800D7A9C(extra, (VECTOR*)coord->workm.t, 0, 3);
                }
                Gp_SpawnEff(0x60055, coord, mem->angle + 0x12200, 0);
                gte_lddp(0x800);
                gte_ldsv(rot);
                gte_gpf12_real();
                gte_stsv(rot);
                if (((s16)mem->age - mem->step) < 8 && mem->scale < 0x20) {
                    extra->flags |= 2;
                    mem->age      = 0;
                    arg0->state   = 2;
                    return;
                }
                mem->step = mem->age;
                return;
            }
            if (mem->scale == 0) {
                return;
            }
            if (mem->age >= 0x4C) {
                goto release;
            }
            if (player->coord.t[1] + 0x100 < coord->coord.t[1]) {
                mem->age += 0xA;
            }
            Gp_UpdateCoord(coord);
            if (!(mem->age & 3)) {
                func_800D7A9C(extra, (VECTOR*)coord->workm.t, 0, 3);
            }
            mem->move.vy += 0x10000 / mem->scale;
            Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
            if (!(((u32)Gp_LcgState >> 16) & 3)) {
                Gp_SpawnEff(0x60042, coord, mem->angle + 0x11000, 0);
            }
            Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
            if (!(((u32)Gp_LcgState >> 16) & 7)) {
                Gp_SpawnEff(0x60055, coord, mem->angle + 0x11000, 0);
            }
            if (mem->age >= 0x33) {
                extra->flags |= 2;
                if (mem->period >= 0x41) {
                    trans       = mem->period - 0x40;
                    mem->period = trans;
                    Gp_SetObjTrans((GpObj20*)extra, trans, trans, trans);
                    return;
                }
            }
            return;
        case 2:
            Gp_UpdateCoord(coord);
            if (!(mem->age & 3)) {
                func_800D7A9C(extra, (VECTOR*)coord->workm.t, 0, 3);
            }
            if (mem->age >= 0x10) {
                goto release;
            }
            memset(&scale, 0, 0x10);
            scale.vx = 0x1000;
            scale.vy = (0x10 - mem->age) << 8;
            scale.vz = 0x1000;
            scale2   = scale;
            ScaleMatrix(&coord->coord, &scale2);
            coord->flg = 0;
            if (mem->period >= 0x81) {
                trans       = mem->period - 0x80;
                mem->period = trans;
                Gp_SetObjTrans((GpObj20*)extra, trans, trans, trans);
            }
            if (mem->age == 8) {
                Gp_SpawnEff(0x600A5, coord, mem->angle >= 0x100, 0);
            }
            return;
    }
    return;
release:
    Gp_ReleaseState1CMem(mem, arg0);
}

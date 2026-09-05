#include "common.h"

#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>

#include "gameplay/3CD8.h"
#include "gameplay/3FB8.h"
#include "main/mem.h"
#include "main/session.h"
#include "main/sound.h"
#include "main/task.h"
#include "weapons/hypervelocity.h"

/// Scratchpad stack pointer, initialised by GameMain (see src/main/gamemain.c).
#define SCRATCH_SP (*(u32*)G_SCRATCH_HEAD)

void WeaponsShared8011db78(Task* task);
void func_hypervelocity_8011EC1C(GsCOORDINATE2* coord, s16 age, s32 radius, u8* rgb);

void func_hypervelocity_8011F168(Task* arg0)
{
    GpEffWork*     mem;
    GsCOORDINATE2* coord;
    s16            flag;
    s16            val;
    u8             rgb[3];

    mem   = arg0->spawnArg2;
    flag  = Gp_State1C->field_4;
    coord = (GsCOORDINATE2*)((TmdObject*)arg0->extra)->field_8;
    if (flag != 0) {
        if (flag < 4) {
            return;
        }
        Gp_ReleaseState1CMem(mem, arg0);
        return;
    }

    Gp_UpdateCoord(coord);
    mem->field_22++;
    if (arg0->state == 0) {
        mem->field_24 = 0xF0;
        mem->field_26 = 0x100;
        arg0->state   = 1;
    }
    rgb[0] = mem->field_24 >> 1;
    rgb[1] = mem->field_24 >> 1;
    rgb[2] = mem->field_24;
    Gp_DrawBand(coord, mem->field_26, rgb);
    mem->field_26 += 0x40;
    val            = mem->field_24 - 0x10;
    mem->field_24  = val;
    if (val < 0x10) {
        Gp_ReleaseState1CMem(mem, arg0);
    }
}

void func_hypervelocity_8011F270(Task* arg0)
{
    GpEffWork*     mem;
    GsCOORDINATE2* coord;
    s16            flag;
    s16            val;
    u8             rgb[3];

    mem   = arg0->spawnArg2;
    flag  = Gp_State1C->field_4;
    coord = (GsCOORDINATE2*)((TmdObject*)arg0->extra)->field_8;
    if (flag != 0) {
        if (flag < 4) {
            return;
        }
        Gp_ReleaseState1CMem(mem, arg0);
        return;
    }

    Gp_UpdateCoord(coord);
    mem->field_22++;
    if (arg0->state == 0) {
        mem->field_24 = 0x80;
        mem->field_26 = 0x200;
        arg0->state   = 1;
    }
    rgb[0] = mem->field_24;
    rgb[1] = mem->field_24;
    rgb[2] = mem->field_24;
    func_hypervelocity_8011EC1C(coord, mem->field_22, mem->field_26, rgb);
    mem->field_26 += 0x60;
    val            = mem->field_24 - 8;
    mem->field_24  = val;
    if (val < 6) {
        Gp_ReleaseState1CMem(mem, arg0);
    }
}

void func_hypervelocity_8011F374(Task* arg0)
{
    Task*        parent;
    TmdObject*   extra;
    TmdObject*   playerExtra;
    HyperCoord*  coord;
    GpActorWork* work;
    HyperMat*    mat;
    s16          count;

    parent      = arg0->parent;
    work        = (GpActorWork*)Game_GetPtrSlot(3);
    extra       = (TmdObject*)arg0->extra;
    playerExtra = work->extra;
    coord       = (HyperCoord*)extra->field_8;

    coord->flg      = 0;
    extra->field_C  = playerExtra->field_C;
    extra->field_20 = playerExtra->field_20;
    extra->field_1C = playerExtra->field_1C;

    SCRATCH_SP -= 0x10;
    switch (arg0->spawnArg1 & 0xF) {
        case 0:
            if (*(u32*)&work->actor->field_954 != 0x40000) {
                arg0->spawnArg1 = 0;
            }
            break;
        case 1:
            if (parent->spawnArg1 & 0x10) {
                if (arg0->killCountdown < 0x3C) {
                    arg0->killCountdown = arg0->killCountdown + 1;
                }
            } else if (arg0->killCountdown > 0) {
                count               = arg0->killCountdown - 1;
                arg0->killCountdown = count;
                if (count == 0) {
                    SndEvt_EnqueueType7(0x20160004, 1);
                }
            }
            coord->coord.mat.t[0] = 0;
            coord->coord.mat.t[1] = -arg0->killCountdown * 4;
            coord->coord.mat.t[2] = -0x16;
            break;
        case 2:
            if (parent->spawnArg1 & 0x20) {
                if (coord->angle >= -0x3FF) {
                    coord->angle = coord->angle - 0x110;
                }
            } else if (coord->angle < 0) {
                coord->angle = coord->angle + 0x110;
            }
            coord->coord.mat.t[0] = -0x14;
            coord->coord.mat.t[1] = -0x15C;
            coord->coord.mat.t[2] = 0xA8;

            mat                = &coord->coord;
            mat->ident.m00_m01 = 0x1000;
            mat->ident.m02_m10 = 0;
            mat->ident.m11_m12 = 0x1000;
            mat->ident.m20_m21 = 0;
            mat->ident.m22     = 0x1000;
            RotMatrixX(coord->angle, &mat->mat);
            break;
    }
    SCRATCH_SP += 0x10;
}

void func_hypervelocity_8011F570(Task* arg0)
{
    Task*          child;
    TmdObject*     childExtra;
    TmdObject*     extra;
    GsCOORDINATE2* coord;

    extra               = (TmdObject*)arg0->extra;
    coord               = (GsCOORDINATE2*)extra->field_8;
    arg0->state        += 1;
    arg0->exitCallback  = WeaponsShared8011db78;
    arg0->killCountdown = 0;
    coord->flg          = 0;
    extra->field_C      = 0;
    if (!(arg0->spawnArg1 & 0xF)) {
        child = Task_Spawn(7, 0x70, 1, 0);
        if (child != NULL) {
            ((GsCOORDINATE2*)((TmdObject*)child->extra)->field_8)->sub = coord;
            childExtra                                                 = (TmdObject*)child->extra;
            childExtra->field_20                                       = extra->field_20;
            childExtra->field_1C                                       = extra->field_1C;
            Task_Reparent(arg0, child);
        }
        child = Task_Spawn(7, 0x74, 2, 0);
        if (child != NULL) {
            ((GsCOORDINATE2*)((TmdObject*)child->extra)->field_8)->sub = coord;
            childExtra                                                 = (TmdObject*)child->extra;
            childExtra->field_20                                       = extra->field_20;
            childExtra->field_1C                                       = extra->field_1C;
            Task_Reparent(arg0, child);
            coord->coord.t[0] = -6;
            coord->coord.t[1] = -0x3C;
            coord->coord.t[2] = -0x16;
        }
    }
}

void func_hypervelocity_8011F694(Task* arg0)
{
    arg0->state = 3;
}

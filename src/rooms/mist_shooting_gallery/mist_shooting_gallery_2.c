#include "common.h"

#include "gameplay/1A8.h"
#include "gameplay/268.h"
#include "gameplay/3CD8.h"
#include "gameplay/D4.h"
#include "gameplay/gameplay.h"

#include "main/display.h"
#include "main/fs.h"
#include "main/gameflag.h"
#include "main/mc.h"
#include "main/mem.h"
#include "main/pad.h"
#include "main/session.h"
#include "main/sound.h"
#include "main/stream.h"
#include "main/task.h"
#include "main/text.h"
#include "main/ui.h"

#include "rooms/mist_shooting_gallery.h"

#include <psyq/libgte.h>

extern s8                        D_8007272D;
extern s8                        D_80073BAE;
extern s16                       D_80114D08;
extern s32                       D_8014D038;
extern TaskDesc                  D_8014E13C;
extern s32                       D_80153274;
extern s32                       D_80153D6C;
extern s32                       func_8014AA54(GpSaveLoc* loc);
extern void                      func_8014AB6C(void);
extern void                      func_8014AF0C(void);
extern MistShootingGalleryLayout D_mist_shooting_gallery_80185198;
extern MistShootingGalleryLayout D_mist_shooting_gallery_801851F8;
extern MistShootingGalleryLayout D_mist_shooting_gallery_80189968;
extern TaskDesc                  D_mist_shooting_gallery_801850DC;

s32 func_mist_shooting_gallery_8017FEB8(Task* task, s32 msgId, GpSaveLoc* src, GpSaveLoc* dst)
{
    *dst = *src;
    if (*(u16*)src == 0x13 && src->field_5 == 0) {
        if (GameFlag_GetNibble(0x7A) != 0) {
            dst->field_3 += 2;
        }
    }
    if (*(u16*)src == 0x14) {
        if (dst->field_2 == 5 && func_8014AA54(src) == 2) {
            return 2;
        }
        if (src->field_5 == 0) {
            if (dst->field_2 == 6) {
                D_8007272D             = 2;
                D_80073BAE             = 4;
                Game_Session->field_68 = 1;
                Gp_ResetInventory();
            }
            if (dst->field_2 == 5) {
                D_8007272D             = 1;
                D_80073BAE             = 3;
                Game_Session->field_68 = 1;
                Gp_ClearInventory();
            }
        }
    }
    return 1;
}

s32 func_mist_shooting_gallery_80180000(s32 arg0, s32 arg1, s32 arg2)
{
    if (arg2 >= 5) {
        if (arg2 >= 9) {
            if (arg2 < 0x23) {
                if (arg2 >= 0x21) {
                    Gp_MsgPlayerWeapon(0);
                    Task_SpawnFromTable(&D_mist_shooting_gallery_801850DC, 0, arg2, 3);
                }
            }
        } else {
            Gp_MsgPlayerWeapon(0);
            Task_SpawnFromTable(&D_mist_shooting_gallery_801850DC, 0, arg2, 1);
        }
    }
    return 0;
}

s32 func_mist_shooting_gallery_8018008C(Task* task, s32 msgId, GpMsg13EF* arg2)
{
    if ((arg2->field_2 == 1) && (D_8014D038 == 0)) {
        Gp_MsgPlayerWeapon(0);
        Task_SpawnFromTable(&D_8014E13C, 1, 1, 0);
        D_80114D08 = 0xA;
    }
    if ((arg2->field_2 == 2) && (GameFlag_GetNibble(0xED) == 0)) {
        func_8014AF0C();
    }
    if (arg2->field_2 == 3) {
        func_8014AB6C();
    }
    if ((arg2->field_2 == 4) && (GameFlag_GetNibble(0x106) == 0)) {
        func_800E3FAC(0xA2, 0x3B);
        GameFlag_SetNibble(0x106, 1);
        func_800E8634((s32)&D_80153274, 0, (s32)&D_80153D6C);
    }
    return 0;
}

INCLUDE_ASM("rooms/nonmatchings/mist_shooting_gallery/mist_shooting_gallery_2", func_mist_shooting_gallery_8018018C);

void func_mist_shooting_gallery_801801E4(s32 arg0)
{
    MistShootingGalleryLayout* dst = &D_mist_shooting_gallery_80189968;
    MistShootingGalleryLayout* src = &D_mist_shooting_gallery_80185198;
    MistShootingGalleryPos     ofs;
    s32                        i;

    for (i = 0; i < 3; i++) {
        dst->positions[i].x = src->positions[i].x;
        dst->positions[i].y = src->positions[i].y;
        dst->positions[i].z = src->positions[i].z;
        dst->links[i]       = src->links[i];
    }
    for (i = 0; i < 8; i++) {
        dst->targets[i].x = src->targets[i].x;
        dst->targets[i].y = src->targets[i].y;
        dst->targets[i].z = src->targets[i].z;
    }
    if (arg0 == 0) {
        ofs.x = 0;
        ofs.y = 0;
    } else {
        ofs.x = 0;
        ofs.y = 0xBB8;
    }
    ofs.z = 0;
    for (i = 0; i < 8; i++) {
        dst->targets[i].x += ofs.x;
        dst->targets[i].y += ofs.y;
        dst->targets[i].z += ofs.z;
    }
}

void func_mist_shooting_gallery_80180390(s32 arg0)
{
    MistShootingGalleryLayout* dst    = &D_mist_shooting_gallery_80189968;
    MistShootingGalleryLayout* src    = &D_mist_shooting_gallery_801851F8;
    MistShootingGalleryLink*   dlinks = &D_mist_shooting_gallery_80189968.links[3];
    MistShootingGalleryLink*   slinks = D_mist_shooting_gallery_801851F8.links;
    MistShootingGalleryPos     ofs;
    s32                        i;
    s32                        j;

    for (i = 0; i < 1; i++) {
        dst->positions[i + 3].x = src->positions[i].x;
        dst->positions[i + 3].y = src->positions[i].y;
        dst->positions[i + 3].z = src->positions[i].z;
        for (j = 0; j < 4; j++) {
            dlinks->field_00[j] = slinks->field_00[j] + 8;
        }
        dlinks->field_08 = slinks->field_08 + 3;
        dlinks->field_0A = slinks->field_0A;
        dlinks++;
        slinks++;
    }
    for (i = 0; i < 4; i++) {
        dst->targets[i + 8].x = src->targets[i].x;
        dst->targets[i + 8].y = src->targets[i].y;
        dst->targets[i + 8].z = src->targets[i].z;
    }
    if (arg0 == 0) {
        ofs.x = 0;
        ofs.y = 0;
    } else {
        ofs.x = 0;
        ofs.y = 0xFA0;
    }
    ofs.z = 0;
    for (i = 0; i < 8; i++) {
        dst->targets[i + 8].x += ofs.x;
        dst->targets[i + 8].y += ofs.y;
        dst->targets[i + 8].z += ofs.z;
    }
}

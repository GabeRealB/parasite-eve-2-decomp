#include "gameplay/loading.h"

#include <psyq/libgpu.h>

#include "common.h"

#include "gameplay/area.h"
#include "companion_load.h"
#include "gameplay/hud_sprites.h"
#include "gameplay/items.h"
#include "loading.h"

#include "main/display.h"
#include "main/fs.h"
#include "main/loadui.h"
#include "main/mc.h"
#include "main/pad.h"
#include "main/scratch.h"
#include "main/session.h"
#include "main/stage.h"
#include "main/task.h"
#include "main/tmd.h"
#include "main/wipsys.h"

/// 5-byte table at `Gp_ConfigCdTable`. `Gp_EnqueueConfigCd` copies it to the stack and
/// indexes it 1-based by `Player_Status.field_26`; the byte is CdCmd 0x21
/// param2[0].
typedef struct _GpTbl5 {
    /* 0x0 */ u8 field_0[5];
} GpTbl5;
STATIC_ASSERT_SIZEOF(GpTbl5, 5);

extern GpAreaRec D_8010CBE4[21];

/// End marker following the stage 1 room table.
extern u32 D_8010CC8C[2];

extern GpAreaRec D_8010CC94[39];

/// End marker following the stage 2 room table.
extern u32 D_8010CDCC[2];

extern GpAreaRec D_8010CDD4[39];

/// End marker following the stage 3 room table.
extern u32 D_8010CF0C[2];

extern GpAreaRec D_8010CF14[50];

/// End marker following the stage 4 room table.
extern u32 D_8010D0A4[2];

extern GpAreaRec D_8010D0AC[34];

/// End marker following the stage 5 room table.
extern u32 D_8010D1BC[2];

static const TaskFuncTable6 Gp_LoadWaitFns;

static const GpTbl5 Gp_ConfigCdTable;

/// Maps `Player_Status.weapon` / `field_22` (and the 0x1B attach id) to a
/// CdCmd 0x21 payload. No-op when `field_21` is 0 or the mapped byte is 0.
static void Gp_EnqueueWeaponCd(void);

static void Gp_LoadWaitCdBusy(Task* task);

static void Gp_LoadWaitIdle(Task* task);

static void Gp_LoadWaitDone(Task* task);

static void Gp_ReloadFromSave(void);

static void Gp_ReloadAtLoc(s32 arg0);

extern GpAreaVariant D_80185E50[];

extern GpAreaObj D_80073410;

extern GpAreaVariant D_80186C50[];

extern GpAreaObj D_80073414;

extern GpAreaVariant D_80184A90[];

extern GpAreaObj D_80073418;

extern GpAreaVariant D_80189DCC[];

extern GpAreaObj D_8007341C;

extern GpAreaVariant D_80199390[];

extern GpAreaObj D_80073420;

extern GpAreaVariant D_80184088[];

extern GpAreaObj D_80073424;

extern GpAreaVariant D_8017EA3C[];

extern GpAreaObj D_80073428;

extern GpAreaVariant D_8017FC9C[];

extern GpAreaObj D_8007342C;

extern GpAreaVariant D_801831C4[];

extern GpAreaObj D_80073430;

extern GpAreaVariant D_80181264[];

extern GpAreaObj D_80073434;

extern GpAreaVariant D_80183020[];

extern GpAreaObj D_80073438;

extern GpAreaVariant D_8018402C[];

extern GpAreaObj D_8007343C;

extern GpAreaVariant D_80185934[];

extern GpAreaObj D_80073440;

extern GpAreaVariant D_8019004C[];

extern GpAreaObj D_80073444;

extern GpAreaVariant D_8018294C[];

extern GpAreaObj D_80073448;

extern GpAreaVariant D_801861E8[];

extern GpAreaObj D_8007344C;

extern GpAreaVariant D_80186BFC[];

extern GpAreaObj D_80073450;

extern GpAreaVariant D_801951B4[];

extern GpAreaObj D_80073454;

extern GpAreaVariant D_8018DF74[];

extern GpAreaObj D_8007355C;

extern GpAreaVariant D_80184A38[];

extern GpAreaObj D_800734E8;

extern GpAreaVariant D_80184F20[];

extern GpAreaObj D_800734EC;

extern GpAreaVariant D_80185654[];

extern GpAreaObj D_800734F0;

extern GpAreaVariant D_80180A50[];

extern GpAreaObj D_800734F4;

extern GpAreaVariant D_8017F598[];

extern GpAreaObj D_800734F8;

extern GpAreaVariant D_80182100[];

extern GpAreaObj D_800734FC;

extern GpAreaVariant D_80180B88[];

extern GpAreaObj D_8007356C;

extern GpAreaVariant D_80189938[];

extern GpAreaObj D_80073500;

extern GpAreaVariant D_801814DC[];

extern GpAreaObj D_80073504;

extern GpAreaVariant D_80180410[];

extern GpAreaObj D_80073508;

extern GpAreaVariant D_8017FA74[];

extern GpAreaObj D_80073514;

extern GpAreaVariant D_80182918[];

extern GpAreaObj D_80073518;

extern GpAreaVariant D_80181B1C[];

extern GpAreaObj D_8007351C;

extern GpAreaVariant D_8017F4B8[];

extern GpAreaObj D_80073520;

extern GpAreaVariant D_8018757C[];

extern GpAreaObj D_80073524;

extern GpAreaVariant D_80188BF0[];

extern GpAreaObj D_80073528;

extern GpAreaVariant D_801842F8[];

extern GpAreaObj D_8007352C;

extern GpAreaVariant D_801800E0[];

extern GpAreaObj D_80073530;

extern GpAreaVariant D_8017EDD4[];

extern GpAreaObj D_80073534;

extern GpAreaVariant D_8017F558[];

extern GpAreaObj D_80073538;

extern GpAreaVariant D_801876F0[];

extern GpAreaObj D_8007353C;

extern GpAreaVariant D_80186220[];

extern GpAreaObj D_80073544;

extern GpAreaVariant D_80186764[];

extern GpAreaObj D_80073548;

extern GpAreaVariant D_801827BC[];

extern GpAreaObj D_80073550;

extern GpAreaVariant D_80180ACC[];

extern GpAreaObj D_80073554;

extern GpAreaVariant D_8017F868[];

extern GpAreaObj D_80073558;

extern GpAreaVariant D_80190624[];

extern GpAreaVariant D_80188A08[];

extern GpAreaVariant D_8018572C[];

extern GpAreaVariant D_80181518[];

extern GpAreaVariant D_8017F62C[];

extern GpAreaVariant D_8017FB98[];

extern GpAreaVariant D_801818D8[];

extern GpAreaVariant D_80189FA4[];

extern GpAreaVariant D_8018075C[];

extern GpAreaVariant D_801809A0[];

extern GpAreaVariant D_80180D24[];

extern GpAreaObj D_8007350C;

extern GpAreaVariant D_801802FC[];

extern GpAreaObj D_80073510;

extern GpAreaVariant D_80181438[];

extern GpAreaVariant D_8017F354[];

extern GpAreaVariant D_801843C4[];

extern GpAreaObj D_80073570;

extern GpAreaVariant D_80188EE4[];

extern GpAreaVariant D_8017EB88[];

extern GpAreaVariant D_80182B5C[];

extern GpAreaVariant D_80180764[];

extern GpAreaVariant D_801803E4[];

extern GpAreaVariant D_8018A70C[];

extern GpAreaObj D_80073574;

extern GpAreaVariant D_801874BC[];

extern GpAreaVariant D_80181F4C[];

extern GpAreaVariant D_801843F0[];

extern GpAreaVariant D_8018C15C[];

extern GpAreaVariant D_80181190[];

extern GpAreaObj D_80073540;

extern GpAreaVariant D_8018EA94[];

extern GpAreaVariant D_801861B4[];

extern GpAreaVariant D_80180888[];

extern GpAreaObj D_8007354C;

extern GpAreaVariant D_80183418[];

extern GpAreaVariant D_80180780[];

extern GpAreaVariant D_801802DC[];

extern GpAreaVariant D_801898F4[];

extern GpAreaObj D_80073690;

extern GpAreaVariant D_8018E238[];

extern GpAreaObj D_80073694;

extern GpAreaVariant D_8017F32C[];

extern GpAreaObj D_80073698;

extern GpAreaVariant D_801801CC[];

extern GpAreaObj D_8007369C;

extern GpAreaVariant D_80183544[];

extern GpAreaObj D_800736A0;

extern GpAreaVariant D_80182A00[];

extern GpAreaObj D_800736A4;

extern GpAreaVariant D_80185504[];

extern GpAreaObj D_800736A8;

extern GpAreaVariant D_80183340[];

extern GpAreaObj D_800736AC;

extern GpAreaVariant D_80184940[];

extern GpAreaObj D_800736B0;

extern GpAreaVariant D_80183598[];

extern GpAreaObj D_800736B4;

extern GpAreaVariant D_80186C9C[];

extern GpAreaObj D_800736B8;

extern GpAreaVariant D_80185A38[];

extern GpAreaObj D_800736BC;

extern GpAreaVariant D_801854E0[];

extern GpAreaObj D_800736C0;

extern GpAreaVariant D_80183FDC[];

extern GpAreaObj D_800736C4;

extern GpAreaVariant D_80185C30[];

extern GpAreaObj D_800736C8;

extern GpAreaVariant D_8018C14C[];

extern GpAreaObj D_800736CC;

extern GpAreaVariant D_80184C00[];

extern GpAreaObj D_800736D0;

extern GpAreaVariant D_80183A98[];

extern GpAreaObj D_800736D4;

extern GpAreaVariant D_8017FE50[];

extern GpAreaObj D_800736D8;

extern GpAreaVariant D_8018B5C4[];

extern GpAreaObj D_800736DC;

extern GpAreaVariant D_8017F17C[];

extern GpAreaObj D_800736E0;

extern GpAreaVariant D_80187678[];

extern GpAreaObj D_800736E4;

extern GpAreaVariant D_8017FBF8[];

extern GpAreaObj D_800736E8;

extern GpAreaVariant D_801830B8[];

extern GpAreaObj D_800736EC;

extern GpAreaVariant D_801825AC[];

extern GpAreaObj D_800736F0;

extern GpAreaVariant D_8017E964[];

extern GpAreaObj D_800736F4;

extern GpAreaVariant D_80184C7C[];

extern GpAreaObj D_800736F8;

extern GpAreaVariant D_801837AC[];

extern GpAreaObj D_800736FC;

extern GpAreaVariant D_80184124[];

extern GpAreaObj D_80073700;

extern GpAreaVariant D_80186258[];

extern GpAreaObj D_80073704;

extern GpAreaVariant D_80186360[];

extern GpAreaObj D_80073708;

extern GpAreaVariant D_80183EEC[];

extern GpAreaObj D_8007370C;

extern GpAreaVariant D_8018933C[];

extern GpAreaObj D_80073710;

extern GpAreaVariant D_80186F40[];

extern GpAreaObj D_80073714;

extern GpAreaVariant D_801855AC[];

extern GpAreaObj D_80073718;

extern GpAreaVariant D_8017FA40[];

extern GpAreaObj D_8007371C;

extern GpAreaVariant D_8018EC3C[];

extern GpAreaObj D_80073728;

extern GpAreaVariant D_8018FA58[];

extern GpAreaObj D_8007372C;

extern GpAreaVariant D_80182610[];

extern GpAreaObj D_80073730;

extern GpAreaVariant D_8018477C[];

extern GpAreaObj D_80073734;

extern GpAreaVariant D_80183D48[];

extern GpAreaObj D_80073738;

extern GpAreaVariant D_80188B9C[];

extern GpAreaObj D_8007373C;

extern GpAreaVariant D_80187350[];

extern GpAreaObj D_80073740;

extern GpAreaVariant D_80184CA4[];

extern GpAreaObj D_80073744;

extern GpAreaVariant D_80187CB8[];

extern GpAreaObj D_80073748;

extern GpAreaVariant D_8018BC10[];

extern GpAreaObj D_8007374C;

extern GpAreaVariant D_8017DD74[];

extern GpAreaObj D_80073858;

extern GpAreaVariant D_80182A04[];

extern GpAreaObj D_8007385C;

extern GpAreaVariant D_80180DA8[];

extern GpAreaObj D_80073860;

extern GpAreaVariant D_80182BF4[];

extern GpAreaObj D_80073864;

extern GpAreaVariant D_8017F758[];

extern GpAreaObj D_80073868;

extern GpAreaVariant D_8018786C[];

extern GpAreaObj D_80073870;

extern GpAreaVariant D_801806B8[];

extern GpAreaObj D_80073874;

extern GpAreaVariant D_8018305C[];

extern GpAreaObj D_8007387C;

extern GpAreaVariant D_80182968[];

extern GpAreaObj D_80073880;

extern GpAreaVariant D_80187470[];

extern GpAreaObj D_80073884;

extern GpAreaVariant D_801876C4[];

extern GpAreaObj D_80073888;

extern GpAreaVariant D_80183F48[];

extern GpAreaObj D_8007388C;

extern GpAreaVariant D_80182B54[];

extern GpAreaObj D_80073890;

extern GpAreaVariant D_80182DE0[];

extern GpAreaObj D_80073894;

extern GpAreaVariant D_80181AF8[];

extern GpAreaObj D_80073898;

extern GpAreaVariant D_80180864[];

extern GpAreaObj D_8007389C;

extern GpAreaVariant D_801808E4[];

extern GpAreaObj D_800738A0;

extern GpAreaVariant D_801866C8[];

extern GpAreaObj D_800738A8;

extern GpAreaVariant D_801874A4[];

extern GpAreaObj D_800738AC;

extern GpAreaVariant D_80180338[];

extern GpAreaVariant D_80180304[];

extern GpAreaObj D_800738B4;

extern GpAreaVariant D_801859DC[];

extern GpAreaObj D_800738B8;

extern GpAreaVariant D_8017E994[];

extern GpAreaObj D_800738BC;

extern GpAreaVariant D_80184A50[];

extern GpAreaObj D_800738C0;

extern GpAreaVariant D_80184230[];

extern GpAreaObj D_800738C4;

extern GpAreaVariant D_8018471C[];

extern GpAreaObj D_800738C8;

extern GpAreaVariant D_80185860[];

extern GpAreaObj D_800738CC;

extern GpAreaVariant D_8017DBB8[];

extern GpAreaObj D_800738D0;

extern GpAreaVariant D_80181728[];

extern GpAreaObj D_800738D4;

GpAreaRec* Gp_AreaTables[6] = { NULL, D_8010CBE4, D_8010CC94, D_8010CDD4, D_8010CF14, D_8010D0AC };
GpAreaRec  D_8010CBE4[21]   = {
    { NULL, NULL },
    { D_80185E50, &D_80073410 },
    { D_80186C50, &D_80073414 },
    { D_80184A90, &D_80073418 },
    { D_80189DCC, &D_8007341C },
    { D_80199390, &D_80073420 },
    { D_80184088, &D_80073424 },
    { D_8017EA3C, &D_80073428 },
    { D_8017FC9C, &D_8007342C },
    { D_801831C4, &D_80073430 },
    { D_80181264, &D_80073434 },
    { D_80183020, &D_80073438 },
    { D_8018402C, &D_8007343C },
    { D_80185934, &D_80073440 },
    { D_8019004C, &D_80073444 },
    { D_8018294C, &D_80073448 },
    { D_801861E8, &D_8007344C },
    { NULL, NULL },
    { D_80186BFC, &D_80073450 },
    { D_801951B4, &D_80073454 },
    { D_8018DF74, &D_8007355C },
};
/// End marker following the stage 1 room table.
u32       D_8010CC8C[2]  = { 0xFFFF, 0 };
GpAreaRec D_8010CC94[39] = {
    { NULL, NULL },
    { D_80184A38, &D_800734E8 },
    { D_80184F20, &D_800734EC },
    { D_80185654, &D_800734F0 },
    { NULL, NULL },
    { D_80180A50, &D_800734F4 },
    { D_8017F598, &D_800734F8 },
    { D_80182100, &D_800734FC },
    { D_80180B88, &D_8007356C },
    { D_80189938, &D_80073500 },
    { NULL, NULL },
    { D_801814DC, &D_80073504 },
    { D_80180410, &D_80073508 },
    { NULL, NULL },
    { NULL, NULL },
    { D_8017FA74, &D_80073514 },
    { D_80182918, &D_80073518 },
    { NULL, NULL },
    { D_80181B1C, &D_8007351C },
    { D_8017F4B8, &D_80073520 },
    { D_8018757C, &D_80073524 },
    { D_80188BF0, &D_80073528 },
    { D_801842F8, &D_8007352C },
    { NULL, NULL },
    { D_801800E0, &D_80073530 },
    { D_8017EDD4, &D_80073534 },
    { D_8017F558, &D_80073538 },
    { D_801876F0, &D_8007353C },
    { NULL, NULL },
    { D_80186220, &D_80073544 },
    { D_80186764, &D_80073548 },
    { NULL, NULL },
    { D_801827BC, &D_80073550 },
    { NULL, NULL },
    { D_80180ACC, &D_80073554 },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { D_8017F868, &D_80073558 },
};
/// End marker following the stage 2 room table.
u32       D_8010CDCC[2]  = { 0xFFFF, 0 };
GpAreaRec D_8010CDD4[39] = {
    { NULL, NULL },
    { D_80190624, &D_800734E8 },
    { D_80188A08, &D_800734EC },
    { D_8018572C, &D_800734F0 },
    { NULL, NULL },
    { D_80181518, &D_800734F4 },
    { D_8017F62C, &D_800734F8 },
    { D_8017FB98, &D_800734FC },
    { D_801818D8, &D_8007356C },
    { D_80189FA4, &D_80073500 },
    { NULL, NULL },
    { D_8018075C, &D_80073504 },
    { D_801809A0, &D_80073508 },
    { D_80180D24, &D_8007350C },
    { D_801802FC, &D_80073510 },
    { D_80181438, &D_80073514 },
    { D_8017F354, &D_80073518 },
    { D_801843C4, &D_80073570 },
    { D_80188EE4, &D_8007351C },
    { D_8017EB88, &D_80073520 },
    { D_80182B5C, &D_80073524 },
    { D_80180764, &D_80073528 },
    { D_801803E4, &D_8007352C },
    { D_8018A70C, &D_80073574 },
    { D_801874BC, &D_80073530 },
    { D_80181F4C, &D_80073534 },
    { D_801843F0, &D_80073538 },
    { D_8018C15C, &D_8007353C },
    { D_80181190, &D_80073540 },
    { D_8018EA94, &D_80073544 },
    { D_801861B4, &D_80073548 },
    { D_80180888, &D_8007354C },
    { D_80183418, &D_80073550 },
    { NULL, NULL },
    { D_80180780, &D_80073554 },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { D_801802DC, &D_80073558 },
};
/// End marker following the stage 3 room table.
u32       D_8010CF0C[2]  = { 0xFFFF, 0 };
GpAreaRec D_8010CF14[50] = {
    { NULL, NULL },
    { D_801898F4, &D_80073690 },
    { D_8018E238, &D_80073694 },
    { D_8017F32C, &D_80073698 },
    { D_801801CC, &D_8007369C },
    { D_80183544, &D_800736A0 },
    { D_80182A00, &D_800736A4 },
    { D_80185504, &D_800736A8 },
    { D_80183340, &D_800736AC },
    { D_80184940, &D_800736B0 },
    { D_80183598, &D_800736B4 },
    { D_80186C9C, &D_800736B8 },
    { D_80185A38, &D_800736BC },
    { D_801854E0, &D_800736C0 },
    { D_80183FDC, &D_800736C4 },
    { D_80185C30, &D_800736C8 },
    { D_8018C14C, &D_800736CC },
    { D_80184C00, &D_800736D0 },
    { D_80183A98, &D_800736D4 },
    { D_8017FE50, &D_800736D8 },
    { D_8018B5C4, &D_800736DC },
    { D_8017F17C, &D_800736E0 },
    { D_80187678, &D_800736E4 },
    { D_8017FBF8, &D_800736E8 },
    { D_801830B8, &D_800736EC },
    { D_801825AC, &D_800736F0 },
    { D_8017E964, &D_800736F4 },
    { D_80184C7C, &D_800736F8 },
    { D_801837AC, &D_800736FC },
    { D_80184124, &D_80073700 },
    { D_80186258, &D_80073704 },
    { D_80186360, &D_80073708 },
    { D_80183EEC, &D_8007370C },
    { D_8018933C, &D_80073710 },
    { D_80186F40, &D_80073714 },
    { D_801855AC, &D_80073718 },
    { D_8017FA40, &D_8007371C },
    { NULL, NULL },
    { NULL, NULL },
    { D_8018EC3C, &D_80073728 },
    { D_8018FA58, &D_8007372C },
    { D_80182610, &D_80073730 },
    { D_8018477C, &D_80073734 },
    { D_80183D48, &D_80073738 },
    { D_80188B9C, &D_8007373C },
    { D_80187350, &D_80073740 },
    { D_80184CA4, &D_80073744 },
    { D_80187CB8, &D_80073748 },
    { D_8018BC10, &D_8007374C },
    { D_8017DD74, &D_800736A4 },
};
/// End marker following the stage 4 room table.
u32       D_8010D0A4[2]  = { 0xFFFF, 0 };
GpAreaRec D_8010D0AC[34] = {
    { NULL, NULL },
    { D_801818D8, &D_80073858 },
    { D_80182A04, &D_8007385C },
    { D_80180DA8, &D_80073860 },
    { D_80182BF4, &D_80073864 },
    { D_8017F758, &D_80073868 },
    { NULL, NULL },
    { D_8018786C, &D_80073870 },
    { D_801806B8, &D_80073874 },
    { NULL, NULL },
    { D_8018305C, &D_8007387C },
    { D_80182968, &D_80073880 },
    { D_80187470, &D_80073884 },
    { D_801876C4, &D_80073888 },
    { D_80183F48, &D_8007388C },
    { D_80182B54, &D_80073890 },
    { D_80182DE0, &D_80073894 },
    { D_80181AF8, &D_80073898 },
    { D_80180864, &D_8007389C },
    { D_801808E4, &D_800738A0 },
    { NULL, NULL },
    { D_801866C8, &D_800738A8 },
    { D_801874A4, &D_800738AC },
    { D_80180338, &D_800738AC },
    { D_80180304, &D_800738B4 },
    { D_801859DC, &D_800738B8 },
    { D_8017E994, &D_800738BC },
    { D_80184A50, &D_800738C0 },
    { D_80184230, &D_800738C4 },
    { D_8018471C, &D_800738C8 },
    { D_80185860, &D_800738CC },
    { D_8017DBB8, &D_800738D0 },
    { D_80181728, &D_800738D4 },
    { NULL, NULL },
};
/// End marker following the stage 5 room table.
u32 D_8010D1BC[2] = { 0xFFFF, 0 };

const TaskFuncTable3 Gp_SessionStates;
const TaskFuncTable8 Gp_LoadStateFns;
const TaskFuncTable3 Gp_RoomObjStates;

static const TaskFuncTable6 Gp_LoadWaitFns = { {
    Gp_ViewBeginLoad,
    Gp_EnqueueViewCd,
    Gp_ViewLoadImage,
    Gp_LoadWaitCdBusy,
    Gp_LoadWaitIdle,
    Gp_LoadWaitDone,
} };

static const GpTbl5 Gp_ConfigCdTable = { { 4, 3, 2, 5, 6 } };

/// Maps `Player_Status.weapon` / `field_22` (and the 0x1B attach id) to a
/// CdCmd 0x21 payload. No-op when `field_21` is 0 or the mapped byte is 0.
static void Gp_EnqueueWeaponCd(void)
{
    u8  param1[8];
    u8  param2[8];
    u16 item;
    s32 val;
    s32 attach;
    s32 flag;

    item = Player_Status.weapon;
    if (item == 0) {
        return;
    }

    param1[0] = 0;
    switch (item) {
        case 0xB:
            param1[0] = 1;
            if (Player_Status.weaponSlotItem == 0xB) {
                param1[0] = 2;
            }
            if (Player_Status.weaponSlotItem == 0xC) {
                param1[0] = 3;
            }
            break;
        case 0xC:
            param1[0] = 4;
            if (Player_Status.weaponSlotItem == 0xB) {
                param1[0] = 5;
            }
            if (Player_Status.weaponSlotItem == 0xC) {
                param1[0] = 6;
            }
            break;
        case 0xD:
            param1[0] = 7;
            if (Player_Status.weaponSlotItem == 0xE) {
                param1[0] = 8;
            }
            if (Player_Status.weaponSlotItem == 0xF) {
                param1[0] = 9;
            }
            break;
        case 0xE:
            param1[0] = 0xA;
            if (Player_Status.weaponSlotItem == 0xE) {
                param1[0] = 0xB;
            }
            if (Player_Status.weaponSlotItem == 0xF) {
                param1[0] = 0xC;
            }
            break;
        case 0xF:
            param1[0] = 0xD;
            val       = Player_Status.weaponSlotItem;
            if (val == 0xE) {
                param1[0] = val;
            }
            if (val == 0xF) {
                param1[0] = val;
            }
            break;
        case 0x17:
            param1[0] = 0x13;
            if (Player_Status.weaponSlotItem == 0xE) {
                param1[0] = 0x14;
            }
            if (Player_Status.weaponSlotItem == 0xF) {
                param1[0] = 0x15;
            }
            break;
        case 0x1B: {
            McItemSlot* slot;

            param1[0] = 0x10;
            slot      = Gp_GetItemSlot(item + 0x7F);
            if (slot->attachId != 0 && slot->attachId != 0xFF) {
                attach = slot->attachId - 0x9F;
                if (attach == 0xB) {
                    param1[0] = 0x11;
                }
                if (attach == 0xC) {
                    param1[0] = 0x12;
                }
            }
            break;
        }
    }

    if (param1[0] == 0) {
        return;
    }

    flag      = 1;
    param1[3] = 0;
    param1[2] = flag;
    param2[0] = 0xA;
    param2[3] = 0;
    param2[2] = 0;
    param2[1] = 0;
    CdCmd_Enqueue(0x21, param1, param2);
    D_800626E8 = flag;
}

void Gp_EnqueueViewCd(Task* task)
{
    GpAreaKey* sess;
    u8         param1[8];
    u8         param2[8];

    sess = &gGameSession->at4.loc;
    if (CdCmd_IsIdle() & 0xFFFF) {
        param1[3] = sess->stage;
        param1[2] = sess->area;
        param1[0] = Gp_GetViewIndex();
        param2[0] = 1;
        param2[1] = 0;
        param2[2] = 0;
        param2[3] = 0;
        CdCmd_Enqueue(0x21, param1, param2);
        task->state++;
    }
}

static void Gp_LoadWaitCdBusy(Task* task)
{
    if (CdCmd_Queue.field_1FA != 0) {
        task->killCountdown++;
    }
    if (task->killCountdown >= 3) {
        task->state = -1;
        Gp_FinishLoadWait(task);
    }
}

static void Gp_LoadWaitIdle(Task* task)
{
    if (CdCmd_IsIdle() & 0xFFFF) {
        task->state = -2;
        Gp_FinishLoadWait(task);
    }
}

static void Gp_LoadWaitDone(Task* task)
{
    if (CdCmd_Queue.field_1FE == 0xFF) {
        task->state = -1;
        Gp_FinishLoadWait(task);
    }
}

void Gp_LoadViewImages(void)
{
    u8 view;
    u8 i;

    view = Gp_GetViewIndex();
    for (i = 0; i < 50; i++) {
        if (D_8006C338[i].field_0 == 2) {
            if (view - 1 == i) {
                while (Fs_LoadImageChunk(D_8006C338[i].field_4, 1)) {
                }
                break;
            }
        }
    }
}

void Gp_FinishLoadWait(Task* task)
{
    Pad_ClearCooldown(0);
    if (task->spawnArg1 == 0) {
        Stage_RequestSpecialFlag(1);
        gGameSession->viewDirty = 0;
        taskKill(task);
        Display_ResetHeapWrapper();
    } else {
        if (task->spawnArg1 == 1) {
            gDisplayState.at100.flags.flipMode = 1;
        }
        gDisplayState.at100.flags.imageSource = 2;
        Task_Spawn(0, 0x17, 0, 0);
        gGameSession->viewReady = 1;
        taskKill(task);
    }
}

void Gp_LoadWaitDispatch(Task* task)
{
    TaskFuncTable6 sp;

    sp = Gp_LoadWaitFns;
    Pad_SetCooldown(0);
    if (task->state < 0) {
        Gp_FinishLoadWait(task);
    } else {
        sp.funcs[task->state](task);
    }
}

static void Gp_ReloadFromSave(void)
{
    Task*       slot;
    McSaveData* save;

    slot            = gameGetPtrSlot(1);
    save            = &Mc_SaveData[0];
    slot->spawnArg1 = save->at4.loc.view;
    ResetGraph(1);
    Gpu_ClearOTag(0);
    Gpu_ClearOTag(1);
    gGameSession->at4.loc.view = save->at4.loc.view;
    Pad_SetCooldown(0);
    Gp_SpawnCurView(2);
    gGameSession->viewReady = 0;
    Task_Spawn(0, 0x1E, 1, 0);
}

static void Gp_ReloadAtLoc(s32 arg0)
{
    Task* slot;

    slot                        = gameGetPtrSlot(1);
    Mc_SaveData[0].at4.loc.view = arg0;
    gGameSession->at4.loc.view  = arg0;
    slot->spawnArg1             = (u8)arg0;
    Pad_SetCooldown(0);
    Gp_SpawnCurView(1);
    gDisplayState.at100.flags.imageSource = 1;
    Task_Spawn(0, 0x1E, 0, 0);
}

void Gp_CommitSpawnLoc(Task* task)
{
    u8 val;

    val                         = (u8)task->spawnArg1;
    Mc_SaveData[0].at4.loc.view = val;
    gGameSession->at4.loc.view  = val;
    taskKill(task);
}

void func_800A99B4(void)
{
    Display_SpawnWithOtSmall(0, 0x26, 0, 0);
}

void Gp_SetupSprtDisplay(Task* task)
{
    DisplayState* ds;
    s32           flag;

    ds                       = &gDisplayState;
    flag                     = ds->keepGraphics;
    ds->at100.flags.flipMode = 2;
    if (flag == 0) {
        Gpu_ResetGraphAndOt();
        Tmd_AllocMissingBuffers();
    }
    Gp_AllocSprtLists();
    taskKill(task);
    Display_ResetHeapWrapper();
}

void Gp_LoadViewAndCd(u8 arg0)
{
    u8           view;
    u8           i;
    GameSession* session;
    u8           param2[8];
    u8           param1[8];

    view = Gp_GetViewIndex();
    for (i = 0; i < 50; i++) {
        if (D_8006C338[i].field_0 == 2) {
            if (view - 1 == i) {
                while (Fs_LoadImageChunk(D_8006C338[i].field_4, 1)) {
                }
                break;
            }
        }
    }
    session   = gGameSession;
    param1[3] = session->at4.loc.stage;
    param1[2] = session->at4.loc.area;
    param1[0] = Gp_GetViewIndex();
    param2[0] = 1;
    if (arg0 != 0) {
        param2[1] = 4;
    } else {
        param2[1] = 0;
    }
    param2[3] = 0;
    param2[2] = 0;
    CdCmd_Enqueue(0x21, param1, param2);
}

void Gp_EnqueueConfigCd(s32 arg0)
{
    u8     param1[8];
    u8     param2[8];
    GpTbl5 table;

    table = Gp_ConfigCdTable;
    if (Mc_SaveData[0].characterId != 0) {
        param1[3] = 0;
        param1[2] = 1;
        param1[0] = 0;
        param2[0] = table.field_0[Player_Status.field_26 - 1];
        if ((u8)arg0 == 0) {
            param2[1] = 0;
        } else {
            param2[1] = 5;
        }
        param2[2] = 6;
        param2[3] = 0;
        CdCmd_Enqueue(0x21, param1, param2);
    }
}

void Gp_EnqueueHeldWeaponCd(void)
{
    u8  param1[8];
    u8  param2[8];
    u8  val;
    s32 flag;

    val = Player_Status.weapon;
    if (val == 0) {
        val = 1;
    }
    flag      = 1;
    param1[0] = val;
    param1[3] = 0;
    param1[2] = flag;
    param2[0] = 3;
    param2[3] = 0;
    param2[2] = 0;
    param2[1] = 0;
    CdCmd_Enqueue(0x21, param1, param2);
    D_800626E8 = flag;
    Gp_EnqueueWeaponCd();
}

void Gp_EnqueueStageCd(void)
{
    u8 param1[8];
    u8 param2[8];

    CdCmd_Enqueue(0x54, &gGameSession->at4.loc.view, NULL);
    param1[3] = 0;
    param1[2] = 0x5A;
    param1[0] = gGameSession->at4.loc.stage;
    param2[3] = 0;
    param2[2] = 0;
    param2[1] = 0;
    param2[0] = 0;
    CdCmd_Enqueue(0x21, param1, param2);
}

void Gp_EnqueueCompanionCd(u8 type, u8 variant)
{
    u8  param2[4];
    u8* param1;

    if (type == 0) {
        return;
    }

    param1                 = SCRATCH_PUSH_BYTES(8);
    gGameSession->field_80 = 0;
    param1[3]              = 0;
    param1[2]              = 0x50;
    param1[0]              = 0;
    param2[0]              = type;
    param2[1]              = 0;
    param2[2]              = 4;
    param2[3]              = 6;
    CdCmd_Enqueue(0x21, param1, param2);

    if (variant != 0) {
        param1[3] = 0;
        param1[2] = 0x50;
        param1[0] = variant;
        param2[0] = type;
        param2[1] = 0;
        param2[2] = 4;
        param2[3] = 6;
        CdCmd_Enqueue(0x21, param1, param2);
        if (variant == 5) {
            gGameSession->companionVariant  = 3;
            Mc_SaveData[0].companionVariant = 3;
        }
    }

    SCRATCH_POP_BYTES(8);
}

void Gp_PumpTmdStream(Task* task)
{
    TmdObject* obj;

    obj = task->extra.tmd;
    if (task->spawnType == 1) {
        obj->tpage = 4;
        obj->clut  = 6;
        if (obj->buffer != NULL) {
            tmdProcessStream(obj);
            tmdProcessStream(obj);
        }
    }
}

const TaskFuncTable3 Gp_SessionStates = { {
    Gp_ResumeSessionTask,
    Gp_SessionState1,
    Gp_BeginSessionTask,
} };
const TaskFuncTable8 Gp_LoadStateFns  = { {
    Gp_LoadWaitBoot,
    Gp_LoadWaitStage,
    Gp_LoadState2,
    Gp_LoadWaitCompanion,
    Gp_LoadWaitSave,
    Gp_LoadWaitAreaCd,
    Gp_FadeGrayHold,
    Gp_LoadFinishTask,
} };
const TaskFuncTable3 Gp_RoomObjStates = { {
    Gp_LinkRoomObjectsSpawn,
    Gp_RoomObjState1,
    taskKill,
} };

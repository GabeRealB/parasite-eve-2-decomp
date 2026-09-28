#include "gameplay/loading.h"

#include <psyq/libgte.h>
#include <psyq/libgpu.h>

#include "common.h"

#include "gameplay/area.h"
#include "companion_load.h"
#include "gameplay/hud_sprites.h"
#include "gameplay/items.h"
#include "loading.h"

#include "main/display.h"
#include "main/fs.h"
#include "main/gameflag.h"
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

extern GpAreaVariant D_80186C50[];

extern GpAreaVariant D_80184A90[];

extern GpAreaVariant D_80189DCC[];

extern GpAreaVariant D_80199390[];

extern GpAreaVariant D_80184088[];

extern GpAreaVariant D_8017EA3C[];

extern GpAreaVariant D_8017FC9C[];

extern GpAreaVariant D_801831C4[];

extern GpAreaVariant D_80181264[];

extern GpAreaVariant D_80183020[];

extern GpAreaVariant D_8018402C[];

extern GpAreaVariant D_80185934[];

extern GpAreaVariant D_8019004C[];

extern GpAreaVariant D_8018294C[];

extern GpAreaVariant D_801861E8[];

extern GpAreaVariant D_80186BFC[];

extern GpAreaVariant D_801951B4[];

extern GpAreaVariant D_8018DF74[];

extern GpAreaVariant D_80184A38[];

extern GpAreaVariant D_80184F20[];

extern GpAreaVariant D_80185654[];

extern GpAreaVariant D_80180A50[];

extern GpAreaVariant D_8017F598[];

extern GpAreaVariant D_80182100[];

extern GpAreaVariant D_80180B88[];

extern GpAreaVariant D_80189938[];

extern GpAreaVariant D_801814DC[];

extern GpAreaVariant D_80180410[];

extern GpAreaVariant D_8017FA74[];

extern GpAreaVariant D_80182918[];

extern GpAreaVariant D_80181B1C[];

extern GpAreaVariant D_8017F4B8[];

extern GpAreaVariant D_8018757C[];

extern GpAreaVariant D_80188BF0[];

extern GpAreaVariant D_801842F8[];

extern GpAreaVariant D_801800E0[];

extern GpAreaVariant D_8017EDD4[];

extern GpAreaVariant D_8017F558[];

extern GpAreaVariant D_801876F0[];

extern GpAreaVariant D_80186220[];

extern GpAreaVariant D_80186764[];

extern GpAreaVariant D_801827BC[];

extern GpAreaVariant D_80180ACC[];

extern GpAreaVariant D_8017F868[];

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

extern GpAreaVariant D_801802FC[];

extern GpAreaVariant D_80181438[];

extern GpAreaVariant D_8017F354[];

extern GpAreaVariant D_801843C4[];

extern GpAreaVariant D_80188EE4[];

extern GpAreaVariant D_8017EB88[];

extern GpAreaVariant D_80182B5C[];

extern GpAreaVariant D_80180764[];

extern GpAreaVariant D_801803E4[];

extern GpAreaVariant D_8018A70C[];

extern GpAreaVariant D_801874BC[];

extern GpAreaVariant D_80181F4C[];

extern GpAreaVariant D_801843F0[];

extern GpAreaVariant D_8018C15C[];

extern GpAreaVariant D_80181190[];

extern GpAreaVariant D_8018EA94[];

extern GpAreaVariant D_801861B4[];

extern GpAreaVariant D_80180888[];

extern GpAreaVariant D_80183418[];

extern GpAreaVariant D_80180780[];

extern GpAreaVariant D_801802DC[];

extern GpAreaVariant D_801898F4[];

extern GpAreaVariant D_8018E238[];

extern GpAreaVariant D_8017F32C[];

extern GpAreaVariant D_801801CC[];

extern GpAreaVariant D_80183544[];

extern GpAreaVariant D_80182A00[];

extern GpAreaVariant D_80185504[];

extern GpAreaVariant D_80183340[];

extern GpAreaVariant D_80184940[];

extern GpAreaVariant D_80183598[];

extern GpAreaVariant D_80186C9C[];

extern GpAreaVariant D_80185A38[];

extern GpAreaVariant D_801854E0[];

extern GpAreaVariant D_80183FDC[];

extern GpAreaVariant D_80185C30[];

extern GpAreaVariant D_8018C14C[];

extern GpAreaVariant D_80184C00[];

extern GpAreaVariant D_80183A98[];

extern GpAreaVariant D_8017FE50[];

extern GpAreaVariant D_8018B5C4[];

extern GpAreaVariant D_8017F17C[];

extern GpAreaVariant D_80187678[];

extern GpAreaVariant D_8017FBF8[];

extern GpAreaVariant D_801830B8[];

extern GpAreaVariant D_801825AC[];

extern GpAreaVariant D_8017E964[];

extern GpAreaVariant D_80184C7C[];

extern GpAreaVariant D_801837AC[];

extern GpAreaVariant D_80184124[];

extern GpAreaVariant D_80186258[];

extern GpAreaVariant D_80186360[];

extern GpAreaVariant D_80183EEC[];

extern GpAreaVariant D_8018933C[];

extern GpAreaVariant D_80186F40[];

extern GpAreaVariant D_801855AC[];

extern GpAreaVariant D_8017FA40[];

extern GpAreaVariant D_8018EC3C[];

extern GpAreaVariant D_8018FA58[];

extern GpAreaVariant D_80182610[];

extern GpAreaVariant D_8018477C[];

extern GpAreaVariant D_80183D48[];

extern GpAreaVariant D_80188B9C[];

extern GpAreaVariant D_80187350[];

extern GpAreaVariant D_80184CA4[];

extern GpAreaVariant D_80187CB8[];

extern GpAreaVariant D_8018BC10[];

extern GpAreaVariant D_8017DD74[];

extern GpAreaVariant D_80182A04[];

extern GpAreaVariant D_80180DA8[];

extern GpAreaVariant D_80182BF4[];

extern GpAreaVariant D_8017F758[];

extern GpAreaVariant D_8018786C[];

extern GpAreaVariant D_801806B8[];

extern GpAreaVariant D_8018305C[];

extern GpAreaVariant D_80182968[];

extern GpAreaVariant D_80187470[];

extern GpAreaVariant D_801876C4[];

extern GpAreaVariant D_80183F48[];

extern GpAreaVariant D_80182B54[];

extern GpAreaVariant D_80182DE0[];

extern GpAreaVariant D_80181AF8[];

extern GpAreaVariant D_80180864[];

extern GpAreaVariant D_801808E4[];

extern GpAreaVariant D_801866C8[];

extern GpAreaVariant D_801874A4[];

extern GpAreaVariant D_80180338[];

extern GpAreaVariant D_80180304[];

extern GpAreaVariant D_801859DC[];

extern GpAreaVariant D_8017E994[];

extern GpAreaVariant D_80184A50[];

extern GpAreaVariant D_80184230[];

extern GpAreaVariant D_8018471C[];

extern GpAreaVariant D_80185860[];

extern GpAreaVariant D_8017DBB8[];

extern GpAreaVariant D_80181728[];

GpAreaRec* Gp_AreaTables[6] = { NULL, D_8010CBE4, D_8010CC94, D_8010CDD4, D_8010CF14, D_8010D0AC };
GpAreaRec  D_8010CBE4[21]   = {
    { NULL, NULL },
    { D_80185E50, &GameFlag_AcropolisBanks[0].areas[0].state },
    { D_80186C50, &GameFlag_AcropolisBanks[0].areas[1].state },
    { D_80184A90, &GameFlag_AcropolisBanks[0].areas[2].state },
    { D_80189DCC, &GameFlag_AcropolisBanks[0].areas[3].state },
    { D_80199390, &GameFlag_AcropolisBanks[0].areas[4].state },
    { D_80184088, &GameFlag_AcropolisBanks[0].areas[5].state },
    { D_8017EA3C, &GameFlag_AcropolisBanks[0].areas[6].state },
    { D_8017FC9C, &GameFlag_AcropolisBanks[0].areas[7].state },
    { D_801831C4, &GameFlag_AcropolisBanks[0].areas[8].state },
    { D_80181264, &GameFlag_AcropolisBanks[0].areas[9].state },
    { D_80183020, &GameFlag_AcropolisBanks[0].areas[10].state },
    { D_8018402C, &GameFlag_AcropolisBanks[0].areas[11].state },
    { D_80185934, &GameFlag_AcropolisBanks[0].areas[12].state },
    { D_8019004C, &GameFlag_AcropolisBanks[0].areas[13].state },
    { D_8018294C, &GameFlag_AcropolisBanks[0].areas[14].state },
    { D_801861E8, &GameFlag_AcropolisBanks[0].areas[15].state },
    { NULL, NULL },
    { D_80186BFC, &GameFlag_AcropolisBanks[0].areas[16].state },
    { D_801951B4, &GameFlag_AcropolisBanks[0].areas[17].state },
    { D_8018DF74, &GameFlag_DryfieldBanks[0].areas[29].state },
};
/// End marker following the stage 1 room table.
u32       D_8010CC8C[2]  = { 0xFFFF, 0 };
GpAreaRec D_8010CC94[39] = {
    { NULL, NULL },
    { D_80184A38, &GameFlag_DryfieldBanks[0].areas[0].state },
    { D_80184F20, &GameFlag_DryfieldBanks[0].areas[1].state },
    { D_80185654, &GameFlag_DryfieldBanks[0].areas[2].state },
    { NULL, NULL },
    { D_80180A50, &GameFlag_DryfieldBanks[0].areas[3].state },
    { D_8017F598, &GameFlag_DryfieldBanks[0].areas[4].state },
    { D_80182100, &GameFlag_DryfieldBanks[0].areas[5].state },
    { D_80180B88, &GameFlag_DryfieldBanks[0].areas[33].state },
    { D_80189938, &GameFlag_DryfieldBanks[0].areas[6].state },
    { NULL, NULL },
    { D_801814DC, &GameFlag_DryfieldBanks[0].areas[7].state },
    { D_80180410, &GameFlag_DryfieldBanks[0].areas[8].state },
    { NULL, NULL },
    { NULL, NULL },
    { D_8017FA74, &GameFlag_DryfieldBanks[0].areas[11].state },
    { D_80182918, &GameFlag_DryfieldBanks[0].areas[12].state },
    { NULL, NULL },
    { D_80181B1C, &GameFlag_DryfieldBanks[0].areas[13].state },
    { D_8017F4B8, &GameFlag_DryfieldBanks[0].areas[14].state },
    { D_8018757C, &GameFlag_DryfieldBanks[0].areas[15].state },
    { D_80188BF0, &GameFlag_DryfieldBanks[0].areas[16].state },
    { D_801842F8, &GameFlag_DryfieldBanks[0].areas[17].state },
    { NULL, NULL },
    { D_801800E0, &GameFlag_DryfieldBanks[0].areas[18].state },
    { D_8017EDD4, &GameFlag_DryfieldBanks[0].areas[19].state },
    { D_8017F558, &GameFlag_DryfieldBanks[0].areas[20].state },
    { D_801876F0, &GameFlag_DryfieldBanks[0].areas[21].state },
    { NULL, NULL },
    { D_80186220, &GameFlag_DryfieldBanks[0].areas[23].state },
    { D_80186764, &GameFlag_DryfieldBanks[0].areas[24].state },
    { NULL, NULL },
    { D_801827BC, &GameFlag_DryfieldBanks[0].areas[26].state },
    { NULL, NULL },
    { D_80180ACC, &GameFlag_DryfieldBanks[0].areas[27].state },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { D_8017F868, &GameFlag_DryfieldBanks[0].areas[28].state },
};
/// End marker following the stage 2 room table.
u32       D_8010CDCC[2]  = { 0xFFFF, 0 };
GpAreaRec D_8010CDD4[39] = {
    { NULL, NULL },
    { D_80190624, &GameFlag_DryfieldBanks[0].areas[0].state },
    { D_80188A08, &GameFlag_DryfieldBanks[0].areas[1].state },
    { D_8018572C, &GameFlag_DryfieldBanks[0].areas[2].state },
    { NULL, NULL },
    { D_80181518, &GameFlag_DryfieldBanks[0].areas[3].state },
    { D_8017F62C, &GameFlag_DryfieldBanks[0].areas[4].state },
    { D_8017FB98, &GameFlag_DryfieldBanks[0].areas[5].state },
    { D_801818D8, &GameFlag_DryfieldBanks[0].areas[33].state },
    { D_80189FA4, &GameFlag_DryfieldBanks[0].areas[6].state },
    { NULL, NULL },
    { D_8018075C, &GameFlag_DryfieldBanks[0].areas[7].state },
    { D_801809A0, &GameFlag_DryfieldBanks[0].areas[8].state },
    { D_80180D24, &GameFlag_DryfieldBanks[0].areas[9].state },
    { D_801802FC, &GameFlag_DryfieldBanks[0].areas[10].state },
    { D_80181438, &GameFlag_DryfieldBanks[0].areas[11].state },
    { D_8017F354, &GameFlag_DryfieldBanks[0].areas[12].state },
    { D_801843C4, &GameFlag_DryfieldBanks[0].areas[34].state },
    { D_80188EE4, &GameFlag_DryfieldBanks[0].areas[13].state },
    { D_8017EB88, &GameFlag_DryfieldBanks[0].areas[14].state },
    { D_80182B5C, &GameFlag_DryfieldBanks[0].areas[15].state },
    { D_80180764, &GameFlag_DryfieldBanks[0].areas[16].state },
    { D_801803E4, &GameFlag_DryfieldBanks[0].areas[17].state },
    { D_8018A70C, &GameFlag_DryfieldBanks[0].areas[35].state },
    { D_801874BC, &GameFlag_DryfieldBanks[0].areas[18].state },
    { D_80181F4C, &GameFlag_DryfieldBanks[0].areas[19].state },
    { D_801843F0, &GameFlag_DryfieldBanks[0].areas[20].state },
    { D_8018C15C, &GameFlag_DryfieldBanks[0].areas[21].state },
    { D_80181190, &GameFlag_DryfieldBanks[0].areas[22].state },
    { D_8018EA94, &GameFlag_DryfieldBanks[0].areas[23].state },
    { D_801861B4, &GameFlag_DryfieldBanks[0].areas[24].state },
    { D_80180888, &GameFlag_DryfieldBanks[0].areas[25].state },
    { D_80183418, &GameFlag_DryfieldBanks[0].areas[26].state },
    { NULL, NULL },
    { D_80180780, &GameFlag_DryfieldBanks[0].areas[27].state },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { D_801802DC, &GameFlag_DryfieldBanks[0].areas[28].state },
};
/// End marker following the stage 3 room table.
u32       D_8010CF0C[2]  = { 0xFFFF, 0 };
GpAreaRec D_8010CF14[50] = {
    { NULL, NULL },
    { D_801898F4, &GameFlag_ShelterBanks[0].areas[0].state },
    { D_8018E238, &GameFlag_ShelterBanks[0].areas[1].state },
    { D_8017F32C, &GameFlag_ShelterBanks[0].areas[2].state },
    { D_801801CC, &GameFlag_ShelterBanks[0].areas[3].state },
    { D_80183544, &GameFlag_ShelterBanks[0].areas[4].state },
    { D_80182A00, &GameFlag_ShelterBanks[0].areas[5].state },
    { D_80185504, &GameFlag_ShelterBanks[0].areas[6].state },
    { D_80183340, &GameFlag_ShelterBanks[0].areas[7].state },
    { D_80184940, &GameFlag_ShelterBanks[0].areas[8].state },
    { D_80183598, &GameFlag_ShelterBanks[0].areas[9].state },
    { D_80186C9C, &GameFlag_ShelterBanks[0].areas[10].state },
    { D_80185A38, &GameFlag_ShelterBanks[0].areas[11].state },
    { D_801854E0, &GameFlag_ShelterBanks[0].areas[12].state },
    { D_80183FDC, &GameFlag_ShelterBanks[0].areas[13].state },
    { D_80185C30, &GameFlag_ShelterBanks[0].areas[14].state },
    { D_8018C14C, &GameFlag_ShelterBanks[0].areas[15].state },
    { D_80184C00, &GameFlag_ShelterBanks[0].areas[16].state },
    { D_80183A98, &GameFlag_ShelterBanks[0].areas[17].state },
    { D_8017FE50, &GameFlag_ShelterBanks[0].areas[18].state },
    { D_8018B5C4, &GameFlag_ShelterBanks[0].areas[19].state },
    { D_8017F17C, &GameFlag_ShelterBanks[0].areas[20].state },
    { D_80187678, &GameFlag_ShelterBanks[0].areas[21].state },
    { D_8017FBF8, &GameFlag_ShelterBanks[0].areas[22].state },
    { D_801830B8, &GameFlag_ShelterBanks[0].areas[23].state },
    { D_801825AC, &GameFlag_ShelterBanks[0].areas[24].state },
    { D_8017E964, &GameFlag_ShelterBanks[0].areas[25].state },
    { D_80184C7C, &GameFlag_ShelterBanks[0].areas[26].state },
    { D_801837AC, &GameFlag_ShelterBanks[0].areas[27].state },
    { D_80184124, &GameFlag_ShelterBanks[0].areas[28].state },
    { D_80186258, &GameFlag_ShelterBanks[0].areas[29].state },
    { D_80186360, &GameFlag_ShelterBanks[0].areas[30].state },
    { D_80183EEC, &GameFlag_ShelterBanks[0].areas[31].state },
    { D_8018933C, &GameFlag_ShelterBanks[0].areas[32].state },
    { D_80186F40, &GameFlag_ShelterBanks[0].areas[33].state },
    { D_801855AC, &GameFlag_ShelterBanks[0].areas[34].state },
    { D_8017FA40, &GameFlag_ShelterBanks[0].areas[35].state },
    { NULL, NULL },
    { NULL, NULL },
    { D_8018EC3C, &GameFlag_ShelterBanks[0].areas[38].state },
    { D_8018FA58, &GameFlag_ShelterBanks[0].areas[39].state },
    { D_80182610, &GameFlag_ShelterBanks[0].areas[40].state },
    { D_8018477C, &GameFlag_ShelterBanks[0].areas[41].state },
    { D_80183D48, &GameFlag_ShelterBanks[0].areas[42].state },
    { D_80188B9C, &GameFlag_ShelterBanks[0].areas[43].state },
    { D_80187350, &GameFlag_ShelterBanks[0].areas[44].state },
    { D_80184CA4, &GameFlag_ShelterBanks[0].areas[45].state },
    { D_80187CB8, &GameFlag_ShelterBanks[0].areas[46].state },
    { D_8018BC10, &GameFlag_ShelterBanks[0].areas[47].state },
    { D_8017DD74, &GameFlag_ShelterBanks[0].areas[5].state },
};
/// End marker following the stage 4 room table.
u32       D_8010D0A4[2]  = { 0xFFFF, 0 };
GpAreaRec D_8010D0AC[34] = {
    { NULL, NULL },
    { D_801818D8, &GameFlag_NeoArkBanks[0].areas[0].state },
    { D_80182A04, &GameFlag_NeoArkBanks[0].areas[1].state },
    { D_80180DA8, &GameFlag_NeoArkBanks[0].areas[2].state },
    { D_80182BF4, &GameFlag_NeoArkBanks[0].areas[3].state },
    { D_8017F758, &GameFlag_NeoArkBanks[0].areas[4].state },
    { NULL, NULL },
    { D_8018786C, &GameFlag_NeoArkBanks[0].areas[6].state },
    { D_801806B8, &GameFlag_NeoArkBanks[0].areas[7].state },
    { NULL, NULL },
    { D_8018305C, &GameFlag_NeoArkBanks[0].areas[9].state },
    { D_80182968, &GameFlag_NeoArkBanks[0].areas[10].state },
    { D_80187470, &GameFlag_NeoArkBanks[0].areas[11].state },
    { D_801876C4, &GameFlag_NeoArkBanks[0].areas[12].state },
    { D_80183F48, &GameFlag_NeoArkBanks[0].areas[13].state },
    { D_80182B54, &GameFlag_NeoArkBanks[0].areas[14].state },
    { D_80182DE0, &GameFlag_NeoArkBanks[0].areas[15].state },
    { D_80181AF8, &GameFlag_NeoArkBanks[0].areas[16].state },
    { D_80180864, &GameFlag_NeoArkBanks[0].areas[17].state },
    { D_801808E4, &GameFlag_NeoArkBanks[0].areas[18].state },
    { NULL, NULL },
    { D_801866C8, &GameFlag_NeoArkBanks[0].areas[20].state },
    { D_801874A4, &GameFlag_NeoArkBanks[0].areas[21].state },
    { D_80180338, &GameFlag_NeoArkBanks[0].areas[21].state },
    { D_80180304, &GameFlag_NeoArkBanks[0].areas[23].state },
    { D_801859DC, &GameFlag_NeoArkBanks[0].areas[24].state },
    { D_8017E994, &GameFlag_NeoArkBanks[0].areas[25].state },
    { D_80184A50, &GameFlag_NeoArkBanks[0].areas[26].state },
    { D_80184230, &GameFlag_NeoArkBanks[0].areas[27].state },
    { D_8018471C, &GameFlag_NeoArkBanks[0].areas[28].state },
    { D_80185860, &GameFlag_NeoArkBanks[0].areas[29].state },
    { D_8017DBB8, &GameFlag_NeoArkBanks[0].areas[30].state },
    { D_80181728, &GameFlag_NeoArkBanks[0].areas[31].state },
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
    if (task->spawnArg1.value == 0) {
        Stage_RequestSpecialFlag(1);
        gGameSession->viewDirty = 0;
        taskKill(task);
        Display_ResetHeapWrapper();
    } else {
        if (task->spawnArg1.value == 1) {
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
    slot->spawnArg1.value = save->state.at4.loc.view;
    ResetGraph(1);
    Gpu_ClearOTag(0);
    Gpu_ClearOTag(1);
    gGameSession->at4.loc.view = save->state.at4.loc.view;
    Pad_SetCooldown(0);
    Gp_SpawnCurView(2);
    gGameSession->viewReady = 0;
    Task_Spawn(0, 0x1E, 1, 0);
}

static void Gp_ReloadAtLoc(s32 arg0)
{
    Task* slot;

    slot                        = gameGetPtrSlot(1);
    Mc_SaveData[0].state.at4.loc.view = arg0;
    gGameSession->at4.loc.view  = arg0;
    slot->spawnArg1.value             = (u8)arg0;
    Pad_SetCooldown(0);
    Gp_SpawnCurView(1);
    gDisplayState.at100.flags.imageSource = 1;
    Task_Spawn(0, 0x1E, 0, 0);
}

void Gp_CommitSpawnLoc(Task* task)
{
    u8 val;

    val                         = (u8)task->spawnArg1.value;
    Mc_SaveData[0].state.at4.loc.view = val;
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
    if (Mc_SaveData[0].state.characterId != 0) {
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
            Mc_SaveData[0].state.companionVariant = 3;
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

#include "common.h"
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include "main/display.h"
#include "main/gamemain.h"
#include "main/mc.h"
#include "main/pad.h"
#include "main/session.h"
#include "main/sound.h"
#include "main/stage.h"
#include "main/task.h"
#include "main/text.h"
#include "main/ui.h"
#include "main/wipsys.h"
#include "gameplay/268.h"
#include "gameplay/3688.h"
#include "gameplay/4CC.h"
#include "rooms/room_common.h"

extern UiObject*      D_80067634;
extern UiObjectDesc   D_8010EFA0;
extern GpItemScan     D_80072724;
extern RoomShopStock  D_8010E138[];
extern u8             D_80071072;
extern u8             D_mist_parking_8017D6D8[];
extern u8             D_mist_parking_80186450[];
extern u8             D_mist_parking_80186464[];
extern u8             D_mist_parking_801864BC[];
extern u8             D_mist_parking_801864C4[];
extern u8             D_mist_parking_801864D0[];
extern u8             D_mist_parking_801864D8[];
extern u8             D_mist_parking_801864E0[];
extern u8             D_mist_parking_801864F4[];
extern u8             D_mist_parking_80186504[];
extern u8             D_mist_parking_80186530[];
extern u8             D_mist_parking_80186718[];
extern UiListItemFunc D_mist_parking_80186538[];
extern UiList         D_mist_parking_80186540;
extern UiObjectDesc   D_mist_parking_801865AC;
extern UiObjectDesc   D_mist_parking_80186654;
extern UiObjectDesc   D_mist_parking_801865C8;
extern UiList         D_mist_parking_8018656C;
extern UiObjectDesc   D_mist_parking_80186590;
extern UiObjectDesc   D_mist_parking_80186600;
extern UiObjectDesc   D_mist_parking_80186670;
extern char           Gp_StrEmpty[];
extern s32            D_mist_parking_80195310;
extern GpItemMap*     D_mist_parking_80195314;

u16* func_mist_parking_8017D8F8(s32 arg0);

/// One row of the vending machine's price ladder (`D_mist_parking_801863B0`,
/// thirteen rows). `spendThreshold` is the running total the player has to have
/// spent for the row to unlock — the last row's is `S32_MAX`, so it never does
/// on its own — and `items` are the three ids the row then offers.
/// `Mc_SaveData.field_934` holds one bit per row and is what this builder reads.
typedef struct {
    /* 0x0 */ s32  spendThreshold;
    /* 0x4 */ s16  items[3];
    /* 0xA */ byte pad_A[2];
} MistParkingShopTier;

extern MistParkingShopTier D_mist_parking_801863B0[13];
extern s32                 D_mist_parking_8018644C;

/// `main`'s rounded-rect panel fill (`Ui_LayoutWithMode0` / `Ui_LayoutWithMode1`
/// are the two named wrappers): draws a `w` x `h` box at (`x`, `y`) relative to
/// the object's base, `mode` selecting the fill style.
INCLUDE_ASM("rooms/nonmatchings/mist_parking/mist_parking_3", func_mist_parking_801812B4);

INCLUDE_ASM("rooms/nonmatchings/mist_parking/mist_parking_3", func_mist_parking_80181468);

INCLUDE_ASM("rooms/nonmatchings/mist_parking/mist_parking_3", func_mist_parking_80181760);

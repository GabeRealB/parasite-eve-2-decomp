#include "gameplay/area_transitions.h"

#include "common.h"

#include "gameplay/area.h"
#include "gameplay/area_flags.h"
#include "gameplay/captions.h"
#include "gameplay/direction.h"
#include "gameplay/direction_input.h"
#include "direction_input.h"
#include "gameplay/loading.h"
#include "gameplay/message.h"
#include "gameplay/scene_runtime.h"
#include "gameplay/world_collision.h"
#include "gameplay/world_state.h"
#include "gameplay/world_targets.h"

#include "main/mc.h"
#include "main/session.h"
#include "main/task.h"

/// 8-byte pair of byte-table pointers at `D_801149FC`. `Gp_MsgPlayerDirFacing`
/// indexes by `(Gp_DirByte & 0x70) >> 4`. `Gp_DirFlags & 0x100` selects
/// `field_4` over `field_0`. The byte at `(Gp_DirByte & 0xF) -
/// GameActor.field_82` is stored into `GameActor.field_930`.
typedef struct _GpDirPair {
    /* 0x0 */ u8* field_0;
    /* 0x4 */ u8* field_4;
} GpDirPair;
STATIC_ASSERT_SIZEOF(GpDirPair, 8);

extern GpDirPair D_801149FC[];

extern u8 D_801149E8[10];

extern u8 D_801149F4[8];

void D_8017DA78(s32 arg0, s32 arg1);

void D_8017EF60(s32 arg0, s32 arg1);

extern u8 D_80189A9C[];

extern u8 D_80189AA8[];

extern u8 D_801826FC[];

extern u8 D_8018270C[];

extern u8 D_8018271C[];

extern u8 D_8018272C[];

extern u8 D_801850C8[];

extern u8 D_801850D8[];

u8 D_801149E8[10] = {
    4,
    4,
    6,
    6,
    6,
    6,
    6,
    6,
    6,
    6
};
u8 D_801149F4[8] = {
    4,
    4,
    4,
    4,
    4,
    4,
    4,
    4
};

GpDirPair D_801149FC[5] = {
    { D_80189A9C, D_80189AA8 },
    { D_801149E8, D_801149F4 },
    { D_801826FC, D_8018270C },
    { D_8018271C, D_8018272C },
    { D_801850C8, D_801850D8 }
};

void Gp_MsgPlayerDirFacing(void)
{
    Task*      slot;
    GameActor* actor;
    u8         flags;
    s32        facing;
    u8*        row;

    actor = gameGetPtrSlot(3)->work;
    flags = Gp_DirByte;
    if (flags & 0x80) {
        facing = actor->field_82;
        if (Gp_DirFlags & 0x100) {
            row              = D_801149FC[(flags & 0x70) >> 4].field_4;
            actor->field_930 = row[(flags & 0xF) - facing];
        } else {
            row              = D_801149FC[(flags & 0x70) >> 4].field_0;
            actor->field_930 = row[(flags & 0xF) - facing];
        }
    } else {
        actor->field_930 = (flags & 0x70) >> 4;
    }

    slot = gameGetPtrSlot(3);
    if (Gp_DispatchMsg(slot, 0x3F0, 0, 0) == 0) {
        Gp_DispatchMsg(slot, 0x3F1, 0, 0);
        D_80114CF8      = 0;
        Gp_DirNibble    = 0;
        Gp_DirByte      = 0;
        Gp_DirFlags     = 0;
        Gp_DirAltNibble = 0;
        Gp_DirAlt       = 0;
        D_80114CD4      = 0;
        D_80114CDD      = 0;
    } else if (Gp_TakePendingObj4C(&D_80114CD4, &Gp_DirAlt, &Gp_DirAltNibble)) {
        if ((u8)D_80114CD4 == 0) {
            Gp_StateF0.field_4 = 1;
            Gp_DirPhase++;
        }
    }
}

void Gp_CommitDirWarp(void)
{
    Task*       slot;
    GpSaveLoc*  loc;
    McSaveData* save;

    slot = gameGetPtrSlot(7);
    loc  = &Gp_WarpLoc;

    /* first two bytes as one halfword (field_1 cleared) */
    Gp_WarpLoc.prefix.packed = Gp_DirAlt;
    loc->field_2             = Gp_DirAltNibble & 0xF;
    loc->field_4             = 1;
    loc->field_3             = 1;
    loc->field_5             = 0;
    Gp_DispatchMsgPtrs(slot, 0x13EE, loc, loc);

    save               = &Mc_SaveData[0];
    save->at4.loc.area = Gp_WarpLoc.prefix.bytes.field_0;
    save->at4.loc.warp = loc->field_2;
    save->at4.loc.room = loc->field_3;
    Task_Spawn(0, 0x11, 0, 0);

    Gp_DirAltNibble = 0;
    Gp_DirAlt       = 0;
    D_80114CF8      = 0;
    Gp_DirNibble    = 0;
    Gp_DirByte      = 0;
    Gp_DirFlags     = 0;
    D_80114CD4      = 0;
    D_80114CDD      = 0;
}

void Gp_PostDirIfCapIdle(void)
{
    if (gGameSession->eventState == 0) {
        if (Gp_CapBusy() == 0) {
            if (Gp_DirNibble == 0xFF) {
                Gp_DispatchMsg(gameGetPtrSlot(7), 0x13F0, Gp_DirByte, 0);
            } else {
                Gp_SpawnIfCapIdle(Gp_DirByte, Gp_DirNibble);
            }
        }
    }
    D_80114CF8      = 0;
    Gp_DirNibble    = 0;
    Gp_DirByte      = 0;
    Gp_DirFlags     = 0;
    Gp_DirAltNibble = 0;
    Gp_DirAlt       = 0;
    D_80114CD4      = 0;
    if (D_80114CDC == 0) {
        gGameSession->dirActionBusy = 0;
    }
}

void Gp_RunDirAction(void)
{
    void (*fns[2])(s32, s32) = { D_8017DA78, D_8017EF60 };

    if (gGameSession->eventState != 0) {
        D_80114CF8      = 0;
        Gp_DirNibble    = 0;
        Gp_DirByte      = 0;
        Gp_DirFlags     = 0;
        Gp_DirAltNibble = 0;
        Gp_DirAlt       = 0;
        D_80114CD4      = 0;
    } else {
        fns[(Gp_DirFlags >> 8) & 0x7F](Gp_DirByte, Gp_DirNibble);
        D_80114CF8      = 0;
        Gp_DirNibble    = 0;
        Gp_DirByte      = 0;
        Gp_DirFlags     = 0;
        Gp_DirAltNibble = 0;
        Gp_DirAlt       = 0;
        D_80114CD4      = 0;
    }
}

void Gp_ApplyAreaRecs(GpAreaApplyRec* recs)
{
    GpAreaKey  key;
    GpAreaRec* tbl;
    GpAreaObj* obj;
    GpAreaKey* sess;
    s32        i;
    s32        stage;
    s32        mask;
    s8         apply;
    s8         mode;

    apply = 0;
    sess  = &gGameSession->at4.loc;
    for (i = 0; recs[i].field_0 != 0xFF; i++) {
        stage     = recs[i].field_0;
        tbl       = Gp_AreaTables[stage];
        key.stage = stage;
        key.area  = recs[i].field_1;
        key.room  = 1;
        key.view  = sess->view;
        mask      = recs[i].field_3 & 0xF0;
        if (mask == 0) {
            apply = 1;
        } else {
            mode = Mc_SaveData[0].gameMode;
            if (mode == 0 || mode == 2) {
                if (mask == 0x10) {
                    apply = 1;
                }
            } else if (mode == 1 || mode == 3) {
                if (mask == 0x20) {
                    apply = 1;
                }
            } else {
                apply = 0;
            }
        }
        if (apply) {
            Gp_SetAreaObjId(&key, recs[i].field_2, 1);
            if (tbl != NULL) {
                obj = tbl[recs[i].field_1].field_4;
                if (obj != NULL) {
                    if (recs[i].field_3 & 0xF) {
                        obj->field_1 |= 4;
                    } else {
                        obj->field_1 &= 0xFB;
                    }
                }
            }
        }
        apply = 0;
    }
}

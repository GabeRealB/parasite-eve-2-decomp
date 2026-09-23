#include "common.h"

#include "gameplay/3A34.h"
#include "gameplay/3CD8.h"
#include "gameplay/3FB8.h"
#include "main/gameflag.h"
#include "main/session.h"
#include "main/sound.h"
#include "rooms/dryfield_breezeway.h"

void func_dryfield_breezeway_8017FF7C(Task* task)
{
    s32            mask;
    RoomEffWork*   eff;
    GsCOORDINATE2* coord;
    GsCOORDINATE2* player;
    s32            limit;
    s32            pan;

    mask   = 1 << gGameSession->at4.loc.view;
    eff    = task->spawnArg2;
    coord  = ((TmdObject*)task->extra)->coords;
    player = ((GpActorWork*)gameGetPtrSlot(3))->extra->coords;
    if (mask & 0x18) {
        func_dryfield_breezeway_8018034C(coord, D_dryfield_breezeway_80183164, 0x600, 0x80);
    } else if (mask & 0x20) {
        func_dryfield_breezeway_80180858(coord, D_dryfield_breezeway_80183164, 0x600, 0x10);
    }
    if (Gp_State1C->eventState != 0) {
        return;
    }
    Gp_State1C->roomEffectMode = 2;
    if (GameFlag_GetNibble(0x5D) == 0) {
        if (gGameSession->at4.loc.view == 2) {
            limit            = (player->coord.t[0] - 5856) >> 7;
            eff->field_10.vx = 12000;
            eff->field_10.vy = -3000;
            eff->field_10.vz = 3000;
            Gp_LcgState      = Gp_LcgState * 5 + 0x71357911;
            if ((u16)((Gp_LcgState >> 16) % 100) < limit) {
                Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
                Gp_SpawnEff(0x6003C, coord, (s32)(Gp_LcgState >> 16) % limit + 0x40, &eff->field_10);
            }
            if ((s16)eff->field_2A == 0) {
                SndEvt_EnqueueType6(0x5216000A, 0, 0);
                eff->field_2A = 1;
            }
        } else if (gGameSession->at4.loc.view == 3) {
            eff->field_24    = 0x10;
            eff->field_10.vx = player->coord.t[0] + 0x100;
            eff->field_10.vy = -3000;
            eff->field_10.vz = 3000;
            Gp_LcgState      = Gp_LcgState * 5 + 0x71357911;
            Gp_SpawnEff(0x6003C, coord, ((Gp_LcgState >> 16) & 0x7F) + 0x40, &eff->field_10);
            if ((s16)eff->field_2A < 2) {
                Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
                if (!((Gp_LcgState >> 16) & 3)) {
                    pan = (s8)Gp_GetObjPan(coord);
                    SndEvt_EnqueueType6(0x5216000B, pan, (s8)gpGetObjDepth(coord));
                    eff->field_2A = 2;
                }
            }
        }
    } else if (GameFlag_GetNibble(0x5D) == 1) {
        if ((s16)eff->field_2A != 0) {
            SndEvt_EnqueueType7(0x5216000A, 0);
            eff->field_2A = 0;
        }
        if ((s16)eff->field_24 != 0) {
            eff->field_24--;
            eff->field_10.vx = 16000;
            eff->field_10.vy = -3000;
            eff->field_10.vz = 2750;
            Gp_LcgState      = Gp_LcgState * 5 + 0x71357911;
            Gp_SpawnEff(0x6003C, coord, ((Gp_LcgState >> 16) & 0xFF) + 0x40, &eff->field_10);
            eff->field_10.vx = 17000;
            eff->field_10.vy = -3000;
            eff->field_10.vz = 4000;
            Gp_LcgState      = Gp_LcgState * 5 + 0x71357911;
            Gp_SpawnEff(0x6003C, coord, ((Gp_LcgState >> 16) & 0xFF) + 0x40, &eff->field_10);
        }
    }
}

INCLUDE_ASM("rooms/nonmatchings/dryfield_breezeway/dryfield_breezeway_6", func_dryfield_breezeway_8018034C);

INCLUDE_ASM("rooms/nonmatchings/dryfield_breezeway/dryfield_breezeway_6", func_dryfield_breezeway_80180858);

INCLUDE_ASM("rooms/nonmatchings/dryfield_breezeway/dryfield_breezeway_6", func_dryfield_breezeway_80181264);

INCLUDE_ASM("rooms/nonmatchings/dryfield_breezeway/dryfield_breezeway_6", func_dryfield_breezeway_80181938);

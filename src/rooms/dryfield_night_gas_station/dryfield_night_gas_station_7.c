#include "common.h"
#include "main/task.h"

#include "gameplay/3CD8.h"
#include "gameplay/D4.h"
#include "main/gameflag.h"
#include "main/session.h"
#include "main/tmd.h"
#include "rooms/dryfield_night_gas_station.h"
#include "rooms/room_common.h"

extern Task* D_dryfield_night_gas_station_801907AC;

extern s32 D_8011572C;
extern s32 D_80115750;
extern s32 D_80115758;
extern u32 Gp_LcgState;

/// The room's effect anchors, 8 bytes apart. Entries 0-9 are drawn in pairs by
/// `func_dryfield_night_gas_station_801812B4`, 10-18 one at a time by
/// `func_dryfield_night_gas_station_80181AF8`, and 19-20 are where the
/// spawned effects are scattered around.
extern SVECTOR D_dryfield_night_gas_station_80189C8C[];
/// Entries 21-24 of the anchor list, reached by name: two
/// `func_dryfield_night_gas_station_801812B4` pairs
/// drawn together whenever one of views 2, 3, 13 or 14 is current.
extern SVECTOR D_dryfield_night_gas_station_80189D34[];
/// Per-anchor view masks, one word per anchor: bit `n` set draws the anchor
/// while view `n` is current.
extern s32 D_dryfield_night_gas_station_80189D54[];

void func_dryfield_night_gas_station_80180C20(void)
{
    D_dryfield_night_gas_station_801907A4 = 0;
    D_dryfield_night_gas_station_801907A8 = 0;
    D_dryfield_night_gas_station_801907AC = 0;
}

/// Gates the room's flag views on its own argument: the flags at 0x34 / 0x3C
/// of the five views the current room's sprite record points at are cleared for
/// a 0 argument and set to the argument for a 1, and any other argument changes
/// nothing. Each view takes only the flag it carries -- the first at 0x34
/// alone, the last two at 0x3C alone -- and
/// `func_dryfield_night_gas_station_80180DC8` drives the 0x44 / 0x4C / 0x54
/// flags of the 0xA0 / 0xAC / 0xC4 three instead.
void func_dryfield_night_gas_station_80180C3C(s32 arg0)
{
    GpAreaKey*                       sess;
    DryfieldNightGasStationSprtRec*  rec;
    DryfieldNightGasStationSprtView* view;
    s32                              flag;

    sess = &gGameSession->at4.loc;
    rec  = (DryfieldNightGasStationSprtRec*)Gp_SprtTables[sess->stage - 1]->field_0[sess->area - 1];
    flag = arg0 & 0xFF;

    switch (flag) {
        case 0:
            view           = rec->field_34;
            view->field_34 = 0;
            view           = rec->field_94;
            view->field_34 = 0;
            view->field_3C = 0;
            view           = rec->field_A0;
            view->field_34 = 0;
            view->field_3C = 0;
            view           = rec->field_AC;
            view->field_3C = 0;
            view           = rec->field_C4;
            view->field_3C = 0;
            break;
        case 1:
            view           = rec->field_34;
            view->field_34 = flag;
            view           = rec->field_94;
            view->field_34 = flag;
            view->field_3C = flag;
            view           = rec->field_A0;
            view->field_34 = flag;
            view->field_3C = flag;
            view           = rec->field_AC;
            view->field_3C = flag;
            view           = rec->field_C4;
            view->field_3C = flag;
            break;
    }
}

/// Gates the room's two sprite command records on game flag nibble 0x8D: a
/// zero nibble clears both commands' skip-link flag, a one sets it. The two
/// records are views 10 and 19 of the current room's sprite record array, and
/// the flag both write is command 6's.
void func_dryfield_night_gas_station_80180D1C(void)
{
    GpAreaKey* sess = &gGameSession->at4.loc;
    GpSprtRec* view = Gp_SprtTables[sess->stage - 1][0].field_0[sess->area - 1];
    s32        flag = GameFlag_GetNibble(0x8D);

    switch (flag) {
        case 0:
            view[10].field_4[6].field_4 = 0;
            view[19].field_4[6].field_4 = 0;
            break;
        case 1:
            view[10].field_4[6].field_4 = flag;
            view[19].field_4[6].field_4 = flag;
            break;
    }
}

/// Switches the room's lamp effect between its lit and dark appearance: the
/// current room's three lamp views have their three flags written to 1 for the
/// 0 argument and to 0 for the 1 argument, and any other argument changes
/// nothing. `func_dryfield_night_gas_station_80180A60` drives it from the
/// blinking-light table, whose own exit passes 0.
void func_dryfield_night_gas_station_80180DC8(s16 arg0)
{
    GpAreaKey*                      sess = &gGameSession->at4.loc;
    DryfieldNightGasStationSprtRec* rec  = (DryfieldNightGasStationSprtRec*)
                                              Gp_SprtTables[sess->stage - 1][0]
                                                  .field_0[sess->area - 1];
    DryfieldNightGasStationSprtView* view;

    switch (arg0) {
        case 0:
            view           = rec->field_A0;
            view->field_44 = 1;
            view->field_4C = 1;
            view           = rec->field_AC;
            view->field_44 = 1;
            view->field_4C = 1;
            view->field_54 = 1;
            view           = rec->field_C4;
            view->field_44 = 1;
            view->field_4C = 1;
            view->field_54 = 1;
            break;
        case 1:
            view           = rec->field_A0;
            view->field_44 = 0;
            view->field_4C = 0;
            view           = rec->field_AC;
            view->field_44 = 0;
            view->field_4C = 0;
            view->field_54 = 0;
            view           = rec->field_C4;
            view->field_44 = 0;
            view->field_4C = 0;
            view->field_54 = 0;
            break;
    }
}

/// Room effect task tick. The first tick installs the room's three effect ids
/// and sets `roomEffectMode` to 2. Every tick it draws the anchors whose view
/// mask includes the current view. While game flag nibble 0x63 is clear, and
/// outside battles and events, each of the two scatter anchors rolls a jittered
/// spawn position and one of three effects (0x60080, 0x6008D, or 0x60070 on a
/// further 1-in-3). Once the nibble is set, and only if it was seen clear
/// before, the anchors keep spawning 0x60070 alone on a 1-in-3.
void func_dryfield_night_gas_station_80180E9C(Task* task)
{
    DryfieldNightGasStationEffWork* work;
    GsCOORDINATE2*                  coord;
    s32                             mask;
    s32                             i;

    work  = task->spawnArg2;
    coord = ((TmdObject*)task->extra)->coords;
    mask  = 1 << Gp_GetViewIndex();
    if (task->state == 0) {
        D_80115758                 = 0x60006;
        D_8011572C                 = 0x60008;
        D_80115750                 = 0x60009;
        Gp_State1C->roomEffectMode = 2;
    }
    for (i = 0; i < 10; i += 2) {
        if (mask & D_dryfield_night_gas_station_80189D54[i]) {
            func_dryfield_night_gas_station_801812B4(&D_dryfield_night_gas_station_80189C8C[i], 0x180, 0x222);
        }
    }
    if (mask & 0x600C) {
        func_dryfield_night_gas_station_801812B4(&D_dryfield_night_gas_station_80189D34[0], 0x180, 0x444);
        func_dryfield_night_gas_station_801812B4(&D_dryfield_night_gas_station_80189D34[2], 0x180, 0x444);
    }
    for (i = 10; i < 19; i++) {
        if (mask & D_dryfield_night_gas_station_80189D54[i]) {
            func_dryfield_night_gas_station_80181AF8(&D_dryfield_night_gas_station_80189C8C[i], 0, 0x380);
        }
    }
    if (GameFlag_GetNibble(0x63) == 0) {
        work->active = 1;
        if (Gp_State1C->battleState != 1 && Gp_State1C->eventState == 0) {
            for (i = 19; i < 21; i++) {
                Gp_LcgState  = Gp_LcgState * 5 + 0x71357911;
                work->kind   = (Gp_LcgState >> 16) % 3;
                Gp_LcgState  = Gp_LcgState * 5 + 0x71357911;
                work->pos.vx = D_dryfield_night_gas_station_80189C8C[i].vx - ((Gp_LcgState >> 16) & 0x1FF) + 0x100;
                work->pos.vy = D_dryfield_night_gas_station_80189C8C[i].vy;
                Gp_LcgState  = Gp_LcgState * 5 + 0x71357911;
                work->pos.vz = D_dryfield_night_gas_station_80189C8C[i].vz - ((Gp_LcgState >> 16) & 0x1FF) + 0x100;
                if (work->kind == 0) {
                    Gp_SpawnEff(0x60080, coord, 0x10300, &work->pos);
                } else if (work->kind == 1) {
                    Gp_SpawnEff(0x6008D, coord, 0x300, &work->pos);
                } else {
                    Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
                    if ((u16)((Gp_LcgState >> 16) % 3) == 0) {
                        Gp_SpawnEff(0x60070, coord, 0xC0013500, &work->pos);
                    }
                }
            }
        }
    } else if (work->active != 0 && Gp_State1C->eventState == 0) {
        for (i = 19; i < 21; i++) {
            Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
            if ((u16)((Gp_LcgState >> 16) % 3) == 0) {
                Gp_LcgState  = Gp_LcgState * 5 + 0x71357911;
                work->pos.vx = D_dryfield_night_gas_station_80189C8C[i].vx - ((Gp_LcgState >> 16) & 0x1FF) + 0x100;
                Gp_LcgState  = Gp_LcgState * 5 + 0x71357911;
                work->pos.vy = D_dryfield_night_gas_station_80189C8C[i].vy;
                work->pos.vz = D_dryfield_night_gas_station_80189C8C[i].vz - ((Gp_LcgState >> 16) & 0x1FF) + 0x100;
                Gp_SpawnEff(0x60070, coord, 0xC0013500, &work->pos);
            }
        }
    }
}

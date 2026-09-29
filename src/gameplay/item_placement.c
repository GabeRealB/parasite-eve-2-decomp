#include "item_placement.h"

#include <psyq/libgte.h>

#include "types.h"

#include "gameplay/area_flags.h"
#include "area_flags.h"
#include "gameplay/enemy.h"
#include "gameplay/item_placement.h"
#include "gameplay/scene_runtime.h"
#include "scene_runtime.h"

#include "gameplay/damage.h"
#include "main/gfx.h"
#include "main/mc.h"
#include "main/session.h"
#include "main/task_types.h"

static inline s32 _gpGetCurBit2Flag(s32 arg0);

static inline void _gpSpawnPlace(GpEnemyDesc* desc, GpBit2Rec* place);

static void Gp_SpawnPlaceById(u16 arg0);

static inline s32 _gpGetCurBit2Flag(s32 arg0)
{
    u32* p;
    u32  word;
    s32  shift;

    p      = &Gp_Bit2Banks[gGameSession->at4.loc.stage].field_4[arg0 >> 4];
    shift  = (arg0 & 0xF) * 2;
    word   = *p;
    word  &= 3 << shift;
    word >>= shift;
    return word;
}
static inline void _gpSpawnPlace(GpEnemyDesc* desc, GpBit2Rec* place)
{
    GpEnemy*   enemy;
    Task*      task;
    TmdObject* extra;
    GpCoord*   coord;
    u16        id;

    id = desc->field_0;
    while (id != 0xFFFF) {
        if (id == place->field_2) {
            enemy = Gp_SpawnEnemyFromTable(&desc->field_4, 0, desc->field_0, NULL);
            if (enemy != NULL) {
                task = enemy->task;
                if (task->spawnType != 0) {
                    extra               = task->extra.tmd;
                    coord               = extra->coords;
                    enemy->placeKey     = place->field_0 | (place->field_4 << 8);
                    enemy->workType     = place->field_2;
                    coord->coord.t[0]   = place->field_8;
                    coord->coord.t[1]   = place->field_A;
                    coord->coord.t[2]   = place->field_C;
                    coord->param.rot.vy = place->field_E;
                    if (coord->param.rot.vy != 0) {
                        Gfx_RotMatrixY(&coord->coord, (s16)place->field_E, 1);
                    }
                    coord->flg = 0;
                }
            }
            return;
        }
        desc++;
        id = desc->field_0;
    }
}

GpBit2Rec D_80114588[] = {
    { 0x2E, 0x804, 0x0, 0x1, 0, 0, 0, 0x0 },
    { 0x1, 0x703, 0x0, 0x1, 0, 0, 0, 0x0 },
    { 0x2, 0xA0, 0x0, 0x3, 0, 0, 0, 0x0 },
    { 0x26, 0x65, 0x0, 0x1, 0, 0, 0, 0x0 },
    { 0x27, 0xB, 0x0, 0x1, 0, 0, 0, 0x0 },
    { 0xB, 0x118, 0x0, 0x201, 0, 0, 0, 0x0 },
    { 0xFFFF, 0x0, 0x0, 0x0, 0, 0, 0, 0x0 },
};

GpBit2Rec D_801145F8[] = {
    { 0xC, 0x116, 0x0, 0x201, 0, 0, 0, 0x0 },
    { 0x1B, 0x119, 0x0, 0x1, 0, 0, 0, 0x0 },
    { 0xFFFF, 0x0, 0x0, 0x0, 0, 0, 0, 0x0 },
};

GpBit2Rec D_80114628[] = {
    { 0xF, 0x5, 0x0, 0x1, 0, 0, 0, 0x0 },
    { 0x10, 0x3, 0x0, 0x1, 0, 0, 0, 0x0 },
    { 0xFFFF, 0x0, 0x0, 0x0, 0, 0, 0, 0x0 },
};

GpBit2Rec D_80114658[] = {
    { 0x28, 0xAD, 0x0, 0x1, 0, 0, 0, 0x0 },
    { 0xFFFF, 0x0, 0x0, 0x0, 0, 0, 0, 0x0 },
};

GpBit2Rec D_80114678[] = {
    { 0x29, 0xD, 0x0, 0x1, 0, 0, 0, 0x0 },
    { 0xFFFF, 0x0, 0x0, 0x0, 0, 0, 0, 0x0 },
};

GpBit2Rec D_80114698[] = {
    { 0xFFFF, 0x0, 0x0, 0x0, 0, 0, 0, 0x0 },
};

GpBit2Rec D_801146A8[] = {
    { 0x12, 0x8, 0x0, 0x1, 0, 0, 0, 0x0 },
    { 0x14, 0x1, 0x0, 0x1, 0, 0, 0, 0x0 },
    { 0xFFFF, 0x0, 0x0, 0x0, 0, 0, 0, 0x0 },
};

GpBit2Rec D_801146D8[] = {
    { 0x2A, 0x7, 0x0, 0x1, 0, 0, 0, 0x0 },
    { 0xFFFF, 0x0, 0x0, 0x0, 0, 0, 0, 0x0 },
};

GpBit2Rec D_801146F8[] = {
    { 0xD, 0xAC, 0x0, 0x1, 0, 0, 0, 0x0 },
    { 0xFFFF, 0x0, 0x0, 0x0, 0, 0, 0, 0x0 },
};

GpBit2Rec D_80114718[] = {
    { 0x9, 0x113, 0x0, 0x1, 0, 0, 0, 0x0 },
    { 0x2D, 0x806, 0x0, 0x101, 0, 0, 0, 0x0 },
    { 0xFFFF, 0x0, 0x0, 0x0, 0, 0, 0, 0x0 },
};

GpBit2Rec D_80114748[] = {
    { 0x15, 0x5, 0x0, 0x1, 0, 0, 0, 0x0 },
    { 0xFFFF, 0x0, 0x0, 0x0, 0, 0, 0, 0x0 },
};

GpBit2Rec D_80114768[] = {
    { 0x16, 0x11B, 0x0, 0x201, 0, 0, 0, 0x0 },
    { 0x17, 0x1, 0x0, 0x1, 0, 0, 0, 0x0 },
    { 0xFFFF, 0x0, 0x0, 0x0, 0, 0, 0, 0x0 },
};

GpBit2Rec D_80114798[] = {
    { 0x19, 0x110, 0x0, 0x1, 0, 0, 0, 0x0 },
    { 0x1A, 0x1, 0x0, 0x1, 0, 0, 0, 0x0 },
    { 0x1E, 0x11E, 0x0, 0x201, 0, 0, 0, 0x0 },
    { 0x1F, 0x5, 0x0, 0x1, 0, 0, 0, 0x0 },
    { 0x20, 0x7, 0x0, 0x1, 0, 0, 0, 0x0 },
    { 0x21, 0x39, 0x0, 0x1, 0, 0, 0, 0x0 },
    { 0xFFFF, 0x0, 0x0, 0x0, 0, 0, 0, 0x0 },
};

GpBit2Rec D_80114808[] = {
    { 0x6, 0x115, 0x0, 0x1, 0, 0, 0, 0x0 },
    { 0xFFFF, 0x0, 0x0, 0x0, 0, 0, 0, 0x0 },
};

GpBit2Rec D_80114828[] = {
    { 0x13, 0xAE, 0x0, 0x1, 0, 0, 0, 0x0 },
    { 0xFFFF, 0x0, 0x0, 0x0, 0, 0, 0, 0x0 },
};

GpBit2Rec D_80114848[] = {
    { 0x4, 0x10F, 0x0, 0x201, 0, 0, 0, 0x0 },
    { 0x23, 0x705, 0x0, 0x1, 0, 0, 0, 0x0 },
    { 0x3, 0x111, 0x0, 0x201, 0, 0, 0, 0x0 },
    { 0xFFFF, 0x0, 0x0, 0x0, 0, 0, 0, 0x0 },
};

GpBit2Rec D_80114888[] = {
    { 0x1D, 0x1, 0x0, 0x1, 0, 0, 0, 0x0 },
    { 0xFFFF, 0x0, 0x0, 0x0, 0, 0, 0, 0x0 },
};

GpBit2Rec D_801148A8[] = {
    { 0x7, 0x114, 0x0, 0x101, 1745, 0, 280, 0x0 },
    { 0x1C, 0x68, 0x0, 0x1, 0, 0, 0, 0x0 },
    { 0xFFFF, 0x0, 0x0, 0x0, 0, 0, 0, 0x0 },
};

GpBit2Rec D_801148D8[] = {
    { 0x8, 0x112, 0x0, 0x201, 0, 0, 0, 0x0 },
    { 0x2F, 0x805, 0x0, 0x101, 0, 0, 0, 0x0 },
    { 0x22, 0x82, 0x0, 0x2, 4128, 0, -256, 0x800 },
    { 0xFFFF, 0x0, 0x0, 0x0, 0, 0, 0, 0x0 },
};

GpBit2Rec D_80114918[] = {
    { 0x2B, 0xD, 0x0, 0x1, 0, 0, 0, 0x0 },
    { 0xFFFF, 0x0, 0x0, 0x0, 0, 0, 0, 0x0 },
};

GpBit2Rec D_80114938[] = {
    { 0xE, 0x704, 0x0, 0x1, 0, 0, 0, 0x0 },
    { 0x30, 0x810, 0x0, 0x101, 0, 0, 0, 0x0 },
    { 0xFFFF, 0x0, 0x0, 0x0, 0, 0, 0, 0x0 },
};

GpBit2Rec D_80114968[] = {
    { 0xA, 0x117, 0x0, 0x1, -5950, -500, -20, 0x0 },
    { 0x2C, 0x38, 0x0, 0x1, 0, 0, 0, 0x0 },
    { 0xFFFF, 0x0, 0x0, 0x0, 0, 0, 0, 0x0 },
};

GpBit2Rec D_80114998[] = {
    { 0x25, 0x3C, 0x0, 0x1, 0, 0, 0, 0x0 },
    { 0xFFFF, 0x0, 0x0, 0x0, 0, 0, 0, 0x0 },
};

GpBit2Rec D_801149B8[] = {
    { 0x11, 0x3A, 0x0, 0x1, 0, 0, 0, 0x0 },
    { 0x5, 0xC, 0x0, 0x1, 0, 0, 0, 0x0 },
    { 0xFFFF, 0x0, 0x0, 0x0, 0, 0, 0, 0x0 },
};

static void Gp_SpawnPlaceById(u16 arg0)
{
    GpAreaKey*  sess;
    GpBit2List* lists;
    GpBit2Rec*  place;
    u16         id;

    sess  = &Mc_SaveData[0].state.at4.loc;
    lists = Gp_Bit2Banks[sess->stage].field_0;
    if (lists == NULL) {
        return;
    }
    place = lists[sess->area].field_0.records;
    if (place == NULL) {
        return;
    }
    id = place->field_0;
    while (id != 0xFFFF) {
        if (id == arg0) {
            if (_gpGetCurBit2Flag(id) != 0) {
                _gpSpawnPlace(lists[sess->area].field_4, place);
            }
            return;
        }
        place++;
        id = place->field_0;
    }
}

void Gp_SpawnPlaces(GpAreaKey* arg0)
{
    GpBit2List*  lists;
    GpBit2Rec*   place;
    GpEnemyDesc* desc;
    GpEnemy*     enemy;
    Task*        task;
    TmdObject*   extra;
    GpCoord*     coord;
    u16          term;
    u16          id;

    lists = Gp_Bit2Banks[arg0->stage].field_0;
    if (lists == NULL) {
        return;
    }
    place = lists[arg0->area].field_0.records;
    if (place == NULL) {
        return;
    }
    term = 0xFFFF;
    if (place->field_0 == term) {
        return;
    }
    do {
        desc = lists[arg0->area].field_4;
        id   = desc->field_0;
        if (id != term) {
            do {
                if (id == place->field_2) {
                    enemy = Gp_SpawnEnemyFromTable(&desc->field_4, 0, desc->field_0, NULL);
                    if (enemy != NULL) {
                        task = enemy->task;
                        if (task->spawnType != 0) {
                            extra               = task->extra.tmd;
                            coord               = extra->coords;
                            enemy->placeKey     = place->field_0 | (place->field_4 << 8);
                            enemy->workType     = place->field_2;
                            coord->coord.t[0]   = place->field_8;
                            coord->coord.t[1]   = place->field_A;
                            coord->coord.t[2]   = place->field_C;
                            coord->param.rot.vy = place->field_E;
                            if (coord->param.rot.vy != 0) {
                                Gfx_RotMatrixY(&coord->coord, (s16)place->field_E, 1);
                            }
                            coord->flg = 0;
                        }
                    }
                    break;
                }
                desc++;
                id = desc->field_0;
            } while (id != term);
        }
        place++;
    } while (place->field_0 != term);
}

#include "common.h"

#include "main/session.h"
#include "main/task.h"
#include "main/tmd.h"

#include "gameplay/1BC.h"
#include "gameplay/3CD8.h"
#include "gameplay/3FB8.h"
#include "gameplay/D4.h"

#include "actors/actor_302600.h"

/* `D_80067704` is the third word of a `D_800676A8` record: it selects the model
 * stream the next `Gp_SpawnEff` uses for the effect's own `TmdObject`. Declared
 * as a one-element array so GCC 2.8.1 cannot treat the store as non-aliasing
 * with the struct traffic that follows and sink it past the loads. */
extern void* D_80067704[1];

/* Model stream in this overlay's own data. */
extern u8 D_actor_302600_80167D30[];

INCLUDE_ASM("actors/nonmatchings/actor_302600/actor_302600_5", func_actor_302600_801658E0);

INCLUDE_ASM("actors/nonmatchings/actor_302600/actor_302600_5", func_actor_302600_80165978);

void func_actor_302600_80165A6C(Actor302600* actor)
{
    GpAreaKey  key;
    GpAreaKey* sessionKey;
    u8         areaByte0;
    GpAreaRec* rec;
    GpCdRec10* entry;
    GpEffWork* eff;
    TmdObject* model;
    s32        idx;
    u32        raw;

    D_80067704[0] = D_actor_302600_80167D30;
    eff           = Gp_SpawnEff(0x40007, actor->field_2C->field_8 + 4, 0x100, NULL);
    if (eff == NULL) {
        return;
    }
    sessionKey  = (GpAreaKey*)&Game_Session->field_4;
    raw         = actor->field_20->field_8;
    model       = (TmdObject*)eff->field_0->extra;
    key.field_3 = sessionKey->field_3;
    key.field_2 = sessionKey->field_2;
    key.field_1 = sessionKey->field_1;
    areaByte0   = sessionKey->field_0;
    idx         = raw >> 12;
    key.field_0 = areaByte0;
    Gp_SyncAreaKeyIndex(&key);
    rec = Gp_GetNestedAreaRec(&key);
    /* offset + base, not `&rec->field_0[idx]`: the ROM adds the scaled index
       onto the table (`addu s0, s0, v0`). */
    entry           = (GpCdRec10*)((idx << 4) + (s32)rec->field_0);
    model->field_24 = entry->field_D;
    model->field_25 = entry->field_E;
    if (model->field_18 != NULL) {
        Tmd_ProcessStream(model);
        Tmd_ProcessStream(model);
    }
}

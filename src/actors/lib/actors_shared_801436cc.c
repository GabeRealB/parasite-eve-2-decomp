#include "common.h"

#include "actors/actors_shared_801436cc.h"

#include "gameplay/1BC.h"
#include "gameplay/D4.h"

#include "main/session.h"
#include "main/tmd.h"

void ActorsShared801436cc(void* arg0, Task* task)
{
    GpAreaKey  key;
    GpAreaKey* sessionKey;
    u8         areaByte0;
    GpAreaRec* rec;
    GpCdRec10* entry;
    TmdObject* model;
    s32        idx;
    u32        raw;

    sessionKey  = (GpAreaKey*)&gGameSession->loc;
    raw         = ((GpWorkObj*)task->parent->spawnArg2)->field_8.as_u16;
    model       = (TmdObject*)task->extra;
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
    task->state++;
}

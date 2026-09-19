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

    sessionKey = (GpAreaKey*)&gGameSession->at4.loc;
    raw        = ((GpWorkObj*)task->parent->spawnArg2)->field_8.as_u16;
    model      = (TmdObject*)task->extra;
    key.stage  = sessionKey->stage;
    key.area   = sessionKey->area;
    key.room   = sessionKey->room;
    areaByte0  = sessionKey->view;
    idx        = raw >> 12;
    key.view   = areaByte0;
    Gp_SyncAreaKeyIndex(&key);
    rec = Gp_GetNestedAreaRec(&key);
    /* offset + base, not `&rec->field_0[idx]`: the ROM adds the scaled index
       onto the table (`addu s0, s0, v0`). */
    entry        = (GpCdRec10*)((idx << 4) + (s32)rec->field_0);
    model->tpage = entry->field_D;
    model->clut  = entry->field_E;
    if (model->buffer != NULL) {
        tmdProcessStream(model);
        tmdProcessStream(model);
    }
    task->state++;
}

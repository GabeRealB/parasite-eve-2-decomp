#include "common.h"

#include "gameplay/1BC.h"
#include "gameplay/D4.h"
#include "gameplay/gameplay.h"
#include "main/session.h"
#include "main/task.h"
#include "main/tmd.h"

/// Setup state of the handler table `D_actor_403200_80131E84`: look up the
/// area placement the parent's spawn record names (its top nibble) under the
/// current session location, give the model that placement's texture page and
/// CLUT, run its stream twice when it has one, and step the task on.
void func_actor_403200_801412D0(GpEnemy* enemy, Task* task)
{
    GpAreaKey    key;
    GpAreaKey*   sessionKey;
    u8           areaByte0;
    GpAreaRec*   rec;
    GpAreaPlace* entry;
    TmdObject*   model;
    s32          idx;
    u32          raw;

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
    entry        = (GpAreaPlace*)((idx << 4) + (s32)rec->field_0);
    model->tpage = entry->tpage;
    model->clut  = entry->clut;
    if (model->buffer != NULL) {
        tmdProcessStream(model);
        tmdProcessStream(model);
    }
    task->state++;
}

/// Per-frame state of the same table: refresh the model's root coordinate. The
/// world position it then copies into a local is never used.
void func_actor_403200_8014139C(GpEnemy* enemy, Task* arg1)
{
    VECTOR sp10;

    ((TmdObject*)arg1->extra)->coords->flg = 0;
    Gp_UpdateCoord(((TmdObject*)arg1->extra)->coords);
    sp10.vx = ((TmdObject*)arg1->extra)->coords->workm.t[0];
    sp10.vy = ((TmdObject*)arg1->extra)->coords->workm.t[1];
    sp10.vz = ((TmdObject*)arg1->extra)->coords->workm.t[2];
}

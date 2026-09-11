#include "common.h"

#include "main/gfx.h"
#include "main/session.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/tmd.h"

#include "gameplay/1BC.h"
#include "gameplay/3A34.h"
#include "gameplay/3CD8.h"
#include "gameplay/D4.h"
#include "gameplay/gameplay.h"

#include "actors/actor_341700.h"

void func_actor_341700_8016A568(Task* arg0);
void func_actor_341700_8016A630(Task* arg0);

void func_actor_341700_80168124(Task* arg0)
{
    Actor341700Work* work                = (Actor341700Work*)arg0->idMap;
    void             (*states[2])(Task*) = {
        func_actor_341700_8016A568,
        func_actor_341700_8016A630,
    };

    states[(s16)work->field_420](arg0);
}

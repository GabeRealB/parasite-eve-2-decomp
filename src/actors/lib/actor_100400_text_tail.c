#include "common.h"

#include "actors/actor_100400.h"
#include "gameplay/1BC.h"
#include "main/sound.h"

extern u32 Gp_LcgState;

void Actor00400_Fn0A2F4(Task* arg0)
{
    Actor100400QuadWork* work;
    Actor100400Obj*      object;

    work   = (Actor100400QuadWork*)arg0->idMap;
    object = work->field_0;
    Actor00400_Fn03318(&work->vertices[0], &work->vertices[1],
                       &work->vertices[2], &work->vertices[3], work->intensity);
    if ((s16)object->field_40 <= 0) {
        arg0->state++;
    }
}

void Actor00400_Fn0A364(Task* arg0)
{
    Actor100400QuadWork* work;
    u8                   intensity;

    work = (Actor100400QuadWork*)arg0->idMap;
    Actor00400_Fn03318(&work->vertices[0], &work->vertices[1],
                       &work->vertices[2], &work->vertices[3], work->intensity);
    intensity       = work->intensity - 1;
    work->intensity = intensity;
    if (intensity == 0) {
        Task_Kill(arg0);
    }
}

void Actor00400_Fn0A3D4(Actor100400* arg0)
{
    Actor100400Work*   work;
    Actor100400Record* record;

    work   = arg0->field_1C;
    record = (Actor100400Record*)(work->field_64A * sizeof(Actor100400Record) + (u32)work->field_608);
    if (record->field_6 != 0) {
        record->field_6 = 0;
    }
    work->field_636 = 0;
    work->field_638 = (u16)work->field_638 + 1;
}

void Actor00400_Fn0A414(Actor100400* arg0)
{
    Actor100400Work* work;
    u16              frame;

    work            = arg0->field_1C;
    frame           = (u16)work->field_636 + 1;
    work->field_636 = frame;
    if ((s16)frame >= 0x12D) {
        Gp_DestroyEnemy((GpEnemy*)arg0->field_20, (Task*)arg0);
    }
}

void Actor00400_Fn0A468(Actor100400* arg0)
{
    Actor100400Work* work                       = arg0->field_1C;
    void             (*states[2])(Actor100400*) = {
        Actor00400_Fn0A510,
        Actor00400_Fn07400,
    };

    states[(s16)work->field_63A](arg0);
}

void Actor00400_Fn0A4BC(Actor100400* arg0)
{
    Actor100400Work* work                       = arg0->field_1C;
    void             (*states[2])(Actor100400*) = {
        Actor00400_Fn0A57C,
        Actor00400_Fn07518,
    };

    states[(s16)work->field_63A](arg0);
}

void Actor00400_Fn0A510(Actor100400* arg0)
{
    Actor100400Work* state;
    Actor100400Work* work;
    u32              random;

    work             = arg0->field_1C;
    random           = Gp_LcgState * 5 + 0x71357911;
    work->field_63E  = work->field_64E;
    state            = arg0->field_1C;
    state->field_63C = 8;
    state->field_632 = ((random >> 16) & 3) + 3;
    state->field_628 = 0x10;
    state->field_624 = 1;
    Gp_LcgState      = random;
    work->field_636  = 0;
    work->field_63A++;
}

void Actor00400_Fn0A57C(Actor100400* arg0)
{
    Actor100400Work* work;

    work            = arg0->field_1C;
    work->field_63C = 2;
    work->field_632 = 0x10;
    work->field_628 = 0x12;
    work->field_624 = 1;
    work->field_636 = 0;
    work->field_63A++;
}

INCLUDE_ASM("actors/nonmatchings/lib/actor_100400_text_tail", Actor00400_Fn0A5B8);

void Actor00400_Fn0A680(Actor100400* arg0)
{
    Actor100400Work* work;

    work            = arg0->field_1C;
    work->field_63C = 6;
    work->field_632 = 0x10;
    work->field_628 = 6;
    work->field_624 = 1;
    work->field_63A++;
}

void Actor00400_Fn0A6B0(Actor100400* arg0)
{
    Actor100400Work* work;
    s32              cond;

    work = arg0->field_1C;
    if ((work->flags_62C.half & 1) || (work->flags_62C.word & 0x102)) {
        cond = 1;
    } else {
        cond = 0;
    }
    if (cond) {
        work            = arg0->field_1C;
        work->field_638 = 2;
        work->field_63A = 0;
    }
}

void Actor00400_Fn0A704(Actor100400* arg0)
{
    Actor100400Work* work;

    work = arg0->field_1C;
    if (work->field_640 < 0x3B4) {
        work->field_638 = 2;
        work->field_63A = 0;
        return;
    }
    Actor00400_Fn00C84(arg0);
    work->field_63A++;
}

void Actor00400_Fn0A760(Actor100400* arg0)
{
    Actor100400Work* work;
    s32              cond;

    work = arg0->field_1C;
    if (work->field_640 < 0x3B4) {
        work->field_638 = 2;
        work->field_63A = 0;
        return;
    }
    Actor00400_Fn00C84(arg0);
    work = arg0->field_1C;
    if ((work->flags_62C.half & 1) || (work->flags_62C.word & 0x102)) {
        cond = 1;
    } else {
        cond = 0;
    }
    if (cond) {
        work            = arg0->field_1C;
        work->field_638 = 2;
        work->field_63A = 0;
    }
}

void Actor00400_Fn0A7F0(Actor100400* arg0)
{
    Actor100400Work* work;

    work            = arg0->field_1C;
    work->field_63C = 6;
    work->field_632 = 0x10;
    work->field_628 = 9;
    work->field_624 = 1;
    work->field_646 = 0x18;
    work->field_63A++;
}

void Actor00400_Fn0A82C(Actor100400* arg0)
{
    Actor100400Work* work;
    s32              cond;

    work = arg0->field_1C;
    if ((work->flags_62C.half & 1) || (work->flags_62C.word & 0x102)) {
        cond = 1;
    } else {
        cond = 0;
    }
    if (cond) {
        work            = arg0->field_1C;
        work->field_638 = 2;
        work->field_63A = 0;
    }
}

void Actor00400_Fn0A880(Actor100400* arg0)
{
    s32              sound;
    s32              pan;
    Actor100400Work* work;
    Actor100400Work* state;

    work  = arg0->field_1C;
    sound = ((arg0->field_20->field_8 >> 12) << 8) | 0x40040006;
    pan   = (s8)Gp_GetObjPan(arg0->field_2C->field_8);
    SndEvt_EnqueueType6(sound, pan, (s8)Gp_GetObjDepth(arg0->field_2C->field_8));
    state            = arg0->field_1C;
    state->field_63C = 6;
    state->field_632 = 0x10;
    state->field_628 = 0xC;
    state->field_624 = 1;
    work->field_63A++;
}

void Actor00400_Fn0A940(Actor100400* arg0)
{
    s32              sound;
    s32              pan;
    Actor100400Work* work;

    work            = arg0->field_1C;
    work->field_63C = 4;
    work->field_632 = 0x10;
    work->field_628 = 0xD;
    work->field_624 = 1;
    sound           = ((arg0->field_20->field_8 >> 12) << 8) | 0x40040006;
    pan             = (s8)Gp_GetObjPan(arg0->field_2C->field_8);
    SndEvt_EnqueueType6(sound, pan, (s8)Gp_GetObjDepth(arg0->field_2C->field_8));
    work->field_63A++;
}

void Actor00400_Fn0A9F4(Actor100400* arg0)
{
    Actor100400Work* work;
    Actor100400Work* state;

    work             = arg0->field_1C;
    work->field_665  = 1;
    state            = arg0->field_1C;
    state->field_63C = 8;
    state->field_632 = 0x10;
    state->field_628 = 0xF;
    state->field_624 = 1;
    work->field_610  = 0x64;
    work->field_636  = 0;
    work->field_63A++;
}

INCLUDE_ASM("actors/nonmatchings/lib/actor_100400_text_tail", Actor00400_Fn0AA40);

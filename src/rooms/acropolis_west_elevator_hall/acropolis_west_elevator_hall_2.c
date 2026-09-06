#include "common.h"

#include "gameplay/1A8.h"
#include "gameplay/3CD8.h"

#include "main/gameflag.h"
#include "main/session.h"
#include "main/task.h"

extern s32 D_acropolis_west_elevator_hall_80184620;
extern s32 D_acropolis_west_elevator_hall_80184890;
extern s32 D_acropolis_west_elevator_hall_801849C8;

INCLUDE_ASM("rooms/nonmatchings/acropolis_west_elevator_hall/acropolis_west_elevator_hall_2", func_acropolis_west_elevator_hall_8017F304);

/// Runs the one-shot cutscene hand-off for the west elevator hall: once the
/// session reports state 8 == 1 the room spawns its scripted task pair, opens
/// the story flags for the elevator and marks the sequence as running; the
/// second block retires it again when the session goes idle.
///
/// `args` and the scratch block above it are dead here - the dispatch that
/// consumed them is gone - but the compiler still reserves and fills them, so
/// they have to stay for the frame layout to match.
void func_acropolis_west_elevator_hall_8017F354(void)
{
    s32 args[2] = { 0, 4 };
    u8  scratch[0x210];
    u8  sessionState;

    if (D_acropolis_west_elevator_hall_801849C8 == 0) {
        sessionState = Game_Session->field_8;
        if (sessionState == 1) {
            D_acropolis_west_elevator_hall_801849C8 = sessionState;
            func_800E8634((s32)&D_acropolis_west_elevator_hall_80184620, 0, (s32)&D_acropolis_west_elevator_hall_80184890);
            GameFlag_SetNibble(3, 0);
            GameFlag_SetNibble(0x155, 1);
            GameFlag_SetNibble(0x7A, 1);
            func_800E3FAC(0xA2, 1);
        }
    }
    if (D_acropolis_west_elevator_hall_801849C8 == 1 && Game_Session->field_1 == 0) {
        D_acropolis_west_elevator_hall_801849C8 = 2;
    }
}

INCLUDE_ASM("rooms/nonmatchings/acropolis_west_elevator_hall/acropolis_west_elevator_hall_2", func_acropolis_west_elevator_hall_8017F418);

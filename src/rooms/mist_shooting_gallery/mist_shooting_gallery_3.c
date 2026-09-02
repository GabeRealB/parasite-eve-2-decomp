#include "common.h"

#include "gameplay/1BC.h"
#include "gameplay/268.h"
#include "gameplay/3A34.h"
#include "gameplay/3CD8.h"
#include "gameplay/3FB8.h"
#include "gameplay/gameplay.h"
#include "main/display.h"
#include "main/mem.h"
#include "main/pad.h"
#include "main/stage.h"
#include "main/task.h"
#include "main/tmd.h"
#include "main/wipsys.h"

#include "rooms/mist_shooting_gallery.h"

#include <psyq/inline_c.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>
#include <psyq/libgte.h>

#define gte_rtps_real() __asm__ volatile("nop; nop; .word 0x4A180001")

extern TaskDesc D_mist_shooting_gallery_801856B8;
extern TaskDesc D_mist_shooting_gallery_801856D0;
extern TaskDesc D_80134F94;
extern u8       D_801153F4;
extern u8       D_80073BA9;

extern MistShootingGalleryRounds D_mist_shooting_gallery_8017DB8C;

/// The wave script the round loop walks: a run of records sharing
/// `field_00` is spawned together, `0xFFF1` waits for the current wave to
/// clear and `0xFFFF` ends the course.
extern MistShootingGallerySpawn* D_mist_shooting_gallery_80186904;

/// The second course's wave script, walked exactly like
/// `D_mist_shooting_gallery_80186904` but by the bonus-round state machine.
extern MistShootingGallerySpawn* D_mist_shooting_gallery_80186910;

/// The first bonus course's wave script, walked exactly like
/// `D_mist_shooting_gallery_80186904` but by the bonus-round state machine.
extern MistShootingGallerySpawn* D_mist_shooting_gallery_8018690C;

/// The second bonus course's wave script, walked exactly like
/// `D_mist_shooting_gallery_80186904` but by the bonus-round state machine.
extern MistShootingGallerySpawn* D_mist_shooting_gallery_80186908;

/// The five course wave scripts as the one array they are: element 0 is the
/// bonus course's script, and elements 1..4 are the same pointers the round
/// scripts above reach by their own addresses (`0x80186904` .. `0x80186910`).
/// The array type is what `func_mist_shooting_gallery_80182C58` needs - an
/// array element counts as a struct reference to GCC 2.8.1's alias analysis,
/// so the load is ordered against the `work->field_04` store that precedes it.
extern MistShootingGallerySpawn* D_mist_shooting_gallery_80186900[];

/// Main-executable flag gating the countdown steps: while it is set the
/// gallery holds its current step instead of advancing the digit sprite.
extern u8 D_80071075;

/// Bonus-course variant selected before the round starts. It picks both the
/// banner sprite (`variant + 0xB`) and the colour it is drawn in (variant 2
/// uses 2 instead of 0x10).
extern s8 D_80072310;

/// Gameplay-side abort request. While it is 1 the bonus course tears itself
/// down: the state machine remembers where it was in `field_06` / `field_21`
/// and jumps to the state-9 shutdown banner.
extern s8 D_80114C0B;

extern s32 Gp_LcgState;

extern void func_8014A908(void);
extern void func_8014A9A0(void);
extern void func_8014B0D4(void);
void        func_mist_shooting_gallery_80182294(GsCOORDINATE2* coord, s16 arg1, s16 arg2, s16 arg3);
void        func_mist_shooting_gallery_801826C4(GsCOORDINATE2* coord, SVECTOR* arg1, s32 arg2, s16 arg3);
void        func_mist_shooting_gallery_80184A80(Task* arg0);
void        func_mist_shooting_gallery_8018458C(MistShootingGalleryWork* work);
u16         func_mist_shooting_gallery_80184AE0(MistShootingGalleryWork* work);
void        func_mist_shooting_gallery_80184BB8(s16 arg0, s16 arg1, s16 arg2);
GpEnemy*    func_mist_shooting_gallery_80184CD0(Task* arg0, MistShootingGallerySpawn* arg1);
void        func_8014B2B8(s16 arg0, s16 arg1, s32 arg2);
void        func_mist_shooting_gallery_801846F4(s32 arg0, s16 arg1, s32 arg2);

/// Per-frame update for one gallery muzzle-flash / tracer effect. The task's
/// `GpEffWork` holds the tracer's endpoint (`field_18`..`field_1C`), its spin
/// angle (`field_26`) and its brightness ramp (`field_24`); the handwritten GTE
/// routines below draw the beam and its glow from the task's own coordinate.
/// While `Gp_State1C` is fading (`field_4 != 0`) the effect only redraws; once
/// the fade is over it seeds a random endpoint around the coordinate's world
/// position, then fades out by 8 per frame and releases its pool block.
INCLUDE_ASM("rooms/nonmatchings/mist_shooting_gallery/mist_shooting_gallery_3", func_mist_shooting_gallery_80182064);
/// Draws one frame of the gallery's muzzle flash: a semi-transparent
/// `POLY_FT4` centred on the effect coordinate's world position, projected by
/// a single `RTPS`. Its half-size is `arg2 * 39 / otz`, and the four corners
/// are that half-size swung to `arg3` and to `arg3 + 0x400`, so the sprite
/// spins with the effect's angle. `arg1` picks one of six 40-pixel-wide
/// frames out of the texture page, and the primitive is queued twice into the
/// same OT slot. Nothing is drawn if the centre projects off-screen.
INCLUDE_ASM("rooms/nonmatchings/mist_shooting_gallery/mist_shooting_gallery_3", func_mist_shooting_gallery_80182294);
/// Draws one frame of the gallery's tracer beam: a semi-transparent
/// `POLY_FT4` stretched between the effect coordinate's world position and
/// `arg1`, the endpoint the effect picked when it spawned. Both points are
/// projected with their own `RTPS` and the quad is given a half-width of
/// `arg3 * 23 / otz`, rotated onto the beam's own screen-space angle so the
/// strip stays perpendicular to it. `arg2` selects the strip out of the
/// texture page: bit 0 picks the left or right half and bit 1 the upper or
/// lower row. Nothing is drawn if either endpoint projects off-screen.
INCLUDE_ASM("rooms/nonmatchings/mist_shooting_gallery/mist_shooting_gallery_3", func_mist_shooting_gallery_801826C4);
INCLUDE_ASM("rooms/nonmatchings/mist_shooting_gallery/mist_shooting_gallery_3", func_mist_shooting_gallery_80182B1C);
/// Per-frame update for the gallery's bonus course. START (`0x100`) aborts the
/// whole mini-game; otherwise the seventeen states run the banner countdown
/// (`field_20` steps the sprite, `D_80072310` picks which variant), seed the
/// course by spawning individual records of `D_mist_shooting_gallery_80186900[0]`
/// on a timer, and finally enter the wave loop of state 15. State 16 is the
/// out-of-ammo banner: it is entered from anywhere the moment the equipped
/// weapon's stock drops below the round's minimum, remembers the interrupted
/// state in `field_06` and returns to it once the banner has played out.
INCLUDE_ASM("rooms/nonmatchings/mist_shooting_gallery/mist_shooting_gallery_3", func_mist_shooting_gallery_80182C58);
/// Per-frame update for the gallery course itself. START (`0x100`) aborts the
/// whole mini-game; otherwise the state runs a "3, 2, 1, GO" countdown
/// (`field_20` steps the digit sprite once a second) before releasing the
/// display reference and entering the wave loop. The loop spawns every record
/// of `D_mist_shooting_gallery_80186904` that carries the current wave number,
/// draws the remaining time, and restarts the state machine once the clock
/// runs out.
INCLUDE_ASM("rooms/nonmatchings/mist_shooting_gallery/mist_shooting_gallery_3", func_mist_shooting_gallery_801831B0);
/// Per-frame update for the gallery's second bonus course. States 0-3 run the
/// "ready" banner and the hand-off wait on `Game_Session::field_4`, gated on
/// the countdown hold `D_80071075`; states 4-5 wait on the player picking up
/// item 0x40, states 6-8 count the banner up through `field_20` while
/// `D_801153F4` holds, state 9 spawns the start jingle and state 10 is the
/// wave loop over `D_mist_shooting_gallery_80186908`. `D_80072310` picks the
/// banner sprite the hand-off draws (`variant + 4`).
INCLUDE_ASM("rooms/nonmatchings/mist_shooting_gallery/mist_shooting_gallery_3", func_mist_shooting_gallery_8018341C);
/// Per-frame update for the gallery's first bonus course. States 0-3 run the
/// "ready" banner and the hand-off wait on `Game_Session::field_4`, state 4
/// seeds the first two records of `D_mist_shooting_gallery_8018690C`, states
/// 5-7 hand the player over to actor mode 2 while the banner counts up through
/// `field_20`, and state 8 is the wave loop proper. `D_80114C0B` is the abort
/// request: once it is raised the machine saves its place in `field_06` /
/// `field_21` and jumps to the state-9 shutdown banner, which restores them.
INCLUDE_ASM("rooms/nonmatchings/mist_shooting_gallery/mist_shooting_gallery_3", func_mist_shooting_gallery_801838FC);
/// Per-frame update for the gallery's second course. Same shape as
/// `func_mist_shooting_gallery_801831B0`: a countdown that steps the digit
/// sprite through `field_20` (gated on `D_80071075`), a hand-off wait on
/// `Game_Session::field_4`, then the wave loop over
/// `D_mist_shooting_gallery_80186910`. `field_22` is the abort request - once
/// it is raised the state machine jumps to the 8 -> 9 shutdown, which releases
/// the `Gp_StateF0` reference and kills the task.
INCLUDE_ASM("rooms/nonmatchings/mist_shooting_gallery/mist_shooting_gallery_3", func_mist_shooting_gallery_80183E78);
INCLUDE_ASM("rooms/nonmatchings/mist_shooting_gallery/mist_shooting_gallery_3", func_mist_shooting_gallery_801842D0);
INCLUDE_ASM("rooms/nonmatchings/mist_shooting_gallery/mist_shooting_gallery_3", func_mist_shooting_gallery_80184470);
INCLUDE_ASM("rooms/nonmatchings/mist_shooting_gallery/mist_shooting_gallery_3", func_mist_shooting_gallery_8018458C);
INCLUDE_ASM("rooms/nonmatchings/mist_shooting_gallery/mist_shooting_gallery_3", func_mist_shooting_gallery_801846F4);
INCLUDE_ASM("rooms/nonmatchings/mist_shooting_gallery/mist_shooting_gallery_3", func_mist_shooting_gallery_801847D4);
void func_mist_shooting_gallery_801848B4(void)
{
    GpEnemy*     enemy;
    TmdObject*   obj;
    GpCoordPose* coord;

    enemy = Gp_SpawnEnemyFromTable(&D_80134F94, 0, 0x200D, NULL);
    if (enemy != NULL) {
        obj           = (TmdObject*)enemy->task->extra;
        obj->field_24 = 0;
        obj->field_25 = 2;
        Tmd_ProcessStream(obj);
        Tmd_ProcessStream(obj);
        coord             = (GpCoordPose*)((TmdObject*)enemy->task->extra)->field_8;
        coord->coord.t[0] = 0x1770;
        coord->coord.t[2] = 0xBB8;
        coord->coord.t[1] = 0;
        enemy->field_A    = 0x900;
    }
}
void func_mist_shooting_gallery_80184954(void)
{
    MistShootingGalleryWork* work = (MistShootingGalleryWork*)D_mist_shooting_gallery_8018E0C4->idMap;

    work->field_1F = 1;
}

s32 func_mist_shooting_gallery_80184970(s32 arg0)
{
    MistShootingGalleryWork* work = (MistShootingGalleryWork*)D_mist_shooting_gallery_8018E0C4->idMap;
    s32                      ret  = 0;

    if (work->difficulty < 3) {
        ret = arg0 >= 0xC8;
    } else if (arg0 >= 0x12C) {
        ret = 1;
    }
    return ret;
}
INCLUDE_ASM("rooms/nonmatchings/mist_shooting_gallery/mist_shooting_gallery_3", func_mist_shooting_gallery_801849BC);

INCLUDE_ASM("rooms/nonmatchings/mist_shooting_gallery/mist_shooting_gallery_3", func_mist_shooting_gallery_80184A14);
void func_mist_shooting_gallery_80184A80(Task* arg0)
{
    GameActor* actor;

    actor                             = ((GpActorWork*)Game_GetPtrSlot(3))->actor;
    actor->field_97B                  = 0;
    actor->field_983                  = 7;
    ((GpObj*)actor->field_AC)->flags |= 0x2000;
    Display_ReleaseRef();
    ((void (*)(Task*, s32))Gp_ReleaseStateF0Clear)(arg0, 0);
    Task_Kill(arg0);
}

u16 func_mist_shooting_gallery_80184AE0(MistShootingGalleryWork* work)
{
    u16 temp = work->field_02;

    if ((temp != 0) && (D_801153F4 == 0)) {
        work->field_02 = temp - 1;
    }
    return work->field_02;
}
INCLUDE_ASM("rooms/nonmatchings/mist_shooting_gallery/mist_shooting_gallery_3", func_mist_shooting_gallery_80184B10);
void func_mist_shooting_gallery_80184BB8(s16 arg0, s16 arg1, s16 arg2)
{
    func_8014B2B8(arg0, arg1, 0xD0);
    Display_InitModeObj(&D_mist_shooting_gallery_801856D0, arg2, 0, 0);
}
void func_mist_shooting_gallery_80184C0C(Task* arg0)
{
    switch (arg0->state) {
        case 0:
            arg0->state         = 1;
            arg0->killCountdown = 0x10;
        case 1:
            if (arg0->killCountdown != 0) {
                arg0->killCountdown--;
                goto call_func;
            }
        pad_check:
            asm volatile("" : : "i"(&&pad_check));
            if (Pad_CheckButtons(0, 1, arg0->spawnArg1) != 0) {
                arg0->state = arg0->state + 1;
                return;
            }
        call_func:
            func_8014B0D4();
            return;
        case 2:
            Task_Kill(arg0);
            Stage_SetEndingFlag();
            return;
    }
}

INCLUDE_ASM("rooms/nonmatchings/mist_shooting_gallery/mist_shooting_gallery_3", func_mist_shooting_gallery_80184CD0);
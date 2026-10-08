/* Part of the Mad Chaser library; see mad_chaser.h. */

#include "mad_chaser_frame_shadows.inc.c"

/// Advances combat behavior, animation and contacts, then refreshes presentation.
///
/// Requires live enemy/model/work, initialized nine-part animation and behavior
/// state 0..10. Running frames track the nearer player and dispatch unless a
/// pull or vanish command takes over, relax spine yaw, pin part 6 when anchored,
/// rebuild rotation and apply contacts. Fatal blast hits enter death even while
/// busy; other death transitions wait for busy to clear. Scripted deaths require
/// the extended task table. Paused frames still update color and ground shadows;
/// hidden frames suppress model drawing and return. Borrows task-owned storage
/// and requires initialized rendering scratch and frame-arena space.
static void _madChaserCombatTick(Task* task)
{
    enum {
        MAD_CHASER_COMBAT_ANCHOR_PART            = 6,
        MAD_CHASER_COMBAT_SPINE_DECAY_MULTIPLIER = 16,
        MAD_CHASER_COMBAT_SPINE_DECAY_SHIFT      = 9
    };
    Enemy*          enemy     = task->spawnArg2.pointer;
    TmdObject*      model     = task->extra.tmd;
    MadChaserWork*  work      = task->work;
    GfxCoord*       rootCoord = model->coords;
    TaskFuncTable11 states    = gMadChaserCombatStates;
    s32             spineYawBits;

    switch (gSceneCombatState.actorControl) {
        case SCENE_COMBAT_ACTORS_HIDDEN:
            model->flags |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
            return;
        case SCENE_COMBAT_ACTORS_RUNNING:
            work->frameCount++;
            _madChaserTrackPlayer(task);
            if (_madChaserTakePullOrVanishCommand(task) == 0) {
                states.funcs[(s16)work->state](task);
            }
            _madChaserTickAnim(task);
            // Narrow the correction before shifting; angles use 4096 units/turn.
            spineYawBits   = (u16)work->spineYaw;
            work->spineYaw = spineYawBits + ((s16)(-(spineYawBits * MAD_CHASER_COMBAT_SPINE_DECAY_MULTIPLIER)) >> MAD_CHASER_COMBAT_SPINE_DECAY_SHIFT);
            _madChaserTwistSpine(task);
            if (work->anchored == 1) {
                _madChaserPinPart(task, MAD_CHASER_COMBAT_ANCHOR_PART, &work->leapAnchorPos);
            }
            _madChaserUpdateRotation(task);
            _madChaserApplyContacts(task, 0);
            if (work->leapCooldown != 0) {
                work->leapCooldown--;
            }
            // Contacts can request death after this frame's behavior has run.
            if (work->hitTaken != 0 && work->hitReaction == MAD_CHASER_HIT_REACTION_BLAST && enemy->hp <= 0) {
                _madChaserEnterTaskState(task, MAD_CHASER_TASK_DEATH);
            }
            if (work->busy == 0 && enemy->hp <= 0) {
                _madChaserEnterTaskState(task, MAD_CHASER_TASK_DEATH);
            } else if (work->command == MAD_CHASER_COMMAND_DROP_DEATH && work->busy == 0) {
                _madChaserEnterTaskState(task, MAD_CHASER_TASK_DROP_DEATH);
            } else if (work->command == MAD_CHASER_COMMAND_SHRINK_DEATH && work->busy == 0) {
                _madChaserEnterTaskState(task, MAD_CHASER_TASK_SHRINK_DEATH);
            }
            rootCoord->composeStamp = GRAPHICS_COORD_DIRTY;
            // Fall through so running and paused frames share presentation.
        case SCENE_COMBAT_ACTORS_PAUSED:
            _madChaserUpdateColor(task->spawnArg2.pointer, &task->extra.tmd->coords[1]);
            _madChaserDrawFrameShadows(task);
            return;
    }
}

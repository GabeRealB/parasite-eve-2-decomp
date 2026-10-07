/* Part of the Glutton library; see glutton.h. */

/// Draws a rain blob's growing floor shadow and drops it into its landing burst.
///
/// Descent begins on tick 20, at 600 plus `speedJitter` world units per tick.
/// Crossing floor y = 0 clamps there, resets the timer, signals a live child
/// effect to burst, plays the owner's landing sound and advances to splatting.
/// The attack body's independent coordinate follows the blob and its contacts
/// are cleared each tick. Requires a live coordinate body, parent enemy and
/// initialized `GluttonProjectileWork`; shutdown unlinks and destroys the blob.
static void _gluttonRainFall(Enemy* enemy, Task* task)
{
    enum { GLUTTON_RAIN_SHADOW_LEAD_TICKS = 20,
           GLUTTON_RAIN_DROP_SPEED        = 600,
           GLUTTON_RAIN_EFFECT_BURST      = 2 };
    GluttonProjectileWork* work;
    GluttonCoord           shadowCoord;
    Enemy*                 owner;
    s32                    soundId;
    s32                    audioPan;

    work = task->work;
    if (gGluttonEnded == 1) {
        worldCollisionUnlinkBody(&work->attackBody);
        enemyDestroy(enemy, task);
        return;
    }

    work->stateTicks++;
    shadowCoord.node.parent = &gGfxViewCoord;
    gfxSetRotIdentity(&shadowCoord.node.coord);
    gfxRotMatrixY(&shadowCoord.node.coord, 0, GRAPHICS_ROTATION_REPLACE);

    shadowCoord.node.coord.t[0]   = task->extra.coordBody->coord->coord.t[0];
    shadowCoord.node.coord.t[1]   = 0;
    shadowCoord.node.coord.t[2]   = task->extra.coordBody->coord->coord.t[2];
    shadowCoord.node.composeStamp = GRAPHICS_COORD_DIRTY;
    actorRenderComposeCoord(&shadowCoord.node);

    // Warn of the landing point while the blob waits overhead.
    effectDrawGroundShadow(MATRIX_TRANS(&shadowCoord.node.workm), (s16)(work->stateTicks * 8 + 0x80),
                           gRoomEffectState->groundShadowShade);

    if (work->stateTicks >= GLUTTON_RAIN_SHADOW_LEAD_TICKS) {
        task->extra.coordBody->coord->coord.t[1] =
            task->extra.coordBody->coord->coord.t[1] + (work->speedJitter + GLUTTON_RAIN_DROP_SPEED);
        if (task->extra.coordBody->coord->coord.t[1] > 0) {
            owner                                    = task->parent->spawnArg2.pointer;
            task->extra.coordBody->coord->coord.t[1] = 0;
            work->stateTicks                         = 0;
            if (work->rainEffect != NULL) {
                work->rainEffect->task->spawnArg1.value = GLUTTON_RAIN_EFFECT_BURST;
            }
            task->state++;
            soundId  = ((owner->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | SOUND_CHARACTER(SOUND_BANK_GLUTTON, 0x0C);
            audioPan = (s8)worldCoordGetOriginAudioPan(task->extra.coordBody->coord);
            sndEvtRequestScriptStart(soundId, audioPan, (s8)worldCoordGetOriginAudioDepth(task->extra.coordBody->coord));
        }
    }

    task->extra.coordBody->coord->composeStamp = GRAPHICS_COORD_DIRTY;
    worldCollisionClearContacts(work->attackContacts);
    // Carry the attack body along with the blob.
    work->bodyCoord.node.coord.t[0]   = task->extra.coordBody->coord->coord.t[0];
    work->bodyCoord.node.coord.t[1]   = task->extra.coordBody->coord->coord.t[1];
    work->bodyCoord.node.coord.t[2]   = task->extra.coordBody->coord->coord.t[2];
    work->bodyCoord.node.composeStamp = GRAPHICS_COORD_DIRTY;
    actorRenderComposeCoord(&work->bodyCoord.node);
}

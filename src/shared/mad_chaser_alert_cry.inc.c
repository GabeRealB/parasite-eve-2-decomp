/* Part of the Mad Chaser library; see mad_chaser.h. */

/// Starts the alert cry animation and sounds it while the enemy is alive.
///
/// Starts clip 9 with a four-normal-frame blend, resets the state counter and
/// advances to the wait step. The sound uses bank 0x402C, entry 2, tagged with
/// this enemy's placement index (0..15), with signed-byte spatial pan and depth.
/// Requires live work, an Enemy spawn argument and loaded model/animation data.
static void _madChaserAlertCry(Task* task)
{
    enum {
        MAD_CHASER_ALERT_CRY_ANIM  = 9,
        MAD_CHASER_ALERT_CRY_SOUND = 0x402C0002,
    };
    MadChaserWork* work;
    Enemy*         enemy;
    Enemy*         soundEnemy;
    s32            soundId;
    s32            panOffset;

    work                  = task->work;
    enemy                 = task->spawnArg2.pointer;
    work->animBlendFrames = 4;
    work->animRate        = ANIMATION_RATE_ONE;
    work->animId          = MAD_CHASER_ALERT_CRY_ANIM;
    work->animRequest     = MAD_CHASER_ANIM_REQUEST_BLEND;
    work->stateFrames     = 0;
    work->subState++;
    if (enemy->hp > 0) {
        soundEnemy = task->spawnArg2.pointer;
        soundId    = ((soundEnemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | MAD_CHASER_ALERT_CRY_SOUND;
        panOffset  = (s8)worldCoordGetOriginAudioPan(task->extra.tmd->coords);
        sndEvtRequestScriptStart(soundId, panOffset, (s8)worldCoordGetOriginAudioDepth(task->extra.tmd->coords));
    }
}

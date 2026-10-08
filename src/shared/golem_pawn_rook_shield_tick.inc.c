/* Part of the Pawn and Rook GOLEM library; see golem_pawn_rook.h. */

/// Follows the body's visibility and breaks its attached shield on request.
///
/// shield remains parented to a live GOLEM body with shared lighting and work.
/// shieldBreakStep 0 refreshes its attached coordinate, 1 spawns four explosion
/// sprites and the spatial break cue, and 2 requests task teardown on the next
/// call. The unused enemy parameter preserves the lifecycle callback signature.
static void _golemPawnRookShieldTick(Enemy* unusedEnemy, Task* shield)
{
    enum {
        GOLEM_PAWN_ROOK_SHIELD_ATTACHED               = 0,
        GOLEM_PAWN_ROOK_SHIELD_BREAK                  = 1,
        GOLEM_PAWN_ROOK_SHIELD_BROKEN                 = 2,
        GOLEM_PAWN_ROOK_SHIELD_CORE_ARGUMENT          = 0x10002600, // size 1536, period 2, no secondary sprites
        GOLEM_PAWN_ROOK_SHIELD_FRAGMENT_ARGUMENT      = 0x01002600, // size 1536, period 2, velocity factor 1
        GOLEM_PAWN_ROOK_SHIELD_FAST_FRAGMENT_ARGUMENT = 0x02002600, // same sprite with velocity factor 2
    };

    Task*              body;
    TmdObject*         shieldModel;
    TmdObject*         bodyModel;
    GolemPawnRookWork* work;
    GfxCoord*          shieldRoot;
    s16                breakStep;
    s32                soundId;
    s32                audioPan;

    body               = shield->parent;
    shieldModel        = shield->extra.tmd;
    bodyModel          = body->extra.tmd;
    work               = body->work;
    shieldRoot         = shieldModel->coords;
    shieldModel->flags = bodyModel->flags;
    breakStep          = work->shieldBreakStep;

    switch (breakStep) {
        case GOLEM_PAWN_ROOK_SHIELD_ATTACHED:
            shieldRoot->composeStamp = GRAPHICS_COORD_DIRTY;
            return;
        case GOLEM_PAWN_ROOK_SHIELD_BREAK:
            effectSpawn(EFFECT_EXPLOSION, shieldRoot, GOLEM_PAWN_ROOK_SHIELD_CORE_ARGUMENT, NULL);
            effectSpawn(EFFECT_EXPLOSION, shieldRoot, GOLEM_PAWN_ROOK_SHIELD_FRAGMENT_ARGUMENT, NULL);
            effectSpawn(EFFECT_EXPLOSION, shieldRoot, GOLEM_PAWN_ROOK_SHIELD_FRAGMENT_ARGUMENT, NULL);
            effectSpawn(EFFECT_EXPLOSION, shieldRoot, GOLEM_PAWN_ROOK_SHIELD_FAST_FRAGMENT_ARGUMENT, NULL);
            work->shieldBreakStep = GOLEM_PAWN_ROOK_SHIELD_BROKEN;
            soundId               = gGolemPawnRookBurstCue |
                      ((((Enemy*)shield->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << GOLEM_PAWN_ROOK_SOUND_INSTANCE_SHIFT);
            audioPan = (s8)worldCoordGetOriginAudioPan(shieldRoot);
            sndEvtRequestScriptStart(soundId, audioPan, (s8)worldCoordGetOriginAudioDepth(shieldRoot));
            return;
        case GOLEM_PAWN_ROOK_SHIELD_BROKEN:
            shield->state = breakStep;
            return;
    }
}

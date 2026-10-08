#include "main/random.h"

/* Part of the Knight and Bishop GOLEM library; see golem_knight_bishop.h. */

/// Starts a random flicker half-period and sets both appearance fade divisors.
///
/// `work` retains the new countdown and divisors for the appearance update.
/// Two successive LCG draws choose a 2..17-frame countdown and add 0..15 to
/// produce the shared 2..32-frame fade divisor. No visibility or tint changes here.
static inline void _golemKnightBishopResetFlickerPeriod(GolemKnightBishopWork* work)
{
    enum {
        GOLEM_KNIGHT_BISHOP_FLICKER_RANDOM_MASK = 15,
        GOLEM_KNIGHT_BISHOP_FLICKER_MIN_FRAMES  = 2,
    };
    s16 fadeFrames;
    work->flickerTimer           = (((gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT) >> 16) & GOLEM_KNIGHT_BISHOP_FLICKER_RANDOM_MASK) + GOLEM_KNIGHT_BISHOP_FLICKER_MIN_FRAMES;
    fadeFrames                   = work->flickerTimer + (((gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT) >> 16) & GOLEM_KNIGHT_BISHOP_FLICKER_RANDOM_MASK);
    work->translucencyFadeFrames = fadeFrames;
    work->colorBlendFadeFrames   = fadeFrames;
}

/// Updates visibility, colour, shadow, feint shrink and hit flicker.
///
/// `task` owns live GOLEM work, model and Enemy spawn data. Fade durations must
/// be positive frame divisors: translucency spans 0..255, colour blend uses Q12
/// and the shadow reaches shade 128. A feint uses 11/16 colour, saves its root
/// matrix and shrinks before hiding. Flicker consumes two LCG draws per half
/// period and emits sparks on odd animation frames. Signed s16 intermediate
/// ramps narrow before their clamps; integer division can extend nominal fades.
static void _golemKnightBishopUpdateAppearance(Task* task)
{
    enum {
        GOLEM_KNIGHT_BISHOP_FEINT_COLOR_BLEND          = TMD_OBJECT_COLOR_BLEND_ONE * 11 / 16,
        GOLEM_KNIGHT_BISHOP_FLICKER_TRANSLUCENCY_LIMIT = 128,
        GOLEM_KNIGHT_BISHOP_SHRINK_SQUASH              = 0,
        GOLEM_KNIGHT_BISHOP_SHRINK_NARROW              = 1,
        GOLEM_KNIGHT_BISHOP_SHRINK_HELD                = 2,
        GOLEM_KNIGHT_BISHOP_TINT_NONE                  = 0,
        GOLEM_KNIGHT_BISHOP_TINT_DIM_BLUE              = 1,
    };
    SVECTOR*               sparkOffset;
    GolemKnightBishopWork* work;
    TmdObject*             model;
    GfxCoord*              root;
    s32                    vanishCue;
    s32                    audioPan;
    s32                    nextWidthScale;
    s32                    nextHeightScale;
    s32                    squashedHeight;
    s32                    heightScale;
    s16                    nextTranslucency;
    s16                    nextShadowShade;
    s16                    flickerFramesLeft;

    sparkOffset = SCRATCH_STACK_RESERVE_BLOCK(SVECTOR);
    work        = task->work;
    model       = task->extra.tmd;
    root        = model->coords;
    switch (work->fadeState) {
        case GOLEM_KNIGHT_BISHOP_FADE_HIDDEN:
            task->extra.tmd->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
            work->shadowShade      = GOLEM_KNIGHT_BISHOP_SHADOW_HIDDEN;
            work->hurtBody.flags  &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
            if (work->appearSound != 0) {
                sndEvtRequestScriptStop(work->appearSound, SOUND_SCRIPT_STOP_KEEP_RELEASE);
                work->appearSound = 0;
            }
            if (work->vanishSound != 0) {
                sndEvtRequestScriptStop(work->vanishSound, SOUND_SCRIPT_STOP_KEEP_RELEASE);
                work->vanishSound = 0;
            }
            break;
        case GOLEM_KNIGHT_BISHOP_FADE_APPEAR:
            model->shading.colorBlend += TMD_OBJECT_COLOR_BLEND_ONE / work->colorBlendFadeFrames;
            if (model->shading.colorBlend >= TMD_OBJECT_COLOR_BLEND_ONE) {
                model->shading.colorBlend = TMD_OBJECT_COLOR_BLEND_ONE;
                nextTranslucency          = work->translucency - GOLEM_KNIGHT_BISHOP_TRANSLUCENCY_INVISIBLE / work->translucencyFadeFrames;
                work->translucency        = nextTranslucency;
                if (nextTranslucency <= 0) {
                    work->translucency = 0;
                    work->fadeState    = GOLEM_KNIGHT_BISHOP_FADE_SHOWN;
                    if (work->appearSound != 0) {
                        sndEvtRequestScriptStop(work->appearSound, SOUND_SCRIPT_STOP_KEEP_RELEASE);
                        work->appearSound = 0;
                    }
                }
            }
            nextShadowShade   = work->shadowShade + GOLEM_KNIGHT_BISHOP_SHADOW_FULL_SHADE / work->translucencyFadeFrames;
            work->shadowShade = nextShadowShade;
            if (nextShadowShade >= GOLEM_KNIGHT_BISHOP_SHADOW_FULL_SHADE) {
                work->shadowShade = GOLEM_KNIGHT_BISHOP_SHADOW_FULL_SHADE;
            }
            break;
        case GOLEM_KNIGHT_BISHOP_FADE_SHOWN:
            work->shadowShade = GOLEM_KNIGHT_BISHOP_SHADOW_FULL_SHADE;
            if (work->appearSound != 0) {
                sndEvtRequestScriptStop(work->appearSound, SOUND_SCRIPT_STOP_KEEP_RELEASE);
                work->appearSound = 0;
            }
            if (work->vanishSound != 0) {
                sndEvtRequestScriptStop(work->vanishSound, SOUND_SCRIPT_STOP_KEEP_RELEASE);
                work->vanishSound = 0;
            }
            break;
        case GOLEM_KNIGHT_BISHOP_FADE_VANISH:
            nextTranslucency   = work->translucency + GOLEM_KNIGHT_BISHOP_TRANSLUCENCY_INVISIBLE / work->translucencyFadeFrames;
            work->translucency = nextTranslucency;
            if (nextTranslucency >= GOLEM_KNIGHT_BISHOP_TRANSLUCENCY_INVISIBLE) {
                work->translucency         = GOLEM_KNIGHT_BISHOP_TRANSLUCENCY_INVISIBLE;
                model->shading.colorBlend -= TMD_OBJECT_COLOR_BLEND_ONE / work->colorBlendFadeFrames;
                if (model->shading.colorBlend <= 0) {
                    model->shading.colorBlend = 0;
                    work->fadeState           = GOLEM_KNIGHT_BISHOP_FADE_HIDDEN;
                    if (work->vanishSound != 0) {
                        sndEvtRequestScriptStop(work->vanishSound, SOUND_SCRIPT_STOP_KEEP_RELEASE);
                        work->vanishSound = 0;
                    }
                    vanishCue = gGolemKnightBishopFadeCue | ((((Enemy*)task->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8);
                    audioPan  = (s8)worldCoordGetOriginAudioPan(root);
                    sndEvtRequestScriptStart(vanishCue, audioPan, (s8)worldCoordGetOriginAudioDepth(root));
                }
            }
            nextShadowShade   = work->shadowShade - GOLEM_KNIGHT_BISHOP_SHADOW_FULL_SHADE / work->translucencyFadeFrames;
            work->shadowShade = nextShadowShade;
            if (nextShadowShade < 0) {
                work->shadowShade = GOLEM_KNIGHT_BISHOP_SHADOW_HIDDEN;
            }
            break;
        case GOLEM_KNIGHT_BISHOP_FADE_FEINT_APPEAR:
            model->shading.colorBlend += (GOLEM_KNIGHT_BISHOP_FEINT_COLOR_BLEND) / work->colorBlendFadeFrames;
            if (model->shading.colorBlend >= GOLEM_KNIGHT_BISHOP_FEINT_COLOR_BLEND) {
                model->shading.colorBlend = GOLEM_KNIGHT_BISHOP_FEINT_COLOR_BLEND;
                nextTranslucency          = work->translucency - GOLEM_KNIGHT_BISHOP_TRANSLUCENCY_INVISIBLE / work->translucencyFadeFrames;
                work->translucency        = nextTranslucency;
                if (nextTranslucency <= 0) {
                    work->fadeState       = GOLEM_KNIGHT_BISHOP_FADE_FEINT_SHOWN;
                    work->translucency    = 0;
                    work->scale.vx        = 0x1000;
                    work->scale.vy        = 0x1000;
                    work->scale.vz        = 0x1000;
                    work->unscaledRootMtx = task->extra.tmd->coords[0].coord;
                    work->shrinkStep      = GOLEM_KNIGHT_BISHOP_SHRINK_SQUASH;
                }
            }
            work->shadowShade = GOLEM_KNIGHT_BISHOP_SHADOW_HIDDEN;
            break;
        case GOLEM_KNIGHT_BISHOP_FADE_FEINT_SHOWN:
            work->shadowShade = GOLEM_KNIGHT_BISHOP_SHADOW_HIDDEN;
            break;
        case GOLEM_KNIGHT_BISHOP_FADE_FEINT_SHRINK:
            work->hurtBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
            switch (work->shrinkStep) {
                case GOLEM_KNIGHT_BISHOP_SHRINK_SQUASH:
                    heightScale = work->scale.vy;
                    if (work->feintBroken != 0) {
                        squashedHeight = heightScale - (GOLEM_KNIGHT_BISHOP_SCALE_ONE / 4);
                    } else {
                        squashedHeight = heightScale - (GOLEM_KNIGHT_BISHOP_SCALE_ONE / 8);
                    }
                    work->scale.vy = squashedHeight;
                    if (squashedHeight <= (GOLEM_KNIGHT_BISHOP_SCALE_ONE / 2)) {
                        work->shrinkStep = GOLEM_KNIGHT_BISHOP_SHRINK_NARROW;
                    }
                    break;
                case GOLEM_KNIGHT_BISHOP_SHRINK_NARROW:
                    if (work->feintBroken != 0) {
                        nextWidthScale  = work->scale.vx - (GOLEM_KNIGHT_BISHOP_SCALE_ONE / 8);
                        nextHeightScale = work->scale.vy + (GOLEM_KNIGHT_BISHOP_SCALE_ONE / 4);
                        work->scale.vx  = nextWidthScale;
                        work->scale.vy  = nextHeightScale;
                    } else {
                        nextWidthScale  = work->scale.vx - GOLEM_KNIGHT_BISHOP_SCALE_ONE / 16;
                        nextHeightScale = work->scale.vy + (GOLEM_KNIGHT_BISHOP_SCALE_ONE / 8);
                        work->scale.vx  = nextWidthScale;
                        work->scale.vy  = nextHeightScale;
                    }
                    if (work->scale.vx <= (GOLEM_KNIGHT_BISHOP_SCALE_ONE / 2)) {
                        work->shrinkStep = GOLEM_KNIGHT_BISHOP_SHRINK_HELD;
                    }
                    break;
            }
            _golemKnightBishopApplyScale(task);
            nextTranslucency   = work->translucency + GOLEM_KNIGHT_BISHOP_TRANSLUCENCY_INVISIBLE / work->translucencyFadeFrames;
            work->translucency = nextTranslucency;
            if (nextTranslucency >= GOLEM_KNIGHT_BISHOP_TRANSLUCENCY_INVISIBLE) {
                work->translucency         = GOLEM_KNIGHT_BISHOP_TRANSLUCENCY_INVISIBLE;
                model->shading.colorBlend -= TMD_OBJECT_COLOR_BLEND_ONE / work->colorBlendFadeFrames;
                if (model->shading.colorBlend <= 0) {
                    model->shading.colorBlend = 0;
                    work->fadeState           = GOLEM_KNIGHT_BISHOP_FADE_HIDDEN;
                    gfxSetRotIdentity(&task->extra.tmd->coords[0].coord);
                    task->extra.tmd->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
                }
            }
            work->shadowShade = GOLEM_KNIGHT_BISHOP_SHADOW_HIDDEN;
            break;
        case GOLEM_KNIGHT_BISHOP_FADE_FLICKER_START:
            work->fadeState = GOLEM_KNIGHT_BISHOP_FADE_FLICKER_DIM;
            _golemKnightBishopResetFlickerPeriod(work);
            break;
        case GOLEM_KNIGHT_BISHOP_FADE_FLICKER_DIM:
            nextTranslucency   = work->translucency + GOLEM_KNIGHT_BISHOP_TRANSLUCENCY_INVISIBLE / work->translucencyFadeFrames;
            work->translucency = nextTranslucency;
            if (nextTranslucency >= GOLEM_KNIGHT_BISHOP_FLICKER_TRANSLUCENCY_LIMIT) {
                work->translucency         = GOLEM_KNIGHT_BISHOP_FLICKER_TRANSLUCENCY_LIMIT;
                model->shading.colorBlend -= TMD_OBJECT_COLOR_BLEND_ONE / work->colorBlendFadeFrames;
                if (model->shading.colorBlend <= TMD_OBJECT_COLOR_BLEND_ONE / 2) {
                    model->shading.colorBlend = TMD_OBJECT_COLOR_BLEND_ONE / 2;
                }
            }
            nextShadowShade   = work->shadowShade - GOLEM_KNIGHT_BISHOP_SHADOW_FULL_SHADE / work->translucencyFadeFrames;
            work->shadowShade = nextShadowShade;
            if (nextShadowShade < 0) {
                work->shadowShade = GOLEM_KNIGHT_BISHOP_SHADOW_HIDDEN;
            }
            flickerFramesLeft  = work->flickerTimer - 1;
            work->flickerTimer = flickerFramesLeft;
            if (flickerFramesLeft <= 0) {
                work->fadeState = GOLEM_KNIGHT_BISHOP_FADE_FLICKER_BRIGHT;
                _golemKnightBishopResetFlickerPeriod(work);
            }
            if (work->tintRequest == GOLEM_KNIGHT_BISHOP_TINT_NONE) {
                work->tintRequest = GOLEM_KNIGHT_BISHOP_TINT_DIM_BLUE;
            }
            if (work->animFrame & 1) {
                sparkOffset->vx = 0;
                sparkOffset->vy = -(((gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT) >> 16) & 0xFF);
                sparkOffset->vz = ((gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT) >> 16) & 0xFF;
                effectSpawn(EFFECT_FLASH_BURST, &task->extra.tmd->coords[3], 0x100, sparkOffset);
            }
            break;
        case GOLEM_KNIGHT_BISHOP_FADE_FLICKER_BRIGHT:
            model->shading.colorBlend += TMD_OBJECT_COLOR_BLEND_ONE / work->colorBlendFadeFrames;
            if (model->shading.colorBlend >= TMD_OBJECT_COLOR_BLEND_ONE) {
                model->shading.colorBlend = TMD_OBJECT_COLOR_BLEND_ONE;
                nextTranslucency          = work->translucency - GOLEM_KNIGHT_BISHOP_TRANSLUCENCY_INVISIBLE / work->translucencyFadeFrames;
                work->translucency        = nextTranslucency;
                if (nextTranslucency <= 0) {
                    work->translucency = 0;
                }
            }
            nextShadowShade   = work->shadowShade + GOLEM_KNIGHT_BISHOP_SHADOW_FULL_SHADE / work->translucencyFadeFrames;
            work->shadowShade = nextShadowShade;
            if (nextShadowShade >= GOLEM_KNIGHT_BISHOP_SHADOW_FULL_SHADE) {
                work->shadowShade = GOLEM_KNIGHT_BISHOP_SHADOW_FULL_SHADE;
            }
            flickerFramesLeft  = work->flickerTimer - 1;
            work->flickerTimer = flickerFramesLeft;
            if (flickerFramesLeft <= 0) {
                work->fadeState = GOLEM_KNIGHT_BISHOP_FADE_FLICKER_DIM;
                _golemKnightBishopResetFlickerPeriod(work);
            }
            if (work->animFrame & 1) {
                sparkOffset->vx = 0;
                sparkOffset->vy = -(((gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT) >> 16) & 0xFF);
                sparkOffset->vz = ((gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT) >> 16) & 0xFF;
                effectSpawn(EFFECT_FLASH_BURST, &task->extra.tmd->coords[3], 0x100, sparkOffset);
            }
            break;
    }
    SCRATCH_STACK_RELEASE_BLOCK(SVECTOR);
}

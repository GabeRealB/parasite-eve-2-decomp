#include "main/random.h"

/* Part of the Knight and Bishop GOLEM library; see golem_knight_bishop.h. */

/// Carries out the change of appearance `fadeState` asks for. The appearance
/// and the vanish fade the display object's `shading.colorBlend`,
/// `translucency` and `shadowShade` in and out over `colorBlendFadeFrames` and
/// `translucencyFadeFrames`, stopping `appearSound` / `vanishSound` as they
/// finish. A feint appears to 11/16 colour weight and saves the root matrix
/// into `unscaledRootMtx`; its shrink winds `scale.vy` and then `scale.vx`
/// down by `shrinkStep` as it vanishes, then resets the root matrix to
/// identity. The flicker alternates between its two halves on `flickerTimer`,
/// each of LCG-rolled length, spawning effect 0x600E0 at the fourth part on
/// odd animation frames.
void golemKnightBishopTranslucencyFade(Task* arg0)
{
    SVECTOR*               sc;
    GolemKnightBishopWork* work;
    TmdObject*             obj;
    GfxCoord*              coord;
    s32                    snd;
    s32                    pan;
    s32                    v;
    s32                    w;
    s32                    sy;
    s32                    y;
    u32                    random;
    s16                    t;

    sc    = (SVECTOR*)SCRATCH_STACK_RESERVE_BYTES(8);
    work  = arg0->work;
    obj   = arg0->extra.tmd;
    coord = obj->coords;
    switch (work->fadeState) {
        case GOLEM_KNIGHT_BISHOP_FADE_HIDDEN:
            arg0->extra.tmd->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
            work->shadowShade      = -1;
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
            obj->shading.colorBlend += TMD_OBJECT_COLOR_BLEND_ONE / work->colorBlendFadeFrames;
            if (obj->shading.colorBlend >= TMD_OBJECT_COLOR_BLEND_ONE) {
                obj->shading.colorBlend = TMD_OBJECT_COLOR_BLEND_ONE;
                t                       = work->translucency - 0xFF / work->translucencyFadeFrames;
                work->translucency      = t;
                if (t <= 0) {
                    work->translucency = 0;
                    work->fadeState    = GOLEM_KNIGHT_BISHOP_FADE_SHOWN;
                    if (work->appearSound != 0) {
                        sndEvtRequestScriptStop(work->appearSound, SOUND_SCRIPT_STOP_KEEP_RELEASE);
                        work->appearSound = 0;
                    }
                }
            }
            t                 = work->shadowShade + 0x80 / work->translucencyFadeFrames;
            work->shadowShade = t;
            if (t >= 0x80) {
                work->shadowShade = 0x80;
            }
            break;
        case GOLEM_KNIGHT_BISHOP_FADE_SHOWN:
            work->shadowShade = 0x80;
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
            t                  = work->translucency + 0xFF / work->translucencyFadeFrames;
            work->translucency = t;
            if (t >= 0xFF) {
                work->translucency       = 0xFF;
                obj->shading.colorBlend -= TMD_OBJECT_COLOR_BLEND_ONE / work->colorBlendFadeFrames;
                if (obj->shading.colorBlend <= 0) {
                    obj->shading.colorBlend = 0;
                    work->fadeState         = GOLEM_KNIGHT_BISHOP_FADE_HIDDEN;
                    if (work->vanishSound != 0) {
                        sndEvtRequestScriptStop(work->vanishSound, SOUND_SCRIPT_STOP_KEEP_RELEASE);
                        work->vanishSound = 0;
                    }
                    snd = gGolemKnightBishopFadeCue | (((u16)((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8);
                    pan = (s8)worldCoordGetOriginAudioPan(coord);
                    sndEvtRequestScriptStart(snd, pan, (s8)worldCoordGetOriginAudioDepth(coord));
                }
            }
            t                 = work->shadowShade - 0x80 / work->translucencyFadeFrames;
            work->shadowShade = t;
            if (t < 0) {
                work->shadowShade = -1;
            }
            break;
        case GOLEM_KNIGHT_BISHOP_FADE_FEINT_APPEAR:
            obj->shading.colorBlend += (TMD_OBJECT_COLOR_BLEND_ONE * 11 / 16) / work->colorBlendFadeFrames;
            if (obj->shading.colorBlend >= TMD_OBJECT_COLOR_BLEND_ONE * 11 / 16) {
                obj->shading.colorBlend = TMD_OBJECT_COLOR_BLEND_ONE * 11 / 16;
                t                       = work->translucency - 0xFF / work->translucencyFadeFrames;
                work->translucency      = t;
                if (t <= 0) {
                    work->fadeState       = GOLEM_KNIGHT_BISHOP_FADE_FEINT_SHOWN;
                    work->translucency    = 0;
                    work->scale.vx        = 0x1000;
                    work->scale.vy        = 0x1000;
                    work->scale.vz        = 0x1000;
                    work->unscaledRootMtx = arg0->extra.tmd->coords[0].coord;
                    work->shrinkStep      = 0;
                }
            }
            work->shadowShade = -1;
            break;
        case GOLEM_KNIGHT_BISHOP_FADE_FEINT_SHOWN:
            work->shadowShade = -1;
            break;
        case GOLEM_KNIGHT_BISHOP_FADE_FEINT_SHRINK:
            work->hurtBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
            switch (work->shrinkStep) {
                case 0:
                    y = work->scale.vy;
                    if (work->feintBroken != 0) {
                        sy = y - 0x400;
                    } else {
                        sy = y - 0x200;
                    }
                    work->scale.vy = sy;
                    if (sy <= 0x800) {
                        work->shrinkStep = 1;
                    }
                    break;
                case 1:
                    if (work->feintBroken != 0) {
                        v              = work->scale.vx - 0x200;
                        w              = work->scale.vy + 0x400;
                        work->scale.vx = v;
                        work->scale.vy = w;
                    } else {
                        v              = work->scale.vx - 0x100;
                        w              = work->scale.vy + 0x200;
                        work->scale.vx = v;
                        work->scale.vy = w;
                    }
                    if (work->scale.vx <= 0x800) {
                        work->shrinkStep = 2;
                    }
                    break;
            }
            _golemKnightBishopApplyScale(arg0);
            t                  = work->translucency + 0xFF / work->translucencyFadeFrames;
            work->translucency = t;
            if (t >= 0xFF) {
                work->translucency       = 0xFF;
                obj->shading.colorBlend -= TMD_OBJECT_COLOR_BLEND_ONE / work->colorBlendFadeFrames;
                if (obj->shading.colorBlend <= 0) {
                    obj->shading.colorBlend = 0;
                    work->fadeState         = GOLEM_KNIGHT_BISHOP_FADE_HIDDEN;
                    gfxSetRotIdentity(&arg0->extra.tmd->coords[0].coord);
                    arg0->extra.tmd->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
                }
            }
            work->shadowShade = -1;
            break;
        case GOLEM_KNIGHT_BISHOP_FADE_FLICKER_START:
            work->fadeState              = GOLEM_KNIGHT_BISHOP_FADE_FLICKER_DIM;
            work->flickerTimer           = (((gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT) >> 16) & 0xF) + 2;
            t                            = work->flickerTimer + (((gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT) >> 16) & 0xF);
            work->translucencyFadeFrames = t;
            work->colorBlendFadeFrames   = t;
            break;
        case GOLEM_KNIGHT_BISHOP_FADE_FLICKER_DIM:
            t                  = work->translucency + 0xFF / work->translucencyFadeFrames;
            work->translucency = t;
            if (t >= 0x80) {
                work->translucency       = 0x80;
                obj->shading.colorBlend -= TMD_OBJECT_COLOR_BLEND_ONE / work->colorBlendFadeFrames;
                if (obj->shading.colorBlend <= TMD_OBJECT_COLOR_BLEND_ONE / 2) {
                    obj->shading.colorBlend = TMD_OBJECT_COLOR_BLEND_ONE / 2;
                }
            }
            t                 = work->shadowShade - 0x80 / work->translucencyFadeFrames;
            work->shadowShade = t;
            if (t < 0) {
                work->shadowShade = -1;
            }
            t                  = work->flickerTimer - 1;
            work->flickerTimer = t;
            if (t <= 0) {
                work->fadeState              = GOLEM_KNIGHT_BISHOP_FADE_FLICKER_BRIGHT;
                work->flickerTimer           = (((gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT) >> 16) & 0xF) + 2;
                t                            = work->flickerTimer + (((gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT) >> 16) & 0xF);
                work->translucencyFadeFrames = t;
                work->colorBlendFadeFrames   = t;
            }
            if (work->tintRequest == 0) {
                work->tintRequest = 1;
            }
            if (work->animFrame & 1) {
                sc->vx = 0;
                sc->vy = -(((gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT) >> 16) & 0xFF);
                sc->vz = ((gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT) >> 16) & 0xFF;
                effectSpawn(EFFECT_FLASH_BURST, &arg0->extra.tmd->coords[3], 0x100, sc);
            }
            break;
        case GOLEM_KNIGHT_BISHOP_FADE_FLICKER_BRIGHT:
            obj->shading.colorBlend += TMD_OBJECT_COLOR_BLEND_ONE / work->colorBlendFadeFrames;
            if (obj->shading.colorBlend >= TMD_OBJECT_COLOR_BLEND_ONE) {
                obj->shading.colorBlend = TMD_OBJECT_COLOR_BLEND_ONE;
                t                       = work->translucency - 0xFF / work->translucencyFadeFrames;
                work->translucency      = t;
                if (t <= 0) {
                    work->translucency = 0;
                }
            }
            t                 = work->shadowShade + 0x80 / work->translucencyFadeFrames;
            work->shadowShade = t;
            if (t >= 0x80) {
                work->shadowShade = 0x80;
            }
            t                  = work->flickerTimer - 1;
            work->flickerTimer = t;
            if (t <= 0) {
                work->fadeState              = GOLEM_KNIGHT_BISHOP_FADE_FLICKER_DIM;
                work->flickerTimer           = (((gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT) >> 16) & 0xF) + 2;
                t                            = work->flickerTimer + (((gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT) >> 16) & 0xF);
                work->translucencyFadeFrames = t;
                work->colorBlendFadeFrames   = t;
            }
            if (work->animFrame & 1) {
                sc->vx = 0;
                sc->vy = -(((gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT) >> 16) & 0xFF);
                sc->vz = ((gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT) >> 16) & 0xFF;
                effectSpawn(EFFECT_FLASH_BURST, &arg0->extra.tmd->coords[3], 0x100, sc);
            }
            break;
    }
    SCRATCH_STACK_RELEASE_BYTES(8);
}

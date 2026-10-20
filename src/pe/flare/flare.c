#include "pe/flare.h"

#include "gameplay/display.h"
#include "gameplay/attachment_state.h"
#include "gameplay/attachments.h"
#include "gameplay/effects.h"
#include "gameplay/room_effects.h"
#include "gameplay/world_coords.h"

#include "main/coord.h"
#include "main/sound.h"

void flareEffectTask(Task* task)
{
    enum {
        FLARE_STATE_INITIALIZE       = 0,
        FLARE_STATE_GROW             = 1,
        FLARE_STATE_FADE             = 2,
        FLARE_GROWTH_FRAMES          = 30,
        FLARE_BRIGHTNESS_MAX         = 255,
        FLARE_INITIAL_DISC_RADIUS    = 0x100,
        FLARE_INNER_BAND_BASE_RADIUS = 0x800,
        FLARE_INNER_BAND_WIDTH       = 0x100,
        FLARE_OUTER_BAND_BASE_RADIUS = 0xC00,
        FLARE_OUTER_BAND_WIDTH       = 0xC0,
        FLARE_FADE_BRIGHTNESS_STEP   = 8,
        FLARE_FADE_RADIUS_STEP       = 0x30
    };
    /// Draws the two concentric Flare discs and writes their pink RGB modulation.
    ///
    /// Borrows a composed GfxCoord and read-only EffectWork. Scale supplies
    /// red, with green one quarter and blue one half; writes all three u8
    /// output channels for the caller's bands or screen tint. Angle is the
    /// disc radius in coordinate units; doubling narrows to s16. Arguments
    /// must be stable, side-effect-free pointers and are read repeatedly.
    /// Uses the current scratch stack, frame arena and ordering table, with
    /// no captured caller identifiers.
    /// Invoke inside a braced caller block.
#define FLARE_DRAW_GLOW_DISCS(centreCoord, effectWork, rgbOutput)                           \
    {                                                                                       \
        (rgbOutput)[0] = (effectWork)->scale;                                               \
        (rgbOutput)[1] = (effectWork)->scale >> 2;                                          \
        (rgbOutput)[2] = (effectWork)->scale >> 1;                                          \
        effectDrawGouraudDisc((centreCoord), (effectWork)->angle, (rgbOutput));             \
        effectDrawGouraudDisc((centreCoord), (s16)((effectWork)->angle << 1), (rgbOutput)); \
    }
    EffectWork*      work;
    const GfxCoord*  effectCoord;
    AttachmentState* attachment;
    s32              soundPan;
    s8               soundDepth;
    u8               glowRgb[3];

    attachment  = &gAttachmentState;
    work        = task->spawnArg2.pointer;
    effectCoord = task->extra.coordBody->coord;
    // A held attachment or any non-running PE control ends both sound and drawing.
    if ((attachment->effectPhase == ATTACHMENT_EFFECT_HELD) || (gRoomEffectState->peEffectControl != ROOM_EFFECT_CONTROL_RUNNING)) {
        sndEvtRequestScriptStop(SOUND_FLARE_USE, SOUND_SCRIPT_STOP_KEEP_RELEASE);
        effectKillTask(work, task);
        return;
    }

    work->age++;
    switch (task->state) {
        case FLARE_STATE_INITIALIZE:
            task->spawnArg1.value = FLARE_GROWTH_FRAMES;
            work->scale           = 0;
            work->angle           = FLARE_INITIAL_DISC_RADIUS;
            work->step            = (FLARE_BRIGHTNESS_MAX + 1) / task->spawnArg1.value;
            task->state           = FLARE_STATE_GROW;
            // Normalize both spatial sound arguments to signed bytes before queuing.
            soundPan   = (s8)worldCoordGetOriginAudioPan(effectCoord);
            soundDepth = worldCoordGetOriginAudioDepth(effectCoord);
            sndEvtRequestScriptStart(SOUND_FLARE_USE, soundPan, soundDepth);
            return;
        case FLARE_STATE_GROW:
            // Brighten the discs while the two surrounding glow bands contract.
            work->scale += work->step;
            work->angle += work->step << 3;
            task->spawnArg1.value--;
            FLARE_DRAW_GLOW_DISCS(effectCoord, work, glowRgb);
            effectDrawOuterGlowBand(effectCoord, (s16)((task->spawnArg1.value << 4) + FLARE_INNER_BAND_BASE_RADIUS), FLARE_INNER_BAND_WIDTH, glowRgb);
            glowRgb[0] >>= 1;
            glowRgb[1] >>= 1;
            glowRgb[2] >>= 1;
            effectDrawOuterGlowBand(effectCoord, (s16)((task->spawnArg1.value << 5) + FLARE_OUTER_BAND_BASE_RADIUS), FLARE_OUTER_BAND_WIDTH, glowRgb);
            if (task->spawnArg1.value == 0) {
                // Request the gameplay result after the final growth draw.
                work->scale        = FLARE_BRIGHTNESS_MAX;
                task->state        = FLARE_STATE_FADE;
                work->period       = 0x600; // Retained phase value; this task never reads it.
                work->step         = 0;
                attachment->flags |= ATTACHMENT_FLAG_APPLY_STATS;
            }
            return;
        case FLARE_STATE_FADE:
            if (work->scale >= FLARE_FADE_BRIGHTNESS_STEP + 1) {
                // Both screen passes use the brightness from before this update's decay.
                FLARE_DRAW_GLOW_DISCS(effectCoord, work, glowRgb);
                work->scale -= FLARE_FADE_BRIGHTNESS_STEP;
                work->angle -= FLARE_FADE_RADIUS_STEP;
                effectDrawScreenTint(glowRgb, GPU_BLEND_ADD);
                effectDrawScreenTint(glowRgb, GPU_BLEND_ADD);
                return;
            }
            effectKillTask(work, task);
            return;
    }
#undef FLARE_DRAW_GLOW_DISCS
}

#ifndef ANTIBODY_ANTIBODY_H
#define ANTIBODY_ANTIBODY_H

#include <engine/task_types.h>

/// An Antibody mote (`EFFECT_ANTIBODY_MOTE`), spawned by the cast at an offset from its parent: it closes on the parent,
/// then rises and goes; now and then one stops instead, drawn larger with a strip to the player.
void antibodyMoteTask(Task* task);

/// The Antibody cast: rings, an arc and a fan of glow wedges grow over the caster while bursts of motes close in, then an
/// aura is spawned and they fade. Applies the spell and plays its cue on the first frame; a held attachment, a room's PE
/// cancellation or the fade's end frees it and its motes.
void antibodyCastTask(Task* task);

#endif // ANTIBODY_ANTIBODY_H

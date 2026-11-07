#include "unused_86.h"

#include <core/pe2.h>

#include <gameplay/player_animation.h>

PE2_EDITOR_LABEL("Animation")
/// The player's clips while it is equipped, by slot.
PE2_BIND(bank, animation_bank)
PE2_BIND_ARG(bank, _symbol, gUnused86PlayerAnimationBank)

PE2_VALUE(bank)

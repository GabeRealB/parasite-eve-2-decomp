"""The event-script vocabulary: opcodes, task slots and message ids by name.

Made once from include/main/session.h (GAME_TASK_SLOT_*), include/gameplay/message.h and captions.h (*_MESSAGE_*)
and include/gameplay/evs.h (EVENT_SCRIPT_OPCODE_*); the extractor never reads C. A message id the table does not name
is written as a number.
"""

OPCODES = {-1: "end", 1: "send_message", 2: "start_flash", 3: "set_view", 4: "wait_frames", 5: "cancel_primary_fade", 6: "restore_hud", 7: "set_event_state", 8: "wait_animation", 9: "wait_cap_cue", 10: "play_weapon_animation", 11: "restore_view", 12: "select_scene", 13: "callback", 14: "start_vibration", 15: "start_sound", 16: "stop_sound", 17: "set_framebuffer_blend", 18: "start_area_music", 19: "stop_area_music", 20: "request_scene_music", 21: "wait_music_load", 22: "shake_screen", 23: "cleanup_scene", 24: "start_primary_fade", 25: "return_primary_fade", 26: "fade_volume", 27: "fade_sound_attenuation", 28: "rebuild_tmd_buffers", 29: "wait_actor_action", 30: "play_scene_audio", 31: "start_scene_audio", 32: "set_ambient_rgb", 33: "clear_ambient_rgb", 34: "finish_scene_stream", 35: "start_secondary_fade", 36: "return_secondary_fade", 37: "cancel_secondary_fade", 38: "restore_weapons", 39: "hide_weapons", 40: "set_light_scale", 41: "set_cap_direct_view_ids", 42: "start_stage_sound", 43: "jump", 44: "call_script", 45: "return", 46: "set_skip_target", 47: "set_skip_keep_sound", 48: "set_dirty_view", 49: "save_view"}
SLOTS = {1: "view_gate", 3: "player", 4: "scene", 5: "room_effect", 6: "cap_control", 7: "room", 10: "companion"}
MESSAGES = {
    0x3E8: "animation_play",  # ANIMATION_MESSAGE_PLAY
    0x3E9: "actor_place",  # GAME_ACTOR_MESSAGE_PLACE
    0x3ED: "animation_is_playing",  # ANIMATION_MESSAGE_IS_PLAYING
    0x3EE: "actor_turn_to_yaw",  # GAME_ACTOR_MESSAGE_TURN_TO_YAW
    0x3EF: "actor_climb_stairs",  # GAME_ACTOR_MESSAGE_CLIMB_STAIRS
    0x3F0: "actor_is_scripted_motion_pending",  # GAME_ACTOR_MESSAGE_IS_SCRIPTED_MOTION_PENDING
    0x3F1: "actor_end_scripted",  # GAME_ACTOR_MESSAGE_END_SCRIPTED
    0x3F2: "actor_move_to",  # GAME_ACTOR_MESSAGE_MOVE_TO
    0x3F3: "actor_set_model_draw",  # GAME_ACTOR_MESSAGE_SET_MODEL_DRAW
    0x3F4: "animation_install_and_play",  # ANIMATION_MESSAGE_INSTALL_AND_PLAY
    0x3F5: "actor_attach_to_coord",  # GAME_ACTOR_MESSAGE_ATTACH_TO_COORD
    0x3F6: "actor_walk_steps",  # GAME_ACTOR_MESSAGE_WALK_STEPS
    0x3F7: "animation_copy_bank_extension",  # ANIMATION_MESSAGE_COPY_BANK_EXTENSION
    0x3F8: "actor_await_button_presses",  # GAME_ACTOR_MESSAGE_AWAIT_BUTTON_PRESSES
    0x3F9: "actor_apply_damage",  # GAME_ACTOR_MESSAGE_APPLY_DAMAGE
    0x3FA: "player_actor_message_enter_scripted_presentation",  # PLAYER_ACTOR_MESSAGE_ENTER_SCRIPTED_PRESENTATION
    0x3FB: "actor_run_to",  # GAME_ACTOR_MESSAGE_RUN_TO
    0x3FC: "actor_set_run_movement",  # GAME_ACTOR_MESSAGE_SET_RUN_MOVEMENT
    0x3FD: "animation_set_rate",  # ANIMATION_MESSAGE_SET_RATE
    0x3FE: "actor_move_by",  # GAME_ACTOR_MESSAGE_MOVE_BY
    0x3FF: "animation_replace_and_play",  # ANIMATION_MESSAGE_REPLACE_AND_PLAY
    0x401: "actor_set_texture_sequence",  # GAME_ACTOR_MESSAGE_SET_TEXTURE_SEQUENCE
    0x402: "player_actor_message_enter_item_use",  # PLAYER_ACTOR_MESSAGE_ENTER_ITEM_USE
    0x7D0: "scene_find_placed_actor",  # SCENE_MESSAGE_FIND_PLACED_ACTOR
    0x7D3: "scene_actor_play_animation",  # ACTOR_MESSAGE_PLAY_ANIMATION
    0x7D4: "scene_actor_place",  # ACTOR_MESSAGE_PLACE
    0x7D5: "scene_actor_set_model_draw",  # ACTOR_MESSAGE_SET_MODEL_DRAW
    0x7D6: "scene_actor_is_present",  # ACTOR_MESSAGE_IS_PRESENT
    0x7D8: "scene_find_other_child",  # SCENE_MESSAGE_FIND_OTHER_CHILD
    0x7D9: "scene_exit_placed_actors",  # SCENE_MESSAGE_EXIT_PLACED_ACTORS
    0x7DA: "scene_broadcast_to_actors",  # SCENE_MESSAGE_BROADCAST_TO_ACTORS
    0x7DB: "command_apply",  # ACTOR_COMMAND_MESSAGE_APPLY
    0x7DD: "scene_actor_walk_to",  # ACTOR_MESSAGE_WALK_TO
    0x7DE: "scene_actor_release_hold",  # ACTOR_MESSAGE_RELEASE_HOLD
    0xFA0: "cap_start",  # CAP_CONTROL_MESSAGE_START
    0xFA1: "cap_resume_timed_record",  # CAP_CONTROL_MESSAGE_RESUME_TIMED_RECORD
    0xFA2: "cap_abort",  # CAP_CONTROL_MESSAGE_ABORT
    0xFA3: "cap_is_busy",  # CAP_CONTROL_MESSAGE_IS_BUSY
    0xFA4: "cap_hide_hud",  # CAP_CONTROL_MESSAGE_HIDE_HUD
    0xFA5: "cap_show_hud",  # CAP_CONTROL_MESSAGE_SHOW_HUD
    0xFA6: "cap_select_scene",  # CAP_CONTROL_MESSAGE_SELECT_SCENE
    0xFA7: "cap_begin_scene_sync",  # CAP_CONTROL_MESSAGE_BEGIN_SCENE_SYNC
    0xFA8: "cap_show_hud_abort",  # CAP_CONTROL_MESSAGE_SHOW_HUD_ABORT
    0x13EE: "room_event_message_resolve",  # ROOM_EVENT_MESSAGE_RESOLVE
    0x13F0: "room_message_command",  # ROOM_MESSAGE_COMMAND
    0x13F1: "room_message_use_key_item",  # ROOM_MESSAGE_USE_KEY_ITEM
    0x13F2: "room_message_sound",  # ROOM_MESSAGE_SOUND
    0x13F4: "room_message_actor_event",  # ROOM_MESSAGE_ACTOR_EVENT
    0x7FFFFFFF: "task_message_table_end",  # TASK_MESSAGE_TABLE_END
}

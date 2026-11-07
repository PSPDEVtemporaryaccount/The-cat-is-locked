.section .rodata
.align 4
.global tex_main_default
tex_main_default:
.incbin "build/main_default.rgba"
.global tex_main_play
tex_main_play:
.incbin "build/main_play.rgba"
.global tex_main_settings
tex_main_settings:
.incbin "build/main_settings.rgba"
.global tex_main_quit
tex_main_quit:
.incbin "build/main_quit.rgba"
.global tex_play_menu
tex_play_menu:
.incbin "build/play_menu.rgba"
.global tex_play_resume
tex_play_resume:
.incbin "build/play_resume.rgba"
.global tex_start_adventure
tex_start_adventure:
.incbin "build/start_adventure.rgba"
.global tex_settings
tex_settings:
.incbin "build/settings.rgba"

.align 4
.global menu_audio_pcm
menu_audio_pcm:
.incbin "build/menu_audio.pcm"
.global menu_audio_pcm_end
menu_audio_pcm_end:

.section .rodata
.align 4
.global menu_main_texture
menu_main_texture:
.incbin "build/menu_main.rgba"
.global menu_play_resume_texture
menu_play_resume_texture:
.incbin "build/menu_play_resume.rgba"
.global menu_play_new_texture
menu_play_new_texture:
.incbin "build/menu_play_new.rgba"
.global menu_settings_texture
menu_settings_texture:
.incbin "build/menu_settings.rgba"
.global menu_resume_texture
menu_resume_texture:
.incbin "build/menu_resume.rgba"

.global menu_arrow_left_texture
menu_arrow_left_texture:
.incbin "build/arrow_left.rgba"
.global menu_arrow_right_texture
menu_arrow_right_texture:
.incbin "build/arrow_right.rgba"

.section .rodata
.align 4
.global menu_audio_pcm
menu_audio_pcm:
.incbin "build/menu_audio.pcm"
.global menu_audio_pcm_end
menu_audio_pcm_end:

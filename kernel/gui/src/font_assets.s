section .rodata

global kenux_font_latin_ttf
global kenux_font_latin_ttf_end
global kenux_font_cjk_ttf
global kenux_font_cjk_ttf_end

align 16
kenux_font_latin_ttf:
    incbin "BRLNSR.TTF"
kenux_font_latin_ttf_end:

align 16
kenux_font_cjk_ttf:
    incbin "simhei.ttf"
kenux_font_cjk_ttf_end:

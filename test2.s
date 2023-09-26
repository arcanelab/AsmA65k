.pc = 0

back:
;    mov   r7, magic
;    bne   back
    sys 7, magic
    sys print, magic
    sys 7, $12345678
    sys print, $12345678

.def magic = $12345678
.def print = 7

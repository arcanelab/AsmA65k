.pc = 0

back:
    mov   r7, magic
    bne   back

.def magic = $12345678

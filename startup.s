.pc = $200

start:
    mov [r0], -1
    sxb [$a000], -2
    ;mov [PC+start]+, 5
    ;or.w [$69857463], 4

;    sxb r0, -1
;    sxw r2, r0
;    sxw r0, [$babefeab]

;    sys $8412, $12345678
loop:
;    jmp loop

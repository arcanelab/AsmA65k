.def    MINIVIC = $f100   ; define base address for the GPU
.def    GFXMODE = MINIVIC + 0
.def    GPUREG1 = MINIVIC + 1
.def    GPUCONF = MINIVIC + 2 ; hey, here's some comment
.def    GPUDISB = MINIVIC + 3
; ---------------------------------------------------------
.pc = $100 ;fds

start:
        MOV     r0, [MINIVIC + r1]
        mov     r0, [r1]
        inc     [r0]
        dec     r9
        dec     pc
        inc     SP
        mov.w   r0, 15          ; this is the first comment
        mov     r1, $ff
        mov     r2, $12345678
        mov.b   r3, 255
        mov.w   r4, $ffff
loop:   mov     [$1000+r7], r6
        bra     loop
        mov     r1, adatok
        add     r2, [nev + r9]  ; here's a comment, too
        add     pc, [nev + sp]  ; here's a comment, too
        add     [adatok], r4
        add     [adatok + r3], r4
        rts

nev:    .text "John Doe"
;adatok: .dword $f, 2, 3 ,4, 5,6,$7,$8,$6444,%01101011
;nev2:   .text "Majoros "The Guy" Bálint" ; a fiam neve

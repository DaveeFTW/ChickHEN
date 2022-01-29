.set noreorder
.set noat
.global long_loop
.ent    long_loop
long_loop:
    li $a0, 0
    li $a1, 100000
loop:
    bnel $a0, $a1, loop
    addiu $a0, $a0, 1
    jr $ra
    nop
.end long_loop

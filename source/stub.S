# Relocatable DOL boot stub. boot.c copies it to 0x80002000 (free low memory) and calls it with
# r3 = op list { count, entry, { dst, src, len } * count }. src == 0 means "zero fill".
# Each range is copied (or cleared), written back from the data cache and dropped from the
# instruction cache; then the stub jumps to the entry point.
    .text
    .globl boot_stub
    .globl boot_stub_end
boot_stub:
    mfmsr   10                  # no external / decrementer interrupts from here on
    rlwinm  10,10,0,17,15
    mtmsr   10
    isync
    lwz     4,0(3)              # count
    lwz     5,4(3)              # entry
    addi    6,3,8
1:  cmpwi   4,0
    beq     9f
    lwz     7,0(6)              # dst
    lwz     8,4(6)              # src
    lwz     9,8(6)              # len
    mr      11,7
    mr      12,9
    cmpwi   9,0
    beq     4f
    cmpwi   8,0
    beq     3f
2:  lbz     10,0(8)
    stb     10,0(7)
    addi    8,8,1
    addi    7,7,1
    addic.  9,9,-1
    bne     2b
    b       4f
3:  li      10,0
5:  stb     10,0(7)
    addi    7,7,1
    addic.  9,9,-1
    bne     5b
4:  cmpwi   12,0                # flush [r11, r11 + r12)
    beq     7f
    rlwinm  11,11,0,0,26        # align down to 32
    addi    12,12,63
6:  dcbst   0,11
    icbi    0,11
    addi    11,11,32
    addic.  12,12,-32
    bgt     6b
7:  addi    6,6,12
    addi    4,4,-1
    b       1b
9:  sync
    isync
    mtlr    5
    blr
boot_stub_end:

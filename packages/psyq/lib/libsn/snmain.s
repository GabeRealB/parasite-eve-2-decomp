/*
 * SN Systems' start-up module, libsn's SNMAIN.obj, reconstructed: the SDK's object holds psylink expressions an
 * ELF relocation cannot (a section's start and end, their difference, a count), so it cannot be converted as the
 * other objects of lib/ were. Its instructions, data and symbols are the object's own; each expression is a symbol
 * whoever links it defines (the image's link script):
 *
 *   __SN_SBSS_START   sectstart(.sbss)        where the start-up clears from
 *   __SN_BSS_END      sectend(.bss)           where it clears to, and the heap begins
 *   __SN_SDATA_START  sectstart(.sdata)       $gp
 *   __SN_CTORS_START  sectstart(.ctors)       the constructors __main calls ...
 *   __SN_CTORS_COUNT  sizeof(.ctors) / 4      ... and how many
 *   __SN_DTORS_START  sectstart(.dtors)       the destructors __do_global_dtors calls ...
 *   __SN_DTORS_COUNT  sizeof(.dtors) / 4      ... and how many
 *   __SN_TEXT_START, __SN_TEXT_SIZE           the image's code, as __text and __textlen hold it
 *   __SN_DATA_START, __SN_DATA_SIZE           its data, as __data and __datalen
 *   __SN_BSS_START, __SN_BSS_SIZE             its BSS, as __bss and __bsslen
 *
 * The object's .rdata, .ctors, .dtors, .sdata and .bss are empty; its .sbss is one word.
 */

.set noat
.set noreorder

.section .text, "ax"

.globl __SN_ENTRY_POINT
__SN_ENTRY_POINT:
    /* Clear .sbss through .bss. */
    lui     $v0, %hi(__SN_SBSS_START)
    addiu   $v0, $v0, %lo(__SN_SBSS_START)
    lui     $v1, %hi(__SN_BSS_END)
    addiu   $v1, $v1, %lo(__SN_BSS_END)
.Lclear:
    sw      $zero, 0x0($v0)
    addiu   $v0, $v0, 0x4
    sltu    $at, $v0, $v1
    bnez    $at, .Lclear
    nop
    /* The stack at the top of RAM, in KSEG0. */
    lui     $v0, %hi(_ramsize)
    lw      $v0, %lo(_ramsize)($v0)
    nop
    addi    $v0, $v0, -0x8
    lui     $t0, 0x8000
    or      $sp, $v0, $t0
    /* The heap from the end of BSS to the stack's bottom. */
    lui     $a0, %hi(__SN_BSS_END)
    addiu   $a0, $a0, %lo(__SN_BSS_END)
    sll     $a0, $a0, 3
    srl     $a0, $a0, 3
    lui     $v1, %hi(_stacksize)
    lw      $v1, %lo(_stacksize)($v1)
    nop
    subu    $a1, $v0, $v1
    subu    $a1, $a1, $a0
    lui     $at, %hi(__heapsize)
    sw      $a1, %lo(__heapsize)($at)
    or      $a0, $a0, $t0
    lui     $at, %hi(__heapbase)
    sw      $a0, %lo(__heapbase)($at)
    lui     $at, %hi(.Lsaved_ra)
    sw      $ra, %lo(.Lsaved_ra)($at)
    lui     $gp, %hi(__SN_SDATA_START)
    addiu   $gp, $gp, %lo(__SN_SDATA_START)
    addu    $fp, $sp, $zero
    jal     InitHeap
    addi    $a0, $a0, 0x4
    lui     $ra, %hi(.Lsaved_ra)
    lw      $ra, %lo(.Lsaved_ra)($ra)
    nop
    jal     main
    nop
    break   0, 1

/* Calls the constructors, once. */
.globl __main
__main:
    lui     $t0, %hi(.Lmain_called)
    lw      $t0, %lo(.Lmain_called)($t0)
    addiu   $sp, $sp, -0x10
    sw      $s0, 0x4($sp)
    sw      $s1, 0x8($sp)
    sw      $ra, 0xC($sp)
    bnez    $t0, .Lmain_done
    ori     $t0, $zero, 0x1
    lui     $at, %hi(.Lmain_called)
    sw      $t0, %lo(.Lmain_called)($at)
    lui     $s0, %hi(__SN_CTORS_START)
    addiu   $s0, $s0, %lo(__SN_CTORS_START)
    lui     $s1, %hi(__SN_CTORS_COUNT)
    addiu   $s1, $s1, %lo(__SN_CTORS_COUNT)
    beqz    $s1, .Lmain_done
    nop
.Lmain_next:
    lw      $t0, 0x0($s0)
    addiu   $s0, $s0, 0x4
    jalr    $t0
    addiu   $s1, $s1, -0x1
    bnez    $s1, .Lmain_next
    nop
.Lmain_done:
    lw      $ra, 0xC($sp)
    lw      $s1, 0x8($sp)
    lw      $s0, 0x4($sp)
    addiu   $sp, $sp, 0x10
    jr      $ra
    nop

/* Calls the destructors, if the constructors were. */
.globl __do_global_dtors
__do_global_dtors:
    lui     $t0, %hi(.Lmain_called)
    lw      $t0, %lo(.Lmain_called)($t0)
    addiu   $sp, $sp, -0x10
    sw      $s0, 0x4($sp)
    sw      $s1, 0x8($sp)
    sw      $ra, 0xC($sp)
    beqz    $t0, .Ldtors_done
    nop
    lui     $s0, %hi(__SN_DTORS_START)
    addiu   $s0, $s0, %lo(__SN_DTORS_START)
    lui     $s1, %hi(__SN_DTORS_COUNT)
    addiu   $s1, $s1, %lo(__SN_DTORS_COUNT)
    beqz    $s1, .Ldtors_done
    nop
.Ldtors_next:
    lw      $t0, 0x0($s0)
    addiu   $s0, $s0, 0x4
    jalr    $t0
    addiu   $s1, $s1, -0x1
    bnez    $s1, .Ldtors_next
    nop
.Ldtors_done:
    lw      $ra, 0xC($sp)
    lw      $s1, 0x8($sp)
    lw      $s0, 0x4($sp)
    addiu   $sp, $sp, 0x10
    jr      $ra
    nop

.section .data, "wa"

.Lmain_called:
    .word 0
.globl __heapbase
__heapbase:
    .word 0
.globl __heapsize
__heapsize:
    .word 0
.globl __text
__text:
    .word __SN_TEXT_START
.globl __textlen
__textlen:
    .word __SN_TEXT_SIZE
.globl __data
__data:
    .word __SN_DATA_START
.globl __datalen
__datalen:
    .word __SN_DATA_SIZE
.globl __bss
__bss:
    .word __SN_BSS_START
.globl __bsslen
__bsslen:
    .word __SN_BSS_SIZE

.section .sbss, "wa", @nobits

/* Where the start-up keeps its return address across InitHeap. */
.Lsaved_ra:
    .space 0x4

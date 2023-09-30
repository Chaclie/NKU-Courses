# load "mytest.s"
# dumpnative "mytest.x"
	.text
main:
    # I type: arithmetic & logic & compare test (10 ins for this part, 0 ins before)
    # $1 is reserved for assembler
    addi    $2, $0, 1           # $2 = 1
    addiu   $2, $2, 2           # $2 = 3
    andi    $3, $2, 3           # $3 = 3
    ori     $4, $3, 4           # $4 = 7
    lui     $5,     0xffff      # $5 = 0xffff0000 = -65536
    ori		$5, $5, 0xf0f5		# $5 = 0xfffff0f5
    xori	$5, $5, 0xf07		# $5 = 0xfffffff2 = -14
    slti    $6, $5, 7           # $6 = 1
    sltiu   $7, $6, 0xfff2  	# $7 = 1
    lui     $8,     0xffff      # $8 = 0xffff0000 = -65536
    
    # R type: arithmetic & logic & compare test (11 ins for this part, 10 ins before)
    add     $9, $2, $6          # $9 = 4
    addu    $10,$4, $9          # $10 = 11
    sub     $11,$10,$7          # $11 = 10
    subu    $12,$11,$9          # $12 = 6
    and     $13,$12,$10         # $13 = 2
    or      $14,$12,$10         # $14 = 15
    xor		$14,$14,$6			# $14 = 14
    xor     $15,$14,$4          # $15 = 9
    nor     $16,$8, $5          # $16 = 13
    slt     $17,$8, $16         # $17 = 1
    sltu    $18,$16,$8          # $18 = 1

    # R type: mul & div & hilo test (13 ins for this part, 21 ins before)
    mult    $12,$5				# $HI = 0xffffffff, $LO = 0xffffffac
    mfhi    $19                 # $19 = 0xffffffff = -1
    mflo    $20                 # $20 = 0xffffffac = -84
    multu   $12,$5				# $HI = 5, $LO = 0xffffffac
    mfhi    $21                 # $21 = 5
    div     $5, $3              # $HI = 0xfffffffe = -2, $LO = 0xfffffffc = -4
    mfhi    $22                 # $22 = 0xfffffffe = -2
    mflo    $23                 # $23 = 0xfffffffc = -4
    divu    $5, $3              # $HI = 2, $LO = 0x55555550 = 1431655760
    mfhi    $24                 # $24 = 2
    mflo    $25                 # $25 = 0x55555550 = 1431655760
    mthi    $15                 # $HI = 9
    mtlo    $12                 # $LO = 6

    # R type: shift test (6 ins for this part, 34 ins before)
    sll     $26,$12,3           # $26 = 0x30 = 48
    srl     $27,$23,11          # $27 = 0x001fffff
    sra     $28,$23,11          # $28 = 0xffffffff = -1
    sllv    $29,$21,$24         # $29 = 0x14 = 20
    srlv    $30,$8, $15         # $30 = 0x007fff80
    srav    $31,$8, $15         # $31 = 0xffffff80

    # dump registers to check correctness

    # I type: load & store test (17 ins for this part, 40 ins before)
	lui		$2,		0x1000		# $2 = 0x10000000 (start of data memory)
	lui		$3,		0x0010		# $3 = 0x00100000 (size of data memory)
	lui		$4,		0x0040		# $4 = 0x00400000
	addi	$4,	$4,	0x0028		# $4 = 0x00400028 (start of text memory)
	lui		$5,		0x0010		# $5 = 0x00100000 (size of text memory)
	lui		$6,		0x7ff0		# $6 = 0x7ff00000 (start of stack memory)
	lui		$7,		0x0010		# $7 = 0x00100000 (size of stack memory)
	addi	$8,	$2,	0x1000		# $8 = 0x10001000 (address aligned by word in data memory)
	lw		$9,	0x14($4)		# $9 = 0x34a5f0f5 (refer to the 6th instruction: ori $5, $5, 0xf0f5)
	sw		$9,	-4($8)			# [word at 0x10000ffc] = 0x34a5f0f5
	lhu		$10,-2($8)			# $10 = 0x34a5
	sb		$10,1($8)			# [word at 0x10001000] = 0x0000a500
	lh		$11,-4($8)			# $11 = 0xfffff0f5
	sh		$11,2($8)			# [word at 0x10001000] = 0xf0f5a500
	lb		$12,2($8)			# $12 = 0xfffffff5 = -11
	lbu		$13,3($8)			# $13 = 0xf0 = 240
	lw		$14,0($8)			# $14 = 0xf0f5a500

    # R & J type: jump test (9 ins for this part, 57 ins before)
    addi    $15,$4, 0xe4        # $15 = 0x0040010c (address of this instruction)
    addi    $16,$15,0x1c        # $16 = 0x00400128 (address of instruction: jr $18)
	jal		JAL_LABEL			# $31 = 0x00400118 (address of next instruction, jump to instruction: add $17,$31,$0)
    jalr    $18,$16             # $18 = 0x0040011c (jump to instruction: jr $18)
	j		J_LABEL				# jump to instruction: addi $19,$19,0x0c
JAL_LABEL:
	add		$17,$31,$0			# $17 = 0x00400118
    jr		$17					# jump to instruction: jalr $18,$16
	jr		$18					# jump to instruction: j J_LABEL
J_LABEL:
	addi	$19,$19,0x0c		# $19 = 0x0b = 11

    # I type: branch test (12 ins for this part, 66 ins before)
    beq		$3,	$5,	1			# branch to instruction: bne $2, $4, -2
	blez	$11,	3			# branch to instruction: bgtz $6, -3
	bne		$2,	$4,	-2			# branch to instruction: blez $11, 3
	bltz	$0,		-2			# branch to instruction: bgez $0, 1
	bgez	$0,		1			# branch to instruction: bltzal $28, 2
	bgtz	$6,		-3			# branch to instruction: bltz $0, -2
	bltzal	$28,	2			# $31 = 0x0040014c (branch to instruction: addi $20,$31,0)
	addi	$21,$31,0			# $21 = 0x0040015c (address of instruction: addi $22,$22,2)
	jr		$21					# jump to instruction: addi $22,$22,2
	addi	$20,$31,0			# $20 = 0x0040014c (address of instruction: addi $21,$31,0)
	bgezal	$29,	-4			# $31 = 0x0040015c (branch to instruction: addi $21,$31,0)
	addi	$22,$22,2			# $22 = 0

    # R type: syscall test (1 ins for this part, 78 ins before)
    syscall                     # $2 = 0xa = 10

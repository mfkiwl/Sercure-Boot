	.section .SE_Key_Data,"a",%progbits
 .syntax unified 
 .thumb 

      	.global SE_ReadKey
SE_ReadKey:
	PUSH {R4-R7}
	MOVW R4, #0x454f
	MOVT R4, #0x5f4d
	MOVW R5, #0x454b
	MOVT R5, #0x5f59
	MOVW R6, #0x4f43
	MOVT R6, #0x504d
	MOVW R7, #0x4e41
	MOVT R7, #0x3159
	STM R0, {R4-R7}
	POP {R4-R7}
	BX LR

	.global SE_ReadKey_Pub
SE_ReadKey_Pub:
	PUSH {R4-R11}
	MOVW R4, #0xf2ba
	MOVT R4, #0xf897
	MOVW R5, #0xe33e
	MOVT R5, #0xdc07
	MOVW R6, #0xc316
	MOVT R6, #0x7871
	MOVW R7, #0xf11d
	MOVT R7, #0x3eb0
	MOVW R8, #0x95f0
	MOVT R8, #0x44b0
	MOVW R9, #0x1254
	MOVT R9, #0x4881
	MOVW R10, #0x2cfb
	MOVT R10, #0xb966
	MOVW R11, #0x3d54
	MOVT R11, #0x4aa5
	STM R0, {R4-R11}
	ADD R0, R0,#32
	MOVW R4, #0x26e8
	MOVT R4, #0x7604
	MOVW R5, #0x37b7
	MOVT R5, #0x3c8b
	MOVW R6, #0xd846
	MOVT R6, #0x6afd
	MOVW R7, #0x6163
	MOVT R7, #0x467c
	MOVW R8, #0x7dc3
	MOVT R8, #0x46e9
	MOVW R9, #0x3144
	MOVT R9, #0xd76e
	MOVW R10, #0x6de1
	MOVT R10, #0x70ba
	MOVW R11, #0x44ed
	MOVT R11, #0x2ba
	STM R0, {R4-R11}
	POP {R4-R11}
	BX LR
	.end

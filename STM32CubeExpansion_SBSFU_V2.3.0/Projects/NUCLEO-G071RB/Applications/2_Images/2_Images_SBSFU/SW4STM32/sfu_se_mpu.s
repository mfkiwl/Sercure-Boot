/**
  ******************************************************************************
  * @file      sfu_se_mpu.s
  * @author    MCD Application Team
  * @brief     Wrapper for SE isolation with MPU.
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2017 STMicroelectronics. All rights reserved.
  *
  * This software component is licensed by ST under Ultimate Liberty license
  * SLA0044, the "License"; You may not use this file except in compliance with
  * the License. You may obtain a copy of the License at:
  *                             www.st.com/SLA0044
  *
  ******************************************************************************
  */
        .section  .text,"ax",%progbits
        .syntax unified
        .weak  MPU_SVC_Handler
        .global SVC_Handler
        .type  SVC_Handler, %function
SVC_Handler:
        MRS r0, PSP
        PUSH {LR}
        BL MPU_SVC_Handler
        POP {PC}

        .global jump_to_function
jump_to_function:
        /* assume R1 R2 are useless */
        LDR R1, [R0]
        LDR R2, [R0,#4]
        MOV SP, R1
        BX  R2

        .global exit_sticky
exit_sticky:
        /* R0: BL vector table address */
        /* R1: magic number */
        /* R2: application vector address */
        MOV R2, R0
        MOV R1, R11
        MOV R3, R10
        LDR R0, [R3]
        MOV SP, R0
        LDR R0, [R3,#4]
        BX R0

        .global launch_application
launch_application:
        /* return from exception to application launch function */
        /* R0: application vector address */
        /* R1: exit function address */
        /* R2: BL function address for exit_sticky */
        /* R3: magic number for exit_sticky */
        /* store R2 and R3 in not used registers */
        MOV R10, R2
        MOV R11, R3
        /* push interrupt context R0 R1 R2 R3 R12 LR PC xPSR */
        /* MOV R2, #0x01000000 ; xPSR activate Thumb bit */
        MOVS R2, #0x1
        LSLS R2, R2, #24
        PUSH {R2} /* FLAGS=0 */
        MOVS R2, #1
        BICS R1, R1, R2  /* clear least significant bit of exit function */
        PUSH {R1}  /* return address = application entry point */
        MOVS R1, #0 /* clear other context registers */
        PUSH {R1} /* LR =0 */
        PUSH {R1} /* R12 =0 */
        PUSH {R1} /* R3 = 0 */
        PUSH {R1} /* R2 = 0 */
        PUSH {R1} /* R1 = 0 */
        /* set R0 to application entry point */
        PUSH {R0} /* R0 = application entry point */
        /* set LR to return to thread mode with main stack */
        /* MOV LR, #0xFFFFFFF9 */
        MOVS R2, #6
        MVNS R2, R2
        MOV LR, R2
        /* return from interrupt */
        BX LR
.end

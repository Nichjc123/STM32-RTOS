.syntax unified
.cpu cortex-m4
.thumb

.global os_kernel_start
.thumb_func
os_kernel_start:
	MRS R0, PSP
    LDMIA R0!, {R4 - R11}
    MSR PSP, R0
    LDR LR, =0xFFFFFFFD
    BX LR


.global PendSV_Handler
.thumb_func
PendSV_Handler:
	MRS R0, PSP
	STMDB R0!, {R4-R11}
	MSR PSP, R0

	BL new_task

	MRS R0, PSP
	LDMIA R0!, {R4-R11}
	MSR PSP, R0
	LDR LR, =0xFFFFFFFD
	BX LR

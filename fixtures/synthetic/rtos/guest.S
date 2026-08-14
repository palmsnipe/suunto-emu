/* Synthetic ARM_CM4F gate source.  This is documentation/provenance only;
 * guest_image.h is the committed raw image consumed by the test. */

reset:      ldr     r7, =mailbox
            movs    r1, #1              /* reset */
            svc     #0

svc:        movs    r1, #2              /* svc-start */
            msr     psp, r0
            bx      lr                  /* fabricated task-A PSP return */

task_a:     movs    r1, #3              /* task-a */
            vmov    s16, r0
            /* Load S0 from fp_one, then add it to itself. */
            vadd.f32 s0, s0, s0
            str     r1, [systick, #0]
wait_a:     ldr     r0, [r6]
            cmp     r0, #2
            beq     fp_restored
            b       wait_a

systick:    movs    r1, #4              /* tick */
            str     r1, [mailbox, #12]
            str     pendsv_set, [icsr]

pendsv:     movs    r1, #5              /* pendsv */
            mrs     r0, psp
            stmdb   r0!, {r4-r11}
            vstmdb  r0!, {s16-s31}
            /* Save the current PSP, load the other task, restore both
             * software-saved banks, then return through the correct frame. */

task_b:     movs    r1, #6              /* task-b */
wait_b:     ldr     r0, [r6]
            cmp     r0, #2
            bne     wait_b
            movs    r1, #10             /* wfi */
            wfi

irq_low:    movs    r1, #7              /* irq-low */
            str     irq1_set, [nvic_ispr]
            movs    r1, #9              /* irq-low-return */
            str     r1, [mailbox, #32]
            movs    r1, #2
            str     r1, [task_flag]

irq_high:   movs    r1, #8              /* irq-high */
            bx      lr

irq_wake:   movs    r1, #11             /* wake */
            str     pendsv_set, [icsr]

fp_restored: movs   r1, #12             /* fp-restored */
             movs   r1, #13             /* done */
             bkpt   #0

/* The image is hand-encoded with the C99 integer-only fixture generator;
 * no assembler, compiler, firmware, or host runtime is required. */

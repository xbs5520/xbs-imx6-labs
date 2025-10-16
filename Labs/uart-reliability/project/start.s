
.global _start

_start:
    /*Interrupt verctor table*/
    ldr pc, =Reset_Handler              /*reset*/
    ldr pc, =Undefined_Handler              /*undefined*/
    ldr pc, =SVC_Handler                /*supervisor*/
    ldr pc, =PrefAbort_Handler          /*prefetch abort*/
    ldr pc, =DataAbort_Handler          /*data abort*/
    ldr pc, =NotUsed_Handler            /*not used*/
    ldr pc, =IRQ_Handler                /*IRQ*/
    ldr pc, =FIQ_Handler                /*FIQ*/

Reset_Handler:
    /*Reset handler*/
    cpsid i                             /*disable Global interrupts*/

    /* close I D chach && MMU*/
    mrc p15, 0, r0, c1, c0, 0          /*read CP15 C1 to r0 register*/
    bic r0, r0, #(0x1 << 12)           /*clear C1 I to disable I cache*/
    bic r0, r0, #(0x1 << 2)            /*clear C1 C to disable D cache*/
    bic r0, r0, #0x2                   /*clear C1 A close alignment check*/
    bic r0, r0, #(0x1 << 11)           /*clear C1 Z clost branch prediction*/
    bic r0, r0, #(0x1 << 0)            /*clear C1 M to disable MMU*/ 
    mcr p15, 0, r0, c1, c0, 0          /*write back to CP15 C1 register*/   

#if 0
    /* 设置中断向量偏移 */
    ldr r0, =0x87800000
    dsb
    isb
    MCR p15,0,r0,c12,c0,0  /* 设置VBAR寄存器=0X87800000 */
    dsb
    isb
#endif


# .global _bss_start
# _bss_start:
#     .word __bss_start

# .global _bss_end
# _bss_end:
#     .word __bss_end
    
#     /*clear BSS*/
#     ldr r0, _bss_start
#     ldr r1, _bss_end
#     mov r2, #0
# bss_loop:
#     stmia r0!, {r2}
#     cmp r0, r1      /* compare r0 r1 */
#     ble bss_loop    /*if r0 < r1,clear bss*/

/*set all moudle's sp*/
/* IRQ mode */
    mrs r0, cpsr
    bic r0, r0, #0x1f
    orr r0, r0, #0x12
    msr cpsr, r0
    ldr sp, =0x80600000

/* sys mode */
    mrs r0, cpsr
    bic r0, r0, #0x1f
    orr r0, r0, #0x1f
    msr cpsr, r0
    ldr sp, =0x80400000

# change to SVC mode
# read cpsr to r0
    mrs r0, cpsr        
# clear bit 0 - 4 11111 0X1f (not change other bit)
    bic r0, r0, #0X1f
# set bit 4 - 0 10011 0X13
    orr r0, r0, #0X13
# write to cpsr
    msr cpsr, r0
# set stack point
    ldr sp, =0X80200000

    cpsie i /* Enable Global interrupts */


# go to main
    b main

    

    /* undefine  */
    Undefined_Handler:
    ldr r0, =Undefined_Handler
    bx r0

    /* SVC  */
    SVC_Handler:
    ldr r0, =SVC_Handler
    bx r0

    /* prefetch */
    PrefAbort_Handler:
    ldr r0, =PrefAbort_Handler
    bx r0

    /* data abort */
    DataAbort_Handler:
    ldr r0, =DataAbort_Handler
    bx r0

    /* not use */
    NotUsed_Handler:
    ldr r0, =NotUsed_Handler
    bx r0

    /* IRQ */
    IRQ_Handler:
    push {lr}                   /*save lr address*/
    push {r0-r3, r12}           /*save r0-r3 and r12 registers*/

    mrs r0, spsr                /*read spsr*/
    push {r0}                   /*save spsr*/

    mrc p15, 4, r1, c15, c0, 0 /*read CP15 c15 to r1 register*/
    add r1, r1, #0x2000        /*GIC + 0x2000 is base CPU interface address*/
    ldr r0, [r1, #0xC]         /*read base + 0x0c = GIC_IAR register*/    

    push {r0, r1}              /*save GIC_IAR and base address*/

    cps #0x13                  /*change to SVC mode allow other interrupt*/

    push {lr}                  /* save SVC lr */
    ldr r2, =system_irq_handler/* loads C function - sys.. to r2*/
    blx r2                     /* excute function with a param */
    pop {lr}                   /* function over lr pop*/

    cps #0x12                  /* IRQ mode */
    pop {r0, r1}
    str r0, [r1, #0X10]        /* 中断执行完成，写 EOIR */
    pop {r0}

    msr spsr_cxsf, r0          /* back to spsr */
    pop {r0-r3, r12}
    pop {lr}
    subs pc, lr, #4

    /* FIQ  */
    FIQ_Handler:
        ldr r0, =FIQ_Handler
    bx r0

 .file   "spinlock.c"
        .text
        .globl  spinlock_init
        .type   spinlock_init, @function
spinlock_init:
.LFB0:
        .cfi_startproc
        pushq   %rbp
        .cfi_def_cfa_offset 16
        .cfi_offset 6, -16
        movq    %rsp, %rbp
        .cfi_def_cfa_register 6
        movq    %rdi, -24(%rbp)
        movq    -24(%rbp), %rax
        movq    %rax, -8(%rbp)
        movl    $0, -12(%rbp)
        movl    -12(%rbp), %eax
        movl    %eax, %edx
        movq    -8(%rbp), %rax
        xchgl   (%rax), %edx
        nop
        popq    %rbp
        .cfi_def_cfa 7, 8
        ret
        .cfi_endproc
.LFE0:
        .size   spinlock_init, .-spinlock_init
        .globl  spinlock_lock
        .type   spinlock_lock, @function
spinlock_lock:
.LFB1:
        .cfi_startproc
        pushq   %rbp
        .cfi_def_cfa_offset 16
        .cfi_offset 6, -16
        movq    %rsp, %rbp
        .cfi_def_cfa_register 6
        movq    %rdi, -24(%rbp)
.L4:
        movl    $0, -12(%rbp)
        movq    -24(%rbp), %rax
        movq    %rax, -8(%rbp)
        movl    $1, -16(%rbp)
        movl    -16(%rbp), %ecx
        movq    -8(%rbp), %rsi
        leaq    -12(%rbp), %rdx
        movl    (%rdx), %eax
        lock cmpxchgl   %ecx, (%rsi)
        movl    %eax, %ecx
        sete    %al
        testb   %al, %al
        jne     .L3
        movl    %ecx, (%rdx)
.L3:
        xorl    $1, %eax
        testb   %al, %al
        jne     .L4
        nop
        nop
        popq    %rbp
        .cfi_def_cfa 7, 8
        ret
        .cfi_endproc
.LFE1:
        .size   spinlock_lock, .-spinlock_lock
        .globl  spinlock_unlock
        .type   spinlock_unlock, @function
spinlock_unlock:
.LFB2:
        .cfi_startproc
        pushq   %rbp
        .cfi_def_cfa_offset 16
        .cfi_offset 6, -16
        movq    %rsp, %rbp
        .cfi_def_cfa_register 6
        movq    %rdi, -24(%rbp)
        movq    -24(%rbp), %rax
        movq    %rax, -8(%rbp)
        movl    $0, -12(%rbp)
        movl    -12(%rbp), %eax
        movl    %eax, %edx
        movq    -8(%rbp), %rax
        xchgl   (%rax), %edx
        nop
        popq    %rbp
        .cfi_def_cfa 7, 8
        ret
        .cfi_endproc
.LFE2:
        .size   spinlock_unlock, .-spinlock_unlock
        .globl  spin
        .bss
        .align 4
        .type   spin, @object
        .size   spin, 4
spin:
        .zero   4
        .globl  counter
        .align 4
        .type   counter, @object
        .size   counter, 4
counter:
        .zero   4
        .text
        .globl  worker
        .type   worker, @function
worker:
.LFB3:
        .cfi_startproc
        pushq   %rbp
        .cfi_def_cfa_offset 16
        .cfi_offset 6, -16
        movq    %rsp, %rbp
        .cfi_def_cfa_register 6
        subq    $24, %rsp
        movq    %rdi, -24(%rbp)
        movl    $0, -4(%rbp)
        jmp     .L7
.L8:
        leaq    spin(%rip), %rax
        movq    %rax, %rdi
        call    spinlock_lock
        movl    counter(%rip), %eax
        addl    $1, %eax
        movl    %eax, counter(%rip)
        leaq    spin(%rip), %rax
        movq    %rax, %rdi
        call    spinlock_unlock
        addl    $1, -4(%rbp)
.L7:
        cmpl    $999999, -4(%rbp)
        jle     .L8
        movl    $0, %eax
        leave
        .cfi_def_cfa 7, 8
        ret
        .cfi_endproc
.LFE3:
        .size   worker, .-worker
        .section        .rodata
        .align 8
.LC0:
        .string "Starting spinlock test with %d threads, %d iterations each\n"
.LC1:
        .string "Expected final counter: %d\n"
.LC2:
        .string "Actual final counter: %d\n"
.LC3:
        .string "PASSED"
.LC4:
        .string "FAILED"
.LC5:
        .string "Test %s\n"
        .text
        .globl  main
        .type   main, @function
main:
.LFB4:
        .cfi_startproc
        pushq   %rbp
        .cfi_def_cfa_offset 16
        .cfi_offset 6, -16
        movq    %rsp, %rbp
        .cfi_def_cfa_register 6
        subq    $48, %rsp
        leaq    spin(%rip), %rax
        movq    %rax, %rdi
        call    spinlock_init
        movl    $1000000, %edx
        movl    $4, %esi
        leaq    .LC0(%rip), %rax
        movq    %rax, %rdi
        movl    $0, %eax
        call    printf@PLT
        movl    $4000000, %esi
        leaq    .LC1(%rip), %rax
        movq    %rax, %rdi
        movl    $0, %eax
        call    printf@PLT
        movl    $0, -4(%rbp)
        jmp     .L11
.L12:
        leaq    -48(%rbp), %rax
        movl    -4(%rbp), %edx
        movslq  %edx, %rdx
        salq    $3, %rdx
        addq    %rdx, %rax
        movl    $0, %ecx
        leaq    worker(%rip), %rdx
        movl    $0, %esi
        movq    %rax, %rdi
        call    pthread_create@PLT
        addl    $1, -4(%rbp)
.L11:
        cmpl    $3, -4(%rbp)
        jle     .L12
        movl    $0, -8(%rbp)
        jmp     .L13
.L14:
        movl    -8(%rbp), %eax
        cltq
        movq    -48(%rbp,%rax,8), %rax
        movl    $0, %esi
        movq    %rax, %rdi
        call    pthread_join@PLT
        addl    $1, -8(%rbp)
.L13:
        cmpl    $3, -8(%rbp)
        jle     .L14
        movl    counter(%rip), %eax
        movl    %eax, %esi
        leaq    .LC2(%rip), %rax
        movq    %rax, %rdi
        movl    $0, %eax
        call    printf@PLT
        movl    counter(%rip), %eax
        cmpl    $4000000, %eax
        jne     .L15
        leaq    .LC3(%rip), %rax
        jmp     .L16
.L15:
        leaq    .LC4(%rip), %rax
.L16:
        movq    %rax, %rsi
        leaq    .LC5(%rip), %rax
        movq    %rax, %rdi
        movl    $0, %eax
        call    printf@PLT
        movl    $0, %eax
        leave
        .cfi_def_cfa 7, 8
        ret
        .cfi_endproc
.LFE4:
        .size   main, .-main
        .ident  "GCC: (Debian 14.2.0-19) 14.2.0"
        .section        .note.GNU-stack,"",@progbits

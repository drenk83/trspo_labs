/* jmp, а не call: setjmp должен запомнить адрес в libpng, а не в этом стабе.
 * Иначе longjmp вернётся сюда и запись PNG сломается. */
    .text
    .globl _setjmp
    .globl setjmp
    .globl __sigsetjmp
    .type _setjmp, @function
_setjmp:
setjmp:
__sigsetjmp:
    movq host_setjmp_fn(%rip), %rax
    testq %rax, %rax
    jz 1f
    jmp *%rax
1:
    xorl %eax, %eax
    ret

    .globl longjmp
    .globl _longjmp
    .type longjmp, @function
longjmp:
_longjmp:
    movq host_longjmp_fn(%rip), %rax
    testq %rax, %rax
    jz 1f
    jmp *%rax
1:
    ud2

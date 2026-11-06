/* jmp, не call: setjmp запоминает адрес libpng, а не этого стаба
 * три имени: glibc зовёт setjmp, _setjmp или __sigsetjmp */
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
    xorl %eax, %eax   /* таблицы ещё нет, притворяемся что ошибки не было */
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
    ud2               /* longjmp без таблицы прыгать некуда */

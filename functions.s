.section .data

.section .text

.globl get_bit
.globl get_byte
.globl downto
.globl prepare_allturbo
.type get_bit, @function
.type get_byte, @function
.type downto, @function
.type prepare_allturbo, @function



# @FUNCTION, get_bit:
# #include <inttypes.h>
# extern char get_bit(uint64_t input, char bitno);
get_bit:
# move bitno to counter register
movq %rsi, %rcx
# move input to return register
movq %rdi, %rax
# extract bit at bitno
shrq %cl, %rax
andq $1, %rax
ret



# @FUNCTION, get_byte:
# #include <inttypes.h>
# extern char get_byte(uint64_t input, char bitno);
get_byte:
# move bitno to counter register
movq %rsi, %rcx
# move input to return register
movq %rdi, %rax
# extract bit at bitno
shrq %cl, %rax
andq $255, %rax
ret



# @FUNCTION, downto:
# #include <inttypes.h>
# extern uint64_t downto(uint64_t input, char upper, char lower);
downto:
# equivalent to: if (upper - lower < 0) return -2;
subq %rdx, %rsi
cmpq $0, %rsi
jl error_input
# move input to retval
movq %rdi, %rax

# shift %rax by %rdx to the right
movq %rdx, %rcx
shrq %cl, %rax

movq $0, %rdi
# %rdx is not needed anymore, so used to hold (2^n)-1
movq $0, %rdx
l_shift_loop:
# shift %rdx by %rsi (upper - lower) times to the left
cmpq %rsi, %rdi
jg l_shift_loop_end
shlq $1, %rdx
incq %rdx
incq %rdi
jmp l_shift_loop

l_shift_loop_end:
# cut the upper bits which are not needed and return
andq %rdx, %rax
ret

error_input:
movq $-2, %rax
ret



# @FUNCTION, prepare_allturbo:
# #include <inttypes.h>
# extern uint64_t prepare_allturbo(uint64_t curr_val, char newval);

prepare_allturbo:
movq %rdi, %rax
shrq $32, %rax
shlq $32, %rax
movq $0, %rcx

prepare_allturbo_loop:
cmpq $4, %rcx
je prepare_allturbo_loop_end
orq %rsi, %rax
shlq $8, %rsi
incq %rcx
jmp prepare_allturbo_loop

prepare_allturbo_loop_end:
ret

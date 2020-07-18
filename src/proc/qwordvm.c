#include <lib/cmem.h>
#include <lib/klib.h>
#include <proc/qwordvm.h>
#include <proc/task.h>
#include <sys/cpu.h>

// TODO: change to something normal in the future
static inline int privilege_check(size_t base, size_t len) {
    if (base & (size_t)0x800000000000 || (base + len) & (size_t)0x800000000000)
        return 1;
    else
        return 0;
}

static int qwordvm_is_peek_allowed(uint64_t addr, uint64_t size) {
    return !privilege_check(addr, size);
}

static int qwordvm_is_poke_allowed(uint64_t addr, uint64_t size) {
    return !privilege_check(addr, size);
}

static inline int qwordvm_is_syscall_supported(uint64_t syscall_num) {
    uint64_t unsupported_syscalls[] = {
        10, // fork syscall
        11, // execve syscall
        44, // interp syscall
    };
    for (uint64_t i = 0; i < sizeof(unsupported_syscalls) / 8; ++i) {
        if (unsupported_syscalls[i] == syscall_num) {
            return 0;
        }
    }
    return 1;
}

#define QWORDVM_DEFINE_BINARY_OP_HANDLER(name, op)                  \
    case name:                                                      \
        if (sp < 2) {                                               \
            return QWORDVM_ERROR_STACK_UNDERFLOW;                   \
        }                                                           \
        stack[sp - 2] = (uint64_t)(stack[sp - 2] op stack[sp - 1]); \
        sp -= 1;                                                    \
        ip += 1;                                                    \
        break

#define QWORDVM_DEFINE_UNARY_OP_HANDLER(name, op)      \
    case name:                                         \
        if (sp < 1) {                                  \
            return QWORDVM_ERROR_STACK_UNDERFLOW;      \
        }                                              \
        stack[sp - 1] = (uint64_t)(op(stack[sp - 1])); \
        ip += 1;                                       \
        break

#define QWORDVM_DEFINE_PUSH_IMM_HANDLER(bits)                             \
    case QWORDVM_OPCODE_PUSH##bits:                                       \
        if (sp == stack_size) {                                           \
            return QWORDVM_ERROR_STACK_OVERFLOW;                          \
        }                                                                 \
        if ((ip + 1 + (bits / 8)) > code_size) {                          \
            return QWORDVM_ERROR_CODE_OUT_OF_BOUNDS;                      \
        }                                                                 \
        stack[sp] = (uint64_t)(*(uint##bits##_t *)(code_start + ip + 1)); \
        sp += 1;                                                          \
        ip += 1 + (bits / 8);                                             \
        break

#define QWORDVM_DEFINE_PEEK_HANDLER(bits)                               \
    case QWORDVM_OPCODE_PEEK##bits:                                     \
        if (sp < 1) {                                                   \
            return QWORDVM_ERROR_STACK_UNDERFLOW;                       \
        }                                                               \
        if (!qwordvm_is_peek_allowed(stack[sp - 1], bits / 8)) {        \
            return QWORDVM_ERROR_INVALID_PEEK;                          \
        }                                                               \
        stack[sp - 1] = (uint64_t)(*(uint##bits##_t *)(stack[sp - 1])); \
        ip += 1;                                                        \
        break

#define QWORDVM_DEFINE_POKE_HANDLER(bits)                                     \
    case QWORDVM_OPCODE_POKE##bits:                                           \
        if (sp < 2) {                                                         \
            return QWORDVM_ERROR_STACK_UNDERFLOW;                             \
        }                                                                     \
        if (!qwordvm_is_poke_allowed(stack[sp - 2], bits / 8)) {              \
            return QWORDVM_ERROR_INVALID_POKE;                                \
        }                                                                     \
        *(uint##bits##_t *)(stack[sp - 2]) = (uint##bits##_t)(stack[sp - 1]); \
        ip += 1;                                                              \
        sp -= 2;                                                              \
        break

int qwordvm_interp(uint64_t code_start, size_t code_size, uint64_t stack_start,
                   uint64_t stack_size) {
    if (!qwordvm_is_peek_allowed(code_start, code_size)) {
        kprint(KPRN_INFO, "qwordvm_interp: invalid code");
        return QWORDVM_ERROR_INVALID_CODE;
    }
    if (!qwordvm_is_poke_allowed(stack_start, stack_size)) {
        kprint(KPRN_INFO, "qwordvm_interp: invalid stack");
        return QWORDVM_ERROR_INVALID_STACK;
    }

    // make this system call preemtable as it doesn't lock anything
    pid_t current_task = cpu_locals[current_cpu].current_task;
    struct thread_t *thread = task_table[current_task];
    locked_write(int, &thread->in_syscall, 0);

    uint64_t *stack = (uint64_t *)stack_start;
    uint64_t sp = 0, ip = 0;
    while (0 <= ip && ip < code_size) {
        enum qwordvm_opcode opcode = *(uint8_t *)(code_start + ip);
        switch (opcode) {
            QWORDVM_DEFINE_BINARY_OP_HANDLER(QWORDVM_OPCODE_ADD, +);
            QWORDVM_DEFINE_BINARY_OP_HANDLER(QWORDVM_OPCODE_SUB, -);
            QWORDVM_DEFINE_BINARY_OP_HANDLER(QWORDVM_OPCODE_MUL, *);
            QWORDVM_DEFINE_BINARY_OP_HANDLER(QWORDVM_OPCODE_LT, <);
            QWORDVM_DEFINE_BINARY_OP_HANDLER(QWORDVM_OPCODE_LE, <=);
            QWORDVM_DEFINE_BINARY_OP_HANDLER(QWORDVM_OPCODE_GT, >);
            QWORDVM_DEFINE_BINARY_OP_HANDLER(QWORDVM_OPCODE_GE, >=);
            QWORDVM_DEFINE_BINARY_OP_HANDLER(QWORDVM_OPCODE_EQ, ==);
            QWORDVM_DEFINE_BINARY_OP_HANDLER(QWORDVM_OPCODE_NE, !=);
            QWORDVM_DEFINE_BINARY_OP_HANDLER(QWORDVM_OPCODE_AND, &&);
            QWORDVM_DEFINE_BINARY_OP_HANDLER(QWORDVM_OPCODE_OR, ||);
            QWORDVM_DEFINE_BINARY_OP_HANDLER(QWORDVM_OPCODE_BAND, &);
            QWORDVM_DEFINE_BINARY_OP_HANDLER(QWORDVM_OPCODE_BOR, |);
            QWORDVM_DEFINE_BINARY_OP_HANDLER(QWORDVM_OPCODE_BXOR, ^);
            QWORDVM_DEFINE_BINARY_OP_HANDLER(QWORDVM_OPCODE_BSHL, <<);
            QWORDVM_DEFINE_BINARY_OP_HANDLER(QWORDVM_OPCODE_BSHR, >>);
            QWORDVM_DEFINE_UNARY_OP_HANDLER(QWORDVM_OPCODE_NOT, !);
            QWORDVM_DEFINE_UNARY_OP_HANDLER(QWORDVM_OPCODE_BNOT, ~);
            QWORDVM_DEFINE_PUSH_IMM_HANDLER(8);
            QWORDVM_DEFINE_PUSH_IMM_HANDLER(16);
            QWORDVM_DEFINE_PUSH_IMM_HANDLER(32);
            QWORDVM_DEFINE_PUSH_IMM_HANDLER(64);
            QWORDVM_DEFINE_PEEK_HANDLER(8);
            QWORDVM_DEFINE_PEEK_HANDLER(16);
            QWORDVM_DEFINE_PEEK_HANDLER(32);
            QWORDVM_DEFINE_PEEK_HANDLER(64);
            QWORDVM_DEFINE_POKE_HANDLER(8);
            QWORDVM_DEFINE_POKE_HANDLER(16);
            QWORDVM_DEFINE_POKE_HANDLER(32);
            QWORDVM_DEFINE_POKE_HANDLER(64);

            case QWORDVM_OPCODE_DIV:
                if (sp < 2) {
                    return QWORDVM_ERROR_STACK_UNDERFLOW;
                }
                if (stack[sp - 1] == 0) {
                    return QWORDVM_ERROR_DIVISION_BY_ZERO;
                }
                stack[sp - 2] = stack[sp - 2] / stack[sp - 1];
                sp -= 1;
                ip += 1;
                break;
            case QWORDVM_OPCODE_REM:
                if (sp < 2) {
                    return QWORDVM_ERROR_STACK_UNDERFLOW;
                }
                if (stack[sp - 1] == 0) {
                    return QWORDVM_ERROR_DIVISION_BY_ZERO;
                }
                stack[sp - 2] = stack[sp - 2] % stack[sp - 1];
                sp -= 1;
                ip += 1;
                break;
            case QWORDVM_OPCODE_JMP:
                if (sp < 1) {
                    return QWORDVM_ERROR_STACK_UNDERFLOW;
                }
                ip = stack[sp - 1];
                sp -= 1;
                break;
            case QWORDVM_OPCODE_JMPC:
                if (sp < 2) {
                    return QWORDVM_ERROR_STACK_UNDERFLOW;
                }
                if (stack[sp - 2] != 0) {
                    ip = stack[sp - 1];
                } else {
                    ip += 1;
                }
                sp -= 2;
                break;
            case QWORDVM_OPCODE_SWAP:
                if (sp < 2) {
                    return QWORDVM_ERROR_STACK_UNDERFLOW;
                }
                uint64_t tmp = stack[sp - 2];
                stack[sp - 2] = stack[sp - 1];
                stack[sp - 1] = tmp;
                ip += 1;
                break;
            case QWORDVM_OPCODE_DUP:
                if (sp == stack_size) {
                    return QWORDVM_ERROR_STACK_OVERFLOW;
                }
                stack[sp] = stack[sp - 1];
                break;
            case QWORDVM_OPCODE_HALT:
                kprint(KPRN_DBG, "Halting");
                return QWORDVM_SUCCESS;
            case QWORDVM_OPCODE_NOP:
                ip += 1;
                break;
            case QWORDVM_OPCODE_SYSCALL:
                if (sp < 2) {
                    return QWORDVM_ERROR_STACK_UNDERFLOW;
                }
                uint64_t syscall_number = stack[sp - 1];
                uint64_t args_count = stack[sp - 2];
                if (args_count > 6) {
                    return QWORDVM_ERROR_TOO_MANY_ARGUMENTS;
                }
                extern syscall_count;
                // TODO: Detect whether syscall is supported in interpreted mode
                if (syscall_number >= &syscall_count) {
                    return QWORDVM_ERROR_UNKNOWN_SYSCALL;
                }
                if (!qwordvm_is_syscall_supported(syscall_number)) {
                    return QWORDVM_ERROR_UNKNOWN_SYSCALL;
                }
                sp -= 2;
                if (sp < args_count) {
                    return QWORDVM_ERROR_STACK_UNDERFLOW;
                }
                uint64_t args[QWORDVM_ARGS_MAX];
                memcpy(args, stack + (sp - args_count), 8 * args_count);
                struct regs_t regs;
                memset(&regs, 0, sizeof(regs));
                if (args_count >= 1) {
                    regs.rdi = args[0];
                }
                if (args_count >= 2) {
                    regs.rsi = args[1];
                }
                if (args_count >= 3) {
                    regs.rdx = args[2];
                }
                if (args_count >= 4) {
                    regs.r10 = args[3];
                }
                if (args_count >= 5) {
                    regs.r8 = args[4];
                }
                if (args_count >= 6) {
                    regs.r9 = args[5];
                }
                sp -= args_count;
                extern uint64_t syscall_table[];
                uint64_t syscall = syscall_table[syscall_number];
                kprint(KPRN_DBG, "qwordvm_interp: calling syscall at %X",
                       syscall);
                // okey, now we want to be non-preemtable
                locked_write(int, &thread->in_syscall, 1);
                int result = ((int (*)(struct regs_t *))syscall)(&regs);
                // back to being preemtable
                locked_write(int, &thread->in_syscall, 0);
                stack[sp] = (uint64_t)result;
                stack[sp + 1] = regs.rdx;
                sp += 2;
                ip += 1;
                break;
            case QWORDVM_OPCODE_CLEAR:
                if (sp < 1) {
                    return QWORDVM_ERROR_STACK_UNDERFLOW;
                }
                sp -= 1;
                ip += 1;
                break;
            default:
                return QWORDVM_ERROR_UNKNOWN_INSTRUCTION;
        }
    }
    return QWORDVM_ERROR_CODE_OUT_OF_BOUNDS;
}

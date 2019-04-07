/*
 * Lux ACPI Implementation
 * Copyright (C) 2018-2019 by Omar Muhamed
 */

#pragma once

#include <stddef.h>
#include <stdint.h>

#include <acpispec/resources.h>
#include <acpispec/tables.h>
#include <lai/host.h>

// Even in freestanding environments, GCC requires memcpy(), memmove(), memset()
// and memcmp() to be present. Thus, we just use them directly.
void *memcpy(void *, const void *, size_t);
void *memmove(void *, const void *, size_t);
void *memset(void *, int, size_t);
int memcmp(const void *, const void *, size_t);

#define LAI_STRINGIFY(x) #x
#define LAI_EXPAND_STRINGIFY(x) LAI_STRINGIFY(x)

#define LAI_ENSURE(cond) \
    do { \
        if(!(cond)) \
            lai_panic("assertion failed: " #cond " at " \
                       __FILE__ ":" LAI_EXPAND_STRINGIFY(__LINE__) "\n"); \
    } while(0)

#define ACPI_MAX_NAME            64
#define ACPI_MAX_RESOURCES        512

#define LAI_NAMESPACE_NAME        1
#define LAI_NAMESPACE_ALIAS        2
#define LAI_NAMESPACE_SCOPE        3
#define LAI_NAMESPACE_FIELD        4
#define LAI_NAMESPACE_METHOD        5
#define LAI_NAMESPACE_DEVICE        6
#define LAI_NAMESPACE_INDEXFIELD    7
#define LAI_NAMESPACE_MUTEX        8
#define LAI_NAMESPACE_PROCESSOR    9
#define LAI_NAMESPACE_BUFFER_FIELD    10
#define LAI_NAMESPACE_THERMALZONE    11

// ----------------------------------------------------------------------------
// Data types defined by AML.
// ----------------------------------------------------------------------------
// Value types: integer, string, buffer, package.
#define LAI_INTEGER            1
#define LAI_STRING             2
#define LAI_BUFFER             3
#define LAI_PACKAGE            4
// Handle type: this is used to represent device (and other) namespace nodes.
#define LAI_HANDLE             5
// Reference types: obtained from RefOp() or Index().
#define LAI_STRING_INDEX       6
#define LAI_BUFFER_INDEX       7
#define LAI_PACKAGE_INDEX      8
// ----------------------------------------------------------------------------
// Internal data types of the interpreter.
// ----------------------------------------------------------------------------
// Name types: unresolved names and names of certain objects.
#define LAI_NULL_NAME          9
#define LAI_UNRESOLVED_NAME   10
#define LAI_ARG_NAME          11
#define LAI_LOCAL_NAME        12
#define LAI_DEBUG_NAME        13
// Reference types: references to object storage.
#define LAI_STRING_REFERENCE  14
#define LAI_BUFFER_REFERENCE  15
#define LAI_PACKAGE_REFERENCE 16

typedef struct lai_object_t
{
    int type;
    uint64_t integer;        // for Name()
    char *string;            // for Name()

    int package_size;        // for Package(), size in entries
    struct lai_object_t *package;    // for Package(), actual entries

    size_t buffer_size;        // for Buffer(), size in bytes
    void *buffer;            // for Buffer(), actual bytes

    char name[ACPI_MAX_NAME];    // for Name References
    struct lai_nsnode_t *handle;

    int index;
} lai_object_t;

typedef struct lai_nsnode_t
{
    char path[ACPI_MAX_NAME];    // full path of object
    int type;
    void *pointer;            // valid for scopes, methods, etc.
    size_t size;            // valid for scopes, methods, etc.

    char alias[ACPI_MAX_NAME];    // for Alias() only
    lai_object_t object;        // for Name()

    uint8_t op_address_space;    // for OpRegions only
    uint64_t op_base;        // for OpRegions only
    uint64_t op_length;        // for OpRegions only

    uint64_t field_offset;        // for Fields only, in bits
    size_t field_size;        // for Fields only, in bits
    uint8_t field_flags;        // for Fields only
    char field_opregion[ACPI_MAX_NAME];    // for Fields only

    uint8_t method_flags;        // for Methods only, includes ARG_COUNT in lowest three bits
    // Allows the OS to override methods. Mainly useful for _OSI, _OS and _REV.
    int (*method_override)(lai_object_t *args, lai_object_t *result);

    uint64_t indexfield_offset;    // for IndexFields, in bits
    char indexfield_index[ACPI_MAX_NAME];    // for IndexFields
    char indexfield_data[ACPI_MAX_NAME];    // for IndexFields
    uint8_t indexfield_flags;    // for IndexFields
    uint8_t indexfield_size;    // for IndexFields

    // TODO: Find a good mechanism for locks.
    //lai_lock_t mutex;        // for Mutex

    uint8_t cpu_id;            // for Processor

    char buffer[ACPI_MAX_NAME];        // for Buffer field
    uint64_t buffer_offset;        // for Buffer field, in bits
    uint64_t buffer_size;        // for Buffer field, in bits
} lai_nsnode_t;

#define LAI_POPULATE_CONTEXT_STACKITEM 1
#define LAI_METHOD_CONTEXT_STACKITEM 2
#define LAI_LOOP_STACKITEM 3
#define LAI_COND_STACKITEM 4
#define LAI_PKG_INITIALIZER_STACKITEM 5
#define LAI_OP_STACKITEM 6
// This implements lai_eval_operand(). // TODO: Eventually remove
// lai_eval_operand() by moving all parsing functionality into lai_exec_run().
#define LAI_EVALOPERAND_STACKITEM 10

typedef struct lai_stackitem_ {
    int kind;
    int opstack_frame;
    union {
        struct {
            lai_nsnode_t *ctx_handle;
            int ctx_limit;
        };
        struct {
            int loop_pred; // Loop predicate PC.
            int loop_end; // End of loop PC.
        };
        struct {
            int cond_taken; // Whether the conditional was true or not.
            int cond_end; // End of conditional PC.
        };
        struct {
            int pkg_index;
            int pkg_end;
            uint8_t pkg_result_mode;
        };
        struct {
            int op_opcode;
            uint8_t op_arg_modes[8];
            uint8_t op_result_mode;
        };
    };
} lai_stackitem_t;

typedef struct lai_state_t
{
    int pc;
    int limit;
    lai_object_t retvalue;
    lai_object_t arg[7];
    lai_object_t local[8];

    // Stack to track the current execution state.
    int stack_ptr;
    int opstack_ptr;
    lai_stackitem_t stack[16];
    lai_object_t opstack[16];
    int context_ptr; // Index of the last CONTEXT_STACKITEM.
} lai_state_t;

void lai_init_state(lai_state_t *);
void lai_finalize_state(lai_state_t *);

__attribute__((always_inline))
inline lai_object_t *lai_retvalue(lai_state_t *state) {
    return &state->retvalue;
}

__attribute__((always_inline))
inline lai_object_t *lai_arg(lai_state_t *state, int n) {
    return &state->arg[n];
}

acpi_fadt_t *lai_fadt;
acpi_aml_t *lai_dsdt;
size_t lai_ns_size;
volatile uint16_t lai_last_event;

// The remaining of these functions are OS independent!
// ACPI namespace functions
void lai_create_namespace(void *);
lai_nsnode_t *lai_resolve(char *);
lai_nsnode_t *lai_get_device(size_t);
lai_nsnode_t *lai_get_deviceid(size_t, lai_object_t *);
lai_nsnode_t *lai_enum(char *, size_t);
void lai_eisaid(lai_object_t *, char *);
size_t lai_read_resource(lai_nsnode_t *, acpi_resource_t *);

// ACPI Control Methods
int lai_eval(lai_object_t *, char *);
int lai_populate(lai_nsnode_t *, void *, size_t, lai_state_t *);
int lai_exec_method(lai_nsnode_t *, lai_state_t *);
int lai_eval_node(lai_nsnode_t *, lai_state_t *);

// Generic Functions
int lai_enable_acpi(uint32_t);
int lai_disable_acpi();
uint16_t lai_read_event();
void lai_set_event(uint16_t);
int lai_enter_sleep(uint8_t);
int lai_pci_route(acpi_resource_t *, uint8_t, uint8_t, uint8_t);


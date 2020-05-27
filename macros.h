#include <stdint.h>
#include <stdlib.h>

#ifdef assert
#undef assert
#endif
#define assert(cond) do { if (!(cond)) { fprintf(stderr, "*** Assertion failure at line %d in %s (function %s): assert(%s)\n", __LINE__, __FILE__, __FUNCTION__, #cond); exit(1); } } while (0)

#if DEBUG
#define debug_printf(args...) fprintf(stderr, "[DEBUG] "args)
#else
#define debug_printf(args...)
#endif

extern uint8_t brainfuck_memory[0x10000];
extern uint8_t shared_instruction_buffer[0x100];
extern char **argv;
extern int argc;
extern const char *input_name;
extern uint8_t *make_jit_footer(uint8_t *size);
extern uint8_t *make_jit_header(uint8_t *size);
extern uint8_t *make_add_instruction(uint8_t change, uint8_t *size);
extern uint8_t *make_substract_instruction(uint8_t change, uint8_t *size);
extern uint8_t *make_loop_begin_instruction(int32_t loop_end_offset, uint8_t *size);
extern uint8_t *make_loop_end_instruction(int32_t loop_begin_offset, uint8_t *size);
extern uint8_t *make_increment_memory_pt_instruction(uint16_t change, uint8_t *size);
extern uint8_t *make_decrement_memory_pt_instruction(uint16_t change, uint8_t *size);
extern uint8_t *make_output_instruction(uint8_t *size);
extern uint8_t *make_input_instruction(uint8_t *size);
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <stdbool.h>

#if __APPLE__
#include <mach/mach.h>
#elif __linux__
#include <malloc.h>
#include <sys/mman.h>
#else
#error Unsupported target
#endif

uint8_t brainfuck_memory[0x10000];
uint8_t shared_instruction_buffer[0x100];
char **argv;
int argc;
const char *input_name;

typedef struct {
	char instruction;
	uint8_t machine_code_size;
	uint8_t *machine_code;
	union {
		uint32_t count;
		uint32_t matching_bracket_index;
	};
} brainfuck_instruction_t;

typedef enum {
	MemoryPointerOverflowBehaviourInvalid = 3,
	MemoryPointerOverflowBehaviourExit = 2,
	MemoryPointerOverflowBehaviourLog = 1,
	MemoryPointerOverflowBehaviourDoNothing = 0
} mem_pt_overflow_behaviour_t;
static mem_pt_overflow_behaviour_t overflow_behaviour = MemoryPointerOverflowBehaviourDoNothing;

void memory_pt_overflow_handler(void) {
	if (overflow_behaviour == MemoryPointerOverflowBehaviourLog) {
		overflow_behaviour = MemoryPointerOverflowBehaviourDoNothing;
		fprintf(stderr, "[!!!] The memory pointer overflowed! This program might require more than 64KiBs of memory, which isn't supported.\n");
	}
	else if (overflow_behaviour == MemoryPointerOverflowBehaviourExit) {
		fprintf(stderr, "[!!!] The memory pointer overflowed. Exiting.\n");
		exit(-2);
	}
}

static brainfuck_instruction_t *parse_brainfuck(char *code, uint32_t *size_pt) {
	uint32_t size=1;
	brainfuck_instruction_t *instructions = malloc(size * sizeof(brainfuck_instruction_t));
	uint32_t i=0;
	for (char c=0; (c=*code); i++, code++) {
		instructions = realloc(instructions, (++size * sizeof(brainfuck_instruction_t)));
		brainfuck_instruction_t current_instruction;
		current_instruction.instruction = c;
		current_instruction.count = 1;
		bool reverse = 0;
		switch (c) {
			case '.':
				current_instruction.machine_code = make_output_instruction(&current_instruction.machine_code_size);
				break;
			case ',':
				current_instruction.machine_code = make_input_instruction(&current_instruction.machine_code_size);
				break;
			case '<': case '>': case '+': case '-':
				while (*(++code) == c) current_instruction.count++;
				--code;
				switch (c) {
					case '<':
						current_instruction.machine_code = make_decrement_memory_pt_instruction(current_instruction.count, &current_instruction.machine_code_size);
						break;
					case '>':
						current_instruction.machine_code = make_increment_memory_pt_instruction(current_instruction.count, &current_instruction.machine_code_size);
						break;
					case '+':
						current_instruction.machine_code = make_add_instruction(current_instruction.count, &current_instruction.machine_code_size);
						break;
					case '-':
						current_instruction.machine_code = make_substract_instruction(current_instruction.count, &current_instruction.machine_code_size);
						break;
				}
				break;
			case ']':
				reverse = 1;
				current_instruction.machine_code = make_loop_end_instruction(0, &current_instruction.machine_code_size);
			case '[': {
				int8_t change = (reverse ? -1 : 1);
				int32_t bracket_counter = change;
				char previous_c = 0;
				char c = 0;
				char *code_pt = code;
				uint32_t final_i = i;
				while ((c = *(code_pt += change))) {
					if (c == ']') bracket_counter--;
					else if (c == '[') bracket_counter++;
					if ((previous_c != c) || (c == '.') || (c == ',') || (c == '[') || (c == ']') || (c == '0')) final_i += change;
					previous_c = c;
					if (!bracket_counter) break;
				}
				assert((reverse && *code_pt == '[') || (!reverse && *code_pt == ']'));
				current_instruction.matching_bracket_index = final_i;
				if (!reverse) {
					current_instruction.machine_code = make_loop_begin_instruction(0, &current_instruction.machine_code_size);
				}
				break;
			}
			case 's': {
				current_instruction.machine_code = make_jit_header(&current_instruction.machine_code_size);
				break;
			}
			case 'e': {
				current_instruction.machine_code = make_jit_footer(&current_instruction.machine_code_size);
				break;
			}
			case '0': {
				current_instruction.machine_code = make_zero_instruction(&current_instruction.machine_code_size);
				break;
			}
			default: assert(false);
		}
		if (!current_instruction.machine_code) {
			current_instruction.machine_code_size = 0;
		}
		else {
			uint8_t *malloc_block = malloc(current_instruction.machine_code_size);
			memcpy(malloc_block, current_instruction.machine_code, current_instruction.machine_code_size);
			current_instruction.machine_code = malloc_block;
		}
		instructions[i] = current_instruction;
	}
	size--;
	uint8_t new_machine_code_size;
	for (i=0; i<size; i++) {
		brainfuck_instruction_t instruction = instructions[i];
		int32_t offset = 0;
		if (instruction.instruction == '[') {
			assert(instructions[instruction.matching_bracket_index].instruction == ']');
			for (uint32_t j=i+1; j<instruction.matching_bracket_index; j++) {
				offset += instructions[j].machine_code_size;
			}
			uint8_t *machine_code = make_loop_begin_instruction(offset, &new_machine_code_size);
			assert(machine_code);
			assert(new_machine_code_size == instruction.machine_code_size);
			memcpy(instruction.machine_code, machine_code, instruction.machine_code_size);
		}
		else if (instruction.instruction == ']') {
			assert(instructions[instruction.matching_bracket_index].instruction == '[');
			for (uint32_t j=i; j>instruction.matching_bracket_index; j--) {
				offset -= instructions[j].machine_code_size;
			}
			uint8_t *machine_code = make_loop_end_instruction(offset, &new_machine_code_size);
			assert(machine_code);
			assert(new_machine_code_size == instruction.machine_code_size);
			memcpy(instruction.machine_code, machine_code, instruction.machine_code_size);
		}
		instructions[i] = instruction;
	}
	if (size_pt) *size_pt = size;
	return instructions;
}

int main(int _argc, char **_argv) {
	argc = _argc;
	argv = _argv;
	
	// Read the environment variables
	{
		const char *env_var = getenv("BFJIT_MEMPT_OVERFLOW_BEHAVIOUR");
		if (env_var) {
			overflow_behaviour = MemoryPointerOverflowBehaviourInvalid;
			if (strlen(env_var) == 1) switch (*env_var - '0') {
				case MemoryPointerOverflowBehaviourDoNothing:
				case MemoryPointerOverflowBehaviourExit:
				case MemoryPointerOverflowBehaviourLog:
					overflow_behaviour = *env_var - '0';
					break;
			}
			if (overflow_behaviour == MemoryPointerOverflowBehaviourInvalid) {
				fprintf(stderr, "%s: Igoring invalid overflow behaviour\n", argv[0]);
				overflow_behaviour = MemoryPointerOverflowBehaviourDoNothing;
			}
		}
	}

	// Open the input file
	FILE *input_file;
	if (argc == 1) {
		input_file = stdin;
		input_name = "/dev/stdin";
	}
	else if (argc == 2) {
		input_name = argv[1];
		input_file = fopen(argv[1], "r");
		if (!input_file) {
			fprintf(stderr, "%s: %s: %s\n", argv[0], input_name, strerror(errno));
			return errno;
		}
	}
	else {
		fprintf(stderr, "Usage: %s [program]\n", argv[0]);
		return -1;
	}
	
	// Get the minified brainfuck code from the input file
	char *brainfuck_code = malloc(256);
	const char *allowed_brainfuck_commands = "+-<>.,[]";
	uint32_t brainfuck_code_size = 256;
	uint8_t allowed_brainfuck_commands_size = strlen(allowed_brainfuck_commands);
	{
		uint32_t buffer_index = 0;
		int c;
		while ((c = getc(input_file)) != EOF) {
			for (uint32_t i=0; i<allowed_brainfuck_commands_size; i++) {
				if (allowed_brainfuck_commands[i] != c) continue;
				brainfuck_code[buffer_index++] = (char)c;
				if (buffer_index == brainfuck_code_size) {
					brainfuck_code = realloc(brainfuck_code, (brainfuck_code_size *= 2));
				}
			}
		}
		brainfuck_code[buffer_index] = 0;
		brainfuck_code = realloc(brainfuck_code, (brainfuck_code_size=strlen(brainfuck_code))+1);
	}
	
	// Remove dead code
	if (brainfuck_code_size >= 2) while (1) {
		bool should_continue = 0;
		for (uint32_t i=0; i<(brainfuck_code_size-2); i++) {
			if (brainfuck_code_size < 2) break;
			bool should_delete = (
				!strncmp(brainfuck_code+i, "+-", 2) ||
				!strncmp(brainfuck_code+i, "-+", 2) ||
				!strncmp(brainfuck_code+i, "<>", 2) ||
				!strncmp(brainfuck_code+i, "><", 2)
			);
			if (should_delete) {
				memmove(brainfuck_code+i, brainfuck_code+i+2, strlen(brainfuck_code+i+2)+1);
				brainfuck_code_size -= 2;
				should_continue = 1;
				i--;
			}
			else if (!strncmp(brainfuck_code+i, ",,", 2)) {
				memmove(brainfuck_code+i, brainfuck_code+i+1, strlen(brainfuck_code+i+1)+1);
				brainfuck_code_size -= 1;
				should_continue = 1;
				i--;
			}
			else if (!strncmp(brainfuck_code+i, "[-]", 3)) {
				memmove(brainfuck_code+i, brainfuck_code+i+2, strlen(brainfuck_code+i+2)+1);
				brainfuck_code[i] = '0';
				brainfuck_code_size -= 2;
				should_continue = 1;
				i--;
			}
		}
		if (brainfuck_code_size < 2) break;
		if (!should_continue) break;
	}
	brainfuck_code_size += 3;
	brainfuck_code = realloc(brainfuck_code, brainfuck_code_size+1);
	memmove(brainfuck_code+1, brainfuck_code, brainfuck_code_size-2);
	brainfuck_code[0] = 's';
	brainfuck_code[brainfuck_code_size] = 0;
	brainfuck_code[brainfuck_code_size-1] = 'e';
	brainfuck_code[brainfuck_code_size-2] = '>';

	// Check if the '[' count matches the ']' count
	{
		int32_t delta = 0;
		for (uint32_t i=0; i<brainfuck_code_size; i++) {
			switch (brainfuck_code[i]) {
				case ']': delta -= 1; break;
				case '[': delta += 1; break;
			}
		}
		if (delta != 0) {
			fprintf(stderr, "%s: %s: '[' count minus ']' count is not 0, instead it is %d.\n", argv[0], input_name, delta);
			return 1;
		}
	}

	// Parse brainfuck and generate machine code
	uint32_t buffer_size = 0;
	uint32_t count;
	brainfuck_instruction_t *instructions = parse_brainfuck(brainfuck_code, &count);
	for (uint32_t i=0; i<count; i++) {
		buffer_size += instructions[i].machine_code_size;
	}

	// Construct the function
	uint32_t buffer_index = 0;
	uint8_t *function_construction_buffer;
	uint32_t real_buffer_size = buffer_size;
	int system_page_size = sysconf(_SC_PAGE_SIZE);
	assert(system_page_size != -1);
	real_buffer_size += (system_page_size - (buffer_size % system_page_size)) % system_page_size;

#if __APPLE__
	assert(posix_memalign((void**)&function_construction_buffer, system_page_size, real_buffer_size) == 0);
#elif __linux__
	function_construction_buffer = memalign(system_page_size, real_buffer_size);
#endif
	
	assert(function_construction_buffer != NULL);
	for (uint32_t i=0; i<count; i++) {
		memcpy(function_construction_buffer+buffer_index, instructions[i].machine_code, instructions[i].machine_code_size);
		buffer_index += instructions[i].machine_code_size;
	}

#if DEBUG
	// Write the function to a file
	FILE *outfile = fopen("function.dump", "w");
	if (outfile) {
		if (fwrite(function_construction_buffer, buffer_size, 1, outfile) == 1) {
			debug_printf("Successfully saved the produced code to function.dump.\n");
		}
		fclose(outfile);
	}
#endif

	// Cleanup
	debug_printf("Cleaning up...\n");
	for (uint32_t i=0; i<count; i++) {
		brainfuck_instruction_t *instruction = &instructions[i];
		if (instruction->machine_code) free(instruction->machine_code);
	}
	debug_printf("Finished executing.\n");
	free(instructions);

	// Make the function executable
#if __APPLE__
	assert(vm_protect(mach_task_self(), (vm_address_t)function_construction_buffer, real_buffer_size, 0, VM_PROT_EXECUTE | VM_PROT_READ) == KERN_SUCCESS);
#elif __linux__
	assert(mprotect(function_construction_buffer, real_buffer_size, PROT_READ | PROT_EXEC) != -1);
#endif

	// Call the function
	debug_printf("Executing...\n");
	int return_value = !!((int(*)(void))function_construction_buffer)();
	debug_printf("Execution completed.\n");

	// Free the function
#if __APPLE__
	assert(vm_protect(mach_task_self(), (vm_address_t)function_construction_buffer, real_buffer_size, 0, VM_PROT_WRITE | VM_PROT_READ) == KERN_SUCCESS);
#elif __linux__
	assert(mprotect(function_construction_buffer, real_buffer_size, PROT_READ | PROT_WRITE) != -1);
#endif
	free(function_construction_buffer);

	return return_value;
}

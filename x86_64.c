#ifdef __x86_64__
#include <stdio.h>
#include <sys/syscall.h>
#include <string.h>

// AL (8-bit)   => current data, may be different than the value at RSI+CX
// CX (16-bit)  => brainfuck memory pointer
// RSI (64-bit) => pointer to brainfuck_memory[0]

uint8_t *make_jit_footer(uint8_t *size) {
	uint8_t *instruction = &shared_instruction_buffer[0];
	static const uint8_t template[] = {
		0x48, 0x29, 0xC0, // SUB RAX, RAX
		0xC3              // RET
	};
	if (size) *size = sizeof(template);
	memcpy(instruction, template, sizeof(template));
	return instruction;
}

uint8_t *make_jit_header(uint8_t *size) {
	assert(sizeof(void *) == 8);
	uint8_t *instruction = &shared_instruction_buffer[0];
	static const uint8_t template[] = {
		0x48, 0x29, 0xC9,                                           // SUB RCX, RCX
		0x48, 0xBE, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, // MOV RSI, memory_base
		0x48, 0x29, 0xC0                                            // SUB RAX, RAX
	};
	if (size) *size = sizeof(template);
	memcpy(instruction, template, sizeof(template));
	uint8_t *base = &brainfuck_memory[0];
	memcpy(instruction+5, &base, 8);
	return instruction;
}

// +
uint8_t *make_add_instruction(uint8_t value_to_add, uint8_t *size) {
	uint8_t *instruction = &shared_instruction_buffer[0];
	static const uint8_t template[] = {
		0x04, 0xFF  // ADD AL, imm8
	};
	if (size) *size = sizeof(template);
	memcpy(instruction, template, sizeof(template));
	memcpy(instruction+1, &value_to_add, 1);
	return instruction;
}

// 0
uint8_t *make_zero_instruction(uint8_t *size) {
	uint8_t *instruction = &shared_instruction_buffer[0];
	static const uint8_t template[] = {
		0x28, 0xC0  // SUB AL, AL
	};
	if (size) *size = sizeof(template);
	memcpy(instruction, template, sizeof(template));
	return instruction;
}

// -
uint8_t *make_substract_instruction(uint8_t value_to_substract, uint8_t *size) {
	uint8_t *instruction = make_add_instruction(value_to_substract, size);
	instruction[0] = 0x2C; // replace ADD with SUB
	return instruction;
}

// [
uint8_t *make_loop_begin_instruction(int32_t loop_end_offset, uint8_t *size) {
	uint8_t *instruction = &shared_instruction_buffer[0];
	static const uint8_t template[] = {
		0x3C, 0x00,                        // CMP AL, 0x0
		0x0F, 0x84, 0xFF, 0xFF, 0xFF, 0xFF // JE loop_end_offset
	};
	if (size) *size = sizeof(template);
	memcpy(instruction, template, sizeof(template));
	memcpy(instruction+4, &loop_end_offset, 4);
	return instruction;
}

// ]
uint8_t *make_loop_end_instruction(int32_t loop_begin_offset, uint8_t *size) {
	uint8_t *instruction = make_loop_begin_instruction(loop_begin_offset, size);
	instruction[3] = 0x85; // replace JE with JNE
	return instruction;
}

// >
uint8_t *make_increment_memory_pt_instruction(uint16_t change, uint8_t *size) {
	uint8_t *instruction = &shared_instruction_buffer[0];
	static const uint8_t template[] = {
		0x48, 0x01, 0xCE,                                           // ADD RSI, RCX
		0x88, 0x06,                                                 // MOV [RSI], AL
		0x48, 0x29, 0xCE,                                           // SUB RSI, RCX
		0x66, 0x81, 0xC1, 0xFF, 0xFF,                               // ADD CX, change
		0x71, 0x12,                                                 // JNO +14
		0x50,                                                       // PUSH RAX
		0x56,                                                       // PUSH RSI
		0x51,                                                       // PUSH RCX
		0x48, 0xB8, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, // MOV RAX, overflow_handler
		0xFF, 0xD0,                                                 // CALL RAX
		0x59,                                                       // POP RCX
		0x5E,                                                       // POP RSI
		0x58,                                                       // POP RAX
		0x48, 0x01, 0xCE,                                           // ADD RSI, RCX
		0x8A, 0x06,                                                 // MOV AL, [RSI]
		0x48, 0x29, 0xCE                                            // SUB RSI, RCX
	};
	if (size) *size = sizeof(template);
	void(*fn)(void) = memory_pt_overflow_handler;
	memcpy(instruction, template, sizeof(template));
	memcpy(instruction+20, &fn, 8);
	memcpy(instruction+11, &change, 2);
	return instruction;
}

// <
uint8_t *make_decrement_memory_pt_instruction(uint16_t change, uint8_t *size) {
	uint8_t *instruction = make_increment_memory_pt_instruction(change, size);
	instruction[10] = 0xE9;
	return instruction;
}

// .
uint8_t *make_output_instruction(uint8_t *size) {
	uint8_t *instruction = &shared_instruction_buffer[0];
	static const uint8_t template[] = {
		0x48, 0x01, 0xCE,                                           // ADD RSI, RCX
		0x88, 0x06,                                                 // MOV [RSI], AL
		0x50,                                                       // PUSH RAX
		0x51,                                                       // PUSH RCX
		0x48, 0xB8, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, // MOV RAX, write_syscall
		0x48, 0xBF, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // MOV RDI, 0x1
		0x48, 0x89, 0xFA,                                           // MOV RDX, RDI
		0x0F, 0x05,                                                 // SYSCALL
		0x59,                                                       // POP RCX
		0x48, 0x29, 0xCE,                                           // SUB RSI, RCX
		0x58                                                        // POP RAX
	};
	memcpy(instruction, template, sizeof(template));
	if (size) *size = sizeof(template);
	uint64_t SYSC = SYS_write;
#if __APPLE__
	SYSC |= 0x2000000;
#endif
	memcpy(instruction+9, &SYSC, 8);
	return instruction;
}

// ,
uint8_t *make_input_instruction(uint8_t *size) {
	uint8_t *instruction = &shared_instruction_buffer[0];
	static const uint8_t template[] = {
		0x48, 0x01, 0xCE,                                           // ADD RSI, RCX
		0x50,                                                       // PUSH RAX
		0x51,                                                       // PUSH RCX
		0x48, 0xB8, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, // MOV RAX, read_syscall
		0x48, 0xBA, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // MOV RDX, 0x1
		0x48, 0x29, 0xFF,                                           // SUB RDI, RDI
		0x0F, 0x05,                                                 // SYSCALL
		0x59,                                                       // POP RCX
		//TODO: Check if RAX contains an error
		0x58,                                                       // POP RAX
		0x8A, 0x06,                                                 // MOV AL, [RSI]
		0x48, 0x29, 0xCE                                            // SUB RSI, RCX
	};
	memcpy(instruction, template, sizeof(template));
	if (size) *size = sizeof(template);
	uint64_t SYSC = SYS_read;
#if __APPLE__
	SYSC |= 0x2000000;
#endif
	memcpy(instruction+7, &SYSC, 8);
	return instruction;
}
#endif
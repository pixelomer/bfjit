#ifdef __x86_64__
#include <stdio.h>
#include <string.h>

// AL (8-bit)   => currently pointed data
// CX (16-bit)  => brainfuck memory pointer
// RSI (64-bit) => pointer to brainfuck_memory[0]

uint8_t *make_jit_footer(uint8_t *size) {
	uint8_t *instruction = &shared_instruction_buffer[0];
	if (size) *size = 4;
	
	// SUB RAX, RAX ;Set the return value to 0 by zeroing RAX.
	instruction[0] = 0x48;
	instruction[1] = 0x29;
	instruction[2] = 0xC0;

	// RET ;Return
	instruction[3] = 0xC3;

	return instruction;
}

uint8_t *make_jit_header(uint8_t *size) {
	assert(sizeof(void *) == 8);
	uint8_t *instruction = &shared_instruction_buffer[0];
	if (size) *size = 16;

	// SUB RCX, RCX ;CX is used as the memory pointer, which is the lowest word of RCX
	instruction[0] = 0x48;
	instruction[1] = 0x29;
	instruction[2] = 0xC9;

	// MOV RSI, memory_base ;RSI is the same as brainfuck_memory
	instruction[3] = 0x48;
	instruction[4] = 0xBE;
	uint8_t *base = &brainfuck_memory[0];
	debug_printf("Brainfuck memory base: %p\n", base);
	memcpy(instruction+5, &base, 8);

	// SUB RAX, RAX ;AL is the lowest byte of RAX. Zero RAX.
	instruction[13] = 0x48;
	instruction[14] = 0x29;
	instruction[15] = 0xC0;

	return instruction;
}

// +
uint8_t *make_add_instruction(uint8_t value_to_add, uint8_t *size) {
	uint8_t *instruction = &shared_instruction_buffer[0];
	if (size) *size = 2;
	instruction[0] = 0x04; // ADD AL, imm8 ;Add 8-bit immediate to 8-bit register AL
	instruction[1] = value_to_add; // Immediate value
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
	if (size) *size = 8;

	// CMP AL, $0
	instruction[0] = 0x3C;
	instruction[1] = 0x00;

	// JE loop_end_offset
	instruction[2] = 0x0F;
	instruction[3] = 0x84;
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
	if (size) *size = 21;

	// ADD RSI, RCX ;Get the current byte's address
	instruction[0] = 0x48;
	instruction[1] = 0x01;
	instruction[2] = 0xCE;

	// MOV [RSI], AL ;Write the current byte to memory
	instruction[3] = 0x88;
	instruction[4] = 0x06;

	// SUB RSI, RCX ;Get back to the brainfuck memory base
	instruction[5] = 0x48;
	instruction[6] = 0x29;
	instruction[7] = 0xCE;

	// ADD CX, change ;Add the change to CX, the brainfuck memory pointer (index)
	instruction[8] = 0x66;
	instruction[9] = 0x81;
	instruction[10] = 0xC1;
	memcpy(instruction+11, &change, 2);

	// ADD RSI, RCX ;Get the new byte's address
	instruction[13] = 0x48;
	instruction[14] = 0x01;
	instruction[15] = 0xCE;

	// MOV AL, [RSI] ;Read the new byte's value from memory into the AL register
	instruction[16] = 0x8A;
	instruction[17] = 0x06;

	// SUB RSI, RCX ;Get back to the brainfuck memory base
	instruction[18] = 0x48;
	instruction[19] = 0x29;
	instruction[20] = 0xCE;

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
	if (size) *size = 21;

	// Save the registers
	instruction[0] = 0x56; // PUSH RSI
	instruction[1] = 0x51; // PUSH RCX
	instruction[2] = 0x50; // PUSH RAX

	// MOV RDI, RAX ;RDI contains the 32-bit input for putchar()
	instruction[3] = 0x48;
	instruction[4] = 0x89;
	instruction[5] = 0xC7;

	// MOV RAX, putchar
	instruction[6] = 0x48;
	instruction[7] = 0xB8;
	int(*fn)(int) = putchar;
	memcpy(instruction+8, &fn, 8);
	
	// CALL RAX
	instruction[16] = 0xFF;
	instruction[17] = 0xD0;

	// Restore registers
	instruction[18] = 0x58; // POP RAX
	instruction[19] = 0x59; // POP RCX
	instruction[20] = 0x5E; // POP RSI

	return instruction;
}

// ,
uint8_t *make_input_instruction(uint8_t *size) {
	uint8_t *instruction = &shared_instruction_buffer[0];
	if (size) *size = 31;

	// Save registers
	instruction[0] = 0x56; // PUSH RSI
	instruction[1] = 0x51; // PUSH RCX

	// SUB RAX, RAX ;Zero RAX.
	instruction[2] = 0x48;
	instruction[3] = 0x29;
	instruction[4] = 0xC0;

	// MOV RAX, getchar
	instruction[5] = 0x48;
	instruction[6] = 0xB8;
	int(*fn)(void) = getchar;
	memcpy(instruction+7, &fn, 8);
	
	// CALL RAX ;The input character will be written to RAX.
	instruction[15] = 0xFF;
	instruction[16] = 0xD0;

	assert(sizeof(int) == 4);

	// CMP EAX, #EOF ;Check if EOF was read
	instruction[17] = 0x3D;
	int value = EOF;
	memcpy(instruction+18, &value, 4);

	// Restore registers
	instruction[22] = 0x59; // POP RCX
	instruction[23] = 0x5E; // POP RSI

	// JNE +1 ;If EAX isn't EOF, don't return
	instruction[24] = 0x0F;
	instruction[25] = 0x85;
	value = 1;
	memcpy(instruction+26, &value, 4);

	instruction[30] = 0xC3; // RET

	return instruction;
}
#endif
#include <stdint.h>
#include <stdio.h>
#include <stdbool.h>

#define OP_NUMBER 6
#define IMM_SHIFT 5
#define OP_CODE_SHIFT 12
#define DESTINATION_REGISTER_SHIFT 9
#define REGISTER_1_SHIFT 6

#define GET_OP_CODE(instruction) (instruction >> OP_CODE_SHIFT & 0xF)
#define IS_ONE(number, bit_position) (number >> bit_position & 1) == 1
#define ISOLATE_FIRST_FIVE_BITS(instruction) (instruction & 0x1F)

#define DESTINATION_REGISTER(instruction) (instruction >> DESTINATION_REGISTER_SHIFT & 0x7)
#define SOURCE_REGISTER_1(instruction) (instruction >> REGISTER_1_SHIFT & 0x7)
#define SOURCE_REGISTER_2(instruction) (instruction & 0x7)

#define IMMEDIATE(number) ISOLATE_FIRST_FIVE_BITS(number)
// Sign extend immediate (coz the other name is too long)
#define SEXTIMM(number) sext(IMMEDIATE(number), IMM_SHIFT)

#define HALT 0x5000

enum registers {
    R0,
    R1,
    R2,
    R3,
    R4,
    R5,
    R6,
    R7,
    RPC, // Program Counter
    RCNT, // Count register
};

const char *op_names[OP_NUMBER] = {
        "add",
        "dec",
        "and",
        "xor",
        "load",
        "halt",
};

typedef uint16_t word_size;

typedef word_size (*instruction_handler)(word_size instruction);

word_size PROGRAM_COUNTER_START = 0x0;
word_size memory[UINT16_MAX + 1] = {0};
word_size registers_mem[RCNT] = {0};
bool running = true;

word_size read_memory(const word_size address) {
    return memory[address];
}

void write_memory(const word_size address, const word_size value) {
    memory[address] = value;
}

word_size sext(word_size value, const int bit_position) {
    const uint8_t shift = bit_position - 1;
    const word_size most_significant_bit = value >> shift;
    const bool is_negative = IS_ONE(most_significant_bit, 0);

    if (is_negative) {
        value |= 0xFF << bit_position;
    }

    return value;
}

word_size add(const word_size instruction) {
    const word_size source_register_1 = SOURCE_REGISTER_1(instruction);
    const word_size source_register_2 = SOURCE_REGISTER_2(instruction);
    const word_size first_operand = registers_mem[source_register_1];
    const word_size second_operand = registers_mem[source_register_2];
    const word_size sum = first_operand + second_operand;

    printf("First operand: r%u -- %d\n", source_register_1, first_operand);
    printf("Second operand: r%u -- %d\n", source_register_2, second_operand);
    printf("Sum: %d\n", sum);

    registers_mem[DESTINATION_REGISTER(instruction)] = sum;

    return sum;
}

word_size decrement(const word_size instruction) {
    const word_size source_register = SOURCE_REGISTER_1(instruction);
    const word_size first_operand = registers_mem[source_register];
    const word_size immediate = SEXTIMM(instruction);
    const word_size difference = first_operand - immediate;

    printf("First operand: r%u -- %d\n", source_register, first_operand);
    printf("Immediate: %d\n", immediate);
    printf("Difference: %d\n", difference);

    registers_mem[DESTINATION_REGISTER(instruction)] = difference;

    return difference;
}

word_size and(const word_size instruction) {
    const word_size source_register_1 = SOURCE_REGISTER_1(instruction);
    const word_size source_register_2 = SOURCE_REGISTER_2(instruction);
    const word_size first_operand = registers_mem[source_register_1];
    const word_size second_operand = registers_mem[source_register_2];
    const word_size result = first_operand & second_operand;

    printf("First operand: r%u -- %d\n", source_register_1, first_operand);
    printf("Second operand: r%u -- %d\n", source_register_2, second_operand);
    printf("And: %d\n", result);

    registers_mem[DESTINATION_REGISTER(instruction)] = result;

    return result;
}

word_size xor(const word_size instruction) {
    const word_size source_register_1 = SOURCE_REGISTER_1(instruction);
    const word_size source_register_2 = SOURCE_REGISTER_2(instruction);
    const word_size first_operand = registers_mem[source_register_1];
    const word_size second_operand = registers_mem[source_register_2];
    const word_size result = first_operand ^ second_operand;

    printf("First operand: r%u -- %d\n", source_register_1, first_operand);
    printf("Second operand: r%u -- %d\n", source_register_2, second_operand);
    printf("Xor: %d\n", result);

    registers_mem[DESTINATION_REGISTER(instruction)] = result;

    return result;
}

word_size load_to_register(const word_size instruction) {
    const word_size immediate = SEXTIMM(instruction);
    const word_size destination_register = DESTINATION_REGISTER(instruction);

    registers_mem[destination_register] = immediate;

    return immediate;
}

word_size halt(const word_size instruction) {
    running = false;

    printf("Halted\n");

    return GET_OP_CODE(instruction);
}

instruction_handler instruction_handlers[OP_NUMBER] = {
        add,
        decrement,
        and,
        xor,
        load_to_register,
        halt
};

void execute_instruction(const word_size instruction) {
    const word_size op_code = GET_OP_CODE(instruction);

    printf("Opcode: %u -- %s\n", op_code, op_names[op_code]);

    if (op_code < OP_NUMBER) {
        instruction_handlers[op_code](instruction);

        printf("Result: %d\n", registers_mem[DESTINATION_REGISTER(instruction)]);
    } else {
        printf("Unknown opcode: %u\n", op_code);
    }
}

void read_and_execute(const char *filename) {
    FILE *file = fopen(filename, "rb");

    if (!file) {
        perror("Error opening binary file");

        return;
    }

    word_size instruction;

    fseek(file, 0, SEEK_END);

    const unsigned int file_size = ftell(file);
    const unsigned int items_count = file_size / sizeof(instruction);

    fseek(file, 0, SEEK_SET);

    fread(memory, sizeof(instruction), items_count, file);

    if (memory[items_count - 1] != HALT) {
        memory[items_count] = HALT;
    }

    registers_mem[RPC] = PROGRAM_COUNTER_START;

    while (running) {
        instruction = read_memory(registers_mem[RPC]++);

        printf("Instruction: %04X\n", instruction);

        execute_instruction(instruction);

        printf("----------------\n");
    }

    fclose(file);
}

void print_registers() {
    for (int i = 0; i < RCNT; i++) {
        printf("Register %d: %d\n", i, (int16_t) registers_mem[i]);
    }
}

void clear_memory() {
    for (int i = 0; i < UINT16_MAX + 1; i++) {
        memory[i] = 0;
    }
}

void clear_registers() {
    for (int i = 0; i < RCNT; i++) {
        registers_mem[i] = 0;
    }
}

void clear() {
    clear_memory();
    clear_registers();
    running = true;
}

int main() {
    const char *binary_files[] = {
            "/home/mykyta/uni/crossplatform-programming/vm/binary_files/sc_add.bin",
            "/home/mykyta/uni/crossplatform-programming/vm/binary_files/sc_and.bin",
            "/home/mykyta/uni/crossplatform-programming/vm/binary_files/sc_dec.bin",
            "/home/mykyta/uni/crossplatform-programming/vm/binary_files/sc_xor.bin",
            "/home/mykyta/uni/crossplatform-programming/vm/binary_files/complex.bin",
    };

    for (int i = 0; i < 5; i++) {
        printf("***************\n");
        printf("Binary file: %s\n", binary_files[i]);

        read_and_execute(binary_files[i]);

        print_registers();
        clear();
    }

    return 0;
}

#define _CRT_SECURE_NO_WARNINGS
#define SDL_MAIN_HANDLED
#include <SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

// Define CPU state
typedef struct {
    uint8_t A;  // Accumulator
    uint8_t X;  // X Register
    uint8_t Y;  // Y Register
    uint8_t SP; // Stack Pointer
    uint16_t PC; // Program Counter
    uint8_t P;  // Status Register
    uint8_t mem[65536]; // 64KB RAM
} CPU;

// Status Register Flags
#define FLAG_N 0x80 // Negative
#define FLAG_V 0x40 // Overflow
#define FLAG_B 0x10 // Break
#define FLAG_D 0x08 // Decimal
#define FLAG_I 0x04 // Interrupt
#define FLAG_Z 0x02 // Zero
#define FLAG_C 0x01 // Carry

// Function prototypes
void reset(CPU* cpu);
void load_rom(CPU* cpu, const char* filename, uint16_t address);
void dump_memory(CPU* cpu, uint16_t start, uint16_t end);
void dump_registers(CPU* cpu);
uint8_t fetch_byte(CPU* cpu);
uint16_t fetch_word(CPU* cpu);
void push_byte(CPU* cpu, uint8_t val);
uint8_t pull_byte(CPU* cpu);
void set_zero_and_negative_flags(CPU* cpu, uint8_t value);
void branch(CPU* cpu, int8_t offset);
uint16_t get_address(CPU* cpu, uint8_t mode);
void execute_instruction(CPU* cpu);
void handle_interrupt(CPU* cpu, uint16_t vector);


// Addressing Mode Constants
#define AM_IMM 0  // Immediate
#define AM_ZP 1   // Zero Page
#define AM_ZPX 2  // Zero Page,X
#define AM_ZPY 3  // Zero Page,Y
#define AM_IZX 4  // (Zero Page,X)
#define AM_IZY 5  // (Zero Page),Y
#define AM_ABS 6  // Absolute
#define AM_ABX 7  // Absolute,X
#define AM_ABY 8  // Absolute,Y
#define AM_IND 9  // Indirect
#define AM_REL 10  // Relative
#define AM_IMP 11 // Implied

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 128

SDL_Window* window = NULL;
SDL_Renderer* renderer = NULL;
SDL_Texture* texture = NULL;
uint32_t pixels[SCREEN_WIDTH * SCREEN_HEIGHT];

uint8_t keyboard_input = 0;

//Color palette
uint32_t palette[16] = {
   0xff000000,  // $0: Black
   0xffffffff,  // $1: White
   0xffff0000,  // $2: Red
   0xff00ffff,  // $3: Cyan
   0xffff00ff,  // $4: Purple
   0xff00ff00,  // $5: Green
   0xff0000ff,  // $6: Blue
   0xffffff00,  // $7: Yellow
   0xffffa500,  // $8: Orange
   0xffa52a2a,  // $9: Brown
   0xffff69b4,  // $a: Light red (Pink)
   0xff696969,  // $b: Dark grey
   0xff808080,  // $c: Grey
   0xff90ee90,  // $d: Light green
   0xffadd8e6,  // $e: Light blue
   0xffd3d3d3   // $f: Light grey
};

int init_sdl() {
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        fprintf(stderr, "SDL could not initialize! SDL_Error: %s\n", SDL_GetError());
        return 1;
    }

    window = SDL_CreateWindow("6502 Emulator", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, SCREEN_WIDTH * 2, SCREEN_HEIGHT * 2, SDL_WINDOW_SHOWN);
    if (window == NULL) {
        fprintf(stderr, "Window could not be created! SDL_Error: %s\n", SDL_GetError());
        return 1;
    }

    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    if (renderer == NULL) {
        fprintf(stderr, "Renderer could not be created! SDL_Error: %s\n", SDL_GetError());
        return 1;
    }
    texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, SCREEN_WIDTH, SCREEN_HEIGHT);
    if (texture == NULL)
    {
        fprintf(stderr, "Texture could not be created! SDL_Error: %s\n", SDL_GetError());
        return 1;
    }

    memset(pixels, 0, sizeof(pixels));

    return 0;
}

void render_screen()
{
    SDL_UpdateTexture(texture, NULL, pixels, SCREEN_WIDTH * sizeof(uint32_t));
    SDL_RenderClear(renderer);
    SDL_RenderCopy(renderer, texture, NULL, NULL);
    SDL_RenderPresent(renderer);
}

int main() {
    CPU cpu;
    char rom_filename[256];
    uint16_t rom_load_address = 0x8000; // Default ROM load address

    if (init_sdl() != 0)
    {
        return 1;
    }

    reset(&cpu); // Initialize the CPU

    printf("Enter ROM filename: ");
    scanf("%s", rom_filename);

    // Load the rom
    load_rom(&cpu, rom_filename, rom_load_address);
    cpu.PC = rom_load_address;

    // Execution loop
    SDL_Event event;
    int cycles = 0;
    while (cycles < 100000)
    {
        if (SDL_PollEvent(&event))
        {
            if (event.type == SDL_QUIT)
            {
                cycles = 1000000;
                break;
            }
            else if (event.type == SDL_KEYDOWN) // Handle key press
            {
                keyboard_input = event.key.keysym.sym & 0xFF;
            }
            else if (event.type == SDL_KEYUP)
            {
                keyboard_input = 0;
            }
        }
        execute_instruction(&cpu);
        cycles++;
        render_screen();
        if (cpu.PC == 0xFFFF)
            break;
    }

    printf("\n--- CPU State ---\n");
    dump_registers(&cpu);
    printf("\n--- Memory Dump ---\n");
    dump_memory(&cpu, rom_load_address - 10, rom_load_address + 100);

    SDL_DestroyTexture(texture);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}

void reset(CPU* cpu) {
    memset(cpu, 0, sizeof(CPU));
    cpu->SP = 0xFF;
    // Set the unused bit in status reg
    cpu->P |= 0x20;

    // Load the reset vector
    cpu->PC = cpu->mem[0xFFFC] | (cpu->mem[0xFFFD] << 8);

    // Seed the random number generator
    srand(time(NULL));

    keyboard_input = 0;
}

void load_rom(CPU* cpu, const char* filename, uint16_t address) {
    FILE* fp = fopen(filename, "rb");
    if (!fp) {
        perror("Error opening ROM file");
        exit(EXIT_FAILURE);
    }

    fseek(fp, 0, SEEK_END);
    long rom_size = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    if (rom_size > 65536 - address) {
        fprintf(stderr, "Error: ROM too large to fit in memory.\n");
        fclose(fp);
        exit(EXIT_FAILURE);
    }

    fread(&cpu->mem[address], 1, rom_size, fp);
    fclose(fp);
    printf("Loaded ROM '%s' into memory at $%04X. Size %ld bytes\n", filename, address, rom_size);
}

void dump_memory(CPU* cpu, uint16_t start, uint16_t end) {
    for (uint16_t i = start; i <= end; ++i) {
        printf("$%04X: %02X ", i, cpu->mem[i]);
        if ((i - start + 1) % 8 == 0) {
            printf("\n");
        }
    }
    printf("\n");
}


void dump_registers(CPU* cpu) {
    printf("A:  $%02X\n", cpu->A);
    printf("X:  $%02X\n", cpu->X);
    printf("Y:  $%02X\n", cpu->Y);
    printf("SP: $%02X\n", cpu->SP);
    printf("PC: $%04X\n", cpu->PC);
    printf("P:  $%02X (N=%d, V=%d, B=%d, D=%d, I=%d, Z=%d, C=%d)\n", cpu->P,
        (cpu->P & FLAG_N) ? 1 : 0,
        (cpu->P & FLAG_V) ? 1 : 0,
        (cpu->P & FLAG_B) ? 1 : 0,
        (cpu->P & FLAG_D) ? 1 : 0,
        (cpu->P & FLAG_I) ? 1 : 0,
        (cpu->P & FLAG_Z) ? 1 : 0,
        (cpu->P & FLAG_C) ? 1 : 0);
}


uint8_t fetch_byte(CPU* cpu) {
    uint16_t address = cpu->PC;
    cpu->PC++;

    if (address == 0x00FE) {
        // If the address is $00FE (random number generator):
        return rand() & 0xff;
    }
    else if (address == 0x00FF) {
        // If the address is $00FF (keyboard input):
        return keyboard_input;
    }
    else {
        // For all other addresses
        return cpu->mem[address];
    }
}

uint16_t fetch_word(CPU* cpu) {
    uint16_t low = cpu->mem[cpu->PC++];
    uint16_t high = cpu->mem[cpu->PC++];
    return (high << 8) | low;
}

void push_byte(CPU* cpu, uint8_t val)
{
    cpu->mem[0x100 + cpu->SP] = val;
    cpu->SP--;
}

uint8_t pull_byte(CPU* cpu)
{
    cpu->SP++;
    return cpu->mem[0x100 + cpu->SP];
}

void set_zero_and_negative_flags(CPU* cpu, uint8_t value)
{
    cpu->P &= ~(FLAG_N | FLAG_Z);
    if (value == 0)
        cpu->P |= FLAG_Z;
    if (value & 0x80)
        cpu->P |= FLAG_N;
}

void branch(CPU* cpu, int8_t offset)
{
    cpu->PC += offset;
}

uint16_t get_address(CPU* cpu, uint8_t mode)
{
    uint16_t address = 0;
    uint8_t zp_addr;
    uint16_t abs_addr;
    uint8_t temp_byte;

    switch (mode)
    {
    case AM_IMM:
        address = cpu->PC++;
        break;
    case AM_ZP:
        zp_addr = fetch_byte(cpu);
        address = zp_addr;
        break;
    case AM_ZPX:
        zp_addr = fetch_byte(cpu);
        address = (zp_addr + cpu->X) & 0xFF;
        break;
    case AM_ZPY:
        zp_addr = fetch_byte(cpu);
        address = (zp_addr + cpu->Y) & 0xFF;
        break;
    case AM_IZX:
        zp_addr = fetch_byte(cpu);
        temp_byte = (zp_addr + cpu->X) & 0xFF;
        address = cpu->mem[temp_byte] | (cpu->mem[(temp_byte + 1) & 0xFF] << 8);
        break;
    case AM_IZY:
        zp_addr = fetch_byte(cpu);
        address = cpu->mem[zp_addr] | (cpu->mem[(zp_addr + 1) & 0xFF] << 8);
        address += cpu->Y;
        break;
    case AM_ABS:
        address = fetch_word(cpu);
        break;
    case AM_ABX:
        address = fetch_word(cpu);
        address += cpu->X;
        break;
    case AM_ABY:
        address = fetch_word(cpu);
        address += cpu->Y;
        break;
    case AM_IND:
        abs_addr = fetch_word(cpu);
        address = cpu->mem[abs_addr] | (cpu->mem[(abs_addr & 0xFF00) | ((abs_addr + 1) & 0xFF)] << 8);
        break;
    case AM_REL:
        address = cpu->PC + (int8_t)fetch_byte(cpu);
        break;

    default:
        break;
    }
    return address;
}

void execute_instruction(CPU* cpu) {
    uint8_t opcode = fetch_byte(cpu);
    uint16_t address = 0;
    uint8_t value = 0;
    uint16_t temp_word = 0;
    uint8_t temp_byte = 0;
    uint8_t temp_carry = 0;

    switch (opcode)
    {
        // --- 0x ---
    case 0x00: // BRK
        push_byte(cpu, cpu->PC >> 8);
        push_byte(cpu, cpu->PC & 0xFF);
        push_byte(cpu, cpu->P | FLAG_B);
        cpu->P |= FLAG_I; // Set interrupt flag
        cpu->PC = (cpu->mem[0xFFFE] | (cpu->mem[0xFFFF] << 8)); // Load interrupt vector
        break;
    case 0x01: // ORA izx
        address = get_address(cpu, AM_IZX);
        cpu->A |= cpu->mem[address];
        set_zero_and_negative_flags(cpu, cpu->A);
        break;
    case 0x02: // KIL
    case 0x12:
    case 0x22:
    case 0x32:
    case 0x42:
    case 0x52:
    case 0x62:
    case 0x72:
    case 0x92:
    case 0xB2:
    case 0xD2:
    case 0xF2:
        printf("KIL Instruction executed, halting.\n");
        cpu->PC = 0xFFFF;
        break;
    case 0x03: // SLO izx
        address = get_address(cpu, AM_IZX);
        value = cpu->mem[address];
        cpu->P &= ~FLAG_C;
        if (value & 0x80)
            cpu->P |= FLAG_C;
        cpu->mem[address] = value << 1;
        cpu->A |= cpu->mem[address];
        set_zero_and_negative_flags(cpu, cpu->A);
        break;
    case 0x04: // NOP zp
        get_address(cpu, AM_ZP);
        break;
    case 0x05: // ORA zp
        address = get_address(cpu, AM_ZP);
        cpu->A |= cpu->mem[address];
        set_zero_and_negative_flags(cpu, cpu->A);
        break;
    case 0x06: // ASL zp
        address = get_address(cpu, AM_ZP);
        value = cpu->mem[address];
        cpu->P &= ~FLAG_C;
        if (value & 0x80)
            cpu->P |= FLAG_C;
        cpu->mem[address] = value << 1;
        set_zero_and_negative_flags(cpu, cpu->mem[address]);
        break;
    case 0x07: // SLO zp
        address = get_address(cpu, AM_ZP);
        value = cpu->mem[address];
        cpu->P &= ~FLAG_C;
        if (value & 0x80)
            cpu->P |= FLAG_C;
        cpu->mem[address] = value << 1;
        cpu->A |= cpu->mem[address];
        set_zero_and_negative_flags(cpu, cpu->A);
        break;
    case 0x08: // PHP
        push_byte(cpu, cpu->P);
        break;
    case 0x09: // ORA imm
        address = get_address(cpu, AM_IMM);
        cpu->A |= cpu->mem[address];
        set_zero_and_negative_flags(cpu, cpu->A);
        break;
    case 0x0A: // ASL
        cpu->P &= ~FLAG_C;
        if (cpu->A & 0x80)
            cpu->P |= FLAG_C;
        cpu->A = cpu->A << 1;
        set_zero_and_negative_flags(cpu, cpu->A);
        break;
    case 0x0B: // ANC imm
        address = get_address(cpu, AM_IMM);
        cpu->A &= cpu->mem[address];
        if (cpu->A & 0x80)
            cpu->P |= FLAG_C;
        else
            cpu->P &= ~FLAG_C;
        set_zero_and_negative_flags(cpu, cpu->A);

        break;
    case 0x0C: // NOP abs
        get_address(cpu, AM_ABS);
        break;
    case 0x0D: // ORA abs
        address = get_address(cpu, AM_ABS);
        cpu->A |= cpu->mem[address];
        set_zero_and_negative_flags(cpu, cpu->A);
        break;
    case 0x0E: // ASL abs
        address = get_address(cpu, AM_ABS);
        value = cpu->mem[address];
        cpu->P &= ~FLAG_C;
        if (value & 0x80)
            cpu->P |= FLAG_C;
        cpu->mem[address] = value << 1;
        set_zero_and_negative_flags(cpu, cpu->mem[address]);
        break;
    case 0x0F: // SLO abs
        address = get_address(cpu, AM_ABS);
        value = cpu->mem[address];
        cpu->P &= ~FLAG_C;
        if (value & 0x80)
            cpu->P |= FLAG_C;
        cpu->mem[address] = value << 1;
        cpu->A |= cpu->mem[address];
        set_zero_and_negative_flags(cpu, cpu->A);
        break;
        // --- 1x ---
    case 0x10: // BPL rel
        address = get_address(cpu, AM_REL);
        if (!(cpu->P & FLAG_N))
            cpu->PC = address;
        break;
    case 0x11: // ORA izy
        address = get_address(cpu, AM_IZY);
        cpu->A |= cpu->mem[address];
        set_zero_and_negative_flags(cpu, cpu->A);
        break;
    case 0x13: // SLO izy
        address = get_address(cpu, AM_IZY);
        value = cpu->mem[address];
        cpu->P &= ~FLAG_C;
        if (value & 0x80)
            cpu->P |= FLAG_C;
        cpu->mem[address] = value << 1;
        cpu->A |= cpu->mem[address];
        set_zero_and_negative_flags(cpu, cpu->A);
        break;
    case 0x14: // NOP zpx
        get_address(cpu, AM_ZPX);
        break;
    case 0x15: // ORA zpx
        address = get_address(cpu, AM_ZPX);
        cpu->A |= cpu->mem[address];
        set_zero_and_negative_flags(cpu, cpu->A);
        break;
    case 0x16: // ASL zpx
        address = get_address(cpu, AM_ZPX);
        value = cpu->mem[address];
        cpu->P &= ~FLAG_C;
        if (value & 0x80)
            cpu->P |= FLAG_C;
        cpu->mem[address] = value << 1;
        set_zero_and_negative_flags(cpu, cpu->mem[address]);
        break;
    case 0x17: // SLO zpx
        address = get_address(cpu, AM_ZPX);
        value = cpu->mem[address];
        cpu->P &= ~FLAG_C;
        if (value & 0x80)
            cpu->P |= FLAG_C;
        cpu->mem[address] = value << 1;
        cpu->A |= cpu->mem[address];
        set_zero_and_negative_flags(cpu, cpu->A);
        break;
    case 0x18: // CLC
        cpu->P &= ~FLAG_C;
        break;
    case 0x19: // ORA aby
        address = get_address(cpu, AM_ABY);
        cpu->A |= cpu->mem[address];
        set_zero_and_negative_flags(cpu, cpu->A);
        break;
    case 0x1A: // NOP
        break;
    case 0x1B: // SLO aby
        address = get_address(cpu, AM_ABY);
        value = cpu->mem[address];
        cpu->P &= ~FLAG_C;
        if (value & 0x80)
            cpu->P |= FLAG_C;
        cpu->mem[address] = value << 1;
        cpu->A |= cpu->mem[address];
        set_zero_and_negative_flags(cpu, cpu->A);
        break;
    case 0x1C: // NOP abx
        get_address(cpu, AM_ABX);
        break;
    case 0x1D: // ORA abx
        address = get_address(cpu, AM_ABX);
        cpu->A |= cpu->mem[address];
        set_zero_and_negative_flags(cpu, cpu->A);
        break;
    case 0x1E: // ASL abx
        address = get_address(cpu, AM_ABX);
        value = cpu->mem[address];
        cpu->P &= ~FLAG_C;
        if (value & 0x80)
            cpu->P |= FLAG_C;
        cpu->mem[address] = value << 1;
        set_zero_and_negative_flags(cpu, cpu->mem[address]);
        break;
    case 0x1F: // SLO abx
        address = get_address(cpu, AM_ABX);
        value = cpu->mem[address];
        cpu->P &= ~FLAG_C;
        if (value & 0x80)
            cpu->P |= FLAG_C;
        cpu->mem[address] = value << 1;
        cpu->A |= cpu->mem[address];
        set_zero_and_negative_flags(cpu, cpu->A);
        break;

        // --- 2x ---
    case 0x20: // JSR abs
        push_byte(cpu, cpu->PC >> 8);
        push_byte(cpu, cpu->PC & 0xFF);
        cpu->PC = get_address(cpu, AM_ABS);
        break;
    case 0x21: // AND izx
        address = get_address(cpu, AM_IZX);
        cpu->A &= cpu->mem[address];
        set_zero_and_negative_flags(cpu, cpu->A);
        break;
    case 0x23: // RLA izx
        address = get_address(cpu, AM_IZX);
        value = cpu->mem[address];
        temp_carry = (cpu->P & FLAG_C) ? 1 : 0;
        cpu->P &= ~FLAG_C;
        if (value & 0x80)
            cpu->P |= FLAG_C;
        cpu->mem[address] = (value << 1) | temp_carry;
        cpu->A &= cpu->mem[address];
        set_zero_and_negative_flags(cpu, cpu->A);
        break;
    case 0x24: // BIT zp
        address = get_address(cpu, AM_ZP);
        value = cpu->mem[address];
        cpu->P &= ~(FLAG_N | FLAG_V | FLAG_Z);
        if (value & FLAG_N)
            cpu->P |= FLAG_N;
        if (value & FLAG_V)
            cpu->P |= FLAG_V;
        if (!(cpu->A & value))
            cpu->P |= FLAG_Z;
        break;
    case 0x25: // AND zp
        address = get_address(cpu, AM_ZP);
        cpu->A &= cpu->mem[address];
        set_zero_and_negative_flags(cpu, cpu->A);
        break;
    case 0x26: // ROL zp
        address = get_address(cpu, AM_ZP);
        value = cpu->mem[address];
        temp_carry = (cpu->P & FLAG_C) ? 1 : 0;
        cpu->P &= ~FLAG_C;
        if (value & 0x80)
            cpu->P |= FLAG_C;
        cpu->mem[address] = (value << 1) | temp_carry;
        set_zero_and_negative_flags(cpu, cpu->mem[address]);
        break;
    case 0x27: // RLA zp
        address = get_address(cpu, AM_ZP);
        value = cpu->mem[address];
        temp_carry = (cpu->P & FLAG_C) ? 1 : 0;
        cpu->P &= ~FLAG_C;
        if (value & 0x80)
            cpu->P |= FLAG_C;
        cpu->mem[address] = (value << 1) | temp_carry;
        cpu->A &= cpu->mem[address];
        set_zero_and_negative_flags(cpu, cpu->A);
        break;
    case 0x28: // PLP
        cpu->P = pull_byte(cpu);
        cpu->P |= 0x20; // Ensure B flag is always 1
        break;
    case 0x29: // AND imm
        address = get_address(cpu, AM_IMM);
        cpu->A &= cpu->mem[address];
        set_zero_and_negative_flags(cpu, cpu->A);
        break;
    case 0x2A: // ROL
        temp_carry = (cpu->P & FLAG_C) ? 1 : 0;
        cpu->P &= ~FLAG_C;
        if (cpu->A & 0x80)
            cpu->P |= FLAG_C;
        cpu->A = (cpu->A << 1) | temp_carry;
        set_zero_and_negative_flags(cpu, cpu->A);
        break;
    case 0x2B: // ANC imm
        address = get_address(cpu, AM_IMM);
        cpu->A &= cpu->mem[address];
        if (cpu->A & 0x80)
            cpu->P |= FLAG_C;
        else
            cpu->P &= ~FLAG_C;
        set_zero_and_negative_flags(cpu, cpu->A);

        break;
    case 0x2C: // BIT abs
        address = get_address(cpu, AM_ABS);
        value = cpu->mem[address];
        cpu->P &= ~(FLAG_N | FLAG_V | FLAG_Z);
        if (value & FLAG_N)
            cpu->P |= FLAG_N;
        if (value & FLAG_V)
            cpu->P |= FLAG_V;
        if (!(cpu->A & value))
            cpu->P |= FLAG_Z;
        break;
    case 0x2D: // AND abs
        address = get_address(cpu, AM_ABS);
        cpu->A &= cpu->mem[address];
        set_zero_and_negative_flags(cpu, cpu->A);
        break;
    case 0x2E: // ROL abs
        address = get_address(cpu, AM_ABS);
        value = cpu->mem[address];
        temp_carry = (cpu->P & FLAG_C) ? 1 : 0;
        cpu->P &= ~FLAG_C;
        if (value & 0x80)
            cpu->P |= FLAG_C;
        cpu->mem[address] = (value << 1) | temp_carry;
        set_zero_and_negative_flags(cpu, cpu->mem[address]);
        break;
    case 0x2F: // RLA abs
        address = get_address(cpu, AM_ABS);
        value = cpu->mem[address];
        temp_carry = (cpu->P & FLAG_C) ? 1 : 0;
        cpu->P &= ~FLAG_C;
        if (value & 0x80)
            cpu->P |= FLAG_C;
        cpu->mem[address] = (value << 1) | temp_carry;
        cpu->A &= cpu->mem[address];
        set_zero_and_negative_flags(cpu, cpu->A);
        break;

        // --- 3x ---
    case 0x30: // BMI rel
        address = get_address(cpu, AM_REL);
        if (cpu->P & FLAG_N)
            cpu->PC = address;
        break;
    case 0x31: // AND izy
        address = get_address(cpu, AM_IZY);
        cpu->A &= cpu->mem[address];
        set_zero_and_negative_flags(cpu, cpu->A);
        break;
    case 0x33: // RLA izy
        address = get_address(cpu, AM_IZY);
        value = cpu->mem[address];
        temp_carry = (cpu->P & FLAG_C) ? 1 : 0;
        cpu->P &= ~FLAG_C;
        if (value & 0x80)
            cpu->P |= FLAG_C;
        cpu->mem[address] = (value << 1) | temp_carry;
        cpu->A &= cpu->mem[address];
        set_zero_and_negative_flags(cpu, cpu->A);
        break;
    case 0x34: // NOP zpx
        get_address(cpu, AM_ZPX);
        break;
    case 0x35: // AND zpx
        address = get_address(cpu, AM_ZPX);
        cpu->A &= cpu->mem[address];
        set_zero_and_negative_flags(cpu, cpu->A);
        break;
    case 0x36: // ROL zpx
        address = get_address(cpu, AM_ZPX);
        value = cpu->mem[address];
        temp_carry = (cpu->P & FLAG_C) ? 1 : 0;
        cpu->P &= ~FLAG_C;
        if (value & 0x80)
            cpu->P |= FLAG_C;
        cpu->mem[address] = (value << 1) | temp_carry;
        set_zero_and_negative_flags(cpu, cpu->mem[address]);
        break;
    case 0x37: // RLA zpx
        address = get_address(cpu, AM_ZPX);
        value = cpu->mem[address];
        temp_carry = (cpu->P & FLAG_C) ? 1 : 0;
        cpu->P &= ~FLAG_C;
        if (value & 0x80)
            cpu->P |= FLAG_C;
        cpu->mem[address] = (value << 1) | temp_carry;
        cpu->A &= cpu->mem[address];
        set_zero_and_negative_flags(cpu, cpu->A);
        break;
    case 0x38: // SEC
        cpu->P |= FLAG_C;
        break;
    case 0x39: // AND aby
        address = get_address(cpu, AM_ABY);
        cpu->A &= cpu->mem[address];
        set_zero_and_negative_flags(cpu, cpu->A);
        break;
    case 0x3A: // NOP
        break;
    case 0x3B: // RLA aby
        address = get_address(cpu, AM_ABY);
        value = cpu->mem[address];
        temp_carry = (cpu->P & FLAG_C) ? 1 : 0;
        cpu->P &= ~FLAG_C;
        if (value & 0x80)
            cpu->P |= FLAG_C;
        cpu->mem[address] = (value << 1) | temp_carry;
        cpu->A &= cpu->mem[address];
        set_zero_and_negative_flags(cpu, cpu->A);
        break;
    case 0x3C: // NOP abx
        get_address(cpu, AM_ABX);
        break;
    case 0x3D: // AND abx
        address = get_address(cpu, AM_ABX);
        cpu->A &= cpu->mem[address];
        set_zero_and_negative_flags(cpu, cpu->A);
        break;
    case 0x3E: // ROL abx
        address = get_address(cpu, AM_ABX);
        value = cpu->mem[address];
        temp_carry = (cpu->P & FLAG_C) ? 1 : 0;
        cpu->P &= ~FLAG_C;
        if (value & 0x80)
            cpu->P |= FLAG_C;
        cpu->mem[address] = (value << 1) | temp_carry;
        set_zero_and_negative_flags(cpu, cpu->mem[address]);
        break;
    case 0x3F: // RLA abx
        address = get_address(cpu, AM_ABX);
        value = cpu->mem[address];
        temp_carry = (cpu->P & FLAG_C) ? 1 : 0;
        cpu->P &= ~FLAG_C;
        if (value & 0x80)
            cpu->P |= FLAG_C;
        cpu->mem[address] = (value << 1) | temp_carry;
        cpu->A &= cpu->mem[address];
        set_zero_and_negative_flags(cpu, cpu->A);
        break;
        // --- 4x ---
    case 0x40: // RTI
        cpu->P = pull_byte(cpu);
        cpu->P |= 0x20; // Ensure B flag is always 1
        cpu->PC = pull_byte(cpu);
        cpu->PC |= pull_byte(cpu) << 8;
        break;
    case 0x41: // EOR izx
        address = get_address(cpu, AM_IZX);
        cpu->A ^= cpu->mem[address];
        set_zero_and_negative_flags(cpu, cpu->A);
        break;
    case 0x43: // SRE izx
        address = get_address(cpu, AM_IZX);
        value = cpu->mem[address];
        cpu->P &= ~FLAG_C;
        if (value & 0x01)
            cpu->P |= FLAG_C;
        cpu->mem[address] = value >> 1;
        cpu->A ^= cpu->mem[address];
        set_zero_and_negative_flags(cpu, cpu->A);
        break;
    case 0x44: // NOP zp
        get_address(cpu, AM_ZP);
        break;
    case 0x45: // EOR zp
        address = get_address(cpu, AM_ZP);
        cpu->A ^= cpu->mem[address];
        set_zero_and_negative_flags(cpu, cpu->A);
        break;
    case 0x46: // LSR zp
        address = get_address(cpu, AM_ZP);
        uint8_t value = cpu->mem[address];
        cpu->P &= ~FLAG_C;
        if (value & 0x01)
            cpu->P |= FLAG_C;
        cpu->mem[address] = value >> 1;
        set_zero_and_negative_flags(cpu, cpu->mem[address]);
        break;
    case 0x47: // SRE zp
        address = get_address(cpu, AM_ZP);
        value = cpu->mem[address];
        cpu->P &= ~FLAG_C;
        if (value & 0x01)
            cpu->P |= FLAG_C;
        cpu->mem[address] = value >> 1;
        cpu->A ^= cpu->mem[address];
        set_zero_and_negative_flags(cpu, cpu->A);
        break;
    case 0x48: // PHA
        push_byte(cpu, cpu->A);
        break;
    case 0x49: // EOR imm
        address = get_address(cpu, AM_IMM);
        cpu->A ^= cpu->mem[address];
        set_zero_and_negative_flags(cpu, cpu->A);
        break;
    case 0x4A: // LSR
        cpu->P &= ~FLAG_C;
        if (cpu->A & 0x01)
            cpu->P |= FLAG_C;
        cpu->A = cpu->A >> 1;
        set_zero_and_negative_flags(cpu, cpu->A);
        break;
    case 0x4B: // ALR imm
        address = get_address(cpu, AM_IMM);
        cpu->A &= cpu->mem[address];
        cpu->P &= ~FLAG_C;
        if (cpu->A & 0x01)
            cpu->P |= FLAG_C;
        cpu->A = cpu->A >> 1;
        set_zero_and_negative_flags(cpu, cpu->A);
        break;
    case 0x4C: // JMP abs
        cpu->PC = get_address(cpu, AM_ABS);
        break;
    case 0x4D: // EOR abs
        address = get_address(cpu, AM_ABS);
        cpu->A ^= cpu->mem[address];
        set_zero_and_negative_flags(cpu, cpu->A);
        break;
    case 0x4E: // LSR abs
        address = get_address(cpu, AM_ABS);
        value = cpu->mem[address];
        cpu->P &= ~FLAG_C;
        if (value & 0x01)
            cpu->P |= FLAG_C;
        cpu->mem[address] = value >> 1;
        set_zero_and_negative_flags(cpu, cpu->mem[address]);
        break;
    case 0x4F: // SRE abs
        address = get_address(cpu, AM_ABS);
        value = cpu->mem[address];
        cpu->P &= ~FLAG_C;
        if (value & 0x01)
            cpu->P |= FLAG_C;
        cpu->mem[address] = value >> 1;
        cpu->A ^= cpu->mem[address];
        set_zero_and_negative_flags(cpu, cpu->A);
        break;
        // --- 5x ---
    case 0x50: // BVC rel
        address = get_address(cpu, AM_REL);
        if (!(cpu->P & FLAG_V))
            cpu->PC = address;
        break;
    case 0x51: // EOR izy
        address = get_address(cpu, AM_IZY);
        cpu->A ^= cpu->mem[address];
        set_zero_and_negative_flags(cpu, cpu->A);
        break;
    case 0x53: // SRE izy
        address = get_address(cpu, AM_IZY);
        value = cpu->mem[address];
        cpu->P &= ~FLAG_C;
        if (value & 0x01)
            cpu->P |= FLAG_C;
        cpu->mem[address] = value >> 1;
        cpu->A ^= cpu->mem[address];
        set_zero_and_negative_flags(cpu, cpu->A);
        break;
    case 0x54: // NOP zpx
        get_address(cpu, AM_ZPX);
        break;
    case 0x55: // EOR zpx
        address = get_address(cpu, AM_ZPX);
        cpu->A ^= cpu->mem[address];
        set_zero_and_negative_flags(cpu, cpu->A);
        break;
    case 0x56: // LSR zpx
        address = get_address(cpu, AM_ZPX);
        value = cpu->mem[address];
        cpu->P &= ~FLAG_C;
        if (value & 0x01)
            cpu->P |= FLAG_C;
        cpu->mem[address] = value >> 1;
        set_zero_and_negative_flags(cpu, cpu->mem[address]);
        break;
    case 0x57: // SRE zpx
        address = get_address(cpu, AM_ZPX);
        value = cpu->mem[address];
        cpu->P &= ~FLAG_C;
        if (value & 0x01)
            cpu->P |= FLAG_C;
        cpu->mem[address] = value >> 1;
        cpu->A ^= cpu->mem[address];
        set_zero_and_negative_flags(cpu, cpu->A);
        break;
    case 0x58: // CLI
        cpu->P &= ~FLAG_I;
        break;
    case 0x59: // EOR aby
        address = get_address(cpu, AM_ABY);
        cpu->A ^= cpu->mem[address];
        set_zero_and_negative_flags(cpu, cpu->A);
        break;
    case 0x5A: // NOP
        break;
    case 0x5B: // SRE aby
        address = get_address(cpu, AM_ABY);
        value = cpu->mem[address];
        cpu->P &= ~FLAG_C;
        if (value & 0x01)
            cpu->P |= FLAG_C;
        cpu->mem[address] = value >> 1;
        cpu->A ^= cpu->mem[address];
        set_zero_and_negative_flags(cpu, cpu->A);
        break;
    case 0x5C: // NOP abx
        get_address(cpu, AM_ABX);
        break;
    case 0x5D: // EOR abx
        address = get_address(cpu, AM_ABX);
        cpu->A ^= cpu->mem[address];
        set_zero_and_negative_flags(cpu, cpu->A);
        break;
    case 0x5E: // LSR abx
        address = get_address(cpu, AM_ABX);
        value = cpu->mem[address];
        cpu->P &= ~FLAG_C;
        if (value & 0x01)
            cpu->P |= FLAG_C;
        cpu->mem[address] = value >> 1;
        set_zero_and_negative_flags(cpu, cpu->mem[address]);
        break;
    case 0x5F: // SRE abx
        address = get_address(cpu, AM_ABX);
        value = cpu->mem[address];
        cpu->P &= ~FLAG_C;
        if (value & 0x01)
            cpu->P |= FLAG_C;
        cpu->mem[address] = value >> 1;
        cpu->A ^= cpu->mem[address];
        set_zero_and_negative_flags(cpu, cpu->A);
        break;

        // --- 6x ---
    case 0x60: // RTS
        cpu->PC = pull_byte(cpu);
        cpu->PC |= pull_byte(cpu) << 8;
        cpu->PC++; // Increment PC after returning
        break;
    case 0x61: // ADC izx
        address = get_address(cpu, AM_IZX);
        value = cpu->mem[address];
        uint16_t result = cpu->A + value + (cpu->P & FLAG_C ? 1 : 0);
        cpu->P &= ~(FLAG_N | FLAG_V | FLAG_Z | FLAG_C);
        if (result & 0x100)
            cpu->P |= FLAG_C;
        if ((cpu->A ^ result) & (value ^ result) & 0x80)
            cpu->P |= FLAG_V;
        cpu->A = result & 0xFF;
        set_zero_and_negative_flags(cpu, cpu->A);
        break;
    case 0x63: // RRA izx
        address = get_address(cpu, AM_IZX);
        value = cpu->mem[address];
        temp_carry = (cpu->P & FLAG_C) ? 1 : 0;
        cpu->P &= ~FLAG_C;
        if (value & 0x01)
            cpu->P |= FLAG_C;
        cpu->mem[address] = (value >> 1) | (temp_carry << 7);
        value = cpu->mem[address];
        result = cpu->A + value + (cpu->P & FLAG_C ? 1 : 0);
        cpu->P &= ~(FLAG_N | FLAG_V | FLAG_Z | FLAG_C);
        if (result & 0x100)
            cpu->P |= FLAG_C;
        if ((cpu->A ^ result) & (value ^ result) & 0x80)
            cpu->P |= FLAG_V;
        cpu->A = result & 0xFF;
        set_zero_and_negative_flags(cpu, cpu->A);
        break;
    case 0x64: // NOP zp
        get_address(cpu, AM_ZP);
        break;
    case 0x65: // ADC zp
        address = get_address(cpu, AM_ZP);
        value = cpu->mem[address];
        result = cpu->A + value + (cpu->P & FLAG_C ? 1 : 0);
        cpu->P &= ~(FLAG_N | FLAG_V | FLAG_Z | FLAG_C);
        if (result & 0x100)
            cpu->P |= FLAG_C;
        if ((cpu->A ^ result) & (value ^ result) & 0x80)
            cpu->P |= FLAG_V;
        cpu->A = result & 0xFF;
        set_zero_and_negative_flags(cpu, cpu->A);
        break;
    case 0x66: // ROR zp
        address = get_address(cpu, AM_ZP);
        value = cpu->mem[address];
        temp_carry = (cpu->P & FLAG_C) ? 1 : 0;
        cpu->P &= ~FLAG_C;
        if (value & 0x01)
            cpu->P |= FLAG_C;
        cpu->mem[address] = (value >> 1) | (temp_carry << 7);
        set_zero_and_negative_flags(cpu, cpu->mem[address]);
        break;
    case 0x67: // RRA zp
        address = get_address(cpu, AM_ZP);
        value = cpu->mem[address];
        temp_carry = (cpu->P & FLAG_C) ? 1 : 0;
        cpu->P &= ~FLAG_C;
        if (value & 0x01)
            cpu->P |= FLAG_C;
        cpu->mem[address] = (value >> 1) | (temp_carry << 7);
        value = cpu->mem[address];
        result = cpu->A + value + (cpu->P & FLAG_C ? 1 : 0);
        cpu->P &= ~(FLAG_N | FLAG_V | FLAG_Z | FLAG_C);
        if (result & 0x100)
            cpu->P |= FLAG_C;
        if ((cpu->A ^ result) & (value ^ result) & 0x80)
            cpu->P |= FLAG_V;
        cpu->A = result & 0xFF;
        set_zero_and_negative_flags(cpu, cpu->A);
        break;
    case 0x68: // PLA
        cpu->A = pull_byte(cpu);
        set_zero_and_negative_flags(cpu, cpu->A);
        break;
    case 0x69: // ADC imm
        address = get_address(cpu, AM_IMM);
        value = cpu->mem[address];
        result = cpu->A + value + (cpu->P & FLAG_C ? 1 : 0);
        cpu->P &= ~(FLAG_N | FLAG_V | FLAG_Z | FLAG_C);
        if (result & 0x100)
            cpu->P |= FLAG_C;
        if ((cpu->A ^ result) & (value ^ result) & 0x80)
            cpu->P |= FLAG_V;
        cpu->A = result & 0xFF;
        set_zero_and_negative_flags(cpu, cpu->A);
        break;
    case 0x6A: // ROR
        temp_carry = (cpu->P & FLAG_C) ? 1 : 0;
        cpu->P &= ~FLAG_C;
        if (cpu->A & 0x01)
            cpu->P |= FLAG_C;
        cpu->A = (cpu->A >> 1) | (temp_carry << 7);
        set_zero_and_negative_flags(cpu, cpu->A);
        break;
    case 0x6B: // ARR imm
        address = get_address(cpu, AM_IMM);
        cpu->A &= cpu->mem[address];
        temp_byte = cpu->A; // Store result of AND
        cpu->P &= ~FLAG_C;
        if ((cpu->A & 0x01))
            cpu->P |= FLAG_C;
        cpu->A = (cpu->A >> 1) | ((cpu->P & FLAG_C) << 7);
        uint8_t temp_result = (temp_byte + (temp_byte & 0x0F));
        cpu->P &= ~FLAG_V;
        if ((temp_result ^ cpu->A) & 0x40)
            cpu->P |= FLAG_V;

        set_zero_and_negative_flags(cpu, cpu->A);

        break;
    case 0x6C: // JMP ind
        cpu->PC = get_address(cpu, AM_IND);
        break;
    case 0x6D: // ADC abs
        address = get_address(cpu, AM_ABS);
        value = cpu->mem[address];
        result = cpu->A + value + (cpu->P & FLAG_C ? 1 : 0);
        cpu->P &= ~(FLAG_N | FLAG_V | FLAG_Z | FLAG_C);
        if (result & 0x100)
            cpu->P |= FLAG_C;
        if ((cpu->A ^ result) & (value ^ result) & 0x80)
            cpu->P |= FLAG_V;
        cpu->A = result & 0xFF;
        set_zero_and_negative_flags(cpu, cpu->A);
        break;
    case 0x6E: // ROR abs
        address = get_address(cpu, AM_ABS);
        value = cpu->mem[address];
        temp_carry = (cpu->P & FLAG_C) ? 1 : 0;
        cpu->P &= ~FLAG_C;
        if (value & 0x01)
            cpu->P |= FLAG_C;
        cpu->mem[address] = (value >> 1) | (temp_carry << 7);
        set_zero_and_negative_flags(cpu, cpu->mem[address]);
        break;
    case 0x6F: // RRA abs
        address = get_address(cpu, AM_ABS);
        value = cpu->mem[address];
        temp_carry = (cpu->P & FLAG_C) ? 1 : 0;
        cpu->P &= ~FLAG_C;
        if (value & 0x01)
            cpu->P |= FLAG_C;
        cpu->mem[address] = (value >> 1) | (temp_carry << 7);
        value = cpu->mem[address];
        result = cpu->A + value + (cpu->P & FLAG_C ? 1 : 0);
        cpu->P &= ~(FLAG_N | FLAG_V | FLAG_Z | FLAG_C);
        if (result & 0x100)
            cpu->P |= FLAG_C;
        if ((cpu->A ^ result) & (value ^ result) & 0x80)
            cpu->P |= FLAG_V;
        cpu->A = result & 0xFF;
        set_zero_and_negative_flags(cpu, cpu->A);
        break;
        // --- 7x ---
    case 0x70: // BVS rel
        address = get_address(cpu, AM_REL);
        if (cpu->P & FLAG_V)
            cpu->PC = address;
        break;
    case 0x71: // ADC izy
        address = get_address(cpu, AM_IZY);
        value = cpu->mem[address];
        result = cpu->A + value + (cpu->P & FLAG_C ? 1 : 0);
        cpu->P &= ~(FLAG_N | FLAG_V | FLAG_Z | FLAG_C);
        if (result & 0x100)
            cpu->P |= FLAG_C;
        if ((cpu->A ^ result) & (value ^ result) & 0x80)
            cpu->P |= FLAG_V;
        cpu->A = result & 0xFF;
        set_zero_and_negative_flags(cpu, cpu->A);
        break;
    case 0x73: // RRA izy
        address = get_address(cpu, AM_IZY);
        value = cpu->mem[address];
        temp_carry = (cpu->P & FLAG_C) ? 1 : 0;
        cpu->P &= ~FLAG_C;
        if (value & 0x01)
            cpu->P |= FLAG_C;
        cpu->mem[address] = (value >> 1) | (temp_carry << 7);
        value = cpu->mem[address];
        result = cpu->A + value + (cpu->P & FLAG_C ? 1 : 0);
        cpu->P &= ~(FLAG_N | FLAG_V | FLAG_Z | FLAG_C);
        if (result & 0x100)
            cpu->P |= FLAG_C;
        if ((cpu->A ^ result) & (value ^ result) & 0x80)
            cpu->P |= FLAG_V;
        cpu->A = result & 0xFF;
        set_zero_and_negative_flags(cpu, cpu->A);
        break;
    case 0x74: // NOP zpx
        get_address(cpu, AM_ZPX);
        break;
    case 0x75: // ADC zpx
        address = get_address(cpu, AM_ZPX);
        value = cpu->mem[address];
        result = cpu->A + value + (cpu->P & FLAG_C ? 1 : 0);
        cpu->P &= ~(FLAG_N | FLAG_V | FLAG_Z | FLAG_C);
        if (result & 0x100)
            cpu->P |= FLAG_C;
        if ((cpu->A ^ result) & (value ^ result) & 0x80)
            cpu->P |= FLAG_V;
        cpu->A = result & 0xFF;
        set_zero_and_negative_flags(cpu, cpu->A);
        break;
    case 0x76: // ROR zpx
        address = get_address(cpu, AM_ZPX);
        value = cpu->mem[address];
        temp_carry = (cpu->P & FLAG_C) ? 1 : 0;
        cpu->P &= ~FLAG_C;
        if (value & 0x01)
            cpu->P |= FLAG_C;
        cpu->mem[address] = (value >> 1) | (temp_carry << 7);
        set_zero_and_negative_flags(cpu, cpu->mem[address]);
        break;
    case 0x77: // RRA zpx
        address = get_address(cpu, AM_ZPX);
        value = cpu->mem[address];
        temp_carry = (cpu->P & FLAG_C) ? 1 : 0;
        cpu->P &= ~FLAG_C;
        if (value & 0x01)
            cpu->P |= FLAG_C;
        cpu->mem[address] = (value >> 1) | (temp_carry << 7);
        value = cpu->mem[address];
        result = cpu->A + value + (cpu->P & FLAG_C ? 1 : 0);
        cpu->P &= ~(FLAG_N | FLAG_V | FLAG_Z | FLAG_C);
        if (result & 0x100)
            cpu->P |= FLAG_C;
        if ((cpu->A ^ result) & (value ^ result) & 0x80)
            cpu->P |= FLAG_V;
        cpu->A = result & 0xFF;
        set_zero_and_negative_flags(cpu, cpu->A);
        break;
    case 0x78: // SEI
        cpu->P |= FLAG_I;
        break;
    case 0x79: // ADC aby
        address = get_address(cpu, AM_ABY);
        value = cpu->mem[address];
        result = cpu->A + value + (cpu->P & FLAG_C ? 1 : 0);
        cpu->P &= ~(FLAG_N | FLAG_V | FLAG_Z | FLAG_C);
        if (result & 0x100)
            cpu->P |= FLAG_C;
        if ((cpu->A ^ result) & (value ^ result) & 0x80)
            cpu->P |= FLAG_V;
        cpu->A = result & 0xFF;
        set_zero_and_negative_flags(cpu, cpu->A);
        break;
    case 0x7A: // NOP
        break;
    case 0x7B: // RRA aby
        address = get_address(cpu, AM_ABY);
        value = cpu->mem[address];
        temp_carry = (cpu->P & FLAG_C) ? 1 : 0;
        cpu->P &= ~FLAG_C;
        if (value & 0x01)
            cpu->P |= FLAG_C;
        cpu->mem[address] = (value >> 1) | (temp_carry << 7);
        value = cpu->mem[address];
        result = cpu->A + value + (cpu->P & FLAG_C ? 1 : 0);
        cpu->P &= ~(FLAG_N | FLAG_V | FLAG_Z | FLAG_C);
        if (result & 0x100)
            cpu->P |= FLAG_C;
        if ((cpu->A ^ result) & (value ^ result) & 0x80)
            cpu->P |= FLAG_V;
        cpu->A = result & 0xFF;
        set_zero_and_negative_flags(cpu, cpu->A);
        break;
    case 0x7C: // NOP abx
        get_address(cpu, AM_ABX);
        break;
    case 0x7D: // ADC abx
        address = get_address(cpu, AM_ABX);
        value = cpu->mem[address];
        result = cpu->A + value + (cpu->P & FLAG_C ? 1 : 0);
        cpu->P &= ~(FLAG_N | FLAG_V | FLAG_Z | FLAG_C);
        if (result & 0x100)
            cpu->P |= FLAG_C;
        if ((cpu->A ^ result) & (value ^ result) & 0x80)
            cpu->P |= FLAG_V;
        cpu->A = result & 0xFF;
        set_zero_and_negative_flags(cpu, cpu->A);
        break;
    case 0x7E: // ROR abx
        address = get_address(cpu, AM_ABX);
        value = cpu->mem[address];
        temp_carry = (cpu->P & FLAG_C) ? 1 : 0;
        cpu->P &= ~FLAG_C;
        if (value & 0x01)
            cpu->P |= FLAG_C;
        cpu->mem[address] = (value >> 1) | (temp_carry << 7);
        set_zero_and_negative_flags(cpu, cpu->mem[address]);
        break;
    case 0x7F: // RRA abx
        address = get_address(cpu, AM_ABX);
        value = cpu->mem[address];
        temp_carry = (cpu->P & FLAG_C) ? 1 : 0;
        cpu->P &= ~FLAG_C;
        if (value & 0x01)
            cpu->P |= FLAG_C;
        cpu->mem[address] = (value >> 1) | (temp_carry << 7);
        value = cpu->mem[address];
        result = cpu->A + value + (cpu->P & FLAG_C ? 1 : 0);
        cpu->P &= ~(FLAG_N | FLAG_V | FLAG_Z | FLAG_C);
        if (result & 0x100)
            cpu->P |= FLAG_C;
        if ((cpu->A ^ result) & (value ^ result) & 0x80)
            cpu->P |= FLAG_V;
        cpu->A = result & 0xFF;
        set_zero_and_negative_flags(cpu, cpu->A);
        break;

        // --- 8x ---
    case 0x80: // NOP imm
        get_address(cpu, AM_IMM);
        break;
    case 0x81: // STA izx
        address = get_address(cpu, AM_IZX);
        cpu->mem[address] = cpu->A;
        break;
    case 0x82: // NOP imm
        get_address(cpu, AM_IMM);
        break;
    case 0x83: // SAX izx
        address = get_address(cpu, AM_IZX);
        cpu->mem[address] = cpu->A & cpu->X;
        break;
    case 0x84: // STY zp
        address = get_address(cpu, AM_ZP);
        cpu->mem[address] = cpu->Y;
        break;
    case 0x85: // STA zp
        address = get_address(cpu, AM_ZP);
        cpu->mem[address] = cpu->A;
        break;
    case 0x86: // STX zp
        address = get_address(cpu, AM_ZP);
        cpu->mem[address] = cpu->X;
        break;
    case 0x87: // SAX zp
        address = get_address(cpu, AM_ZP);
        cpu->mem[address] = cpu->A & cpu->X;
        break;
    case 0x88: // DEY
        cpu->Y--;
        set_zero_and_negative_flags(cpu, cpu->Y);
        break;
    case 0x8A: // TXA
        cpu->A = cpu->X;
        set_zero_and_negative_flags(cpu, cpu->A);
        break;
    case 0x8B: // XAA imm
        address = get_address(cpu, AM_IMM);
        cpu->A = cpu->X & cpu->mem[address];
        set_zero_and_negative_flags(cpu, cpu->A);

        break;
    case 0x8C: // STY abs
        address = get_address(cpu, AM_ABS);
        cpu->mem[address] = cpu->Y;
        break;
    case 0x8D: // STA abs
        address = get_address(cpu, AM_ABS);
        cpu->mem[address] = cpu->A;
        break;
    case 0x8E: // STX abs
        address = get_address(cpu, AM_ABS);
        cpu->mem[address] = cpu->X;
        break;
    case 0x8F: // SAX abs
        address = get_address(cpu, AM_ABS);
        cpu->mem[address] = cpu->A & cpu->X;
        break;
        // --- 9x ---
    case 0x90: // BCC rel
        address = get_address(cpu, AM_REL);
        if (!(cpu->P & FLAG_C))
            cpu->PC = address;
        break;
    case 0x91: // STA izy
        address = get_address(cpu, AM_IZY);
        cpu->mem[address] = cpu->A;
        break;
    case 0x93: // AHX izy
        address = get_address(cpu, AM_IZY);
        cpu->mem[address] = cpu->A & cpu->X & (address >> 8);
        break;
    case 0x94: // STY zpx
        address = get_address(cpu, AM_ZPX);
        cpu->mem[address] = cpu->Y;
        break;
    case 0x95: // STA zpx
        address = get_address(cpu, AM_ZPX);
        cpu->mem[address] = cpu->A;
        break;
    case 0x96: // STX zpy
        address = get_address(cpu, AM_ZPY);
        cpu->mem[address] = cpu->X;
        break;
    case 0x97: // SAX zpy
        address = get_address(cpu, AM_ZPY);
        cpu->mem[address] = cpu->A & cpu->X;
        break;
    case 0x98: // TYA
        cpu->A = cpu->Y;
        set_zero_and_negative_flags(cpu, cpu->A);
        break;
    case 0x99: // STA aby
        address = get_address(cpu, AM_ABY);
        cpu->mem[address] = cpu->A;
        break;
    case 0x9A: // TXS
        cpu->SP = cpu->X;
        break;
    case 0x9B: // TAS aby
        address = get_address(cpu, AM_ABY);
        cpu->SP = cpu->A & cpu->X;
        cpu->mem[address] = cpu->SP & (address >> 8);
        break;
    case 0x9C: // SHY abx
        address = get_address(cpu, AM_ABX);
        cpu->mem[address] = cpu->Y & (address >> 8);
        break;
    case 0x9D: // STA abx
        address = get_address(cpu, AM_ABX);
        cpu->mem[address] = cpu->A;
        break;
    case 0x9E: // SHX aby
        address = get_address(cpu, AM_ABY);
        cpu->mem[address] = cpu->X & (address >> 8);
        break;
    case 0x9F: // AHX aby
        address = get_address(cpu, AM_ABY);
        cpu->mem[address] = cpu->A & cpu->X & (address >> 8);
        break;
        // --- Ax ---
    case 0xA0: // LDY imm
        address = get_address(cpu, AM_IMM);
        cpu->Y = cpu->mem[address];
        set_zero_and_negative_flags(cpu, cpu->Y);
        break;
    case 0xA1: // LDA izx
        address = get_address(cpu, AM_IZX);
        cpu->A = cpu->mem[address];
        set_zero_and_negative_flags(cpu, cpu->A);
        break;
    case 0xA2: // LDX imm
        address = get_address(cpu, AM_IMM);
        cpu->X = cpu->mem[address];
        set_zero_and_negative_flags(cpu, cpu->X);
        break;
    case 0xA3: // LAX izx
        address = get_address(cpu, AM_IZX);
        cpu->A = cpu->X = cpu->mem[address];
        set_zero_and_negative_flags(cpu, cpu->A);
        break;
    case 0xA4: // LDY zp
        address = get_address(cpu, AM_ZP);
        cpu->Y = cpu->mem[address];
        set_zero_and_negative_flags(cpu, cpu->Y);
        break;
    case 0xA5: // LDA zp
        address = get_address(cpu, AM_ZP);
        cpu->A = cpu->mem[address];
        set_zero_and_negative_flags(cpu, cpu->A);
        break;
    case 0xA6: // LDX zp
        address = get_address(cpu, AM_ZP);
        cpu->X = cpu->mem[address];
        set_zero_and_negative_flags(cpu, cpu->X);
        break;
    case 0xA7: // LAX zp
        address = get_address(cpu, AM_ZP);
        cpu->A = cpu->X = cpu->mem[address];
        set_zero_and_negative_flags(cpu, cpu->A);
        break;
    case 0xA8: // TAY
        cpu->Y = cpu->A;
        set_zero_and_negative_flags(cpu, cpu->Y);
        break;
    case 0xA9: // LDA imm
        address = get_address(cpu, AM_IMM);
        cpu->A = cpu->mem[address];
        set_zero_and_negative_flags(cpu, cpu->A);
        break;
    case 0xAA: // TAX
        cpu->X = cpu->A;
        set_zero_and_negative_flags(cpu, cpu->X);
        break;
    case 0xAB: // LAX imm
        address = get_address(cpu, AM_IMM);
        cpu->A = cpu->X = cpu->mem[address];
        set_zero_and_negative_flags(cpu, cpu->A);
        break;
    case 0xAC: // LDY abs
        address = get_address(cpu, AM_ABS);
        cpu->Y = cpu->mem[address];
        set_zero_and_negative_flags(cpu, cpu->Y);
        break;
    case 0xAD: // LDA abs
        address = get_address(cpu, AM_ABS);
        cpu->A = cpu->mem[address];
        set_zero_and_negative_flags(cpu, cpu->A);
        break;
    case 0xAE: // LDX abs
        address = get_address(cpu, AM_ABS);
        cpu->X = cpu->mem[address];
        set_zero_and_negative_flags(cpu, cpu->X);
        break;
    case 0xAF: // LAX abs
        address = get_address(cpu, AM_ABS);
        cpu->A = cpu->X = cpu->mem[address];
        set_zero_and_negative_flags(cpu, cpu->A);
        break;

        // --- Bx ---
    case 0xB0: // BCS rel
        address = get_address(cpu, AM_REL);
        if (cpu->P & FLAG_C)
            cpu->PC = address;
        break;
    case 0xB1: // LDA izy
        address = get_address(cpu, AM_IZY);
        cpu->A = cpu->mem[address];
        set_zero_and_negative_flags(cpu, cpu->A);
        break;
    case 0xB3: // LAX izy
        address = get_address(cpu, AM_IZY);
        cpu->A = cpu->X = cpu->mem[address];
        set_zero_and_negative_flags(cpu, cpu->A);
        break;
    case 0xB4: // LDY zpx
        address = get_address(cpu, AM_ZPX);
        cpu->Y = cpu->mem[address];
        set_zero_and_negative_flags(cpu, cpu->Y);
        break;
    case 0xB5: // LDA zpx
        address = get_address(cpu, AM_ZPX);
        cpu->A = cpu->mem[address];
        set_zero_and_negative_flags(cpu, cpu->A);
        break;
    case 0xB6: // LDX zpy
        address = get_address(cpu, AM_ZPY);
        cpu->X = cpu->mem[address];
        set_zero_and_negative_flags(cpu, cpu->X);
        break;
    case 0xB7: // LAX zpy
        address = get_address(cpu, AM_ZPY);
        cpu->A = cpu->X = cpu->mem[address];
        set_zero_and_negative_flags(cpu, cpu->A);
        break;
    case 0xB8: // CLV
        cpu->P &= ~FLAG_V;
        break;
    case 0xB9: // LDA aby
        address = get_address(cpu, AM_ABY);
        cpu->A = cpu->mem[address];
        set_zero_and_negative_flags(cpu, cpu->A);
        break;
    case 0xBA: // TSX
        cpu->X = cpu->SP;
        set_zero_and_negative_flags(cpu, cpu->X);
        break;
    case 0xBB: // LAS aby
        address = get_address(cpu, AM_ABY);
        cpu->A = cpu->X = cpu->SP = cpu->mem[address] & cpu->SP;
        set_zero_and_negative_flags(cpu, cpu->A);
        break;
    case 0xBC: // LDY abx
        address = get_address(cpu, AM_ABX);
        cpu->Y = cpu->mem[address];
        set_zero_and_negative_flags(cpu, cpu->Y);
        break;
    case 0xBD: // LDA abx
        address = get_address(cpu, AM_ABX);
        cpu->A = cpu->mem[address];
        set_zero_and_negative_flags(cpu, cpu->A);
        break;
    case 0xBE: // LDX aby
        address = get_address(cpu, AM_ABY);
        cpu->X = cpu->mem[address];
        set_zero_and_negative_flags(cpu, cpu->X);
        break;
    case 0xBF: // LAX aby
        address = get_address(cpu, AM_ABY);
        cpu->A = cpu->X = cpu->mem[address];
        set_zero_and_negative_flags(cpu, cpu->A);
        break;

        // --- Cx ---
    case 0xC0: // CPY imm
        address = get_address(cpu, AM_IMM);
        value = cpu->mem[address];
        cpu->P &= ~(FLAG_N | FLAG_Z | FLAG_C);
        if (cpu->Y >= value)
            cpu->P |= FLAG_C;
        set_zero_and_negative_flags(cpu, cpu->Y - value);
        break;
    case 0xC1: // CMP izx
        address = get_address(cpu, AM_IZX);
        value = cpu->mem[address];
        cpu->P &= ~(FLAG_N | FLAG_Z | FLAG_C);
        if (cpu->A >= value)
            cpu->P |= FLAG_C;
        set_zero_and_negative_flags(cpu, cpu->A - value);
        break;
    case 0xC2: // NOP imm
        get_address(cpu, AM_IMM);
        break;
    case 0xC3: // DCP izx
        address = get_address(cpu, AM_IZX);
        value = cpu->mem[address];
        value--;
        cpu->mem[address] = value;
        cpu->P &= ~(FLAG_N | FLAG_Z | FLAG_C);
        if (cpu->A >= value)
            cpu->P |= FLAG_C;
        set_zero_and_negative_flags(cpu, cpu->A - value);
        break;
    case 0xC4: // CPY zp
        address = get_address(cpu, AM_ZP);
        value = cpu->mem[address];
        cpu->P &= ~(FLAG_N | FLAG_Z | FLAG_C);
        if (cpu->Y >= value)
            cpu->P |= FLAG_C;
        set_zero_and_negative_flags(cpu, cpu->Y - value);
        break;
    case 0xC5: // CMP zp
        address = get_address(cpu, AM_ZP);
        value = cpu->mem[address];
        cpu->P &= ~(FLAG_N | FLAG_Z | FLAG_C);
        if (cpu->A >= value)
            cpu->P |= FLAG_C;
        set_zero_and_negative_flags(cpu, cpu->A - value);
        break;
    case 0xC6: // DEC zp
        address = get_address(cpu, AM_ZP);
        value = cpu->mem[address];
        value--;
        cpu->mem[address] = value;
        set_zero_and_negative_flags(cpu, value);
        break;
    case 0xC7: // DCP zp
        address = get_address(cpu, AM_ZP);
        value = cpu->mem[address];
        value--;
        cpu->mem[address] = value;
        cpu->P &= ~(FLAG_N | FLAG_Z | FLAG_C);
        if (cpu->A >= value)
            cpu->P |= FLAG_C;
        set_zero_and_negative_flags(cpu, cpu->A - value);
        break;
    case 0xC8: // INY
        cpu->Y++;
        set_zero_and_negative_flags(cpu, cpu->Y);
        break;
    case 0xC9: // CMP imm
        address = get_address(cpu, AM_IMM);
        value = cpu->mem[address];
        cpu->P &= ~(FLAG_N | FLAG_Z | FLAG_C);
        if (cpu->A >= value)
            cpu->P |= FLAG_C;
        set_zero_and_negative_flags(cpu, cpu->A - value);
        break;
    case 0xCA: // DEX
        cpu->X--;
        set_zero_and_negative_flags(cpu, cpu->X);
        break;
    case 0xCB: // AXS imm
        address = get_address(cpu, AM_IMM);
        value = cpu->mem[address];
        cpu->X = (cpu->A & cpu->X) - value;
        cpu->P &= ~(FLAG_N | FLAG_Z | FLAG_C);
        if ((cpu->A & cpu->X) >= value)
            cpu->P |= FLAG_C;
        set_zero_and_negative_flags(cpu, cpu->X);

        break;
    case 0xCC: // CPY abs
        address = get_address(cpu, AM_ABS);
        value = cpu->mem[address];
        cpu->P &= ~(FLAG_N | FLAG_Z | FLAG_C);
        if (cpu->Y >= value)
            cpu->P |= FLAG_C;
        set_zero_and_negative_flags(cpu, cpu->Y - value);
        break;
    case 0xCD: // CMP abs
        address = get_address(cpu, AM_ABS);
        value = cpu->mem[address];
        cpu->P &= ~(FLAG_N | FLAG_Z | FLAG_C);
        if (cpu->A >= value)
            cpu->P |= FLAG_C;
        set_zero_and_negative_flags(cpu, cpu->A - value);
        break;
    case 0xCE: // DEC abs
        address = get_address(cpu, AM_ABS);
        value = cpu->mem[address];
        value--;
        cpu->mem[address] = value;
        set_zero_and_negative_flags(cpu, value);
        break;
    case 0xCF: // DCP abs
        address = get_address(cpu, AM_ABS);
        value = cpu->mem[address];
        value--;
        cpu->mem[address] = value;
        cpu->P &= ~(FLAG_N | FLAG_Z | FLAG_C);
        if (cpu->A >= value)
            cpu->P |= FLAG_C;
        set_zero_and_negative_flags(cpu, cpu->A - value);
        break;

        // --- Dx ---
    case 0xD0: // BNE rel
        address = get_address(cpu, AM_REL);
        if (!(cpu->P & FLAG_Z))
            cpu->PC = address;
        break;
    case 0xD1: // CMP izy
        address = get_address(cpu, AM_IZY);
        value = cpu->mem[address];
        cpu->P &= ~(FLAG_N | FLAG_Z | FLAG_C);
        if (cpu->A >= value)
            cpu->P |= FLAG_C;
        set_zero_and_negative_flags(cpu, cpu->A - value);
        break;
    case 0xD3: // DCP izy
        address = get_address(cpu, AM_IZY);
        value = cpu->mem[address];
        value--;
        cpu->mem[address] = value;
        cpu->P &= ~(FLAG_N | FLAG_Z | FLAG_C);
        if (cpu->A >= value)
            cpu->P |= FLAG_C;
        set_zero_and_negative_flags(cpu, cpu->A - value);
        break;
    case 0xD4: // NOP zpx
        get_address(cpu, AM_ZPX);
        break;
    case 0xD5: // CMP zpx
        address = get_address(cpu, AM_ZPX);
        value = cpu->mem[address];
        cpu->P &= ~(FLAG_N | FLAG_Z | FLAG_C);
        if (cpu->A >= value)
            cpu->P |= FLAG_C;
        set_zero_and_negative_flags(cpu, cpu->A - value);
        break;
    case 0xD6: // DEC zpx
        address = get_address(cpu, AM_ZPX);
        value = cpu->mem[address];
        value--;
        cpu->mem[address] = value;
        set_zero_and_negative_flags(cpu, value);
        break;
    case 0xD7: // DCP zpx
        address = get_address(cpu, AM_ZPX);
        value = cpu->mem[address];
        value--;
        cpu->mem[address] = value;
        cpu->P &= ~(FLAG_N | FLAG_Z | FLAG_C);
        if (cpu->A >= value)
            cpu->P |= FLAG_C;
        set_zero_and_negative_flags(cpu, cpu->A - value);
        break;
    case 0xD8: // CLD
        cpu->P &= ~FLAG_D;
        break;
    case 0xD9: // CMP aby
        address = get_address(cpu, AM_ABY);
        value = cpu->mem[address];
        cpu->P &= ~(FLAG_N | FLAG_Z | FLAG_C);
        if (cpu->A >= value)
            cpu->P |= FLAG_C;
        set_zero_and_negative_flags(cpu, cpu->A - value);
        break;
    case 0xDA: // NOP
        break;
    case 0xDB: // DCP aby
        address = get_address(cpu, AM_ABY);
        value = cpu->mem[address];
        value--;
        cpu->mem[address] = value;
        cpu->P &= ~(FLAG_N | FLAG_Z | FLAG_C);
        if (cpu->A >= value)
            cpu->P |= FLAG_C;
        set_zero_and_negative_flags(cpu, cpu->A - value);
        break;
    case 0xDC: // NOP abx
        get_address(cpu, AM_ABX);
        break;
    case 0xDD: // CMP abx
        address = get_address(cpu, AM_ABX);
        value = cpu->mem[address];
        cpu->P &= ~(FLAG_N | FLAG_Z | FLAG_C);
        if (cpu->A >= value)
            cpu->P |= FLAG_C;
        set_zero_and_negative_flags(cpu, cpu->A - value);
        break;
    case 0xDE: // DEC abx
        address = get_address(cpu, AM_ABX);
        value = cpu->mem[address];
        value--;
        cpu->mem[address] = value;
        set_zero_and_negative_flags(cpu, value);
        break;
    case 0xDF: // DCP abx
        address = get_address(cpu, AM_ABX);
        value = cpu->mem[address];
        value--;
        cpu->mem[address] = value;
        cpu->P &= ~(FLAG_N | FLAG_Z | FLAG_C);
        if (cpu->A >= value)
            cpu->P |= FLAG_C;
        set_zero_and_negative_flags(cpu, cpu->A - value);
        break;

        // --- Ex ---
    case 0xE0: // CPX imm
        address = get_address(cpu, AM_IMM);
        value = cpu->mem[address];
        cpu->P &= ~(FLAG_N | FLAG_Z | FLAG_C);
        if (cpu->X >= value)
            cpu->P |= FLAG_C;
        set_zero_and_negative_flags(cpu, cpu->X - value);
        break;
    case 0xE1: // SBC izx
        address = get_address(cpu, AM_IZX);
        value = cpu->mem[address];
        result = cpu->A - value - ((cpu->P & FLAG_C) ? 0 : 1);
        cpu->P &= ~(FLAG_N | FLAG_V | FLAG_Z | FLAG_C);
        if (!(result & 0x100))
            cpu->P |= FLAG_C;

        if ((cpu->A ^ result) & (~value ^ result) & 0x80)
            cpu->P |= FLAG_V;
        cpu->A = result & 0xFF;
        set_zero_and_negative_flags(cpu, cpu->A);

        break;
    case 0xE2: // NOP imm
        get_address(cpu, AM_IMM);
        break;
    case 0xE3: // ISC izx
        address = get_address(cpu, AM_IZX);
        value = cpu->mem[address];
        value++;
        cpu->mem[address] = value;
        result = cpu->A - value - ((cpu->P & FLAG_C) ? 0 : 1);
        cpu->P &= ~(FLAG_N | FLAG_V | FLAG_Z | FLAG_C);
        if (!(result & 0x100))
            cpu->P |= FLAG_C;
        if ((cpu->A ^ result) & (~value ^ result) & 0x80)
            cpu->P |= FLAG_V;
        cpu->A = result & 0xFF;
        set_zero_and_negative_flags(cpu, cpu->A);
        break;
    case 0xE4: // CPX zp
        address = get_address(cpu, AM_ZP);
        value = cpu->mem[address];
        cpu->P &= ~(FLAG_N | FLAG_Z | FLAG_C);
        if (cpu->X >= value)
            cpu->P |= FLAG_C;
        set_zero_and_negative_flags(cpu, cpu->X - value);
        break;
    case 0xE5: // SBC zp
        address = get_address(cpu, AM_ZP);
        value = cpu->mem[address];
        result = cpu->A - value - ((cpu->P & FLAG_C) ? 0 : 1);
        cpu->P &= ~(FLAG_N | FLAG_V | FLAG_Z | FLAG_C);
        if (!(result & 0x100))
            cpu->P |= FLAG_C;
        if ((cpu->A ^ result) & (~value ^ result) & 0x80)
            cpu->P |= FLAG_V;
        cpu->A = result & 0xFF;
        set_zero_and_negative_flags(cpu, cpu->A);
        break;
    case 0xE6: // INC zp
        address = get_address(cpu, AM_ZP);
        value = cpu->mem[address];
        value++;
        cpu->mem[address] = value;
        set_zero_and_negative_flags(cpu, value);
        break;
    case 0xE7: // ISC zp
        address = get_address(cpu, AM_ZP);
        value = cpu->mem[address];
        value++;
        cpu->mem[address] = value;
        result = cpu->A - value - ((cpu->P & FLAG_C) ? 0 : 1);
        cpu->P &= ~(FLAG_N | FLAG_V | FLAG_Z | FLAG_C);
        if (!(result & 0x100))
            cpu->P |= FLAG_C;
        if ((cpu->A ^ result) & (~value ^ result) & 0x80)
            cpu->P |= FLAG_V;
        cpu->A = result & 0xFF;
        set_zero_and_negative_flags(cpu, cpu->A);
        break;
    case 0xE8: // INX
        cpu->X++;
        set_zero_and_negative_flags(cpu, cpu->X);
        break;
    case 0xE9: // SBC imm
        address = get_address(cpu, AM_IMM);
        value = cpu->mem[address];
        result = cpu->A - value - ((cpu->P & FLAG_C) ? 0 : 1);
        cpu->P &= ~(FLAG_N | FLAG_V | FLAG_Z | FLAG_C);
        if (!(result & 0x100))
            cpu->P |= FLAG_C;
        if ((cpu->A ^ result) & (~value ^ result) & 0x80)
            cpu->P |= FLAG_V;
        cpu->A = result & 0xFF;
        set_zero_and_negative_flags(cpu, cpu->A);
        break;
    case 0xEA: // NOP
        break;
    case 0xEB: // SBC imm
        address = get_address(cpu, AM_IMM);
        value = cpu->mem[address];
        result = cpu->A - value - ((cpu->P & FLAG_C) ? 0 : 1);
        cpu->P &= ~(FLAG_N | FLAG_V | FLAG_Z | FLAG_C);
        if (!(result & 0x100))
            cpu->P |= FLAG_C;
        if ((cpu->A ^ result) & (~value ^ result) & 0x80)
            cpu->P |= FLAG_V;
        cpu->A = result & 0xFF;
        set_zero_and_negative_flags(cpu, cpu->A);
        break;
    case 0xEC: // CPX abs
        address = get_address(cpu, AM_ABS);
        value = cpu->mem[address];
        cpu->P &= ~(FLAG_N | FLAG_Z | FLAG_C);
        if (cpu->X >= value)
            cpu->P |= FLAG_C;
        set_zero_and_negative_flags(cpu, cpu->X - value);
        break;
    case 0xED: // SBC abs
        address = get_address(cpu, AM_ABS);
        value = cpu->mem[address];
        result = cpu->A - value - ((cpu->P & FLAG_C) ? 0 : 1);
        cpu->P &= ~(FLAG_N | FLAG_V | FLAG_Z | FLAG_C);
        if (!(result & 0x100))
            cpu->P |= FLAG_C;
        if ((cpu->A ^ result) & (~value ^ result) & 0x80)
            cpu->P |= FLAG_V;
        cpu->A = result & 0xFF;
        set_zero_and_negative_flags(cpu, cpu->A);
        break;
    case 0xEE: // INC abs
        address = get_address(cpu, AM_ABS);
        value = cpu->mem[address];
        value++;
        cpu->mem[address] = value;
        set_zero_and_negative_flags(cpu, value);
        break;
    case 0xEF: // ISC abs
        address = get_address(cpu, AM_ABS);
        value = cpu->mem[address];
        value++;
        cpu->mem[address] = value;
        result = cpu->A - value - ((cpu->P & FLAG_C) ? 0 : 1);
        cpu->P &= ~(FLAG_N | FLAG_V | FLAG_Z | FLAG_C);
        if (!(result & 0x100))
            cpu->P |= FLAG_C;
        if ((cpu->A ^ result) & (~value ^ result) & 0x80)
            cpu->P |= FLAG_V;
        cpu->A = result & 0xFF;
        set_zero_and_negative_flags(cpu, cpu->A);
        break;

        // --- Fx ---
    case 0xF0: // BEQ rel
        address = get_address(cpu, AM_REL);
        if (cpu->P & FLAG_Z)
            cpu->PC = address;
        break;
    case 0xF1: // SBC izy
        address = get_address(cpu, AM_IZY);
        value = cpu->mem[address];
        result = cpu->A - value - ((cpu->P & FLAG_C) ? 0 : 1);
        cpu->P &= ~(FLAG_N | FLAG_V | FLAG_Z | FLAG_C);
        if (!(result & 0x100))
            cpu->P |= FLAG_C;
        if ((cpu->A ^ result) & (~value ^ result) & 0x80)
            cpu->P |= FLAG_V;
        cpu->A = result & 0xFF;
        set_zero_and_negative_flags(cpu, cpu->A);
        break;
    case 0xF3: // ISC izy
        address = get_address(cpu, AM_IZY);
        value = cpu->mem[address];
        value++;
        cpu->mem[address] = value;
        result = cpu->A - value - ((cpu->P & FLAG_C) ? 0 : 1);
        cpu->P &= ~(FLAG_N | FLAG_V | FLAG_Z | FLAG_C);
        if (!(result & 0x100))
            cpu->P |= FLAG_C;
        if ((cpu->A ^ result) & (~value ^ result) & 0x80)
            cpu->P |= FLAG_V;
        cpu->A = result & 0xFF;
        set_zero_and_negative_flags(cpu, cpu->A);
        break;
    case 0xF4: // NOP zpx
        get_address(cpu, AM_ZPX);
        break;
    case 0xF5: // SBC zpx
        address = get_address(cpu, AM_ZPX);
        value = cpu->mem[address];
        result = cpu->A - value - ((cpu->P & FLAG_C) ? 0 : 1);
        cpu->P &= ~(FLAG_N | FLAG_V | FLAG_Z | FLAG_C);
        if (!(result & 0x100))
            cpu->P |= FLAG_C;
        if ((cpu->A ^ result) & (~value ^ result) & 0x80)
            cpu->P |= FLAG_V;
        cpu->A = result & 0xFF;
        set_zero_and_negative_flags(cpu, cpu->A);
        break;
    case 0xF6: // INC zpx
        address = get_address(cpu, AM_ZPX);
        value = cpu->mem[address];
        value++;
        cpu->mem[address] = value;
        set_zero_and_negative_flags(cpu, value);
        break;
    case 0xF7: // ISC zpx
        address = get_address(cpu, AM_ZPX);
        value = cpu->mem[address];
        value++;
        cpu->mem[address] = value;
        result = cpu->A - value - ((cpu->P & FLAG_C) ? 0 : 1);
        cpu->P &= ~(FLAG_N | FLAG_V | FLAG_Z | FLAG_C);
        if (!(result & 0x100))
            cpu->P |= FLAG_C;
        if ((cpu->A ^ result) & (~value ^ result) & 0x80)
            cpu->P |= FLAG_V;
        cpu->A = result & 0xFF;
        set_zero_and_negative_flags(cpu, cpu->A);
        break;
    case 0xF8: // SED
        cpu->P |= FLAG_D;
        break;
    case 0xF9: // SBC aby
        address = get_address(cpu, AM_ABY);
        value = cpu->mem[address];
        result = cpu->A - value - ((cpu->P & FLAG_C) ? 0 : 1);
        cpu->P &= ~(FLAG_N | FLAG_V | FLAG_Z | FLAG_C);
        if (!(result & 0x100))
            cpu->P |= FLAG_C;
        if ((cpu->A ^ result) & (~value ^ result) & 0x80)
            cpu->P |= FLAG_V;
        cpu->A = result & 0xFF;
        set_zero_and_negative_flags(cpu, cpu->A);
        break;
    case 0xFA: // NOP
        break;
    case 0xFB: // ISC aby
        address = get_address(cpu, AM_ABY);
        value = cpu->mem[address];
        value++;
        cpu->mem[address] = value;
        result = cpu->A - value - ((cpu->P & FLAG_C) ? 0 : 1);
        cpu->P &= ~(FLAG_N | FLAG_V | FLAG_Z | FLAG_C);
        if (!(result & 0x100))
            cpu->P |= FLAG_C;
        if ((cpu->A ^ result) & (~value ^ result) & 0x80)
            cpu->P |= FLAG_V;
        cpu->A = result & 0xFF;
        set_zero_and_negative_flags(cpu, cpu->A);
        break;
    case 0xFC: // NOP abx
        get_address(cpu, AM_ABX);
        break;
    case 0xFD: // SBC abx
        address = get_address(cpu, AM_ABX);
        value = cpu->mem[address];
        result = cpu->A - value - ((cpu->P & FLAG_C) ? 0 : 1);
        cpu->P &= ~(FLAG_N | FLAG_V | FLAG_Z | FLAG_C);
        if (!(result & 0x100))
            cpu->P |= FLAG_C;
        if ((cpu->A ^ result) & (~value ^ result) & 0x80)
            cpu->P |= FLAG_V;
        cpu->A = result & 0xFF;
        set_zero_and_negative_flags(cpu, cpu->A);
        break;
    case 0xFE: // INC abx
        address = get_address(cpu, AM_ABX);
        value = cpu->mem[address];
        value++;
        cpu->mem[address] = value;
        set_zero_and_negative_flags(cpu, value);
        break;
    case 0xFF: // ISC abx
        address = get_address(cpu, AM_ABX);
        value = cpu->mem[address];
        value++;
        cpu->mem[address] = value;
        result = cpu->A - value - ((cpu->P & FLAG_C) ? 0 : 1);
        cpu->P &= ~(FLAG_N | FLAG_V | FLAG_Z | FLAG_C);
        if (!(result & 0x100))
            cpu->P |= FLAG_C;
        if ((cpu->A ^ result) & (~value ^ result) & 0x80)
            cpu->P |= FLAG_V;
        cpu->A = result & 0xFF;
        set_zero_and_negative_flags(cpu, cpu->A);
        break;
    default:
        printf("Unknown Opcode: 0x%02X at $%04X\n", opcode, cpu->PC - 1);
        cpu->PC = 0xFFFF;
        break;
    }

    if (address >= 0x200 && address < (0x200 + SCREEN_WIDTH * SCREEN_HEIGHT))
    {
        pixels[address - 0x200] = palette[cpu->mem[address]];
    }
}

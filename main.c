// C stdlib
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>

// Allegro 5
#include <allegro5/allegro.h>
#include <allegro5/allegro_primitives.h>
#include <allegro5/keyboard.h>

/*
 * TODO:
 * - Something's broken, no idea what, need to find and fix it tho
 */

void must_init(bool test, const char *description) {
    if (test) {
        return;
    } else {
        printf("couln't initialise %s\n", description);
        exit(1);
    }
}

// keyboard
// add all this so I can just `key[ALLEGRO_KEY_CODE]` to read keys, a la Allegro 4
#define KEY_SEEN 0b01
#define KEY_RELEASED 0b10
unsigned char key[ALLEGRO_KEY_MAX];


int chip_keys[16] = {
    ALLEGRO_KEY_1, ALLEGRO_KEY_2, ALLEGRO_KEY_3, ALLEGRO_KEY_4,
    ALLEGRO_KEY_Q, ALLEGRO_KEY_W, ALLEGRO_KEY_E, ALLEGRO_KEY_R,
    ALLEGRO_KEY_A, ALLEGRO_KEY_S, ALLEGRO_KEY_D, ALLEGRO_KEY_F,
    ALLEGRO_KEY_Z, ALLEGRO_KEY_X, ALLEGRO_KEY_C, ALLEGRO_KEY_V
};


void keyboard_init() {
    memset(key, 0, sizeof(key));
}

void keyboard_update(ALLEGRO_EVENT *event) {
    switch (event->type) {
        case ALLEGRO_EVENT_TIMER:
            for (int i = 0; i < ALLEGRO_KEY_MAX; i++) {
                key[i] &= KEY_SEEN;
            }
            break;

        case ALLEGRO_EVENT_KEY_DOWN:
            key[event->keyboard.keycode] = KEY_SEEN | KEY_RELEASED;
            break;

        case ALLEGRO_EVENT_KEY_UP:
            key[event->keyboard.keycode] &= KEY_RELEASED;
            break;
    }
}

// Display
#define BUFFER_W 64
#define BUFFER_H 32

#define DISP_SCALE 20                  // need to change this so I can dynamically change it down the road
#define DISP_W (BUFFER_W * DISP_SCALE) // because let's be real, hardcoded final resolutions are shit
#define DISP_H (BUFFER_H * DISP_SCALE) // what is this, Flash Player running in a browser???

ALLEGRO_DISPLAY *disp;
ALLEGRO_BITMAP *buffer;

// CHIP-8 memory and registers go bellow this comment

// registers
uint8_t  V[0x10] = {0}; // RW, general purpose registers
uint16_t I = 0;         // RW, Index Register
uint16_t PC = 0x200;    // RO, address of current instruction
uint8_t  SP = 0;        // RO, index of the top value on the stack
uint8_t  DT = 0;        // RW, delay timer <- both of them count down every clock tick till they reach 0
uint8_t  ST = 0;        // RW, sound timer <-/

uint16_t stack[0x1f] = {0};

uint16_t opcode = 0x0000; // current opcode being parsed, for use down in main()

// memory
uint8_t  memory[0x1000] = {
    // Init the memory with fonts stored from 0x000 to 0x054 (inclussive)
    // everything else is zeroed
    0xF0, 0x90, 0x90, 0x90, 0xF0, // 0 at 0x000
    0x20, 0x60, 0x20, 0x20, 0x70, // 1 at 0x005
    0xF0, 0x10, 0xF0, 0x80, 0xF0, // 2 at 0x00a
    0xF0, 0x10, 0xF0, 0x10, 0xF0, // 3 at 0x010
    0x90, 0x90, 0xF0, 0x10, 0x10, // 4 at 0x015
    0xF0, 0x80, 0xF0, 0x10, 0xF0, // 5 at 0x01a
    0xF0, 0x80, 0xF0, 0x90, 0xF0, // 6 at 0x020
    0xF0, 0x10, 0x20, 0x40, 0x40, // 7 at 0x025
    0xF0, 0x90, 0xF0, 0x90, 0xF0, // 8 at 0x02a
    0xF0, 0x90, 0xF0, 0x10, 0xF0, // 9 at 0x030
    0xF0, 0x90, 0xF0, 0x90, 0x90, // A at 0x035
    0xE0, 0x90, 0xE0, 0x90, 0xE0, // B at 0x03a
    0xF0, 0x80, 0x80, 0x80, 0xF0, // C at 0x040
    0xE0, 0x90, 0x90, 0x90, 0xE0, // D at 0x045
    0xF0, 0x80, 0xF0, 0x80, 0xF0, // E at 0x04a
    0xF0, 0x80, 0xF0, 0x80, 0x80  // F at 0x050
};

void load_rom(char *path) {
    // I hope this works fine
    FILE *ROM = fopen(path, "r");

    for (int addr = 0x200; addr <= 0xFFF; addr++) {
        if (!read(fileno(ROM), &memory[addr], 1)) {
            break;
        }
    }

    fclose(ROM);
}


// Video
uint64_t vmemory[32] = {0};

void flip_pix(uint8_t x, uint8_t y) {
    vmemory[y] ^= 0b1 << (63 - x);
}

void set_pix(uint8_t x, uint8_t y) {
    vmemory[y] |= 0b1 << (63 - x);
}

/*
void get_pix(uint8_t x, uint8_t y) {
    vmemory[y] & (0b1 << (63 - y));
}
// */

// leftover, used for debugging when I needed to init the video memory to
// anything other than all 0s, left it in commented in case I'll need it again
/*
void init_memory() {
    memory[0]  = 0b0000000000000000000000000000000000000000000000000000000000000000;
    memory[1]  = 0b0000000000000000000000000000000000000000000000000000000000000000;
    memory[2]  = 0b0000000000000000000000000000000000000000000000000000000000000000;
    memory[3]  = 0b0000000000000000000000000000000000000000000000000000000000000000;
    memory[4]  = 0b0000000000000000000000000000000000000000000000000000000000000000;
    memory[5]  = 0b0000000000000000000000000000000000000000000000000000000000000000;
    memory[6]  = 0b0000000000000000000000000000000000000000000000000000000000000000;
    memory[7]  = 0b0000000000000000000000000000000000000000000000000000000000000000;
    memory[8]  = 0b0000000000000000000000000000000000000000000000000000000000000000;
    memory[9]  = 0b0000000000000000000000000000000000000000000000000000000000000000;
    memory[10] = 0b0000000000000000000000000000000000000000000000000000000000000000;
    memory[11] = 0b0000000000000000000000000000000000000000000000000000000000000000;
    memory[12] = 0b0000000000000000000000000000000000000000000000000000000000000000;
    memory[13] = 0b0000000000000000000000000000000000000000000000000000000000000000;
    memory[14] = 0b0000000000000000000000000000000000000000000000000000000000000000;
    memory[15] = 0b0000000000000000000000000000000000000000000000000000000000000000;
    memory[16] = 0b0000000000000000000000000000000000000000000000000000000000000000;
    memory[17] = 0b0000000000000000000000000000000000000000000000000000000000000000;
    memory[18] = 0b0000000000000000000000000000000000000000000000000000000000000000;
    memory[19] = 0b0000000000000000000000000000000000000000000000000000000000000000;
    memory[20] = 0b0000000000000000000000000000000000000000000000000000000000000000;
    memory[21] = 0b0000000000000000000000000000000000000000000000000000000000000000;
    memory[22] = 0b0000000000000000000000000000000000000000000000000000000000000000;
    memory[23] = 0b0000000000000000000000000000000000000000000000000000000000000000;
    memory[24] = 0b0000000000000000000000000000000000000000000000000000000000000000;
    memory[25] = 0b0000000000000000000000000000000000000000000000000000000000000000;
    memory[26] = 0b0000000000000000000000000000000000000000000000000000000000000000;
    memory[27] = 0b0000000000000000000000000000000000000000000000000000000000000000;
    memory[28] = 0b0000000000000000000000000000000000000000000000000000000000000000;
    memory[29] = 0b0000000000000000000000000000000000000000000000000000000000000000;
    memory[30] = 0b0000000000000000000000000000000000000000000000000000000000000000;
    memory[31] = 0b0000000000000000000000000000000000000000000000000000000000000000;
}
// */

// CHIP-8 memory, functions, and registers go above this comment

void memory_dump() {
    // just dumps the current state of the memory
    for (int addr = 0x000; addr <= 0xFFF; addr++) {
        if (!(addr % 0x50)) {
            printf("\n0x%03x:\t", addr);
        }
        printf("%02x ", memory[addr]);
    }
    printf("\n");
}

int main(int argc, char **argv) {
    if (argc > 1) {
        load_rom(argv[1]);
    } else {
        printf("Please supply a CHIP-8 ROM file\n");
        exit(1);
    }

    memory_dump();

    // init Allegro itself
    must_init(al_init(), "Allegro");
    must_init(al_install_keyboard(), "keyboard"); // keyboard addon to read the keyboard
    keyboard_init(); // initializes the wrapper

    // set the timer to tick every 60th of a second so the logic and drawing advance at 60Hz
    ALLEGRO_TIMER *timer = al_create_timer(1.0 / 60.0);
    must_init(timer, "timer"); // also make sure it's properly inited :P

    // event queue, same as above
    ALLEGRO_EVENT_QUEUE *queue = al_create_event_queue();
    must_init(queue, "queue");

    // the display
    // tl;dr: it's basically tripple buffered, a bitmap buffer at CHIP-8's native
    // resolution and then the the buffer required by allegro in the form of `disp`
    // this second one's at the final rendered resolution and exists solely because
    // Allegro 5 requires this backbuffer
    must_init(disp = al_create_display(DISP_W, DISP_H), "display");
    // and backbuffer
    must_init(buffer = al_create_bitmap(BUFFER_W, BUFFER_H), "buffer");
    // then init the display to black
    al_set_target_bitmap(buffer);
    al_clear_to_color(al_map_rgb(0x00, 0x00, 0x00));
    al_set_target_backbuffer(disp);


    must_init(al_init_primitives_addon(), "primitives"); // Allegro's graphics primitives addon

    // register all the event sources with the event queue inited above
    al_register_event_source(queue, al_get_keyboard_event_source());
    al_register_event_source(queue, al_get_display_event_source(disp));
    al_register_event_source(queue, al_get_timer_event_source(timer));


    bool done = false;   // exit flag
    bool redraw = true;  // redraw flag
    int status = 0;      // exit status
    bool hang = false;   // for 0x
    ALLEGRO_EVENT event; // used to read one event at a time

    al_start_timer(timer); // start the timer
    while (true) {
        al_wait_for_event(queue, &event); // wait till there are events in the event queue then read the first event in the queue

        switch (event.type) {
            case ALLEGRO_EVENT_TIMER: // clock ticks
                if (!hang) {
                    opcode = (memory[PC] << 8) | memory[PC + 1];

                    //*
                    // print PC, opcode pointed to by PC, then the registers
                    printf("0x%03x:\t0x%04x\t", PC, opcode);
                    //print the registers
                    printf("V0: 0x%02x  ", V[0x0]);
                    printf("V1: 0x%02x  ", V[0x1]);
                    printf("V2: 0x%02x  ", V[0x2]);
                    printf("V3: 0x%02x  ", V[0x3]);
                    printf("V4: 0x%02x  ", V[0x4]);
                    printf("V5: 0x%02x  ", V[0x5]);
                    printf("V6: 0x%02x  ", V[0x6]);
                    printf("V7: 0x%02x  ", V[0x7]);
                    printf("V8: 0x%02x  ", V[0x8]);
                    printf("V9: 0x%02x  ", V[0x9]);
                    printf("VA: 0x%02x  ", V[0xA]);
                    printf("VB: 0x%02x  ", V[0xB]);
                    printf("VC: 0x%02x  ", V[0xC]);
                    printf("VD: 0x%02x  ", V[0xD]);
                    printf("VE: 0x%02x  ", V[0xE]);
                    printf("VF: 0x%02x  ", V[0xF]);
                    printf("I:  0x%03x  ", I);
                    printf("SP: 0x%02x  ", SP);
                    printf("DT: 0x%02x  ", DT);
                    printf("ST: 0x%02x\n", ST);
                    // */

                    switch ((opcode & 0xF000) >> 12) {
                        case 0x0:
                            switch (opcode & 0x0FFF) {
                                case 0x0E0:
                                    for (int i = 0; i < 32; i++) {
                                        vmemory[i] = 0;
                                    }
                                    break;

                                case 0x0EE:
                                    PC = stack[SP];
                                    SP--;
                                    break;
                            }
                            break;

                        case 0x1:
                            PC = opcode & 0xFFF - 2;
                            break;

                        case 0x2:
                            SP++;
                            stack[SP] = PC;
                            PC = opcode & 0xFFF - 2;
                            break;

                        case 0x3:
                            if (V[(opcode & 0xF00) >> 8] == (opcode & 0x0FF)) {
                                PC += 2;
                            }
                            break;

                        case 0x4:
                            if (V[(opcode & 0xF00) >> 8] != (opcode & 0x0FF)) {
                                PC += 2;
                            }
                            break;

                        case 0x5:
                            if (V[(opcode & 0xF00) >> 8] == V[(opcode & 0x0F0) >> 4]) {
                                PC += 2;
                            }
                            break;

                        case 0x6:
                            V[(opcode & 0xF00) >> 8] = opcode & 0x0FF;
                            break;

                        case 0x7:
                            V[(opcode & 0xF00) >> 8] += opcode & 0x0FF;
                            break;

                        case 0x8:
                            switch (opcode & 0xf) { // last nibble is interpreted because yes
                                case 0x0:
                                    V[(opcode & 0xF00) >> 8] = V[(opcode & 0x0F0) >> 4];
                                    break;

                                case 0x1:
                                    V[(opcode & 0xF00) >> 8] |= V[(opcode & 0x0F0) >> 4];
                                    break;

                                case 0x2:
                                    V[(opcode & 0xF00) >> 8] &= V[(opcode & 0x0F0) >> 4];
                                    break;

                                case 0x3:
                                    V[(opcode & 0xF00) >> 8] ^= V[(opcode & 0x0F0) >> 4];
                                    break;

                                case 0x4:
                                    V[0xF] = (
                                        (uint16_t)V[(opcode & 0xF00) >> 8] +
                                        (uint16_t)V[(opcode & 0x0F0) >> 4]
                                    ) > 0xFF;
                                    V[(opcode & 0xF00) >> 8] += V[(opcode & 0x0F0) >> 4];
                                    break;

                                case 0x5:
                                    V[0xF] = (
                                        V[(opcode & 0xF00) >> 8] >
                                        V[(opcode & 0x0F0) >> 4]
                                    );
                                    V[(opcode & 0xF00) >> 8] -= V[(opcode & 0x0F0) >> 4];
                                    break;

                                case 0x6:
                                    V[0xF] = V[(opcode & 0xF00) >> 8] & 0b1;
                                    V[(opcode & 0xF00) >> 8] >>= 1;
                                    break;

                                case 0x7:
                                    V[0xF] = (
                                        V[(opcode & 0xF00) >> 8] >
                                        V[(opcode & 0x0F0) >> 4]
                                    );
                                    V[(opcode & 0xF00) >> 8] = V[(opcode & 0x0F0) >> 4] - V[(opcode & 0xF00) >> 8];
                                    break;

                                case 0xE:
                                    V[0xF] = V[(opcode & 0xF00) >> 8] >> 7;
                                    V[(opcode & 0xF00) >> 8] <<= 1;
                                    break;
                            }
                            break;

                        case 0x9:
                            if (V[(opcode & 0xF00) >> 8] != V[(opcode & 0x0F0) >> 4]) {
                                PC += 2;
                            }
                            break;

                        case 0xA:
                            I = opcode & 0x0FFF;
                            break;

                        case 0xB:
                            PC = (opcode & 0x0FFF) + V[0] - 2;
                            break;

                        case 0xC:
                            V[(opcode & 0x0F00) >> 8] = rand() & (opcode & 0x00FF);
                            break;

                        case 0xD:
                            for (int offset = 0; offset < (opcode & 0x00F); offset++) {
                                // stops drawing if the line is out of screen
                                if ((((opcode & 0x0F0) >> 4) + offset) >= 32) break;

                                // ok so what the FUCK is going on here?
                                // firstly checks for colisions aka bitwise and's the sprite line by
                                // line with existing screen contents then flattens the result to plain 1 or 0
                                V[0xF] = (memory[I + offset] & (uint8_t)(vmemory[((opcode & 0x0F0) >> 4) + offset] >> (63 - ((opcode & 0xF00) >> 8)))) != 0;

                                // secondly it actually draws the sprite line to the screen buffer
                                vmemory[((opcode & 0x0F0) >> 4) + offset] ^= (uint64_t)memory[I + offset] << (63 - ((opcode & 0xF00) >> 8));

                            }
                            break;

                        case 0xE:
                            switch (opcode & 0xff) {
                                case 0x9E:
                                    // skip next instruction if key stored in 0xf00 is pressed
                                    if (key[chip_keys[(opcode & 0xF00) >> 8]]) PC += 2;
                                    break;

                                case 0xA1:
                                    // skip next instruction if key stored in 0xf00 is not pressed
                                    if (!key[chip_keys[(opcode & 0xF00) >> 8]]) PC += 2;
                                    break;
                            }
                            break;

                        case 0xF:
                            switch (opcode & 0xff) {
                                case 0x07:
                                    V[(opcode & 0x0F00) >> 8] = DT;
                                    break;

                                case 0x0A:
                                    // hangs the program until a key is pressed
                                    // the key is then stored in 0xf00
                                    // (then unhanging happens bellow in the main event switch)
                                    hang = true;
                                    break;

                                case 0x15:
                                    DT = V[(opcode & 0x0F00) >> 8];
                                    break;

                                case 0x18:
                                    ST = V[(opcode & 0x0F00) >> 8];
                                    break;

                                case 0x1E:
                                    I += V[(opcode & 0x0F00) >> 8];
                                    break;

                                case 0x29:
                                    I = V[(opcode & 0xF00) >> 8] * 0x05;
                                    break;

                                case 0x33:
                                    memory[I]     = V[(opcode & 0xF00) >> 8] / 100;
                                    memory[I + 1] = V[(opcode & 0xF00) >> 8] % 100 / 10;
                                    memory[I + 2] = V[(opcode & 0xF00) >> 8] % 10;
                                    break;

                                case 0x55:
                                    for (int offset = 0x0; offset <= ((opcode & 0x0F00) >> 8); offset++) {
                                        memory[I + offset] = V[offset];
                                    }
                                    break;

                                case 0x65:
                                    for (int offset = 0x0; offset <= ((opcode & 0x0F00) >> 8); offset++) {
                                        V[offset] = memory[I + offset];
                                    }
                                    break;
                            }
                            break;

                    }

                    if (DT) DT--;
                    if (ST) ST--;
                    PC += 2;
                }

                redraw = true;
                break;

            case ALLEGRO_EVENT_KEY_DOWN:
                for (uint8_t i = 0; i < 16; i++) {
                    if (chip_keys[i] == event.keyboard.keycode) {
                        V[(opcode & 0x0F00) >> 8] = i;
                        hang = false;
                        break;
                    }
                }
                break;

            case ALLEGRO_EVENT_DISPLAY_CLOSE:
                // the X button is pressed in the window's titlebar (or <A-f4> is pressed)
                done = true;
                break;
        }

        if (done) break;
        if (PC > 0xFFF) {
            printf("Program Counter out of bounds\nShould be between 0x000 and 0xFFF, is at 0x%03x\n", PC);
            status = 1;
            break;
        }

        keyboard_update(&event);

        // redraw only if both the draw flag is set and the queue is empty
        if (redraw && al_is_event_queue_empty(queue)) {
            al_set_target_bitmap(buffer); // set the internal res backbuffer here

            // code to draw from the VM's video memory goes here
            al_clear_to_color(al_map_rgb(0x12, 0x12, 0x12)); // draw all unset pixels
            for (int y = 0; y < 32; y++) {
                for (int x = 0; x < 64; x++) {
                    if ((vmemory[y] >> (63 - x)) & 0b1) {
                        al_put_pixel(x, y, al_map_rgb(0xEE, 0xEE, 0xEE));
                    }
                }
            }

            // set tell Allegro to draw to the scaled backbuffer
            // then scale and draw the native backbuffer to it
            al_set_target_backbuffer(disp);
            al_draw_scaled_bitmap(buffer, 0, 0, BUFFER_W, BUFFER_H, 0, 0, DISP_W, DISP_H, 0);

            al_flip_display(); // flip the OGL buffers
            redraw = false; // unset the draw flag
        }
    }

    // just general cleanup, y'know, destroying the display and event queue and all that
    al_destroy_bitmap(buffer);
    al_destroy_display(disp);

    al_destroy_event_queue(queue);
    al_destroy_timer(timer);

    return status;
}

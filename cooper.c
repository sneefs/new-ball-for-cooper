#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/sleep.h>
#include <util/delay.h>
#include <stdlib.h>

/* Three LEDs.. */
#define RED_LED 0
#define GRN_LED 1
#define BLU_LED 2

/* ..connected in order to ATTiny pins PB0, PB1 and PB4. */
#define RED_PIN 0
#define GRN_PIN 1
#define BLU_PIN 4

/* In PWM mode, we simply put desired match levels into these registers. */
volatile uint8_t* CompReg[] = {&OCR0A, &OCR0B, &OCR1B};

volatile uint8_t hi_rgb[3];
volatile uint8_t lo_rgb[3];

// Sets leds to specified levels 0 (off) to 255 (max)
void set_duty_cycles(volatile uint8_t rgb[3]){
    *CompReg[RED_LED] = 255 - rgb[RED_LED];
    *CompReg[GRN_LED] = 255 - rgb[GRN_LED];
    *CompReg[BLU_LED] = 255 - rgb[BLU_LED];
}

// just write out the 6 permutations of {0,1,2}
const uint8_t perms[] = {0x24, 0x21, 0x12, 0x18, 0x09, 0x06};

void permute(uint8_t i, volatile uint8_t vals[3])
{
    uint8_t j, k;
    uint8_t perm = perms[i % 6];
    i = vals[0];
    j = vals[1];
    k = vals[2];
    vals[perm & 3] = i;
    vals[(perm >> 2) & 3] = j;
    vals[perm >> 4] = k;
}

void setup_pwm(void)
{
    // Set output mode on our 3 pins
    DDRB = 1<<RED_PIN | 1<<GRN_PIN | 1<<BLU_PIN;

    // Configure counter/timer0 for fast PWM on PB0 and PB1
    TCCR0A = 3<<COM0A0 | 3<<COM0B0 | 3<<WGM00;
    TCCR0B = 0<<WGM02 | 3<<CS00;

    // Configure counter/timer1 for fast PWM on PB4
    GTCCR = 1<<PWM1B | 3<<COM1B0;
    TCCR1 = 3<<COM1A0 | 7<<CS10;
}

uint8_t random_color(void)
{
    // Generate a random RGB color value,
    // with the constraint that red+green+blue values == total
    // OK just say total == 255 to avoid 16bit math bloat.
    int16_t r =  rand() & 0x7FFF;
    uint8_t rnum = r & 0xff;
    hi_rgb[0] = rnum;
    hi_rgb[1] = (r >> 7) % (256 - rnum);
    hi_rgb[2] = 255 - hi_rgb[1] - rnum;
    permute(rnum, hi_rgb);
    lo_rgb[0] = hi_rgb[0] / 8;
    lo_rgb[1] = hi_rgb[1] / 8;
    lo_rgb[2] = hi_rgb[2] / 8;
    return rnum;
}


////////////////////////////////////////////////////////////////////////////////
// Effect 1:
// Make some nice colors (without worrying much).
//
// I figured, with my leds not calibrated and all, forget about color spaces and
// whatnot. Why not just have a pulsing "swish" thing - one second during which
// all LEDs ramp up to full brightness and back to zero in some smooth pulsing
// way.  Staggering them in time - like this http://sneef.net/swish.png - should
// cycle through quite a few RGB combinations, especially with 6 different
// swishes from the permutations of {R,G,B} led going off 1st, 2nd, 3rd.
// (Plus, total brightness will be a similar smooth curve that way).
//
// "Brightness" at time t = 0..49, retardation = 0, 1, 2
uint8_t get_duty_cycle(uint8_t t, uint8_t retardation)
{
    // These 50 values here (for 1 second at 50Hz) are just sin(t)^2
    // for the first led. Delay by 8 and 16 values for the second and
    // third led respectively.
    const uint8_t sin2[] = {
        0x00, 0x02, 0x08, 0x12, 0x20, 0x31, 0x44, 0x5a, 0x70, 0x88,
        0x9f, 0xb5, 0xc9, 0xdb, 0xe9, 0xf5, 0xfc, 0xff, 0xfe, 0xf9,
        0xf0, 0xe4, 0xd3, 0xc1, 0xac, 0x95, 0x7e, 0x67, 0x51, 0x3c,
        0x29, 0x1a, 0x0d, 0x05, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    };
    retardation = 50 - 8 * retardation;
    return sin2[(retardation + t) % 50];
}

// Do one swish (p selects one of 6 possible swishes).
void swish(uint8_t p)
{
    for(uint8_t i=0; i<50; i++){
        hi_rgb[RED_LED] = get_duty_cycle(i, 0);
        hi_rgb[GRN_LED] = get_duty_cycle(i, 1);
        hi_rgb[BLU_LED] = get_duty_cycle(i, 2);
        permute(p, hi_rgb);
        set_duty_cycles(hi_rgb);
        _delay_ms(25); // 50Hz-ish; don't care that computation also adds time
    }
}


////////////////////////////////////////////////////////////////////////////////
// Effect 2:
// Slowly transition slowly to a new random color.
//
// Just do a linear interpolation. Intermediate values will lie on the same
// plane of r+g+b=total "brightness" as the start and end random_color() values.
uint8_t x_t(uint8_t i, uint8_t C, uint8_t N) {
    // computes i*C/N
    uint16_t tmp = C;
    tmp *= i;
    tmp /= N;
    return tmp;
}
// uint8_t x_t(uint8_t i, uint8_t C, uint8_t N) {
//     // computes i*C/N while staying inside uint8 bounds
//     uint8_t lg = C / N;
//     uint8_t sm = C % N;
//     lg *= i;
//     if (N>16) {
//         sm = x_t(i/2, sm, N/2);
//     } else {
//         sm *= i;
//         sm /= N;
//     }
//     return lg + sm;
// }
uint8_t col_t(uint8_t i, uint8_t a, uint8_t b, uint8_t N) {
    return (b>a) ? a + x_t(i, b-a, N) : a - x_t(i, a-b, N);
}

// go from current hi_rgb to a new random color in nsteps
uint8_t transition_to_random(uint8_t nsteps, uint8_t stepdelay_small)
{
    uint8_t old_r, old_g, old_b;
    uint8_t new_r, new_g, new_b;
    uint8_t rnum;

    old_r = hi_rgb[0];
    old_g = hi_rgb[1];
    old_b = hi_rgb[2];

    rnum = random_color();
    new_r = hi_rgb[0] / 2;
    new_g = hi_rgb[1] / 2;
    new_b = hi_rgb[2] / 2;

    for(uint8_t i=1; i<=nsteps; i++) {
        hi_rgb[0] = col_t(i, old_r, new_r, nsteps);
        hi_rgb[1] = col_t(i, old_g, new_g, nsteps);
        hi_rgb[2] = col_t(i, old_b, new_b, nsteps);
        set_duty_cycles(hi_rgb);
        if (stepdelay_small)
            _delay_ms(70);
        else
            _delay_ms(100);
    }
    return rnum;
}


////////////////////////////////////////////////////////////////////////////////
// Effect 3:
// Spell out "Cooper" in morse code: -.-. --- --- .--. . .-.
//
#define MORSE_DOT_MS 70
#define MORSE_BREAK_MS 70
#define MORSE_DASH_MS 210
#define MORSE_PAUSE_MS 210

#define NLETTERS 6   // six letters in "Cooper"
#define NEFFECTS 3   // morse in three different styles
#define MAXPOS 252   // (255 / NLETTERS / NEFFECTS) * NLETTERS * NEFFECTS

uint8_t morse_pos = 0; // counts each morsed letter, wraps at MAXPOS
uint8_t crazy = 0;

// some color picking functions (will morse each letter in a different color)

static void basic_color(uint8_t pos)
{
    // Generate 6 "extreme" colors: one or two individual LEDs on, others off.
    uint8_t col = 127;
    uint8_t val = pos / 2 + pos / 6;
    val = 1 << (val % 3);
    if (pos & 1)
        val = ~val & 0x0f;
    else
        col = 255;
    hi_rgb[0] = (val & 1) ? col : 0;
    hi_rgb[1] = (val & 2) ? col : 0;
    hi_rgb[2] = (val & 4) ? col : 0;
    lo_rgb[0] = 0;
    lo_rgb[1] = 0;
    lo_rgb[2] = 0;
}

// get color to morse next letter in, cycling through different styles
void next_color(void)
{
    crazy = 0;
    switch (morse_pos/NLETTERS % NEFFECTS)
    {
      case 0:
          // blink white for all letters
          hi_rgb[RED_LED] = 255;
          hi_rgb[GRN_LED] = 255;
          hi_rgb[BLU_LED] = 255;
          lo_rgb[RED_LED] = 64;
          lo_rgb[GRN_LED] = 64;
          lo_rgb[BLU_LED] = 64;
          break;
      case 1:
          // pick a random color for each dot or dash
          random_color();
          crazy = 1;
          break;
      case 2:
          // use color "extremes" (only one or two LEDs on)
          basic_color(morse_pos);
          break;
    }
    morse_pos = (morse_pos+1) % MAXPOS;
}

#define morse_on() (set_duty_cycles(hi_rgb))
#define morse_off() (set_duty_cycles(lo_rgb))

// (In "crazy mode" use different color for every single dot or dash)
void crazy_color(void) { if (crazy) random_color(); }

#define morse_pause() (                         \
        next_color(), _delay_ms(MORSE_PAUSE_MS))
#define morse_dot() (                           \
        morse_on(),  _delay_ms(MORSE_DOT_MS),   \
        morse_off(), _delay_ms(MORSE_BREAK_MS), \
        crazy_color())
#define morse_dash() (                          \
        morse_on(),  _delay_ms(MORSE_DASH_MS),  \
        morse_off(), _delay_ms(MORSE_BREAK_MS), \
        crazy_color())

void cooper(void) {
    // Encoded "cooper" in 4 bytes, using a kind-of Huffman code
    // where dash  =  0
    //       dot   = 01
    //       pause = 11
    uint32_t cooper = 4002296228;
    morse_pause();

    // Now decode it
    for (uint8_t i=0; i<22; i++)
      (cooper >>= 1, ~cooper & 1) ? morse_dash() :
        (cooper >>= 1, ~cooper & 1) ? morse_dot() :
          morse_pause();
    morse_dot(); // final dot didn't fit ;)
}


////////////////////////////////////////////////////////////////////////////////
// Code to enter sleep mode, and wake up on a pin change
//
ISR(PCINT0_vect) {
    // Interrupt handler for the pin change that wakes us from
    // powersave mode. Nothing to do, the program will just pick up
    // right after the call to sleep_cpu()
}

void power_down_until_PB3_pinchange(void)
{
    GIMSK |= 1<<PCIE; // pin change interrupt enable
    PCMSK |= 1<<PCINT3; // (on pin PB3)

    // put CPU to sleep
    set_sleep_mode(SLEEP_MODE_PWR_DOWN);
    sleep_enable();
    sleep_bod_disable();
    sei(); // enable interrupts
    sleep_cpu();

    // hola, someone woke us up with a pin change on PB3
    sleep_disable();
    GIMSK &= ~(1<<PCIE); // disable interrupts again
    PCMSK &= ~(1<<PCINT3);
    cli();
}


////////////////////////////////////////////////////////////////////////////////
//
int main(void)
{
    uint8_t rnum, j;
    uint8_t swish_pos = 0;

    cli(); // clear global interrupt mask - disables all interrupts
    setup_pwm();

    hi_rgb[0] = 0;
    hi_rgb[1] = 0;
    hi_rgb[2] = 0;

    while(1) {
        set_duty_cycles(hi_rgb);
        transition_to_random(16, 1);

        for (uint8_t i=0; i<10; i++) {
            transition_to_random(64, 0);
            transition_to_random(64, 0);
            transition_to_random(64, 0);
            transition_to_random(64, 0);
            transition_to_random(64, 0);
            transition_to_random(64, 0);
            for (j=0; j<100; j++) {
                transition_to_random(1, 1);
            }
            transition_to_random(64, 0);
            transition_to_random(64, 0);
            rnum = transition_to_random(64, 0);

            for (j=0; j<(rnum%6)+1; j++) {
                swish(swish_pos++);
            }
            _delay_ms(100);

            cooper();
        }
        hi_rgb[0] = 0;
        hi_rgb[1] = 0;
        hi_rgb[2] = 0;
        set_duty_cycles(hi_rgb);
        _delay_ms(50);
        power_down_until_PB3_pinchange();
    }
    return 0;
}

// Compiling: cooper.c
// avr-gcc -c -I. -g -O3                    -funsigned-char -funsigned-bitfields -fpack-struct -fshort-enums -Wall -Wstrict-prototypes -DF_CPU=1000000      -Wa,-adhlns=cooper.lst  -mmcu=attiny45 -std=gnu99 cooper.c -o cooper.o
//
// Linking: cooper.elf
// avr-gcc -I. -g -O3                       -funsigned-char -funsigned-bitfields -fpack-struct -fshort-enums -Wall -Wstrict-prototypes -DF_CPU=1000000      -Wa,-adhlns=cooper.o  -mmcu=attiny45 -std=gnu99 cooper.o --output cooper.elf -Wl,-Map=.map,--cref
//
// Creating load file for Flash: cooper.hex
// avr-objcopy -O ihex              -R .eeprom cooper.elf cooper.hex
// rm cooper.o cooper.elf

// sudo avrdude -c usbtiny -p t45 -U flash:w:cooper.hex

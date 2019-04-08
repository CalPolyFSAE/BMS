/* System */
#include "pin_mux.h"
#include "clock_config.h"
#include "MKE18F16.h"
#include "fsl_debug_console.h"

/* BSP */
#include "System.h"
#include "gpio.h"
#include "spi.h"

/* Project */
#include "defines.h"
#include "ltcutility.h"
#include "utility.h"
#include "drivers.h"
#include "cache.h"
#include "actions.h"

using namespace BSP;

//gpio::GPIO_port ltccsport = gpio::PortE;
//uint8_t ltccspin = 6;

cache_t cache = {0};

volatile uint32_t ticks;
volatile uint32_t delayticks;
volatile uint8_t pin = 0;

void delay(uint32_t x){
    delayticks = x;
    while(delayticks);
}

void tick(void){
}

void switchMux(uint8_t mux, uint8_t pin){
    uint8_t addr = 0x90 | (mux<<1);
    uint8_t comm = 0x08 | pin;
    uint8_t data[6] = { 0x60, 0x08, 0x00, 0x09, 0x70, 0x09 };
    data[0] |= (addr>>4)&0x0f;
    data[1] |= (addr<<4)&0xf0;
    data[3] |= (comm<<4)&0xf0;
    bms::wrcomm(data);
    bms::wait();
    delay(3);
    bms::stcomm(2);
    bms::wait();
}

typedef void (*state_t)(void);
static state_t state;
void stateNormal(void);
void stateError(void);

uint8_t manage(void){

    // read volts
    
    bms::rdauxa();
    bms::wait();
    delay(1);
    bms::rdcva();
    bms::wait();
    delay(1);
    bms::rdcvb();
    bms::wait();
    delay(1);
    bms::rdcvc();
    bms::wait();
    delay(1);
    bms::rdcvd();
    bms::wait();
    delay(1);

    // start adc conversion to be read on next cycle...
    // registers cleared when watchdog times out, approx 2s
    bms::adcvax();
    bms::wait();

    uint16_t rxmask = {0};
    for(uint8_t i = 0; i < cells; i++){
        rxmask |= 1<<i;
    }
    uint8_t done = 0;
    while(1){
        done = 0;
        for(uint8_t i = 0; i < slaves; i++)
            if(cache.adcRxMask[i] == rxmask) done++;
        if(done==slaves){ 
            diagnoseVolts();
            break;
        }
        if(ticks == 0) return 0;
    }

    if(!cache.allok) {
        return 1;
    }

    // read temps
    
/*    delay(ms(2));
    //pin = (pin+1)%8;
    pin = (pin+1)%8;
    switchMux(1, pin);
*/
    return 0;
    
}


void stateNormal(void){

    while(ticks);
    ticks = ms(1000);
    if(manage()){
       state = stateError;
       return;
    }

}

void bmsNotOk(void){
    // turn off ok signal
}

void stateError(void){

    bmsNotOk();

    while(ticks);
    ticks = ms(3000);

    manage();

}

uint32_t minticks;

void stateI2CTest(void){

    ticks = ms(500);
    while(ticks);
    uint8_t data[6] = { 0x69, 0x28, 0x00, 0x89, 0x70, 0x09 };
    bms::wrcomm(data);
    bms::wait();
    ticks = 3;
    while(ticks);
    bms::stcomm(2);
    bms::wait();
    bms::rdcomm();
    bms::wait();

}

void stateI2CSwitchTest(void){

    ticks = ms(500);
    while(ticks);
    pin = (pin+1)%8;
    switchMux(1, pin);

}

int main(void) {
    BOARD_InitBootPins();
    BOARD_InitBootClocks();
    SysTick_Config(SYSTICK);

    gpio::GPIO::set(ltccsport, ltccspin);
    bms::init();
    initspi();
    actinit();

    action_t a;
    a.a = wait;
    a.wait = ms(500);
    actadd(a);
    a.a = csdown;
    actadd(a);
    a.a = wait;
    a.wait = ms(3);
    actadd(a);
    a.a = csup;
    actadd(a);
    a.a = spitx;
    a.data[0] = 0xaa;
    a.len = 3;
    a.comm = bms::PLADC;
    actadd(a);

    while(1){
        actexec();
    }

    memset(&cache, 0, sizeof(cache_t));

    cache.allok = 1;
    cache.voltok = 1;
    cache.tempok = 1;

    ticks = 0;

    bms::init();
    initspi();

    state = stateNormal;
    
    ticks = ms(500);
    minticks = 0;
    pin = 0;

    // setup: 
    // - set refup to 1 to keep reference voltage on
    // - start ADC conversion and wait for completion
    uint8_t confdat[6];
    memset(confdat, 0, 6);
    confdat[0] = 0xfc;
    bms::adcvax();
    bms::wait();
    delay(ms(2));

    while(1){

        // repeat process at 2Hz
        ticks = ms(500);
        while(ticks);

        bms::wrcfga(confdat);
        bms::wait();
        bms::rdcfga();
        bms::wait();

        pin = (pin+1)%8;
        switchMux(1, pin);

        // wait for mux to settle, then trigger conversion
        delay(ms(50));
        bms::adcvax();
        // wait for conversion to finish, then read from auxa register
        delay(ms(10));

        bms::rdauxa();
        bms::wait();
        delay(1);
        bms::rdcva();
        bms::wait();
        delay(1);
        bms::rdcvb();
        bms::wait();
        delay(1);
        bms::rdcvc();
        bms::wait();
        delay(1);
        bms::rdcvd();
        bms::wait();
        delay(1);
        uint16_t rxmask = {0};
        for(uint8_t i = 0; i < cells; i++){
            rxmask |= 1<<i;
        }
        uint8_t done = 0;
        while(1){
            done = 0;
            for(uint8_t i = 0; i < slaves; i++)
                if(cache.adcRxMask[i] == rxmask) done++;
            if(done==slaves){ 
                diagnoseVolts();
                break;
            }
            if(ticks == 0) break;
        }
    }
        
    return 0;
}

extern "C" {
void SysTick_Handler(void){
//    if(ticks) ticks--;
///    if(delayticks) delayticks--;
//    bms::tick();
    acttick();
}
}
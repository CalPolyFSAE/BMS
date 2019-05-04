#include "bmsutil.h"
#include "cache.h"

extern cache_t cache;

void update();
// todo: insert in place
// todo: don't let actions happen until inserting is done
uint8_t wakeup(void){
    if(actqcheck(4*slaves)) return 1;

    for(uint8_t i = 0; i < slaves; i++){
        actcsdown();
        actwait(us(400));
        actcsup();
        actwait(us(100));
/*
        // inserted in reverse order 
        action_t a;
        a.a = wait;
        a.wait = us(100);
        actinsert(a);
        a.a = csup;
        actinsert(a);
        a.a = wait;
        a.wait = us(400);
        actinsert(a);
        a.a = csdown;
        actinsert(a);
*/
    }

    return 0;
}

uint8_t readall(void){
    
    bms::adcvax();
    actwait(ms(50));
    wakeup();
    bms::rdcva();
    bms::rdcvb();
    bms::rdcvc();
    bms::rdcvd();
    bms::rdauxa();
    actexecute(voltCheck);
    actexecute(getTemp);

    return 0;
}

void voltCheck(void){

    uint32_t vmax = 0;
    uint32_t vmin = ~0;
    uint32_t vtot = 0;
    uint32_t vavg = 0;

    uint16_t i;
    uint16_t n = slaves * cells;

    for(i = 0; i < n; i++){

        if(cache.volts[i] > vmax) vmax = cache.volts[i];
        if(cache.volts[i] < vmin) vmin = cache.volts[i];
        vtot += cache.volts[i];

    }

    vavg = vtot/n;

    cache.voltageMax = vmax;
    cache.voltageMin = vmin;
    cache.voltageTotal = vtot;
    cache.voltageMean = vavg;

    if(vmax > voltageLimitUpper) {
        cache.voltError();
        update();
    } else if(vmin < voltageLimitLower) {
        cache.voltError();
        update();
    } else {
        cache.voltGood();
    }

}

void stepMux(){
    if(cache.muxpin > (cache.muxpin+1)%8){
        muxOff(cache.mux);
        muxOff(cache.mux);
        muxOff(cache.mux);
        cache.mux = (cache.mux+1)%2;
    }
    cache.muxpin = (cache.muxpin+1)%8;
    muxOn(cache.mux, cache.muxpin);
    muxOn(cache.mux, cache.muxpin);
}

void muxOn(uint8_t mux, uint8_t pin){
    uint8_t addr = 0x90 | (mux<<1);
    uint8_t comm = 0x08 | pin;
    uint8_t data[6] = { 0x60, 0x08, 0x00, 0x09, 0x70, 0x09 };
    data[0] |= (addr>>4)&0x0f;
    data[1] |= (addr<<4)&0xf0;
    data[3] |= (comm<<4)&0xf0;
    bms::wrcomm(data);
    bms::stcomm(2);
    bms::rdcomm();
}

void muxOff(uint8_t mux){
    uint8_t addr = 0x90 | (mux<<1);
    uint8_t comm = 0x00;
    uint8_t data[6] = { 0x60, 0x08, 0x00, 0x09, 0x70, 0x09 };
    data[0] |= (addr>>4)&0x0f;
    data[1] |= (addr<<4)&0xf0;
    data[3] |= (comm<<4)&0xf0;
    bms::wrcomm(data);
    bms::stcomm(2);
    bms::rdcomm();
    actwait(ms(10));
}

void getTemp(){

    uint8_t tempid = (cache.mux<<3)|cache.muxpin; // binary: 0000mppp
    for(uint8_t i = 0; i < slaves; i++){
        cache.temps[(thermistors*i)+tempid] = cache.gpio[5*i];
        if(cache.tempArray[i] & 1<<tempid){ // thermistor is cell, not aux
            tempCheck(i, tempid);
        }
    }

    static uint16_t tempsRead = 0;
    tempsRead+=slaves;
    if(tempsRead == thermistors*slaves){
        uint16_t totalWorking = 0;
        uint16_t totalGood = 0;
        if(cache.tempBroken <= tempExtra){
            for(uint16_t i = 0; i < slaves; i++){
                for(uint16_t j = 0; j < thermistors; j++){
                    if(cache.tempArray[i] & 1<<j){
                        totalWorking++;
                        if(cache.tempGood[i] & 1<<j){
                            totalGood++;
                        }
                    }
                }
            }
            if(totalWorking == totalGood){
                cache.tempOk();
            } else {
                cache.tempError();
                update();
            }
        } else {
            cache.tempError();
            update();
        }
        tempsRead = 0;
    }
}
    
uint8_t tempCheck(uint8_t slave, uint8_t id){
    uint16_t temp = cache.temps[(slave*thermistors)+id];
    if(temp > tempBrokenUpper || temp < tempBrokenLower){
        cache.tempBroken++;
        cache.tempArray[slave] &= ~(1<<id);
        cache.tempGood[slave] &= ~(1<<id);
        return 1;
    } else if(temp > tempLimitUpper || temp < tempLimitLower) {
        cache.tempGood[slave] &= ~(1<<id);
        cache.tempError();
        update();
        return 1;
    } else {
        cache.tempGood[slave] |= 1<<id;
    }
    return 0;
}

// write standard configuration to all slaves
void setup(){
    uint8_t confdat[6*slaves];
    memset(confdat, 0, 6*slaves);
    for(uint8_t i = 0; i < slaves; i++) confdat[6*i] = 0xfc;
    wakeup();
    bms::wrcfga(confdat);
}
    

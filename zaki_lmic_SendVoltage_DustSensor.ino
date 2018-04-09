/*******************************************************************************
 * Copyright (c) 2015 Thomas Telkamp and Matthijs Kooijman
 *
 * Permission is hereby granted, free of charge, to anyone
 * obtaining a copy of this document and accompanying files,
 * to do whatever they want with them without any restriction,
 * including, but not limited to, copying, modification and redistribution.
 * NO WARRANTY OF ANY KIND IS PROVIDED.
 *
 * This example sends a valid LoRaWAN packet with payload "Hello,
 * world!", using frequency and encryption settings matching those of
 * the (early prototype version of) The Things Network.
 *
 * Note: LoRaWAN per sub-band duty-cycle limitation is enforced (1% in g1,
 *  0.1% in g2).
 *
 * Change DEVADDR to a unique address!
 * See http://thethingsnetwork.org/wiki/AddressSpace
 *
 * Do not forget to define the radio type correctly in config.h.
 *
 * 29 December 2016 Modified by Zaki to cater for RF95 + Arduino 
 *******************************************************************************/

#include <SPI.h>
#include <lmic.h>
#include <hal/hal.h>

/* For Voltage Sensor */
volatile float averageVcc = 0.0;

/* For Dust Sensor */
#define DUST_SENSOR_DIGITAL_PIN_PM10  3

long concentrationPM10 = 0;
long ppmv;
int temperature=30; //external temperature, if you can replace this with a DHT11 or better 
float valDUSTPM25 =0.0;
float lastDUSTPM25 =0.0;
float valDUSTPM10 =0.0;
float lastDUSTPM10 =0.0;
unsigned long duration;
unsigned long starttime;
unsigned long endtime;
unsigned long sampletime_ms = 30000;
unsigned long lowpulseoccupancy = 0;
float ratio = 0;
/******************************************/

/************************************* OTAA Section : must fill these ******************************************************************/
/* This EUI must be in little-endian format, so least-significant-byte
   first. When copying an EUI from ttnctl output, this means to reverse
   the bytes. For TTN issued EUIs the last bytes should be 0xD5, 0xB3, 0x70. */
   
//static const u1_t PROGMEM APPEUI[8]={ 0x5E, 0x1D, 0x00, 0xF0, 0x7E, 0xD5, 0xB3, 0x70 };
/*Eg : 70B3D57EF0001D5E as got from TTN dashboard*/
//void os_getArtEui (u1_t* buf) { memcpy_P(buf, APPEUI, 8);}

/* This should also be in little endian format, see above. */
//static const u1_t PROGMEM DEVEUI[8]={ 0x69, 0x32, 0x30, 0x89, 0x05, 0xE4, 0x54, 0x00 };
/*Eg : 0054E40589303269 as got from TTN dashboard */
//void os_getDevEui (u1_t* buf) { memcpy_P(buf, DEVEUI, 8);}

/* This key should be in big endian format (or, since it is not really a
   number but a block of memory, endianness does not really apply). In
   practice, a key taken from ttnctl can be copied as-is.
   The key shown here is the semtech default key. */
//static const u1_t PROGMEM APPKEY[16] = { 0xFF, 0xDF, 0x9C, 0x9A, 0x03, 0x55, 0xFF, 0xAC, 0xA3, 0xD7, 0xDA, 0x52, 0x02, 0xC8, 0xFC, 0xE2 };
/*Eg : FFDF9C9A0355FFACA3D7DA5202C8FCE2 as got from TTN dashboard */
//void os_getDevKey (u1_t* buf) {  memcpy_P(buf, APPKEY, 16);}

/**************************************** OTAA Section Ends *********************************************************************************/


/********************************* ABP Section : Need to fill these *************************************************************************/
/* LoRaWAN NwkSKey, network session key
   This is the default Semtech key, which is used by the prototype TTN
   network initially. */
static const PROGMEM u1_t NWKSKEY[16] = { 0x65, 0x76, 0x29, 0x93, 0xED, 0x0C, 0x04, 0xD5, 0x67, 0x0E, 0x7A, 0xBF, 0x2E, 0xDF, 0xE0, 0x52 };
/*Eg : D29EA733C5C5FF7ACE85FC5F62AABB5D as got from TTN dashboard*/

/* LoRaWAN AppSKey, application session key
   This is the default Semtech key, which is used by the prototype TTN
   network initially. */
static const u1_t PROGMEM APPSKEY[16] = { 0x20, 0x0C, 0xDC, 0xF1, 0x05, 0x6A, 0xF9, 0x51, 0xA7, 0x47, 0x3B, 0x82, 0xAB, 0x69, 0x03, 0xF3 };
/*Eg : D0F130ACC73306B1F67ABF92EF1ACD0F as got from TTN dashboard */

/* LoRaWAN end-device address (DevAddr)
   See http://thethingsnetwork.org/wiki/AddressSpace */
static const u4_t DEVADDR = 0x26021B0E ; // <-- Change this address for every node!
/*Eg : 260219FF as got from TTN dashboard */

/* These callbacks are only used in over-the-air activation, so they are
   left empty here (we cannot leave them out completely unless
   DISABLE_JOIN is set in config.h, otherwise the linker will complain). */
   
void os_getArtEui (u1_t* buf) { }
void os_getDevEui (u1_t* buf) { }
void os_getDevKey (u1_t* buf) { }

/********************************************* ABP Section Ends ***************************************************************************/


//static uint8_t mydata[] = "Hello, mine is ABP!";
static osjob_t sendjob;


/* Schedule TX every this many seconds (might become longer due to duty
   cycle limitations). */
const unsigned TX_INTERVAL = 35;

/* Pin mapping */
const lmic_pinmap lmic_pins = {
    .nss = 10,
    .rxtx = LMIC_UNUSED_PIN,
    .rst = 9,
    .dio = {2, 6, 7},
};

void onEvent (ev_t ev) {
    Serial.print(os_getTime());
    Serial.print(": ");
    switch(ev) {
        case EV_SCAN_TIMEOUT:
            Serial.println(F("EV_SCAN_TIMEOUT"));
            break;
        case EV_BEACON_FOUND:
            Serial.println(F("EV_BEACON_FOUND"));
            break;
        case EV_BEACON_MISSED:
            Serial.println(F("EV_BEACON_MISSED"));
            break;
        case EV_BEACON_TRACKED:
            Serial.println(F("EV_BEACON_TRACKED"));
            break;
        case EV_JOINING:
            Serial.println(F("EV_JOINING"));
            break;
        case EV_JOINED:
            Serial.println(F("EV_JOINED"));
            break;
        case EV_RFU1:
            Serial.println(F("EV_RFU1"));
            break;
        case EV_JOIN_FAILED:
            Serial.println(F("EV_JOIN_FAILED"));
            break;
        case EV_REJOIN_FAILED:
            Serial.println(F("EV_REJOIN_FAILED"));
            break;
            break;
        case EV_TXCOMPLETE:
            Serial.println(F("EV_TXCOMPLETE (includes waiting for RX windows)"));
            if(LMIC.dataLen) 
            {
                 // data received in rx slot after tx
                 Serial.print(F("Received "));
                 Serial.print(LMIC.dataLen);
                 Serial.print(F(" bytes of payload: 0x"));
                 for (int i = 0; i < LMIC.dataLen; i++) {
                    if (LMIC.frame[LMIC.dataBeg + i] < 0x10) 
                    {
                        Serial.print(F("0"));
                    }
                    Serial.print(LMIC.frame[LMIC.dataBeg + i], HEX);
                    
                    if (i==0) //check the first byte
                    {
                      if (LMIC.frame[LMIC.dataBeg + 0] == 0x00)
                      {
                          Serial.print(F(" Yes!!!! "));
                      }
                    }
                    
                 }
                 Serial.println();
            }
            
            /* Schedule next transmission */
            os_setTimedCallback(&sendjob, os_getTime()+sec2osticks(TX_INTERVAL), do_send);
            break;
        case EV_LOST_TSYNC:
            Serial.println(F("EV_LOST_TSYNC"));
            break;
        case EV_RESET:
            Serial.println(F("EV_RESET"));
            break;
        case EV_RXCOMPLETE:
            /* data received in ping slot */
            Serial.println(F("EV_RXCOMPLETE"));
            break;
        case EV_LINK_DEAD:
            Serial.println(F("EV_LINK_DEAD"));
            break;
        case EV_LINK_ALIVE:
            Serial.println(F("EV_LINK_ALIVE"));
            break;
         default:
            Serial.println(F("Unknown event"));
            break;
    }
}

void do_send(osjob_t* j){
    
    static uint8_t message[6];
    
    /* Check if there is not a current TX/RX job running */
    if (LMIC.opmode & OP_TXRXPEND) {
        Serial.println(F("OP_TXRXPEND, not sending"));
    } else {
        /* Prepare upstream data transmission at the next possible time. */
        Serial.println(" ");   

        
        /******* Voltage Sensor ***************/
        averageVcc = averageVcc + (float) readVcc();
        int16_t averageVccInt = round(averageVcc);
        Serial.print(averageVccInt);
        Serial.print(" mV \n"); 
        message[0] = highByte(averageVccInt);
        message[1] = lowByte(averageVccInt);
        /**************************************/

        /************** For Dust Sensor ********************/
        //get PM 1.0 - density of particles over 1 μm.
        concentrationPM10=getPM(DUST_SENSOR_DIGITAL_PIN_PM10);
        Serial.print("PM10: ");
        Serial.println(concentrationPM10);

        //ppmv=mg/m3 * (0.08205*Tmp)/Molecular_mass
        //0.08205   = Universal gas constant in atm·m3/(kmol·K)
        ppmv=(concentrationPM10*0.0283168/100/1000) *  (0.08205*temperature)/0.01;
        Serial.print("ppmv PM10: ");
        Serial.println(ppmv);
        Serial.print("\n");
  
        if ((ceil(concentrationPM10) != lastDUSTPM10)&&((long)concentrationPM10>0)) 
        {           
           lastDUSTPM10 = ceil(concentrationPM10);
           int16_t averageDUSTPM10 = round(lastDUSTPM10);           
           message[2] = highByte(averageDUSTPM10);
           message[3] = lowByte(averageDUSTPM10);
        
           if(ppmv > 0)
           {  
              int16_t averageppmv = round(ppmv);
              message[4] = highByte(averageppmv);
              message[5] = lowByte(averageppmv);
           }
           else
           {
              message[4] = 0;
              message[5] = 0;
           }
            
        }
        else
        {
           message[2] = 0;
           message[3] = 0;
           message[4] = 0;
           message[5] = 0;
        }
        /**************************************/
        
        LMIC_setTxData2(1, message, sizeof(message), 0);        
        Serial.println(F("Packet queued"));
        /*Print Freq being used*/
        Serial.print("Transmit on Channel : ");Serial.println(LMIC.txChnl);
        /*Print mV being supplied*/
        
        delay(1000);
        averageVcc = 0;
                
    }
    /* Next TX is scheduled after TX_COMPLETE event. */
}

void setup() {
    Serial.begin(115200);
    Serial.println(F("Starting"));   

    /*****for dust sensor********/
    pinMode(DUST_SENSOR_DIGITAL_PIN_PM10,INPUT);
    /**************************/
    
    LoraInitialization();  // Do all Lora Init Stuff

    /* Start job */
    do_send(&sendjob);
}

void loop() {   
    os_runloop_once();   
}

long readVcc() {
  long result;
  // Read 1.1V reference against AVcc
  ADMUX = _BV(REFS0) | _BV(MUX3) | _BV(MUX2) | _BV(MUX1);
  delay(2); // Wait for Vref to settle
  ADCSRA |= _BV(ADSC); // Convert
  while (bit_is_set(ADCSRA,ADSC));
  result = ADCL;
  result |= ADCH<<8;
  result = 1126400L / result; // Back-calculate AVcc in mV
  return result;
}


void LoraInitialization(){
  /* LMIC init */
    os_init();
    /* Reset the MAC state. Session and pending data transfers will be discarded. */
    LMIC_reset();

    LMIC_setClockError(MAX_CLOCK_ERROR * 1 / 100);

    /****************** ABP Only uses this section *****************************************/
    /* Set static session parameters. Instead of dynamically establishing a session
       by joining the network, precomputed session parameters are be provided.*/
    #ifdef PROGMEM
    /* On AVR, these values are stored in flash and only copied to RAM
       once. Copy them to a temporary buffer here, LMIC_setSession will
       copy them into a buffer of its own again. */
    uint8_t appskey[sizeof(APPSKEY)];
    uint8_t nwkskey[sizeof(NWKSKEY)];
    memcpy_P(appskey, APPSKEY, sizeof(APPSKEY));
    memcpy_P(nwkskey, NWKSKEY, sizeof(NWKSKEY));
    LMIC_setSession (0x1, DEVADDR, nwkskey, appskey);
    #else
    /* If not running an AVR with PROGMEM, just use the arrays directly */
    LMIC_setSession (0x1, DEVADDR, NWKSKEY, APPSKEY);
    #endif
    /******************* ABP Only Ends ****************************************************/

    /* Disable link check validation */
    LMIC_setLinkCheckMode(0);

    /* Set data rate and transmit power (note: txpow seems to be ignored by the library) */
    //LMIC_setDrTxpow(DR_SF7,14); 
    LMIC_setDrTxpow(DR_SF12,20);  //lowest Datarate possible in 915MHz region
}

long getPM(int DUST_SENSOR_DIGITAL_PIN) {

  starttime = millis();

  while (1) {
  
    duration = pulseIn(DUST_SENSOR_DIGITAL_PIN, LOW);
    lowpulseoccupancy += duration;
    endtime = millis();
    
    if ((endtime-starttime) > sampletime_ms)
    {
        ratio = (lowpulseoccupancy-endtime+starttime)/(sampletime_ms*10.0);  // Integer percentage 0=>100
        long concentration = 1.1*pow(ratio,3)-3.8*pow(ratio,2)+520*ratio+0.62; // using spec sheet curve
        //Serial.print("lowpulseoccupancy:");
        //Serial.print(lowpulseoccupancy);
        //Serial.print("\n");
        //Serial.print("ratio:");
        //Serial.print(ratio);
        //Serial.print("\n");
        //Serial.print("PPDNS42:");
        //Serial.println(concentration);
        //Serial.print("\n");
    
        lowpulseoccupancy = 0;
        return(concentration);    
    }
  }  
}


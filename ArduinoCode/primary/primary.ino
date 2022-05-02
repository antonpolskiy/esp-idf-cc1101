#include <Arduino.h>
#include <cc1101.h> // https://github.com/veonik/arduino-cc1101
#include <ccpacket.h>

// Attach CC1101 pins to their corresponding SPI pins
// Uno pins:
// CSN (SS) => 10
// MOSI => 11
// MISO => 12
// SCK => 13
// GD0 => A valid interrupt pin for your platform (defined below this)

// ATmega2560/1280
#if defined(__AVR_ATmega2560__) || defined(__AVR_ATmega1280__)
#define CC1101Interrupt 4 // Pin 19
#define CC1101_GDO0 19

// Teensy 3.5
#elif defined(__MK64FX512__)
#define CC1101Interrupt 9 // Pin 9
#define CC1101_GDO0 9

// ATmega328
#else
#define CC1101Interrupt 0 // Pin 2
#define CC1101_GDO0 2
#endif

CC1101 radio;

byte syncWord[2] = {199, 10};
bool packetWaiting;
unsigned long lastSend = 0;
unsigned int sendDelay = 1000;

void messageReceived() {
    packetWaiting = true;
}

void setup() {
    Serial.begin(115200);
    radio.init();
    radio.setSyncWord(syncWord);
    radio.setCarrierFreq(CFREQ_433);
    radio.disableAddressCheck();
    radio.setChannel(0);
    radio.setTxPowerAmp(PA_LongDistance);

    bool install = true;
    Serial.print(F("CC1101_PARTNUM "));
    //Serial.println(radio.readReg(CC1101_PARTNUM, CC1101_STATUS_REGISTER));
    byte partnum = radio.readReg(CC1101_PARTNUM, CC1101_STATUS_REGISTER);
    Serial.println(partnum);
    if (partnum != 0) install = false;
    
    Serial.print(F("CC1101_VERSION "));
    //Serial.println(radio.readReg(CC1101_VERSION, CC1101_STATUS_REGISTER));
    byte version = radio.readReg(CC1101_VERSION, CC1101_STATUS_REGISTER);
    Serial.println(version);
    if (version != 20) install = false;

    Serial.print(F("CC1101_MARCSTATE "));
    Serial.println(radio.readReg(CC1101_MARCSTATE, CC1101_STATUS_REGISTER) & 0x1f);

    if (install) {
      Serial.println(F("CC1101 radio initialized."));
    } else {
      Serial.println(F("CC1101 radio not initialized."));
      while(1);
    }
    attachInterrupt(CC1101Interrupt, messageReceived, FALLING);
}

// Get signal strength indicator in dBm.
// See: http://www.ti.com/lit/an/swra114d/swra114d.pdf
int rssi(char raw) {
    uint8_t rssi_dec;
    // TODO: This rssi_offset is dependent on baud and MHz; this is for 38.4kbps and 433 MHz.
    uint8_t rssi_offset = 74;
    rssi_dec = (uint8_t) raw;
    if (rssi_dec >= 128)
        return ((int)( rssi_dec - 256) / 2) - rssi_offset;
    else
        return (rssi_dec / 2) - rssi_offset;
}

// Get link quality indicator.
int lqi(char raw) {
    return 0x3F - raw;
}

void loop() {
    static bool waiting = false;
    static int counter = 0;
    unsigned long now = millis();
    if (now > lastSend + sendDelay && waiting == false) {
        lastSend = now;
        //const char *message = "hello world";
        char message[64];
        sprintf(message, "Hello World %ld", now);
        CCPACKET packet;
        // We also need to include the 0 byte at the end of the string
        packet.length = strlen(message)  + 1;
        strncpy((char *) packet.data, message, packet.length);

        radio.sendData(packet);
        Serial.println(F("Sent packet..."));
        waiting = true;
        counter = 0;
    }

    // Wait for a response from the other party 
    if (packetWaiting) {
        detachInterrupt(CC1101Interrupt);
        packetWaiting = false;
        CCPACKET packet;
        if (radio.receiveData(&packet) > 0) {
            Serial.println();
            Serial.println(F("Received packet..."));
            if (!packet.crc_ok) {
                Serial.println(F("crc not ok"));
            }
            Serial.print(F("lqi: "));
            Serial.println(lqi(packet.lqi));
            Serial.print(F("rssi: "));
            Serial.print(rssi(packet.rssi));
            Serial.println(F("dBm"));

            if (packet.crc_ok && packet.length > 0) {
                Serial.print(F("len: "));
                Serial.println(packet.length);
                Serial.print(F("data: "));
                Serial.println((const char *) packet.data);
            }
            waiting = false;
        }
        attachInterrupt(CC1101Interrupt, messageReceived, FALLING);
    } // end packetWaiting

    if (waiting) {
      delay(1);
      counter++;
      if (counter == 2000) {
        Serial.println("No responce from others");
        waiting = false;
      }
    }
}

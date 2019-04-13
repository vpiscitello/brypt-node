#include <time.h>
#include "lora.hpp"

// Compilation instructions:
// g++ -c -Wall lora.cpp
// g++ main.o -lwiringPi -o lora_app

int main (int argc, char *argv[]) {

    // if (argc < 2) {
    //     printf ("Usage: argv[0] sender|rec [message]\n");
    //     exit(1);
    // }

    wiringPiSetup () ;
    pinMode(ssPin, OUTPUT);
    pinMode(dio0, INPUT);
    pinMode(RST, OUTPUT);

    wiringPiSPISetup(CHANNEL, 500000);

    SetupLoRa();

    /*if (!strcmp("sender", argv[1])) {
        opmodeLora();
        // enter standby mode (required for FIFO loading))
        opmode(OPMODE_STANDBY);

        writeReg(RegPaRamp, (readReg(RegPaRamp) & 0xF0) | 0x08); // set PA ramp-up time 50 uSec

        configPower(23);

        printf("Send packets at SF%i on %.6lf Mhz.\n", sf,(double)freq/1000000);
        printf("------------------\n");

        if (argc > 2)
            strncpy((char *)hello, argv[2], sizeof(hello));

        while(1) {
            txlora(hello, strlen((char *)hello));
            delay(5000);
        }
    } else {

        // radio init
        opmodeLora();
        opmode(OPMODE_STANDBY);
        opmode(OPMODE_RX);
        printf("Listening at SF%i on %.6lf Mhz.\n", sf,(double)freq/1000000);
        printf("------------------\n");
        while(1) {
            receivepacket(); 
            delay(1);
        }

    }*/

	
	opmodeLora();
	opmode(OPMODE_STANDBY);

	writeReg(RegPaRamp, (readReg(RegPaRamp) & 0xF0) | 0x08); // set PA ramp-up time 50 uSec

	configPower(23);

	printf("Send and recieve packets at SF%i on %.6lf Mhz.\n", sf, (double) freq/1000000);
	printf("------------------\n");
	while(1){
		opmode(OPMODE_STANDBY);
		txlora(hello, strlen((char*) hello));
		delay(100);
		clock_t now = clock();
		while(((clock() - now)/CLOCKS_PER_SEC) < 5){
			clock_t t = clock();
			opmode(OPMODE_STANDBY);
			opmode(OPMODE_RX);
			while(((clock() - t)/CLOCKS_PER_SEC) < 0.1);
			receivepacket();
			delay(100);
		}
	}
	

    return (0);
}

#include "./crypto.h"
#include <iostream>
#include <chrono>

typedef std::chrono::high_resolution_clock Clock;

int main() {

	crypto c;

	auto start = Clock::now();

	for (unsigned long long i =0; i < 30000000; i++){
		c.aes_ctr_256_decrypt();	
	}

	auto end = Clock::now();

	std::cout<<"Duration: "<< std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count()
				 	<< " nanoseconds" << std::endl;


	return 0;
}


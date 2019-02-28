#include "./crypto.h"
#include <iostream>
#include <chrono>

typedef std::chrono::high_resolution_clock Clock;

int main() {

	crypto c;

	auto start = Clock::now();

	for (unsigned long long i =0; i < 30000000; i++){
		c.hmac_sha2(c.get_plaintext());	
	}

	auto end = Clock::now();

	std::cout<<"Duration: "<< std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count()
				 	<< " nanoseconds" << std::endl;


	return 0;
}


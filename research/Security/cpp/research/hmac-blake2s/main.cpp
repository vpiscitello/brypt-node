#include "./crypto.h"
#include <iostream>

int main() {

	crypto c;
	
	while(1){
		c.hmac_blake2s(c.get_plaintext());	
	}

	return 0;
}


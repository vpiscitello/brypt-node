#include "./crypto.h"
#include <iostream>

int main() {

	crypto c;

	while(1) {
		c.hmac_sha2(c.get_plaintext());	
	}

	return 0;
}


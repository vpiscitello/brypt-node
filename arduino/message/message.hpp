#ifndef MESSAGE_HPP
#define MESSAGE_HPP

#include "Arduino.h"

// #include <sstream>
// #include <functional>

#include "utility.hpp"

//#include <Base64.h>

#include <Hash.h>
//#include <SHA256.h>
#include <BLAKE2s.h>
#include <AES.h>
#include <CTR.h>
#include <Crypto.h>


#define HASH_SIZE 32
#define CRYPTO_AES_DEFAULT "AES-256-CTR"
#define KEY_SIZE 32

static const String base64_chars = 
	"ABCDEFGHIJKLMNOPQRSTUVWXYZ"
	"abcdefghijklmnopqrstuvwxyz"
	"0123456789+/";
	
// Base64 Encode/Decode source: https://github.com/ReneNyffenegger/cpp-base64
static inline bool is_base64(unsigned char c) {
    return (isalnum(c) || (c == '+') || (c == '/'));
}

class Message {
	private:
		String raw;                // Raw string format of the message

		String source_id;          // ID of the sending node
		String destination_id;     // ID of the receiving node
		String await_id;           // ID of the awaiting request on a passdown message

		Command::Type command;            // Command type to be run
		unsigned int phase;                      // Phase of the Command state

		String data;               // Encrypted data to be sent
		String timestamp;          // Current timestamp

		Message * response;             // A circular message for the response to the current message

		String auth_token;         // Current authentication token created via HMAC
		unsigned int nonce;             // Current message nonce
		String key;

		enum MessageChunk {
			SOURCEID_CHUNK, DESTINATIONID_CHUNK, COMMAND_CHUNK, PHASE_CHUNK, NONCE_CHUNK, DATASIZE_CHUNK, DATA_CHUNK, TIMESTAMP_CHUNK,
			FIRST_CHUNK = SOURCEID_CHUNK,
			LAST_CHUNK = TIMESTAMP_CHUNK
		};

	public:
		// Constructors Functions
		/* **************************************************************************
		 ** Function: Default Message Constructor
		 ** Description: Initializes all class variables to default values.
		 ** *************************************************************************/
		Message() {
			this->raw = "";
			this->source_id = "";
			this->destination_id = "";
			this->command = NO_CMD;
			this->phase = -1;
			this->data = "";
			this->timestamp = "";
			this->response = NULL;
			this->auth_token = "";
			this->nonce = 0;
			this->set_timestamp();
		}
		
		/* **************************************************************************
		 ** Function: Constructor for Recieved Messages
		 ** Description: Takes raw string input and unpacks it into the class variables.
		 ** *************************************************************************/
		Message(String raw){
			//Serial.print("Raw: ");
			//Serial.println(raw);
			this->raw = this->base64_decode(raw);
			//Serial.print("Raw Decoded: ");
			//Serial.println(this->raw);
			
			this->key = NET_KEY;
			
			this->unpack();
			this->response = NULL;
		}
		
		/* **************************************************************************
		 ** Function: Constructor for new Messages
		 ** Description: Initializes new messages provided the intended values.
		 ** *************************************************************************/
		Message(String source_id, String destination_id, Command::Type command, int phase, String data, unsigned int nonce) {
			this->raw = "";
			this->source_id = source_id;
			this->destination_id = destination_id;
			this->command = command;
			this->phase = phase;
			this->data = data;
			this->timestamp = "";
			this->response = NULL;
			this->auth_token = "";
			this->nonce = nonce;
			this->set_timestamp();
			this->data = this->encrypt(this->data);
		}

		// Getter Functions
		/* **************************************************************************
		 ** Function: get_source_id
		 ** Description: Return the ID of the Node that sent the message.
		 ** *************************************************************************/
		String get_source_id() {
			return this->source_id;
		}
		
		/* **************************************************************************
		 ** Function: get_destination_id
		 ** Description: Return the ID of the Node message going to.
		 ** *************************************************************************/
		String get_destination_id() {
			return this->destination_id;
		}
		
		/* **************************************************************************
		 ** Function: get_await_id
		 ** Description: Return the ID of the AwaitObject attached to flood request.
		 ** *************************************************************************/
		String get_await_id() {
			return this->await_id;
		}
		
		/* **************************************************************************
		 ** Function: get_command
		 ** Description: Return the designated command to handle the message.
		 ** *************************************************************************/
		Command::Type get_command() {
			return this->command;
		}
		
		/* **************************************************************************
		 ** Function: get_phase
		 ** Description: Return the commands phase.
		 ** *************************************************************************/
		unsigned int get_phase() {
			return this->phase;
		}
		
		/* **************************************************************************
		 ** Function: get_data
		 ** Description: Return the data content of the message.
		 ** *************************************************************************/
		String get_data() {
			return this->data;
		}
		
		/* **************************************************************************
		 ** Function: get_timestamp
		 ** Description: Return the timestamp of when the message was created.
		 ** *************************************************************************/
		String get_timestamp() {
			return this->timestamp;
		}
		
		/* **************************************************************************
		 ** Function: get_nonce
		 ** Description: Return the commands phase.
		 ** *************************************************************************/
		unsigned int get_nonce() {
			return this->nonce;
		}
		
		/* **************************************************************************
		 ** Function: get_pack
		 ** Description: Return the packed data of the message. If it has not been
		 ** sent pack the data first.
		 ** *************************************************************************/
		String get_pack() {
			//Serial.println("Get pack called");
			if ( this->raw == "" ) {
				//Serial.println("Calling pack");
				this->pack();
			}
			//Serial.println("Appending auth token");
			String raw_pack = this->raw + this->auth_token;
			//Serial.println("Calling base64 encode");
			return this->base64_encode( raw_pack, raw_pack.length() );
		}
		/* **************************************************************************
		 ** Function: get_response
		 ** Description: Return the Response for the message.
		 ** *************************************************************************/
		String get_response() {
			if ( this->response == NULL ) {
				return "";
			}
			return this->response->get_pack();
		}

		// Setter Functions
		/* **************************************************************************
		 ** Function: set_raw
		 ** Description: Set the raw string for the message.
		 ** *************************************************************************/
		void set_raw(String raw) {
			this->raw = raw;
		}
		
		/* **************************************************************************
		 ** Function: set_source_id
		 ** Description: Set the Node ID of the message.
		 ** *************************************************************************/
		void set_source_id(String source_id) {
			this->source_id = source_id;
		}
		
		/* **************************************************************************
		 ** Function: set_destination_id
		 ** Description: Set the Node ID of the message.
		 ** *************************************************************************/
		void set_destination_id(String destination_id) {
			this->destination_id = destination_id;
		}
		
		/* **************************************************************************
		 ** Function: set_command
		 ** Description: Set the command of the message.
		 ** *************************************************************************/
		void set_command(Command::Type command, int phase) {
			this->command = command;
			this->phase = phase;
		}
		
		/* **************************************************************************
		 ** Function: set_data
		 ** Description: Set the data content of the message.
		 ** *************************************************************************/
		void set_data(String data) {
			this->data = data;
		}
		
		/* **************************************************************************
		 ** Function: set_nonce
		 ** Description: Set the current nonce of the message.
		 ** *************************************************************************/
		void set_nonce(unsigned int nonce) {
			this->nonce = nonce;
		}
		
		/* **************************************************************************
		 ** Function: set_timestamp
		 ** Description: Determine the timestamp of the message using the system clock.
		 ** *************************************************************************/
		void set_timestamp() {
			this->timestamp = "000000000";
			// std::stringstream ss;
			// std::chrono::seconds seconds;
			// std::chrono::time_point<std::chrono::system_clock> current_time;
			// current_time = std::chrono::system_clock::now();
			// seconds = std::chrono::duration_cast<std::chrono::seconds>( current_time.time_since_epoch() );
			// ss.clear();
			// ss << seconds.count();
			// this->timestamp = ss.str();
		}
		
		/* **************************************************************************
		 ** Function: set_response
		 ** Description: Set the message Response provided the data content and sending
		 ** Node ID.
		 ** *************************************************************************/
		void set_response(String source_id, String data) {
			if (this->response == NULL) {
				this->response = new Message(source_id, this->source_id, this->command, this->phase + 1, data, this->nonce + 1);
			} else {
				this->response->set_source_id( source_id );
				this->response->set_destination_id( this->source_id );
				this->response->set_command( this->command, this->phase + 1 );
				this->response->set_data( data );
				this->response->set_nonce( this->nonce + 1 );
			}
		}
		
		// Utility Functions
		/* **************************************************************************
		 ** Function: pack_chunk
		 ** Description: Wrap a string message chunk with seperators.
		 ** *************************************************************************/
		String pack_chunk(String content) {
			String packed;
			packed = packed + (char)2;
			packed = packed + content;
			packed = packed + (char)3;
			packed = packed + (char)29;
			return packed;
		}
		
		/* **************************************************************************
		 ** Function: pack_chunk
		 ** Description: Wrap an unsigned int message chunk with seperators.
		 ** *************************************************************************/
		String pack_chunk(unsigned int content) {
			String packed;
			packed = packed + (char)2;
			packed = packed + String(content);
			packed = packed + (char)3;
			packed = packed + (char)29;
			return packed;
		}
		
		/* **************************************************************************
		 ** Function: pack
		 ** Description: Pack the Message class values into a single raw string.
		 ** *************************************************************************/
		void pack() {
			String packed;
			//Serial.println("Pack called");

			packed = packed + (char)1;

			packed = packed + (this->pack_chunk( this->source_id ));
			packed = packed + (this->pack_chunk( this->destination_id ));
			packed = packed + (this->pack_chunk((unsigned int)this->command));
			packed = packed + (this->pack_chunk((unsigned int)this->phase));
			packed = packed + (this->pack_chunk((unsigned int)this->nonce));
			packed = packed + (this->pack_chunk((unsigned int)this->data.length()));
			packed = packed + (this->pack_chunk(this->data));

			packed = packed + (char)4;

			//Serial.println("Calling hmac on the packed message");
			this->auth_token = this->hmac_blake2s( packed );
			//Serial.println("After hmac, setting raw to packed");
			this->raw = packed;
		}
		
		/* **************************************************************************
		 ** Function: unpack
		 ** Description: Unpack the raw message string into the Message class variables.
		 ** *************************************************************************/
		void unpack() {//decrypt
		  int loops = 0;
		  MessageChunk chunk = FIRST_CHUNK;
		  int data_size = 0;
		  int last_end = 0;

		  while (chunk <= LAST_CHUNK ) {
			int chunk_end = this->raw.indexOf( ( char ) 29, (last_end + 1) );     // Find chunk seperator

			switch (chunk) {
			  // Source ID
			  case SOURCEID_CHUNK:
				this->source_id = this->raw.substring( last_end + 2, ( chunk_end - 1 ) );
				break;
				// Destination ID
			  case DESTINATIONID_CHUNK:
				this->destination_id = this->raw.substring( last_end + 2, ( chunk_end - 1 ) );
				break;
				// Command Type
			  case COMMAND_CHUNK:
				this->command = ( Command::Type ) (
					this->raw.substring( last_end + 2, ( chunk_end - 1 ) )
					).toInt();
				break;
				// Command Phase
			  case PHASE_CHUNK:
				this->phase = (
					this->raw.substring( last_end + 2, ( chunk_end - 1 ) )
						).toInt();
				break;
				// Nonce
			  case NONCE_CHUNK:
				this->nonce = (
					this->raw.substring( last_end + 2, ( chunk_end - 1 ) )
						).toInt();
				break;
				// Data Size
			  case DATASIZE_CHUNK:
				data_size = (
					this->raw.substring( last_end + 2, ( chunk_end - 1 ) )
					  ).toInt();
				break;
				// Data
			  case DATA_CHUNK:
				this->data = this->raw.substring( last_end + 2, ( last_end + data_size + 2 ) );
				this->decrypt(this->data);
				break;
				// Timestamp
			  case TIMESTAMP_CHUNK:
				this->timestamp = this->raw.substring( last_end + 2, ( chunk_end - 1 ) );
				break;
				// End of Message Parsing
			  default:
				break;
			}

			last_end = chunk_end;
			loops++;
			chunk = (MessageChunk) loops;
		  }

		  this->auth_token = this->raw.substring( last_end + 2 );
		  this->raw = this->raw.substring( 0, ( this->raw.length() - this->auth_token.length() ) );
		  
		  std::size_t id_sep_found;
		  id_sep_found = this->source_id.indexOf(ID_SEPERATOR);
		  if (id_sep_found != -1) {
			this->await_id = this->source_id.substring(id_sep_found + 1);
			this->source_id = this->source_id.substring(0, id_sep_found);
		  }

		  id_sep_found = this->destination_id.indexOf(ID_SEPERATOR);
		  if (id_sep_found != -1) {
			this->await_id = this->destination_id.substring(id_sep_found + 1);
			this->destination_id = this->destination_id.substring(0, id_sep_found);
		  }
		}
		
		/* **************************************************************************
		 ** Function: hmac
		 ** Description: HMAC a provided message and return the authentication token.
		 ** *************************************************************************/
		
		/*String hmac(String mssg){
			unsigned char result[HASH_SIZE];
			unsigned char tmpres[HASH_SIZE];
			int mlen = mssg.length();
			char * mssgptr;
			char noncebuf[12];
			mssg.toCharArray(mssgptr, mlen);
			itoa(this->nonce, noncebuf, 10);
			byte tmpkey[32];
			memset(result, 0x00, sizeof(result));
			BLAKE2s *h;
			//long keynum = this->key.toInt();
			//keynum = keynum+nonce;
			//String(keynum).getBytes(tmpkey, 32);

			char key_chars[(this->key).length()];
			(this->key).toCharArray(key_chars, sizeof(key_chars));
			h->resetHMAC((void *)key_chars, 32);
			h->update(noncebuf, 12);
			h->finalizeHMAC((const void *)key_chars, (size_t)12, (void *)tmpkey, (size_t)HASH_SIZE);

			h->resetHMAC(tmpkey, 32);
			h->update((const void *)mssgptr, (size_t)mlen);
			h->finalizeHMAC((const void *)tmpkey, (size_t)mlen, (void *)result, (size_t)HASH_SIZE);
			Serial.println(result[0]);
			String trueres;
			trueres = String((char *)result);
			return trueres;
		}*/
		
		/*
		String hash(Hash *h, uint8_t *value, byte *mssg){
			h->reset();
			h->update(mssg, sizeof(mssg));
			h->finalize(buffer, h->hashSize()); 
			size_t inc = 1;
			size_t size = strlen(mssg);
			size_t posn, len;
			//  uint8_t value[HASH_SIZE];
			h->reset();
			for (posn = 0; posn < size; posn += inc) {
				len = size - posn;
				if (len > inc)
				len = inc;
				h->update(mssg + posn, len);
			}
			h->finalize(value, HASH_SIZE);
		}
		*/
		
		
		void hmac(Hash * h, byte * key, byte * result, byte * mssg) {
			h->resetHMAC(key, strlen((char *)key));
			h->update(mssg, strlen((char *)mssg));
			h->finalizeHMAC(key, strlen((char *)key), result, HASH_SIZE);
		}
		
		String hmac_blake2s(String message) {
			byte hmac_mssg[1024];
			memset(hmac_mssg, '\0', 1024);
			message.getBytes(hmac_mssg, 1024);
			
			byte key256[33];
			memset(key256, '\0', 33);
			NET_KEY.getBytes(key256, 33);
			
			BLAKE2s blake2s;
			byte buffer[128];
			byte result[HASH_SIZE];

			memset(buffer, 0x00, sizeof(buffer));
			memset(result, 0x00, sizeof(result));

			// This one matches what is on the other devices
			this->hmac(&blake2s, key256, result, hmac_mssg);
			String cpystr2((char *)result);
			String str2 = cpystr2;
			//Serial.print("Key bytes: ");
			//Serial.println(str2);
			//Serial.println("Key bytes ints: ");
			//for (int idx = 0; idx < 32; idx++) {
			//	Serial.print((int)(str2.charAt(idx)));
			//	Serial.print(" ");
			//}
			//Serial.println("");
			
			return str2;
		}
		
		String encrypt(String plaintext){
			//Serial.println("ENCRYPT");
			CTR<AES256> aes_ctr_256;
			byte ctxt[1024];
			memset(ctxt, 0x00, sizeof(ctxt));

			crypto_feed_watchdog();
			
			byte key[33];
			memset(key, '\0', 33);
			NET_KEY.getBytes(key, 33);
			aes_ctr_256.setKey(key, 32);
			
			byte iv[16];
			memset(iv, '\0', 16);
			String nonce(NET_NONCE);
			nonce.getBytes(iv, 16);
			aes_ctr_256.setIV(iv, 16);
			aes_ctr_256.setCounterSize(4);
			
			byte mssg[1024];
			memset(mssg, '\0', 1024);
			plaintext.getBytes(mssg, 1024);
			aes_ctr_256.encrypt(ctxt, mssg, strlen((const char *)mssg));
			
			String cpystr((char *)ctxt);
			String ciphertext = cpystr;
			//Serial.print("Ciphertext: ");
			//Serial.println(ciphertext);
			
			//Serial.print("Encrypted bytes: ");
			//for (int idx = 0; idx < ciphertext.length(); idx++) {
			//	Serial.print((int)(ciphertext.charAt(idx)));
			//	Serial.print(" ");
			//}
			//Serial.println("");
			
			return ciphertext;
		}
		
		String decrypt(String ciphertext){
			
			byte ctxt[1024];
			memset(ctxt, '\0', 1024);
			ciphertext.getBytes(ctxt, 1024);
			
			//Serial.println("DECRYPT");
			byte buffer[1024];
			CTR<AES256> aes_ctr_256;
			memset(buffer, 0x00, sizeof(ctxt));

			crypto_feed_watchdog();
			
			byte key[33];
			memset(key, '\0', 33);
			NET_KEY.getBytes(key, 33);
			aes_ctr_256.setKey(key, 32);
			
			byte iv[16];
			memset(iv, '\0', 16);
			String nonce(NET_NONCE);
			nonce.getBytes(iv, 16);
			aes_ctr_256.setIV(iv, 16);
			aes_ctr_256.setCounterSize(4);
			
			aes_ctr_256.decrypt(buffer, ctxt, strlen((const char *)ctxt));
			
			String cpystr((char *)buffer);
			String plain = cpystr;
			//Serial.print("Plaintext: ");
			//Serial.println(plain);
			
			
			//Serial.print("Decrypted bytes: ");
			//for (int idx = 0; idx < plain.length(); idx++) {
			//	Serial.print((int)(plain.charAt(idx)));
			//	Serial.print(" ");
			//}
			//Serial.println("");
			
			this->data = plain;
			
			return plain;
		}
		
		/* **************************************************************************
		 ** Function: verify
		 ** Description: Compare the Message token with the computed HMAC.
		 ** *************************************************************************/
		bool verify() {
			bool verified = false;
			String check_token = "";

			if (this->raw != "" || this->auth_token != "") {
				check_token = this->hmac_blake2s( this->raw );
				if (this->auth_token == check_token) {
					verified = true;
				}
			}

			return verified;
		}
		
		/* **************************************************************************
		** Function: base64_encode
		** Description: Encode a std::string to a Base64 message
		** Source: https://github.com/ReneNyffenegger/cpp-base64/blob/master/base64.cpp#L45
		** *************************************************************************/
		String base64_encode(String message, unsigned int in_len) {
		  //Serial.println("Going to encode");
		  //Serial.print("Message to encode: ");
		  //Serial.println(message);
		  //Serial.print("Message length: ");
		  //Serial.println(in_len);
		  String encoded;
		  int idx = 0, jdx = 0;
		  unsigned char char_array_3[3], char_array_4[4];
		  unsigned char const * bytes_to_encode = reinterpret_cast<const unsigned char *>( message.c_str() );

		  while (in_len--) {
			char_array_3[idx++] = *(bytes_to_encode++);

			if(idx == 3) {
			  char_array_4[0] = ( char_array_3[0] & 0xfc ) >> 2;
			  char_array_4[1] = ( (char_array_3[0] & 0x03) << 4 ) + ( (char_array_3[1] & 0xf0) >> 4 );
			  char_array_4[2] = ( (char_array_3[1] & 0x0f) << 2 ) + ( (char_array_3[2] & 0xc0) >> 6 );
			  char_array_4[3] = char_array_3[2] & 0x3f;

			  for (idx = 0; idx < 4; idx++) {
				encoded += base64_chars[char_array_4[idx]];
			  }

			  idx = 0;
			}
		  }

		  if (idx) {
			for (jdx = idx; jdx < 3; jdx++) {
			  char_array_3[jdx] = '\0';
			}
			 
			char_array_4[0] = ( char_array_3[0] & 0xfc ) >> 2;  
			char_array_4[1] = ( (char_array_3[0] & 0x03) << 4 ) + ( (char_array_3[1] & 0xf0) >> 4 );
			char_array_4[2] = ( (char_array_3[1] & 0x0f) << 2 ) + ( (char_array_3[2] & 0xc0) >> 6 );      

			for (jdx = 0; jdx < idx + 1; jdx++) {
			  encoded += base64_chars[char_array_4[jdx]];
			}

			while (idx++ < 3) {
			  encoded += '=';
			}
		  }
		  //Serial.print("Going to return encoded string: ");
		  //Serial.println(encoded);

		  return encoded;
		}

		/* **************************************************************************
		** Function: base64_decode
		** Description: Decode a Base64 message to a std::string
		** Source: https://github.com/ReneNyffenegger/cpp-base64/blob/master/base64.cpp#L87
		** *************************************************************************/
		String base64_decode(String const& message) {
		  String decoded;
		  int in_len = message.length();
		  int idx = 0, jdx = 0, in_ = 0;
		  unsigned char char_array_3[3], char_array_4[4];

		  while ( in_len-- && ( message[in_] != '=' ) && is_base64( message[in_] ) ) {
			char_array_4[idx++] = message[in_]; in_++;
			
			if (idx == 4 ) {
			  for (idx = 0; idx < 4; idx++) {
				char_array_4[idx] = base64_chars.indexOf( char_array_4[idx] );
			  }

			  char_array_3[0] = ( char_array_4[0] << 2 ) + ( (char_array_4[1] & 0x30) >> 4 );
			  char_array_3[1] = ( (char_array_4[1] & 0x0f) << 4 ) + ( (char_array_4[2] & 0x3c) >> 2 );
			  char_array_3[2] = ( (char_array_4[2] & 0x03) << 6 ) + char_array_4[3];

			  for (idx = 0; idx < 3; idx++) {
				decoded += (char)char_array_3[idx];
			  }

			  idx = 0;
			}
		  }

		  if (idx) {
			for (jdx = 0; jdx < idx; jdx++) {
			  char_array_4[jdx] = base64_chars.indexOf( char_array_4[jdx] );
			}

			char_array_3[0] = ( char_array_4[0] << 2 ) + ( (char_array_4[1] & 0x30) >> 4 ); 
			char_array_3[1] = ( (char_array_4[1] & 0x0f) << 4 ) + ( (char_array_4[2] & 0x3c) >> 2 );

			for (jdx = 0; jdx < idx - 1; jdx++) {
			  decoded += (char)char_array_3[jdx];
			}
		  }

		  return decoded;
		}
};

#endif

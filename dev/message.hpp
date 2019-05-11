#ifndef MESSAGE_HPP
#define MESSAGE_HPP

#include <chrono>
#include <sstream>
#include <functional>
#include <string>

#include "utility.hpp"

#include <openssl/conf.h>
#include <openssl/err.h>
#include <openssl/sha.h>
#include <openssl/evp.h>
#include <openssl/hmac.h>

static const std::string base64_chars = 
"ABCDEFGHIJKLMNOPQRSTUVWXYZ"
"abcdefghijklmnopqrstuvwxyz"
"0123456789+/";

// Base64 Encode/Decode source: https://github.com/ReneNyffenegger/cpp-base64
static inline bool is_base64(unsigned char c) {
	return (isalnum(c) || (c == '+') || (c == '/'));
}

class Message {
	private:
		std::string raw;                // Raw string format of the message

		std::string source_id;          // ID of the sending node
		std::string destination_id;     // ID of the receiving node
		std::string await_id;           // ID of the awaiting request on a passdown message

		CommandType command;            // Command type to be run
		unsigned int phase;             // Phase of the Command state

		std::string data;               // Encrypted data to be sent
		unsigned int dataLen;		// Data length

		std::string timestamp;          // Current timestamp

		std::string key;		//Key used for crypto
		Message * response;             // A circular message for the response to the current message

		std::string auth_token;         // Current authentication token created via HMAC
		unsigned int nonce;             // Current message nonce

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
		Message(std::string raw) {
			this->raw = this->base64_decode( raw );
			this->key = NET_KEY;
			this->unpack();
			// this->aes_ctr_256_decrypt(this->data, this->data.size());
			this->response = NULL;
		}
		/* **************************************************************************
		 ** Function: Constructor for new Messages
		 ** Description: Initializes new messages provided the intended values.
		 ** *************************************************************************/
		Message(std::string source_id, std::string destination_id, CommandType command, int phase, std::string data, unsigned int nonce) {
			this->raw = "";
			this->source_id = source_id;
			this->destination_id = destination_id;
			this->command = command;
			this->phase = phase;
			this->data = data;
			this->timestamp = "";
			this->response = NULL;
			this->auth_token = "";
			this->nonce = NET_NONCE;
			this->set_timestamp();
			this->key = NET_KEY;  // In utility
			this->aes_ctr_256_encrypt(this->data, this->data.size());
		}

		// Getter Functions
		/* **************************************************************************
		 ** Function: get_source_id
		 ** Description: Return the ID of the Node that sent the message.
		 ** *************************************************************************/
		std::string get_source_id() {
			return this->source_id;
		}
		/* **************************************************************************
		 ** Function: get_destination_id
		 ** Description: Return the ID of the Node message going to.
		 ** *************************************************************************/
		std::string get_destination_id() {
			return this->destination_id;
		}
		/* **************************************************************************
		 ** Function: get_await_id
		 ** Description: Return the ID of the AwaitObject attached to flood request.
		 ** *************************************************************************/
		std::string get_await_id() {
			return this->await_id;
		}
		/* **************************************************************************
		 ** Function: get_command
		 ** Description: Return the designated command to handle the message.
		 ** *************************************************************************/
		CommandType get_command() {
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
		std::string get_data() {
			return this->data;
		}
		/* **************************************************************************
		 ** Function: get_timestamp
		 ** Description: Return the timestamp of when the message was created.
		 ** *************************************************************************/
		std::string get_timestamp() {
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
		std::string get_pack() {
			if ( this->raw == "" ) {
				this->pack();
			}
			std::string raw_pack = this->raw + this->auth_token;
			return this->base64_encode( raw_pack, raw_pack.size() );
		}
		/* **************************************************************************
		 ** Function: get_response
		 ** Description: Return the Response for the message.
		 ** *************************************************************************/
		std::string get_response() {
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
		void set_raw(std::string raw) {
			this->raw = raw;
		}
		/* **************************************************************************
		 ** Function: set_source_id
		 ** Description: Set the Node ID of the message.
		 ** *************************************************************************/
		void set_source_id(std::string source_id) {
			this->source_id = source_id;
		}
		/* **************************************************************************
		 ** Function: set_destination_id
		 ** Description: Set the Node ID of the message.
		 ** *************************************************************************/
		void set_destination_id(std::string destination_id) {
			this->destination_id = destination_id;
		}
		/* **************************************************************************
		 ** Function: set_command
		 ** Description: Set the command of the message.
		 ** *************************************************************************/
		void set_command(CommandType command, int phase) {
			this->command = command;
			this->phase = phase;
		}
		/* **************************************************************************
		 ** Function: set_data
		 ** Description: Set the data content of the message.
		 ** *************************************************************************/
		void set_data(std::string data) {
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
			this->timestamp = get_system_timestamp();
		}
		/* **************************************************************************
		 ** Function: set_response
		 ** Description: Set the message Response provided the data content and sending
		 ** Node ID.
		 ** *************************************************************************/
		void set_response(std::string source_id, std::string data) {
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
		std::string pack_chunk(std::string content) {
			std::string packed;
			packed.append( 1, 2 );	// Start of telemetry chunk
			packed.append( content );
			packed.append( 1, 3 );	// End of telemetry chunk
			packed.append( 1, 29 );	// Telemetry seperator
			return packed;
		}
		/* **************************************************************************
		 ** Function: pack_chunk
		 ** Description: Wrap an unsigned int message chunk with seperators.
		 ** *************************************************************************/
		std::string pack_chunk(unsigned int content) {
			std::string packed;
			std::stringstream ss;
			ss.clear();
			packed.append( 1, 2 );	// Start of telemetry chunk
			ss << content;
			packed.append( ss.str() );
			packed.append( 1, 3 );	// End of telemetry chunk
			packed.append( 1, 29 );	// Telemetry seperator
			return packed;
		}
		/* **************************************************************************
		 ** Function: pack
		 ** Description: Pack the Message class values into a single raw string.
		 ** *************************************************************************/
		void pack() {
			std::string packed;

			packed.append( 1, 1 );	// Start of telemetry pack

			packed.append( this->pack_chunk( this->source_id ) );
			packed.append( this->pack_chunk( this->destination_id ) );
			packed.append( this->pack_chunk( (unsigned int)this->command ) );
			packed.append( this->pack_chunk( (unsigned int)this->phase ) );
			packed.append( this->pack_chunk( (unsigned int)this->nonce ) );
			packed.append( this->pack_chunk( (unsigned int)this->data.size() ) );
			packed.append( this->pack_chunk( this->data ) );
			packed.append( this->pack_chunk( this->timestamp ) );

			packed.append( 1, 4 );	// End of telemetry pack

			this->auth_token = this->hmac_blake2s( packed , packed.size() );
			this->raw = packed;
		}
		/* **************************************************************************
		 ** Function: unpack
		 ** Description: Unpack the raw message string into the Message class variables.
		 ** *************************************************************************/
		void unpack() {
			int loops = 0;
			MessageChunk chunk = FIRST_CHUNK;
			std::size_t last_end = 0;
			std::size_t data_size = 0;

			// Iterate while there are message chunks to be parsed.
			while (chunk <= LAST_CHUNK ) {
				std::size_t chunk_end = this->raw.find( ( char ) 29, last_end + 1 );     // Find chunk seperator

				switch (chunk) {
					// Source ID
					case SOURCEID_CHUNK:
						this->source_id = this->raw.substr( last_end + 2, ( chunk_end - 1 ) - ( last_end + 2) );

						break;
						// Destination ID
					case DESTINATIONID_CHUNK:
						this->destination_id = this->raw.substr( last_end + 2, ( chunk_end - 1 ) - ( last_end + 2) );
						break;
						// Command Type
					case COMMAND_CHUNK:
						this->command = ( CommandType ) std::stoi(
								this->raw.substr( last_end + 2, ( chunk_end - 1 ) - ( last_end + 2) )
								);
						break;
						// Command Phase
					case PHASE_CHUNK:
						this->phase = std::stoi(
								this->raw.substr( last_end + 2, ( chunk_end - 1 ) - ( last_end + 2) )
								);
						break;
						// Nonce
					case NONCE_CHUNK:
						this->nonce = std::stoi(
								this->raw.substr( last_end + 2, ( chunk_end - 1 ) - ( last_end + 2) )
								);
						break;
						// Data Size
					case DATASIZE_CHUNK:
						data_size = ( std::size_t ) std::stoi(
								this->raw.substr( last_end + 2, ( chunk_end - 1 ) - ( last_end + 2) )
								);
						break;
						// Data
					case DATA_CHUNK:
						this->data = this->raw.substr( last_end + 2, data_size );
						this->aes_ctr_256_decrypt(this->data, this->data.size());
						break;
						// Timestamp
					case TIMESTAMP_CHUNK:
						this->timestamp = this->raw.substr( last_end + 2, ( chunk_end - 1 ) - ( last_end + 2) );
						break;
						// End of Message Parsing
					default:
						break;
				}

				last_end = chunk_end;
				loops++; chunk = (MessageChunk) loops;
			}

			this->auth_token = this->raw.substr( last_end + 2 );
			this->raw = this->raw.substr( 0, ( this->raw.size() - this->auth_token.size() ) );

			std::size_t id_sep_found;
			id_sep_found = this->source_id.find(ID_SEPERATOR);
			if (id_sep_found != std::string::npos) {
				this->await_id = this->source_id.substr(id_sep_found + 1);
				this->source_id = this->source_id.substr(0, id_sep_found);
			}

			id_sep_found = this->destination_id.find(ID_SEPERATOR);
			if (id_sep_found != std::string::npos) {
				this->await_id = this->destination_id.substr(id_sep_found + 1);
				this->destination_id = this->destination_id.substr(0, id_sep_found);
			}

		}

		/* **************************************************************************
		 * ** Function: clear_buff
		 * ** Description: Clears provided buffer
		 * ** *************************************************************************/
		void clear_buff(unsigned char* buff, unsigned int buffLen){
			unsigned int i;
			for(i=0; i < buffLen; i++){
				buff[i] = '\0';
			}
		}

		/* **************************************************************************
		 * ** Function: aes_ctr_256_encrypt
		 * ** Description: Encrypt a provided message with AES-CTR-256 and return ciphert
		 * ext.
		 * ** *************************************************************************/
		std::string aes_ctr_256_encrypt(std::string message, unsigned int msg_len) {
			int length = 0;
			int data_len = 0;

			unsigned char ciphertext[msg_len + 1];
			memset(ciphertext, '\0', msg_len + 1);

			unsigned char * plaintext = (unsigned char *) message.c_str();

			unsigned char iv[16];
			std::string nonce_str = std::to_string(this->nonce);
			memcpy(iv, (unsigned char *) nonce_str.c_str(), nonce_str.size());
			memset(iv + nonce_str.size(), '\0', 16 - nonce_str.size());

			unsigned char * key = (unsigned char *) this->key.c_str();

			EVP_CIPHER_CTX * ctx;

			ctx = EVP_CIPHER_CTX_new();
			EVP_EncryptInit_ex( ctx, EVP_aes_256_ctr(), NULL, key, iv );
			EVP_EncryptUpdate( ctx, ciphertext, &length, plaintext, msg_len );

			data_len = length;
			EVP_EncryptFinal_ex( ctx, ciphertext + length, &length );
			data_len += length;

			EVP_CIPHER_CTX_free( ctx );

			std::string encrypted = "";
			for( int idx = 0; idx < data_len; idx++) {
				encrypted += static_cast<char>( ciphertext[idx] );
			}

			this->data = encrypted;

			return encrypted;	   	
		}

		/* **************************************************************************
		 * ** Function: aes_ctr_256_decrypt
		 * ** Description: Decrypt a provided message with AES-CTR-256 and return decrypt
		 * ed text.	
		 * ** *************************************************************************/
		std::string aes_ctr_256_decrypt(std::string message, unsigned int msg_len) {
			int length = 0;
			int data_len = 0;

			unsigned char plaintext[msg_len + 1];
			memset( plaintext, '\0', msg_len + 1);

			unsigned char * ciphertext = (unsigned char *) message.c_str();

			unsigned char iv[16];
			std::string nonce_str = std::to_string(this->nonce);
			memcpy(iv, (unsigned char *) nonce_str.c_str(), nonce_str.size());
			memset(iv + nonce_str.size(), '\0', 16 - nonce_str.size());

			unsigned char * key = (unsigned char *) this->key.c_str();


			EVP_CIPHER_CTX * ctx;
			ctx = EVP_CIPHER_CTX_new();
			EVP_DecryptInit_ex( ctx, EVP_aes_256_ctr(), NULL, key, iv );
			EVP_DecryptUpdate( ctx, plaintext, &length, ciphertext, msg_len );

			data_len = length;
			EVP_DecryptFinal_ex( ctx, plaintext + length, &length );
			data_len += length;

			EVP_CIPHER_CTX_free( ctx );

			std::string decrypted = "";
			for(int idx = 0; idx < data_len; idx++) {
				decrypted += (char) plaintext[idx];
			}
			this->data = decrypted;

			return decrypted;
		}

		/* **************************************************************************
		 * ** Function: hmac_sha2
		 * ** Description: HMAC SHA2 a provided message and return the authentication tok
		 * en.
		 * ** *************************************************************************/
		std::string hmac_sha2(std::string mssgData, int mssgLen) {
			const EVP_MD *md;
			md = EVP_get_digestbyname("sha256");
			const unsigned char* mssg = (const unsigned char*)mssgData.c_str();
			const unsigned char* k = (const unsigned char*)key.c_str();
			unsigned int length = 0;
			unsigned char* d;
			d = HMAC(md, k, this->key.size(), mssg, mssgLen, NULL, &length);
			std::string cppDigest = (char*)d;
			return cppDigest;
		}

		/* **************************************************************************
		 * ** Function: hmac_blake2s
		 * ** Description: HMAC Blake2s a provided message and return the authentication token.
		 * ** *************************************************************************/
		std::string hmac_blake2s(std::string mssgData, int mssgLen) {
			const EVP_MD *md;
			md = EVP_get_digestbyname("blake2s256");
			const unsigned char* mssg = (const unsigned char*)mssgData.c_str();
			const unsigned char* k = (const unsigned char*)key.c_str();
			unsigned int length = 0;
			unsigned char* d;
			d = HMAC(md, k, (this->key).size(), mssg, mssgLen, NULL, &length);
			std::string cppDigest = (char*)d;
			return cppDigest;
		}

		/* **************************************************************************
		 ** Function: base64_encode
		 ** Description: Encode a std::string to a Base64 message
		 ** Source: https://github.com/ReneNyffenegger/cpp-base64/blob/master/base64.cpp#L45
		 ** *************************************************************************/
		std::string base64_encode(std::string message, unsigned int in_len) {
			std::string encoded;
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

			return encoded;
		}

		/* **************************************************************************
		 ** Function: base64_decode
		 ** Description: Decode a Base64 message to a std::string
		 ** Source: https://github.com/ReneNyffenegger/cpp-base64/blob/master/base64.cpp#L87
		 ** *************************************************************************/
		std::string base64_decode(std::string const& message) {
			std::string decoded;
			int in_len = message.size();
			int idx = 0, jdx = 0, in_ = 0;
			unsigned char char_array_3[3], char_array_4[4];

			while ( in_len-- && ( message[in_] != '=' ) && is_base64( message[in_] ) ) {
				char_array_4[idx++] = message[in_]; in_++;

				if (idx == 4 ) {
					for (idx = 0; idx < 4; idx++) {
						char_array_4[idx] = base64_chars.find( char_array_4[idx] );
					}

					char_array_3[0] = ( char_array_4[0] << 2 ) + ( (char_array_4[1] & 0x30) >> 4 );
					char_array_3[1] = ( (char_array_4[1] & 0x0f) << 4 ) + ( (char_array_4[2] & 0x3c) >> 2 );
					char_array_3[2] = ( (char_array_4[2] & 0x03) << 6 ) + char_array_4[3];

					for (idx = 0; idx < 3; idx++) {
						decoded += char_array_3[idx];
					}

					idx = 0;
				}
			}

			if (idx) {
				for (jdx = 0; jdx < idx; jdx++) {
					char_array_4[jdx] = base64_chars.find( char_array_4[jdx] );
				}

				char_array_3[0] = ( char_array_4[0] << 2 ) + ( (char_array_4[1] & 0x30) >> 4 );	
				char_array_3[1] = ( (char_array_4[1] & 0x0f) << 4 ) + ( (char_array_4[2] & 0x3c) >> 2 );

				for (jdx = 0; jdx < idx - 1; jdx++) {
					decoded += char_array_3[jdx];
				}
			}

			return decoded;
		}

		/* **************************************************************************
		 ** Function: verify
		 ** Description: Compare the Message token with the computed HMAC.
		 ** *************************************************************************/
		bool verify() {
			bool verified = false;
			std::string check_token = "";

			if (this->raw != "" || this->auth_token != "") {
				check_token = this->hmac_blake2s( this->raw, this->raw.size() );
				if (this->auth_token == check_token) {
					verified = true;
				}
			}

			return verified;
		}
};

#endif

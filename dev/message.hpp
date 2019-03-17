#ifndef MESSAGE_HPP
#define MESSAGE_HPP

#include <chrono>
#include <sstream>
#include <functional>

#include "utility.hpp"

class Message {
    private:
        std::string raw;                // Raw string format of the message

        std::string source_id;          // ID of the sending node
        std::string destination_id;     // ID of the receiving node
        std::string await_id;           // ID of the awaiting request on a passdown message

        CommandType command;            // Command type to be run
        unsigned int phase;             // Phase of the Command state

        std::string data;               // Encrypted data to be sent
        std::string timestamp;          // Current timestamp

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
            this->raw = raw;
            this->unpack();
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
            this->nonce = nonce;
            this->set_timestamp();
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
            return this->raw + this->auth_token;
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

            this->auth_token = this->hmac( packed );
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

            std::cout << "== [Message] Source: " << this->source_id << '\n';
            std::cout << "== [Message] Destination: " << this->destination_id << '\n';
            std::cout << "== [Message] Await: " << this->await_id << '\n';

        }
        /* **************************************************************************
        ** Function: hmac
        ** Description: HMAC a provided message and return the authentication token.
        ** *************************************************************************/
        std::string hmac(std::string message) {
            int key = 3005;
        	std::stringstream token, inner, in_key, out_key;

        	unsigned long in_val = ( ( unsigned long ) key ^ ( 0x5c * 32 ) );      // Get the inner key
        	unsigned long out_val = ( ( unsigned long ) key ^ ( 0x36 * 32 ) );     // Get the outer key
        	in_key << in_val;
        	out_key << out_val;

            inner << std::hash<std::string>{}( in_key.str() + message );
            token << std::hash<std::string>{}( out_key.str() + inner.str() );      // Generate the HMAC

            // token = std::hash<std::string>{}(
            //                 out_key.str() +
            //                 std::hash<std::string>{}( in_key.str() + message )
            //         );      // Generate the HMAC

            return token.str();
        }
        /* **************************************************************************
        ** Function: verify
        ** Description: Compare the Message token with the computed HMAC.
        ** *************************************************************************/
        bool verify() {
            bool verified = false;
            std::string check_token = "";

            if (this->raw != "" || this->auth_token != "") {
                check_token = this->hmac( this->raw );
                if (this->auth_token == check_token) {
                    verified = true;
                }
            }

            return verified;
        }
};

#endif

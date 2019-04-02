#ifndef MESSAGE_HPP
#define MESSAGE_HPP

#include "Arduino.h"

// #include <sstream>
// #include <functional>

#include "utility.hpp"

class Message {
    private:
        String raw;                // Raw string format of the message

        String source_id;          // ID of the sending node
        String destination_id;     // ID of the receiving node
        String await_id;           // ID of the awaiting request on a passdown message

        CommandType command;            // Command type to be run
        unsigned int phase;                      // Phase of the Command state

        String data;               // Encrypted data to be sent
        String timestamp;          // Current timestamp

        Message * response;             // A circular message for the response to the current message

        String auth_token;         // Current authentication token created via HMAC
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
            // this->set_timestamp();
        }
        /* **************************************************************************
        ** Function: Constructor for Recieved Messages
        ** Description: Takes raw string input and unpacks it into the class variables.
        ** *************************************************************************/
        Message(String raw) {
            this->raw = raw;
            this->unpack();
            this->response = NULL;
        }
        /* **************************************************************************
        ** Function: Constructor for new Messages
        ** Description: Initializes new messages provided the intended values.
        ** *************************************************************************/
        Message(String source_id, String destination_id, CommandType command, int phase, String data, unsigned int nonce) {
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
          // this->set_timestamp();
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
            if ( this->raw == "" ) {
                this->pack();
            }
            return (this->raw + this->auth_token);
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
        void set_command(CommandType command, int phase) {
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
        // void set_timestamp() {
        //     std::stringstream ss;
        //     std::chrono::seconds seconds;
        //     std::chrono::time_point<std::chrono::system_clock> current_time;
        //     current_time = std::chrono::system_clock::now();
        //     seconds = std::chrono::duration_cast<std::chrono::seconds>( current_time.time_since_epoch() );
        //     ss.clear();
        //     ss << seconds.count();
        //     this->timestamp = ss.str();
        // }
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

            packed = packed + (char)1;

            packed = packed + (this->pack_chunk( this->source_id ));
            packed = packed + (this->pack_chunk( this->destination_id ));
            packed = packed + (this->pack_chunk((unsigned int)this->command));
            packed = packed + (this->pack_chunk((unsigned int)this->phase));
            packed = packed + (this->pack_chunk((unsigned int)this->nonce));
            packed = packed + (this->pack_chunk((unsigned int)this->data.length()));
            packed = packed + (this->pack_chunk(this->data));

            packed = packed + (char)4;

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
                std::size_t chunk_end = this->raw.indexOf( ( char ) 29 );     // Find chunk seperator

                switch (chunk) {
                    // Source ID
                    case SOURCEID_CHUNK:
                        this->source_id = this->raw.substring( last_end + 2, ( chunk_end - 1 ) - ( last_end ) );
                        break;
                    // Destination ID
                    case DESTINATIONID_CHUNK:
                        this->destination_id = this->raw.substring( last_end + 2, ( chunk_end - 1 ) - ( last_end ) );
                        break;
                    // Command Type
                    case COMMAND_CHUNK:
                        this->command = ( CommandType ) (
                                            this->raw.substring( last_end + 2, ( chunk_end - 1 ) - ( last_end + 2) )
                                        ).toInt();
                        break;
                    // Command Phase
                    case PHASE_CHUNK:
                        this->phase = (
                                        this->raw.substring( last_end + 2, ( chunk_end - 1 ) - ( last_end + 2) )
                                      ).toInt();
                        break;
                    // Nonce
                    case NONCE_CHUNK:
                        this->nonce = (
                                        this->raw.substring( last_end + 2, ( chunk_end - 1 ) - ( last_end + 2) )
                                      ).toInt();
                        break;
                    // Data Size
                    case DATASIZE_CHUNK:
                        data_size = ( std::size_t ) (
                                        this->raw.substring( last_end + 2, ( chunk_end - 1 ) - ( last_end + 2) )
                                    ).toInt();
                        break;
                    // Data
                    case DATA_CHUNK:
                        this->data = this->raw.substring( last_end + 2, data_size );
                        break;
                    // // Timestamp
                    // case TIMESTAMP_CHUNK:
                    //     this->timestamp = this->raw.substring( last_end + 2, ( chunk_end - 1 ) - ( last_end + 2) );
                    //     break;
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

            // Serial.print("== [Message] Source: ");
            // Serial.println(this->source_id);
            // Serial.print("== [Message] Destination: ");
            // Serial.println(this->destination_id);
            // Serial.print("== [Message] Await: ");
            // Serial.println(this->await_id);
        }
        /* **************************************************************************
        ** Function: hmac
        ** Description: HMAC a provided message and return the authentication token.
        ** *************************************************************************/
        String hmac(String message) {
            int key = 3005;
            String in_key, out_key, inner, token;
            token = "12345";
            return token;
        }
        /* **************************************************************************
        ** Function: verify
        ** Description: Compare the Message token with the computed HMAC.
        ** *************************************************************************/
        bool verify() {
            bool verified = false;
            String check_token = "";

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

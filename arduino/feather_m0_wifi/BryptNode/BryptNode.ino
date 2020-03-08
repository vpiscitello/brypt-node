#include <SPI.h>
#include <WiFi101.h>

#include "message.hpp"
#include "utility.hpp"

const int NETWORK_LIST_SIZE = 20;
const int AP_NAME_LEN = 40;
const TechnologyType PREFERRED_TECHNOLOGY_TYPE = TCP_TYPE;

// Enum to keep track of current state
enum CurrentState { DETERMINE_NETWORK, AP_DETERMINED, AP_CONNECTED, SERVER_CONNECTED };

int builtin_led =  LED_BUILTIN;

// Keep track of the current wifi status
int wifi_status = WL_IDLE_STATUS;

// Server port to create for the web portal
WiFiServer server(80);

void listNetworks();
void printWiFiStatus();
void start_access_point(char ssid[]);
int check_wifi_status(int);
void scan_available_networks();
void send_http_response(WiFiClient);
String handle_form_submission(WiFiClient, String);
void printEncryptionType(int);
void printMacAddress(byte mac[]);

char AP_names[NETWORK_LIST_SIZE][AP_NAME_LEN];
String access_point;

// Initial state on setup is to determine the network
CurrentState state = DETERMINE_NETWORK;
int contacted = 0;

// CONFIG IP address of server to connect to
IPAddress remote_server(192, 168, 30, 1);
int remote_port = 3005;
String device_id = "4";

WiFiClient server_connection;

void setup() {
    // Configure pins for Adafruit ATWINC1500 Feather
    WiFi.setPins(8,7,4,2);

    //Initialize serial and wait for port to open:
    Serial.begin(9600);
//    while (!Serial) {
//        ; // wait for serial port to connect. Needed for native USB port only
//    }

    // Turn the LED on in order to indicate that the upload is complete
    pinMode(builtin_led, OUTPUT);

    // Check for the presence of the shield:
    if (WiFi.status() == WL_NO_SHIELD) {
        Serial.println("No WiFi shield");
        // Block forever
        while (true) {
          delay(1000);
        }
    }


    scan_available_networks();

    char ssid[] = "feather_m0_setup";
    start_access_point(ssid);

    // TESTING
    Serial.println("Ending AP");
    WiFi.end();
    state = AP_DETERMINED;
}

void loop() {
    if (state == DETERMINE_NETWORK) {
        wifi_status = check_wifi_status(wifi_status);
      
        // Get a new client
        WiFiClient client = server.available();
      
        // If a client received
        if (client) {
            // Initialize a string to hold the data in the request from the client
            String currentLine = "";
      
            // Loop while the client is still connected
            while (client.connected()) {
              if (client.available()) {             // if there's bytes to read from the client,
              char c = client.read();             // read a byte, then
              Serial.write(c);                    // print it out the serial monitor
              if (c == '\n') {                    // if the byte is a newline character
      
                // if the current line is blank, you got two newline characters in a row.
                // that's the end of the client HTTP request, so send a response:
                if (currentLine.length() == 0) {
                  send_http_response(client);
                  // break out of the while loop:
                  
                  break;
                }
                else {      // if you got a newline, then clear currentLine:
                  currentLine = "";
                }
              }
              else if (c != '\r') {    // if you got anything else but a carriage return character,
                currentLine += c;      // add it to the end of the currentLine
              }
              else if (c == '\r') {
                  int action_page = currentLine.indexOf("action_page");
                  if (action_page > 0) {
                        currentLine = currentLine.substring(action_page);

			// Get the access point they want to connect to
			String access_point_index = parse_php_req(currentLine, "ap_name=", "&host_ip");
			access_point = AP_names[access_point_index.toInt()];
			Serial.print("Access point: ");
			Serial.println(access_point);

			// Get the host IP address the device should connect to
			String host_ip_str = parse_php_req(currentLine, "host_ip=", "&host_port");
			// TODO Parse host_ip_str into IP_address data type
			remote_server = convert_str_to_ip(host_ip_str);

			// Get the host port that the device should connect on
			String host_port_str = parse_php_req(currentLine, "host_port=", "&device_id");
			remote_port = host_port_str.toInt();

			// Get the device id of the current device
			String device_id_str = parse_php_req(currentLine, "device_id=", " HTTP");
			if (device_id_str != "") {
			    device_id = device_id_str;
			}

			// Close down the client and the access point
                        client.stop();
                        WiFi.end();
                        state = AP_DETERMINED;
                    }
                }
            }
          }
          // close the connection:
          client.stop();
          Serial.println("client disconnected");
        }
    } else if (state == AP_DETERMINED) {
	connect_to_network(access_point);
    } else if (state == AP_CONNECTED) {
	connect_to_server(remote_server, remote_port);
        
    } else if (state == SERVER_CONNECTED && !contacted) {
	initial_contact();
    } else if (state == SERVER_CONNECTED && contacted) {
        String message = "";
        while (true) {
            cont_recv();
            delay(500);
        }
    }
}

void initial_contact() {
    // As soon as we have connected to the socket, conduct initial_contact
    Serial.println("Setting up initial contact with coordinator");
    delay(100);

    String resp = "";

    while (resp.length() < 1  || (int)resp.charAt(0) != 6) {
	// Send ack
	String ack_msg = "\x06";
	Serial.println("Sending ACK");
	server_connection.print(ack_msg);
	server_connection.flush();
	delay(500);

	// Should get an ACK back
	resp = recv();
	Serial.print("Received: ");
	Serial.println((int)resp.charAt(0));

	// TODO validate the ACK back and abort if necessary
    }

    
    String preferred_comm_tech = String((int)PREFERRED_TECHNOLOGY_TYPE);
    Serial.print("Going to send comm tech: ");
    Serial.println(preferred_comm_tech);
    delay(100);
    server_connection.print(preferred_comm_tech);
    server_connection.flush();
    delay(500);
    
    resp = recv();
    Serial.println("");
    Serial.print("Received: ");
    Serial.println(resp);
    Message port_message(resp);
    String coord_id = port_message.get_source_id();
    String new_port = port_message.get_data();
    Serial.print("Coord id: ");
    Serial.println(coord_id);
    Serial.print("Req port: ");
    Serial.println(new_port);

    Serial.println("Making info message");
    Serial.print("Port message nonce: ");
    Serial.println(port_message.get_nonce());
    Message info_message(device_id, coord_id, CONNECT_TYPE, 1, preferred_comm_tech, port_message.get_nonce());
    delay(100);

    Serial.print("Sending info message: ");
    Serial.println(info_message.get_pack());
    server_connection.print(info_message.get_pack());
    server_connection.flush();
    delay(500);
    
    resp = recv();
    Serial.print("Received: ");
    Serial.println((int)resp.charAt(0));

    // Should shut down connection now
    server_connection.stop();

    // Connect to the new port
    connect_to_server(remote_server, new_port.toInt());

    Serial.println("Connected to secondary port");
    contacted = 1;
}

void connect_to_server(IPAddress server, int port) {
    Serial.print("\nStarting connection to server (");
    Serial.print(server);
    Serial.print(") on port (");
    Serial.print(port);
    Serial.println(")...");
    // Try to connect to the TCP or StreamBridge socket
    if (server_connection.connect(server, port)) {
	Serial.println("Connected to server");
	state = SERVER_CONNECTED;
    } else {
	//delay(10);
    }
}

void connect_to_network(String connection_ap) {
    // We have discovered an AP name so we want to connect to the WiFi network
    while (wifi_status != WL_CONNECTED) {
	Serial.print("Attempting to connect to WPA SSID: ");
	Serial.println(connection_ap);
//	wifi_status = WiFi.begin("brypt1", "asdfasdf");
	// Connect to WPA/WPA2 network:
//	wifi_status = WiFi.begin(connection_ap);
  wifi_status = WiFi.begin("brypt-net-68b32");
    
	delay(1000);
    }

    // Print out information about the network
    Serial.print("Successfully connected to the network");
    printWiFiStatus();

    // Get the current time now that we (maybe) have internet
    Serial.print("Wifi Time: ");
    Serial.println(WiFi.getTime());
    state = AP_CONNECTED;
}

// Parse the information given back from the PHP request
// Used to parse out the access point name, host IP addr, host port,
// and this device's ID
String parse_php_req(String current_line, String from, String to) {
    if (current_line.indexOf(from) > 0) {
	int from_index = current_line.indexOf(from);
	int to_index = current_line.indexOf(to);
	String req_str = current_line.substring(from_index+from.length(), to_index);
	//Serial.print("req_string value is ");
	//Serial.println(req_str);
	return req_str;
    }
}

// Split this 192.168.137.22 to an IPAddress class
IPAddress convert_str_to_ip(String ip_addr_str) {
    int ip_addr[4];

    ip_addr_str += '.';

    int nxt_index = ip_addr_str.indexOf('.');
    String split_str = ip_addr_str.substring(0, nxt_index);
    ip_addr_str = ip_addr_str.substring(nxt_index + 1);
    int idx = 0;

    while (nxt_index != -1) {
	Serial.print("SPLIT STR: ");
	Serial.println(split_str);
	ip_addr[idx] = split_str.toInt();

	nxt_index = ip_addr_str.indexOf('.');
	Serial.print("NEXT INDEX: ");
	Serial.println(nxt_index);
	split_str = ip_addr_str.substring(0, nxt_index);
	ip_addr_str = ip_addr_str.substring(nxt_index + 1);
	Serial.print("NEXT SPLIT: ");
	Serial.println(split_str);
	delay(1000);
	idx++;
    }

    Serial.print("ip: ");
    Serial.print(ip_addr[0]);
    Serial.print(".");
    Serial.print(ip_addr[1]);
    Serial.print(".");
    Serial.print(ip_addr[2]);
    Serial.print(".");
    Serial.println(ip_addr[3]);

    IPAddress coord_server(ip_addr[0], ip_addr[1], ip_addr[2], ip_addr[3]);
    return coord_server;
}

String recv() {
    String message = "";
    while (server_connection.available()) {
        char c = server_connection.read();
//        Serial.write(c);
        message = message + c;
        // We have read the whole message
        if (c == (char)4) {
            return message;
//            handle_messaging(message);
        }
    }
    return message;
}

void cont_recv() {
    String message = "";
    while (server_connection.available()) {
        char c = '\0';
        c = server_connection.read();
//        Serial.write(c);
        message = message + c;
        // We have read the whole message
        if (c == (char)4 || c == '=') {
            Serial.println("Found the end of the message");
            if (message.length() > 1) {
                message = message.substring(0, message.length()-1);
                handle_messaging(message);
                message = "";
                return;
            }
        }
    }
}




void handle_messaging(String message) {
    Message recvd_msg(message);
    
    Serial.println("");
    Serial.println("");
    Serial.print("Received message: ");
    Serial.println(recvd_msg.get_pack());
    Serial.print("Source ID: ");
    Serial.println(recvd_msg.get_source_id());
    Serial.print("Dest ID: ");
    Serial.println(recvd_msg.get_destination_id());
    Serial.print("Command: ");
    Serial.println(recvd_msg.get_command());
    Serial.print("Phase: ");
    Serial.println(recvd_msg.get_phase());
    Serial.print("Data: ");
    Serial.println(recvd_msg.get_data());

    switch(recvd_msg.get_command()) {
        case QUERY_TYPE: {
            switch(recvd_msg.get_phase()) {
                // Case 0 is the flood phase: Not handled by feathers because it has nothing to forward it on to
                case 1: {// Respond phase
                    Serial.println("Received query phase 1");
                    // Check to see if the ID matches mine?
//                    String source_id = recvd_msg.get_destination_id();
                    String destination_id = recvd_msg.get_source_id();
                    String await_id = recvd_msg.get_await_id();
                    int phase = recvd_msg.get_phase()+1;
                    Serial.println(await_id);
                    if (await_id != "") {
                        destination_id = destination_id + ";" + await_id;
                    }
                    CommandType command = QUERY_TYPE;
                    unsigned int nonce = recvd_msg.get_nonce();
                    String timestamp = recvd_msg.get_timestamp() + 2;
                    String data = "{ \"reading\": 23.1, \"timestamp\": \"" + timestamp + "\" }"; // Bogus data

                    Message message(device_id, destination_id, command, phase, data, nonce);

                    String response = message.get_pack();
//                    Serial.println("");
//                    Serial.print("Responding with: ");
//                    Serial.println(response);
//                    Serial.print("Source ID: ");
//                    Serial.println(recvd_msg.get_source_id());
//                    Serial.print("Dest ID: ");
//                    Serial.println(recvd_msg.get_destination_id());
//                    Serial.print("Command: ");
//                    Serial.println(recvd_msg.get_command());
//                    Serial.print("Phase: ");
//                    Serial.println(recvd_msg.get_phase());
//                    Serial.print("Data: ");
//                    Serial.println(recvd_msg.get_data());
                    Serial.print("Sending message: ");
                    Serial.println(response);
                    Serial.print("Message data: ");
                    Serial.println(message.get_data());
                    Serial.println("");
                    server_connection.print(response);
                    server_connection.flush();
                    delay(100);
                    break;
                }
                // Case 2 is the aggregate phase: Not handled by feathers because they don't have anything to receive from
                case 3: {// Close phase
                    Serial.println();
                    Serial.println("RECEIVED CLOSE PHASE");
                    Serial.println();
                    break;
                }
            }
            break;
        }
        case INFORMATION_TYPE: {
            switch(recvd_msg.get_phase()) {
                // Case 0 is the flood phase: Not handled by feathers because it has nothing to forward it on to
                case 1: {// Respond phase
                    break;
                }
                // Case 2 is the aggregate phase: Not handled by feathers because they don't have anything to receive from
                case 3: {// Close phase
                    break;
                }
            }
            break;
        }
//        case HEARTBEAT_TYPE:
//            break;
    }
}

void printWiFiStatus() {
    // print the SSID of the network you're attached to:
    Serial.print("SSID: ");
    Serial.println(WiFi.SSID());

    // print your WiFi shield's IP address:
    IPAddress ip = WiFi.localIP();
    Serial.print("IP Address: ");
    Serial.println(ip);

    // print the received signal strength:
    long rssi = WiFi.RSSI();
    Serial.print("signal strength (RSSI):");
    Serial.print(rssi);
    Serial.println(" dBm");
    // print where to go in a browser:
    Serial.print("To see this page in action, open a browser to http://");
    Serial.println(ip);

}

void listNetworks() {
    // Scan for nearby networks:
    Serial.println("** Scan Networks **");
    int numSsid = WiFi.scanNetworks();
    if (numSsid == -1) {
        Serial.println("Couldn't get a wifi connection");
        while (true);
    }

    // Print the list of networks seen:
    Serial.print("number of available networks:");
    Serial.println(numSsid);

    for (int i = 0; i < NETWORK_LIST_SIZE; i++) {
        memset(AP_names[i], '\0', AP_NAME_LEN);
    }
    // Print the network number and name for each network found:
    for (int thisNet = 0; thisNet < numSsid; thisNet++) {
        Serial.print(thisNet);
        Serial.print(") ");
        Serial.print(WiFi.SSID(thisNet));
        Serial.print("\tSignal: ");
        Serial.print(WiFi.RSSI(thisNet));
        Serial.print(" dBm");
        Serial.print("\tEncryption: ");
        printEncryptionType(WiFi.encryptionType(thisNet));
        Serial.flush();
        strncpy(AP_names[thisNet], WiFi.SSID(thisNet), (AP_NAME_LEN-1));
    }
}

void printEncryptionType(int thisType) {
    // read the encryption type and print out the name:
    switch (thisType) {
        case ENC_TYPE_WEP:
            Serial.println("WEP");
            break;
        case ENC_TYPE_TKIP:
            Serial.println("WPA");
            break;
        case ENC_TYPE_CCMP:
            Serial.println("WPA2");
            break;
        case ENC_TYPE_NONE:
            Serial.println("None");
            break;
        case ENC_TYPE_AUTO:
            Serial.println("Auto");
            break;
    }
}

void printMacAddress(byte mac[]) {
    for (int i = 5; i >= 0; i--) {
        if (mac[i] < 16) {
            Serial.print("0");
        }
        Serial.print(mac[i], HEX);
        if (i > 0) {
            Serial.print(":");
        }
    }
    Serial.println();
}

void scan_available_networks() {
    // Scan for existing networks:
    while (1) {
        Serial.println("Scanning available networks...");
        int brypt_found = 0;
        listNetworks();
        for (int i = 0; i < NETWORK_LIST_SIZE; i++) {
             Serial.print("Item is: ");
             Serial.println(AP_names[i]);
            if (strncmp("brypt", AP_names[i], 5) == 0) {
                // Serial.println("Found brypt thing");
                brypt_found = 1;
            }
        }
        if (brypt_found == 1) {
            break;
        }
//       TEMPORARY, DELETE LATER
//       break;
        // last_scan = millis();
        delay(10000);
    }
}

void start_access_point(char ssid[]) {
    // Print the network name (SSID);
    Serial.print("Creating access point named: ");
    Serial.println(ssid);

    // Create open network. Change this line if you want to create an WEP network:
    wifi_status = WiFi.beginAP(ssid);
    if (wifi_status != WL_AP_LISTENING) {
        Serial.println("Creating access point failed");
        // Block forever
        while (true);
    }

    // Start the web server on port 80
    server.begin();

    // Print the wifi status now that we are connected
    printWiFiStatus();
}

// add source id to web portal and add it to the parsing
void send_http_response(WiFiClient client) {
  // HTTP headers always start with a response code (e.g. HTTP/1.1 200 OK)
  // and a content-type so the client knows what's coming, then a blank line:
  client.println("HTTP/1.1 200 OK");
  client.println("Content-type:text/html");
  client.println();

  client.println("<body style=\"display: flex;    flex-direction: column;    justify-content: flex-start;    align-items: center;    background: #F4F7F8;    background: linear-gradient(to right, #E6E9F0, #F4F7F8);\">    <div class=\"box\" style=\"display: flex;    flex-direction: row;    justify-content: center;    align-items: center;    flex-wrap: nowrap;    height: 640px;    width: 420px;    background: #FBFBFB;    border-radius: 8px;    box-shadow: 0px 0px 20px rgba(30, 30, 30, 0.15);    box-sizing: border-box;    color: #4A5C62;    font-family: 'Source Sans Pro', sans-serif;    font-size: 14px;    font-weight: 300;    line-height: 1.15;    margin-top: 50px;\">  <form action=\"/action_page.php\">      <h1 style=\"position: relative;    width: 80%;    margin-bottom: 30px;    font-size: 2em;    color: #2D383C;    text-transform: uppercase;    text-align: center;    font-weight: 700;    font-family: 'Chivo', sans-serif;    margin-block-start: 0.83em;    margin-block-end: 0.83em;    box-sizing: border-box;    display: block;    line-height: 1.15;    margin: 0 auto;\">Available connections</h1>      <br>      <div class=\"listelems\" style=\"position: relative;    overflow-y: scroll;    height: 200px;    text-align: center;    width: 250px;    display: block;    position: relative;    display: block;    margin: 0 auto;\">");
  for (int i = 0; i < 10; i++) {
      if (strcmp(AP_names[i], "") != 0) {
          client.print("<div class=\"net-item\" style=\"padding-top: 5px;    padding-bottom: 5px;    transition: 0.3s ease-in-out all;\"><p>(");
          client.print(i);
          client.print(") ");
          client.print(AP_names[i]);
          client.println("</p></div>");
      }
  }
  // For some reason when adding styling to the inputs it causes issues
  client.println("</div>      <br>      <div class=\"entries\" style=\"text-align: center;    background: #FBFBFB;    border-radius: 8px;    box-sizing: border-box;    color: #4A5C62;    font-family: 'Source Sans Pro', sans-serif;    font-size: 14px;    font-weight: 300;    line-height: 1.15;\">      <input type=\"text\" name=\"ap_name\" placeholder=\"Access Point Number\" style=\"\">      <input type=\"text\" name=\"host_ip\" placeholder=\"Host IP Address\" style=\"\">      <input type=\"text\" name=\"host_port\" placeholder=\"Host Port Number\" style=\"\">      <input type=\"text\" name=\"device_id\" placeholder=\"Device ID Number\" style=\"\">      <input type=\"submit\" value=\"Submit\" style=\"position: relative;font-size: 1em;    font-weight: 700;    font-family: 'Chivo', sans-serif;    display: block;    position: relative;    top: 0px;    width: 250px;    height: 45px;    margin: 0 auto;margin-top: 10px;    font-size: 1.4em;    font-weight: 700;    text-transform: uppercase;    letter-spacing: 0.1em;    color: rgba(251, 251, 251, 0.6);    cursor: pointer;    -webkit-appearance: none;    background: rgb(26, 204, 149);    background: linear-gradient(to right top, rgb(26, 204, 148) 0%, rgb(67, 231, 179) 100%);    opacity: 0.85;    border: none;    box-shadow: 0px 0px 20px rgba(26, 204, 149, 0.5);    transition: 0.3s ease-in-out all;    border-radius: 4px;    text-decoration: none;\">      </div>  </form>    </div></body>");
  client.println();
}

int check_wifi_status(int wifi_status) {
    if (wifi_status != WiFi.status()) {
        // it has changed update the variable
        wifi_status = WiFi.status();
      
        if (wifi_status == WL_AP_CONNECTED) {
            byte remoteMac[6];
      
            // a device has connected to the AP
            Serial.print("Device connected to AP, MAC address: ");
            WiFi.APClientMacAddress(remoteMac);
            printMacAddress(remoteMac);
            // WiFi.end();
            // conn_stat = 1;
        } else {
            // a device has disconnected from the AP, and we are back in listening mode
            Serial.println("Device disconnected from AP");
        }
    }
    return wifi_status;
}

String handle_form_submission(WiFiClient client, String currentLine) {
    if (currentLine.indexOf("action_page") > 0) {
        int index = currentLine.indexOf("ap_name=");
        access_point = currentLine.substring(index+8);
        Serial.print("Access point is: ");
        Serial.println(access_point);
        int flag = 0;
        for (int i = 0; i < access_point.length(); i++) {
            if (access_point.charAt(i) == ' ') {
              Serial.println("Found space");
              access_point.setCharAt(i, '\0');
              break;
            }
        }
        Serial.print("Access point: ");
        Serial.println(access_point);
        client.stop();
        WiFi.end();
        state = AP_DETERMINED;
    }
}

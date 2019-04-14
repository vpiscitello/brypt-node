#include <SPI.h>
#include <WiFi101.h>

#include "message.hpp"
#include "utility.hpp"

//#include "zmqduino.h"
// #include "arduino_connection.hpp"

const int NETWORK_LIST_SIZE = 20;
const int AP_NAME_LEN = 40;

// Enum to keep track of current state
enum CurrentState { DETERMINE_NETWORK, AP_DETERMINED, AP_CONNECTED, SERVER_CONNECTED };


// int last_scan = 0;
// const int SCAN_DURATION = 10000;

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

// CONFIG IP address of server to connect to
IPAddress remote_server(192,168,30,5);

WiFiClient server_connection;

String data = "Hello World!";      // Set the message data
int phase = 0;                          // Set the message command phase

void setup() {
    // Configure pins for Adafruit ATWINC1500 Feather
    WiFi.setPins(8,7,4,2);

    //Initialize serial and wait for port to open:
    Serial.begin(9600);
    while (!Serial) {
	      ; // wait for serial port to connect. Needed for native USB port only
    }

    // Turn the LED on in order to indicate that the upload is complete
    pinMode(builtin_led, OUTPUT);

    // Check for the presence of the shield:
    if (WiFi.status() == WL_NO_SHIELD) {
      	Serial.println("No WiFi shield");
      	// Block forever
      	while (true);
    }

    scan_available_networks();

    // by default the local IP address of will be 192.168.1.1
    // you can override it with the following:
    // WiFi.config(IPAddress(10, 0, 0, 1));

    char ssid[] = "feather_m0_setup";
    start_access_point(ssid);

//    // TESTING
//    Serial.println("Ending AP");
//    WiFi.end();
//    state = AP_DETERMINED;
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
                        if (currentLine.indexOf("ap_name=") > 0) {
                            int ap_num = currentLine.indexOf("ap_name=");
                            currentLine = currentLine.substring(ap_num);
                            int ampersand_index = currentLine.indexOf("&");
                            String ap_num_str = currentLine.substring(ap_num+8, ampersand_index);
                            Serial.print("ap_num value is ");
                            Serial.print(ap_num_str);
                            Serial.println("");
                            int flag = 0;
                            for (int i = 0; i < ap_num_str.length(); i++) {
                                if (ap_num_str.charAt(i) == ' ') {
                                    Serial.println("Found space");
                                    ap_num_str.setCharAt(i, '\0');
                                    flag = 1;
                                }
                                if (flag == 1) {
                                    ap_num_str.setCharAt(i, '\0');
                                }
                            }
                            access_point = AP_names[ap_num_str.toInt()];
                            Serial.print("Access point: ");
                            Serial.println(access_point);
                        }
                        if (currentLine.indexOf("host_ip=") > 0) {
                            int host_ip = currentLine.indexOf("host_ip=");
                            currentLine = currentLine.substring(host_ip);
                            int ampersand_index = currentLine.indexOf("&");
                            String host_ip_str = currentLine.substring(host_ip+8, ampersand_index);
                            Serial.print("Host IP value is ");
                            Serial.print(host_ip_str);
                            Serial.println("");
                        }
                        if (currentLine.indexOf("host_port=") > 0) {
                            int host_port = currentLine.indexOf("host_port=");
                            currentLine = currentLine.substring(host_port);
                            int ampersand_index = currentLine.indexOf("&");
                            String host_port_str = currentLine.substring(host_port+10, ampersand_index);
                            Serial.print("Host IP value is ");
                            Serial.print(host_port_str);
                            Serial.println("");
                        }
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
  	    // We have discovered an AP name so we want to connect to the WiFi network
  	    while (wifi_status != WL_CONNECTED) {
        		Serial.print("Attempting to connect to WPA SSID: ");
        		Serial.println(access_point);
        		// Connect to WPA/WPA2 network:
//            		wifi_status = WiFi.begin("brypt1", "asdfasdf");
  		      wifi_status = WiFi.begin(access_point);
        
        		delay(1000);
  	    }
  
  	    // Print out information about the network
  	    Serial.print("Successfully connected to the network");
  	    printWiFiStatus();
  	    // printCurrentNet();
  	    // printWiFiData();
  
  	    // Get the current time now that we (maybe) have internet
  	    Serial.print("Wifi Time: ");
  	    Serial.println(WiFi.getTime());
  	    state = AP_CONNECTED;
  	} else if (state == AP_CONNECTED) {
  	    Serial.println("\nStarting connection to server...");
  	    // Try to connect to the TCP or StreamBridge socket
  	    if (server_connection.connect(remote_server, 3001)) {
        		Serial.println("Connected to server");
        		state = SERVER_CONNECTED;
  	    } else {
            delay(1000);
  	    }
  	} else if (state = SERVER_CONNECTED) {
  	    // As soon as we have connected to the socket, send a message

        // TODO Add the handshake process
  	    Serial.print("Message is: ");
        String msg = "Hello";
        Serial.println(msg);
        server_connection.println(msg);
  	    server_connection.flush();
  	    // server_connection.stop();
  	    delay(100);
        String message;
  	    while (server_connection.available()) {
        		char c = server_connection.read();
        		Serial.write(c);
            message = message + c;
            // TODO determine when we have read the whole message
            handle_messaging(message);
  	    }
  
  	    Serial.println();
  	    delay(1000);
  	}
}

void handle_messaging(String message) {
    Message recvd_msg(message);

    Serial.print("Command: ");
    Serial.println(recvd_msg.get_command());
    Serial.print("Phase: ");
    Serial.println(recvd_msg.get_phase());
    Serial.print("Data: ");
    Serial.println(recvd_msg.get_data());

    switch(recvd_msg.get_command()) {
        case QUERY_TYPE:
            switch(recvd_msg.get_phase()) {
                case 0: // Flood phase
                    // Not handled by feathers because it has nothing to forward it on to
                    break
                case 1: // Respond phase
                    // get my own ID?
                    String source_id = recvd_msg.get_destination_id();
                    String destination_id = recvd_msg.get_source_id();
                    String await_id = recvd_msg.get_await_id();
                    if (await_id != "") {
                        destination_id = destination_id + ";" + await_id;
                    }
                    unsigned int nonce = recvd_msg.get_nonce() + 1;
                    String data = "23.1";

                    Message message(source_id, destination_id, command, phase, data, nonce);

                    String response = message.get_pack();
                    server_connection.println(response);
                    server_connection.flush();
                    break
                case 2: // Aggregate phase
                    // Not handled by feathers because they don't have anything to receive from
                    break
                case 3: // Close phase
                    break
            }
            break;
        case INFORMATION_TYPE:
            switch(recvd_msg.get_phase()) {
                case 0: // Flood phase
                    // Not handled by feathers because it has nothing to forward it on to
                    break
                case 1: // Respond phase
                    break
                case 2: // Aggregate phase
                    // Not handled by feathers because they don't have anything to receive from
                    break
                case 3: // Close phase
                    break
            }
            break;
//        case HEARTBEAT_TYPE:
//            break;
    }
}

// void printWiFiData() {
//   // Print your WiFi shield's IP address:
//   IPAddress ip = WiFi.localIP();
//   Serial.print("IP Address: ");
//   Serial.println(ip);
//   // Serial.println(ip);
//
//   // Print your MAC address:
//   byte mac[6];
//   WiFi.macAddress(mac);
//   Serial.print("MAC address: ");
//   printMacAddress(mac);
//
// }
//
// void printCurrentNet() {
//   // Print the SSID of the network you're attached to:
//   Serial.print("Current network SSID: ");
//   Serial.println(WiFi.SSID());
//
//   // Print the MAC address of the router you're attached to:
//   byte bssid[6];
//   WiFi.BSSID(bssid);
//   Serial.print("BSSID: ");
//   printMacAddress(bssid);
//
//   // Print the received signal strength:
//   long rssi = WiFi.RSSI();
//   Serial.print("Signal strength (RSSI):");
//   Serial.println(rssi);
//
//   // Print the encryption type:
//   byte encryption = WiFi.encryptionType();
//   Serial.print("Encryption Type:");
//   Serial.println(encryption, HEX);
//   Serial.println();
// }

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

  client.println("<body style=\"display: flex;    flex-direction: column;    justify-content: flex-start;    align-items: center;    background: #F4F7F8;    background: linear-gradient(to right, #E6E9F0, #F4F7F8);\">    <div class=\"box\" style=\"display: flex;    flex-direction: row;    justify-content: center;    align-items: center;    flex-wrap: nowrap;    height: 580px;    width: 420px;    background: #FBFBFB;    border-radius: 8px;    box-shadow: 0px 0px 20px rgba(30, 30, 30, 0.15);    box-sizing: border-box;    color: #4A5C62;    font-family: 'Source Sans Pro', sans-serif;    font-size: 14px;    font-weight: 300;    line-height: 1.15;    margin-top: 50px;\">  <form action=\"/action_page.php\">      <h1 style=\"position: relative;    width: 80%;    margin-bottom: 30px;    font-size: 2em;    color: #2D383C;    text-transform: uppercase;    text-align: center;    font-weight: 700;    font-family: 'Chivo', sans-serif;    margin-block-start: 0.83em;    margin-block-end: 0.83em;    box-sizing: border-box;    display: block;    line-height: 1.15;    margin: 0 auto;\">Available connections</h1>      <br>      <div class=\"listelems\" style=\"position: relative;    overflow-y: scroll;    height: 200px;    text-align: center;    width: 250px;    display: block;    position: relative;    display: block;    margin: 0 auto;\">");
  for (int i = 0; i < 10; i++) {
      if (strcmp(AP_names[i], "") != 0) {
          client.print("<div class=\"net-item\" style=\"padding-top: 5px;    padding-bottom: 5px;    transition: 0.3s ease-in-out all; hover {    top: -4px;    background: rgba(30, 30, 30, 0.15);    opacity: 1;    box-shadow: 0px 4px 20px rgba(20, 20, 20, 0.15);    transition: 0.3s ease-in-out all;}\"><p>(");
          client.print(i);
          client.print(") ");
          client.print(AP_names[i]);
          client.println("</p></div>");
      }
  }

  client.println("</div>      <br>      <div class=\"entries\" style=\"text-align: center;    background: #FBFBFB;    border-radius: 8px;    box-sizing: border-box;    color: #4A5C62;    font-family: 'Source Sans Pro', sans-serif;    font-size: 14px;    font-weight: 300;    line-height: 1.15;\">      <input type=\"text\" name=\"ap_name\" placeholder=\"Access Point Number\" style=\"position: relative;margin-top: 10px;    background: rgba(126, 154, 148, 0.05);    box-shadow: none;    width: 60%;    padding: 12px 14px;    border: 2px solid transparent;    border-radius: 4px;    outline: none;focus {    border: 2px solid rgba(26, 204, 149, 1);    box-shadow: 0px 0px 20px rgba(26, 204, 149, 0.4);}\">      <input type=\"text\" name=\"host_ip\" placeholder=\"Host IP Address\" style=\"position: relative;margin-top: 10px;    background: rgba(126, 154, 148, 0.05);    box-shadow: none;    width: 60%;    padding: 12px 14px;    border: 2px solid transparent;    border-radius: 4px;    outline: none;focus {    border: 2px solid rgba(26, 204, 149, 1);    box-shadow: 0px 0px 20px rgba(26, 204, 149, 0.4);}\">      <input type=\"text\" name=\"host_port\" placeholder=\"Host Port Number\" style=\"position: relative;margin-top: 10px;    background: rgba(126, 154, 148, 0.05);    box-shadow: none;    width: 60%;    padding: 12px 14px;    border: 2px solid transparent;    border-radius: 4px;    outline: none;focus {    border: 2px solid rgba(26, 204, 149, 1);    box-shadow: 0px 0px 20px rgba(26, 204, 149, 0.4);}\">      <input type=\"submit\" value=\"Submit\" style=\"position: relative;margin-top: 10px;font-size: 1em;    font-weight: 700;    font-family: 'Chivo', sans-serif;    display: block;    position: relative;    top: 0px;    width: 250px;    height: 45px;    margin: 0 auto; margin-top: 10px;    font-size: 1.4em;    font-weight: 700;    text-transform: uppercase;    letter-spacing: 0.1em;    color: rgba(251, 251, 251, 0.6);    cursor: pointer;    -webkit-appearance: none;    background: rgb(26, 204, 149);    background: linear-gradient(to right top, rgb(26, 204, 148) 0%, rgb(67, 231, 179) 100%);    opacity: 0.85;    border: none;    box-shadow: 0px 0px 20px rgba(26, 204, 149, 0.5);    transition: 0.3s ease-in-out all;    border-radius: 4px;    text-decoration: none;hover {    top: -4px;    color: rgba(251, 251, 251, 1);    background: rgb(26, 204, 149);    background: linear-gradient(to right top, rgb(26, 204, 148) 25%, rgb(67, 231, 179) 100%);    opacity: 1;    box-shadow: 0px 4px 20px rgba(26, 204, 149, 0.6);}\">      </div>  </form>    </div></body>");
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

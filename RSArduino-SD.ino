/*
 * RSArduino Remote Server Administration Code
 *
 * Requires:
 * - RSArduino Shield and Arduino Uno (or compatible)
 * - Remote temp Sensors
 * - Wires installed to computer interface
 *
 *   by  Braden Licastro [braden(at)fullforceapps.com]
 *   and Devin Boyer     [devin(at)fullforceapps.com]
 * 
 * Pro:
 * - HTTP Request managed.
 * - Switch page selection.
 * - HTML pages on flash memory.
 *
 * Cons:
 * - No explicit parameters management.
 */
 
#include <SD.h>
#include <SPI.h>
#include <EthernetV2_0.h>

/****************************************************************************************
*          Device Pin Configuration and Other Definitions
*****************************************************************************************/
#define W5200_CS  10
#define SDCARD_CS 4
#define REQ_BUF_SZ 20
#define FN_SZ 13

/****************************************************************************************
*          Ethernet configuration
*****************************************************************************************/
byte mac[] PROGMEM = { 0xDE, 0x05, 0x97, 0x2C, 0x61, 0xB3 }; // Unique MAC
// byte mac[] PROGMEM = { 0x00, 0x90, 0xA9, 0xA1, 0x9E, 0xD6 }; // WDTVLiveHub MAC
// IPAddress ip(192,168,1,137); // Only needed for static, change code below.

// Initialize ethernet server (Default HTTP port 80)
EthernetServer server(80);

/****************************************************************************************
*          Global Variable Declarations
*****************************************************************************************/
File webFile;
char filename[FN_SZ];
char HTTP_req[REQ_BUF_SZ] = {0};
char req_index = 0;

/****************************************************************************************
*          Initial Configuration
*****************************************************************************************/
void setup() {
    Serial.begin(9600); // Baud rate for console monitoring

    // Initialize SD card before web server
    Serial.print(F("Initializing SD card..."));
    // Set up correct initial pin config
    pinMode(W5200_CS, OUTPUT);
    digitalWrite(W5200_CS,HIGH);
    pinMode(SDCARD_CS,OUTPUT);
    
    // Initialize the SDCard Module
    if (!SD.begin(SDCARD_CS)) {
        Serial.println(F(" FAIL"));
        return;
    }
    Serial.println(F(" SUCCESS"));
    
    // Check for necessary files for web server
    //if(!SD.exists("index.htm")) {
    //    Serial.println(F("ERROR - Can't find index file!"));
    //    return;
    //}
    //Serial.println(F("Found index file."));
    digitalWrite(SDCARD_CS, HIGH);
    
    Serial.print(F("Starting Free Memory: "));
    Serial.println(FreeRam());
    
    // Start the Ethernet connection
    if (Ethernet.begin(mac) == 0) { // DHCP Assigned
    //if (Ethernet.begin(mac, ip) == 0) { // Static IP
        Serial.println(F("Ethernet Enable: FAIL"));
        // No point in carrying on
        for(;;)
            ;
    }
    Serial.print(F("Now Free Memory: "));
    Serial.println(FreeRam());
    server.begin();
    
    // Log vital stats
    Serial.println(F("Ethernet Enable: SUCCESS"));
    Serial.print(F("IP: "));
    Serial.println(Ethernet.localIP());
    Serial.println();
}

/****************************************************************************************
*          Main Program
*****************************************************************************************/
void loop() {
    // Try to get a client
    EthernetClient client = server.available();
    
    // We got one!!! *Ghost busters, du duh dun dun dun!*
    if (client) {
        // HTTP Requests must end with a blank line
        boolean currentLineIsBlank = true;
        
        // We have a new client!
        //Serial.println(F("New client connected."));
        while (client.connected()) {
            if (client.available()) {   // client data available to read
                // Read 1 byte (character) from client
                char c = client.read();
                
                if (req_index < (REQ_BUF_SZ - 1)) {
                  HTTP_req[req_index] = c;
                  req_index++;
                }
                
                // DEBUG: Print HTTP request character to serial monitor
                //Serial.print(c);
                
                // Last line of client request is blank and ends with \n
                // Respond to client only after last line received
                if (c == '\n' && currentLineIsBlank) {
                    // Open requested web page file
                    ExtractFileName();
                    
                    // Determine http request type and work from there
                    if(StrContains(filename, ".htm") || !StrContains(filename, ".")) {

                        if (!SD.exists(filename)) {
                            // Change the file to the 404 file and return the right header
                            strcpy(filename, "404.htm");
                            client.println(F("HTTP/1.1 404 NOT FOUND"));
                        } else {
                            // send a standard http response header
                            client.println(F("HTTP/1.1 200 OK"));
                        }
                        client.println(F("Content-Type: text/html"));
                        client.println(F("Connection: close"));
                        client.println();
                        webFile = SD.open(filename);
                    } else if (StrContains(filename, ".gif") || StrContains(filename, ".bmp") || StrContains(filename, ".png") || StrContains(filename, ".jpg")) {
                        webFile = SD.open(filename);
                        if (webFile) {
                            client.println(F("HTTP/1.1 200 OK"));
                            client.println();
                        }
                    }

                    // Send web page
                    if (webFile) {
                        while(webFile.available()) {
                            // Send web page to client
                            client.write(webFile.read());
                        }
                        webFile.close();
                    }
                    // Reset indices and clear filename and HTTP request
                    req_index = 0;
                    StrClear(HTTP_req, REQ_BUF_SZ);
                    StrClear(filename, FN_SZ);
                    break;
                }
                
                // Every line of text received from the client ends with \r\n
                if (c == '\n') {
                    // Last character on line of received text
                    // Starting new line with next character read
                    currentLineIsBlank = true;
                } 
                else if (c != '\r') {
                    // A text character was received from client
                    currentLineIsBlank = false;
                }
            }
        }
        
        // Give the web browser time to receive the data then close the connection
        delay(1);
        client.stop();
        
        //Serial.println(F("HTTP Request completed.\nSession Closed."));
    }
}

/****************************************************************************************
*          Searches for the string sfind in the string str
*              Returns 1 if string found
*              Returns 0 if string not found
*****************************************************************************************/
char StrContains(char *str, char *sfind) {
    char found = 0;
    char index = 0;
    char len;

    len = strlen(str);
    
    if (strlen(sfind) > len) {
        return 0;
    }
    while (index < len) {
        if (str[index] == sfind[found]) {
            found++;
            if (strlen(sfind) == found) {
                return 1;
            }
        } else {
            found = 0;
        }
        index++;
    }
    return 0;
}

/****************************************************************************************
*          Extract the file being requested from the header
*              Returns the filename if specified
*              Returns index.htm if root requested (/)
*****************************************************************************************/
void ExtractFileName() {
    int i, j;
    
    i=0;
    while (HTTP_req[i++] != ' ') {
        ;
    }
    
    j=0;
    while (HTTP_req[i] != ' ' && j < FN_SZ) {
        filename[j++] = HTTP_req[i++];
    }
    filename[j] = 0;
    if (filename[0] == '/' && filename[1] == 0) {
        strcpy(filename, "index.htm");
    }
}

/****************************************************************************************
*          Sets every element of str to 0
*              Clears Array, Garbage Cleanup
*****************************************************************************************/
void StrClear(char *str, char length) {
    for(int i = 0; i < length; i++) {
        str[i] = 0;
    }
}

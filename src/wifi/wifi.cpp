/*
   TriSensorWiFi
   Forked from: https://github.com/javos65/EasyWifi-for-MKR1010
*/

#include "wifi.h"
#include "form_html.h"
#include "gen404_html.h"
#include "success_html.h"
#include "../libyuarel/yuarel.h"

// #define DBGON    //  Uncomment to enable debugging: Prints messages to serial console and uses LED to show status
// #define DBGON_X   // Include packets in Serial debug stream

// Constructor
TriSensorWiFi::TriSensorWiFi() {
}

int TriSensorWiFi::status() {
  return WiFi.status();
}

// Login to local network  //
void TriSensorWiFi::start() {
  int conn_attempts = 0;

  WiFi.disconnect();
  delay(2000);

  #ifdef DBGON
  nina_led(READY_TO_START);
  #endif

  int wifi_status = WiFi.status();

  while ((wifi_status != WL_CONNECTED) || (WiFi.RSSI() <= -90) || (WiFi.RSSI() == 0)) {
    // Load credentials from flash if available.
    if (read_wifi_credentials() == 0) {
      #ifdef DBGON
      nina_led(NO_CREDS_FOUND);
      #endif
    }
    else if (read_mqtt_credentials() == 0) {
      #ifdef DBGON
      nina_led(NO_CREDS_FOUND);
      #endif
    }
    else {
      // Attempt to connect if credentials were loaded from flash.
      Serial.println("* Loaded credentials from flash");
      while (((wifi_status != WL_CONNECTED) || (WiFi.RSSI() <= -90) || (WiFi.RSSI() == 0)) && conn_attempts < MAXCONNECT) {

        #ifdef DBGON
        Serial.print("* Attempt #");
        Serial.print(conn_attempts + 1);
        Serial.print(" to connect to using stored credentials.");
        Serial.println(wifi_creds.ssid);
        #endif

        wifi_status = WiFi.begin(wifi_creds.ssid, wifi_creds.password);
        delay(2000);
        conn_attempts++;
      }
    }

    if (wifi_status == WL_CONNECTED) {
      #ifdef DBGON
      nina_led(CONNECTED);
      print_wifi_status();
      #endif
    }
    else {
      #ifdef DBGON
      nina_led(OPENING_AP);
      Serial.println("* Opening Access Point");
      #endif

      ap_setup();

      ap_input_flag = 0;

      while (ap_input_flag == 0) { // Keep AP open till input is received
        if (ap_status != WiFi.status()) {
          ap_status = WiFi.status();
          if (ap_status == WL_AP_CONNECTED) {
            #ifdef DBGON
            nina_led(CLIENT_CONNECTED);
            Serial.println("* Device connected to AP");
            #endif

            dns_req_count = 0;
          }
          else { // a device has disconnected from the AP, and we are back in listening mode
            #ifdef DBGON
            nina_led(OPENING_AP);
            Serial.println("* Device disconnected from AP");
            #endif
          }
        }

        if (ap_status == WL_AP_CONNECTED) {
          ap_dns_scan();
          ap_wifi_client_check();
        }

        delay(500);
      }

      udpap_dns.stop();
      WiFi.end();
      WiFi.disconnect();

      #ifdef DBGON
      nina_led(READY_TO_START);
      #endif

      delay(2000);
    }
  }

  #ifdef DBGON
  nina_led(CONNECTED);
  Serial.println("* Already connected.");
  print_wifi_status();
  #endif
}

void TriSensorWiFi::print_wifi_status() {
  #ifdef DBGON
    // print the SSID of the network you're attached to:
    Serial.print("* SSID: ");
    Serial.print(WiFi.SSID());

    // print your WiFi shield's IP address:
    IPAddress ip = WiFi.localIP();
    Serial.print(" - IP Address: ");
    Serial.print(ip);

    // print your WiFi gateway:
    IPAddress ip2 = WiFi.gatewayIP();
    Serial.print(" - IP Gateway: ");
    Serial.print(ip2);

    // print the received signal strength:
    long rssi = WiFi.RSSI();
    Serial.print("- Rssi: ");
    Serial.print(rssi);
    Serial.println(" dBm");
  #endif
}

/**
 * Erases wifi and mqtt credentials stored on flash.
 * @return 1 if both credential files were erased, 0 if there was an error erasing one or both files.
 */
bool TriSensorWiFi::erase() {
  int results = 0;
  results += erase_wifi_credentials();
  results += erase_mqtt_credentials();

  return (results == 2);
}

// Shut down the NINA chip
void TriSensorWiFi::end() {
  Serial.println("* Disconnecting from WiFi network.");
  WiFi.disconnect();
  delay(1000);
  WiFi.end();
  delay(1000);
}

// Set Name of AccessPoint
byte TriSensorWiFi::apname(char * name) {
  int t = 0;
  while (name[t] != 0) {
    ap_name[t] = name[t];
    t++;
    if (t >= SSIDBUFFERSIZE) break;
  }
  ap_name[t] = 0; // close string
  return t;
}

void TriSensorWiFi::get_mqtt_creds(char *host, char *port, char *user, char *pass) {
  strcpy(host, mqtt_creds.host);
  strcpy(port, mqtt_creds.port);
  strcpy(user, mqtt_creds.username);
  strcpy(pass, mqtt_creds.password);
}

void TriSensorWiFi::get_name(char *name) {
  strcpy(name, sensor_name);
}

/* Wifi Acces Point Initialization */
void TriSensorWiFi::ap_setup() {
  int tr = 5; // Maximum attempts to setup AP

  #ifdef DBGON
  Serial.print("* Creating access point named: ");
  Serial.println(ap_name);
  #endif

  // Generate random IP adress in 172.0.0.0/24 private IP range, with last octet always equal to 1.
  ap_ipaddr = IPAddress(172, (char) random(0, 255), (char) random(0, 255), 0x01);
  WiFi.end(); // close Wifi - just to be sure
  delay(3000);

  // The AP will also serve as the gateway and DNS server.
  WiFi.config(ap_ipaddr, ap_ipaddr, ap_ipaddr, IPAddress(255, 255, 255, 0));

  // Wait until access point is listening
  while (tr > 0) {
    ap_status = WiFi.beginAP(ap_name, APCHANNEL);
    if (ap_status != WL_AP_LISTENING) {
      #ifdef DBGON
      Serial.print(".");
      #endif

      --tr;
      //WiFi.disconnect();
      // Why was this here?  Config was already set a few lines earlier.
      // WiFi.config(ap_ipaddr, ap_ipaddr, ap_ipaddr, IPAddress(255, 255, 255, 0));
      delay(2000);
    } else break; // break while loop when AP is connected
  }

  if (tr == 0) { // not possible to connect
    #ifdef DBGON
    Serial.println("* Failed to create access point.");
    #endif

    return;
  }

  delay(2000);
  print_wifi_status();
  udpap_dns.begin(UDPPORT); // start UDP server
  web_server.begin(); // start the AP web server on port 80
}

/* DNS Routines via UDP, act on DNS requests on Port 53*/
/* assumes wifi UDP service has been started */
void TriSensorWiFi::ap_dns_scan() {
  int t = 0; // generic loop counter
  int r, p; // reply and packet counters
  unsigned int packet_size = 0;
  unsigned int reply_size = 0;
  byte dns_reply_buffer[UDP_PACKET_SIZE];
  packet_size = udpap_dns.parsePacket();

  if (packet_size) { // We've received a packet, read the data from it
    udpap_dns.read(udp_packet_buffer, packet_size);
    client_ipaddr = udpap_dns.remoteIP();
    dns_client_port = udpap_dns.remotePort();

    if ((client_ipaddr != ap_ipaddr)) { // skip own requests - ie ntp-pool time requestfrom Wifi module
      #ifdef DBGON_X
      Serial.print("DNS-packets (");
      Serial.print(packet_size);
      Serial.print(") from ");
      Serial.print(client_ipaddr);
      Serial.print(" port ");
      Serial.println(dns_client_port);
      for (t = 0; t < packet_size; ++t) {
        Serial.print(udp_packet_buffer[t], HEX);
        Serial.print(":");
      }
      Serial.println(" ");
      for (t = 0; t < packet_size; ++t) {
        Serial.print((char) udp_packet_buffer[t]);
      }
      Serial.println("");
      #endif

      //Copy Packet ID and IP into DNS header and DNS answer
      dns_reply_header[0] = udp_packet_buffer[0];
      dns_reply_header[1] = udp_packet_buffer[1]; // Copy ID of Packet offset 0 in Header
      dns_reply_answer[12] = ap_ipaddr[0];
      dns_reply_answer[13] = ap_ipaddr[1];
      dns_reply_answer[14] = ap_ipaddr[2];
      dns_reply_answer[15] = ap_ipaddr[3]; // copy AP Ip adress offset 12 in Answer
      r = 0; // set reply buffer counter
      p = 12; // set packetbuffer counter @ QUESTION QNAME section

      // copy Header into reply
      for (t = 0; t < DNSHEADER_SIZE; ++t) dns_reply_buffer[r++] = dns_reply_header[t];

      // copy Qusetion into reply:  Name labels till octet=0x00
      while (udp_packet_buffer[p] != 0) dns_reply_buffer[r++] = udp_packet_buffer[p++];

      // copy end of question plus Qtype and Qclass 5 octets
      for (t = 0; t < 5; ++t) dns_reply_buffer[r++] = udp_packet_buffer[p++];

      //copy Answer into reply
      for (t = 0; t < DNSANSWER_SIZE; ++t) dns_reply_buffer[r++] = dns_reply_answer[t];
      reply_size = r;

      #ifdef DBGON_X
      Serial.print("* DNS-Reply (");
      Serial.print(reply_size);
      Serial.print(") from ");
      Serial.print(ap_ipaddr);
      Serial.print(" port ");
      Serial.println(UDPPORT);
      for (t = 0; t < reply_size; ++t) {
        Serial.print(dns_reply_buffer[t], HEX);
        Serial.print(":");
      }
      Serial.println(" ");
      for (t = 0; t < reply_size; ++t) {
        Serial.print((char) dns_reply_buffer[t]);
      }
      Serial.println("");
      #endif

      // Send DNS UDP packet
      udpap_dns.beginPacket(client_ipaddr, dns_client_port);
      udpap_dns.write(dns_reply_buffer, reply_size);
      udpap_dns.endPacket();
      dns_req_count++;
    } // end loop correct IP
  } // end loop received packet
}

// Check the AP wifi Client Responses and read the inputs on the main AP web-page.
void TriSensorWiFi::ap_wifi_client_check() {

  WiFiClient client = web_server.available();

  if (client) { // if you get a client,
    #ifdef DBGON
    Serial.println("* New AP webclient"); // print a message out the serial port
    #endif

    // If the client has bytes available, begin handling the request.
    if (client.connected() && client.available()) {
      char c = client.read();
      char request_line[128];
      char *req_ptr = &request_line[0];

      // Get the request line containing the HTTP operation and route requested
      while (c != '\n' && c != '\r') {
        *req_ptr = c;
        req_ptr++;
        c = client.read();
      }

      // End the request_line with a null terminator.
      *req_ptr = '\0';

      #ifdef DBGON
      Serial.print("* Request line: ");
      Serial.println(request_line);
      #endif

      // Now, check the first line with header info and jump to the appropriate route

      if (strncmp(request_line, "GET /hotspot-detect.html", 24) == 0) {
        #ifdef DBGON
        Serial.println("* Handling GET /hotspot-detect.html");
        #endif

        client.println(FORM_HTML);
      }

      else if (strncmp(request_line, "GET /generate_204", 17) == 0) {
        #ifdef DBGON
        Serial.println("* Handling GET /generate_204");
        #endif

        client.println(GEN404_HTML);
      }

      else if (strncmp(request_line, "POST /checkpass.html", 20) == 0) {
        #ifdef DBGON
        Serial.println("* Handling POST /checkpass.html");
        #endif

        char current_line[1024];
        char *current_line_ptr = &current_line[0];

        // The POST request body will include multiple lines with headers, followed by an empty line,
        // followed by a line containing the application/x-www-form-urlencoded parameters.
        // So every time we encounter a newline, clear current_line, then at the end all that
        // is left is the param line.
        while (client.connected()) {
          if (client.available()) {
            c = client.read();

            #ifdef DBGON_X
            Serial.write(c); // print it out the serial monitor
            #endif

            if (c == '\n') { // if the byte is a newline character
              *current_line_ptr = '\0';
              #ifdef DBGON
              Serial.print("* (HTTP Header) ");
              Serial.println(current_line);
              #endif
              current_line_ptr = &current_line[0];
            }
            else if (c != '\r') {
              *current_line_ptr = c;
              current_line_ptr++;
            }
          }
          else {
            // Done reading from client
            ap_input_flag = 1;
            *current_line_ptr = '\0';
            break;
          }
        }

        #ifdef DBGON
        Serial.print("* POST body: ");
        Serial.println(current_line);
        #endif

        struct yuarel_param params[3];
        int p = yuarel_parse_query(current_line, '&', params, 8);

        bool inputs_valid = true;
        for (int i=0; i<p; i++) {
          if (strcmp(params[i].key, "wifi_ssid") == 0) {
            url_decode_in_place(params[i].val);
            strcpy(wifi_creds.ssid, params[i].val);
          }
          else if (strcmp(params[i].key, "wifi_pass") == 0) {
            url_decode_in_place(params[i].val);
            strcpy(wifi_creds.password, params[i].val);
          }
          else if (strcmp(params[i].key, "mqtt_host") == 0) {
            url_decode_in_place(params[i].val);
            strcpy(mqtt_creds.host, params[i].val);
          }
          else if (strcmp(params[i].key, "mqtt_user") == 0) {
            url_decode_in_place(params[i].val);
            strcpy(mqtt_creds.username, params[i].val);
          }
          else if (strcmp(params[i].key, "mqtt_pass") == 0) {
            url_decode_in_place(params[i].val);
            strcpy(mqtt_creds.password, params[i].val);
          }
          else if (strcmp(params[i].key, "mqtt_port") == 0) {
            url_decode_in_place(params[i].val);
            int port = atoi(params[i].val);

            // Port must be a positive integer. use default mqtt port as a fallback.
            if (port < 1) {
              port = 1883;
            }

            char port_buf[8];
            itoa(port, port_buf, 10);
            strcpy(mqtt_creds.port, port_buf);
          }
          else if (strcmp(params[i].key, "device_name") == 0) {
            url_decode_in_place(params[i].val);
            strcpy(sensor_name, params[i].val);
          }
        }

        if (inputs_valid) {
          write_wifi_credentials();
          write_mqtt_credentials();
        }
        else {
          #ifdef DBGON
          Serial.println("Failed to write credentials because form inputs were not valid.");
          #endif
        }

        client.println(SUCCESS_HTML);

        delay(2000);
      }

      while(client.available()) {
        // Discard any remaining client data because client.connected() will return true
        // unless all data has been read, even if the client has been closed.
        client.read();
      }

      client.stop();
    }

    #ifdef DBGON
    Serial.println("* AP Client not connected, or no bytes were available to read.");
    #endif
  }
}

/**
 * Reads stored wifi credentials from file and inserts them into wifi_creds struct
 * @return The number of bytes read from file
 */
byte TriSensorWiFi::read_wifi_credentials() {
  int u, t, c = 0;
  char buf[256], comma = 1, new_line = '\n';
  char bufc[32];  // sized to largest string it needs to hold, but ssid and password are limited to 32
  WiFiStorageFile file = WiFiStorage.open(WIFI_CRED_FILE);

  if (file) {
    file.seek(0);

    // read file buffer into memory, max size is 64 bytes for 2 wifi strings + 192 bytes for mqtt creds
    if (file.available()) {
      c = file.read(buf, 256);
    }

    // If the first character is an end of string, we got nothing.  Bail.
    if (c != 0) {
      t = 0;
      u = 0;
      while (buf[t] != comma) { // read from buffer until a comma is encountered
        bufc[u++] = buf[t++];
        if (u > 31) break;
      }

      bufc[u] = 0;
      strcpy(wifi_creds.ssid, bufc);
      u = 0;

      t++; // move to second part: wifi pass
      while (buf[t] != new_line) { // read from buffer until new line is encountered
        bufc[u++] = buf[t++];
        if (u > 31) break;
      }

      bufc[u] = 0;
      strcpy(wifi_creds.password, bufc);
    }

    file.close();

    #ifdef DBGON
    Serial.print("* Successfully read wifi credentials. SSID: ");
    Serial.println(wifi_creds.ssid);
    #endif

    return (c);
  } else {
    file.close();

    #ifdef DBGON
    Serial.println("* Failed to read wifi credentials.");
    #endif

    return (0);
  }
}

/**
 * Writes WiFi credentials to flash file, comma separated
 * @return The number of bytes written.
 */
byte TriSensorWiFi::write_wifi_credentials() {
  int c = 0;
  char comma = 1, zero = 0;
  char buf[32];

  WiFiStorageFile file = WiFiStorage.open(WIFI_CRED_FILE);

  if (file) {
    file.erase();
  }

  c = file.write(wifi_creds.ssid, sizeof(wifi_creds.ssid));
  c += file.write(&comma, 1);

  c += file.write(wifi_creds.password, sizeof(wifi_creds.password));
  c += file.write(&zero, 1);

  if (c != 0) {
    #ifdef DBGON
    Serial.print("* Wrote wifi credentials. Total bytes: ");
    Serial.println(c);
    #endif
    file.close();
    return (c);
  }
  else {
    #ifdef DBGON
    Serial.println("* Failed to write wifi credentials");
    #endif
    file.close();
    return (0);
  }
}

/**
 * Erases wifi credentials stored on flash.
 * @return 1 if credentials were successfully erased, 0 if they aren't.
 */
byte TriSensorWiFi::erase_wifi_credentials() {
  // First check if the file exists at all
  if (!check_wifi_credential_file()) return 0;

  WiFiStorageFile file = WiFiStorage.open(WIFI_CRED_FILE);

  if (file) {
    file.seek(0);
    file.erase();
    file.close();

    #ifdef DBGON
    Serial.println("* Erased wifi credentials file : ");
    #endif

    return (1);
  }
  else {
    #ifdef DBGON
    Serial.println("* Failed to erase wifi credentials file.");
    #endif

    file.close();
    return (0);
  }
}

/**
 * Reads stored mqtt credentials from file and inserts them into mqtt_creds struct
 * @return The number of bytes read from file
 */
byte TriSensorWiFi::read_mqtt_credentials() {
  int u, t, c = 0;
  char buf[256], comma = 1, new_line = '\n';
  char bufc[128];  // sized to largest string it needs to hold, which is the mqtt host
  WiFiStorageFile file = WiFiStorage.open(MQTT_CRED_FILE);

  if (file) {
    file.seek(0);

    // read file buffer into memory, 128 host + 8 port + 32 user + 32 password = 200
    if (file.available()) {
      c = file.read(buf, 256);
    }

    // If the first character is a newline, we don't have anything in the file so skip to the end.
    if (c != 0) {
      t = 0; // counter for complete stored string
      u = 0; // counter for each piece of stored credential

      while (buf[t] != comma) { // read from buffer until a comma is encountered
        bufc[u++] = buf[t++];
        if (u > 127) break;
      }

      bufc[u] = 0; // add a null termination to the host string & copy to credential struct
      strcpy(mqtt_creds.host, bufc);

      // move to second part: mqtt port
      u = 0;
      t++;

      while (buf[t] != comma) { // read from buffer until a comma is encountered
        bufc[u++] = buf[t++];
        if (u > 7) break;
      }

      bufc[u] = 0; // add a null termination to the port string & copy to credential struct
      strcpy(mqtt_creds.port, bufc);

      // move to third part: mqtt username
      u = 0;
      t++;

      while (buf[t] != comma) { // read from buffer until a comma is encountered
        bufc[u++] = buf[t++];
        if (u > 31) break;
      }

      bufc[u] = 0; // add a null termination to the port string, and copy to credential struct
      strcpy(mqtt_creds.username, bufc);

      // move to fourth part: mqtt password
      u = 0;
      t++;

      while (buf[t] != comma) { // read from buffer until comma is encountered
        bufc[u++] = buf[t++];
        if (u > 31) break;
      }

      bufc[u] = 0; // add a null termination to the password string, and copy to credential struct
      strcpy(mqtt_creds.password, bufc);

      // move to fifth part: sensor name
      u = 0;
      t++;

      while (buf[t] != new_line) { // read from buffer until new_line is encountered
        bufc[u++] = buf[t++];
        if (u > 31) break;
      }

      bufc[u] = 0; // add a null termination to the sensor name, and copy to variable
      strcpy(sensor_name, bufc);
    }

    file.close();

    #ifdef DBGON
    Serial.print("* Successfully read mqtt credentials. Total bytes: ");
    Serial.println(t);
    #endif

    return (c);
  } else {
    file.close();

    #ifdef DBGON
    Serial.println("* Failed to read mqtt credentials. Sad face emoji.");
    #endif

    return (0);
  }
}

/**
 * Writes mqtt credentials to flash file, comma separated. Username and password are hashed.
 * host,port,username,password
 * @return The number of bytes written.
 */
byte TriSensorWiFi::write_mqtt_credentials() {
  int c = 0;
  char comma = 1, zero = 0;

  WiFiStorageFile file = WiFiStorage.open(MQTT_CRED_FILE);

  if (file) {
    file.erase();
  }

  c = file.write(mqtt_creds.host, sizeof(mqtt_creds.host));
  c += file.write(&comma, 1);

  c += file.write(mqtt_creds.port, sizeof(mqtt_creds.port));
  c += file.write(&comma, 1);

  c += file.write(mqtt_creds.username, sizeof(mqtt_creds.username));
  c += file.write(&comma, 1);

  c += file.write(mqtt_creds.password, sizeof(mqtt_creds.password));
  c += file.write(&comma, 1);

  c += file.write(sensor_name, sizeof(sensor_name));
  c += file.write(&zero, 1);

  file.close();

  if (c != 0) {
    #ifdef DBGON
    Serial.print("* Wrote mqtt credentials. Total bytes: ");
    Serial.println(c);
    #endif

    return (c);
  }
  else {
    #ifdef DBGON
    Serial.println("* Failed to write mqtt credentials");
    #endif

    return (0);
  }
}

/**
 * Erases mqtt credentials stored on flash.
 * @return 1 if credentials were successfully erased, 0 if they aren't.
 */
byte TriSensorWiFi::erase_mqtt_credentials() {
  // First check if the file exists at all
  if (!check_mqtt_credential_file()) return 0;

  WiFiStorageFile file = WiFiStorage.open(MQTT_CRED_FILE);

  if (file) {
    file.seek(0);
    file.erase();

    #ifdef DBGON
    Serial.print("* Erased mqtt credentials file : ");
    Serial.println(file);
    #endif

    file.close();
    return (1);
  }
  else {
    #ifdef DBGON
    Serial.println("* Could not erase mqtt credentials file. This might mean the file does not exist.");
    #endif

    file.close();
    return (0);
  }
}

/**
 * Checks whether the WiFi credential file exists
 * @return boolean true if the file is found, false if it is NOT found
*/
bool TriSensorWiFi::check_wifi_credential_file() {
  WiFiStorageFile file = WiFiStorage.open(WIFI_CRED_FILE);

  if (file) {
    #ifdef DBGON
    Serial.print("* WiFi credentials file found: ");
    Serial.println(WIFI_CRED_FILE);
    #endif
    file.close();
    return true;
  }
  else {
    #ifdef DBGON
    Serial.print("* WiFi credential file NOT found: ");
    Serial.println(WIFI_CRED_FILE);
    #endif
    file.close();
    return false;
  }
}

/**
 * Checks whether the MQTT credential file exists
 * @return boolean true if the file is found, false if it is NOT found
 */
bool TriSensorWiFi::check_mqtt_credential_file() {
  WiFiStorageFile file = WiFiStorage.open(MQTT_CRED_FILE);

  if (file) {
    #ifdef DBGON
    Serial.print("* MQTT credentials file found: ");
    Serial.println(MQTT_CRED_FILE);
    #endif
    file.close();
    return true;
  }
  else {
    #ifdef DBGON
    Serial.print("* MQTT credential file NOT found: ");
    Serial.println(MQTT_CRED_FILE);
    #endif
    file.close();
    return false;
  }
}

/* Set RGB led on uBlox Module R-G-B , max 128*/
void TriSensorWiFi::nina_led(char r, char g, char b) {
  // Set LED pin modes to output
  WiFiDrv::pinMode(25, OUTPUT);
  WiFiDrv::pinMode(26, OUTPUT);
  WiFiDrv::pinMode(27, OUTPUT);

  // Set all LED color
  WiFiDrv::analogWrite(25, g % 128); // GREEN
  WiFiDrv::analogWrite(26, r % 128); // RED
  WiFiDrv::analogWrite(27, b % 128); // BLUE
}

/**
 * Decodes url parameters
 * @param dst The decoded output string
 * @param src The url encoded input string
 */
void TriSensorWiFi::url_decode(char *dst, const char *src) {
  char a, b;
  while (*src) {
    if ((*src == '%') && (a = src[1]) && (b = src[2]) && isxdigit(a) && isxdigit(b)) {
      if (a >= 'a') {
        a -= 'a' - 'A';
      }
      if (a >= 'A') {
        a -= ('A' - 10);
      }
      else {
        a -= '0';
      }

      if (b >= 'a') {
        b -= 'a' - 'A';
      }
      if (b >= 'A') {
        b -= ('A' - 10);
      }
      else {
        b -= '0';
      }

      *dst++ = 16 * a + b;
      src += 3;
    } else if (*src == '+') {
      *dst++ = ' ';
      src++;
    } else {
      *dst++ = *src++;
    }
  }
  *dst++ = '\0';
}

/**
 * Replaces a char array containing an encuded url string with its decoded equivalent
 * @param input The url string to be decoded
 */
void TriSensorWiFi::url_decode_in_place(char *input) {
  char *orig = input;
  char decoded[strlen(input)];

  TriSensorWiFi::url_decode(decoded, input);

  input = orig;
  strcpy(input, decoded);
}

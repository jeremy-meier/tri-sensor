 /*
 * TriSensorWiFi
 *
 * Forked from: https://github.com/javos65/EasyWifi-for-MKR1010
 */
#ifndef EASYWIFI_H
#define EASYWIFI_H

#include "Arduino.h"
#include <WiFiNINA.h>
#include <WiFiUdp.h>

#define SSIDBUFFERSIZE 32
#define APCHANNEL  5 // AP wifi channel

#define WIFI_CRED_FILE "/fs/wifi_creds"
#define MQTT_CRED_FILE "/fs/mqtt_creds"

#define MAXCONNECT 3                       // Max number of wifi logon connects before opening AP
#define ESCAPECONNECT 15                   // Max number of Total wifi logon retries-connects before escaping/stopping the Wifi start

// Define UDP settings for DNS
#define UDP_PACKET_SIZE 1024          // UDP packet size time out, preventign too large packet reads
#define DNSHEADER_SIZE 12             // DNS Header
#define DNSANSWER_SIZE 16             // DNS Answer = standard set with Packet Compression
#define DNSMAXREQUESTS 32             // trigger first DNS requests, to redirect to own web-page
#define UDPPORT  53                   // local port to listen for UDP packets

// Define RGB values for NINALed
#define NOT_CONNECTED 16,0,0      //RED
#define NO_CREDS_FOUND 5,3,0      //ORANGE
#define CONNECTED 0,8,0           //GREEN
#define READY_TO_START 0,0,20     //BLUE
#define OPENING_AP 6,0,10         //PURPLE
#define CLIENT_CONNECTED 0,6,10   //CYAN

struct WiFiCreds {
  char ssid[32] = "";
  char password[32] = "";
};

struct MqttCreds {
  char host[128];
  char port[8];
  char username[32];
  char password[32];
};

class TriSensorWiFi {
  public:
    TriSensorWiFi();
    int status();
    void start();
    bool erase();
    byte apname(char *name);
    void end();
    void get_mqtt_creds(char *host, char *port, char *user, char *pass);
    void get_name(char *name);

  private:
    byte dns_reply_header[DNSHEADER_SIZE] = {
      0x00,
      0x00, // ID, to be filled in #offset 0
      0x81,
      0x80, // answer header Codes
      0x00,
      0x01, //QDCOUNT = 1 question
      0x00,
      0x01, //ANCOUNT = 1 answer
      0x00,
      0x00, //NSCOUNT / ignore
      0x00,
      0x00 //ARCOUNT / ignore
    };

    byte dns_reply_answer[DNSANSWER_SIZE] = {
      0xc0,
      0x0c, // pointer to pos 12 : NAME Labels
      0x00,
      0x01, // TYPE
      0x00,
      0x01, // CLASS
      0x00,
      0x00, // TTL
      0x18,
      0x4c, // TLL 2 days
      0x00,
      0x04, // RDLENGTH = 4
      0x00,
      0x00, // IP adress octets to be filled #offset 12
      0x00,
      0x00 // IP adress octeds to be filled
    };

    char ap_name[SSIDBUFFERSIZE];
    int ap_status = WL_IDLE_STATUS;
    int ap_input_flag;

    struct WiFiCreds wifi_creds = {};
    struct MqttCreds mqtt_creds = {};

    char sensor_name[32];

    int dns_client_port;
    int dns_req_count = 0;

    byte udp_packet_buffer[UDP_PACKET_SIZE];

    WiFiServer web_server = WiFiServer(80);
    WiFiUDP udpap_dns;
    IPAddress ap_ipaddr;
    IPAddress client_ipaddr;

    static void url_decode(char *dst, const char *src);
    static void url_decode_in_place(char *input);

    bool check_wifi_credential_file();
    byte erase_wifi_credentials();
    byte write_wifi_credentials();
    byte read_wifi_credentials();

    bool check_mqtt_credential_file();
    byte erase_mqtt_credentials();
    byte write_mqtt_credentials();
    byte read_mqtt_credentials();

    void ap_wifi_client_check();
    void ap_dns_scan();
    void list_networks();
    void ap_setup();
    void print_wifi_status();
    void nina_led(char r, char g, char b);
};

#endif

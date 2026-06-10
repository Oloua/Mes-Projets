/*
 * Copyright (C) 2016 Unwired Devices <info@unwds.com>
 *               2017 Inria Chile
 *               2017 Inria
 *
 * This file is subject to the terms and conditions of the GNU Lesser
 * General Public License v2.1. See the file LICENSE in the top level
 * directory for more details.
 */

/**
 * @ingroup     tests
 * @{
 * @file
 * @brief       Test application for SX127X modem driver
 *
 * @author      Eugene P. <ep@unwds.com>
 * @author      José Ignacio Alamos <jose.alamos@inria.cl>
 * @author      Alexandre Abadie <alexandre.abadie@inria.fr>
 * @}
 */

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>

#include "thread.h"
#include "shell.h"
//#include "shell_commands.h"

#include "net/netdev.h"
#include "net/netdev/lora.h"
#include "net/lora.h"

#include "board.h"

#include "sx127x_internal.h"
#include "sx127x_params.h"
#include "sx127x_netdev.h"

#include <ctype.h>

#include "fmt.h"

#define SX127X_LORA_MSG_QUEUE   (16U)
#ifndef SX127X_STACKSIZE
#define SX127X_STACKSIZE        (THREAD_STACKSIZE_DEFAULT)
#endif

#define MSG_TYPE_ISR            (0x3456)

/*Partie Meshtastic*/

#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>

static sx127x_t sx127x;

struct __attribute__((__packed__)) MeshtasticHeader
{
	uint32_t destination_id; // The destination's unique NodeID. 0xFFFFFFFF for broadcast. Little Endian.
	uint32_t source_id; //  The sender's unique NodeID. Little Endian.
	uint32_t packet_id; // The sending node's unique packet ID for this packet. Little Endian.

	uint8_t hope_start: 3;  // 0:2 original HopLimit
	uint8_t from_mqtt: 1;   // 3:3 ViaMQTT (packet came via MQTT)
	uint8_t want_ack: 1;    // 4:4
	uint8_t hop_limit: 3;   // 5:7

	uint8_t channel_hash; // Channel hash. Used as hint for decryption for the receiver.

	uint8_t next_hop; // Next-hop used for relaying
	uint8_t relay_node; // Relay node of the current transmission

	uint8_t pb_payload[237]; // Max. 237 bytes (excl. protobuf overhead)
};

typedef struct MeshtasticHeader MeshtasticHeader_t;

/** Check Size */
bool meshtastic_check_valid_frame_size(const uint8_t *frame_buffer, const uint8_t frame_size);

/** Get Destination Id */
uint32_t meshtastic_get_destination_id(const uint8_t *frame_buffer, const uint8_t frame_size);

/** Get Source Id */
uint32_t meshtastic_get_source_id(const uint8_t *frame_buffer, const uint8_t frame_size);

/** Get Packet Id */
uint32_t meshtastic_get_packet_id(const uint8_t *frame_buffer, const uint8_t frame_size);

/** Get Hop Limit */
uint8_t meshtastic_get_hop_limit(const uint8_t *frame_buffer, const uint8_t frame_size);

/** Get Hope Start */
uint8_t meshtastic_get_hope_start(const uint8_t *frame_buffer, const uint8_t frame_size);

/** Is From MQTT */
bool meshtastic_is_from_mqtt(const uint8_t *frame_buffer, const uint8_t frame_size);

/** Is Want Ack */
bool meshtastic_is_want_ack(const uint8_t *frame_buffer, const uint8_t frame_size);

/** Get Channel Hash */
uint8_t meshtastic_get_channel_hash(const uint8_t *frame_buffer, const uint8_t frame_size);

/** Get Protobuf Payload */
void meshtastic_get_pb_payload(const uint8_t *frame_buffer, const uint8_t frame_size, uint8_t *payload, uint8_t* payload_size);

/** Print payload */
void meshtastic_printf(const uint8_t *frame_buffer, const uint8_t frame_size);

/** Check Size */
bool meshtastic_check_valid_frame_size(const uint8_t *frame_buffer, const uint8_t frame_size) {
	(void)frame_buffer;
	return frame_size >= 16;
}

/** Get Destination Id */
uint32_t meshtastic_get_destination_id(const uint8_t *frame_buffer, const uint8_t frame_size) {
	(void)frame_size;
	return ((MeshtasticHeader_t*)frame_buffer)->destination_id;
}

/** Get Source Id */
uint32_t meshtastic_get_source_id(const uint8_t *frame_buffer, const uint8_t frame_size) {
	(void)frame_size;
	return ((MeshtasticHeader_t*)frame_buffer)->source_id;
}

/** Get Packet Id */
uint32_t meshtastic_get_packet_id(const uint8_t *frame_buffer, const uint8_t frame_size) {
	(void)frame_size;
	return ((MeshtasticHeader_t*)frame_buffer)->packet_id;
}

/** Get Hop Limit */
uint8_t meshtastic_get_hop_limit(const uint8_t *frame_buffer, const uint8_t frame_size) {
	(void)frame_size;
	return ((MeshtasticHeader_t*)frame_buffer)->hop_limit;
}

/** Get Hope Start */
uint8_t meshtastic_get_hope_start(const uint8_t *frame_buffer, const uint8_t frame_size) {
	(void)frame_size;
	return ((MeshtasticHeader_t*)frame_buffer)->hope_start;
}

/** Is From MQTT */
bool meshtastic_is_from_mqtt(const uint8_t *frame_buffer, const uint8_t frame_size) {
	(void)frame_size;
	return ((MeshtasticHeader_t*)frame_buffer)->from_mqtt == 1;
}

/** Is Want Ack */
bool meshtastic_is_want_ack(const uint8_t *frame_buffer, const uint8_t frame_size) {
	(void)frame_size;
	return ((MeshtasticHeader_t*)frame_buffer)->want_ack == 1;
}

/** Get Channel Hash */
uint8_t meshtastic_get_channel_hash(const uint8_t *frame_buffer, const uint8_t frame_size) {
	(void)frame_size;
	return ((MeshtasticHeader_t*)frame_buffer)->channel_hash;
}

/** Get Protobuf Payload */
void meshtastic_get_pb_payload(const uint8_t *frame_buffer, const uint8_t frame_size, uint8_t *payload, uint8_t* payload_size) {
	*payload_size = frame_size - 16;
	memcpy(payload, ((MeshtasticHeader_t*)frame_buffer)->pb_payload, *payload_size);
}

/** Print payload */
void meshtastic_printf(const uint8_t *frame_buffer, const uint8_t frame_size) {
	if(frame_size < 16) {
		printf("too short for a meshtastic frame\n");
		return;
	}

	const MeshtasticHeader_t*  m = (MeshtasticHeader_t*)frame_buffer;

	printf("destination_id: %8lx\n", m->destination_id);
	printf("source_id:      %8lx\n", m->source_id);
	printf("packet_id:      %8lx\n", m->packet_id);

	printf("hop_limit:      %d\n", m->hop_limit);
	printf("hope_start:     %d\n", m->hope_start);
	printf("from_mqtt:      %s\n", m->from_mqtt == 1 ? "true" : "false");
	printf("want_ack:       %s\n", m->want_ack == 1 ? "true" : "false");

	printf("channel_hash:   %2x\n", m->channel_hash);
	printf("pb_payload:     ");

	for (uint8_t j = 0; j < frame_size - 16; j++) {
		printf("%02X ", m->pb_payload[j]);
	}
	printf("\n");

	// TODO display encrypted protobuf payload
}
/*#########################################################"*/

static size_t convert_hex(uint8_t *dest, const char *src) {
    size_t i;
    int value;
    size_t len = strlen(src);

    for (i = 0; i < len / 2; i++) {
        if (sscanf(src + i * 2, "%2x", &value) != 1) {
            break;
        }
        dest[i] = (uint8_t)value;
    }
    return i;
}

static void print_hex(const uint8_t *data, size_t len) {
    for (size_t i = 0; i < len; i++) {
        printf("%02X ", data[i]);
    }
    puts("");
}

static uint8_t sx127x_hex_payload[255];
static sx127x_t sx127x;

//pour envoyer un message hexadécimal
int sendhex_cmd(int argc, char **argv) {
    if (argc <= 1) {
        puts("usage: sendhex <hexpayload>");
        return -1;
    }

    const size_t len = convert_hex(sx127x_hex_payload, argv[1]);
    if (len == 0) {
        puts("Invalid hex payload");
        return -1;
    }

    netdev_t *netdev = &sx127x.netdev;

    iolist_t iolist = {
        .iol_base = sx127x_hex_payload,
        .iol_len  = len
    };

    if (netdev->driver->send(netdev, &iolist) == -ENOTSUP) {
        puts("Cannot send: radio is still transmitting");
        return -1;
    }

    printf("Sent hex payload (%zu bytes)\n", len);
    return 0;
}



static char stack[SX127X_STACKSIZE];
static kernel_pid_t _recv_pid;

static char message[32];

int lora_setup_cmd(int argc, char **argv)
{

    if (argc < 4) {
        puts("usage: setup "
             "<bandwidth (125, 250, 500)> "
             "<spreading factor (7..12)> "
             "<code rate (5..8)>");
        return -1;
    }

    /* Check bandwidth value */
    int bw = atoi(argv[1]);
    uint8_t lora_bw;

    switch (bw) {
    case 125:
        puts("setup: setting 125KHz bandwidth");
        lora_bw = LORA_BW_125_KHZ;
        break;

    case 250:
        puts("setup: setting 250KHz bandwidth");
        lora_bw = LORA_BW_250_KHZ;
        break;

    case 500:
        puts("setup: setting 500KHz bandwidth");
        lora_bw = LORA_BW_500_KHZ;
        break;

    default:
        puts("[Error] setup: invalid bandwidth value given, "
             "only 125, 250 or 500 allowed.");
        return -1;
    }

    /* Check spreading factor value */
    uint8_t lora_sf = atoi(argv[2]);

    if (lora_sf < 7 || lora_sf > 12) {
        puts("[Error] setup: invalid spreading factor value given");
        return -1;
    }

    /* Check coding rate value */
    int cr = atoi(argv[3]);

    if (cr < 5 || cr > 8) {
        puts("[Error ]setup: invalid coding rate value given");
        return -1;
    }
    uint8_t lora_cr = (uint8_t)(cr - 4);

    /* Configure radio device */
    netdev_t *netdev = &sx127x.netdev;

    netdev->driver->set(netdev, NETOPT_BANDWIDTH,
                        &lora_bw, sizeof(lora_bw));
    netdev->driver->set(netdev, NETOPT_SPREADING_FACTOR,
                        &lora_sf, sizeof(lora_sf));
    netdev->driver->set(netdev, NETOPT_CODING_RATE,
                        &lora_cr, sizeof(lora_cr));

    puts("[Info] setup: configuration set with success");

    return 0;
}

int random_cmd(int argc, char **argv)
{
    (void)argc;
    (void)argv;

    netdev_t *netdev = &sx127x.netdev;
    uint32_t rand;

    netdev->driver->get(netdev, NETOPT_RANDOM, &rand, sizeof(rand));
    printf("random: number from sx127x: %u\n",
           (unsigned int)rand);

    /* reinit the transceiver to default values */
    sx127x_init_radio_settings(&sx127x);

    return 0;
}

int register_cmd(int argc, char **argv)
{
    if (argc < 2) {
        puts("usage: register <get | set>");
        return -1;
    }

    if (strstr(argv[1], "get") != NULL) {
        if (argc < 3) {
            puts("usage: register get <all | allinline | regnum>");
            return -1;
        }

        if (strcmp(argv[2], "all") == 0) {
            puts("- listing all registers -");
            uint8_t reg = 0, data = 0;
            /* Listing registers map */
            puts("Reg   0  1  2  3  4  5  6  7  8  9  A  B  C  D  E  F");
            for (unsigned i = 0; i <= 7; i++) {
                printf("0x%02X ", i << 4);

                for (unsigned j = 0; j <= 15; j++, reg++) {
                    data = sx127x_reg_read(&sx127x, reg);
                    printf("%02X ", data);
                }
                puts("");
            }
            puts("-done-");
            return 0;
        }
        else if (strcmp(argv[2], "allinline") == 0) {
            puts("- listing all registers in one line -");
            /* Listing registers map */
            for (uint16_t reg = 0; reg < 256; reg++) {
                printf("%02X ", sx127x_reg_read(&sx127x, (uint8_t)reg));
            }
            puts("- done -");
            return 0;
        }
        else {
            long int num = 0;
            /* Register number in hex */
            if (strstr(argv[2], "0x") != NULL) {
                num = strtol(argv[2], NULL, 16);
            }
            else {
                num = atoi(argv[2]);
            }

            if (num >= 0 && num <= 255) {
                printf("[regs] 0x%02X = 0x%02X\n",
                       (uint8_t)num,
                       sx127x_reg_read(&sx127x, (uint8_t)num));
            }
            else {
                puts("regs: invalid register number specified");
                return -1;
            }
        }
    }
    else if (strstr(argv[1], "set") != NULL) {
        if (argc < 4) {
            puts("usage: register set <regnum> <value>");
            return -1;
        }

        long num, val;

        /* Register number in hex */
        if (strstr(argv[2], "0x") != NULL) {
            num = strtol(argv[2], NULL, 16);
        }
        else {
            num = atoi(argv[2]);
        }

        /* Register value in hex */
        if (strstr(argv[3], "0x") != NULL) {
            val = strtol(argv[3], NULL, 16);
        }
        else {
            val = atoi(argv[3]);
        }

        sx127x_reg_write(&sx127x, (uint8_t)num, (uint8_t)val);
    }
    else {
        puts("usage: register get <all | allinline | regnum>");
        return -1;
    }

    return 0;
}

int send_cmd(int argc, char **argv)
{
    if (argc <= 1) {
        puts("usage: send <payload>");
        return -1;
    }

    printf("sending \"%s\" payload (%u bytes)\n",
           argv[1], (unsigned)strlen(argv[1]) + 1);

    iolist_t iolist = {
        .iol_base = argv[1],
        .iol_len = (strlen(argv[1]) + 1)
    };

    netdev_t *netdev = &sx127x.netdev;

    if (netdev->driver->send(netdev, &iolist) == -ENOTSUP) {
        puts("Cannot send: radio is still transmitting");
    }

    return 0;
}

int listen_cmd(int argc, char **argv)
{
    (void)argc;
    (void)argv;

    netdev_t *netdev = &sx127x.netdev;
    /* Switch to continuous listen mode */
    const netopt_enable_t single = false;

    netdev->driver->set(netdev, NETOPT_SINGLE_RECEIVE, &single, sizeof(single));
    const uint32_t timeout = 0;

    netdev->driver->set(netdev, NETOPT_RX_TIMEOUT, &timeout, sizeof(timeout));

    /* Switch to RX state */
    netopt_state_t state = NETOPT_STATE_RX;

    netdev->driver->set(netdev, NETOPT_STATE, &state, sizeof(state));

    sx127x_set_preamble_length(&sx127x, 16);

    printf("Listen mode set\n");

    return 0;
}

int syncword_cmd(int argc, char **argv)
{
    if (argc < 2) {
        puts("usage: syncword <get|set>");
        return -1;
    }

    netdev_t *netdev = &sx127x.netdev;
    uint8_t syncword;

    if (strstr(argv[1], "get") != NULL) {
        netdev->driver->get(netdev, NETOPT_SYNCWORD, &syncword,
                            sizeof(syncword));
        printf("Syncword: 0x%02x\n", syncword);
        return 0;
    }

    if (strstr(argv[1], "set") != NULL) {
        if (argc < 3) {
            puts("usage: syncword set <syncword>");
            return -1;
        }
        syncword = fmt_hex_byte(argv[2]);
        netdev->driver->set(netdev, NETOPT_SYNCWORD, &syncword,
                            sizeof(syncword));
        printf("Syncword set to %02x\n", syncword);
    }
    else {
        puts("usage: syncword <get|set>");
        return -1;
    }

    return 0;
}
int channel_cmd(int argc, char **argv)
{
    if (argc < 2) {
        puts("usage: channel <get|set>");
        return -1;
    }

    netdev_t *netdev = &sx127x.netdev;
    uint32_t chan;

    if (strstr(argv[1], "get") != NULL) {
        netdev->driver->get(netdev, NETOPT_CHANNEL_FREQUENCY, &chan,
                            sizeof(chan));
        printf("Channel: %i\n", (int)chan);
        return 0;
    }

    if (strstr(argv[1], "set") != NULL) {
        if (argc < 3) {
            puts("usage: channel set <channel>");
            return -1;
        }
        chan = atoi(argv[2]);
        netdev->driver->set(netdev, NETOPT_CHANNEL_FREQUENCY, &chan,
                            sizeof(chan));
        printf("New channel set\n");
    }
    else {
        puts("usage: channel <get|set>");
        return -1;
    }

    return 0;
}

int rx_timeout_cmd(int argc, char **argv)
{
    if (argc < 2) {
        puts("usage: channel <get|set>");
        return -1;
    }

    netdev_t *netdev = &sx127x.netdev;
    uint16_t rx_timeout;

    if (strstr(argv[1], "set") != NULL) {
        if (argc < 3) {
            puts("usage: rx_timeout set <rx_timeout>");
            return -1;
        }
        rx_timeout = atoi(argv[2]);
        netdev->driver->set(netdev, NETOPT_RX_SYMBOL_TIMEOUT, &rx_timeout,
                            sizeof(rx_timeout));
        printf("rx_timeout set to %i\n", rx_timeout);
    }
    else {
        puts("usage: rx_timeout set");
        return -1;
    }

    return 0;
}

int reset_cmd(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    netdev_t *netdev = &sx127x.netdev;

    puts("resetting sx127x...");
    netopt_state_t state = NETOPT_STATE_RESET;

    netdev->driver->set(netdev, NETOPT_STATE, &state, sizeof(netopt_state_t));
    return 0;
}

static void _set_opt(netdev_t *netdev, netopt_t opt, bool val, char *str_help)
{
    netopt_enable_t en = val ? NETOPT_ENABLE : NETOPT_DISABLE;

    netdev->driver->set(netdev, opt, &en, sizeof(en));
    printf("Successfully ");
    if (val) {
        printf("enabled ");
    }
    else {
        printf("disabled ");
    }
    printf("%s\n", str_help);
}

int crc_cmd(int argc, char **argv)
{
    netdev_t *netdev = &sx127x.netdev;

    if (argc < 3 || strcmp(argv[1], "set") != 0) {
        printf("usage: %s set <1|0>\n", argv[0]);
        return 1;
    }

    int tmp = atoi(argv[2]);

    _set_opt(netdev, NETOPT_INTEGRITY_CHECK, tmp, "CRC check");
    return 0;
}

int implicit_cmd(int argc, char **argv)
{
    netdev_t *netdev = &sx127x.netdev;

    if (argc < 3 || strcmp(argv[1], "set") != 0) {
        printf("usage: %s set <1|0>\n", argv[0]);
        return 1;
    }

    int tmp = atoi(argv[2]);

    _set_opt(netdev, NETOPT_FIXED_HEADER, tmp, "implicit header");
    return 0;
}

int payload_cmd(int argc, char **argv)
{
    netdev_t *netdev = &sx127x.netdev;

    if (argc < 3 || strcmp(argv[1], "set") != 0) {
        printf("usage: %s set <payload length>\n", argv[0]);
        return 1;
    }

    uint16_t tmp = atoi(argv[2]);

    netdev->driver->set(netdev, NETOPT_PDU_SIZE, &tmp, sizeof(tmp));
    printf("Successfully set payload to %i\n", tmp);
    return 0;
}



static void _event_cb(netdev_t *dev, netdev_event_t event)
{
    if (event == NETDEV_EVENT_ISR) {
        msg_t msg;

        msg.type = MSG_TYPE_ISR;
        msg.content.ptr = dev;

        if (msg_send(&msg, _recv_pid) <= 0) {
            puts("gnrc_netdev: possibly lost interrupt.");
        }
    }
    else {
        size_t len;
        netdev_lora_rx_info_t packet_info;
        switch (event) {
        case NETDEV_EVENT_RX_STARTED:
            puts("Data reception started");
            break;

        case NETDEV_EVENT_RX_COMPLETE:
            len = dev->driver->recv(dev, NULL, 0, 0);
            dev->driver->recv(dev, message, len, &packet_info);
            printf(
                "{Payload: \"%s\" (%d bytes), RSSI: %i, SNR: %i, TOA: %" PRIu32 "}\n",
                message, (int)len,
                packet_info.rssi, (int)packet_info.snr,
                sx127x_get_time_on_air((const sx127x_t *)dev, len));

            printf("Payload (HEX): ");
   	    print_hex((uint8_t *)message, len);
   	    break;

        case NETDEV_EVENT_TX_COMPLETE:
            sx127x_set_sleep(&sx127x);
            puts("Transmission completed");
            break;

        case NETDEV_EVENT_CAD_DONE:
            break;

        case NETDEV_EVENT_TX_TIMEOUT:
            sx127x_set_sleep(&sx127x);
            break;

        default:
            printf("Unexpected netdev event received: %d\n", event);
            break;
        }
    }
}

void *_recv_thread(void *arg)
{
    (void)arg;

    static msg_t _msg_q[SX127X_LORA_MSG_QUEUE];

    msg_init_queue(_msg_q, SX127X_LORA_MSG_QUEUE);

    while (1) {
        msg_t msg;
        msg_receive(&msg);
        if (msg.type == MSG_TYPE_ISR) {
            netdev_t *dev = msg.content.ptr;
            dev->driver->isr(dev);
        }
        else {
            puts("Unexpected msg type");
        }
    }
}


int init_sx1272_cmd(int argc, char **argv)
{
    (void)argc;
    (void)argv;
	    sx127x.params = sx127x_params[0];
	    netdev_t *netdev = &sx127x.netdev;

	    netdev->driver = &sx127x_driver;

		sx127x_set_preamble_length(&sx127x, 16);  //ajout

        netdev->event_callback = _event_cb;

//        printf("%8x\n", (unsigned int)netdev->driver);
//        printf("%8x\n", (unsigned int)netdev->driver->init);

	    if (netdev->driver->init(netdev) < 0) {
	        puts("Failed to initialize SX127x device, exiting");
	        return 1;
	    }

	    _recv_pid = thread_create(stack, sizeof(stack), THREAD_PRIORITY_MAIN - 1,
	                              THREAD_CREATE_STACKTEST, _recv_thread, NULL,
	                              "recv_thread");

	    if (_recv_pid <= KERNEL_PID_UNDEF) {
	        puts("Creation of receiver thread failed");
	        return 1;
	    }
        puts("5");

        return 0;
}

int config_mesh_cmd(int argc, char **argv){
	(void)argc;
	(void)argv;
    // Exemple d'utilisation de argv[1]
    printf("Param received: %s\n", argv[1]);
        netdev_t *netdev = &sx127x.netdev;

        int chan = 869462500;
        //On configure le channel
         netdev->driver->set(netdev, NETOPT_CHANNEL_FREQUENCY, &chan,
                            sizeof(chan));
        // CRC_Check
        int tmp = 1;
        _set_opt(netdev, NETOPT_INTEGRITY_CHECK, tmp, "CRC check");
        // syncword

    uint8_t syncword = 0xAA;
        
        netdev->driver->set(netdev, NETOPT_SYNCWORD, &syncword,
                            sizeof(syncword));
          uint8_t lora_bw;
           lora_bw = LORA_BW_125_KHZ;
 uint8_t lora_sf = 11;
  int cr = 5;
 uint8_t lora_cr = (uint8_t)(cr - 4);
    //netdev_t *netdev = &sx127x.netdev;

    netdev->driver->set(netdev, NETOPT_BANDWIDTH,
                        &lora_bw, sizeof(lora_bw));
    netdev->driver->set(netdev, NETOPT_SPREADING_FACTOR,
                        &lora_sf, sizeof(lora_sf));
    netdev->driver->set(netdev, NETOPT_CODING_RATE,
   
                     &lora_cr, sizeof(lora_cr));
	return 0;
}
static const shell_command_t shell_commands[] = {
	{ "init",    "Initialize SX1272",     					init_sx1272_cmd },
	{ "setup",    "Initialize LoRa modulation settings",     lora_setup_cmd },
    { "implicit", "Enable implicit header",                  implicit_cmd },
    { "crc",      "Enable CRC",                              crc_cmd },
    { "payload",  "Set payload length (implicit header)",    payload_cmd },
    { "random",   "Get random number from sx127x",           random_cmd },
    { "syncword", "Get/Set the syncword",                    syncword_cmd },
    { "rx_timeout", "Set the RX timeout",                    rx_timeout_cmd },
    { "channel",  "Get/Set channel frequency (in Hz)",       channel_cmd },
    { "register", "Get/Set value(s) of registers of sx127x", register_cmd },
    { "send",     "Send raw payload string",                 send_cmd },
    { "listen",   "Start raw payload listener",              listen_cmd },
    { "reset",    "Reset the sx127x device",                 reset_cmd },
    { "sendhex", "Send hex payload via SX127x", sendhex_cmd },
    { "meshtastic", "configure meshtastic com", config_mesh_cmd},
    { NULL, NULL, NULL }
};

int main(void) {

    init_sx1272_cmd(0,NULL);

    /* start the shell */
    puts("Initialization successful - starting the shell now");
    char line_buf[SHELL_DEFAULT_BUFSIZE];

    shell_run(shell_commands, line_buf, SHELL_DEFAULT_BUFSIZE);

    return 0;
}

///////////////////////////////////////////////////////////////////////////////////////////////////

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>

#include "thread.h"
#include "shell.h"
#include "net/netdev.h"
#include "net/netdev/lora.h"
#include "net/lora.h"
#include "board.h"
#include "sx127x_internal.h"
#include "sx127x_params.h"
#include "sx127x_netdev.h"
#include "fmt.h"

#define SX127X_LORA_MSG_QUEUE   (16U)
#define SX127X_STACKSIZE        (THREAD_STACKSIZE_DEFAULT)
#define MSG_TYPE_ISR            (0x3456)
#define NODE_ID                 6   // ID de notre noeud

static sx127x_t sx127x;
static char stack[SX127X_STACKSIZE];
static kernel_pid_t _recv_pid;

//--------------------------------------------------
// Roles et types de message
//--------------------------------------------------
typedef enum { ROLE_SENDER=0, ROLE_ROUTER=1, ROLE_RECEIVER=2 } NodeRole;
typedef enum { MSG_ACK=0, MSG_NODEINFO=1, MSG_TEXT=2, MSG_TELEMETRY=3 } MsgType;

// Rôle actuel (modifiable dynamiquement)
static NodeRole nodeRole = ROLE_SENDER;

//--------------------------------------------------
// Trame Mesh
//--------------------------------------------------
struct __attribute__((__packed__)) MeshFrame {
    uint8_t to;
    uint8_t from;
    uint8_t hop;
    uint8_t hoplim;
    uint8_t role;
    uint8_t msg_type;
    uint32_t length;      // longueur du payload
    uint8_t payload[254]; // max 254
};
typedef struct MeshFrame MeshFrame_t;

//--------------------------------------------------
// Utils
//--------------------------------------------------
void printHex(const uint8_t *data, size_t len) {
    for(size_t i=0;i<len;i++) printf("%02X ", data[i]);
    printf("\n");
}

//--------------------------------------------------
// Mesh : envoi
//--------------------------------------------------
int sendMeshFrame(MeshFrame_t *frame){
    netdev_t *netdev = &sx127x.netdev;

    iolist_t iolist = {
        .iol_base = (uint8_t*)frame,
        .iol_len  = 6 + 4 + frame->length // header(6+length) + payload
    };

    if(netdev->driver->send(netdev, &iolist) == -ENOTSUP){
        puts("Cannot send: radio busy");
        return -1;
    }
    return 0;
}

void sendTextMessage(uint8_t to, const char *text){
    MeshFrame_t frame;
    frame.to = to;
    frame.from = NODE_ID;
    frame.hop = 0;
    frame.hoplim = 5;
    frame.role = nodeRole;
    frame.msg_type = MSG_TEXT;

    uint32_t len = strlen(text);
    if(len>254) len=254;
    frame.length = len;
    memcpy(frame.payload,text,len);

    sendMeshFrame(&frame);
}

//--------------------------------------------------
// Envoi ACK
//--------------------------------------------------
void sendAck(uint8_t to, uint8_t packet_hop){
    MeshFrame_t frame;
    frame.to = to;
    frame.from = NODE_ID;
    frame.hop = packet_hop;
    frame.hoplim = 1;       // pas de relais pour ACK
    frame.role = nodeRole;
    frame.msg_type = MSG_ACK;
    frame.length = 0;       // pas de payload
    sendMeshFrame(&frame);
}

//--------------------------------------------------
// Mesh : réception et routage
//--------------------------------------------------
void onMeshReceive(MeshFrame_t *frame){
    if(frame->to == NODE_ID){
        if(frame->msg_type == MSG_ACK){
            printf("ACK reçu de %d\n", frame->from);
            return;
        }

        // Afficher le message reçu
        printf("Message reçu (type %d, hop %d/%d) de %d: ", frame->msg_type, frame->hop, frame->hoplim, frame->from);
        // Affichage de la tram reçue en hexa
        printHex((uint8_t*)frame, 6 + 4 + frame->length);
        for(uint32_t i=0;i<frame->length;i++) putchar(frame->payload[i]);
        printf("\n");

        // Répondre par ACK
        sendAck(frame->from, frame->hop);
        return;
    }

    // Relais si hop < hoplim et si node n’est pas RECEIVER pur
    if(frame->hop < frame->hoplim && nodeRole != ROLE_RECEIVER){
        frame->hop++;
        sendMeshFrame(frame);
    }
}

//--------------------------------------------------
// Shell : changer le rôle
//--------------------------------------------------
int role_cmd(int argc, char **argv){
    if(argc<2){
        puts("usage: ROLE <0=SENDER|1=ROUTER|2=RECEIVER>");
        return -1;
    }
    int r = atoi(argv[1]);
    if(r<0 || r>2) return -1;
    nodeRole = (NodeRole)r;
    printf("Node role set to %d\n", nodeRole);
    return 0;
}

//--------------------------------------------------
// Shell : envoyer un message
//--------------------------------------------------
int sendmsg_cmd(int argc, char **argv){
    if(argc<3){
        puts("usage: SENDMSG <to> <text>");
        return -1;
    }
    uint8_t to = atoi(argv[1]);
    sendTextMessage(to, argv[2]);
    return 0;
}

//--------------------------------------------------
// Configuration Meshtastic
//--------------------------------------------------
static void _set_opt(netdev_t *netdev, netopt_t opt, bool val, char *str_help){
    netopt_enable_t en = val ? NETOPT_ENABLE : NETOPT_DISABLE;
    netdev->driver->set(netdev, opt, &en, sizeof(en));
    printf("%s %s\n", str_help, val ? "enabled" : "disabled");
}

int config_mesh_cmd(int argc, char **argv){
    (void)argc; (void)argv;
    netdev_t *netdev = &sx127x.netdev;

    // Channel
    int chan = 869462500;
    netdev->driver->set(netdev, NETOPT_CHANNEL_FREQUENCY, &chan, sizeof(chan));

    // CRC check
    _set_opt(netdev, NETOPT_INTEGRITY_CHECK, 1, "CRC check");

    // SyncWord = 0xAA
    uint8_t syncword = 0xAA;
    netdev->driver->set(netdev, NETOPT_SYNCWORD, &syncword, sizeof(syncword));

    // LoRa params
    uint8_t lora_bw = LORA_BW_125_KHZ;
    uint8_t lora_sf = 11;
    uint8_t lora_cr = 1; // coding rate 5 -> 5-4=1
    netdev->driver->set(netdev, NETOPT_BANDWIDTH, &lora_bw, sizeof(lora_bw));
    netdev->driver->set(netdev, NETOPT_SPREADING_FACTOR, &lora_sf, sizeof(lora_sf));
    netdev->driver->set(netdev, NETOPT_CODING_RATE, &lora_cr, sizeof(lora_cr));

    printf("Meshtastic config applied with SyncWord=0x%02X\n", syncword);
    return 0;
}

//--------------------------------------------------
// Event callback
//--------------------------------------------------
static void _event_cb(netdev_t *dev, netdev_event_t event){
    size_t len;
    netdev_lora_rx_info_t packet_info;
    static uint8_t rx_buf[512];

    switch(event){
        case NETDEV_EVENT_RX_COMPLETE:
            len = dev->driver->recv(dev, rx_buf, sizeof(rx_buf), &packet_info);
            if(len >= sizeof(MeshFrame_t)) onMeshReceive((MeshFrame_t*)rx_buf);
            break;
        default: break;
    }
}

//--------------------------------------------------
// Thread de réception
//--------------------------------------------------
void *_recv_thread(void *arg){
    (void)arg;
    static msg_t _msg_q[SX127X_LORA_MSG_QUEUE];
    msg_init_queue(_msg_q, SX127X_LORA_MSG_QUEUE);

    while(1){
        msg_t msg;
        msg_receive(&msg);
        if(msg.type == MSG_TYPE_ISR){
            netdev_t *dev = msg.content.ptr;
            dev->driver->isr(dev);
        }
    }
    return NULL;
}

//--------------------------------------------------
// Init SX127x
//--------------------------------------------------
int init_sx1272_cmd(int argc, char **argv){
    (void)argc; (void)argv;
    sx127x.params = sx127x_params[0];
    netdev_t *netdev = &sx127x.netdev;
    netdev->driver = &sx127x_driver;

    sx127x_set_preamble_length(&sx127x, 16);
    netdev->event_callback = _event_cb;

    if(netdev->driver->init(netdev)<0){
        puts("Failed to initialize SX127x");
        return 1;
    }

    _recv_pid = thread_create(stack, sizeof(stack), THREAD_PRIORITY_MAIN-1,
                              THREAD_CREATE_STACKTEST, _recv_thread, NULL, "recv_thread");

    if(_recv_pid <= KERNEL_PID_UNDEF){
        puts("Receiver thread creation failed");
        return 1;
    }

    puts("SX127x initialized");
    return 0;
}

//--------------------------------------------------
// Shell commands
//--------------------------------------------------
static const shell_command_t shell_commands[] = {
    {"init", "Initialize SX1272", init_sx1272_cmd},
    {"meshtastic", "Configure Meshtastic PHY", config_mesh_cmd},
    {"ROLE", "Set node role 0=SENDER,1=ROUTER,2=RECEIVER", role_cmd},
    {"SENDMSG", "Send text message <to> <text>", sendmsg_cmd},
    {NULL,NULL,NULL}
};

//--------------------------------------------------
// main
//--------------------------------------------------
int main(void){
    init_sx1272_cmd(0,NULL);

    puts("Initialization complete - starting shell");
    char line_buf[SHELL_DEFAULT_BUFSIZE];
    shell_run(shell_commands, line_buf, SHELL_DEFAULT_BUFSIZE);

    return 0;
}


/****************************************************************
*             _____  _____  _   _______   ___
*            |  __ \| ____|| | |__   __| / _ \   TM
*            | |  \ | |___ | |    | |   / / \ \
*            | |  | | ____|| |    | |  | |___| |
*            | |_/  | |___ | |___ | |  |  ___  |
*            |_____/|_____||_____||_|  |_|   |_|
*
*
* COPYRIGHT:
* Copyright (c) DELTA THINGS Pvt Ltd as an unpublished work
* THIS SOFTWARE AND/OR MATERIAL IS THE PROPERTY OF
* DELTA THINGS Pvt Ltd.  ALL USE, DISCLOSURE, AND/OR
* REPRODUCTION NOT SPECIFICALLY AUTHORIZED BY
* DELTA THINGS Pvt Ltd IS PROHIBITED.
****************************************************************/

#include <stdbool.h>
#ifdef __cplusplus
extern "C" {
#endif

#define STM32_ACK	0x79
#define STM32_NACK	0x1F
#define STM32_BUSY	0x76

#define STM32_CMD_INIT	0x7F
#define STM32_CMD_GET	0x00	/* get the version and command supported */
#define STM32_CMD_GVR	0x01	/* get version and read protection status */
#define STM32_CMD_GID	0x02	/* get ID */
#define STM32_CMD_RM	0x11	/* read memory */
#define STM32_CMD_GO	0x21	/* go */
#define STM32_CMD_WM	0x31	/* write memory */
#define STM32_CMD_WM_NS	0x32	/* no-stretch write memory */
#define STM32_CMD_ER	0x43	/* erase */
#define STM32_CMD_EE	0x44	/* extended erase */
#define STM32_CMD_EE_NS	0x45	/* extended erase no-stretch */
#define STM32_CMD_WP	0x63	/* write protect */
#define STM32_CMD_WP_NS	0x64	/* write protect no-stretch */
#define STM32_CMD_UW	0x73	/* write unprotect */
#define STM32_CMD_UW_NS	0x74	/* write unprotect no-stretch */
#define STM32_CMD_RP	0x82	/* readout protect */
#define STM32_CMD_RP_NS	0x83	/* readout protect no-stretch */
#define STM32_CMD_UR	0x92	/* readout unprotect */
#define STM32_CMD_UR_NS	0x93	/* readout unprotect no-stretch */
#define STM32_CMD_CRC	0xA1	/* compute CRC */
#define STM32_CMD_ERR	0xFF	/* not a valid command */

#define STM32_RESYNC_TIMEOUT	35	/* seconds */
#define STM32_MASSERASE_TIMEOUT	35	/* seconds */
#define STM32_SECTERASE_TIMEOUT	5	/* seconds */
#define STM32_BLKWRITE_TIMEOUT	1	/* seconds */
#define STM32_WUNPROT_TIMEOUT	1	/* seconds */
#define STM32_WPROT_TIMEOUT	1	/* seconds */
#define STM32_RPROT_TIMEOUT	1	/* seconds */

#define STM32_CMD_GET_LENGTH	17	/* bytes in the reply */

#define STM32STADDR  	0x8000000     // STM32 codes start address, you can change to other address if use custom bootloader like 0x8002000

#define UART_NO 1
#define STM32_PAGE_SIZE 256
#define STM32_REPROGRAMMING_MAX_STATES 4

struct STM32_CHIP{
    uint16_t chip_id;
    const char *chipName;
};

typedef enum {
	STM32_ERR_OK = 0,
	STM32_ERR_UNKNOWN,	/* Generic error */
    STM32_ERR_WAIT,
	STM32_ERR_NACK,
	STM32_ERR_NO_CMD,	/* Command not available in bootloader */
} stm32_err_t;

typedef enum {
  STM32_ERASE_ER = 0,      //Noraml erase command
  STM32_ERASE_ER_PAGE_CMD, //Noramal Erase mass erase command
  STM32_ERASE_EE,          //Extended erase command
  STM32_ERASE_EE_PAGE_CMD  //Extended Erase mass erase command
} stm32_erase_cmd_t;

typedef enum {
  STM32_REPROG_WM_CMD = 0,
  STM32_REPROG_WM_REPEAT_CMD,
  STM32_REPROG_ADD_CMD,
  STM32_REPROG_DATA_CMD
} stm32_reprogram_cmd_t;

/******************** Global Function Prototypes ************/
enum mgos_host_ota_state stm32BeginReprogramming (struct host_ota_update_context *ctx);
void stm32GetReprogrammingCtx(struct host_ota_update_context *ctx);
#ifdef __cplusplus
}
#endif /* __cplusplus */
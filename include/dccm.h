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

struct host_ota_update_context;
typedef void (*mgos_host_ota_updater_result_cb)(struct host_ota_update_context *ctx);

#define MGOS_EVENT_DCCM_BASE MGOS_EVENT_BASE('D', 'C', 'M')
enum mgos_event_ota {
  MGOS_EVENT_DCCM_BEGIN =
      MGOS_EVENT_DCCM_BASE, /* ev_data: struct mgos_upd_info */
  MGOS_EVENT_DCCM_STATUS,   /* ev_data: struct mgos_ota_status */
  MGOS_EVENT_DCCM_REQUEST,  /* ev_data: struct ota_request_param */
};

enum mgos_host_ota_state {
  MGOS_HOST_OTA_STATE_IDLE = 0, /* idle */
  MGOS_HOST_OTA_STATE_PROGRESS, /* "progress" */
  MGOS_HOST_OTA_STATE_ERROR,    /* "error" */
  MGOS_HOST_OTA_STATE_SUCCESS,  /* "success" */
};

struct host_mcu_file_info {
  char name[50];
  uint32_t size;  
  uint16_t bytes_processed;
};

struct host_ota_update_context {
 
  uint8_t hostOTAstep ;
  uint8_t hostInternalCmdStep ;
  uint8_t  *data;
  int8_t result;

  enum mgos_host_ota_state host_ota_state; 
  const char *status_msg;
  bool waitingForPrevCmdToComplete;  
  bool ignore_same_version;
  bool need_reboot;
  bool hostMCUFlashCmdretry;

  char host_mcu_id[35];
  size_t data_len; 
  struct mbuf *uart_rx_buff;
  struct host_mcu_file_info current_file;
  struct mg_rpc_request_info *s_host_ota_update_req_frame ;

  FILE *fp;
  
  enum mgos_host_ota_state(*host_mcu_begin_reprogramming)(struct host_ota_update_context *); 
  void (*host_mcu_reset)();

  mgos_timer_id wdt;
  mgos_host_ota_updater_result_cb result_cb;

};

struct state {
  struct mg_rpc_request_info *ri; /* RPC request info */
  int uart_no;                    /* UART number to write to */
  int status;                     /* Request status */
  int64_t written;                /* Number of bytes written */
  FILE *fp;                       /* File to write to */
};

struct host_ota_update_context *host_ota_updater_context_create(int timeout , const struct mg_str MCU_Type ,const char *ota_file);
const char *mgos_host_ota_state_str(enum mgos_host_ota_state state);
void DCCMStateMachine(struct mbuf *rx_data);
#ifdef __cplusplus
}
#endif /* __cplusplus */
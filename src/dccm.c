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

#include "mgos.h"
#include "mgos_rpc.h"
#include "mgos_gpio.h"
#include "dccm.h"
#include "stm32ota.h"
#include "mgos_mqtt.h"

#define LED_ON   (mgos_sys_config_get_board_led1_active_high())
#define LED_OFF  (!mgos_sys_config_get_board_led1_active_high())
#define ATTRIBUTE_TOPIC "v1/devices/me/attributes"

static struct mbuf lb = {0};
struct host_ota_update_context *s_host_ota_ctx = NULL;
/**********************************************************************!
 * @fn          resetHostOTA
 * @brief        
 * @param[in]   void
 * @return      bool
 **********************************************************************/
void resetHostOTA (void)
{
  if (s_host_ota_ctx != NULL)
  {
    if (s_host_ota_ctx->s_host_ota_update_req_frame != NULL)
    {
      if (s_host_ota_ctx->result_cb != NULL)
      {
        s_host_ota_ctx->result_cb(s_host_ota_ctx);
        s_host_ota_ctx->result_cb = NULL;
      }
      else
      {
        s_host_ota_ctx->s_host_ota_update_req_frame = NULL;
      }
    }
    if (s_host_ota_ctx->wdt != MGOS_INVALID_TIMER_ID)
    {
      mgos_clear_timer(s_host_ota_ctx->wdt);
      s_host_ota_ctx->wdt = MGOS_INVALID_TIMER_ID;
    }
    if (s_host_ota_ctx->data != NULL)
    {
      free(s_host_ota_ctx->data);
    }
    if (s_host_ota_ctx->fp != NULL)
    {
      fclose(s_host_ota_ctx->fp);
    }
    free(s_host_ota_ctx);
    s_host_ota_ctx = NULL;
  }
}
  /**********************************************************************!
 * @fn          mgos_host_ota_updater_result_cb
 * @brief       Initialization function 
 * @param[in]   void
 * @return      bool
 **********************************************************************/
  static void host_ota_updater_result_cb(struct host_ota_update_context * ctx)
  {
    LOG(LL_INFO, ("DCCM  : host_ota_updater_result_cb "));
    if (ctx->s_host_ota_update_req_frame == NULL)
      return;
    mg_rpc_send_errorf(ctx->s_host_ota_update_req_frame, (ctx->result > 0 ? 0 : -1), ctx->status_msg);
    ctx->s_host_ota_update_req_frame = NULL;
  }
  /**********************************************************************!
 * @fn          host_ota_updater_abort
 * @brief       Initialization function 
 * @param[in]   void
 * @return      bool
 **********************************************************************/
  static void host_ota_updater_abort(void *arg)
  {
    struct host_ota_update_context *ctx = (struct host_ota_update_context *)arg;
    if (s_host_ota_ctx != ctx)
      return;
    LOG(LL_ERROR, ("Host MCU OTA abort : timeout"));
    resetHostOTA();
  }
  /**********************************************************************!
 * @fn          host_ota_update_context
 * @brief       Initialization function 
 * @param[in]   void
 * @return      bool
 **********************************************************************/
  struct host_ota_update_context *host_ota_updater_context_create(int timeout,
                                                                  const struct mg_str MCU_Type,
                                                                  const char *ota_file)
  {
    FILE *fp = 0;
    /* try to open file */
    fp = fopen(ota_file, "rb");

    if (fp == NULL)
    {
      LOG(LL_ERROR, ("Host MCU bin %s file not found", ota_file));
      return NULL;
    }

    /* determine file size */
    cs_stat_t st;
    if (mg_stat(ota_file, &st) != 0)
    {
      LOG(LL_ERROR, ("%s file size fail", ota_file));
      return NULL;
    }

    LOG(LL_INFO, ("host mcu bin file size :%ld", st.st_size));

    if (s_host_ota_ctx != NULL)
    {
      LOG(LL_ERROR, ("Host MCU falshing already in progress"));
      return NULL;
    }

    s_host_ota_ctx = calloc(1, sizeof(*s_host_ota_ctx));
    if (s_host_ota_ctx == NULL)
    {
      LOG(LL_ERROR, ("s_host_ota_ctx Out of memory"));
      return NULL;
    }

    s_host_ota_ctx->host_mcu_begin_reprogramming = NULL;   
    s_host_ota_ctx->need_reboot = false;
    s_host_ota_ctx->ignore_same_version = false;
    s_host_ota_ctx->host_mcu_reset = NULL;
    s_host_ota_ctx->hostOTAstep = 0;
    s_host_ota_ctx->waitingForPrevCmdToComplete = false;
    s_host_ota_ctx->status_msg="Host MCU reprograming started";

    if (mg_strcmp(MCU_Type, mg_mk_str("STM32")) == 0)
    {
      stm32GetReprogrammingCtx(s_host_ota_ctx);
    }
    else
    {
      LOG(LL_ERROR, ("Wrong host MCU chip type"));
      free(s_host_ota_ctx);
      s_host_ota_ctx = NULL;
      return NULL;
    }
    struct mg_str filename = mg_mk_str(ota_file);
    strcpy(s_host_ota_ctx->host_mcu_id, "unknown");

    memcpy(s_host_ota_ctx->current_file.name, filename.p, filename.len);
    s_host_ota_ctx->current_file.size = (long)st.st_size;
    s_host_ota_ctx->current_file.bytes_processed = 0;
    s_host_ota_ctx->fp = fp;
    s_host_ota_ctx->result_cb = host_ota_updater_result_cb;
    s_host_ota_ctx->s_host_ota_update_req_frame = NULL;

    if (timeout <= 0)
    {
      timeout = mgos_sys_config_get_update_timeout();
    }
    s_host_ota_ctx->wdt = mgos_set_timer(timeout * 1000, 0, host_ota_updater_abort, s_host_ota_ctx);
    LOG(LL_INFO, ("starting, host MCU OTA timeout %d", timeout));
    s_host_ota_ctx->host_ota_state = MGOS_HOST_OTA_STATE_PROGRESS;

    return s_host_ota_ctx;
  }
  
  /**********************************************************************!
 * @fn          uart_dispatcher
 * @brief       UART callback for RX and TX messages
 * @param[in]   void
 * @return      bool
 **********************************************************************/
  static void uart_dispatcher(int uart_no, void *arg)
  {
    enum mgos_host_ota_state ret = MGOS_HOST_OTA_STATE_IDLE;
    //assert(uart_no == UART_NO);

    size_t rx_av = mgos_uart_read_avail(UART_NO);
    if (rx_av == 0)
      return;
    mgos_uart_read_mbuf(UART_NO, &lb, rx_av);
    /* Handle all the wonderful possibilities of different line endings. */    
    for (uint8_t i = 0; i < (lb.len); i++)
    {
      LOG(LL_INFO, ("data rcvd :%02X ", lb.buf[i]));
    }

    if(s_host_ota_ctx != NULL)
    {
      s_host_ota_ctx->uart_rx_buff = &lb;
      ret = s_host_ota_ctx->host_mcu_begin_reprogramming(s_host_ota_ctx);
      if (ret == MGOS_HOST_OTA_STATE_SUCCESS ||
          ret == MGOS_HOST_OTA_STATE_ERROR)
      {
        resetHostOTA();
      }
    }
    else
    {
      LOG(LL_INFO, ("Host MCU OTA OFF"));
      mbuf_remove(&lb, lb.len);
    }
    //mbuf_clear(&lb);
    (void)arg;
  }
  /**********************************************************************!
 * @fn          http_cb
 * @brief       callback function for http file download 
 * @param[in]   void
 * @return      bool
 **********************************************************************/
  static void http_cb(struct mg_connection * c, int ev, void *ev_data, void *ud)
  {
    struct http_message *hm = (struct http_message *)ev_data;
    struct state *state = (struct state *)ud;

    switch (ev)
    {
    case MG_EV_CONNECT:
      state->status = *(int *)ev_data;
      break;
    case MG_EV_HTTP_CHUNK:
    {
      /*
       * Write data to file or UART. mgos_uart_write() blocks until
       * all data is written.
       */
      size_t n =
          (state->fp != NULL)
              ? fwrite(hm->body.p, 1, hm->body.len, state->fp)
              : mgos_uart_write(state->uart_no, hm->body.p, hm->body.len);
      if (n != hm->body.len)
      {
        c->flags |= MG_F_CLOSE_IMMEDIATELY;
        state->status = 500;
      }
      state->written += n;
      c->flags |= MG_F_DELETE_CHUNK;
      break;
    }
    case MG_EV_HTTP_REPLY:
      /* Only when we successfully got full reply, set the status. */
      state->status = hm->resp_code;
      LOG(LL_INFO, ("Finished fetching"));
      c->flags |= MG_F_CLOSE_IMMEDIATELY;
      break;
    case MG_EV_CLOSE:
      LOG(LL_INFO, ("status %d bytes %llu", state->status, state->written));
      if (state->status == 200)
      {
        /* Report success only for HTTP 200 downloads */
        mg_rpc_send_responsef(state->ri, "{written: %llu}", state->written);
      }
      else
      {
        mg_rpc_send_errorf(state->ri, state->status, NULL);
      }
      if (state->fp != NULL)
        fclose(state->fp);
      free(state);
      break;
    }
  }
  /**********************************************************************!
 * @fn          fetch_handler
 * @brief       RPC handler for CCM.fetch 
 * @param[in]   void
 * @return      bool
 **********************************************************************/
  static void fetch_handler(struct mg_rpc_request_info * ri, void *cb_arg,
                            struct mg_rpc_frame_info *fi, struct mg_str args)
  {
    struct state *state;
    int uart_no = -1;
    FILE *fp = NULL;
    char *url = NULL, *path = NULL;

    json_scanf(args.p, args.len, ri->args_fmt, &url, &uart_no, &path);

    if (url == NULL || (uart_no < 0 && path == NULL))
    {
      mg_rpc_send_errorf(ri, 500, "expecting url, uart or file");
      goto done;
    }

    if (path != NULL && (fp = fopen(path, "w")) == NULL)
    {
      mg_rpc_send_errorf(ri, 500, "cannot open %s", path);
      goto done;
    }

    if ((state = calloc(1, sizeof(*state))) == NULL)
    {
      mg_rpc_send_errorf(ri, 500, "OOM");
      goto done;
    }

    state->uart_no = uart_no;
    state->fp = fp;
    state->ri = ri;

    LOG(LL_INFO, ("Fetching %s to %d/%s", url, uart_no, path ? path : ""));
    if (!mg_connect_http(mgos_get_mgr(), http_cb, state, url, NULL, NULL))
    {
      free(state);
      mg_rpc_send_errorf(ri, 500, "malformed URL");
      goto done;
    }

    (void)cb_arg;
    (void)fi;

  done:
    free(url);
    free(path);
  }
  /**********************************************************************!
 * @fn          ccm_OTA_handler
 * @brief       RPC handler for Flash loder commands 
 * @param[in]   void
 * @return      bool
 **********************************************************************/
  static void DCCM_OTA_cmd_handler(struct mg_rpc_request_info * ri, void *cb_arg,
                                   struct mg_rpc_frame_info *fi, struct mg_str args)
  {

    char *chip = NULL, *cmd = NULL, *path = NULL;
    struct host_ota_update_context *ctx = NULL;
    struct mg_str host_mcu_type = mg_mk_str(mgos_sys_config_get_dccm_host_ota_chip_type());

    json_scanf(args.p, args.len, ri->args_fmt, &chip, &cmd, &path);

    if (chip == NULL || cmd == NULL || path == NULL)
    {
      mg_rpc_send_errorf(ri, 500, "expecting host mcu chip type, command type ,path");
      goto done;
    }

    if (mg_strcmp(mg_mk_str(cmd), mg_mk_str("Flash")) == 0)
    {
      LOG(LL_INFO, ("Chip Type %s setting mode to %s", chip, cmd));
      if (s_host_ota_ctx == NULL)
      {
        //Set time out to 120 sec
        ctx = host_ota_updater_context_create(120, host_mcu_type, path);
        if (ctx == NULL)
        {
          mg_rpc_send_errorf(ri, -1, "Failed to init host ota flash updater");
        }
        else
        {
          ctx->s_host_ota_update_req_frame = ri;
          if(s_host_ota_ctx->host_mcu_begin_reprogramming != NULL)
          {
             (void)s_host_ota_ctx->host_mcu_begin_reprogramming(ctx);
          }
          else
          {
            resetHostOTA();
          }
        }
      }
      else
      {
        /* code */
        mg_rpc_send_errorf(ri, 500, "Previous host MCU flasing is active", false);
      }
    }
    else if (mg_strcmp(mg_mk_str(cmd), mg_mk_str("Reset")) == 0)
    {
      LOG(LL_INFO, ("Chip Type %s setting mode to %s", chip, cmd));
      if (s_host_ota_ctx == NULL)
      {
        if (mg_strcmp(host_mcu_type, mg_mk_str("STM32")) == 0)
        {
          ctx = host_ota_updater_context_create(120, host_mcu_type, path);
          if (ctx == NULL)
          {
            mg_rpc_send_errorf(ri, -1, "Failed to reset host MCU");
          }
          else
          {            
            if(s_host_ota_ctx->host_mcu_reset != NULL)
            {
              s_host_ota_ctx->host_mcu_reset();          
              mg_rpc_send_errorf(ri, 200, "Host MCU reset successful");
            }
            else
            {
              mg_rpc_send_errorf(ri, -1, "Host MCU reset handler not found ");
            }
            resetHostOTA();
          }
        }
        else
        {
          LOG(LL_INFO, ("Chip Type %s not supported", chip));
        }
      }
      else
      {
        /* code */
        mg_rpc_send_errorf(ri, 500, "Host MCU  reset fail !! Host MCU flashing is active");
      }
    }
    else
    {
      LOG(LL_INFO, ("Invalid DCCM OTA Command"));
      goto done;
    }

    (void)cb_arg;
    (void)fi;

  done:
    free(chip);
    free(cmd);
    free(path);
  }

  /********************************************************************!
 * @fn          host_ota_cb
 * @brief       Callback function for host MCU ota update
 * @param[in]   event , event data ,additional arguments
 * https://mongoose-os.com/docs/mongoose-os/api/core/mgos_event.h.md
 * @return      *args
 ********************************************************************/
static void host_ota_cb(int ev, void *evd, void *arg) {
  struct host_ota_update_context *host_ota_status = (struct host_ota_update_context *) evd;  
  int otaled_pin = mgos_sys_config_get_board_led1_pin();
  bool res=false;
  uint8_t progress = 0;

  struct mbuf buf;
  mbuf_init(&buf, 0);
  struct json_out out = JSON_OUT_MBUF(&buf);

  LOG(LL_INFO, ("OTA Event (%d)", ev));
  switch (ev) {    
    case MGOS_EVENT_DCCM_STATUS: {
      if(host_ota_status->current_file.size != 0)
      {
         progress = (((uint32_t) host_ota_status->current_file.bytes_processed * 100)/host_ota_status->current_file.size) ;
      }
      else
      {
        progress = 0;
      }

      LOG(LL_INFO, ("OTA State(%s) OTA Progress(%d)",host_ota_status->status_msg,
                                                     progress));
      json_printf(&out,"{host_mcu: %Q, host_ota_prog: %u, host_ota_msg: %Q}",
                            host_ota_status->host_mcu_id,
                            progress,
                            host_ota_status->status_msg
                            );

      res = mgos_mqtt_pub(ATTRIBUTE_TOPIC, buf.buf, buf.len, 0 /* qos */, false /* retain */);

      LOG(LL_INFO,("OTA msg published: %s",res ? "yes" : "no"));                            
      switch(host_ota_status->host_ota_state)
      {
        case MGOS_HOST_OTA_STATE_PROGRESS:{              
          mgos_gpio_blink(otaled_pin, 500, 500);          
        }
        break;
        case MGOS_HOST_OTA_STATE_IDLE :{
          mgos_gpio_blink(otaled_pin, 0, 0);
          mgos_gpio_write(otaled_pin, LED_OFF);
        }
        break;
        case MGOS_HOST_OTA_STATE_ERROR :{
          mgos_gpio_blink(otaled_pin, 200, 200);      
        }
        break;
        case MGOS_HOST_OTA_STATE_SUCCESS :{
          mgos_gpio_blink(otaled_pin, 0, 0); 
          mgos_gpio_write(otaled_pin, LED_ON);     
        }
        break;
      } 
      break;
    }   
  }
  mbuf_free(&buf); 
  (void) arg;
}

  /**********************************************************************!
 * @fn          mgos_dccm_init
 * @brief       Initialization function 
 * @param[in]   void
 * @return      bool
 **********************************************************************/
  bool mgos_dccm_init(void)
  {

    if (mgos_sys_config_get_dccm_host_ota_enable())
    {
      mgos_gpio_setup_output(mgos_sys_config_get_dccm_host_ota_rst_pin(), true);
   	 LOG(LL_INFO,("Init : reset pin high"));
      mgos_gpio_setup_output(mgos_sys_config_get_dccm_host_ota_boot0_pin(), false);
	 LOG(LL_INFO,("Init : boot pin low"));
      //Check if interface type is UART
      if (mg_strcmp(mg_mk_str(mgos_sys_config_get_dccm_host_ota_intf_type()),
                    mg_mk_str("UART")) == 0)
      {
        struct mgos_uart_config ucfg;
        mgos_uart_config_set_defaults(UART_NO, &ucfg);
        /*
      * At this point it is possible to adjust baud rate, pins and other settings.
      * 115200 8-N-1 is the default mode, now update config as per chip type
      */
        ucfg.baud_rate = mgos_sys_config_get_dccm_host_ota_uart_baud_rate();
        //Check the host chip type
        if (mg_strcmp(mg_mk_str(mgos_sys_config_get_dccm_host_ota_chip_type()),
                      mg_mk_str("STM32")) == 0)
        {
          ucfg.num_data_bits = 8;
          ucfg.parity = MGOS_UART_PARITY_EVEN;
          ucfg.stop_bits = MGOS_UART_STOP_BITS_1;
          ucfg.dev.rx_gpio = mgos_sys_config_get_dccm_host_ota_uart_rx_pin();
          ucfg.dev.tx_gpio = mgos_sys_config_get_dccm_host_ota_uart_tx_pin();
        }

        if (!mgos_uart_configure(UART_NO, &ucfg))
        {
          LOG(LL_ERROR, ("UART%d init failed", UART_NO));
          return MGOS_APP_INIT_ERROR;
        }
        else
        {
          LOG(LL_INFO, ("UART%d init Success", UART_NO));
        }

        //Add the dispatcher
        mgos_uart_set_dispatcher(UART_NO, uart_dispatcher, NULL /* arg */);
        //Enable the UART interface
        mgos_uart_set_rx_enabled(UART_NO, true);
      }
      mg_rpc_add_handler(mgos_rpc_get_global(), "CCM.Fetch",
                         "{url: %Q, uart: %d, file: %Q}", fetch_handler, NULL);

      mg_rpc_add_handler(mgos_rpc_get_global(), "CCM.Cmd",
                         "{chip_type: %Q,cmd: %Q, file: %Q}", DCCM_OTA_cmd_handler, NULL);

      /*Host OTA update events */
      mgos_event_add_group_handler(MGOS_EVENT_DCCM_BASE, host_ota_cb, NULL);
                    
    }
    return true;
  }

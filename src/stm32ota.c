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

#include <stdio.h>
#include "mgos.h"
#include "dccm.h"
#include "stm32ota.h"


const struct STM32_CHIP stm32_chip[] ={
    {0,"Unknown Chip"},
    {0x440,"STM32F030x8/05x"},
    {0x444,"STM32F03xx4/6"},
    {0x442,"STM32F030xC"},
    {0x445,"STM32F04xxx/070x6"},
    {0x448,"STM32F070xB/071xx/072xx"},    
    {0x442,"STM32F09xxx"},
    {0x412,"STM32F10xxx-LD"},
    {0x410,"STM32F10xxx-MD"},
    {0x414,"STM32F10xxx-HD"},
    {0x420,"STM32F10xxx-MD-VL"},
    {0x428,"STM32F10xxx-HD-VL"},
    {0x418,"STM32F105/107"},
    {0x430,"STM32F10xxx-XL-D"},
    {0x411,"STM32F20xxxx"},
    {0x432,"STM32F373xx/378xx"},    
    {0x422,"STM32F302xB(C)/303xB(C)/358xx"},    
    {0x439,"STM32F301xx/302x4(6/8)/318xx"},    
    {0x438,"STM32F303x4(6/8)/334xx/328xx"},
    {0x446,"STM32F302xD(E)/303xD(E)/398xx"},   
    {0x413,"STM32F40xxx/41xxx"},
    {0x419,"STM32F42xxx/43xxx"},
    {0x423,"STM32F401xB(C)"},
    {0x433,"STM32F401xD(E)"},
    {0x458,"STM32F410xx"},
    {0x431,"STM32F411xx"},
    {0x441,"STM32F412xx"},
    {0x421,"STM32F446xx"},
    {0x434,"STM32F469xx/479xx"},
    {0x463,"STM32F413xx/423xx"},
    {0x452,"STM32F72xxx/73xxx"},
    {0x449,"STM32F74xxx/75xxx"},
    {0x451,"STM32F76xxx/77xxx"},
    {0x450,"STM32H74xxx/75xxx"},
    {0x457,"STM32L01xxx/02xxx"},
    {0x425,"STM32L031xx/041xx"},
    {0x417,"STM32L05xxx/06xxx"},
    {0x447,"STM32L07xxx/08xxx"},
    {0x416,"STM32L1xxx6(8/B)"},
    {0x429,"STM32L1xxx6(8/B)A"},
    {0x427,"STM32L1xxxC"},
    {0x436,"STM32L1xxxD"},
    {0x437,"STM32L1xxxE"},
    {0x435,"STM32L43xxx/44xxx"},
    {0x462,"STM32L45xxx/46xxx"},
    {0x415,"STM32L47xxx/48xxx"},
    {0x461,"STM32L496xx/4A6xx"}
};

/***************** Function Prototypes **********************************/
static void stm32RunMode(struct host_ota_update_context *ctx);
static stm32_err_t stm32FlashMode(struct host_ota_update_context *ctx);
static stm32_err_t stm32Erase(struct host_ota_update_context *ctx);
static stm32_err_t stm32GetId(struct host_ota_update_context *ctx);
static stm32_err_t stm32programflash(struct host_ota_update_context *ctx);
static stm32_err_t stm32_send_init_seq(void);
static uint8_t stm32SendData(unsigned char * data, uint16_t wrlen);
static bool stm32Address(unsigned long addr);
static bool stm32SendCommand(uint8_t commd);
static bool readMcuOtaBinData(struct host_ota_update_context *ctx);
static uint8_t stm32CheckResponse(struct mbuf *rx_data,struct mbuf *dataBuff);
/**********************************************************************!
 * @fn          stm32Address
 * @brief       UART callback for RX and TX messages
 * @param[in]   void
 * @return      bool
 **********************************************************************/
static bool stm32Address(unsigned long addr)
{ // Tested
  uint8_t sendaddr[5];
 
  /* must be 32bit aligned */
	if (addr & 0x3) {
		LOG(LL_ERROR, ("Error: WRITE address must be 4 byte aligned\n"));
		return false;
	}

  sendaddr[0] = addr >> 24;
  sendaddr[1] = (addr >> 16) & 0xFF;
  sendaddr[2] = (addr >> 8) & 0xFF;
  sendaddr[3] = addr & 0xFF;  
  sendaddr[4] = sendaddr[0] ^ sendaddr[1] ^ sendaddr[2] ^ sendaddr[3];
  
  mgos_uart_write(UART_NO, sendaddr, sizeof(sendaddr));
  LOG(LL_INFO, ("STM32 ADD:[ %02X %02X %02X %02X ] CS:%02X", sendaddr[0],sendaddr[1],
                                                             sendaddr[2],sendaddr[3],
                                                             sendaddr[4]));
  return true;
}
/**********************************************************************!
 * @fn          stm32SendData
 * @brief       UART callback for RX and TX messages
 * @param[in]   void
 * @return      bool
 **********************************************************************/
static uint8_t stm32SendData(uint8_t *data, uint16_t len)
{ 
  uint8_t cs, buf[256 + 2];
	uint16_t i, aligned_len;	

  aligned_len = (len + 3) & ~3;
	cs = aligned_len - 1;
	buf[0] = aligned_len - 1;
	for (i = 0; i < len; i++) {
		cs ^= data[i];
		buf[i + 1] = data[i];
	}
	/* padding data */
	for (i = len; i < aligned_len; i++) {
		cs ^= 0xFF;
		buf[i + 1] = 0xFF;
	}
	buf[aligned_len + 1] = cs;  
  mgos_uart_write(UART_NO, &buf, aligned_len + 2); 

  return true;
}

/**********************************************************************!
 * @fn          stm32CheckResponse
 * @brief       
 * @param[in]   void
 * @return      bool
 **********************************************************************/
static uint8_t stm32CheckResponse(struct mbuf *rx_data,struct mbuf *dataBuff)
{
  uint8_t ret = false;
  for (uint16_t i = 0; i < (rx_data->len); i++)
  {
    //LOG(LL_INFO, ("data rcvd :%02X ", rx_data->buf[i]));
    if (rx_data->buf[i] == STM32_ACK)
    {
      ret = STM32_ACK;
      // Check if we need to coppy any return data 
      if (dataBuff != NULL)
      {
        mbuf_move(rx_data, dataBuff);
      }
      break;
    }
    else if (rx_data->buf[i] == STM32_NACK)
    {
      ret = STM32_NACK;
      break;
    }
  }
  mbuf_remove(rx_data, rx_data->len);
  return ret;
}
/**********************************************************************!
 * @fn          stm32_send_init_seq
 * @brief       
 * @param[in]   void
 * @return      bool
 **********************************************************************/
static stm32_err_t stm32_send_init_seq()
{
	uint8_t cmd = STM32_CMD_INIT; 

  mgos_uart_write(UART_NO, &cmd, sizeof(cmd));  
  
  return STM32_ERR_UNKNOWN;
  
}

/**********************************************************************!
 * @fn          readMcuOtaBinData
 * @brief       UART callback for RX and TX messages
 * @param[in]   void
 * @return      bool
 **********************************************************************/
static bool readMcuOtaBinData(struct host_ota_update_context *ctx)
{  
  bool ret = true;
  FILE *fp = ctx->fp;
  
  if (fp == NULL)
  {
    fp = fopen(ctx->current_file.name, "rb");
  } 

  if (fp == NULL)
  {
    LOG(LL_ERROR, ("Cannot open host MCU binary file"));
    ret = false;
    goto clean;
  }
  else
  {
    {
      LOG(LL_INFO, ("FP : %lx",(unsigned long) fp));
    }
  }

  if(STM32_PAGE_SIZE <= 0)
  {
     LOG(LL_ERROR, ("STM32_PAGE_SIZE cannot be 0"));
     goto clean;
  }
  else if(STM32_PAGE_SIZE > 0 && ctx->data == NULL) 
  {
    /* try to allocate the chunk of needed size */
    ctx->data = (uint8_t *) malloc(STM32_PAGE_SIZE);
    LOG(LL_INFO, ("ctx->data : %lx",(unsigned long) ctx->data));
    ctx->data_len = STM32_PAGE_SIZE;
    if (ctx->data == NULL) { 
      ret = false;    
      goto clean;
    }
  }
 
  /* Check if we have reached < STM32_PAGE_SIZE bytes to send */   
  if((ctx->current_file.bytes_processed / STM32_PAGE_SIZE) < 
    ((ctx->current_file.size) / STM32_PAGE_SIZE))
  {
    ctx->data_len = STM32_PAGE_SIZE;    
  }
  else
  {
    ctx->data_len = (ctx->current_file.size) % STM32_PAGE_SIZE;;     
  }

  /* seek & read the data */
  if (fseek(fp, ctx->current_file.bytes_processed, SEEK_SET))
  {
    LOG(LL_ERROR, ("Host MCU bin %d cannot seek", ctx->current_file.bytes_processed));
    ret = false;
    goto clean;
  }

  if ((long)fread(ctx->data, 1, ctx->data_len, fp) != ctx->data_len)
  {
    LOG(LL_ERROR, ("Host MCU bin fread fail %d", ctx->data_len));
    ret = false;
    goto clean;
  }
  else
  {
    ctx->current_file.bytes_processed += ctx->data_len;
  }

  LOG(LL_INFO, ("bytes processed %u total Size :%u",ctx->current_file.bytes_processed,ctx->current_file.size));

clean:
  return ret;
}
/**********************************************************************!
 * @fn          stm32programflash
 * @brief       UART callback for RX and TX messages
 * @param[in]   void
 * @return      bool
 **********************************************************************/
static stm32_err_t stm32programflash(struct host_ota_update_context *ctx)
{  
  stm32_err_t s_err = STM32_ERR_WAIT;
  uint8_t ackResponse;

  //Flow : STM32_CMD_WM-->ACK-->STM32STADDR-->ACK--> SEND-DATA-->ACK-->STM32_CMD_WM (until toal binary file is sent)
  
  if(ctx->waitingForPrevCmdToComplete == false)
  {
    stm32SendCommand(STM32_CMD_WM);
    ctx->waitingForPrevCmdToComplete = true;
    ctx->hostInternalCmdStep = STM32_REPROG_WM_CMD;
  }
  else
  {
    ackResponse = stm32CheckResponse(ctx->uart_rx_buff, NULL);
    //if Write Memory command is successful , send Address command
    if (ackResponse == STM32_ACK && ctx->hostInternalCmdStep == STM32_REPROG_WM_CMD)
    {
      stm32Address(STM32STADDR + (((uint32_t)ctx->current_file.bytes_processed / STM32_PAGE_SIZE) * STM32_PAGE_SIZE));
      ctx->hostInternalCmdStep = STM32_REPROG_ADD_CMD;
    }
    //Address Sent successful , now send data
    else if (ackResponse == STM32_ACK && ctx->hostInternalCmdStep == STM32_REPROG_ADD_CMD)
    {
      ctx->hostInternalCmdStep = STM32_REPROG_DATA_CMD;
      //Check if we can read stored MCU binary data
      if (readMcuOtaBinData(ctx))
      {
        LOG(LL_INFO, ("ctx->data : %lx", (unsigned long)ctx->data));
        if (ctx->data_len == 0 || ctx->data_len > STM32_PAGE_SIZE)
        {
          s_err = STM32_ERR_NACK;
          ctx->hostInternalCmdStep = STM32_REPROG_WM_CMD;
          ctx->status_msg = "Host MCU binary File read failure";
          LOG(LL_INFO, ("Host MCU binary File read failure"));
        }
        else
        {
          (void)stm32SendData(ctx->data, ctx->data_len);
          LOG(LL_INFO, ("Host OTA chunk data sent successful"));
          ctx->hostInternalCmdStep = STM32_REPROG_WM_REPEAT_CMD;
        }
      }
    }
    //if previous data sent is successful , resend Write command to repeat the process
    else if (ackResponse == STM32_ACK && ctx->hostInternalCmdStep == STM32_REPROG_WM_REPEAT_CMD)
    {
      LOG(LL_INFO, ("Total bytes sent to MCU %u %u ", ctx->current_file.bytes_processed, ctx->current_file.size));
      //check if more data to send ?
      if (ctx->current_file.bytes_processed < ctx->current_file.size)
      {
        stm32SendCommand(STM32_CMD_WM);
        ctx->hostInternalCmdStep = STM32_REPROG_WM_CMD;
      }
      //All data is sent
      else
      {
        ctx->waitingForPrevCmdToComplete = false;
        ctx->hostInternalCmdStep = STM32_REPROG_WM_CMD;
        s_err = STM32_ERR_OK;
        //check if need reboot MCU after reprogramming
        if (ctx->need_reboot == true)
        {
          stm32RunMode(ctx);
        }
      }
    }
    else
    {
      s_err = STM32_ERR_NACK;
      ctx->hostInternalCmdStep = STM32_REPROG_WM_CMD;
      ctx->status_msg = "Host MCU falsh program failure ";
    }  
  }

  LOG(LL_INFO, ("stm32Erase Status : 0x%02x ", s_err));
  return (s_err);

}

/**********************************************************************!
 * @fn          stm32SendCommand
 * @brief       UART callback for RX and TX messages
 * @param[in]   void
 * @return      bool
 **********************************************************************/
static bool stm32SendCommand(uint8_t cmd)
{ 
  uint8_t payload[2];
	payload[0] = cmd;
	payload[1] = cmd ^ 0xFF;
  LOG(LL_INFO, ("STM32 Sending Command %02X %02X ", payload[0], payload[1]));
  mgos_uart_write(UART_NO, &payload, sizeof(payload));
  return (true);
}
/**********************************************************************!
 * @fn          stm32Erase
 * @brief       UART callback for RX and TX messages
 * @param[in]   void
 * @return      bool
 **********************************************************************/
static stm32_err_t stm32Erase(struct host_ota_update_context *ctx)
{ 
  stm32_err_t s_err = STM32_ERR_UNKNOWN;
  uint8_t ackResponse;
   
  if(ctx->waitingForPrevCmdToComplete == false)
  {
     /* regular erase (0x43) */
    stm32SendCommand(STM32_CMD_ER);
    ctx->waitingForPrevCmdToComplete = true;
    ctx->hostInternalCmdStep = STM32_ERASE_ER;
  }
  else
  {
    ackResponse = stm32CheckResponse(ctx->uart_rx_buff, NULL);
    //send noraml erase data page command
    if(ackResponse == STM32_ACK && ctx->hostInternalCmdStep == STM32_ERASE_ER)
    {
       /* 0xFF 0x00 the magic number for mass erase */  
      stm32SendCommand (0xFF) ; 
      ctx->hostInternalCmdStep = STM32_ERASE_ER_PAGE_CMD;      
    }
    //send extended erase data page command
    else if (ackResponse == STM32_ACK && ctx->hostInternalCmdStep == STM32_ERASE_EE)
    {  
      ctx->hostInternalCmdStep = STM32_ERASE_EE_PAGE_CMD;
      uint8_t payload[3];
      /* 0xFFFF the magic number for mass erase */
      payload[0] = 0xFF;
      payload[1] = 0xFF;
      payload[2] = 0x00;	/* checksum */
      LOG(LL_INFO, ("STM32 trying extended erase command"));    
      mgos_uart_write(UART_NO, &payload, sizeof(payload));
    }
    //successful chip erase
    else if(ackResponse == STM32_ACK && 
            (ctx->hostInternalCmdStep == STM32_ERASE_ER_PAGE_CMD || 
            ctx->hostInternalCmdStep == STM32_ERASE_EE_PAGE_CMD))
    {
       ctx->waitingForPrevCmdToComplete = false;
       s_err = STM32_ERR_OK;
       ctx->hostInternalCmdStep = STM32_ERASE_ER;    
    } 
    // Tried normal Earse command , lets try extended erase   
    else if (ackResponse == STM32_NACK && 
             ctx->hostInternalCmdStep == STM32_ERASE_ER)
    {      
      /* Try Extended Erase */
      stm32SendCommand(STM32_CMD_EE);
      ctx->hostInternalCmdStep = STM32_ERASE_EE;
    }    
    //Tried allerase commands but failed, send error code
    else if (ackResponse == STM32_NACK && 
             ctx->hostInternalCmdStep == STM32_ERASE_EE)
    {      
      s_err = STM32_ERR_NACK;
      ctx->hostInternalCmdStep = STM32_ERASE_ER;
      ctx->status_msg = "Host MCU falsh erase failure ";     
    }
  }

  LOG(LL_INFO, ("stm32Erase Status : 0x%02x ",s_err));
  return (s_err);
}
/**********************************************************************!
 * @fn          stm32FlashMode
 * @brief       UART callback for RX and TX messages
 * @param[in]   void
 * @return      bool
 **********************************************************************/
static stm32_err_t stm32FlashMode(struct host_ota_update_context *ctx)
{  
  stm32_err_t s_err = STM32_ERR_UNKNOWN;
  uint8_t ackResponse;

  if(ctx->waitingForPrevCmdToComplete == false)
  {
    mgos_gpio_write(mgos_sys_config_get_dccm_host_ota_boot0_pin(), true);
    LOG(LL_INFO,("Flash mode : boot pin high"));
    mgos_msleep(50);
    mgos_gpio_write(mgos_sys_config_get_dccm_host_ota_rst_pin(), false);
    LOG(LL_INFO,("Flash mode : reset pin low"));
    //mgos_gpio_write(LED, LOW);
    mgos_msleep(50);
    mgos_gpio_write(mgos_sys_config_get_dccm_host_ota_rst_pin(), true);
    LOG(LL_INFO,("Flash mode : reset pin high"));
    mgos_msleep(100);

    s_err = stm32_send_init_seq();
    ctx->waitingForPrevCmdToComplete = true;
  } 
  else
  {
    ackResponse = stm32CheckResponse(ctx->uart_rx_buff, NULL);
    if (ackResponse == STM32_ACK)
    {
      ctx->waitingForPrevCmdToComplete = false;
      s_err = STM32_ERR_OK;
    }
    else if (ackResponse == STM32_NACK)
    {
      s_err = STM32_ERR_NACK;
      ctx->status_msg = "MCU boot mode failure , retry programming ";
    }    
  }

  LOG(LL_INFO, ("stm32FlashMode Status : 0x%02x ",s_err));

  return (s_err);
}
/**********************************************************************!
 * @fn          stm32RunMode
 * @brief       UART callback for RX and TX messages
 * @param[in]   void
 * @return      bool
 **********************************************************************/
static void stm32RunMode(struct host_ota_update_context *ctx)
{ 
  mgos_gpio_write(mgos_sys_config_get_dccm_host_ota_boot0_pin(), false);
  LOG(LL_INFO,("Run mode : boot pin low"));
  mgos_msleep(50);
  mgos_gpio_write(mgos_sys_config_get_dccm_host_ota_rst_pin(), false);
  LOG(LL_INFO,("Run mode : reset pin low"));
  //digitalWrite(LED, LOW);
  mgos_msleep(50);
  mgos_gpio_write(mgos_sys_config_get_dccm_host_ota_rst_pin(), true);
  LOG(LL_INFO,("Run mode : reset pin high"));
  mgos_msleep(100);
  
  LOG(LL_INFO, ("STM32 reboot-->rubn mode successful"));

}
/**********************************************************************!
 * @fn          stm32GetId
 * @brief       UART callback for RX and TX messages
 * @param[in]   void
 * @return      bool
 **********************************************************************/
static stm32_err_t stm32GetId(struct host_ota_update_context *ctx)
{ // Tested
  uint16_t getid = 0;
  stm32_err_t s_err = STM32_ERR_UNKNOWN;
  struct mbuf rx_data ={0};
  uint8_t ackResponse;

  if(ctx->waitingForPrevCmdToComplete == false)
  {
    stm32SendCommand(STM32_CMD_GID);
    ctx->waitingForPrevCmdToComplete = true;
    s_err = STM32_ERR_WAIT;
  }
  else
  {
    ackResponse = stm32CheckResponse(ctx->uart_rx_buff,&rx_data);
    if(ackResponse == STM32_ACK)
    {
      if (rx_data.len >= 5)
      {
        if (rx_data.buf[0] == STM32_ACK &&
            rx_data.buf[1] == 0x1 &&
            rx_data.buf[4] == STM32_ACK)
        {
          getid = (rx_data.buf[2] << 8) + rx_data.buf[3];      
          // Initialize with unknown chip 
          for( uint16_t i =0; i< (sizeof (stm32_chip)/sizeof(stm32_chip[0]));i++)
          {
            if(stm32_chip[i].chip_id == getid)
            {
              LOG(LL_INFO, ("chip found %s",stm32_chip[i].chipName));
              strcpy(ctx->host_mcu_id,stm32_chip[i].chipName);
              ctx->waitingForPrevCmdToComplete = false;
              s_err = STM32_ERR_OK; 
              break;              
            }
            //LOG(LL_INFO, ("chipid ::%02X %s", stm32_chip[i].chip_id, stm32_chip[i].chipName));
          }
        }
      }      
    }
    else if(ackResponse == STM32_NACK)
    {
      s_err = STM32_ERR_NACK;
      ctx->status_msg = "MCU Get ID Fail";
    }    
  }

  mbuf_free(&rx_data);
  LOG(LL_INFO, ("chipid ::%02X %s s_err: 0x%02x", getid, ctx->host_mcu_id,s_err));  
  return (s_err);
}

/**********************************************************************!
 * @fn          stm32GetId
 * @brief       UART callback for RX and TX messages
 * @param[in]   void
 * @return      bool
 **********************************************************************/
enum mgos_host_ota_state stm32BeginReprogramming (struct host_ota_update_context *ctx)
{
  static stm32_err_t s_err = STM32_ERR_NO_CMD;
  enum mgos_host_ota_state ret = ctx->host_ota_state;
  /* Define array of reprogramming sequence - order is important */  
  stm32_err_t (*stm32Reprogramstates[STM32_REPROGRAMMING_MAX_STATES]) \
  (struct host_ota_update_context *ctx) = {stm32FlashMode,
                                           stm32GetId,
                                           stm32Erase,
                                           stm32programflash};

  //Waiting for previous command response ?
  if (ctx->waitingForPrevCmdToComplete == true)
  {
    //call previous function to verify the response
    s_err = stm32Reprogramstates[ctx->hostOTAstep](ctx);
    if (s_err == STM32_ERR_OK)
    {
      ctx->hostOTAstep++;
    }
    //If NACK response abort the reprogramming
    else if (s_err == STM32_ERR_NACK)
    {
      ctx->hostOTAstep = STM32_REPROGRAMMING_MAX_STATES;
      ret = MGOS_HOST_OTA_STATE_ERROR;
    }
  }
  //If previous commond response is successful ,execute next command sequence
  if (ctx->hostOTAstep < STM32_REPROGRAMMING_MAX_STATES &&
      ctx->waitingForPrevCmdToComplete == false)
  {
    s_err = stm32Reprogramstates[ctx->hostOTAstep](ctx);
    ret = MGOS_HOST_OTA_STATE_PROGRESS;    
  }
  else if (ctx->waitingForPrevCmdToComplete == false &&
           ctx->hostOTAstep >= STM32_REPROGRAMMING_MAX_STATES)
  {
    ret = MGOS_HOST_OTA_STATE_SUCCESS;
    ctx->result = 0;
    ctx->status_msg = "MCU Reprogramming Successfull";
  }

  ctx->host_ota_state = ret;

  mgos_event_trigger(MGOS_EVENT_DCCM_STATUS, ctx);

  LOG(LL_INFO, ("OTA State %d step %d waiting : %d", ctx->host_ota_state, ctx->hostOTAstep, ctx->waitingForPrevCmdToComplete));

  return ctx->host_ota_state;
}
/**********************************************************************!
 * @fn          stm32GetReprogrammingCtx
 * @brief       UART callback for RX and TX messages
 * @param[in]   void
 * @return      bool
 **********************************************************************/
void stm32GetReprogrammingCtx(struct host_ota_update_context *s_host_ota_ctx)
{
   s_host_ota_ctx->hostOTAstep = 0;
   s_host_ota_ctx->host_ota_state = MGOS_HOST_OTA_STATE_IDLE;
   s_host_ota_ctx->host_mcu_begin_reprogramming = stm32BeginReprogramming;   
   s_host_ota_ctx->need_reboot = true;
   s_host_ota_ctx->ignore_same_version = false;
   s_host_ota_ctx->host_mcu_reset = stm32RunMode;
}

#if 0

#endif



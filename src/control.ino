#include "config.h"
#include "scsi_defs.h"

#if SUPPORT_CONTROL
/*
 * RECEIVE Command processing.
 */
void onControlReceiveCommand(uint32_t adds, uint32_t len)
{
  LOG("-R ");
  LOGHEX4N(len);

  if(!m_sel) {
    m_sts |= STATUS_CHECK;
    m_phase = PHASE_STATUSIN;
    return;
  }
  
  switch(m_cmd[1] & 0x3) {
    case 0:
      if(len == 0) return;
      break;
    case 1: // Fixed
      break;
    case 2: // SILI
      break;
    case 3: // Illegal Request
      m_sts = 0x02;
      m_sel->m_sense.m_key = ILLEGAL_REQUEST;
      m_sel->m_sense.m_code = INVALID_FIELD_IN_CDB;       /* "Invalid field in CDB" */
      m_sel->m_sense.m_key_specific[0] = ERROR_IN_OPCODE;  /* "Error in Byte 1" */
      m_sel->m_sense.m_key_specific[1] = 0x00;
      m_sel->m_sense.m_key_specific[2] = 0x01;
      m_phase = PHASE_STATUSIN;
      return;
  }

  LED_ON();
  writeDataPhase(len, m_responsebuffer);
  LED_OFF();

  m_phase = PHASE_STATUSIN;
}

/*
 * SEND Command processing.
 */
void onControlSendCommand(uint32_t adds, uint32_t len)
{
  LOG("-W ");
  LOGHEX4N(len);
  
  if(!m_sel) {
    m_sts |= STATUS_CHECK;
    m_phase = PHASE_STATUSIN;
    return;
  }
  if(len > MAX_BLOCKSIZE) {
      m_sel->m_sense.m_key = ILLEGAL_REQUEST;
      m_sel->m_sense.m_code = INVALID_FIELD_IN_CDB;    /* "Invalid field in CDB" */
      m_sel->m_sense.m_key_specific[0] = ERROR_IN_OPCODE;  /* "Error in Byte 4" */
      m_sel->m_sense.m_key_specific[1] = 0x00;
      m_sel->m_sense.m_key_specific[2] = 0x04;
      m_sts |= STATUS_CHECK;
      m_phase = PHASE_STATUSIN;
      return;
  }

  LED_ON();
  readDataPhase(len, m_responsebuffer);  
  LED_OFF();

  m_phase = PHASE_STATUSIN;
}

void ControlReceive6CommandHandler() {
  LOG("[Receive6]");
  onControlReceiveCommand((((uint32_t)m_cmd[1] & 0x1F) << 16) | ((uint32_t)m_cmd[2] << 8) | m_cmd[3], (m_cmd[4] == 0) ? 0x100 : m_cmd[4]);
}

void ControlSend6CommandHandler() {
  LOG("[Send6]");
  onControlSendCommand((((uint32_t)m_cmd[1] & 0x1F) << 16) | ((uint32_t)m_cmd[2] << 8) | m_cmd[3], (m_cmd[4] == 0) ? 0x100 : m_cmd[4]);
}

void ConfigureControlHandlers(VirtualDevice_t *vdev) {
  for(int c = 0; c < 256; c++)
    vdev->m_handler[c] = &UnknownCommandHandler;

  vdev->m_handler[CMD_TEST_UNIT_READY]               = &TestUnitCommandHandler;
  vdev->m_handler[CMD_REQUEST_SENSE]                 = &RequestSenseCommandHandler;
  vdev->m_handler[CMD_READ6]                         = &ControlReceive6CommandHandler;
  vdev->m_handler[CMD_WRITE6]                        = &ControlSend6CommandHandler;
  vdev->m_handler[CMD_INQUIRY]                       = &InquiryCommandHandler;
  vdev->m_handler[CMD_SEND_DIAGNOSTIC]               = &SendDiagnosticCommandHandler;
}

// If config file exists, read the first three lines and copy the contents.
// File must be well formed or you will get junk in the SCSI Vendor fields.
void ConfigureControl(VirtualDevice_t *vdev, const char *image_name) {
  for(int i = 0; SCSI_INQUIRY_RESPONSE[i][0] != 0xff; i++) {
    if(SCSI_INQUIRY_RESPONSE[i][0] == DEV_PROCESSOR) {
      memcpy(vdev->m_inquiryresponse, SCSI_INQUIRY_RESPONSE[i], SCSI_INQUIRY_RESPONSE_SIZE);
      break;
    }
  }

  vdev->m_type = DEV_PROCESSOR;
  ConfigureControlHandlers(vdev);
}
#endif

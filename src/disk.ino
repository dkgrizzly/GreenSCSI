#include "scsi_defs.h"

void Read6CommandHandler() {
  LOG("[Read6]");
  m_sts |= onReadCommand((((uint32_t)m_cmd[1] & 0x1F) << 16) | ((uint32_t)m_cmd[2] << 8) | m_cmd[3], (m_cmd[4] == 0) ? 0x100 : m_cmd[4]);
  m_phase = PHASE_STATUSIN;
}

void Write6CommandHandler() {
  LOG("[Write6]");
  m_sts |= onWriteCommand((((uint32_t)m_cmd[1] & 0x1F) << 16) | ((uint32_t)m_cmd[2] << 8) | m_cmd[3], (m_cmd[4] == 0) ? 0x100 : m_cmd[4]);
  m_phase = PHASE_STATUSIN;
}

void Seek6CommandHandler() {
  LOG("[Seek6]");
  m_phase = PHASE_STATUSIN;
}

/*
 * READ CAPACITY command processing.
 */
void ReadCapacityCommandHandler() {
  LOGN("[ReadCapacity]");
  if(!m_img) {
    m_sts |= 0x02; // Image file absent
    m_phase = PHASE_STATUSIN;
    return;
  }

  uint32_t bl = m_img->m_blocksize;
  uint32_t bc = m_img->m_fileSize / bl;
  uint8_t buf[8] = {
    (uint8_t)(((uint32_t)(bc >> 24))&0xff), (uint8_t)(((uint32_t)(bc >> 16))&0xff), (uint8_t)(((uint32_t)(bc >> 8))&0xff), (uint8_t)(((uint32_t)(bc))&0xff),
    (uint8_t)(((uint32_t)(bl >> 24))&0xff), (uint8_t)(((uint32_t)(bl >> 16))&0xff), (uint8_t)(((uint32_t)(bl >> 8))&0xff), (uint8_t)(((uint32_t)(bl))&0xff)
  };
  writeDataPhase(8, buf);
  m_phase = PHASE_STATUSIN;
}

void Read10CommandHandler() {
  LOG("[Read10]");
  m_sts |= onReadCommand(((uint32_t)m_cmd[2] << 24) | ((uint32_t)m_cmd[3] << 16) | ((uint32_t)m_cmd[4] << 8) | m_cmd[5], ((uint32_t)m_cmd[7] << 8) | m_cmd[8]);
  m_phase = PHASE_STATUSIN;
}

void Write10CommandHandler() {
  LOG("[Write10]");
  m_sts |= onWriteCommand(((uint32_t)m_cmd[2] << 24) | ((uint32_t)m_cmd[3] << 16) | ((uint32_t)m_cmd[4] << 8) | m_cmd[5], ((uint32_t)m_cmd[7] << 8) | m_cmd[8]);
  m_phase = PHASE_STATUSIN;
}

void Seek10CommandHandler() {
  LOG("[Seek10]");
  m_phase = PHASE_STATUSIN;
}

/*
 * READ6 / 10 Command processing.
 */
uint8_t onReadCommand(uint32_t adds, uint32_t len)
{
  LOG("-R ");
  LOGHEX6(adds);
  LOG(" ");
  LOGHEX4N(len);

  if(!m_img) return 0x02; // Image file absent
  
  LED_ON();
  writeDataPhaseSD(adds, len);
  LED_OFF();
  return 0x00; //sts
}

/*
 * WRITE6 / 10 Command processing.
 */
uint8_t onWriteCommand(uint32_t adds, uint32_t len)
{
  LOG("-W ");
  LOGHEX6(adds);
  LOG(" ");
  LOGHEX4N(len);
  
  if(!m_img) return 0x02; // Image file absent
  
  LED_ON();
  readDataPhaseSD(adds, len);
  LED_OFF();
  return 0; //sts
}

void ConfigureDiskHandlers(int id) {
  for(int c = 0; c < 256; c++)
    m_handler[id][c] = &UnknownCommandHandler;

  m_handler[id][CMD_TEST_UNIT_READY] = &TestUnitCommandHandler;
  m_handler[id][CMD_REZERO_UNIT]     = &RezeroUnitCommandHandler;
  m_handler[id][CMD_REQUEST_SENSE]   = &RequestSenseCommandHandler;
  m_handler[id][CMD_FORMAT_UNIT]     = &FormatUnitCommandHandler;
  m_handler[id][0x06] = &FormatUnitCommandHandler;
  m_handler[id][0x07] = &ReassignBlocksCommandHandler;
  m_handler[id][CMD_READ6]           = &Read6CommandHandler;
  m_handler[id][CMD_WRITE6]          = &Write6CommandHandler;
  m_handler[id][CMD_SEEK6]           = &Seek6CommandHandler;
  m_handler[id][CMD_INQUIRY]         = &InquiryCommandHandler;
  m_handler[id][CMD_MODE_SELECT6]                  = &ModeSelect6CommandHandler;
  m_handler[id][CMD_MODE_SENSE6]     = &ModeSense6CommandHandler;
  m_handler[id][CMD_START_STOP_UNIT] = &StartStopUnitCommandHandler;
  m_handler[id][CMD_PREVENT_REMOVAL] = &PreAllowMediumRemovalCommandHandler;
  m_handler[id][CMD_READ_CAPACITY10] = &ReadCapacityCommandHandler;
  m_handler[id][CMD_READ10]          = &Read10CommandHandler;
  m_handler[id][CMD_WRITE10]         = &Write10CommandHandler;
  m_handler[id][CMD_SEEK10]          = &Seek10CommandHandler;
  m_handler[id][CMD_MODE_SENSE10]    = &ModeSense10CommandHandler;
#if SCSI_SELECT == 1
  m_handler[id][0xC2]                = &DTCsetDriveParameterCommandHandler;
#endif    
}

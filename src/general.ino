#include "scsi_defs.h"


/*
 * INQUIRY command processing.
 */
void InquiryCommandHandler() {
  LOGN("[Inquiry]");
  uint8_t len = m_cmd[4];
  uint8_t buf[36];
  memcpy(buf, m_inquiryresponse[m_id], 36);

  if(!m_img) buf[0] = 0x7f;

  writeDataPhase(len < 36 ? len : 36, buf);
  m_phase = PHASE_STATUSIN;
}

/*
 * REQUEST SENSE command processing.
 */
void RequestSenseCommandHandler() {
  LOGN("[RequestSense]");
  uint8_t len = m_cmd[4];
  uint8_t buf[18] = {
    0x70,   //CheckCondition
    0,      //Segment number
    0x00,   //Sense key
    0, 0, 0, 0,  //information
    17 - 7 ,   //Additional data length
    0,
  };

  if(!m_img) {
    // Image file absent
    buf[2] = 0x02; // NOT_READY
    buf[12] = 0x25; // Logical Unit Not Supported
  } else {
    buf[2] = m_sense[m_id][m_lun].m_key;
    m_sense[m_id][m_lun].m_key = 0;
  }
  writeDataPhase(len < 18 ? len : 18, buf);  
  m_phase = PHASE_STATUSIN;
}

void TestUnitCommandHandler() {
  LOGN("[TestUnit]");
  if(!m_img) {
    // Image file absent
    m_sense[m_id][m_lun].m_key = NOT_READY; // NOT_READY
    m_sense[m_id][m_lun].m_code = NO_MEDIA; // Logical Unit Not Supported
  }
  m_phase = PHASE_STATUSIN;
}

void RezeroUnitCommandHandler() {
  LOGN("[RezeroUnit]");
  m_phase = PHASE_STATUSIN;
}

void FormatUnitCommandHandler() {
  LOGN("[FormatUnit]");
  m_phase = PHASE_STATUSIN;
}

void ReassignBlocksCommandHandler() {
  LOGN("[ReassignBlocks]");
  m_phase = PHASE_STATUSIN;
}

void ModeSense6CommandHandler() {
  LOGN("[ModeSense6]");
  m_sts |= onModeSenseCommand(m_cmd[1]&0x80, m_cmd[2], m_cmd[4]);
  m_phase = PHASE_STATUSIN;
}

uint8_t onModeSelectCommand() {
  m_sense[m_id][m_lun].m_code = INVALID_FIELD_IN_CDB;    /* "Invalid field in CDB" */
  m_sense[m_id][m_lun].m_key_specific[0] = ERROR_IN_OPCODE;  /* "Error in Byte 2" */
  m_sense[m_id][m_lun].m_key_specific[1] = 0x00;
  m_sense[m_id][m_lun].m_key_specific[2] = 0x04;
  return 0x02;
}

void ModeSelect6CommandHandler() {
  LOG("[ModeSelect6] ");
  uint16_t len = m_cmd[4];
  readDataPhase(len, m_buf);

  for(int i = 1; i < len; i++ ) {
    LOG(":");
    LOGHEX2(m_buf[i]);
  }
  LOGN("");
//  m_sts |= onModeSelectCommand();
  m_phase = PHASE_STATUSIN;
}

void ModeSelect10CommandHandler() {
  LOGN("[ModeSelect10]");
  uint16_t len = ((uint16_t)m_cmd[7] << 8) | m_cmd[8];
  readDataPhase(len, m_buf);

  for(int i = 1; i < len; i++ ) {
    LOG(":");
    LOGHEX2(m_buf[i]);
  }
  LOGN("");
//  m_sts |= onModeSelectCommand();
  m_phase = PHASE_STATUSIN;
}

void StartStopUnitCommandHandler() {
  LOGN("[StartStopUnit]");
  m_phase = PHASE_STATUSIN;
}

void PreAllowMediumRemovalCommandHandler() {
  LOGN("[PreAllowMed.Removal]");
  m_phase = PHASE_STATUSIN;
}

void ModeSense10CommandHandler() {
  LOGN("[ModeSense10]");
  onModeSenseCommand(m_cmd[1] & 0x80, m_cmd[2], ((uint32_t)m_cmd[7] << 8) | m_cmd[8]);
  m_phase = PHASE_STATUSIN;
}

void UnknownCommandHandler() {
  LOGN("[*Unknown]");
  m_sts |= 0x02;
  m_sense[m_id][m_lun].m_key = 5;
  m_phase = PHASE_STATUSIN;
}

void BadLunCommandHandler() {
  LOGN("[Bad LUN]");
  m_sts |= 0x02;
  m_phase = PHASE_STATUSIN;
}

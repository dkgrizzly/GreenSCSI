#include "config.h"
#include "scsi_defs.h"


/*
 * INQUIRY command processing.
 */
void InquiryCommandHandler() {
  LOGN("[Inquiry]");
  uint8_t len;

  if(!m_sel) {
    memset(m_responsebuffer, 0, 96);
    m_responsebuffer[0] = 0x7f;
    m_responsebuffer[4] = 35 - 4;
    len = 36;
  } else {
    memcpy(m_responsebuffer, m_sel->m_inquiryresponse, 96);
    len = m_responsebuffer[4] + 5;
  }

  len = min(len, m_cmd[4]);
  writeDataPhase(len, m_responsebuffer);
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

  if(!m_sel) {
    // Image file absent
    buf[2] = 0x02; // NOT_READY
    buf[12] = 0x25; // Logical Unit Not Supported
  } else {
    buf[2] = m_sel->m_sense.m_key;
    buf[12] = m_sel->m_sense.m_code;
    buf[13] = m_sel->m_sense.m_key_specific[0];
    buf[14] = m_sel->m_sense.m_key_specific[1];
    buf[15] = m_sel->m_sense.m_key_specific[2];
    m_sel->m_sense.m_key = 0;
    m_sel->m_sense.m_code = 0;
    m_sel->m_sense.m_key_specific[0] = 0;
    m_sel->m_sense.m_key_specific[1] = 0;
    m_sel->m_sense.m_key_specific[2] = 0;
  }

  writeDataPhase(len < 18 ? len : 18, buf);  
  m_phase = PHASE_STATUSIN;
}

void TestUnitCommandHandler() {
  LOGN("[TestUnit]");
  if(!m_sel) {
    m_sts |= STATUS_CHECK;
    return;
  }
  if(!m_sel->m_file.isOpen()) {
    m_sts |= STATUS_CHECK;
    m_sel->m_sense.m_key = NOT_READY; // Not ready
    m_sel->m_sense.m_code = LUN_NOT_READY; // Logical Unit Not Ready, Manual Intervention Required
    m_sel->m_sense.m_key_specific[0] = 0x03;
    return;
  }

  m_phase = PHASE_STATUSIN;
}

void RezeroUnitCommandHandler() {
  LOGN("[RezeroUnit]");
  if(!m_sel) {
    m_sts |= STATUS_CHECK;
    return;
  }
  if(!m_sel->m_file.isOpen()) {
    m_sts |= STATUS_CHECK;
    m_sel->m_sense.m_key = NOT_READY; // Not ready
    m_sel->m_sense.m_code = LUN_NOT_READY; // Logical Unit Not Ready, Manual Intervention Required
    m_sel->m_sense.m_key_specific[0] = 0x03;
    return;
  }
  m_phase = PHASE_STATUSIN;
}

void FormatUnitCommandHandler() {
  LOGN("[FormatUnit]");
  if(!m_sel) {
    m_sts |= STATUS_CHECK;
    return;
  }
  if(!m_sel->m_file.isOpen()) {
    m_sts |= STATUS_CHECK;
    m_sel->m_sense.m_key = NOT_READY; // Not ready
    m_sel->m_sense.m_code = LUN_NOT_READY; // Logical Unit Not Ready, Manual Intervention Required
    m_sel->m_sense.m_key_specific[0] = 0x03;
    return;
  }

  m_phase = PHASE_STATUSIN;
}

void ReassignBlocksCommandHandler() {
  LOGN("[ReassignBlocks]");
  if(!m_sel) {
    m_sts |= STATUS_CHECK;
    return;
  }
  if(!m_sel->m_file.isOpen()) {
    m_sts |= STATUS_CHECK;
    m_sel->m_sense.m_key = NOT_READY; // Not ready
    m_sel->m_sense.m_code = LUN_NOT_READY; // Logical Unit Not Ready, Manual Intervention Required
    m_sel->m_sense.m_key_specific[0] = 0x03;
    return;
  }

  m_phase = PHASE_STATUSIN;
}

#if SUPPORT_APPLE
void AppleEECommandHandler() {
  LOGN("[Apple:0xEE]");
  m_phase = PHASE_STATUSIN;
}
#endif

/*
 * MODE SENSE command processing.
 */
void ModeSenseCommandHandler()
{
  uint16_t len, maxlen;
  int page, pagemax, pagemin;

  switch(m_cmd[0]) {
    case CMD_MODE_SENSE6:
      LOGN("[ModeSense6]");
      maxlen = m_cmd[4];
      len = 1;
      break;
    case CMD_MODE_SENSE10:
      LOGN("[ModeSense10]");
      maxlen = ((uint16_t)m_cmd[7] << 8) | m_cmd[8];
      len = 2;
      break;
    default:
      m_sts |= STATUS_CHECK;
      m_phase = PHASE_STATUSIN;
      return;
  }
  
  /* Check whether medium is present */
  if(!m_sel) {
    m_sts |= STATUS_CHECK; // Image file absent
    m_phase = PHASE_STATUSIN;
    return;
  }
  if(!m_sel->m_file.isOpen()) {
    m_sts |= STATUS_CHECK;
    m_sel->m_sense.m_key = NOT_READY; // Not ready
    m_sel->m_sense.m_code = LUN_NOT_READY; // Logical Unit Not Ready, Manual Intervention Required
    m_sel->m_sense.m_key_specific[0] = 0x03;
    m_phase = PHASE_STATUSIN;
    return;
  }

  memset(m_responsebuffer, 0, sizeof(m_responsebuffer));
#if 0
  if(m_sel->m_quirks & QUIRKS_SASI) {
    int pageCode = cmd2 & 0x3F;
  
    // Assuming sector size 512, number of sectors 25, number of heads 8 as default settings
    int size = m_sel->m_fileSize;
    int cylinders = (int)(size >> 9);
    cylinders >>= 3;
    cylinders /= 25;
    int sectorsize = 512;
    int sectors = 25;
    int heads = 8;
    // Sector size
   int disksize = 0;
    for(disksize = 16; disksize > 0; --(disksize)) {
      if ((1 << disksize) == sectorsize)
        break;
    }
    // Number of blocks
    uint32_t diskblocks = (uint32_t)(size >> disksize);
    int a = 4;
    if(dbd == 0) {
      uint32_t bl = m_sel->m_blocksize;
      uint32_t bc = m_sel->m_fileSize / bl;
      uint8_t c[8] = {
        0,// Density code
        bc >> 16, bc >> 8, bc,
        0, //Reserve
        bl >> 16, bl >> 8, bl
      };
      memcpy(&m_responsebuffer[4], c, 8);
      a += 8;
      m_responsebuffer[3] = 0x08;
    }
    switch(pageCode) {
    case 0x3F:
    {
      m_responsebuffer[len + 0] = 0x01;
      m_responsebuffer[len + 1] = 0x06;
      a += 8;
    }
    case 0x03:  // drive parameters
    {
      m_responsebuffer[len + 0] = 0x80 | 0x03; // Page code
      m_responsebuffer[len + 1] = 0x16; // Page length
      m_responsebuffer[len + 2] = (uint8_t)(heads >> 8);// number of sectors / track
      m_responsebuffer[len + 3] = (uint8_t)(heads);// number of sectors / track
      m_responsebuffer[len + 10] = (uint8_t)(sectors >> 8);// number of sectors / track
      m_responsebuffer[len + 11] = (uint8_t)(sectors);// number of sectors / track
      int size = 1 << disksize;
      m_responsebuffer[len + 12] = (uint8_t)(size >> 8);// number of sectors / track
      m_responsebuffer[len + 13] = (uint8_t)(size);// number of sectors / track
      a += 24;
      if(pageCode != 0x3F) {
        break;
      }
    }
    case 0x04:  // drive parameters
    {
        LOGN("AddDrive");
        m_responsebuffer[len + 0] = 0x04; // Page code
        m_responsebuffer[len + 1] = 0x12; // Page length
        m_responsebuffer[len + 2] = (cylinders >> 16);// Cylinder length
        m_responsebuffer[len + 3] = (cylinders >> 8);
        m_responsebuffer[len + 4] = cylinders;
        m_responsebuffer[len + 5] = heads;   // Number of heads
        a += 20;
      if(pageCode != 0x3F) {
        break;
      }
    }
    default:
      break;
    }
    m_responsebuffer[0] = a - 1;
    writeDataPhase(len < a ? len : a, m_responsebuffer);
  } else
#endif
  {
    /* Default medium type */
    m_responsebuffer[len++] = (m_sel->m_type == DEV_OPTICAL) ? 0xf0 : 0x00;
    /* Write protected */
    m_responsebuffer[len++] = (m_sel->m_type == DEV_OPTICAL) ? 0x80 : 0x00;

    // ModeSense10 has two extra bytes, and Block Descriptor Length has an extra MSB
    if(m_cmd[0] == CMD_MODE_SENSE10) {
      len += 2;
      m_responsebuffer[len++] = 0;
    }
  
    /* Add block descriptor if DBD is not set */
    if (m_cmd[1] & 0x08) {
      m_responsebuffer[len++] = 0;             /* No block descriptor */
    } else {
      if(m_sel->m_type == DEV_TAPE) {
        m_responsebuffer[len++] = 8;             /* Block descriptor length */
        m_responsebuffer[len++] = 0x40;          /* Medium Density Code */
        m_responsebuffer[len++] = 0x00;          /* Number of Blocks (0) */
        m_responsebuffer[len++] = 0x00;
        m_responsebuffer[len++] = 0x00;
        m_responsebuffer[len++] = 0x00;          /* Block Length 1024 */
        m_responsebuffer[len++] = 0x00;
        m_responsebuffer[len++] = 0x04;
        m_responsebuffer[len++] = 0x00;
      } else {
        uint32_t capacity = (m_sel->m_fileSize / m_sel->m_blocksize);
        m_responsebuffer[len++] = 8;             /* Block descriptor length */
        m_responsebuffer[len++] = (capacity >> 24) & 0xff;
        m_responsebuffer[len++] = (capacity >> 16) & 0xff;
        m_responsebuffer[len++] = (capacity >> 8) & 0xff;
        m_responsebuffer[len++] = capacity & 0xff;
        m_responsebuffer[len++] = (m_sel->m_blocksize >> 24) & 0xff;
        m_responsebuffer[len++] = (m_sel->m_blocksize >> 16) & 0xff;
        m_responsebuffer[len++] = (m_sel->m_blocksize >> 8) & 0xff;
        m_responsebuffer[len++] = (m_sel->m_blocksize) & 0xff;
      }
    }
  
    /* Check for requested mode page */
    page = m_cmd[2] & 0x3F;
    pagemax = (page != 0x3f) ? page : 0x3e;
    pagemin = (page != 0x3f) ? page : 0x00;
    for(page = pagemax; page >= pagemin; page--) {
      switch (page) {
        case MODEPAGE_VENDOR_SPECIFIC:
          /* Accept request only for current values */
          if (m_cmd[2] & 0xC0) {
            //DEBUGPRINT(DBG_TRACE, " [2]==%d", m_cmd[2]);
            /* Prepare sense data */
            m_sts |= STATUS_CHECK;
            m_sel->m_sense.m_key = ILLEGAL_REQUEST;
            m_sel->m_sense.m_code = INVALID_FIELD_IN_CDB;    /* "Invalid field in CDB" */
            m_sel->m_sense.m_key_specific[0] = ERROR_IN_OPCODE;  /* "Error in Byte 2" */
            m_sel->m_sense.m_key_specific[1] = 0x00;
            m_sel->m_sense.m_key_specific[2] = 0x02;
            m_phase = PHASE_STATUSIN;
            return;
          }
  
          /* Unit attention */
          m_responsebuffer[len++] = 0x80; // PS, page id
          m_responsebuffer[len++] = 0x02; // Page length
          m_responsebuffer[len++] = 0x00;
          m_responsebuffer[len++] = 0x00;
          break;
        case MODEPAGE_RW_ERROR_RECOVERY:
          if(m_cmd[2] & 0x40) {
            m_sts |= STATUS_CHECK;
            m_sel->m_sense.m_key = ILLEGAL_REQUEST;
            m_sel->m_sense.m_code = INVALID_FIELD_IN_CDB;    /* "Invalid field in CDB" */
            m_sel->m_sense.m_key_specific[0] = ERROR_IN_OPCODE;  /* "Error in Byte 2" */
            m_sel->m_sense.m_key_specific[1] = 0x00;
            m_sel->m_sense.m_key_specific[2] = 0x02;
            m_phase = PHASE_STATUSIN;
            return;
          }
          m_responsebuffer[len++] = MODEPAGE_RW_ERROR_RECOVERY; // PS, page id
          m_responsebuffer[len++] = 0x0a; // Page length
          m_responsebuffer[len++] = 0x05; // 
          m_responsebuffer[len++] = 0x00; // Read Retry Count
          m_responsebuffer[len++] = 0x00; // Reserved
          m_responsebuffer[len++] = 0x00; // Reserved
          m_responsebuffer[len++] = 0x00; // Reserved
          m_responsebuffer[len++] = 0x00; // Reserved
          m_responsebuffer[len++] = 0x00; // Write Retry Count
          m_responsebuffer[len++] = 0x00; // Reserved
          m_responsebuffer[len++] = 0x00; // Recovery Time Limit
          m_responsebuffer[len++] = 0x00;
          break;
#if 0
        case MODEPAGE_DCRC_PARAMETERS:
          m_responsebuffer[len++] = 0x82; // PS, page id
          m_responsebuffer[len++] = 0x0e; // Page length
          m_responsebuffer[len++] = 0xe6; // Buffer full ratio, 90%
          m_responsebuffer[len++] = 0x1a; // Buffer empty ratio, 10%
          m_responsebuffer[len++] = 0x00; // Bus inactivity limit
          m_responsebuffer[len++] = 0x00;
          m_responsebuffer[len++] = 0x00; // Disconnect time limit
          m_responsebuffer[len++] = 0x00;
          m_responsebuffer[len++] = 0x00; // Connect time limit
          m_responsebuffer[len++] = 0x00;
          m_responsebuffer[len++] = 0x00; // Maximum burst size
          m_responsebuffer[len++] = 0x00;
          m_responsebuffer[len++] = 0x00; // EMDP, Dimm, DTDC
          m_responsebuffer[len++] = 0x00; // Reserved
          m_responsebuffer[len++] = 0x00; // Reserved
          m_responsebuffer[len++] = 0x00; // Reserved
          break;
#endif
#if SUPPORT_APPLE
      case MODEPAGE_APPLE:
        if( m_sel->m_quirks & QUIRKS_APPLE) {
          m_responsebuffer[len++] = 0xb0;
          m_responsebuffer[len++] = 0x16;
          m_responsebuffer[len++] = 'A';
          m_responsebuffer[len++] = 'P';
          m_responsebuffer[len++] = 'P';
          m_responsebuffer[len++] = 'L';
          m_responsebuffer[len++] = 'E';
          m_responsebuffer[len++] = ' ';
          m_responsebuffer[len++] = 'C';
          m_responsebuffer[len++] = 'O';
          m_responsebuffer[len++] = 'M';
          m_responsebuffer[len++] = 'P';
          m_responsebuffer[len++] = 'U';
          m_responsebuffer[len++] = 'T';
          m_responsebuffer[len++] = 'E';
          m_responsebuffer[len++] = 'R';
          m_responsebuffer[len++] = ',';
          m_responsebuffer[len++] = ' ';
          m_responsebuffer[len++] = 'I';
          m_responsebuffer[len++] = 'N';
          m_responsebuffer[len++] = 'C';
          m_responsebuffer[len++] = ' ';
          m_responsebuffer[len++] = ' ';
          m_responsebuffer[len++] = ' ';
        }
        break;
#endif
      case MODEPAGE_FORMAT_PARAMETERS:
          m_responsebuffer[len + 0] = MODEPAGE_FORMAT_PARAMETERS; //Page code
          m_responsebuffer[len + 1] = 0x16; // Page length
          m_responsebuffer[len + 11] = 0x3F;//Number of sectors / track
          len += 24;
          break;
        case MODEPAGE_RIGID_GEOMETRY:
          {
            uint32_t bc = m_sel->m_fileSize / m_sel->m_file;
            m_responsebuffer[len + 0] = MODEPAGE_RIGID_GEOMETRY; //Page code
            m_responsebuffer[len + 1] = 0x16; // Page length
            m_responsebuffer[len + 2] = bc >> 16;// Cylinder length
            m_responsebuffer[len + 3] = bc >> 8;
            m_responsebuffer[len + 4] = bc;
            m_responsebuffer[len + 5] = 1;   //Number of heads
            len += 24;
          }
          break;
        default:
          if(pagemin == pagemax) {
            m_sts |= STATUS_CHECK;
            m_sel->m_sense.m_key = ILLEGAL_REQUEST;
            m_sel->m_sense.m_code = INVALID_FIELD_IN_CDB;       /* "Invalid field in CDB" */
            m_sel->m_sense.m_key_specific[0] = ERROR_IN_OPCODE;  /* "Error in Byte 2" */
            m_sel->m_sense.m_key_specific[1] = 0x00;
            m_sel->m_sense.m_key_specific[2] = 0x02;
            m_phase = PHASE_STATUSIN;
            return;
          }
      }
    }
  
    /* Report size of requested data */
    if(m_cmd[0] == CMD_MODE_SENSE6) {
      m_responsebuffer[0] = len;
    } else {  
      m_responsebuffer[0] = (len >> 8) & 0xff;
      m_responsebuffer[1] = len & 0xff;
    }  

    /* Truncate data if necessary */
    if (maxlen < len) {
      len = maxlen;
    }
  
    // Send it
    writeDataPhase(len, m_responsebuffer);
  }
  m_phase = PHASE_STATUSIN;
}

uint8_t onModeSelectCommand() {
  return STATUS_GOOD;
}

void ModeSelect6CommandHandler() {
  LOG("[ModeSelect6] ");
  uint16_t len = m_cmd[4];
  if(!m_sel) {
    m_sts |= STATUS_CHECK; // Image file absent
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
  readDataPhase(len, m_responsebuffer);

  for(int i = 1; i < len; i++ ) {
    LOG(":");
    LOGHEX2(m_responsebuffer[i]);
  }
  LOGN("");
  m_sts |= onModeSelectCommand();
  m_phase = PHASE_STATUSIN;
}

void ModeSelect10CommandHandler() {
  LOGN("[ModeSelect10]");
  uint16_t len = ((uint16_t)m_cmd[7] << 8) | m_cmd[8];
  if(!m_sel) {
    m_sts |= STATUS_CHECK; // Image file absent
    m_phase = PHASE_STATUSIN;
    return;
  }
  if(len > MAX_BLOCKSIZE) {
      m_sel->m_sense.m_key = ILLEGAL_REQUEST;
      m_sel->m_sense.m_code = INVALID_FIELD_IN_CDB;    /* "Invalid field in CDB" */
      m_sel->m_sense.m_key_specific[0] = ERROR_IN_OPCODE;  /* "Error in Byte 7" */
      m_sel->m_sense.m_key_specific[1] = 0x00;
      m_sel->m_sense.m_key_specific[2] = 0x07;
      m_sts |= STATUS_CHECK;
      m_phase = PHASE_STATUSIN;
      return;
  }
  readDataPhase(len, m_responsebuffer);

  for(int i = 1; i < len; i++ ) {
    LOG(":");
    LOGHEX2(m_responsebuffer[i]);
  }
  LOGN("");
  m_sts |= onModeSelectCommand();
  m_phase = PHASE_STATUSIN;
}

void SearchDataEqualCommandHandler() {
  LOGN("[SearchDataEqual]");
  m_phase = PHASE_STATUSIN;
}

void ReadDefectCommandHandler() {
  LOGN("[ReadDefect]");
  m_responsebuffer[0] = 0x00;
  m_responsebuffer[1] = m_cmd[2];
  m_responsebuffer[2] = m_cmd[7]; // List Length MSB
  m_responsebuffer[3] = m_cmd[8]; // List Length LSB
  writeDataPhase(4, m_responsebuffer);

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

void UnknownCommandHandler() {
  LOGN("[*Unknown]");
  m_sts |= STATUS_CHECK;
  if(m_sel) {
    m_sel->m_sense.m_key = 5;
  }
  m_phase = PHASE_STATUSIN;
}

void BadLunCommandHandler() {
  LOGN("[Bad LUN]");
  m_sts |= STATUS_CHECK;
  m_phase = PHASE_STATUSIN;
}

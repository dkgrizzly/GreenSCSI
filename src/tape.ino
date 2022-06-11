#include "config.h"
#include "scsi_defs.h"

#if SUPPORT_TAPE
/*
 * READ6 / 10 Command processing.
 */
void onTapeReadCommand(uint32_t adds, uint32_t len)
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
  writeDataPhaseTape(len);
  LED_OFF();

  m_phase = PHASE_STATUSIN;
}

/*
 * WRITE6 / 10 Command processing.
 */
void onTapeWriteCommand(uint32_t adds, uint32_t len)
{
  LOG("-W ");
  LOGHEX4N(len);
  
  if(!m_sel) {
    m_sts |= STATUS_CHECK;
    m_phase = PHASE_STATUSIN;
    return;
  }
  
  LED_ON();
  m_sel->m_file.write(&len, 4);
  readDataPhaseTape(len);
  m_sel->m_file.write(&len, 4);
  LED_OFF();

  m_phase = PHASE_STATUSIN;
}

void TapeRead6CommandHandler() {
  LOG("[Read6]");
  m_sts |= onTapeReadCommand((((uint32_t)m_cmd[1] & 0x1F) << 16) | ((uint32_t)m_cmd[2] << 8) | m_cmd[3], (m_cmd[4] == 0) ? 0x100 : m_cmd[4]);
  m_phase = PHASE_STATUSIN;
}

void TapeWrite6CommandHandler() {
  LOG("[Write6]");
  m_sts |= onTapeWriteCommand((((uint32_t)m_cmd[1] & 0x1F) << 16) | ((uint32_t)m_cmd[2] << 8) | m_cmd[3], (m_cmd[4] == 0) ? 0x100 : m_cmd[4]);
  m_phase = PHASE_STATUSIN;
}

void TapeSeek6CommandHandler() {
  LOG("[Seek6]");
  m_phase = PHASE_STATUSIN;
}

void TapeRead10CommandHandler() {
  LOG("[Read10]");
  m_sts |= onTapeReadCommand(((uint32_t)m_cmd[2] << 24) | ((uint32_t)m_cmd[3] << 16) | ((uint32_t)m_cmd[4] << 8) | m_cmd[5], ((uint32_t)m_cmd[7] << 8) | m_cmd[8]);
  m_phase = PHASE_STATUSIN;
}

void TapeWrite10CommandHandler() {
  LOG("[Write10]");
  m_sts |= onTapeWriteCommand(((uint32_t)m_cmd[2] << 24) | ((uint32_t)m_cmd[3] << 16) | ((uint32_t)m_cmd[4] << 8) | m_cmd[5], ((uint32_t)m_cmd[7] << 8) | m_cmd[8]);
  m_phase = PHASE_STATUSIN;
}

void TapeSeek10CommandHandler() {
  LOG("[Seek10]");
  m_phase = PHASE_STATUSIN;
}

void TapeModeSense6CommandHandler() {
  uint8_t len;
  int page, pagemax, pagemin;

  LOGN("[ModeSense6]");
  /* Check whether medium is present */
  if(!m_sel) {
    m_sts |= STATUS_CHECK;
    m_phase = PHASE_STATUSIN;
    return;
  }

  memset(m_responsebuffer, 0, sizeof(m_responsebuffer));

  len = 1;
  /* Default medium type */
  m_responsebuffer[len++] = 0xf0;
  /* Write protected */
  m_responsebuffer[len++] = 0x80;

  /* Add block descriptor if DBD is not set */
  if (m_cmd[1] & 0x08) {
    m_responsebuffer[len++] = 0;             /* No block descriptor */
  } else {
    uint32_t capacity = (m_sel->m_fileSize / m_sel->m_blocksize) - 1;
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

  /* Check for requested mode page */
  page = m_cmd[2] & 0x3F;
  pagemax = (page != 0x3f) ? page : 0x3e;
  pagemin = (page != 0x3f) ? page : 0x00;
  for(page = pagemax; page >= pagemin; page--) {
    switch (page) {
      default:
        if(pagemin == pagemax) {
          /* Requested mode page is not supported */
          /* Prepare sense data */
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
  m_responsebuffer[0] = len;

  /* Truncate data if necessary */
  if (m_cmd[4] < len) {
    len = m_cmd[4];
  }

  // Send it
  writeDataPhase(len, m_responsebuffer);
  m_phase = PHASE_STATUSIN;
}

void TapeLoadUnloadCommandHandler() {
  if(m_cmd[4] & 1) {
    LOGN("[Load]");
  } else {
    LOGN("[Unload]");
  }
  if(!m_sel) {
    m_sts |= STATUS_CHECK;
    m_phase = PHASE_STATUSIN;
    return;
  }
  
  m_phase = PHASE_STATUSIN;
}

void TapePreventRemovalCommandHandler() {
  if(m_cmd[4] & 1) {
    LOGN("[Prevent Removal]");
  } else {
    LOGN("[Allow Removal]");
  }
  if(!m_sel) {
    m_sts |= STATUS_CHECK;
    m_phase = PHASE_STATUSIN;
    return;
  }
  
  m_phase = PHASE_STATUSIN;
}

void TapeReadCapacityCommandHandler() {
  LOGN("[ReadCapacity]");
  if(!m_sel) {
    m_sts |= STATUS_CHECK; // Image file absent
    m_phase = PHASE_STATUSIN;
    return;
  }

  uint32_t bl = m_sel->m_blocksize;
  uint32_t bc = m_sel->m_fileSize / bl;
  uint8_t buf[8] = {
    (uint8_t)(((uint32_t)(bc >> 24))&0xff), (uint8_t)(((uint32_t)(bc >> 16))&0xff), (uint8_t)(((uint32_t)(bc >> 8))&0xff), (uint8_t)(((uint32_t)(bc))&0xff),
    (uint8_t)(((uint32_t)(bl >> 24))&0xff), (uint8_t)(((uint32_t)(bl >> 16))&0xff), (uint8_t)(((uint32_t)(bl >> 8))&0xff), (uint8_t)(((uint32_t)(bl))&0xff)
  };
  writeDataPhase(8, buf);
  m_phase = PHASE_STATUSIN;
}

void TapeEraseCommandHandler() {
  LOGN("[Erase]");
  if(!m_sel) {
    m_sts |= STATUS_CHECK; // Image file absent
    m_phase = PHASE_STATUSIN;
    return;
  }
  // Truncate the current .tap file (maybe archive it by renaming instead of truncating?)
}

void TapeReadBlockLimitsCommandHandler() {
  uint16_t len = 0;
  LOGN("[ReadBlockLimits]");
  if(!m_sel) {
    m_sts |= STATUS_CHECK; // Image file absent
    m_phase = PHASE_STATUSIN;
    return;
  }
  m_responsebuffer[len++] = 0x00;
  m_responsebuffer[len++] = 0xFF; // Maximum Block Length
  m_responsebuffer[len++] = 0xFF;
  m_responsebuffer[len++] = 0xFF;
  m_responsebuffer[len++] = 0x00; // Minimum Block Length
  m_responsebuffer[len++] = 0x00;

  writeDataPhase(len, buf);
  m_phase = PHASE_STATUSIN;
}

void TapeRewindUnitCommandHandler() {
  LOGN("[Rewind]");
  if(!m_sel) {
    m_sts |= STATUS_CHECK; // Image file absent
    m_phase = PHASE_STATUSIN;
    return;
  }
  m_sel->m_filemarks = 0;
  m_sel->m_file.seek(0);
  m_phase = PHASE_STATUSIN;
}

void TapeReadPositionCommandHandler() {
  uint16_t len = 0;
  LOGN("[ReadPosition]");
  if(!m_sel) {
    m_sts |= STATUS_CHECK; // Image file absent
    m_phase = PHASE_STATUSIN;
    return;
  }

  m_responsebuffer[len++] = (m_sel->m_filemarks == 0) ? (1 << 7) : 0;
  m_responsebuffer[len++] = 0x00; // Partition(0)
  m_responsebuffer[len++] = 0x00; // Reserved
  m_responsebuffer[len++] = 0x00; // Reserved
  m_responsebuffer[len++] = (m_sel->m_filemarks & 0xff); // First Block Location
  m_responsebuffer[len++] = (m_sel->m_filemarks & 0xff00) >> 8;
  m_responsebuffer[len++] = (m_sel->m_filemarks & 0xff0000) >> 16;
  m_responsebuffer[len++] = (m_sel->m_filemarks & 0xff000000) >> 24;
  m_responsebuffer[len++] = (m_sel->m_filemarks & 0xff); // Last Block Location
  m_responsebuffer[len++] = (m_sel->m_filemarks & 0xff00) >> 8;
  m_responsebuffer[len++] = (m_sel->m_filemarks & 0xff0000) >> 16;
  m_responsebuffer[len++] = (m_sel->m_filemarks & 0xff000000) >> 24;
  m_responsebuffer[len++] = 0x00; // Reserved
  m_responsebuffer[len++] = 0x00; // Blocks in buffer (0)
  m_responsebuffer[len++] = 0x00;
  m_responsebuffer[len++] = 0x00;
  m_responsebuffer[len++] = 0x00; // Bytes in buffer (0)
  m_responsebuffer[len++] = 0x00;
  m_responsebuffer[len++] = 0x00;
  m_responsebuffer[len++] = 0x00;

  writeDataPhase(len, buf);
  m_phase = PHASE_STATUSIN;
}

void ConfigureTapeHandlers(VirtualDevice_t *vdev) {
  for(int c = 0; c < 256; c++)
    vdev->m_handler[c] = &UnknownCommandHandler;

  vdev->m_handler[CMD_ERASE]                         = &TapeEraseCommandHandler;
  vdev->m_handler[CMD_TEST_UNIT_READY]               = &TestUnitCommandHandler;
  vdev->m_handler[CMD_REZERO_UNIT]                   = &TapeRewindUnitCommandHandler;
  vdev->m_handler[CMD_REQUEST_SENSE]                 = &RequestSenseCommandHandler;
  vdev->m_handler[CMD_READ_BLOCK_LIMITS]             = &TapeReadBlockLimitsCommandHandler;
  vdev->m_handler[CMD_READ6]                         = &TapeRead6CommandHandler;
  vdev->m_handler[CMD_WRITE6]                        = &TapeWrite6CommandHandler;
  vdev->m_handler[CMD_SEEK6]                         = &TapeSeek6CommandHandler;
  vdev->m_handler[CMD_INQUIRY]                       = &InquiryCommandHandler;
  vdev->m_handler[CMD_MODE_SELECT6]                  = &ModeSelect6CommandHandler;
  vdev->m_handler[CMD_MODE_SENSE6]                   = &TapeModeSense6CommandHandler;
  vdev->m_handler[CMD_START_STOP_UNIT]               = &TapeLoadUnloadCommandHandler;
  vdev->m_handler[CMD_PREVENT_REMOVAL]               = &TapePreventRemovalCommandHandler;
  vdev->m_handler[CMD_READ_CAPACITY10]               = &TapeReadCapacityCommandHandler;
  vdev->m_handler[CMD_READ10]                        = &TapeRead10CommandHandler;
  vdev->m_handler[CMD_WRITE10]                       = &TapeWrite10CommandHandler;
  vdev->m_handler[CMD_SEEK10]                        = &TapeSeek10CommandHandler;
  vdev->m_handler[CMD_READPOSITION10]                = &TapeReadPositionCommandHandler;
  vdev->m_handler[CMD_MODE_SELECT10]                 = &ModeSelect10CommandHandler;
  vdev->m_handler[CMD_MODE_SENSE10]                  = &TapeModeSense10CommandHandler;
}

// If config file exists, read the first three lines and copy the contents.
// File must be well formed or you will get junk in the SCSI Vendor fields.
void ConfigureTape(VirtualDevice_t *vdev, const char *image_name) {
  for(int i = 0; SCSI_INQUIRY_RESPONSE[i][0] != 0xff; i++) {
    if(SCSI_INQUIRY_RESPONSE[i][0] == DEV_TAPE) {
      memcpy(vdev->m_inquiryresponse, SCSI_INQUIRY_RESPONSE[i], SCSI_INQUIRY_RESPONSE_SIZE);
      break;
    }
  }

  if(image_name) {
    char configname[MAX_FILE_PATH+1];
    memcpy(configname, image_name, MAX_FILE_PATH+1);
    char *psuffix = strstr(configname, ".img");
    if(psuffix) {
      strcpy(psuffix, ".cfg");
    } else {
      sprintf(configname, "mt%d%d.cfg", vdev->m_id, vdev->m_lun);
    }
  
    FsFile config_file = sd.open(configname, O_RDONLY);
    if (config_file.isOpen()) {
      char vendor[9];
      memset(vendor, 0, sizeof(vendor));
      config_file.readBytes(vendor, sizeof(vendor));
      LOGN("SCSI VENDOR: ");
      LOGN(vendor);
      memcpy(&(vdev->m_inquiryresponse[8]), vendor, 8);
    
      char product[17];
      memset(product, 0, sizeof(product));
      config_file.readBytes(product, sizeof(product));
      LOGN("SCSI PRODUCT: ");
      LOGN(product);
      memcpy(&(vdev->m_inquiryresponse[16]), product, 16);
    
      char version[5];
      memset(version, 0, sizeof(version));
      config_file.readBytes(version, sizeof(version));
      LOGN("SCSI VERSION: ");
      LOGN(version);
      memcpy(&(vdev->m_inquiryresponse[32]), version, 4);
      config_file.close();
    }
  }

  vdev->m_type = DEV_TAPE;
  ConfigureTapeHandlers(vdev);
}
#endif

#include "scsi_defs.h"

static const uint8_t SessionTOC[] =
{
  0x00, // toc length, MSB
  0x0A, // toc length, LSB
  0x01, // First session number
  0x01, // Last session number,
  // TRACK 1 Descriptor
  0x00, // reserved
  0x14, // Q sub-channel encodes current position, Digital track
  0x01, // First track number in last complete session
  0x00, // Reserved
  0x00,0x00,0x00,0x00 // LBA of first track in last session
};

static const uint8_t FullTOC[] =
{
  0x00, // toc length, MSB
  0x44, // toc length, LSB
  0x01, // First session number
  0x01, // Last session number,
  // A0 Descriptor
  0x01, // session number
  0x14, // ADR/Control
  0x00, // TNO
  0xA0, // POINT
  0x00, // Min
  0x00, // Sec
  0x00, // Frame
  0x00, // Zero
  0x01, // First Track number.
  0x00, // Disc type 00 = Mode 1
  0x00,  // PFRAME
  // A1
  0x01, // session number
  0x14, // ADR/Control
  0x00, // TNO
  0xA1, // POINT
  0x00, // Min
  0x00, // Sec
  0x00, // Frame
  0x00, // Zero
  0x01, // Last Track number
  0x00, // PSEC
  0x00,  // PFRAME
  // A2
  0x01, // session number
  0x14, // ADR/Control
  0x00, // TNO
  0xA2, // POINT
  0x00, // Min
  0x00, // Sec
  0x00, // Frame
  0x00, // Zero
  0x79, // LEADOUT position BCD
  0x59, // leadout PSEC BCD
  0x74, // leadout PFRAME BCD
  // TRACK 1 Descriptor
  0x01, // session number
  0x14, // ADR/Control
  0x00, // TNO
  0x01, // Point
  0x00, // Min
  0x00, // Sec
  0x00, // Frame
  0x00, // Zero
  0x00, // PMIN
  0x00, // PSEC
  0x00,  // PFRAME
  // b0
  0x01, // session number
  0x54, // ADR/Control
  0x00, // TNO
  0xB1, // POINT
  0x79, // Min BCD
  0x59, // Sec BCD
  0x74, // Frame BCD
  0x00, // Zero
  0x79, // PMIN BCD
  0x59, // PSEC BCD
  0x74,  // PFRAME BCD
  // c0
  0x01, // session number
  0x54, // ADR/Control
  0x00, // TNO
  0xC0, // POINT
  0x00, // Min
  0x00, // Sec
  0x00, // Frame
  0x00, // Zero
  0x00, // PMIN
  0x00, // PSEC
  0x00  // PFRAME
};

static const uint8_t DiscInfoBlock[] =
{
  0x00, // disc info length, MSB
  0x44, // disc info length, LSB
  0x0e, // DiscStatus = Complete
  0x01, // First Track on Disc
  0x01, // Sessions on Disc (LSB)
  0x01, // First Track in Last Session (LSB)
  0x01, // Last Track in Last Session (LSB)
  0x20, // DID_V DBC_V URU
  0x00, // Disc Type
  0x00, // Sessions on Disc (MSB)
  0x00, // First Track in Last Session (MSB)
  0x00, // Last Track in Last Session (MSB)
  0x00, // DiscID (MSB)
  0x00, // DiscID
  0x00, // DiscID
  0x00, // DiscID (LSB)
  0x00, // Last Session Lead-In Start (MSB)
  0x00, //   IN MSF
  0x00, //
  0x00, // Last Session Lead-In Start (LSB)
  0x00, // Last Possible Lead-Out Start (MSB)
  0x00, //   IN MSF
  0x00, //
  0x00, // Last Possible Lead-Out Start (LSB)
  0x00, // Bar Code (MSB)
  0x00, //
  0x00, //
  0x00, //
  0x00, //
  0x00, //
  0x00, //
  0x00, // Bar Code (LSB)
  0x00, // Reserved
  0x00, // Number of OPC Entries
};

static void LBA2MSF(uint32_t LBA, uint8_t* MSF)
{
  MSF[0] = 0; // reserved.
  MSF[3] = (uint32_t)(LBA % 75); // F
  uint32_t rem = LBA / 75;

  MSF[2] = (uint32_t)(rem % 60); // S
  MSF[1] = (uint32_t)(rem / 60); // M
}


void OpticalModeSense6CommandHandler() {
  uint8_t len;
  int page, pagemax, pagemin;

  LOGN("[ModeSense6]");
  /* Check whether medium is present */
  if(!m_img) {
    m_sts |= 0x02;
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
    uint32_t capacity = (m_img->m_fileSize / m_img->m_blocksize) - 1;
    m_responsebuffer[len++] = 8;             /* Block descriptor length */
    m_responsebuffer[len++] = (capacity >> 24) & 0xff;
    m_responsebuffer[len++] = (capacity >> 16) & 0xff;
    m_responsebuffer[len++] = (capacity >> 8) & 0xff;
    m_responsebuffer[len++] = capacity & 0xff;
    m_responsebuffer[len++] = (m_img->m_blocksize >> 24) & 0xff;
    m_responsebuffer[len++] = (m_img->m_blocksize >> 16) & 0xff;
    m_responsebuffer[len++] = (m_img->m_blocksize >> 8) & 0xff;
    m_responsebuffer[len++] = (m_img->m_blocksize) & 0xff;
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
          m_sense[m_id][m_lun].m_key = ILLEGAL_REQUEST;
          m_sense[m_id][m_lun].m_code = INVALID_FIELD_IN_CDB;    /* "Invalid field in CDB" */
          m_sense[m_id][m_lun].m_key_specific[0] = ERROR_IN_OPCODE;  /* "Error in Byte 2" */
          m_sense[m_id][m_lun].m_key_specific[1] = 0x00;
          m_sense[m_id][m_lun].m_key_specific[2] = 0x02;
          m_phase = PHASE_STATUSIN;
          return;
        }

        /* Unit attention */
        m_responsebuffer[len++] = 0x80; // PS, page id
        m_responsebuffer[len++] = 0x02; // Page length
        m_responsebuffer[len++] = 0x00;
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
      case MODEPAGE_RW_ERROR_RECOVERY:
        m_responsebuffer[len++] = 0x81; // PS, page id
        m_responsebuffer[len++] = 0x0a; // Page length
        m_responsebuffer[len++] = 0x07; // 
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
      default:
        if(pagemin == pagemax) {
          /* Requested mode page is not supported */
          /* Prepare sense data */
          m_sts |= STATUS_CHECK;
          m_sense[m_id][m_lun].m_key = ILLEGAL_REQUEST;
          m_sense[m_id][m_lun].m_code = INVALID_FIELD_IN_CDB;       /* "Invalid field in CDB" */
          m_sense[m_id][m_lun].m_key_specific[0] = ERROR_IN_OPCODE;  /* "Error in Byte 2" */
          m_sense[m_id][m_lun].m_key_specific[1] = 0x00;
          m_sense[m_id][m_lun].m_key_specific[2] = 0x02;
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

void OpticalReadSimpleTOC()
{
  int MSF = m_cmd[1] & 0x02 ? 1 : 0;
  uint16_t allocationLength = (m_cmd[7] << 8) | m_cmd[8];
  uint8_t len = 0;

  if(!m_img) {
    m_sts |= 0x02;
    m_phase = PHASE_STATUSIN;
    return;
  }
  
  memset(m_responsebuffer, 0, sizeof(m_responsebuffer));

  uint32_t capacity = (m_img->m_fileSize / m_img->m_blocksize) - 1;
  m_responsebuffer[len++] = 0x00; // toc length, MSB
  m_responsebuffer[len++] = 0x00; // toc length, LSB
  m_responsebuffer[len++] = 0x01; // First track number
  m_responsebuffer[len++] = 0x01; // Last track number

  // We only support track 1 and 0xaa (lead-out).
  // track 0 means "return all tracks"
  switch (m_cmd[6]) {
    case 0x00:
    case 0x01:
      m_responsebuffer[len++] = 0x00; // reserved
      m_responsebuffer[len++] = 0x14; // Q sub-channel encodes current position, Digital track
      m_responsebuffer[len++] = 0x01; // Track 1
      m_responsebuffer[len++] = 0x00; // Reserved
      m_responsebuffer[len++] = 0x00; // MSB LBA
      m_responsebuffer[len++] = 0x00; // 
      m_responsebuffer[len++] = 0x00; // 
      m_responsebuffer[len++] = 0x00; // LSB LBA
    case 0xAA:
      m_responsebuffer[len++] = 0x00; // reserved
      m_responsebuffer[len++] = 0x14; // Q sub-channel encodes current position, Digital track
      m_responsebuffer[len++] = 0xAA; // Leadout Track
      m_responsebuffer[len++] = 0x00; // Reserved
      // Replace start of leadout track
      if (MSF)
      {
        LBA2MSF(capacity, m_responsebuffer + len);
        len+=4;
      }
      else
      {
        // Track start sector (LBA)
        m_responsebuffer[len++] = capacity >> 24;
        m_responsebuffer[len++] = capacity >> 16;
        m_responsebuffer[len++] = capacity >> 8;
        m_responsebuffer[len++] = capacity;
      }
      break;
    default:
      m_sts |= STATUS_CHECK;
      m_sense[m_id][m_lun].m_key = ILLEGAL_REQUEST; // Illegal Request
      m_sense[m_id][m_lun].m_code = INVALID_FIELD_IN_CDB; // Invalid field in CDB
      m_sense[m_id][m_lun].m_key_specific[0] = ERROR_IN_OPCODE; // Error in Byte 6
      m_sense[m_id][m_lun].m_key_specific[1] = 0x00;
      m_sense[m_id][m_lun].m_key_specific[2] = 0x06;
      m_phase = PHASE_STATUSIN;
  }

  m_responsebuffer[0] = (len >> 8) & 0xff;
  m_responsebuffer[1] = len & 0xff;
  
  // Truncate if necessary
  if(allocationLength < len)
    len = allocationLength;

  // Send it
  writeDataPhase(len, m_responsebuffer);
  m_phase = PHASE_STATUSIN;
}

void OpticalReadSessionInfo()
{
//  int MSF = m_cmd[1] & 0x02 ? 1 : 0;
  uint16_t allocationLength = (m_cmd[7] << 8) | m_cmd[8];
  uint32_t len = sizeof(SessionTOC);
  memcpy(m_responsebuffer, SessionTOC, len);

  // Truncate if necessary
  if(allocationLength < len)
    len = allocationLength;

  // Send it
  writeDataPhase(len, m_responsebuffer);
  m_phase = PHASE_STATUSIN;
}

uint8_t fromBCD(uint8_t val)
{
  return ((val >> 4) * 10) + (val & 0xF);
}

void OpticalReadFullTOC(int convertBCD)
{
  uint16_t allocationLength = (m_cmd[7] << 8) | m_cmd[8];
  // We only support session 1.
  if (m_cmd[6] > 1) {
    m_sts |= STATUS_CHECK;
    m_sense[m_id][m_lun].m_key = ILLEGAL_REQUEST; // Illegal Request
    m_sense[m_id][m_lun].m_code = INVALID_FIELD_IN_CDB; // Invalid field in CDB
    m_sense[m_id][m_lun].m_key_specific[0] = ERROR_IN_OPCODE; // Error in Byte 6
    m_sense[m_id][m_lun].m_key_specific[1] = 0x00;
    m_sense[m_id][m_lun].m_key_specific[2] = 0x06;
    m_phase = PHASE_STATUSIN;
    return;
  }

  uint32_t len = sizeof(FullTOC);
  memcpy(m_responsebuffer, FullTOC, len);

  if (convertBCD)
  {
    uint32_t descriptor = 4;
    while (descriptor < len)
    {
      int i;
      for (i = 0; i < 7; ++i)
      {
        m_responsebuffer[descriptor + i] =
          fromBCD(m_responsebuffer[descriptor + 4 + i]);
      }
      descriptor += 11;
    }

  }

  // Truncate if necessary
  if(allocationLength < len)
    len = allocationLength;

  // Send it
  writeDataPhase(len, m_responsebuffer);
  m_phase = PHASE_STATUSIN;
}

void OpticalReadTOCCommandHandler()
{
  LOGN("[Read TOC]");
  switch(m_cmd[2] & 0xf) {
    case 0:
      LOGN("Simple");
      OpticalReadSimpleTOC();
      return;
    case 1:
      LOGN("Session");
      OpticalReadSessionInfo();
      return;
    case 2:
      LOGN("Full 0");
      OpticalReadFullTOC(0);
      return;
    case 3:
      LOGN("Full 1");
      OpticalReadFullTOC(1);
      return;
  }

  m_sts |= STATUS_CHECK;
  m_sense[m_id][m_lun].m_key = ILLEGAL_REQUEST; // Illegal Request
  m_sense[m_id][m_lun].m_code = INVALID_FIELD_IN_CDB; // Invalid field in CDB
  m_sense[m_id][m_lun].m_key_specific[0] = ERROR_IN_OPCODE; // Error in Byte 2
  m_sense[m_id][m_lun].m_key_specific[1] = 0x00;
  m_sense[m_id][m_lun].m_key_specific[2] = 0x02;
  m_phase = PHASE_STATUSIN;
}

static uint8_t SimpleHeader[] =
{
  0x01, // 2048byte user data, L-EC in 288 byte aux field.
  0x00, // reserved
  0x00, // reserved
  0x00, // reserved
  0x00,0x00,0x00,0x00 // Track start sector (LBA or MSF)
};

void OpticalHeaderCommandHandler()
{
  uint32_t len = sizeof(SimpleHeader);
  memcpy(m_responsebuffer, SimpleHeader, len);

  LOGN("[Read Header]");

  //int MSF = m_cmd[1] & 0x02 ? 1 : 0;
  //uint32_t lba = 0;
  uint16_t allocationLength = (m_cmd[7] << 8) | m_cmd[8];

  // Truncate if necessary
  if(allocationLength < len)
    len = allocationLength;

  // Send it
  writeDataPhase(len, m_responsebuffer);
  m_phase = PHASE_STATUSIN;
}


void OpticalReadDiscInfoCommandHandler()
{
  uint16_t allocationLength = (m_cmd[7] << 8) | m_cmd[8];
  uint32_t len = sizeof(DiscInfoBlock);
  uint32_t capacity = (m_img->m_fileSize / m_img->m_blocksize) - 1;
  memcpy(m_responsebuffer, DiscInfoBlock, len);

  LOGN("[DiscInfo]");

  // Truncate if necessary
  if(allocationLength < len)
    len = allocationLength;

  LBA2MSF(capacity, m_responsebuffer + 20);

  // Send it
  writeDataPhase(len, m_responsebuffer);
  m_phase = PHASE_STATUSIN;
}

uint16_t SupportedFeatures[] = {
  0x0001, // Core Features
  0x0003, // Removable Media
  0x0010, // Random Readable
  0x001D, // Reads all Media Types
  0x001E, // Reads CD Structures
};

void OpticalGetConfigurationCommandHandler()
{
  uint16_t sfn = (m_cmd[2] << 8) | m_cmd[3];
  uint16_t allocationLength = (m_cmd[7] << 8) | m_cmd[8];
  uint16_t sfnmax = (allocationLength - 8) / 4;
  uint16_t sfi = 0;
  uint8_t len;

  memset(m_responsebuffer, 0, sizeof(m_responsebuffer));

  switch(m_cmd[1] & 0x3) {
    case 0: {
      break;
    }
    case 1: {
      break;
    }
    case 2: {
      sfnmax = 1;
      break;
    }
  }

  len = 8;
  for(sfi = 0; (sfi < sizeof(SupportedFeatures)) && sfnmax; sfi++) {
    if( SupportedFeatures[sfi] > sfn ) {
      m_responsebuffer[len++] = (SupportedFeatures[sfi] >> 8) & 0xff;
      m_responsebuffer[len++] = SupportedFeatures[sfi] & 0xff;
      m_responsebuffer[len++] = 0;
      m_responsebuffer[len++] = 0;
      sfnmax--;
    }
  }

  m_responsebuffer[0] = (len >> 24) & 0xff;
  m_responsebuffer[1] = (len >> 16) & 0xff;
  m_responsebuffer[2] = (len >> 8) & 0xff;
  m_responsebuffer[3] = (len) & 0xff;

  m_responsebuffer[6] = 0x00; // CD-ROM Profile
  m_responsebuffer[7] = 0x08;
  
  // Truncate if necessary
  if(allocationLength < len)
    len = allocationLength;

  // Send it
  writeDataPhase(len, m_responsebuffer);
  m_phase = PHASE_STATUSIN;
}


void OpticalEventStatusCommandHandler()
{
  uint8_t len = 0;
  memset(m_responsebuffer, 0, sizeof(m_responsebuffer));

  LOGN("[Read Event Status]");

  if((m_cmd[1] & 1) == 0) {
    m_sts |= STATUS_CHECK;
    m_sense[m_id][m_lun].m_key = ILLEGAL_REQUEST; // Illegal Request
    m_sense[m_id][m_lun].m_code = INVALID_FIELD_IN_CDB; // Invalid field in CDB
    m_sense[m_id][m_lun].m_key_specific[0] = ERROR_IN_OPCODE; // Error in Byte 1
    m_sense[m_id][m_lun].m_key_specific[1] = 0x00;
    m_sense[m_id][m_lun].m_key_specific[2] = 0x01;
    m_phase = PHASE_STATUSIN;
    return;
  }

  int ncr;
  for(ncr = 0; ncr < 8; ncr++) {
    if(m_cmd[4] & (1<<ncr)) {
      switch (ncr) {
        case 1:
          m_responsebuffer[len++] = 0x00; // MSB Descriptor Length
          m_responsebuffer[len++] = 0x06; // LSB
          m_responsebuffer[len++] = 0x02; // Operational Change Event
          m_responsebuffer[len++] = 0x12; // Supported Event Class
          m_responsebuffer[len++] = 0x00; // No Change
          m_responsebuffer[len++] = 0x00; // Operational Status
          m_responsebuffer[len++] = 0x00; // MSB Operational Change
          m_responsebuffer[len++] = 0x00; // LSB
          break;
        case 4:
          m_responsebuffer[len++] = 0x00; // MSB Descriptor Length
          m_responsebuffer[len++] = 0x06; // LSB
          m_responsebuffer[len++] = 0x04; // Media Change Event
          m_responsebuffer[len++] = 0x12; // Supported Event Class
          m_responsebuffer[len++] = 0x00; // No Change
          m_responsebuffer[len++] = (!m_img) ? 0x00 : 0x02; // Media Present
          m_responsebuffer[len++] = 0x00; // Start Slot
          m_responsebuffer[len++] = 0x00; // End Slot
          break;
      }
    }
  }

  uint16_t allocationLength = (m_cmd[7] << 8) | m_cmd[8];

  // Truncate if necessary
  if(allocationLength < len)
    len = allocationLength;

  // Send it
  writeDataPhase(len, m_responsebuffer);
  m_phase = PHASE_STATUSIN;
}

void OpticalLockTrayCommandHandler() {
  if(m_cmd[4] & 1) {
    LOGN("[Lock Tray]");
  } else {
    LOGN("[Unlock Tray]");
  }
  m_phase = PHASE_STATUSIN;
}

void OpticalReadCapacityCommandHandler() {
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

void ConfigureOpticalHandlers(int id) {
  for(int c = 0; c < 256; c++)
    m_handler[id][c] = &UnknownCommandHandler;

  m_handler[id][CMD_TEST_UNIT_READY]               = &TestUnitCommandHandler;
  m_handler[id][CMD_REZERO_UNIT]                   = &RezeroUnitCommandHandler;
  m_handler[id][CMD_REQUEST_SENSE]                 = &RequestSenseCommandHandler;
  m_handler[id][CMD_READ6]                         = &Read6CommandHandler;
  m_handler[id][CMD_SEEK6]                         = &Seek6CommandHandler;
  m_handler[id][CMD_INQUIRY]                       = &InquiryCommandHandler;
  m_handler[id][CMD_MODE_SELECT6]                  = &ModeSelect6CommandHandler;
  m_handler[id][CMD_MODE_SENSE6]                   = &OpticalModeSense6CommandHandler;
  m_handler[id][CMD_START_STOP_UNIT]               = &StartStopUnitCommandHandler;
  m_handler[id][CMD_PREVENT_REMOVAL]               = &OpticalLockTrayCommandHandler;
  m_handler[id][CMD_READ_CAPACITY10]               = &OpticalReadCapacityCommandHandler;
  m_handler[id][CMD_READ10]                        = &Read10CommandHandler;
  m_handler[id][CMD_SEEK10]                        = &Seek10CommandHandler;
  m_handler[id][CMD_READ_TOC]                      = &OpticalReadTOCCommandHandler;
  m_handler[id][CMD_READ_HEADER]                   = &OpticalHeaderCommandHandler;
  m_handler[id][CMD_GET_CONFIGURATION]             = &OpticalGetConfigurationCommandHandler;
  m_handler[id][CMD_GET_EVENT_STATUS_NOTIFICATION] = &OpticalEventStatusCommandHandler;
  m_handler[id][CMD_READ_DISC_INFORMATION]         = &OpticalReadDiscInfoCommandHandler;
  m_handler[id][CMD_MODE_SELECT10]                 = &ModeSelect10CommandHandler;
  m_handler[id][CMD_MODE_SENSE10]                  = &ModeSense10CommandHandler;
}

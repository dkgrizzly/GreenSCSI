#include "config.h"
#include "scsi_defs.h"

/*
 * Check that the image file is present and the block range is valid.
 */
byte checkBlockCommand(uint32_t adds, uint32_t len)
{
  // Check that image file is present
  if(!m_sel) {
    return STATUS_CHECK;
  }
  if(!m_sel->m_file.isOpen()) {
    m_sel->m_sense.m_key = NOT_READY; // Not ready
    m_sel->m_sense.m_code = LUN_NOT_READY; // Logical Unit Not Ready, Manual Intervention Required
    m_sel->m_sense.m_key_specific[0] = 0x03;
    return STATUS_CHECK;
  }
  // Check block range is valid
  uint32_t bc = m_sel->m_fileSize / m_sel->m_blocksize;
  if (adds >= bc || (adds + len) > bc) {
    m_sel->m_sense.m_key = 5; // Illegal request
    m_sel->m_sense.m_code = INVALID_LBA; // Logical block address out of range
    return STATUS_CHECK;
  }
  return 0x00;
}

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
  if(!m_sel) {
    m_sts |= STATUS_CHECK;
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

  uint32_t bl = m_sel->m_blocksize;
  uint32_t bc = m_sel->m_fileSize / bl;
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
  uint8_t sts;

  LOG("-R ");
  LOGHEX6(adds);
  LOG(" ");
  LOGHEX4N(len);

  sts = checkBlockCommand(adds, len);
  if(sts) return sts;
  
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
  uint8_t sts;

  LOG("-W ");
  LOGHEX6(adds);
  LOG(" ");
  LOGHEX4N(len);
  
  sts = checkBlockCommand(adds, len);
  if(sts) return sts;
    
  LED_ON();
  readDataPhaseSD(adds, len);
  LED_OFF();
  return 0; //sts
}

void ConfigureDiskHandlers(VirtualDevice_t *vdev) {
  for(int c = 0; c < 256; c++)
    vdev->m_handler[c] = &UnknownCommandHandler;

  vdev->m_handler[CMD_TEST_UNIT_READY] = &TestUnitCommandHandler;
  vdev->m_handler[CMD_REZERO_UNIT]     = &RezeroUnitCommandHandler;
  vdev->m_handler[CMD_REQUEST_SENSE]   = &RequestSenseCommandHandler;
  vdev->m_handler[CMD_FORMAT_UNIT]     = &FormatUnitCommandHandler;
  vdev->m_handler[CMD_FORMAT_UNIT_ALT] = &FormatUnitCommandHandler;
  vdev->m_handler[CMD_REASSIGN_BLOCKS] = &ReassignBlocksCommandHandler;
  vdev->m_handler[CMD_READ6]           = &Read6CommandHandler;
  vdev->m_handler[CMD_WRITE6]          = &Write6CommandHandler;
  vdev->m_handler[CMD_SEEK6]           = &Seek6CommandHandler;
  vdev->m_handler[CMD_INQUIRY]         = &InquiryCommandHandler;
  vdev->m_handler[CMD_MODE_SELECT6]    = &ModeSelect6CommandHandler;
  vdev->m_handler[CMD_MODE_SENSE6]     = &ModeSenseCommandHandler;
  vdev->m_handler[CMD_START_STOP_UNIT] = &StartStopUnitCommandHandler;
  vdev->m_handler[CMD_PREVENT_REMOVAL] = &PreAllowMediumRemovalCommandHandler;
  vdev->m_handler[CMD_READ_CAPACITY10] = &ReadCapacityCommandHandler;
  vdev->m_handler[CMD_READ10]          = &Read10CommandHandler;
  vdev->m_handler[CMD_WRITE10]         = &Write10CommandHandler;
  vdev->m_handler[CMD_SEEK10]          = &Seek10CommandHandler;
  vdev->m_handler[CMD_MODE_SENSE10]    = &ModeSenseCommandHandler;
  vdev->m_handler[CMD_SEARCH_DATA_EQUAL] = &SearchDataEqualCommandHandler;
  vdev->m_handler[CMD_READ_DEFECT_DATA] = &ReadDefectCommandHandler;
#if SUPPORT_SASI
  if(vdev->m_quirks & QUIRKS_SASI)
    vdev->m_handler[CMD_SET_DRIVE_PARAMETER] = &DTCsetDriveParameterCommandHandler;
#endif
#if SUPPORT_APPLE
  if(vdev->m_quirks & QUIRKS_APPLE)
    vdev->m_handler[CMD_MAC_UNKNOWN] = &AppleEECommandHandler;
#endif
}

// If config file exists, read the first three lines and copy the contents.
// File must be well formed or you will get junk in the SCSI Vendor fields.
void ConfigureDisk(VirtualDevice_t *vdev, const char *image_name) {
  for(int i = 0; SCSI_INQUIRY_RESPONSE[i][0] != 0xff; i++) {
    if(SCSI_INQUIRY_RESPONSE[i][0] == DEV_DISK) {
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
      sprintf(configname, "hd%d%d.cfg", vdev->m_id, vdev->m_lun);
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

  vdev->m_type = DEV_DISK;
  ConfigureDiskHandlers(vdev);
}

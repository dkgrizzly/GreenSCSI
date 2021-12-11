#if SUPPORT_INITIATOR

// Take control of the SCSI bus
void initiatorTakeBus() {
  pinModeFastSlew(ATN, OUTPUT_OPENDRAIN);
  pinModeFastSlew(ACK, OUTPUT_OPENDRAIN);
  pinModeFastSlew(RST, OUTPUT_OPENDRAIN);
  pinModeFastSlew(SEL, OUTPUT_OPENDRAIN);

  pinMode(BSY, INPUT_PULLUP);
  pinMode(MSG, INPUT_PULLUP);
  pinMode(CD,  INPUT_PULLUP);
  pinMode(IO,  INPUT_PULLUP);
  pinMode(REQ, INPUT_PULLUP);

  pinModeFastSlew(DB0, OUTPUT_OPENDRAIN);
  pinModeFastSlew(DB1, OUTPUT_OPENDRAIN);
  pinModeFastSlew(DB2, OUTPUT_OPENDRAIN);
  pinModeFastSlew(DB3, OUTPUT_OPENDRAIN);
  pinModeFastSlew(DB4, OUTPUT_OPENDRAIN);
  pinModeFastSlew(DB5, OUTPUT_OPENDRAIN);
  pinModeFastSlew(DB6, OUTPUT_OPENDRAIN);
  pinModeFastSlew(DB7, OUTPUT_OPENDRAIN);
  pinModeFastSlew(DB8, OUTPUT_OPENDRAIN);

  SCSI_INITIATOR_INACTIVE();
}

// Return the bus to floating, allowing other hosts to take over
void initiatorReleaseBus() {
  // Input port
  pinMode(ATN, INPUT_PULLUP);
  pinMode(ACK, INPUT_PULLUP);
  pinMode(RST, INPUT_PULLUP);
  pinMode(SEL, INPUT_PULLUP);

  // Output port
  pinModeFastSlew(BSY, OUTPUT_OPENDRAIN);
  pinModeFastSlew(MSG, OUTPUT_OPENDRAIN);
  pinModeFastSlew(CD, OUTPUT_OPENDRAIN);
  pinModeFastSlew(IO, OUTPUT_OPENDRAIN);
  pinModeFastSlew(REQ, OUTPUT_OPENDRAIN);

  pinModeFastSlew(DB0, OUTPUT_OPENDRAIN);
  pinModeFastSlew(DB1, OUTPUT_OPENDRAIN);
  pinModeFastSlew(DB2, OUTPUT_OPENDRAIN);
  pinModeFastSlew(DB3, OUTPUT_OPENDRAIN);
  pinModeFastSlew(DB4, OUTPUT_OPENDRAIN);
  pinModeFastSlew(DB5, OUTPUT_OPENDRAIN);
  pinModeFastSlew(DB6, OUTPUT_OPENDRAIN);
  pinModeFastSlew(DB7, OUTPUT_OPENDRAIN);
  pinModeFastSlew(DB8, OUTPUT_OPENDRAIN);

  // Turn off the output port
  SCSI_TARGET_INACTIVE();
}

// Issue a bus reset
void initiatorBusFree() {
  SET_RST_ACTIVE();
  delay(50);
  SET_RST_INACTIVE();
  delay(20);
}

// Selection phase, assert Host & Target ID bits and assert SEL, wait for BSY to be asserted by the target, or timeout.
int initiateSelection(uint8_t target_id, boolean withATN) {
  int timeout = 10000;

  GPIOB_PDDR = SCSI_DB_MASK;
  SCSI_DB_OUTPUT((1 << target_id) | 0x80);

  SET_SEL_ACTIVE();

  while(timeout--) {
    delay(20);
    if(GET_BSY()) break;
  }

  if(withATN) SET_ATN_ACTIVE();

  SET_SEL_INACTIVE();

  SCSI_DB_INPUT();

  return GET_BSY();
}

// Message Out phase, hold ATN asserted until the last message byte
int initiateMessageOut(const uint8_t *buf, uint8_t len) {
  int timeout = 10000;

  SET_ATN_ACTIVE();

  while(timeout--) {
    delay(20);
    if(!GET_CD() && !GET_IO() && GET_MSG()) break;
  }

  if(timeout > 0) {
    for(uint8_t x = 0; x < len; x++) {
      timeout = 10000;
      while(!GET_REQ() && (timeout--));
      if(!timeout) break;

      GPIOB_PDDR = SCSI_DB_MASK;
      SCSI_DB_OUTPUT(buf[x]);

      if((x + 1) == len)
        SET_ATN_INACTIVE();

      SET_ACK_ACTIVE();
      timeout = 10000;
      while(GET_REQ() && (timeout--));
      SET_ACK_INACTIVE();
      if(!timeout) break;
    }
  }

  SET_ATN_INACTIVE();

  SCSI_DB_INPUT();

  return (timeout > 0);
}

// Message In phase, read a single byte from target once it's phase matches
int initiateMessageIn(uint8_t *buf) {
  int timeout = 10000;

  while(timeout--) {
    delay(20);
    if(!GET_CD() && GET_IO() && GET_MSG()) break;
  }

  if(timeout > 0) {
    timeout = 10000;
    while(!GET_REQ() && (timeout--));
    if(!timeout) goto failed;

    buf[0] = readIO();

    SET_ACK_ACTIVE();
    timeout = 10000;
    while(GET_REQ() && (timeout--));
    SET_ACK_INACTIVE();
  }

failed:
  return (timeout > 0);
}

// Command Out phase, tell the target what we want from it.
int initiateCommandOut(const uint8_t *buf, uint8_t len) {
  int timeout = 10000;

  while(timeout--) {
    delay(20);
    if(GET_CD() && !GET_IO() && !GET_MSG()) break;
  }

  if(timeout > 0) {
    for(uint8_t x = 0; x < len; x++) {
      timeout = 10000;
      while(!GET_REQ() && (timeout--));
      if(!timeout) break;

      GPIOB_PDDR = SCSI_DB_MASK;
      SCSI_DB_OUTPUT(buf[x]);

      SET_ACK_ACTIVE();
      timeout = 10000;
      while(GET_REQ() && (timeout--));
      SET_ACK_INACTIVE();
      if(!timeout) break;
    }
  }

  SCSI_DB_INPUT();

  return (timeout > 0);
}

// Data Out phase, send data to target
int initiateDataOut(const uint8_t *buf, uint16_t len) {
  int timeout = 10000;

  while(timeout--) {
    delay(20);
    if(!GET_CD() && !GET_IO() && !GET_MSG()) break;
  }

  if(timeout > 0) {
    for(uint16_t x = 0; x < len; x++) {
      timeout = 10000;
      while(!GET_REQ() && (timeout--));
      if(!timeout) break;

      GPIOB_PDDR = SCSI_DB_MASK;
      SCSI_DB_OUTPUT(buf[x]);

      SET_ACK_ACTIVE();
      timeout = 10000;
      while(GET_REQ() && (timeout--));
      SET_ACK_INACTIVE();

      if(!timeout) break;
    }
  }

  SCSI_DB_INPUT();
  
  return (timeout > 0);
}

// Data In phase, get data from target
int initiateDataIn(uint8_t *buf, uint16_t len) {
  int timeout = 10000;

  while(timeout--) {
    delay(20);
    if(!GET_CD() && GET_IO() && !GET_MSG()) break;
  }

  if(timeout > 0) {
    for(uint16_t x = 0; x < len; x++) {
      timeout = 10000;
      while(!GET_REQ() && (timeout--));
      if(!timeout) break;

      buf[x] = readIO();

      SET_ACK_ACTIVE();
      timeout = 10000;
      while(GET_REQ() && (timeout--));
      SET_ACK_INACTIVE();

      if(!timeout) break;
    }
  }
 
  return (timeout > 0);
}

// Status In phase, read target's command status
int initiateStatusIn(uint8_t *buf) {
  int timeout = 10000;

  while(timeout--) {
    delay(20);
    if(GET_CD() && GET_IO() && !GET_MSG()) break;
  }

  if(timeout > 0) {
    timeout = 10000;
    while(!GET_REQ() && (timeout--));
    if(!timeout) goto failed;

    buf[0] = readIO();

    SET_ACK_ACTIVE();
    timeout = 10000;
    while(GET_REQ() && (timeout--));
    SET_ACK_INACTIVE();
  }
 
failed:
  return (timeout > 0);
}

int initiateReadCapacity(uint8_t target_id, uint32_t *blockSize, uint32_t *blockCount) {
  initiatorTakeBus();

  // Select Target
  if(!initiateSelection(target_id, true)) goto failed;

  // Select LUN 0
  m_buf[0] = 0x80;
  if(!initiateMessageOut(m_buf, 1)) goto failed;

  // Execute Read Capacity
  memset(m_cmd, 0x00, 10);
  m_cmd[0] = CMD_READ_CAPACITY10;
  if(!initiateCommandOut(m_cmd, 10)) goto failed;

  // Get Data Back
  if(!initiateDataIn(m_buf, 8)) goto failed;

  *blockSize = (m_buf[0] << 24) | (m_buf[1] << 16) || (m_buf[2] << 8) | (m_buf[3] << 0); 
  *blockCount = (m_buf[4] << 24) | (m_buf[5] << 16) || (m_buf[6] << 8) | (m_buf[7] << 0);

  // Get Status
  if(!initiateStatusIn(m_buf)) goto failed;

  // Get Messages
  if(!initiateMessageIn(m_buf)) goto failed;

  return 1;

failed:
  initiatorBusFree();
  initiatorReleaseBus();
  return 0;
}

int initiateReadVolume(FsFile file, uint8_t target_id, uint32_t blockSize, uint32_t blockCount) {
  uint32_t pos = 0;
  initiatorTakeBus();

  while(blockCount) {
    uint32_t blocks = min(blockCount, (MAX_BLOCKSIZE / blockSize));
    if(!initiateSelection(target_id, true)) goto failed;

    // Select LUN 0
    m_buf[0] = 0x80;
    if(!initiateMessageOut(m_buf, 1)) goto failed;
    
    // Execute Read
    memset(m_cmd, 0x00, 10);
    m_cmd[0] = CMD_READ10;
    m_cmd[2] = (pos >> 24) & 0xff;
    m_cmd[3] = (pos >> 16) & 0xff;
    m_cmd[4] = (pos >> 8) & 0xff;
    m_cmd[5] = (pos >> 0) & 0xff;
    m_cmd[6] = (blocks >> 8) & 0xff;
    m_cmd[7] = (blocks >> 0) & 0xff;
    if(!initiateCommandOut(m_cmd, 10)) goto failed;
    
    // Get Data Back
    if(!initiateDataIn(m_buf, blockSize * blocks)) goto failed;

    file.write(m_buf, blockSize * blocks);

    // Get Status
    if(!initiateStatusIn(m_buf)) goto failed;
  
    // Get Messages
    if(!initiateMessageIn(m_buf)) goto failed;

    pos += blocks;
    blockCount -= blocks;
  }

  return 1;

failed:
  initiatorBusFree();
  initiatorReleaseBus();
  return 0;
}

void dumpcmd(int argc, char **argv) {
  uint8_t target_id = 0xff;
  uint32_t blockSize, blockCount;
  char tmp_path[MAX_FILE_PATH+1];
  FsFile file;

  if(argc < 3) {
    // Syntax error
    return;
  }

  target_id = strtoul(argv[1], NULL, 0);
  if(target_id > 7) {
    Serial.print("ERROR");
    if(scriptlevel >= 0) Serial.printf(" on line %d of '%s'", exec_line, exec_filename);
    Serial.printf(": SCSI ID '%s' is not within range.\r\n", argv[1]);
    return;
  }

  fixupPath(tmp_path, argv[2]);
  if(strncmp(tmp_path, "/sd/", 4)) {
    Serial.print("ERROR");
    if(scriptlevel >= 0) Serial.printf(" on line %d of '%s'", exec_line, exec_filename);
    Serial.printf(": Can only create images on the SD Card.\r\n");
    return;
  }

  if(!file.open(tmp_path+3, O_WRONLY | O_CREAT | O_TRUNC)) {
    Serial.print("ERROR");
    if(scriptlevel >= 0) Serial.printf(" on line %d of '%s'", exec_line, exec_filename);
    Serial.printf(": Unable to open '%s'.\r\n", tmp_path);
    return;
  }

  detachInterrupt(RST);
  detachInterrupt(SEL);

  if(!initiateReadCapacity(target_id, &blockSize, &blockCount)) {
    Serial.print("ERROR");
    if(scriptlevel >= 0) Serial.printf(" on line %d of '%s'", exec_line, exec_filename);
    Serial.printf(": Unable to read the capacity of SCSI target device.\r\n");
  } else {
    initiateReadVolume(file, target_id, blockSize, blockCount);
  }

  attachInterrupt(RST, onBusReset, FALLING);
  attachInterrupt(SEL, SelectionPhaseISR, FALLING);
  
  file.close();
}

#endif

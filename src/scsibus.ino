#include "config.h"
#include "scsi_defs.h"

/*
 *  Data uint8_t to GPIOB register setting value and parity table
*/

// Parity bit generation
#define PTY(V)   ((1^((V)^((V)>>1)^((V)>>2)^((V)>>3)^((V)>>4)^((V)>>5)^((V)>>6)^((V)>>7)))&1)

// Set DBP
#define DBP(D)    ((((uint32_t)(D)<<16)|(PTY(D)<<11)) ^ SCSI_DB_MASK)

#define DBP8(D)   DBP(D),DBP(D+1),DBP(D+2),DBP(D+3),DBP(D+4),DBP(D+5),DBP(D+6),DBP(D+7)
#define DBP32(D)  DBP8(D),DBP8(D+8),DBP8(D+16),DBP8(D+24)

// BSRR register control value that simultaneously performs DB set, DP set, and REQ = H (inactrive)
const uint32_t db_bsrr[256]={
  DBP32(0x00),DBP32(0x20),DBP32(0x40),DBP32(0x60),
  DBP32(0x80),DBP32(0xA0),DBP32(0xC0),DBP32(0xE0)
};
// Parity bit acquisition
#define PARITY(DB) ((db_bsrr[DB & 0xff] & 0x0800) >> 11)

// Macro cleaning
#undef DBP32
#undef DBP8
//#undef DBP
//#undef PTY

#if USE_DB2ID_TABLE
/* DB to SCSI-ID translation table */
const uint8_t db2scsiid[256]={
  0xff,
  0,
  1,1,
  2,2,2,2,
  3,3,3,3,3,3,3,3,
  4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,
  5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,
  6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,
  6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,
  7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,
  7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,
  7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,
  7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7
};
#endif

// Put DB and DP in output mode
inline void SCSI_DB_OUTPUT(uint8_t d) {
  GPIOB_PDOR = db_bsrr[d];
}

// Put DB and DP in input mode
inline void SCSI_DB_INPUT()  {
  GPIOB_PDDR = 0x00000000;
  GPIOB_PDOR = SCSI_DB_MASK;
}

/*
 * Bus reset interrupt.
 */
void onBusReset(void)
{
#if SUPPORT_SASI
  // SASI I / F for X1 turbo has RST pulse write cycle +2 clock ==
  // I can't filter because it only activates about 1.25us
  if(!(m_sel->m_quirks & QUIRKS_SASI)) {
#endif
    if(digitalRead(RST)) return;
    delayMicroseconds(20);
    if(digitalRead(RST)) return;
#if SUPPORT_SASI
  }
#endif

  SCSI_DB_INPUT();

  LOGN("BusReset!");
  m_isBusReset = true;
}


/*
 * Read by handshake.
 */
inline uint8_t readHandshake(void)
{
  SET_REQ_ACTIVE();
  while(!GET_ACK()) { if(m_isBusReset) { return 0; } }
  uint8_t r = readIO();
  SET_REQ_INACTIVE();
  while(GET_ACK()) { if(m_isBusReset) { return 0; } }
  return r;  
}

inline void readHandshakeBlock(uint8_t *d, uint16_t len)
{
  while(len) {
    SET_REQ_ACTIVE();
    while(!GET_ACK()) { if(m_isBusReset) { return; } }
    *d++ = readIO();
    len--;
    SET_REQ_INACTIVE();
    while(GET_ACK()) { if(m_isBusReset) { return; } }
  }
}

/*
 * Write with a handshake.
 */
inline void writeHandshake(uint8_t d)
{
  GPIOB_PDDR = SCSI_DB_MASK;
  SCSI_DB_OUTPUT(d);

  SET_REQ_ACTIVE();
  while(!GET_ACK()) { if(m_isBusReset) { return; } }
  SET_REQ_INACTIVE();
  SCSI_DB_INPUT();
  while(GET_ACK()) { if(m_isBusReset) { return; } }
}

inline void writeHandshakeBlock(const uint8_t *d, uint16_t len)
{
  GPIOB_PDDR = SCSI_DB_MASK;
  while(len) {
    SCSI_DB_OUTPUT(*d++);
    SET_REQ_ACTIVE();
    while(!GET_ACK()) { if(m_isBusReset) { return; } }
    len--;
    SET_REQ_INACTIVE();
    while(GET_ACK()) { if(m_isBusReset) { return; } }
  }
  SCSI_DB_INPUT();
}

/*
 * Data in phase.
 *  Send len uint8_ts of data array p.
 */
void writeDataPhase(int len, const uint8_t* p)
{
  //LOGN("DATAIN PHASE");
  SET_MSG_INACTIVE();
  SET_CD_INACTIVE();
  SET_IO_ACTIVE();

#if READ_SPEED_OPTIMIZE
  writeHandshakeBlock(p, len);
#else
  for (int i = 0; i < len; i++) {
    if(m_isBusReset) {
      m_phase = PHASE_BUSFREE;
      return;
    }
    writeHandshake(p[i]);
  }
#endif
  if(m_isBusReset) {
    m_phase = PHASE_BUSFREE;
  }
}

/* 
 * Data in phase.
 *  Send len block while reading from SD card.
 */
void writeDataPhaseSD(uint32_t adds, uint32_t len)
{
#if READ_SPEED_OPTIMIZE
  uint32_t bigread = (MAX_BLOCKSIZE / m_sel->m_blocksize);
#endif
  uint32_t i = 0;

  //LOGN("DATAIN PHASE(SD)");
  uint32_t pos = adds * m_sel->m_blocksize;
  m_sel->m_file.seek(pos);

  SET_MSG_INACTIVE();
  SET_CD_INACTIVE();
  SET_IO_ACTIVE();

  while(i < len) {
      // Asynchronous reads will make it faster ...
#if READ_SPEED_OPTIMIZE
    if((len-i) >= bigread) {
      m_sel->m_file.read(m_buf, MAX_BLOCKSIZE);
      writeHandshakeBlock(m_buf, MAX_BLOCKSIZE);
      i += bigread;
    } else {
      m_sel->m_file.read(m_buf, m_sel->m_blocksize * (len-i));
      writeHandshakeBlock(m_buf, m_sel->m_blocksize * (len-i));
      i = len;
    }
#else
    m_sel->m_file.read(m_buf, m_sel->m_blocksize);
    for(unsigned int j = 0; j < m_sel->m_blocksize; j++) {
      if(m_isBusReset) {
        m_phase = PHASE_BUSFREE;
        return;
      }
      writeHandshake(m_buf[j]);
    }
#endif
    if(m_isBusReset) {
      m_phase = PHASE_BUSFREE;
      return;
    }
  }
}

/* 
 * Data in phase.
 *  Send len block while reading from SD card.
 */
void writeDataPhaseRaw(uint32_t adds, uint32_t len)
{
#if READ_SPEED_OPTIMIZE_RAW
  uint32_t bigread = (MAX_BLOCKSIZE / m_sel->m_blocksize);
#endif
  uint32_t i = 0;

  //LOGN("DATAIN PHASE(RAW)");
  uint32_t pos = ((adds * m_sel->m_blocksize) / 512) + m_sel->m_firstSector;

  SET_MSG_INACTIVE();
  SET_CD_INACTIVE();
  SET_IO_ACTIVE();

  while(i < len) {
      // Asynchronous reads will make it faster ...
#if READ_SPEED_OPTIMIZE_RAW
    if((len-i) >= bigread) {
      sd.card()->readSectors(pos, m_buf, (MAX_BLOCKSIZE / 512));
      writeHandshakeBlock(m_buf, MAX_BLOCKSIZE);
      i += bigread;
      pos += (MAX_BLOCKSIZE / 512);
    } else {
      sd.card()->readSectors(pos, m_buf, ((m_sel->m_blocksize * (len-i)) / 512));
      writeHandshakeBlock(m_buf, m_sel->m_blocksize * (len-i));
      i = len;
    }
#else
    sd.card()->readSectors(pos, m_buf, (m_sel->m_blocksize / 512));
    pos++;
    for(unsigned int j = 0; j < m_sel->m_blocksize; j++) {
      if(m_isBusReset) {
        m_phase = PHASE_BUSFREE;
        return;
      }
      writeHandshake(m_buf[j]);
    }
#endif
    if(m_isBusReset) {
      m_phase = PHASE_BUSFREE;
      return;
    }
  }
}

/*
 * Data out phase.
 *  len block read
 */
void readDataPhase(int len, uint8_t* p)
{
  //LOGN("DATAOUT PHASE");
  SET_MSG_INACTIVE();
  SET_CD_INACTIVE();
  SET_IO_INACTIVE();

#if WRITE_SPEED_OPTIMIZE
  readHandshakeBlock(p, len);
#else
  for(int i = 0; i < len; i++)
    p[i] = readHandshake();
#endif
  if(m_isBusReset) {
    m_phase = PHASE_BUSFREE;
  }
}

/*
 * Data out phase.
 *  Write to SD card while reading len block.
 */
void readDataPhaseSD(uint32_t adds, uint32_t len)
{
#if WRITE_SPEED_OPTIMIZE
  uint32_t bigread = (MAX_BLOCKSIZE / m_sel->m_blocksize);
#endif
  uint32_t i = 0;

  //LOGN("DATAOUT PHASE(SD)");
  uint32_t pos = adds * m_sel->m_blocksize;
  m_sel->m_file.seek(pos);
  SET_MSG_INACTIVE();
  SET_CD_INACTIVE();
  SET_IO_INACTIVE();

  while(i < len) {
#if WRITE_SPEED_OPTIMIZE
    if((len-i) >= bigread) {
      readHandshakeBlock(m_buf, MAX_BLOCKSIZE);
      m_sel->m_file.write(m_buf, MAX_BLOCKSIZE);
      i += bigread;
    } else {
      readHandshakeBlock(m_buf, m_sel->m_blocksize * (len-i));
      m_sel->m_file.write(m_buf, m_sel->m_blocksize * (len-i));
      i = len;
    }
#else
    for(unsigned int j = 0; j <  m_sel->m_blocksize; j++) {
      if(m_isBusReset) {
        return;
      }
      m_buf[j] = readHandshake();
    }
    m_sel->m_file.write(m_buf, m_sel->m_blocksize);
#endif
  }
  m_sel->m_file.flush();
}

/*
 * Data out phase.
 *  Write to SD card while reading len block.
 */
void readDataPhaseRaw(uint32_t adds, uint32_t len)
{
#if WRITE_SPEED_OPTIMIZE_RAW
  uint32_t bigread = (MAX_BLOCKSIZE / m_sel->m_blocksize);
#endif
  uint32_t i = 0;

  //LOGN("DATAOUT PHASE(RAW)");
  uint32_t pos = ((adds * m_sel->m_blocksize) / 512) + m_sel->m_firstSector;

  SET_MSG_INACTIVE();
  SET_CD_INACTIVE();
  SET_IO_INACTIVE();

  while(i < len) {
#if WRITE_SPEED_OPTIMIZE_RAW
    if((len-i) >= bigread) {
      readHandshakeBlock(m_buf, MAX_BLOCKSIZE);
      sd.card()->writeSectors(pos, m_buf, (MAX_BLOCKSIZE / 512));
      i += bigread;
      pos += (MAX_BLOCKSIZE / 512);
    } else {
      readHandshakeBlock(m_buf, m_sel->m_blocksize * (len-i));
      sd.card()->writeSectors(pos, m_buf, ((m_sel->m_blocksize * (len-i)) / 512));
      i = len;
    }
#else
    for(unsigned int j = 0; j <  m_sel->m_blocksize; j++) {
      if(m_isBusReset) {
        return;
      }
      m_buf[j] = readHandshake();
    }
    sd.card()->writeSectors(pos, m_buf, (m_sel->m_blocksize / 512));
    i++;
    pos += (m_sel->m_blocksize / 512);
#endif
  }
  m_sel->m_file.flush();
}

/*
 * Data out phase.
 *  Verify SD card while reading len block.
 */
void verifyDataPhaseSD(uint32_t adds, uint32_t len)
{
#if WRITE_SPEED_OPTIMIZE
  uint32_t bigread = (MAX_BLOCKSIZE / m_sel->m_blocksize);
#endif
  uint32_t i = 0;

  //LOGN("DATAOUT PHASE(SD)");
  uint32_t pos = adds * m_sel->m_blocksize;
  m_sel->m_file.seek(pos);
  SET_MSG_INACTIVE();
  SET_CD_INACTIVE();
  SET_IO_INACTIVE();

  while(i < len) {
#if WRITE_SPEED_OPTIMIZE
    if((len-i) >= bigread) {
      readHandshakeBlock(m_buf, MAX_BLOCKSIZE);
      //m_sel->m_file.verify(m_buf, MAX_BLOCKSIZE);
      i += bigread;
    } else {
      readHandshakeBlock(m_buf, m_sel->m_blocksize * (len-i));
      //m_sel->m_file.verify(m_buf, m_sel->m_blocksize * (len-i));
      i = len;
    }
#else
    for(unsigned int j = 0; j <  m_sel->m_blocksize; j++) {
      if(m_isBusReset) {
        return;
      }
      m_buf[j] = readHandshake();
    }
    //m_sel->m_file.verify(m_buf, m_sel->m_blocksize);
#endif
  }
}


/*
 * Data out phase.
 *  Verify SD card while reading len block.
 */
void verifyDataPhaseRaw(uint32_t adds, uint32_t len)
{
#if WRITE_SPEED_OPTIMIZE_RAW
  uint32_t bigread = (MAX_BLOCKSIZE / m_sel->m_blocksize);
#endif
  uint32_t i = 0;

  //LOGN("DATAOUT PHASE(RAW)");
  //uint32_t pos = ((adds * m_sel->m_blocksize) / 512) + m_sel->m_firstSector;
  SET_MSG_INACTIVE();
  SET_CD_INACTIVE();
  SET_IO_INACTIVE();

  while(i < len) {
#if WRITE_SPEED_OPTIMIZE_RAW
    if((len-i) >= bigread) {
      readHandshakeBlock(m_buf, MAX_BLOCKSIZE);
      i += bigread;
    } else {
      readHandshakeBlock(m_buf, m_sel->m_blocksize * (len-i));
      i = len;
    }
#else
    for(unsigned int j = 0; j <  m_sel->m_blocksize; j++) {
      if(m_isBusReset) {
        return;
      }
      m_buf[j] = readHandshake();
    }
#endif
  }
}

/*  
 *  GreenSCSI
 *  Copyright (c) 2021  David Kuder
 *  Copyright (c) 2021  Eric Helgeson, Androda
 *  
 *  This file is free software: you may copy, redistribute and/or modify it  
 *  under the terms of the GNU General Public License as published by the  
 *  Free Software Foundation, either version 2 of the License, or (at your  
 *  option) any later version.  
 *  
 *  This file is distributed in the hope that it will be useful, but  
 *  WITHOUT ANY WARRANTY; without even the implied warranty of  
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU  
 *  General Public License for more details.  
 *  
 *  You should have received a copy of the GNU General Public License  
 *  along with this program.  If not, see https://github.com/erichelgeson/bluescsi.  
 *  
 * This file incorporates work covered by the following copyright and  
 * permission notice:  
 *  
 *     Copyright (c) 2019 komatsu   
 *  
 *     Permission to use, copy, modify, and/or distribute this software  
 *     for any purpose with or without fee is hereby granted, provided  
 *     that the above copyright notice and this permission notice appear  
 *     in all copies.  
 *  
 *     THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL  
 *     WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED  
 *     WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE  
 *     AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR  
 *     CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS  
 *     OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT,  
 *     NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN  
 *     CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.  
 */

#include <Arduino.h> // For Platform.IO
#include <SPI.h>
#include <SdFat.h>
#include "sdios.h"

#define DEBUG            0      // 0:No debug information output
                                // 1: Debug information output to USB Serial
                                // 2: Debug information output to LOG.txt (slow)

#define SCSI_SELECT      0      // 0 for STANDARD
                                // 1 for SHARP X1turbo
                                // 2 for NEC PC98
#define READ_SPEED_OPTIMIZE  1 // Faster reads
#define WRITE_SPEED_OPTIMIZE 1 // Speeding up writes
#define USE_DB2ID_TABLE      1 // Use table to get ID from SEL-DB

// SCSI config
#define NUM_SCSIID  7          // Maximum number of supported SCSI-IDs (The minimum is 0)
#define NUM_SCSILUN 7          // Maximum number of LUNs supported     (The minimum is 0)
#define READ_PARITY_CHECK 0    // Perform read parity check (unverified)

// HDD format
#define MAX_BLOCKSIZE 4096     // Maximum BLOCK size

// SDFAT
SdFs sd;

#if DEBUG == 1
#define LOG(XX)     Serial.print(XX)
#define LOGHEX(XX)  Serial.print(XX, HEX)
#define LOGN(XX)    Serial.println(XX)
#define LOGHEXN(XX) Serial.println(XX, HEX)
#elif DEBUG == 2
#define LOG(XX)     LOG_FILE.println(XX); LOG_FILE.sync();
#define LOGHEX(XX)  LOG_FILE.println(XX, HEX); LOG_FILE.sync();
#define LOGN(XX)    LOG_FILE.println(XX); LOG_FILE.sync();
#define LOGHEXN(XX) LOG_FILE.println(XX, HEX); LOG_FILE.sync();
#else
#define LOG(XX)     //Serial.print(XX)
#define LOGHEX(XX)  //Serial.print(XX, HEX)
#define LOGN(XX)    //Serial.println(XX)
#define LOGHEXN(XX) //Serial.println(XX, HEX)
#endif

#define DB0       0     // SCSI:DB0
#define DB1       1     // SCSI:DB1
#define DB2       29    // SCSI:DB2
#define DB3       30    // SCSI:DB3
#define DB4       43    // SCSI:DB4
#define DB5       46    // SCSI:DB5
#define DB6       44    // SCSI:DB6
#define DB7       45    // SCSI:DB7
#define DB8       32     // SCSI:DBP
#define ATN       12    // SCSI:ATN
#define BSY       10    // SCSI:BSY
#define ACK       9     // SCSI:ACK
#define RST       8     // SCSI:RST
#define MSG       7     // SCSI:MSG
#define SEL       6     // SCSI:SEL
#define CD        5     // SCSI:C/D
#define REQ       4     // SCSI:REQ
#define IO        3     // SCSI:I/O

#define SD_CS     BUILTIN_SDCARD      // SDCARD:CS
#define LED       13     // LED

// LED control
#define LED_ON()       digitalWrite(LED, HIGH);
#define LED_OFF()      digitalWrite(LED, LOW);

// SCSI output pin control: opendrain active LOW (direct pin drive)
inline void SCSI_OUT(int VPIN, boolean ACTIVE) {
  if(ACTIVE) {
    pinMode(VPIN, OUTPUT_OPENDRAIN);
    digitalWrite(VPIN, LOW);
  } else {
    pinMode(VPIN, INPUT);
  }
};

// SCSI input pin check (invert)
inline boolean SCSI_IN(int VPIN) {
  pinMode(VPIN, INPUT_PULLUP);
  return !digitalRead(VPIN);
};

#define SCSI_DB_MASK 0x00ff0800

// Put DB and DP in output mode
inline void SCSI_DB_OUTPUT() {
  GPIOB_PDOR = SCSI_DB_MASK;
  GPIOB_PDDR = 0x00000000;
}

// Put DB and DP in input mode
inline void SCSI_DB_INPUT()  {
  GPIOB_PDDR = 0x00000000;
}

// Turn on the output only for BSY
#define SCSI_BSY_ACTIVE()      { SCSI_OUT(BSY, true); }
// BSY,REQ,MSG,CD,IO Turn on the output (no change required for OD)
#define SCSI_TARGET_ACTIVE()   { }
// BSY,REQ,MSG,CD,IO Turn off output, BSY is the last input
#define SCSI_TARGET_INACTIVE() { SCSI_OUT(REQ, false); SCSI_OUT(MSG, false); SCSI_OUT(CD, false); SCSI_OUT(IO, false); SCSI_OUT(BSY, false); pinMode(BSY, INPUT_PULLUP); }

// HDDiamge file
#define HDIMG_ID_POS  2                 // Position to embed ID number
#define HDIMG_LUN_POS 3                 // Position to embed LUN numbers
#define HDIMG_BLK_POS 5                 // Position to embed block size numbers
#define MAX_FILE_PATH 32                // Maximum file name length

// HDD image
typedef struct hddimg_struct
{
    FsFile      m_file;                 // File object
    uint64_t    m_fileSize;             // File size
    size_t      m_blocksize;            // SCSI BLOCK size
}HDDIMG;
HDDIMG  img[NUM_SCSIID][NUM_SCSILUN]; // Maximum number

uint8_t       m_senseKey = 0;         // Sense key
volatile bool m_isBusReset = false;   // Bus reset

byte          scsi_id_mask;           // Mask list of responding SCSI IDs
byte          m_id;                   // Currently responding SCSI-ID
byte          m_lun;                  // Logical unit number currently responding
byte          m_sts;                  // Status byte
byte          m_msg;                  // Message bytes
HDDIMG       *m_img;                  // HDD image for current SCSI-ID, LUN
byte          m_buf[MAX_BLOCKSIZE+1]; // General purpose buffer + overrun fetch
int           m_msc;
byte          m_msb[256];             // Command storage bytes

/*
 *  Data byte to GPIOB register setting value and parity table
*/


// Parity bit generation
#define PTY(V)   ((1^((V)^((V)>>1)^((V)>>2)^((V)>>3)^((V)>>4)^((V)>>5)^((V)>>6)^((V)>>7)))&1)

// Set DBP
#define DBP(D)    (((uint32_t)(D)<<16)|(PTY(D)<<11))

#define DBP8(D)   DBP(D),DBP(D+1),DBP(D+2),DBP(D+3),DBP(D+4),DBP(D+5),DBP(D+6),DBP(D+7)
#define DBP32(D)  DBP8(D),DBP8(D+8),DBP8(D+16),DBP8(D+24)

// BSRR register control value that simultaneously performs DB set, DP set, and REQ = H (inactrive)
static const uint32_t db_bsrr[256]={
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
static const byte db2scsiid[256]={
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

// Log File
#define VERSION "1.1-SNAPSHOT"
#define LOG_FILENAME "LOG.txt"
FsFile LOG_FILE;

// SCSI Drive Vendor information
byte SCSI_INFO_BUF[36] = {
  0x00, //device type
  0x00, //RMB = 0
  0x01, //ISO, ECMA, ANSI version
  0x01, //Response data format
  35 - 4, //Additional data length
  0, 0, //Reserve
  0x00, //Support function
  'Q', 'U', 'A', 'N', 'T', 'U', 'M', ' ', // vendor 8
  'F', 'I', 'R', 'E', 'B', 'A', 'L', 'L', '1', ' ', ' ',' ', ' ', ' ', ' ', ' ', // product 16
  '1', '.', '0', ' ' // version 4
};

void onFalseInit(void);
void noSDCardFound(void);
void onBusReset(void);
void initFileLog(void);
void finalizeFileLog(void);

/*
 * IO read.
 */
inline byte readIO(void)
{
  // Port input data register
  uint32_t ret = GPIOB_PDIR;
  byte bret = (byte)((~ret)>>16);
#if READ_PARITY_CHECK
  if((db_bsrr[bret]^ret)&0x800)
    m_sts |= 0x01; // parity error
#endif

  return bret;
}

// If config file exists, read the first three lines and copy the contents.
// File must be well formed or you will get junk in the SCSI Vendor fields.
void readSCSIDeviceConfig() {
  FsFile config_file = sd.open("scsi-config.txt", O_RDONLY);
  if (!config_file.isOpen()) {
    return;
  }
  char vendor[9];
  memset(vendor, 0, sizeof(vendor));
  config_file.readBytes(vendor, sizeof(vendor));
  LOG_FILE.print("SCSI VENDOR: ");
  LOG_FILE.println(vendor);
  memcpy(&(SCSI_INFO_BUF[8]), vendor, 8);

  char product[17];
  memset(product, 0, sizeof(product));
  config_file.readBytes(product, sizeof(product));
  LOG_FILE.print("SCSI PRODUCT: ");
  LOG_FILE.println(product);
  memcpy(&(SCSI_INFO_BUF[16]), product, 16);

  char version[5];
  memset(version, 0, sizeof(version));
  config_file.readBytes(version, sizeof(version));
  LOG_FILE.print("SCSI VERSION: ");
  LOG_FILE.println(version);
  memcpy(&(SCSI_INFO_BUF[32]), version, 4);
  config_file.close();
}

// read SD information and print to logfile
void readSDCardInfo()
{
  cid_t sd_cid;

  if(sd.card()->readCID(&sd_cid))
  {
    LOG_FILE.print("Sd MID:");
    LOG_FILE.print(sd_cid.mid, 16);
    LOG_FILE.print(" OID:");
    LOG_FILE.print(sd_cid.oid[0]);
    LOG_FILE.println(sd_cid.oid[1]);

    LOG_FILE.print("Sd Name:");
    LOG_FILE.print(sd_cid.pnm[0]);
    LOG_FILE.print(sd_cid.pnm[1]);
    LOG_FILE.print(sd_cid.pnm[2]);
    LOG_FILE.print(sd_cid.pnm[3]);
    LOG_FILE.println(sd_cid.pnm[4]);

    LOG_FILE.print("Sd Date:");
    LOG_FILE.print(sd_cid.mdt_month);
    LOG_FILE.print("/20"); // CID year is 2000 + high/low
    LOG_FILE.print(sd_cid.mdt_year_high);
    LOG_FILE.println(sd_cid.mdt_year_low);

    LOG_FILE.print("Sd Serial:");
    LOG_FILE.println(sd_cid.psn);
    LOG_FILE.sync();
  }
}

/*
 * Open HDD image file
 */

bool hddimageOpen(HDDIMG *h,const char *image_name,int id,int lun,int blocksize)
{
  h->m_fileSize = 0;
  h->m_blocksize = blocksize;
  h->m_file = sd.open(image_name, O_RDWR);
  if(h->m_file.isOpen())
  {
    h->m_fileSize = h->m_file.size();
    LOG_FILE.print("Imagefile: ");
    LOG_FILE.print(image_name);
    if(h->m_fileSize>0)
    {
      // check blocksize dummy file
      LOG_FILE.print(" / ");
      LOG_FILE.print(h->m_fileSize);
      LOG_FILE.print("bytes / ");
      LOG_FILE.print(h->m_fileSize / 1024);
      LOG_FILE.print("KiB / ");
      LOG_FILE.print(h->m_fileSize / 1024 / 1024);
      LOG_FILE.println("MiB");
      return true; // File opened
    }
    else
    {
      h->m_file.close();
      h->m_fileSize = h->m_blocksize = 0; // no file
      LOG_FILE.println("FileSizeError");
    }
  }
  return false;
}

/*
 * Initialization.
 *  Initialize the bus and set the PIN orientation
 */
void setup()
{
  // PIN initialization
  pinMode(LED, OUTPUT);
  LED_OFF();

  //GPIO(SCSI BUS)Initialization

  // Input port
  pinMode(ATN, INPUT_PULLUP);
  pinMode(BSY, INPUT_PULLUP);
  pinMode(ACK, INPUT_PULLUP);
  pinMode(RST, INPUT_PULLUP);
  pinMode(SEL, INPUT_PULLUP);
  // Output port
  pinMode(MSG, OUTPUT_OPENDRAIN);
  pinMode(CD,  OUTPUT_OPENDRAIN);
  pinMode(REQ, OUTPUT_OPENDRAIN);
  pinMode(IO,  OUTPUT_OPENDRAIN);

  pinMode(DB0, OUTPUT_OPENDRAIN);
  pinMode(DB1, OUTPUT_OPENDRAIN);
  pinMode(DB2, OUTPUT_OPENDRAIN);
  pinMode(DB3, OUTPUT_OPENDRAIN);
  pinMode(DB4, OUTPUT_OPENDRAIN);
  pinMode(DB5, OUTPUT_OPENDRAIN);
  pinMode(DB6, OUTPUT_OPENDRAIN);
  pinMode(DB7, OUTPUT_OPENDRAIN);
  pinMode(DB8, OUTPUT_OPENDRAIN);

  // DB and DP are input modes
  SCSI_DB_INPUT();
  
  // Turn off the output port
  SCSI_TARGET_INACTIVE();

  LED_ON();

  if(!sd.begin(SdioConfig(FIFO_SDIO))) {
#if DEBUG > 0
    Serial.println("SD initialization failed!");
#endif
    noSDCardFound();
  }
  initFileLog();
  readSCSIDeviceConfig();
  readSDCardInfo();

  //Sector data overrun byte setting
  m_buf[MAX_BLOCKSIZE] = 0xff; // DB0 all off,DBP off
  //HD image file open
  scsi_id_mask = 0x00;

  // Iterate over the root path in the SD card looking for candidate image files.
  SdFile root;
  root.open("/");
  SdFile file;
  bool imageReady;
  int usedDefaultId = 0;
  while (1) {
    if (!file.openNext(&root, O_READ)) break;
    char name[MAX_FILE_PATH+1];
    if(!file.isDir()) {
      file.getName(name, MAX_FILE_PATH+1);
      file.close();
      String file_name = String(name);
      file_name.toLowerCase();
      if(file_name.startsWith("hd")) {
        // Defaults for Hard Disks
        int id  = 1; // 0 and 3 are common in Macs for physical HD and CD, so avoid them.
        int lun = 0;
        int blk = 512;

        // Positionally read in and coerase the chars to integers.
        // We only require the minimum and read in the next if provided.
        int file_name_length = file_name.length();
        if(file_name_length > 2) { // HD[N]
          int tmp_id = name[HDIMG_ID_POS] - '0';

          if(tmp_id > -1 && tmp_id < 8) {
            id = tmp_id; // If valid id, set it, else use default
          } else {
            usedDefaultId++;
          }
        }
        if(file_name_length > 3) { // HD0[N]
          int tmp_lun = name[HDIMG_LUN_POS] - '0';

          if(tmp_lun > -1 && tmp_lun < 2) {
            lun = tmp_lun; // If valid id, set it, else use default
          }
        }
        int blk1 = 0, blk2 = 0, blk3 = 0, blk4 = 0;
        if(file_name_length > 8) { // HD00_[111]
          blk1 = name[HDIMG_BLK_POS] - '0';
          blk2 = name[HDIMG_BLK_POS+1] - '0';
          blk3 = name[HDIMG_BLK_POS+2] - '0';
          if(file_name_length > 9) // HD00_NNN[1]
            blk4 = name[HDIMG_BLK_POS+3] - '0';
        }
        if(blk1 == 2 && blk2 == 5 && blk3 == 6) {
          blk = 256;
        } else if(blk1 == 1 && blk2 == 0 && blk3 == 2 && blk4 == 4) {
          blk = 1024;
        } else if(blk1 == 2 && blk2 == 0 && blk3 == 4 && blk4 == 8) {
          blk  = 2048;
        }

        if(id < NUM_SCSIID && lun < NUM_SCSILUN) {
          HDDIMG *h = &img[id][lun];
          imageReady = hddimageOpen(h,name,id,lun,blk);
          if(imageReady) { // Marked as a responsive ID
            scsi_id_mask |= 1<<id;
          }
        } else {
          LOG_FILE.print("Bad LUN or SCSI id for image: ");
          LOG_FILE.println(name);
          LOG_FILE.sync();
        }
      } else {
          LOG_FILE.print("Not an image: ");
          LOG_FILE.println(name);
          LOG_FILE.sync();
      }
    }
  }
  if(usedDefaultId > 0) {
    LOG_FILE.println("!! More than one image did not specify a SCSI ID. Last file will be used at ID 1. !!");
    LOG_FILE.sync();
  }
  root.close();

  // Error if there are 0 image files
  if(scsi_id_mask==0) {
    LOG_FILE.println("ERROR: No valid images found!");
    onFalseInit();
  }

  finalizeFileLog();
  LED_OFF();

  //Occurs when the RST pin state changes from HIGH to LOW
  attachInterrupt(RST, onBusReset, FALLING);
}

/*
 * Setup initialization logfile
 */
void initFileLog() {
  LOG_FILE = sd.open(LOG_FILENAME, O_WRONLY | O_CREAT | O_TRUNC);
  LOG_FILE.println("GreenSCSI <-> SD - https://github.com/dkgrizzly/GreenSCSI");
  LOG_FILE.print("VERSION: ");
  LOG_FILE.println(VERSION);
  LOG_FILE.print("DEBUG:");
  LOG_FILE.print(DEBUG);
  LOG_FILE.print(" SCSI_SELECT:");
  LOG_FILE.print(SCSI_SELECT);
  LOG_FILE.print(" SDFAT_FILE_TYPE:");
  LOG_FILE.println(SDFAT_FILE_TYPE);
  LOG_FILE.print("SdFat version: ");
  LOG_FILE.println(SD_FAT_VERSION_STR);
  LOG_FILE.print("SdFat Max FileName Length: ");
  LOG_FILE.println(MAX_FILE_PATH);
  LOG_FILE.println("Initialized SD Card - lets go!");
  LOG_FILE.sync();
}

/*
 * Finalize initialization logfile
 */
void finalizeFileLog() {
  // View support drive map
  LOG_FILE.print("ID");
  for(int lun=0;lun<NUM_SCSILUN;lun++)
  {
    LOG_FILE.print(":LUN");
    LOG_FILE.print(lun);
  }
  LOG_FILE.println(":");
  //
  for(int id=0;id<NUM_SCSIID;id++)
  {
    LOG_FILE.print(" ");
    LOG_FILE.print(id);
    for(int lun=0;lun<NUM_SCSILUN;lun++)
    {
      HDDIMG *h = &img[id][lun];
      if( (lun<NUM_SCSILUN) && (h->m_file))
      {
        LOG_FILE.print((h->m_blocksize<1000) ? ": " : ":");
        LOG_FILE.print(h->m_blocksize);
      }
      else      
        LOG_FILE.print(":----");
    }
    LOG_FILE.println(":");
  }
  LOG_FILE.println("Finished initialization of SCSI Devices - Entering main loop.");
  LOG_FILE.sync();
  LOG_FILE.close();
}

/*
 * Initialization failed, blink 3x fast
 */
void onFalseInit(void)
{
  LOG_FILE.sync();
  while(true) {
    for(int i = 0; i < 3; i++) {
      LED_ON();
      delay(250);
      LED_OFF();
      delay(250);
    }
    delay(3000);
  }
}

/*
 * No SC Card found, blink 5x fast
 */
void noSDCardFound(void)
{
  while(true) {
    for(int i = 0; i < 5; i++) {
      LED_ON();
      delay(250);
      LED_OFF();
      delay(250);
    }
    delay(3000);
  }
}

/*
 * Bus reset interrupt.
 */
void onBusReset(void)
{
#if SCSI_SELECT == 1
  // SASI I / F for X1 turbo has RST pulse write cycle +2 clock ==
  // I can't filter because it only activates about 1.25us
  {{
#else
  if(!digitalRead(RST))) {
    delayMicroseconds(20);
    if(!digitalRead(RST))) {
#endif  
  // BUS FREE is done in the main process
      // Should I enter DB and DBP once?
      SCSI_DB_INPUT();

      LOGN("BusReset!");
      m_isBusReset = true;
    }
  }
}

/*
 * Read by handshake.
 */
inline byte readHandshake(void)
{
  SCSI_OUT(REQ,true)
  //SCSI_DB_INPUT();
  while( ! SCSI_IN(ACK)) { if(m_isBusReset) return 0; }
  byte r = readIO();
  SCSI_OUT(REQ, false);
  while( SCSI_IN(ACK)) { if(m_isBusReset) return 0; }
  return r;  
}

/*
 * Write with a handshake.
 */
inline void writeHandshake(byte d)
{
  SCSI_DB_OUTPUT(); // (180ns)
  GPIOB_PDDR = db_bsrr[d];
  GPIOB_PDOR = db_bsrr[d] ^ SCSI_DB_MASK; // setup DB,DBP (160ns)
  SCSI_OUT(REQ, false);  // ACK.Fall to DB output delay 100ns(MAX)  (DTC-510B)

  SCSI_OUT(REQ, false); // setup wait (30ns)
  SCSI_OUT(REQ, false); // setup wait (30ns)
  SCSI_OUT(REQ, false); // setup wait (30ns)
  SCSI_OUT(REQ, true);   // (30ns)
  //while(!SCSI_IN(ACK)) { if(m_isBusReset){ SCSI_DB_INPUT() return; }}
  while(!m_isBusReset && !SCSI_IN(ACK));
  // ACK.Fall to REQ.Raise delay 500ns(typ.) (DTC-510B)
  GPIOB_PDDR = DBP(0xff);
  GPIOB_PDOR = DBP(0xff) ^ SCSI_DB_MASK;  // DB=0xFF
  SCSI_OUT(REQ, false);
  // REQ.Raise to DB hold time 0ns
  SCSI_DB_INPUT(); // (150ns)
  while( SCSI_IN(ACK)) { if(m_isBusReset) return; }
}

/*
 * Data in phase.
 *  Send len bytes of data array p.
 */
void writeDataPhase(int len, const byte* p)
{
  LOGN("DATAIN PHASE");
  SCSI_OUT(MSG, false); //  gpio_write(MSG, low);
  SCSI_OUT(CD , false); //  gpio_write(CD, low);
  SCSI_OUT(IO , true); //  gpio_write(IO, high);
  for (int i = 0; i < len; i++) {
    if(m_isBusReset) {
      return;
    }
    writeHandshake(p[i]);
  }
}

/* 
 * Data in phase.
 *  Send len block while reading from SD card.
 */
void writeDataPhaseSD(uint32_t adds, uint32_t len)
{
  LOGN("DATAIN PHASE(SD)");
  uint32_t pos = adds * m_img->m_blocksize;
  m_img->m_file.seek(pos);

  SCSI_OUT(MSG, false); //  gpio_write(MSG, low);
  SCSI_OUT(CD , false); //  gpio_write(CD, low);
  SCSI_OUT(IO , true); //  gpio_write(IO, high);

  for(uint32_t i = 0; i < len; i++) {
      // Asynchronous reads will make it faster ...
    m_img->m_file.read(m_buf, m_img->m_blocksize);

#if READ_SPEED_OPTIMIZE

#define REQ_ON() SCSI_OUT(REQ, true);
#define FETCH_SRC()   (src_byte = *srcptr++)
#define FETCH_BSRR_DB() (bsrr_val = bsrr_tbl[src_byte])
#define REQ_OFF_DB_SET(BSRR_VAL) { SCSI_OUT(REQ, false); GPIOB_PDDR = BSRR_VAL; GPIOB_PDOR = BSRR_VAL ^ SCSI_DB_MASK; }
#define WAIT_ACK_ACTIVE()   while(!m_isBusReset && !SCSI_IN(ACK))
#define WAIT_ACK_INACTIVE() do{ if(m_isBusReset) return; }while(SCSI_IN(ACK)) 

    SCSI_DB_OUTPUT();
    register byte *srcptr= m_buf;                 // Source buffer
    register byte *endptr= m_buf +  m_img->m_blocksize; // End pointer

    /*register*/ byte src_byte;                       // Send data bytes
    register const uint32_t *bsrr_tbl = db_bsrr;  // Table to convert to BSRR
    register uint32_t bsrr_val;                   // BSRR value to output (DB, DBP, REQ = ACTIVE)

    // prefetch & 1st out
    FETCH_SRC();
    FETCH_BSRR_DB();
    REQ_OFF_DB_SET(bsrr_val);
    // DB.set to REQ.F setup 100ns max (DTC-510B)
    // Maybe there should be some weight here
    //　WAIT_ACK_INACTIVE();
    do{
      // 0
      REQ_ON();
      FETCH_SRC();
      FETCH_BSRR_DB();
      WAIT_ACK_ACTIVE();
      // ACK.F  to REQ.R       500ns typ. (DTC-510B)
      REQ_OFF_DB_SET(bsrr_val);
      WAIT_ACK_INACTIVE();
      // 1
      REQ_ON();
      FETCH_SRC();
      FETCH_BSRR_DB();
      WAIT_ACK_ACTIVE();
      REQ_OFF_DB_SET(bsrr_val);
      WAIT_ACK_INACTIVE();
      // 2
      REQ_ON();
      FETCH_SRC();
      FETCH_BSRR_DB();
      WAIT_ACK_ACTIVE();
      REQ_OFF_DB_SET(bsrr_val);
      WAIT_ACK_INACTIVE();
      // 3
      REQ_ON();
      FETCH_SRC();
      FETCH_BSRR_DB();
      WAIT_ACK_ACTIVE();
      REQ_OFF_DB_SET(bsrr_val);
      WAIT_ACK_INACTIVE();
      // 4
      REQ_ON();
      FETCH_SRC();
      FETCH_BSRR_DB();
      WAIT_ACK_ACTIVE();
      REQ_OFF_DB_SET(bsrr_val);
      WAIT_ACK_INACTIVE();
      // 5
      REQ_ON();
      FETCH_SRC();
      FETCH_BSRR_DB();
      WAIT_ACK_ACTIVE();
      REQ_OFF_DB_SET(bsrr_val);
      WAIT_ACK_INACTIVE();
      // 6
      REQ_ON();
      FETCH_SRC();
      FETCH_BSRR_DB();
      WAIT_ACK_ACTIVE();
      REQ_OFF_DB_SET(bsrr_val);
      WAIT_ACK_INACTIVE();
      // 7
      REQ_ON();
      FETCH_SRC();
      FETCH_BSRR_DB();
      WAIT_ACK_ACTIVE();
      REQ_OFF_DB_SET(bsrr_val);
      WAIT_ACK_INACTIVE();
    }while(srcptr < endptr);
    SCSI_DB_INPUT();
#else
    for(unsigned int j = 0; j < m_img->m_blocksize; j++) {
      if(m_isBusReset) {
        return;
      }
      writeHandshake(m_buf[j]);
    }
#endif
  }
}

/*
 * Data out phase.
 *  len block read
 */
void readDataPhase(int len, byte* p)
{
  LOGN("DATAOUT PHASE");
  SCSI_OUT(MSG,false); //  gpio_write(MSG, low);
  SCSI_OUT(CD ,false); //  gpio_write(CD, low);
  SCSI_OUT(IO ,false); //  gpio_write(IO, low);
  for(uint32_t i = 0; i < len; i++)
    p[i] = readHandshake();
}

/*
 * Data out phase.
 *  Write to SD card while reading len block.
 */
void readDataPhaseSD(uint32_t adds, uint32_t len)
{
  LOGN("DATAOUT PHASE(SD)");
  uint32_t pos = adds * m_img->m_blocksize;
  m_img->m_file.seek(pos);
  SCSI_OUT(MSG,false); //  gpio_write(MSG, low);
  SCSI_OUT(CD ,false); //  gpio_write(CD, low);
  SCSI_OUT(IO ,false); //  gpio_write(IO, low);
  for(uint32_t i = 0; i < len; i++) {
#if WRITE_SPEED_OPTIMIZE
  register byte *dstptr= m_buf;
    register byte *endptr= m_buf + m_img->m_blocksize;

    for(dstptr=m_buf;dstptr<endptr;dstptr+=8) {
      dstptr[0] = readHandshake();
      dstptr[1] = readHandshake();
      dstptr[2] = readHandshake();
      dstptr[3] = readHandshake();
      dstptr[4] = readHandshake();
      dstptr[5] = readHandshake();
      dstptr[6] = readHandshake();
      dstptr[7] = readHandshake();
      if(m_isBusReset) {
        return;
      }
    }
#else
    for(int j = 0; j <  m_img->m_blocksize; j++) {
      if(m_isBusReset) {
        return;
      }
      m_buf[j] = readHandshake();
    }
#endif
    m_img->m_file.write(m_buf, m_img->m_blocksize);
  }
  m_img->m_file.flush();
}

/*
 * INQUIRY command processing.
 */
#if SCSI_SELECT == 2
byte onInquiryCommand(byte len)
{
  byte buf[36] = {
    0x00, //Device type
    0x00, //RMB = 0
    0x01, //ISO,ECMA,ANSI version
    0x01, //Response data format
    35 - 4, //Additional data length
    0, 0, //Reserve
    0x00, //Support function
    'N', 'E', 'C', 'I', 'T', 'S', 'U', ' ',
    'A', 'r', 'd', 'S', 'C', 'S', 'i', 'n', 'o', ' ', ' ',' ', ' ', ' ', ' ', ' ',
    '0', '0', '1', '0',
  };
  if(!m_img) buf[0] = 0x7f;
  writeDataPhase(len < 36 ? len : 36, buf);
  return 0x00;
}
#else
byte onInquiryCommand(byte len)
{
  byte buf[36];
  memcpy(buf, SCSI_INFO_BUF, 36);

  if(!m_img) buf[0] = 0x7f;

  writeDataPhase(len < 36 ? len : 36, buf);
  return 0x00;
}
#endif

/*
 * REQUEST SENSE command processing.
 */
void onRequestSenseCommand(byte len)
{
  byte buf[18] = {
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
    buf[2] = m_senseKey;
    m_senseKey = 0;
  }
  writeDataPhase(len < 18 ? len : 18, buf);  
}

/*
 * READ CAPACITY command processing.
 */
byte onReadCapacityCommand(byte pmi)
{
  if(!m_img) return 0x02; // Image file absent
  
  uint32_t bl = m_img->m_blocksize;
  uint32_t bc = m_img->m_fileSize / bl;
  uint8_t buf[8] = {
    bc >> 24, bc >> 16, bc >> 8, bc,
    bl >> 24, bl >> 16, bl >> 8, bl    
  };
  writeDataPhase(8, buf);
  return 0x00;
}

/*
 * READ6 / 10 Command processing.
 */
byte onReadCommand(uint32_t adds, uint32_t len)
{
  LOGN("-R");
  LOGHEXN(adds);
  LOGHEXN(len);

  if(!m_img) return 0x02; // Image file absent
  
  LED_ON();
  writeDataPhaseSD(adds, len);
  LED_OFF();
  return 0x00; //sts
}

/*
 * WRITE6 / 10 Command processing.
 */
byte onWriteCommand(uint32_t adds, uint32_t len)
{
  LOGN("-W");
  LOGHEXN(adds);
  LOGHEXN(len);
  
  if(!m_img) return 0x02; // Image file absent
  
  LED_ON();
  readDataPhaseSD(adds, len);
  LED_OFF();
  return 0; //sts
}

/*
 * MODE SENSE command processing.
 */
#if SCSI_SELECT == 2
byte onModeSenseCommand(byte dbd, int cmd2, uint32_t len)
{
  if(!m_img) return 0x02; // Image file absent

  int pageCode = cmd2 & 0x3F;

  // Assuming sector size 512, number of sectors 25, number of heads 8 as default settings
  int size = m_img->m_fileSize;
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
  memset(m_buf, 0, sizeof(m_buf)); 
  int a = 4;
  if(dbd == 0) {
    uint32_t bl = m_img->m_blocksize;
    uint32_t bc = m_img->m_fileSize / bl;
    byte c[8] = {
      0,// Density code
      bc >> 16, bc >> 8, bc,
      0, //Reserve
      bl >> 16, bl >> 8, bl
    };
    memcpy(&m_buf[4], c, 8);
    a += 8;
    m_buf[3] = 0x08;
  }
  switch(pageCode) {
  case 0x3F:
  {
    m_buf[a + 0] = 0x01;
    m_buf[a + 1] = 0x06;
    a += 8;
  }
  case 0x03:  // drive parameters
  {
    m_buf[a + 0] = 0x80 | 0x03; // Page code
    m_buf[a + 1] = 0x16; // Page length
    m_buf[a + 2] = (byte)(heads >> 8);// number of sectors / track
    m_buf[a + 3] = (byte)(heads);// number of sectors / track
    m_buf[a + 10] = (byte)(sectors >> 8);// number of sectors / track
    m_buf[a + 11] = (byte)(sectors);// number of sectors / track
    int size = 1 << disksize;
    m_buf[a + 12] = (byte)(size >> 8);// number of sectors / track
    m_buf[a + 13] = (byte)(size);// number of sectors / track
    a += 24;
    if(pageCode != 0x3F) {
      break;
    }
  }
  case 0x04:  // drive parameters
  {
      LOGN("AddDrive");
      m_buf[a + 0] = 0x04; // Page code
      m_buf[a + 1] = 0x12; // Page length
      m_buf[a + 2] = (cylinders >> 16);// Cylinder length
      m_buf[a + 3] = (cylinders >> 8);
      m_buf[a + 4] = cylinders;
      m_buf[a + 5] = heads;   // Number of heads
      a += 20;
    if(pageCode != 0x3F) {
      break;
    }
  }
  default:
    break;
  }
  m_buf[0] = a - 1;
  writeDataPhase(len < a ? len : a, m_buf);
  return 0x00;
}
#else
byte onModeSenseCommand(byte dbd, int cmd2, uint32_t len)
{
  if(!m_img) return 0x02; // No image file

  memset(m_buf, 0, sizeof(m_buf));
  int pageCode = cmd2 & 0x3F;
  int a = 4;
  if(dbd == 0) {
    uint32_t bl =  m_img->m_blocksize;
    uint32_t bc = m_img->m_fileSize / bl;

    byte c[8] = {
      0,//Density code
      bc >> 16, bc >> 8, bc,
      0, //Reserve
      bl >> 16, bl >> 8, bl    
    };
    memcpy(&m_buf[4], c, 8);
    a += 8;
    m_buf[3] = 0x08;
  }
  switch(pageCode) {
  case 0x3F:
  case 0x03:  //Drive parameters
    m_buf[a + 0] = 0x03; //Page code
    m_buf[a + 1] = 0x16; // Page length
    m_buf[a + 11] = 0x3F;//Number of sectors / track
    a += 24;
    if(pageCode != 0x3F) {
      break;
    }
  case 0x04:  //Drive parameters
    {
      uint32_t bc = m_img->m_fileSize / m_img->m_file;
      m_buf[a + 0] = 0x04; //Page code
      m_buf[a + 1] = 0x16; // Page length
      m_buf[a + 2] = bc >> 16;// Cylinder length
      m_buf[a + 3] = bc >> 8;
      m_buf[a + 4] = bc;
      m_buf[a + 5] = 1;   //Number of heads
      a += 24;
    }
    if(pageCode != 0x3F) {
      break;
    }
  default:
    break;
  }
  m_buf[0] = a - 1;
  writeDataPhase(len < a ? len : a, m_buf);
  return 0x00;
}
#endif
    
#if SCSI_SELECT == 1
/*
 * dtc510b_setDriveparameter
 */
#define PACKED  __attribute__((packed))
typedef struct PACKED dtc500_cmd_c2_param_struct
{
  uint8_t StepPlusWidth;        // Default is 13.6usec (11)
  uint8_t StepPeriod;         // Default is  3  msec.(60)
  uint8_t StepMode;         // Default is  Bufferd (0)
  uint8_t MaximumHeadAdress;      // Default is 4 heads (3)
  uint8_t HighCylinderAddressByte;  // Default set to 0   (0)
  uint8_t LowCylinderAddressByte;   // Default is 153 cylinders (152)
  uint8_t ReduceWrietCurrent;     // Default is above Cylinder 128 (127)
  uint8_t DriveType_SeekCompleteOption;// (0)
  uint8_t Reserved8;          // (0)
  uint8_t Reserved9;          // (0)
} DTC510_CMD_C2_PARAM;

static void logStrHex(const char *msg,uint32_t num)
{
    LOG(msg);
    LOGHEXN(num);
}

static byte dtc510b_setDriveparameter(void)
{
  DTC510_CMD_C2_PARAM DriveParameter;
  uint16_t maxCylinder;
  uint16_t numLAD;
  //uint32_t stepPulseUsec;
  int StepPeriodMsec;

  // receive paramter
  writeDataPhase(sizeof(DriveParameter),(byte *)(&DriveParameter));
 
  maxCylinder =
    (((uint16_t)DriveParameter.HighCylinderAddressByte)<<8) |
    (DriveParameter.LowCylinderAddressByte);
  numLAD = maxCylinder * (DriveParameter.MaximumHeadAdress+1);
  //stepPulseUsec  = calcStepPulseUsec(DriveParameter.StepPlusWidth);
  StepPeriodMsec = DriveParameter.StepPeriod*50;
  logStrHex (" StepPlusWidth      : ",DriveParameter.StepPlusWidth);
  logStrHex (" StepPeriod         : ",DriveParameter.StepPeriod   );
  logStrHex (" StepMode           : ",DriveParameter.StepMode     );
  logStrHex (" MaximumHeadAdress  : ",DriveParameter.MaximumHeadAdress);
  logStrHex (" CylinderAddress    : ",maxCylinder);
  logStrHex (" ReduceWrietCurrent : ",DriveParameter.ReduceWrietCurrent);
  logStrHex (" DriveType/SeekCompleteOption : ",DriveParameter.DriveType_SeekCompleteOption);
  logStrHex (" Maximum LAD        : ",numLAD-1);
  return  0; // error result
}
#endif

/*
 * MsgIn2.
 */
void MsgIn2(int msg)
{
  LOGN("MsgIn2");
  SCSI_OUT(MSG, true); //  gpio_write(MSG, high);
  SCSI_OUT(CD , true); //  gpio_write(CD, high);
  SCSI_OUT(IO , true); //  gpio_write(IO, high);
  writeHandshake(msg);
}

/*
 * MsgOut2.
 */
void MsgOut2()
{
  LOGN("MsgOut2");
  SCSI_OUT(MSG, true); //  gpio_write(MSG, high);
  SCSI_OUT(CD , true); //  gpio_write(CD, high);
  SCSI_OUT(IO , false); //  gpio_write(IO, low);
  m_msb[m_msc] = readHandshake();
  m_msc++;
  m_msc %= 256;
}

/*
 * Main loop.
 */
void loop() 
{
  //int msg = 0;
  m_msg = 0;
  m_lun = 0xff;

  // Wait until RST = H, BSY = H, SEL = L
  do {} while( SCSI_IN(BSY) || !SCSI_IN(SEL) || SCSI_IN(RST));

  // BSY+ SEL-
  // If the ID to respond is not driven, wait for the next
  //byte db = readIO();
  //byte scsiid = db & scsi_id_mask;
  byte scsiid = readIO() & scsi_id_mask;
  if((scsiid) == 0) {
    return;
  }
  LOGN("Selection");
  m_isBusReset = false;
  // Set BSY to-when selected
  SCSI_BSY_ACTIVE();     // Turn only BSY output ON, ACTIVE

  // Ask for a TARGET-ID to respond
#if USE_DB2ID_TABLE
  m_id = db2scsiid[scsiid];
  //if(m_id==0xff) return;
#else
  for(m_id=7;m_id>=0;m_id--)
    if(scsiid & (1<<m_id)) break;
  //if(m_id<0) return;
#endif

  // Wait until SEL becomes false
  while(!digitalRead(SEL) && digitalRead(BSY)) {
    if(m_isBusReset) {
      goto BusFree;
    }
  }
  SCSI_TARGET_ACTIVE()  // (BSY), REQ, MSG, CD, IO output turned on
  //  
  if(!digitalRead(ATN)) {
    bool syncenable = false;
    int syncperiod = 50;
    int syncoffset = 0;
    int loopWait = 0;
    m_msc = 0;
    memset(m_msb, 0x00, sizeof(m_msb));
    while(!digitalRead(ATN) && loopWait < 255) {
      MsgOut2();
      loopWait++;
    }
    for(int i = 0; i < m_msc; i++) {
      // ABORT
      if (m_msb[i] == 0x06) {
        goto BusFree;
      }
      // BUS DEVICE RESET
      if (m_msb[i] == 0x0C) {
        syncoffset = 0;
        goto BusFree;
      }
      // IDENTIFY
      if (m_msb[i] >= 0x80) {
        m_lun = m_msb[i] & 0x07;
      }
      // Extended message
      if (m_msb[i] == 0x01) {
        // Check only when synchronous transfer is possible
        if (!syncenable || m_msb[i + 2] != 0x01) {
          MsgIn2(0x07);
          break;
        }
        // Transfer period factor(50 x 4 = Limited to 200ns)
        syncperiod = m_msb[i + 3];
        if (syncperiod > 50) {
          syncperiod = 50;
        }
        // REQ/ACK offset(Limited to 16)
        syncoffset = m_msb[i + 4];
        if (syncoffset > 16) {
          syncoffset = 16;
        }
        // STDR response message generation
        MsgIn2(0x01);
        MsgIn2(0x03);
        MsgIn2(0x01);
        MsgIn2(syncperiod);
        MsgIn2(syncoffset);
        break;
      }
    }
  }

  LOG("Command:");
  SCSI_OUT(MSG, false); // gpio_write(MSG, low);
  SCSI_OUT(CD , true); // gpio_write(CD, high);
  SCSI_OUT(IO , false); // gpio_write(IO, low);
  
  int len;
  byte cmd[12];
  cmd[0] = readHandshake(); if(m_isBusReset) goto BusFree;
  LOGHEX(cmd[0]);
  // Command length selection, reception
  static const int cmd_class_len[8]={6,10,10,6,6,12,6,6};
  len = cmd_class_len[cmd[0] >> 5];
  cmd[1] = readHandshake(); LOG(":");LOGHEX(cmd[1]); if(m_isBusReset) goto BusFree;
  cmd[2] = readHandshake(); LOG(":");LOGHEX(cmd[2]); if(m_isBusReset) goto BusFree;
  cmd[3] = readHandshake(); LOG(":");LOGHEX(cmd[3]); if(m_isBusReset) goto BusFree;
  cmd[4] = readHandshake(); LOG(":");LOGHEX(cmd[4]); if(m_isBusReset) goto BusFree;
  cmd[5] = readHandshake(); LOG(":");LOGHEX(cmd[5]); if(m_isBusReset) goto BusFree;
  // Receive the remaining commands
  for(int i = 6; i < len; i++ ) {
    cmd[i] = readHandshake();
    LOG(":");
    LOGHEX(cmd[i]);
    if(m_isBusReset) goto BusFree;
  }
  // LUN confirmation
  m_sts = cmd[1]&0xe0;      // Preset LUN in status byte
  if(m_lun == 0xff) m_lun = m_sts>>5;
  // HDD Image selection
  m_img = (HDDIMG *)0; // None
  if( (m_lun <= NUM_SCSILUN) )
  {
    m_img = &(img[m_id][m_lun]); // There is an image
    if(!(m_img->m_file.isOpen()))
      m_img = (HDDIMG *)0;       // Image absent
  }
  //LOGHEX(((uint32_t)m_img));
  
  LOG(":ID ");
  LOG(m_id);
  LOG(":LUN ");
  LOG(m_lun);

  LOGN("");

  if(!m_img) {
    switch(cmd[0]) {
    case 0x03:
      LOGN("[RequestSense]");
      onRequestSenseCommand(cmd[4]);
      break;
    case 0x12:
      LOGN("[Inquiry]");
      m_sts |= onInquiryCommand(cmd[4]);
      break;
    default:
      LOGN("[Bad LUN]");
      m_sts |= 0x02;
      break;
    }
  } else {
    switch(cmd[0]) {
    case 0x00:
      LOGN("[Test Unit]");
      break;
    case 0x01:
      LOGN("[Rezero Unit]");
      break;
    case 0x03:
      LOGN("[RequestSense]");
      onRequestSenseCommand(cmd[4]);
      break;
    case 0x04:
      LOGN("[FormatUnit]");
      break;
    case 0x06:
      LOGN("[FormatUnit]");
      break;
    case 0x07:
      LOGN("[ReassignBlocks]");
      break;
    case 0x08:
      LOGN("[Read6]");
      m_sts |= onReadCommand((((uint32_t)cmd[1] & 0x1F) << 16) | ((uint32_t)cmd[2] << 8) | cmd[3], (cmd[4] == 0) ? 0x100 : cmd[4]);
      break;
    case 0x0A:
      LOGN("[Write6]");
      m_sts |= onWriteCommand((((uint32_t)cmd[1] & 0x1F) << 16) | ((uint32_t)cmd[2] << 8) | cmd[3], (cmd[4] == 0) ? 0x100 : cmd[4]);
      break;
    case 0x0B:
      LOGN("[Seek6]");
      break;
    case 0x12:
      LOGN("[Inquiry]");
      m_sts |= onInquiryCommand(cmd[4]);
      break;
    case 0x1A:
      LOGN("[ModeSense6]");
      m_sts |= onModeSenseCommand(cmd[1]&0x80, cmd[2], cmd[4]);
      break;
    case 0x1B:
      LOGN("[StartStopUnit]");
      break;
    case 0x1E:
      LOGN("[PreAllowMed.Removal]");
      break;
    case 0x25:
      LOGN("[ReadCapacity]");
      m_sts |= onReadCapacityCommand(cmd[8]);
      break;
    case 0x28:
      LOGN("[Read10]");
      m_sts |= onReadCommand(((uint32_t)cmd[2] << 24) | ((uint32_t)cmd[3] << 16) | ((uint32_t)cmd[4] << 8) | cmd[5], ((uint32_t)cmd[7] << 8) | cmd[8]);
      break;
    case 0x2A:
      LOGN("[Write10]");
      m_sts |= onWriteCommand(((uint32_t)cmd[2] << 24) | ((uint32_t)cmd[3] << 16) | ((uint32_t)cmd[4] << 8) | cmd[5], ((uint32_t)cmd[7] << 8) | cmd[8]);
      break;
    case 0x2B:
      LOGN("[Seek10]");
      break;
    case 0x5A:
      LOGN("[ModeSense10]");
      onModeSenseCommand(cmd[1] & 0x80, cmd[2], ((uint32_t)cmd[7] << 8) | cmd[8]);
      break;
#if SCSI_SELECT == 1
    case 0xc2:
      LOGN("[DTC510B setDriveParameter]");
      m_sts |= dtc510b_setDriveparameter();
      break;
#endif    
    default:
      LOGN("[*Unknown]");
      m_sts |= 0x02;
      m_senseKey = 5;
      break;
    }
  }
  if(m_isBusReset) {
     goto BusFree;
  }

  LOGN("Sts");
  SCSI_OUT(MSG, false); // gpio_write(MSG, low);
  SCSI_OUT(CD , true); // gpio_write(CD, high);
  SCSI_OUT(IO , true); // gpio_write(IO, high);
  writeHandshake(m_sts);
  if(m_isBusReset) {
     goto BusFree;
  }

  LOGN("MsgIn");
  SCSI_OUT(MSG, true); // gpio_write(MSG, high);
  SCSI_OUT(CD , true); // gpio_write(CD, high);
  SCSI_OUT(IO , true); // gpio_write(IO, high);
  writeHandshake(m_msg);

BusFree:
  LOGN("BusFree");
  m_isBusReset = false;
  //SCSI_OUT(REQ,false) // gpio_write(REQ, low);
  //SCSI_OUT(MSG,false) // gpio_write(MSG, low);
  //SCSI_OUT(CD ,false) // gpio_write(CD, low);
  //SCSI_OUT(IO ,false) // gpio_write(IO, low);
  //SCSI_OUT(BSY,false)
  SCSI_TARGET_INACTIVE(); // Turn off BSY, REQ, MSG, CD, IO output
}

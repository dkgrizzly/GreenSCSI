/*  
 *  GreenSCSI
 *  Copyright (c) 2021  David Kuder
 *  
 *  Based on BlueSCSI
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
#include "scsi_defs.h"

#define DEBUG            0      // 0:No debug information output (Faster by 2-3x+)
                                // 1: Debug information output to USB Serial
                                // 2: Debug information output to LOG.txt (slow)

#define SCSI_SELECT      0      // 0 for STANDARD
                                // 1 for SHARP X1turbo
                                // 2 for NEC PC98
#define READ_INTERRUPT       1  // Should be Fastest (until we get Synch working)
#define READ_SPEED_OPTIMIZE  0  // Faster reads
#define WRITE_INTERRUPT      1  // Should be Fastest (until we get Synch working)
#define WRITE_SPEED_OPTIMIZE 0  // Faster writes
#define USE_DB2ID_TABLE      1  // Use table to get ID from SEL-DB

// SCSI config
#define NUM_SCSIID  8          // Maximum number of supported SCSI-IDs (The minimum is 0)
#define NUM_SCSILUN 8          // Maximum number of LUNs supported     (The minimum is 0)
#define READ_PARITY_CHECK 0    // Perform read parity check (unverified)

// HDD format
#define MAX_BLOCKSIZE 4096     // Maximum BLOCK size

// SDFAT
SdFs sd;

#if DEBUG == 1
#define LOG(XX)     Serial.print(XX)
#define LOGHEX2(XX)  Serial.printf("%02x", XX)
#define LOGHEX4(XX)  Serial.printf("%04x", XX)
#define LOGHEX6(XX)  Serial.printf("%06x", XX)
#define LOGN(XX)    Serial.println(XX)
#define LOGHEX2N(XX) Serial.printf("%02x\r\n", XX)
#define LOGHEX4N(XX) Serial.printf("%04x\r\n", XX)
#define LOGHEX6N(XX) Serial.printf("%06x\r\n", XX)
#elif DEBUG == 2
#define LOG(XX)      LOG_FILE.print(XX); LOG_FILE.sync();
#define LOGHEX2(XX)  LOG_FILE.printf("%02x", XX); LOG_FILE.sync();
#define LOGHEX4(XX)  LOG_FILE.printf("%04x", XX); LOG_FILE.sync();
#define LOGHEX6(XX)  LOG_FILE.printf("%06x", XX); LOG_FILE.sync();
#define LOGN(XX)     LOG_FILE.println(XX); LOG_FILE.sync();
#define LOGHEX2N(XX) LOG_FILE.printf("%02x\r\n", XX); LOG_FILE.sync();
#define LOGHEX4N(XX) LOG_FILE.printf("%04x\r\n", XX); LOG_FILE.sync();
#define LOGHEX6N(XX) LOG_FILE.printf("%06x\r\n", XX); LOG_FILE.sync();
#else
#define LOG(XX)      //Serial.print(XX)
#define LOGHEX2(XX)  //Serial.printf("%02x", XX)
#define LOGHEX4(XX)  //Serial.printf("%04x", XX)
#define LOGHEX6(XX)  //Serial.printf("%06x", XX)
#define LOGN(XX)     //Serial.println(XX)
#define LOGHEX2N(XX) //Serial.printf("%02x\r\n", XX)
#define LOGHEX4N(XX) //Serial.printf("%04x\r\n", XX)
#define LOGHEX6N(XX) //Serial.printf("%06x\r\n", XX)
#endif

#define DB0       0     // SCSI:DB0
#define DB1       1     // SCSI:DB1
#define DB2       29    // SCSI:DB2
#define DB3       30    // SCSI:DB3
#define DB4       43    // SCSI:DB4
#define DB5       46    // SCSI:DB5
#define DB6       44    // SCSI:DB6
#define DB7       45    // SCSI:DB7
#define DB8       32    // SCSI:DBP
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

//#define SET_REQ_ACTIVE() SCSI_OUT(REQ, true)
//#define SET_REQ_INACTIVE() SCSI_OUT(REQ, false)
#define SET_REQ_ACTIVE() { GPIOA_PCOR = (1 << 13); }
#define SET_REQ_INACTIVE() { GPIOA_PSOR = (1 << 13); }
#define SET_MSG_ACTIVE() { GPIOD_PCOR = (1 << 2); }
#define SET_MSG_INACTIVE() { GPIOD_PSOR = (1 << 2); }
#define SET_CD_ACTIVE() { GPIOD_PCOR = (1 << 7); }
#define SET_CD_INACTIVE() { GPIOD_PSOR = (1 << 7); }
#define SET_IO_ACTIVE() { GPIOA_PCOR = (1 << 12); }
#define SET_IO_INACTIVE() { GPIOA_PSOR = (1 << 12); }

//#define GET_ACK() SCSI_IN(ACK)
#define GET_ACK() (!(GPIOC_PDIR & (1 << 3)))
#define GET_ATN() (!(GPIOC_PDIR & (1 << 7)))
#define GET_BSY() (!(GPIOC_PDIR & (1 << 4)))
#define GET_RST() (!(GPIOD_PDIR & (1 << 3)))
#define GET_SEL() (!(GPIOD_PDIR & (1 << 4)))

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
#define SCSI_TARGET_INACTIVE() { SCSI_DB_INPUT(); SET_REQ_INACTIVE(); SET_MSG_INACTIVE(); SET_CD_INACTIVE(); SET_IO_INACTIVE(); SCSI_OUT(BSY, false); pinMode(BSY, INPUT_PULLUP); }

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
} HDDIMG;
HDDIMG  img[NUM_SCSIID][NUM_SCSILUN]; // Maximum number

volatile bool m_isBusReset = false;   // Bus reset

uint8_t       scsi_id_mask;           // Mask list of responding SCSI IDs
uint8_t       m_id;                   // Currently responding SCSI-ID
uint8_t       m_lun;                  // Logical unit number currently responding
uint8_t       m_sts;                  // Status uint8_t
uint8_t       m_msg;                  // Message uint8_ts
HDDIMG       *m_img;                  // HDD image for current SCSI-ID, LUN
uint8_t       m_buf[MAX_BLOCKSIZE+1]; // General purpose buffer + overrun fetch
int           m_msc;
uint8_t       m_msb[256];             // Command storage uint8_ts
uint8_t       m_phase;
uint8_t       m_cmdlen;
uint8_t       m_cmd[12];
uint8_t       m_inquiryresponse[NUM_SCSIID][96];
uint8_t       m_responsebuffer[256];

typedef struct m_sense_s {
  uint8_t     m_key;
  uint8_t     m_code;
  uint8_t     m_key_specific[4];
} msense_t;

msense_t      m_sense[NUM_SCSIID][NUM_SCSILUN];

typedef void (*CommandHandler_t)();
CommandHandler_t m_handler[NUM_SCSIID][256];
CommandHandler_t m_badlunhandler[256];

typedef enum {
  PHASE_BUSFREE = 0,
  PHASE_SELECTION,
  PHASE_MESSAGEOUT,
  PHASE_COMMAND,
  PHASE_STATUSIN,
  PHASE_MESSAGEIN,
} phase_t;

/*
 *  Data uint8_t to GPIOB register setting value and parity table
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
static const uint8_t db2scsiid[256]={
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
#if DEBUG == 2
#define LOG_FILENAME "LOG.txt"
FsFile LOG_FILE;
#endif

// SCSI Drive Vendor information
static uint8_t SCSI_DISK_INQUIRY_RESPONSE[96] = {
  0x00, //device type
  0x00, //RMB = 0
  0x01, //ISO, ECMA, ANSI version
  0x01, //Response data format
  35 - 4, //Additional data length
  0, 0, //Reserve
  0x00, //Support function
  'Q', 'U', 'A', 'N', 'T', 'U', 'M', ' ', // vendor 8
  'F', 'I', 'R', 'E', 'B', 'A', 'L', 'L', '1', ' ', ' ',' ', ' ', ' ', ' ', ' ', // product 16
  '1', '.', '0', ' ', // version 4
  0
};

#if 0
static uint8_t SCSI_NEC_INQUIRY_RESPONSE[96] = {
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
  0
};
#endif

static uint8_t SCSI_CDROM_INQUIRY_RESPONSE[96] = {
  0x05, //device type
  0x80, //RMB = 0
  0x05, //ISO, ECMA, ANSI version
  0x02, //Response data format
  46 - 4, //Additional data length
  0, 0, //Reserve
  0x00, //Support function
  'S', 'O', 'N', 'Y', ' ', ' ', ' ', ' ', // vendor 8
  'C', 'D', '-', 'R', 'O', 'M', ' ', 'C', 'D', 'U', '-','8', '0', '0', '3', ' ', // product 16
  '1', '.', '7', 'w', // version 4
  // Release Number (1 Byte)
  0x20,
  // Revision Date (10 Bytes)
  '2','0','1','6','/','1','2','/','2','6',
  0
};

void onFalseInit(void);
void noSDCardFound(void);
void onBusReset(void);
void initFileLog(void);
void finalizeFileLog(void);

/*
 * IO read.
 */
inline uint8_t readIO(void)
{
  // Port input data register
  uint32_t ret = GPIOB_PDIR;
  uint8_t bret = (uint8_t)((~ret)>>16);
#if READ_PARITY_CHECK
  if((db_bsrr[bret] ^ ret) & 0x0800)
    m_sts |= 0x01; // parity error
#endif

  return bret;
}

// If config file exists, read the first three lines and copy the contents.
// File must be well formed or you will get junk in the SCSI Vendor fields.
void ConfigureDiskIdentity(int id, const char *image_name) {
  char configname[MAX_FILE_PATH+1];
  memcpy(configname, image_name, MAX_FILE_PATH+1);
  char *psuffix = strstr(configname, ".img");
  if(psuffix) {
    strcpy(psuffix, ".cfg");
  } else {
    sprintf(configname, "hd%d0_512.cfg", id);
  }
  memcpy(m_inquiryresponse[id], SCSI_DISK_INQUIRY_RESPONSE, sizeof(SCSI_DISK_INQUIRY_RESPONSE));

  FsFile config_file = sd.open(configname, O_RDONLY);
  if (!config_file.isOpen()) {
    return;
  }
  char vendor[9];
  memset(vendor, 0, sizeof(vendor));
  config_file.readBytes(vendor, sizeof(vendor));
  LOGN("SCSI VENDOR: ");
  LOGN(vendor);
  memcpy(&(m_inquiryresponse[id][8]), vendor, 8);

  char product[17];
  memset(product, 0, sizeof(product));
  config_file.readBytes(product, sizeof(product));
  LOGN("SCSI PRODUCT: ");
  LOGN(product);
  memcpy(&(m_inquiryresponse[id][16]), product, 16);

  char version[5];
  memset(version, 0, sizeof(version));
  config_file.readBytes(version, sizeof(version));
  LOGN("SCSI VERSION: ");
  LOGN(version);
  memcpy(&(m_inquiryresponse[id][32]), version, 4);
  config_file.close();
}

// If config file exists, read the first three lines and copy the contents.
// File must be well formed or you will get junk in the SCSI Vendor fields.
void ConfigureOpticalIdentity(int id, const char *image_name) {
  char configname[MAX_FILE_PATH+1];
  memcpy(configname, image_name, MAX_FILE_PATH+1);
  char *psuffix = strstr(configname, ".img");
  if(psuffix) {
    strcpy(psuffix, ".cfg");
  } else {
    sprintf(configname, "cd%d0_512.cfg", id);
  }
  memcpy(m_inquiryresponse[id], SCSI_CDROM_INQUIRY_RESPONSE, sizeof(SCSI_CDROM_INQUIRY_RESPONSE));

  FsFile config_file = sd.open(configname, O_RDONLY);
  if (!config_file.isOpen()) {
    return;
  }
  char vendor[9];
  memset(vendor, 0, sizeof(vendor));
  config_file.readBytes(vendor, sizeof(vendor));
  LOGN("SCSI VENDOR: ");
  LOGN(vendor);
  memcpy(&(m_inquiryresponse[id][8]), vendor, 8);

  char product[17];
  memset(product, 0, sizeof(product));
  config_file.readBytes(product, sizeof(product));
  LOGN("SCSI PRODUCT: ");
  LOGN(product);
  memcpy(&(m_inquiryresponse[id][16]), product, 16);

  char version[5];
  memset(version, 0, sizeof(version));
  config_file.readBytes(version, sizeof(version));
  LOGN("SCSI VERSION: ");
  LOGN(version);
  memcpy(&(m_inquiryresponse[id][32]), version, 4);
  config_file.close();
}

// read SD information and print to logfile
void readSDCardInfo()
{
  cid_t sd_cid;

  if(sd.card()->readCID(&sd_cid))
  {
    LOG("Sd MID:");
    LOGHEX2(sd_cid.mid);
    LOG(" OID:");
    LOG(sd_cid.oid[0]);
    LOGN(sd_cid.oid[1]);

    LOG("Sd Name:");
    LOG(sd_cid.pnm[0]);
    LOG(sd_cid.pnm[1]);
    LOG(sd_cid.pnm[2]);
    LOG(sd_cid.pnm[3]);
    LOGN(sd_cid.pnm[4]);

    LOG("Sd Date:");
    LOG(sd_cid.mdt_month);
    LOG("/20"); // CID year is 2000 + high/low
    LOG(sd_cid.mdt_year_high);
    LOGN(sd_cid.mdt_year_low);

    LOG("Sd Serial:");
    LOGN(sd_cid.psn);
  }
}

/*
 * Open HDD image file
 */

boolean OpenDiskImage(void *hp,const char *image_name,int id,int lun,int blocksize)
{
  HDDIMG *h = (HDDIMG *)hp;

  h->m_fileSize = 0;
  h->m_blocksize = blocksize;
  h->m_file = sd.open(image_name, O_RDWR);
  if(h->m_file.isOpen())
  {
    h->m_fileSize = h->m_file.size();
    LOG(" Imagefile: ");
    LOG(image_name);
    if(h->m_fileSize>0)
    {
      // check blocksize dummy file
      LOG(" / ");
      LOG(h->m_fileSize / h->m_blocksize);
      LOG(" sectors / ");
      LOG(h->m_fileSize / 1024);
      LOG(" KiB / ");
      LOG(h->m_fileSize / 1024 / 1024);
      LOGN(" MiB");
      return true; // File opened
    }
    else
    {
      h->m_file.close();
      h->m_fileSize = h->m_blocksize = 0; // no file
      LOGN(" FileSizeError");
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
  Serial.begin(115200);

  delay(1500);
  
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

  //Occurs when the RST pin state changes from HIGH to LOW
  attachInterrupt(RST, onBusReset, FALLING);
  attachInterrupt(ACK, ackTransferISR, CHANGE);

  LED_ON();

  if(!sd.begin(SdioConfig(FIFO_SDIO))) {
#if DEBUG > 0
    Serial.println("SD initialization failed!");
#endif
    noSDCardFound();
  }
  initFileLog();
  readSDCardInfo();
  ConfigureBadLunHandlers();

  //Sector data overrun uint8_t setting
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
      if((file_name.startsWith("hd") || file_name.startsWith("cd")) && file_name.endsWith(".img")) {
        // Defaults for Hard Disks
        int id  = 1; // 0 and 3 are common in Macs for physical HD and CD, so avoid them.
        int lun = 0;
        int blk = 512;

        // Positionally read in and coerase the chars to integers.
        // We only require the minimum and read in the next if provided.
        int file_name_length = file_name.length();
        if(file_name_length > 2) { // HD[N]
          int tmp_id = file_name[HDIMG_ID_POS] - '0';

          if(tmp_id > -1 && tmp_id < NUM_SCSIID) {
            id = tmp_id; // If valid id, set it, else use default
          } else {
            usedDefaultId++;
          }
        }
        if(file_name_length > 3) { // HD0[N]
          int tmp_lun = file_name[HDIMG_LUN_POS] - '0';

          if(tmp_lun > -1 && tmp_lun < NUM_SCSILUN) {
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

          // Configure Handlers and Inquiry Response for all luns
          if(file_name.startsWith("hd")) {
            LOG("HD at ID ");
            LOG(id);
            LOG(":LUN ");
            LOG(lun);
            if(lun == 0) {
              ConfigureDiskIdentity(id, name);
              ConfigureDiskHandlers(id);
            }
          }
          if(file_name.startsWith("cd")) {
            LOG("CD at ID ");
            LOG(id);
            LOG(":LUN ");
            LOG(lun);
            if(lun == 0) {
              ConfigureOpticalIdentity(id, name);
              ConfigureOpticalHandlers(id);
            }
          }
          imageReady = OpenDiskImage(h, name, id, lun, blk);
          if(imageReady) { // Marked as a responsive ID
            scsi_id_mask |= 1<<id;
          }
        } else {
          LOG("Bad LUN or SCSI id for image: ");
          LOGN(name);
        }
      } else {
          LOG("Not an image: ");
          LOGN(name);
      }
    }
  }
  if(usedDefaultId > 0) {
    LOGN("!! More than one image did not specify a SCSI ID. Last file will be used at ID 1. !!");
  }
  root.close();

  // Error if there are 0 image files
  if(scsi_id_mask==0) {
    LOGN("ERROR: No valid images found!");
    onFalseInit();
  }

  finalizeFileLog();
  LED_OFF();
}

/*
 * Setup initialization logfile
 */
void initFileLog() {
#if DEBUG == 2
  LOG_FILE = sd.open(LOG_FILENAME, O_WRONLY | O_CREAT | O_TRUNC);
  LOGN("GreenSCSI <-> SD - https://github.com/dkgrizzly/GreenSCSI");
  LOG("VERSION: ");
  LOGN(VERSION);
  LOG("DEBUG:");
  LOG(DEBUG);
  LOG(" SCSI_SELECT:");
  LOG(SCSI_SELECT);
  LOG(" SDFAT_FILE_TYPE:");
  LOGN(SDFAT_FILE_TYPE);
  LOG("SdFat version: ");
  LOGN(SD_FAT_VERSION_STR);
  LOG("SdFat Max FileName Length: ");
  LOGN(MAX_FILE_PATH);
  LOGN("Initialized SD Card - lets go!");
#endif
}

/*
 * Finalize initialization logfile
 */
void finalizeFileLog() {
  // View support drive map
  LOG("ID");
  for(int lun=0;lun<NUM_SCSILUN;lun++)
  {
    LOG(":LUN");
    LOG(lun);
  }
  LOGN(":");
  //
  for(int id=0;id<NUM_SCSIID;id++)
  {
    LOG(" ");
    LOG(id);
    for(int lun=0;lun<NUM_SCSILUN;lun++)
    {
      HDDIMG *h = &img[id][lun];
      if( (lun<NUM_SCSILUN) && (h->m_file))
      {
        LOG((h->m_blocksize<1000) ? ": " : ":");
        LOG(h->m_blocksize);
      }
      else      
        LOG(":----");
    }
    LOGN(":");
  }
  LOGN("Finished initialization of SCSI Devices - Entering main loop.");
#if DEBUG == 2
  LOG_FILE.close();
#endif
}

/*
 * Initialization failed, blink 3x fast
 */
void onFalseInit(void)
{
#if DEBUG == 2
  LOG_FILE.sync();
  LOG_FILE.close();
#endif
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
#if DEBUG == 2
  LOG_FILE.sync();
  LOG_FILE.close();
#endif
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
 * ISR Driven Transfers
 */
#define TRANSFER_IDLE 0
#define TRANSFER_OUT0 1
#define TRANSFER_OUT1 2
#define TRANSFER_IN0  3
#define TRANSFER_IN1  4
uint8_t TransferPhase = TRANSFER_IDLE;
uint16_t TransferBytesLeft = 0;
uint8_t *TransferBuffer;

void ackTransferISR() {
  switch(TransferPhase) {
    case TRANSFER_IDLE:
      return;
    case TRANSFER_OUT0:
      TransferPhase = TRANSFER_OUT1;
      GPIOB_PDDR = db_bsrr[*TransferBuffer];
      GPIOB_PDOR = db_bsrr[*TransferBuffer] ^ SCSI_DB_MASK; // setup DB,DBP (160ns)
      SET_REQ_ACTIVE();   // (30ns)
      return;
    case TRANSFER_OUT1:
      SET_REQ_INACTIVE();
      TransferBuffer++;
      TransferBytesLeft--;
      TransferPhase = (TransferBytesLeft > 0) ? TRANSFER_OUT0 : TRANSFER_IDLE;
      return;
    case TRANSFER_IN0:
      TransferPhase = TRANSFER_IN1;
      SET_REQ_ACTIVE();   // (30ns)
      return;
    case TRANSFER_IN1:
      *TransferBuffer = readIO();
      SET_REQ_INACTIVE();
      TransferBuffer++;
      TransferBytesLeft--;
      TransferPhase = (TransferBytesLeft > 0) ? TRANSFER_IN0 : TRANSFER_IDLE;
      return;
  }
}

inline void readHandshakeBlockInterrupt(uint8_t *d, uint16_t len)
{
  //SCSI_DB_INPUT();
  if(len) {
    TransferBuffer = d;
    TransferBytesLeft = len;
    TransferPhase = TRANSFER_IN1;
    SET_REQ_ACTIVE();
  }
}

inline void writeHandshakeBlockInterrupt(const uint8_t *d, uint16_t len)
{
  //SCSI_DB_INPUT();
  if(len) {
    TransferBuffer = (uint8_t*)d;
    TransferBytesLeft = len;
    TransferPhase = TRANSFER_OUT1;
    GPIOB_PDDR = db_bsrr[*TransferBuffer];
    GPIOB_PDOR = db_bsrr[*TransferBuffer] ^ SCSI_DB_MASK; // setup DB,DBP (160ns)
    SET_REQ_ACTIVE();
  }
}

inline boolean waitInterruptTransferDone() {
  for(;;) {
    if(TransferPhase == TRANSFER_IDLE) {
      return 1;
    }
    if(m_isBusReset) {
      return 0;
    }
    yield();
  }
}

/*
 * Bus reset interrupt.
 */
void onBusReset(void)
{
  TransferPhase = TRANSFER_IDLE;

#if SCSI_SELECT == 1
  // SASI I / F for X1 turbo has RST pulse write cycle +2 clock ==
  // I can't filter because it only activates about 1.25us
  {{
#else
  if(!digitalRead(RST)) {
    delayMicroseconds(20);
    if(!digitalRead(RST)) {
#endif  
      SCSI_DB_INPUT();

      LOGN("BusReset!");
      m_isBusReset = true;
    }
  }
}

/*
 * Read by handshake.
 */
inline uint8_t readHandshake(void)
{
  SET_REQ_ACTIVE();
  //SCSI_DB_INPUT();
  while(!m_isBusReset && !GET_ACK());
  uint8_t r = readIO();
  SET_REQ_INACTIVE();
  while(GET_ACK()) { if(m_isBusReset) return 0; }
  return r;  
}

inline void readHandshakeBlock(uint8_t *d, uint16_t len)
{
  //SCSI_DB_INPUT();
  while(len) {
    SET_REQ_ACTIVE();
    while(!m_isBusReset && !GET_ACK());
    *d++ = readIO();
    len--;
    SET_REQ_INACTIVE();
    while(GET_ACK()) { if(m_isBusReset) return; }
  }
}


/*
 * Write with a handshake.
 */
inline void writeHandshake(uint8_t d)
{
  SCSI_DB_OUTPUT(); // (180ns)
  GPIOB_PDDR = db_bsrr[d];
  GPIOB_PDOR = db_bsrr[d] ^ SCSI_DB_MASK; // setup DB,DBP (160ns)

  // ACK.Fall to DB output delay 100ns(MAX)  (DTC-510B)
  SET_REQ_ACTIVE();   // (30ns)
  while(!m_isBusReset && !GET_ACK());
  // ACK.Fall to REQ.Raise delay 500ns(typ.) (DTC-510B)
  GPIOB_PDDR = DBP(0xff);
  GPIOB_PDOR = DBP(0xff) ^ SCSI_DB_MASK;  // DB=0xFF
  SET_REQ_INACTIVE();

  // REQ.Raise to DB hold time 0ns
  SCSI_DB_INPUT(); // (150ns)
  while( GET_ACK()) { if(m_isBusReset) return; }
}

inline void writeHandshakeBlock(const uint8_t *d, uint16_t len)
{
  SCSI_DB_OUTPUT(); // (180ns)
  while(len) {
    GPIOB_PDDR = db_bsrr[*d];
    GPIOB_PDOR = db_bsrr[*d] ^ SCSI_DB_MASK; // setup DB,DBP (160ns)
    SET_REQ_ACTIVE();   // (30ns)
    while(!m_isBusReset && !GET_ACK());
    d++;
    len--;
    SET_REQ_INACTIVE();
    while( GET_ACK()) { if(m_isBusReset) return; }
  }
  SCSI_DB_INPUT(); // (150ns)
}

/*
 * Data in phase.
 *  Send len uint8_ts of data array p.
 */
void writeDataPhase(int len, const uint8_t* p)
{
  //LOGN("DATAIN PHASE");
  SET_MSG_INACTIVE(); //  gpio_write(MSG, low);
  SCSI_OUT(CD , false); //  gpio_write(CD, low);
  SCSI_OUT(IO , true);  //  gpio_write(IO, high);

#if READ_INTERRUPT
  writeHandshakeBlockInterrupt(p, len);
  waitInterruptTransferDone();
#elif READ_SPEED_OPTIMIZE
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
#if (READ_SPEED_OPTIMIZE or READ_INTERRUPT)
  uint32_t bigread = (MAX_BLOCKSIZE / m_img->m_blocksize);
#endif
  uint32_t i = 0;

  //LOGN("DATAIN PHASE(SD)");
  uint32_t pos = adds * m_img->m_blocksize;
  m_img->m_file.seek(pos);

  SET_MSG_INACTIVE(); //  gpio_write(MSG, low);
  SCSI_OUT(CD , false); //  gpio_write(CD, low);
  SCSI_OUT(IO , true); //  gpio_write(IO, high);

  while(i < len) {
      // Asynchronous reads will make it faster ...
#if READ_INTERRUPT
    if((len-i) >= bigread) {
      m_img->m_file.read(m_buf, MAX_BLOCKSIZE/2);
      waitInterruptTransferDone();
      if(m_isBusReset) {
        m_phase = PHASE_BUSFREE;
        return;
      }
      writeHandshakeBlockInterrupt(m_buf, MAX_BLOCKSIZE/2);
      m_img->m_file.read(m_buf+(MAX_BLOCKSIZE/2), MAX_BLOCKSIZE/2);

      waitInterruptTransferDone();
      if(m_isBusReset) {
        m_phase = PHASE_BUSFREE;
        return;
      }
      writeHandshakeBlockInterrupt(m_buf+(MAX_BLOCKSIZE/2), MAX_BLOCKSIZE/2);
      i += bigread;
    } else {
      waitInterruptTransferDone();
      if(m_isBusReset) {
        m_phase = PHASE_BUSFREE;
        return;
      }
      m_img->m_file.read(m_buf, m_img->m_blocksize * (len-i));
      writeHandshakeBlock(m_buf, m_img->m_blocksize * (len-i));
      waitInterruptTransferDone();
      i = len;
    }
#elif READ_SPEED_OPTIMIZE
    if((len-i) >= bigread) {
      m_img->m_file.read(m_buf, MAX_BLOCKSIZE);
      writeHandshakeBlock(m_buf, MAX_BLOCKSIZE);
      i += bigread;
    } else {
      m_img->m_file.read(m_buf, m_img->m_blocksize * (len-i));
      writeHandshakeBlock(m_buf, m_img->m_blocksize * (len-i));
      i = len;
    }
#else
    m_img->m_file.read(m_buf, m_img->m_blocksize);
    for(unsigned int j = 0; j < m_img->m_blocksize; j++) {
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
  SCSI_OUT(CD , false);
  SCSI_OUT(IO , false);

#if WRITE_INTERRUPT
  readHandshakeBlockInterrupt(p, len);
  waitInterruptTransferDone();
#elif WRITE_SPEED_OPTIMIZE
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
#if (WRITE_INTERRUPT or WRITE_SPEED_OPTIMIZE)
  uint32_t bigread = (MAX_BLOCKSIZE / m_img->m_blocksize);
#endif
  uint32_t i = 0;

  //LOGN("DATAOUT PHASE(SD)");
  uint32_t pos = adds * m_img->m_blocksize;
  m_img->m_file.seek(pos);
  SET_MSG_INACTIVE();
  SCSI_OUT(CD , false);
  SCSI_OUT(IO , false);
#if WRITE_INTERRUPT
  if((len-i) >= bigread) {
    readHandshakeBlockInterrupt(m_buf, (MAX_BLOCKSIZE/2));
  } else {
    readHandshakeBlockInterrupt(m_buf, (len * m_img->m_blocksize));
  }
#endif
  while(i < len) {
#if WRITE_INTERRUPT
    if((len-i) >= bigread) {
      waitInterruptTransferDone();
      readHandshakeBlockInterrupt(m_buf+(MAX_BLOCKSIZE/2), MAX_BLOCKSIZE/2);
      m_img->m_file.write(m_buf, MAX_BLOCKSIZE/2);
      waitInterruptTransferDone();
      i += bigread;
      if((len-i) >= bigread) {
        readHandshakeBlockInterrupt(m_buf, (MAX_BLOCKSIZE/2));
        m_img->m_file.write(m_buf+(MAX_BLOCKSIZE/2), MAX_BLOCKSIZE/2);
      } else if(i < len) {
        m_img->m_file.write(m_buf+(MAX_BLOCKSIZE/2), MAX_BLOCKSIZE/2);
        readHandshakeBlockInterrupt(m_buf, (len * m_img->m_blocksize));
      }
    } else {
      waitInterruptTransferDone();
      m_img->m_file.write(m_buf, m_img->m_blocksize * (len-i));
      i = len;
    }
#elif WRITE_SPEED_OPTIMIZE
    if((len-i) >= bigread) {
      readHandshakeBlock(m_buf, MAX_BLOCKSIZE);
      m_img->m_file.write(m_buf, MAX_BLOCKSIZE);
      i += bigread;
    } else {
      readHandshakeBlock(m_buf, m_img->m_blocksize * (len-i));
      m_img->m_file.write(m_buf, m_img->m_blocksize * (len-i));
      i = len;
    }
#else
    for(unsigned int j = 0; j <  m_img->m_blocksize; j++) {
      if(m_isBusReset) {
        return;
      }
      m_buf[j] = readHandshake();
    }
    m_img->m_file.write(m_buf, m_img->m_blocksize);
#endif
  }
  m_img->m_file.flush();
}

/*
 * MODE SENSE command processing.
 */
#if SCSI_SELECT == 2
uint8_t onModeSenseCommand(uint8_t dbd, int cmd2, uint32_t len)
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
    uint8_t c[8] = {
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
    m_buf[a + 2] = (uint8_t)(heads >> 8);// number of sectors / track
    m_buf[a + 3] = (uint8_t)(heads);// number of sectors / track
    m_buf[a + 10] = (uint8_t)(sectors >> 8);// number of sectors / track
    m_buf[a + 11] = (uint8_t)(sectors);// number of sectors / track
    int size = 1 << disksize;
    m_buf[a + 12] = (uint8_t)(size >> 8);// number of sectors / track
    m_buf[a + 13] = (uint8_t)(size);// number of sectors / track
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
uint8_t onModeSenseCommand(uint8_t dbd, int cmd2, uint32_t len)
{
  if(!m_img) return 0x02; // No image file

  memset(m_buf, 0, sizeof(m_buf));
  int pageCode = cmd2 & 0x3F;
  unsigned int a = 4;
  if(dbd == 0) {
    uint32_t bl =  m_img->m_blocksize;
    uint32_t bc = m_img->m_fileSize / bl;

    uint8_t c[8] = {
      0,//Density code
      (uint8_t)(((uint32_t)(bc >> 16))&0xff), (uint8_t)(((uint32_t)(bc >> 8))&0xff), (uint8_t)((bc)&0xff),
      0, //Reserve
      (uint8_t)(((uint32_t)(bl >> 16))&0xff), (uint8_t)(((uint32_t)(bl >> 8))&0xff), (uint8_t)((bl)&0xff)
    };
    memcpy(&m_buf[4], c, 8);
    a += 8;
    m_buf[3] = 0x08;
  }
  switch(pageCode) {
  case 0x3F:
  case MODEPAGE_RW_ERROR_RECOVERY:
    if(cmd2 & 0x40) {
      m_sense[m_id][m_lun].m_key = ILLEGAL_REQUEST;
      m_sense[m_id][m_lun].m_code = INVALID_FIELD_IN_CDB;    /* "Invalid field in CDB" */
      m_sense[m_id][m_lun].m_key_specific[0] = ERROR_IN_OPCODE;  /* "Error in Byte 2" */
      m_sense[m_id][m_lun].m_key_specific[1] = 0x00;
      m_sense[m_id][m_lun].m_key_specific[2] = 0x02;
      return 0x02;
    }
    m_buf[a++] = 0x01; // PS, page id
    m_buf[a++] = 0x0a; // Page length
    m_buf[a++] = 0x05; // 
    m_buf[a++] = 0x00; // Read Retry Count
    m_buf[a++] = 0x00; // Reserved
    m_buf[a++] = 0x00; // Reserved
    m_buf[a++] = 0x00; // Reserved
    m_buf[a++] = 0x00; // Reserved
    m_buf[a++] = 0x00; // Write Retry Count
    m_buf[a++] = 0x00; // Reserved
    m_buf[a++] = 0x00; // Recovery Time Limit
    m_buf[a++] = 0x00;
    if(pageCode != 0x3F) {
      break;
    }
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
    m_sense[m_id][m_lun].m_key = ILLEGAL_REQUEST;
    m_sense[m_id][m_lun].m_code = INVALID_FIELD_IN_CDB;    /* "Invalid field in CDB" */
    m_sense[m_id][m_lun].m_key_specific[0] = ERROR_IN_OPCODE;  /* "Error in Byte 2" */
    m_sense[m_id][m_lun].m_key_specific[1] = 0x00;
    m_sense[m_id][m_lun].m_key_specific[2] = 0x02;
    return 0x02;
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
  uint8_t HighCylinderAddressuint8_t;  // Default set to 0   (0)
  uint8_t LowCylinderAddressuint8_t;   // Default is 153 cylinders (152)
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

void DTCsetDriveParameterCommandHandler() {
  LOGN("[DTC510B setDriveParameter]");

  DTC510_CMD_C2_PARAM DriveParameter;
  uint16_t maxCylinder;
  uint16_t numLAD;
  //uint32_t stepPulseUsec;
  int StepPeriodMsec;

  // receive paramter
  writeDataPhase(sizeof(DriveParameter),(uint8_t *)(&DriveParameter));
 
  maxCylinder =
    (((uint16_t)DriveParameter.HighCylinderAddressuint8_t)<<8) |
    (DriveParameter.LowCylinderAddressuint8_t);
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
  m_sts = 0;
  m_phase = PHASE_STATUSIN;
}
#endif

/*
 * MsgIn2.
 */
void MsgIn2(int msg)
{
  //LOGN("MsgIn2");
  SET_MSG_ACTIVE();
  SCSI_OUT(CD , true);
  SCSI_OUT(IO , true);
  writeHandshake(msg);
}

/*
 * MsgOut2.
 */
void MsgOut2()
{
  //LOGN("MsgOut2");
  SET_MSG_ACTIVE();
  SCSI_OUT(CD , true);
  SCSI_OUT(IO , false);
  m_msb[m_msc] = readHandshake();
  m_msc++;
  m_msc %= 256;
}

/*
 * Main loop.
 */
void loop() {
  switch(m_phase) {
    default:
    case PHASE_BUSFREE:
      BusFreePhaseHandler();
      break;
    case PHASE_SELECTION:
      SelectionPhaseHandler();
      break;
    case PHASE_MESSAGEOUT:
      MessageOutPhaseHandler();
      break;
    case PHASE_COMMAND:
      CommandPhaseHandler();
      break;
    case PHASE_STATUSIN:
      StatusInPhaseHandler();
      break;
    case PHASE_MESSAGEIN:
      MessageInPhaseHandler();
      break;
  }
}

void SelectionPhaseHandler() {
  m_msg = 0;

  // Wait until RST = H, BSY = H, SEL = L
  do {} while( SCSI_IN(BSY) || !SCSI_IN(SEL) || SCSI_IN(RST));

  // BSY+ SEL-
  // If the ID to respond is not driven, wait for the next
  //uint8_t db = readIO();
  //uint8_t scsiid = db & scsi_id_mask;
  uint8_t scsiid = readIO() & scsi_id_mask;
  if((scsiid) == 0) {
    return;
  }
  LOG("Selection ");
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
  m_lun = 0xff;

  //LOGN("Wait !SEL");
  // Wait until SEL becomes inactive
  while(!digitalRead(SEL) && digitalRead(BSY)) {
    if(m_isBusReset) {
      LOGN("!SEL");
      m_phase = PHASE_BUSFREE;
      return;
    }
  }
  m_phase = PHASE_MESSAGEOUT;
}

void MessageOutPhaseHandler() {
  //LOGN("ACTIVE");
  SCSI_TARGET_ACTIVE();  // (BSY), REQ, MSG, CD, IO output turned on
  //  
  if(!digitalRead(ATN)) {
    //LOGN("ATN");
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
        m_phase = PHASE_BUSFREE;
        return;
      }
      // BUS DEVICE RESET
      if (m_msb[i] == 0x0C) {
        syncoffset = 0;
        m_phase = PHASE_BUSFREE;
        return;
      }
      // IDENTIFY message overrides LUN IDs specified in commands.
      if (m_msb[i] & 0x80) {
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
  m_phase = PHASE_COMMAND;
}

void CommandPhaseHandler() {
  LOG("Command:");
  SET_MSG_INACTIVE();
  SCSI_OUT(CD , true);
  SCSI_OUT(IO , false);
  
  m_cmd[0] = readHandshake();
  if(m_isBusReset) {
    m_phase = PHASE_BUSFREE;
    return;
  }
  LOGHEX2(m_cmd[0]);

  // Command length selection, reception
  static const int cmd_class_len[8]={6,10,10,6,6,12,6,6};
  m_cmdlen = cmd_class_len[m_cmd[0] >> 5];

  // Receive the remaining command bytes
  for(int i = 1; i < m_cmdlen; i++ ) {
    m_cmd[i] = readHandshake();
    LOG(":");
    LOGHEX2(m_cmd[i]);
    if(m_isBusReset) {
      m_phase = PHASE_BUSFREE;
      return;
    }
  }

  // LUN confirmation
  m_sts = m_cmd[1]&0xe0;      // Preset LUN in status byte if not specified by Identify Message
  if(m_lun == 0xff) m_lun = (m_sts>>5);

  // HDD Image selection
  m_img = (HDDIMG *)0; // None
  if( (m_lun < NUM_SCSILUN) )
  {
    m_img = &(img[m_id][m_lun]); // There is an image
    if(!(m_img->m_file.isOpen()))
      m_img = (HDDIMG *)0;       // Image absent
  }
  
  LOG(":ID ");
  LOG(m_id);
  LOG(":LUN ");
  LOG(m_lun);

  LOG(" ");

  if(!m_img) {
    m_badlunhandler[m_cmd[0]]();
  } else {
    m_handler[m_id][m_cmd[0]]();
  }
}

void ConfigureBadLunHandlers() {
  for(int c = 0; c < 256; c++)
    m_badlunhandler[c] = &BadLunCommandHandler;

  m_badlunhandler[0x03] = &RequestSenseCommandHandler;
  m_badlunhandler[0x12] = &InquiryCommandHandler;
}

void StatusInPhaseHandler() {
  //LOGN("Sts");
  SET_MSG_INACTIVE();
  SCSI_OUT(CD , true);
  SCSI_OUT(IO , true);
  writeHandshake(m_sts);
  m_phase = PHASE_MESSAGEIN;
}

void MessageInPhaseHandler() {
  //LOGN("MsgIn");
  SET_MSG_ACTIVE();
  SCSI_OUT(CD , true);
  SCSI_OUT(IO , true);
  writeHandshake(m_msg);
  m_phase = PHASE_BUSFREE;
}

void BusFreePhaseHandler() {
  //LOGN("BusFree");
  m_isBusReset = false;
  SCSI_TARGET_INACTIVE();
  m_phase = PHASE_SELECTION;
}

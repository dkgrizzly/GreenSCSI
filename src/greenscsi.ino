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
#include <LittleFS.h>
#include <SdFat.h>
#include "sdios.h"
#include "config.h"
#include "scsi_defs.h"
#include "cmd.h"

// SDFAT
SdFs sd;
LittleFS_Program lfs;


struct part_s {
  uint8_t boot;
  uint8_t beginCHS[3];
  uint8_t type;
  uint8_t endCHS[3];
  uint32_t firstSector;
  uint32_t totalSectors;
} __attribute__((packed));
typedef struct part_s part_t;
//-----------------------------------------------------------------------------
struct mbr_s {
  uint8_t   bootCode[446];
  part_t part[4];
  uint8_t   signature[2];
} __attribute__((packed));
typedef struct mbr_s mbr_t;

boolean debuglog = 0;

#if DEBUG == 1
#define LOG(XX)      if(debuglog) Serial.print(XX)
#define LOGHEX2(XX)  if(debuglog) Serial.printf("%02x", XX)
#define LOGHEX4(XX)  if(debuglog) Serial.printf("%04x", XX)
#define LOGHEX6(XX)  if(debuglog) Serial.printf("%06x", XX)
#define LOGHEX8(XX)  if(debuglog) Serial.printf("%08x", XX)
#define LOGN(XX)     if(debuglog) Serial.println(XX)
#define LOGHEX2N(XX) if(debuglog) Serial.printf("%02x\r\n", XX)
#define LOGHEX4N(XX) if(debuglog) Serial.printf("%04x\r\n", XX)
#define LOGHEX6N(XX) if(debuglog) Serial.printf("%06x\r\n", XX)
#define LOGHEX8N(XX) if(debuglog) Serial.printf("%08x\r\n", XX)
#elif DEBUG == 2
#define LOG(XX)      LOG_FILE.print(XX); LOG_FILE.sync();
#define LOGHEX2(XX)  LOG_FILE.printf("%02x", XX); LOG_FILE.sync();
#define LOGHEX4(XX)  LOG_FILE.printf("%04x", XX); LOG_FILE.sync();
#define LOGHEX6(XX)  LOG_FILE.printf("%06x", XX); LOG_FILE.sync();
#define LOGHEX8(XX)  LOG_FILE.printf("%08x", XX); LOG_FILE.sync();
#define LOGN(XX)     LOG_FILE.println(XX); LOG_FILE.sync();
#define LOGHEX2N(XX) LOG_FILE.printf("%02x\r\n", XX); LOG_FILE.sync();
#define LOGHEX4N(XX) LOG_FILE.printf("%04x\r\n", XX); LOG_FILE.sync();
#define LOGHEX6N(XX) LOG_FILE.printf("%06x\r\n", XX); LOG_FILE.sync();
#define LOGHEX8N(XX) LOG_FILE.printf("%08x\r\n", XX); LOG_FILE.sync();
#else
#define LOG(XX)      //Serial.print(XX)
#define LOGHEX2(XX)  //Serial.printf("%02x", XX)
#define LOGHEX4(XX)  //Serial.printf("%04x", XX)
#define LOGHEX6(XX)  //Serial.printf("%06x", XX)
#define LOGHEX8(XX)  //Serial.printf("%08x", XX)
#define LOGN(XX)     //Serial.println(XX)
#define LOGHEX2N(XX) //Serial.printf("%02x\r\n", XX)
#define LOGHEX4N(XX) //Serial.printf("%04x\r\n", XX)
#define LOGHEX6N(XX) //Serial.printf("%06x\r\n", XX)
#define LOGHEX8N(XX) //Serial.printf("%08x\r\n", XX)
#endif

#define DB0       0     // SCSI:DB0  Port B Bit 16
#define DB1       1     // SCSI:DB1  Port B Bit 17
#define DB2       29    // SCSI:DB2  Port B Bit 18
#define DB3       30    // SCSI:DB3  Port B Bit 19
#define DB4       43    // SCSI:DB4  Port B Bit 20
#define DB5       46    // SCSI:DB5  Port B Bit 21
#define DB6       44    // SCSI:DB6  Port B Bit 22
#define DB7       45    // SCSI:DB7  Port B Bit 23
#define DB8       32    // SCSI:DBP  Port B Bit 11

#define ATN       12    // SCSI:ATN  Port C Bit 7
#define BSY       10    // SCSI:BSY  Port C Bit 4
#define ACK       9     // SCSI:ACK  Port C Bit 3
#define RST       8     // SCSI:RST  Port D Bit 3
#define MSG       7     // SCSI:MSG  Port D Bit 2
#define SEL       6     // SCSI:SEL  Port D Bit 4
#define CD        5     // SCSI:C/D  Port D Bit 7
#define REQ       4     // SCSI:REQ  Port A Bit 13
#define IO        3     // SCSI:I/O  Port A Bit 12

#define SD_CS     BUILTIN_SDCARD      // SDCARD:CS
#define LED       13     // LED  Port C Bit 5

// LED control
#define LED_ON()       { GPIOC_PSOR = (1 << 5); }
#define LED_OFF()      { GPIOC_PCOR = (1 << 5); }

#define SET_REQ_ACTIVE()    { GPIOA_PCOR = (1 << 13); }
#define SET_REQ_INACTIVE()  { GPIOA_PSOR = (1 << 13); }
//#define SET_MSG_ACTIVE()    { GPIOD_PCOR = (1 << 2); __asm__("nop""\n\t""nop""\n\t"); }
//#define SET_MSG_INACTIVE()  { GPIOD_PSOR = (1 << 2); __asm__("nop""\n\t""nop""\n\t"); }
//#define SET_CD_ACTIVE()     { GPIOD_PCOR = (1 << 7); __asm__("nop""\n\t""nop""\n\t"); }
//#define SET_CD_INACTIVE()   { GPIOD_PSOR = (1 << 7); __asm__("nop""\n\t""nop""\n\t"); }
//#define SET_IO_ACTIVE()     { GPIOA_PCOR = (1 << 12); __asm__("nop""\n\t""nop""\n\t"); }
//#define SET_IO_INACTIVE()   { GPIOA_PSOR = (1 << 12); __asm__("nop""\n\t""nop""\n\t"); }

//#define SET_REQ_ACTIVE()     { pinMode(REQ, OUTPUT_OPENDRAIN);  digitalWrite(REQ, LOW); }
//#define SET_REQ_INACTIVE()   { digitalWrite(REQ, HIGH); pinMode(REQ, INPUT); }
#define SET_MSG_ACTIVE()     { pinMode(MSG, OUTPUT_OPENDRAIN); digitalWrite(MSG, LOW); }
#define SET_MSG_INACTIVE()   { digitalWrite(MSG, HIGH); pinMode(MSG, INPUT); }
#define SET_CD_ACTIVE()     { pinMode(CD, OUTPUT_OPENDRAIN);  digitalWrite(CD, LOW); }
#define SET_CD_INACTIVE()   { digitalWrite(CD, HIGH); pinMode(CD, INPUT); }
#define SET_IO_ACTIVE()     { pinMode(IO, OUTPUT_OPENDRAIN); digitalWrite(IO, LOW); }
#define SET_IO_INACTIVE()   { digitalWrite(IO, HIGH); pinMode(IO, INPUT); }
//#define SET_BSY_ACTIVE()     { pinMode(BSY, OUTPUT_OPENDRAIN); digitalWrite(BSY, LOW); }
//#define SET_BSY_INACTIVE()   { digitalWrite(BSY, HIGH); pinMode(BSY, INPUT); }

#define SET_BSY_ACTIVE()   { GPIOC_PCOR = (1 << 4); __asm__("nop""\n\t"); }
#define SET_BSY_INACTIVE() { GPIOC_PSOR = (1 << 4); }

#define GET_ACK() (!(GPIOC_PDIR & (1 << 3)))
#define GET_ATN() (!(GPIOC_PDIR & (1 << 7)))
#define GET_BSY() (!(GPIOC_PDIR & (1 << 4)))
#define GET_RST() (!(GPIOD_PDIR & (1 << 3)))
#define GET_SEL() (!(GPIOD_PDIR & (1 << 4)))

// Initiator Mode
#define SET_ACK_ACTIVE()   { GPIOC_PCOR = (1 << 3); __asm__("nop""\n\t"); }
#define SET_ACK_INACTIVE() { GPIOC_PSOR = (1 << 3); }
#define SET_ATN_ACTIVE()   { GPIOC_PCOR = (1 << 7); __asm__("nop""\n\t"); }
#define SET_ATN_INACTIVE() { GPIOC_PSOR = (1 << 7); }
#define SET_RST_ACTIVE()   { GPIOD_PCOR = (1 << 3); __asm__("nop""\n\t"); }
#define SET_RST_INACTIVE() { GPIOD_PSOR = (1 << 3); }
#define SET_SEL_ACTIVE()   { GPIOD_PCOR = (1 << 4); __asm__("nop""\n\t"); }
#define SET_SEL_INACTIVE() { GPIOD_PSOR = (1 << 4); }

#define GET_MSG() (!(GPIOD_PDIR & (1 << 2)))
#define GET_CD()  (!(GPIOD_PDIR & (1 << 7)))
#define GET_IO()  (!(GPIOA_PDIR & (1 << 12)))
#define GET_REQ() (!(GPIOA_PDIR & (1 << 13)))

// Turn on the output only for BSY
// BSY,REQ,MSG,CD,IO Turn on the output (no change required for OD)
#define SCSI_TARGET_ACTIVE()   { }
// BSY,REQ,MSG,CD,IO Turn off output
#define SCSI_TARGET_INACTIVE() { SCSI_DB_INPUT(); SET_REQ_INACTIVE(); SET_MSG_INACTIVE(); SET_CD_INACTIVE(); SET_IO_INACTIVE(); SET_BSY_INACTIVE(); }

#define SCSI_INITIATOR_INACTIVE() { SCSI_DB_INPUT(); SET_SEL_INACTIVE(); SET_ACK_INACTIVE(); SET_ATN_INACTIVE(); SET_RST_INACTIVE(); }

#define SCSI_DB_MASK 0x00ff0800


// HDImage file
#define HDIMG_ID_POS  2                 // Position to embed ID number
#define HDIMG_LUN_POS 3                 // Position to embed LUN numbers
#define HDIMG_BLK_POS 5                 // Position to embed block size numbers
#define MAX_FILE_PATH 32                // Maximum file name length

typedef struct m_sense_s {
  uint8_t     m_key;
  uint8_t     m_code;
  uint8_t     m_key_specific[4];
} msense_t;

typedef void (*CommandHandler_t)();
CommandHandler_t m_badlunhandler[256];

// VirtualDevice
typedef struct VirtualDevice_s
{
  boolean     m_enabled;
  uint8_t     m_id;
  uint8_t     m_lun;
  uint8_t     m_type;                 // Device Type
  char        m_filename[MAX_FILE_PATH+1];
  FsFile      m_file;                 // File object
  uint64_t    m_fileSize;             // File size
  uint8_t     m_sectors;
  uint8_t     m_heads;
  uint32_t    m_cylinders;
  size_t      m_blocksize;            // SCSI BLOCK size
  uint32_t    m_firstSector;          // First sector for partition
  boolean     m_rawPart;              // Raw Partition (True) or Image File (False)
#if SUPPORT_TAPE
  size_t      m_filemarks;            // Tape position counter (file marks since BOM)
#endif
  uint8_t     m_inquiryresponse[SCSI_INQUIRY_RESPONSE_SIZE];
  CommandHandler_t m_handler[256];
  msense_t    m_sense;
  uint16_t    m_quirks;
} VirtualDevice_t;

VirtualDevice_t  m_vdev[NUM_VDEV];    // Maximum number
uint8_t       m_vdevcnt = 0;          // Number of allocated vdevs
uint8_t       m_vdevmap[NUM_SCSIID][NUM_SCSILUN]; // Map ID/LUN to a vdev

volatile bool m_isBusReset = false;   // Bus reset

uint8_t       scsi_id_mask;           // Mask list of responding SCSI IDs
uint8_t       m_id;                   // Currently responding SCSI-ID
uint8_t       m_lun;                  // Logical unit number currently responding
uint8_t       m_sts;                  // Status uint8_t
uint8_t       m_msg;                  // Message uint8_ts
VirtualDevice_t *m_sel;               // VirtualDevice for current SCSI-ID, LUN
uint8_t       m_buf[MAX_BLOCKSIZE+1]; // General purpose buffer + overrun fetch
int           m_msc;
uint8_t       m_msb[256];             // Command storage uint8_ts
uint8_t       m_cmdlen;
uint8_t       m_cmd[12];
uint8_t       m_responsebuffer[256];
uint16_t      default_quirks = (SUPPORT_SASI_DEFAULT ? QUIRKS_SASI : 0) | (SUPPORT_APPLE_DEFAULT ? QUIRKS_APPLE : 0);
uint16_t ledbits = 0;
uint8_t ledbit = 0;

uint8_t cardMBR[512];

typedef enum {
  PHASE_BUSFREE = 0,
  PHASE_SELECTION,
  PHASE_MESSAGEOUT,
  PHASE_COMMAND,
  PHASE_STATUSIN,
  PHASE_MESSAGEIN,
} phase_t;
phase_t       m_phase = PHASE_BUSFREE;

// Log File
#define VERSION "1.4-20230529"
#if DEBUG == 2
#define LOG_FILENAME "LOG.txt"
FsFile LOG_FILE;
#endif


void onBusReset(void);
void initFileLog(void);
void finalizeFileLog(void);

/*
 * IO read.
 */
inline uint8_t readIO(void)
{
  // Port input data register
  uint32_t ret = ~GPIOB_PDIR;
  //uint8_t bret = ret >> 16;
#if READ_PARITY_CHECK
  if((db_bsrr[bret] ^ ret) & 0x0800)
    m_sts |= 0x01; // parity error
#endif

  return ret >> 16; //bret;
}

boolean OpenImage(VirtualDevice_t *h, const char *image_name)
{
  h->m_fileSize = 0;
  h->m_file = sd.open(image_name, O_RDWR);
  if(h->m_file.isOpen()) {
    h->m_fileSize = h->m_file.size();
    h->m_cylinders = (uint32_t)((uint64_t)h->m_fileSize / ((uint64_t)h->m_blocksize * (uint64_t)h->m_heads * (uint64_t)h->m_sectors));

    return true; // File opened
  }
  return false;
}

#if SUPPORT_DISK or SUPPORT_OPTICAL
/*
 * Open HDD or CDROM image file
 */
boolean OpenDiskImage(VirtualDevice_t *h, const char *image_name, int blocksize)
{
  if(!strncmp(image_name, "/tgts/", 6)) {
    LOGN("/tgts/ path is not supported for disk images.");
    return false;
  }

  if(!strncmp(image_name, "/vdevs/", 7)) {
    LOGN("/vdevs/ path is not supported for disk images.");
    return false;
  }

  if(!strncmp(image_name, "/diag/", 6)) {
    LOGN("/diag/ path is not supported for disk images.");
    return false;
  }

  if(!strncmp(image_name, "/nv/", 4)) {
    LOGN("/nv/ path is not supported for disk images.");
    return false;
  }

  h->m_rawPart = false;

  if(!strncmp(image_name, "/raw/part", 9)) {
    int partIndex = image_name[9] - '0';
    mbr_t *mbr = (mbr_t *)cardMBR;
    if((partIndex < 0) || (partIndex > 3)) {
      LOGN("partition index is outside the allowed range.");
      return false;
    }

    sd.card()->readSector(0, cardMBR);
    if(mbr->part[partIndex].type != 0x87) {
      LOGN("partition is of the wrong type.");
      return false;
    }
    
    h->m_blocksize = blocksize;
    h->m_fileSize = ((uint64_t)mbr->part[partIndex].totalSectors) * ((uint64_t)512);
    h->m_cylinders = (uint32_t)((uint64_t)h->m_fileSize / ((uint64_t)h->m_blocksize * (uint64_t)h->m_heads * (uint64_t)h->m_sectors));
    h->m_rawPart = true;
    h->m_firstSector = mbr->part[partIndex].firstSector;

    LOG(" Imagefile: ");
    LOG(image_name);
    LOG(" / ");
    LOG(h->m_fileSize / h->m_blocksize);
    LOG(" sectors / ");
    LOG(h->m_fileSize / 1024);
    LOG(" KiB / ");
    LOG(h->m_fileSize / 1024 / 1024);
    LOGN(" MiB");
    return true; // File opened
  }

  if(!strncmp(image_name, "/sd/", 4))
    image_name += 3;

  h->m_fileSize = 0;
  h->m_blocksize = blocksize;
  h->m_file = sd.open(image_name, O_RDWR);
  if(h->m_file.isOpen())
  {
    h->m_fileSize = h->m_file.size();
    h->m_cylinders = (uint32_t)((uint64_t)h->m_fileSize / ((uint64_t)h->m_blocksize * (uint64_t)h->m_heads * (uint64_t)h->m_sectors));
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
#endif /* SUPPORT_DISK or SUPPORT_OPTICAL */

#if SUPPORT_TAPE
/*
 * Open Tape image file
 */
boolean OpenTapeImage(VirtualDevice_t *h, const char *image_name)
{
  if(!strncmp(image_name, "/tgts/", 6)) {
    LOGN("/tgts/ path is not supported for tape images.");
    return false;
  }

  if(!strncmp(image_name, "/vdevs/", 7)) {
    LOGN("/vdevs/ path is not supported for tape images.");
    return false;
  }

  if(!strncmp(image_name, "/diag/", 6)) {
    LOGN("/diag/ path is not supported for tape images.");
    return false;
  }

  if(!strncmp(image_name, "/nv/", 4)) {
    LOGN("/nv/ path is not supported for tape images.");
    return false;
  }

  if(!strncmp(image_name, "/sd/", 4))
    image_name += 3;

  h->m_fileSize = 0;
  h->m_blocksize = 0;
  h->m_filemarks = 0;
  h->m_file = sd.open(image_name, O_RDWR);
  if(h->m_file.isOpen()) {
    h->m_fileSize = h->m_file.size();
    h->m_blocksize = 1024;
    LOG(" Imagefile: ");
    LOG(image_name);
    if(h->m_fileSize>0) {
      // check blocksize dummy file
      LOG(" / ");
      LOG(h->m_fileSize / 1024);
      LOG(" KiB / ");
      LOG(h->m_fileSize / 1024 / 1024);
      LOGN(" MiB");
    } else {
      LOGN(" / Blank Tape");
    }
    return true; // File opened
  }
  return false;
}
#endif /* SUPPORT_TAPE */

void pinModeFastSlew(uint8_t pin, int dummy) {
  pinMode(pin, OUTPUT_OPENDRAIN);
  volatile uint32_t *config = portConfigRegister(pin);
  *config = PORT_PCR_ODE | PORT_PCR_DSE | PORT_PCR_MUX(1);
}

/*
 * Initialization.
 *  Initialize the bus and set the PIN orientation
 */
void setup()
{
#if DEBUG > 0
  Serial.begin(115200);

  delay(1500);
#endif

  for(int id = 0; id < NUM_SCSIID; id++)
    for(int lun = 0; lun < NUM_SCSILUN; lun++)
      m_vdevmap[id][lun] = 0xff;
  m_vdevcnt = 0;

  // PIN initialization
  pinMode(LED, OUTPUT);
  LED_OFF();

  //GPIO(SCSI BUS)Initialization

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

  //Occurs when the RST pin state changes from HIGH to LOW
  attachInterrupt(RST, onBusReset, FALLING);
  attachInterrupt(SEL, SelectionPhaseISR, FALLING);

  LED_ON();

  // Filesystems
  lfs.begin(256 * 1024); // 256KB of program memory to be used as nonvolatile storage

  if(!sd.begin(SdioConfig(FIFO_SDIO))) {
#if DEBUG > 0
    Serial.println("SD initialization failed!");
#endif
    ledbits = 0b0000001010101010;
  }
  initFileLog();
  ConfigureBadLunHandlers();

  //Sector data overrun uint8_t setting
  //m_buf[MAX_BLOCKSIZE] = 0xff; // DB0 all off,DBP off
  //HD image file open
  scsi_id_mask = 0x00;

  // If greenscsi.cfg exists, run it (try first from SD, otherwise from flash)
  if(sd.exists("/greenscsi.cfg")) {
    execscript((char*)"/sd/greenscsi.cfg");
    execLoop();
  } else
  if(lfs.exists("/greenscsi.cfg")) {
    execscript((char*)"/nv/greenscsi.cfg");
    execLoop();
  }

  // Scan for images if we haven't defined any targets yet.
  if(m_vdevcnt == 0) findImages();

  finalizeFileLog();
  LED_OFF();

  cmdDisplay();
}

void findImages() {
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

#if SUPPORT_DISK or SUPPORT_OPTICAL
      if((file_name.startsWith("hd") || file_name.startsWith("cd")) && (file_name.endsWith(".img") || file_name.endsWith(".hda"))) {
        // Defaults for Hard Disks and CD-ROMs
        int id  = -1; // 0 and 3 are common in Macs for physical HD and CD, so avoid them.
        int lun = 0;
        int blk = 512;

        // Positionally read in and coerase the chars to integers.
        // We only require the minimum and read in the next if provided.
        int file_name_length = file_name.length();
        if(file_name_length > 2) { // HD[N]
          int tmp_id = file_name[HDIMG_ID_POS] - '0';

          if(tmp_id > -1 && tmp_id < NUM_SCSIID) {
            id = tmp_id; // If valid id, set it, else use default
            if(file_name_length > 3) { // HD0[N]
              int tmp_lun = file_name[HDIMG_LUN_POS] - '0';
    
              if(tmp_lun > -1 && tmp_lun < NUM_SCSILUN) {
                lun = tmp_lun; // If valid id, set it, else use default
              }
            }
          } else {
            id = ++usedDefaultId;
            lun = 0;
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
          uint8_t v = m_vdevcnt;
          VirtualDevice_t *h = &m_vdev[v];
          m_vdevmap[id][lun] = v;
          h->m_id = id;
          h->m_lun = lun;
          imageReady = false;

          // Configure Handlers and Inquiry Response for all luns
#if SUPPORT_DISK
          if(file_name.startsWith("hd")) {
            LOG(" Disk at ID ");
            LOG(id);
            LOG(":LUN ");
            LOG(lun);
            ConfigureDisk(h, name);
            imageReady = OpenDiskImage(h, name, blk);
          }
#endif /* SUPPORT_DISK */
#if SUPPORT_OPTICAL
          if(file_name.startsWith("cd")) {
            LOG("CDROM at ID ");
            LOG(id);
            LOG(":LUN ");
            LOG(lun);
            ConfigureOptical(h, name);
            imageReady = OpenDiskImage(h, name, blk);
          }
#endif /* SUPPORT_OPTICAL */
          if(imageReady) { // Marked as a responsive ID
            m_vdevcnt++;
            sprintf(h->m_filename, "/sd/%s", name);
            h->m_enabled = true;
            scsi_id_mask |= 1<<id;
          }
        } else {
          LOG("Bad LUN or SCSI id for image: ");
          LOGN(name);
        }
      } else
#endif /* SUPPORT_DISK or SUPPORT_OPTICAL */
#if SUPPORT_TAPE
      if(file_name.startsWith("dt") && file_name.endsWith(".tap")) {
        // Defaults for Tapes
        int id  = 1; // 0 and 3 are common in Macs for physical HD and CD, so avoid them.
        int lun = 0;

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

        if(id < NUM_SCSIID && lun < NUM_SCSILUN) {
          uint8_t v = m_vdevcnt;
          VirtualDevice_t *h = &m_vdev[v];
          m_vdevmap[id][lun] = v;
          h->m_id = id;
          h->m_lun = lun;
          imageReady = false;

          LOG(" Tape at ID ");
          LOG(id);
          LOG(":LUN ");
          LOG(lun);
          ConfigureTape(h, name);
          imageReady = OpenTapeImage(h, name);
          if(imageReady) { // Marked as a responsive ID
            m_vdevcnt++;
            h->m_enabled = true;
            sprintf(h->m_filename, "/sd/%s", name);
            scsi_id_mask |= 1<<id;
          }
        } else {
          LOG("Bad LUN or SCSI id for image: ");
          LOGN(name);
        }
      } else 
#endif /* SUPPORT_TAPE */
      {
//          LOG("Not an image: ");
//          LOGN(name);
      }
    } else {
      file.close();
    }
  }
  if(usedDefaultId > 0) {
    LOGN("!! More than one image did not specify a SCSI ID. Last file will be used at ID 1. !!");
  }
  if(m_vdevcnt == 0) {
    LOGN("!! No Virtual Devices defined. Please manually configure. !!");
  }
  root.close();
}

/*
 * Setup initialization logfile
 */
void initFileLog() {
#if DEBUG == 2
  LOG_FILE = sd.open(LOG_FILENAME, O_WRONLY | O_CREAT | O_TRUNC);
#endif
#if DEBUG
  LOGN("GreenSCSI <-> SD - https://github.com/dkgrizzly/GreenSCSI");
  LOG("VERSION: ");
  LOGN(VERSION);
  LOG("DEBUG:");
  LOGN(DEBUG);
#endif
}

/*
 * Finalize initialization logfile
 */
void finalizeFileLog() {
  LOGN("Initialization complete.");
#if DEBUG == 2
  LOG_FILE.close();
#endif
}


/*
 * MsgIn2.
 */
void MsgIn2(int msg)
{
  //LOGN("MsgIn2");
  SET_MSG_ACTIVE();
  SET_CD_ACTIVE();
  SET_IO_ACTIVE();
  writeHandshake(msg);
}

/*
 * MsgOut2.
 */
void MsgOut2()
{
  //LOGN("MsgOut2");
  SET_MSG_ACTIVE();
  SET_CD_ACTIVE();
  SET_IO_INACTIVE();
  m_msb[m_msc] = readHandshake();
  m_msc++;
  m_msc %= 256;
}

/*
 * Main loop.
 */
elapsedMillis ticktock;

void loop() {
  if(ledbits) {
    if(ticktock > 250) {
      if(ledbits & (1<<ledbit)) {
        LED_ON();
      } else {
        LED_OFF();
      }

      ledbit++;
      ledbit &= 0xf;

      ticktock = 0;
    }

    // Cancel ID error indication if we have any IDs enabled
    if((ledbit == 0) && (ledbits == 0b0000000000101010) && (scsi_id_mask != 0))
      ledbits = 0;
  }
  if(!ledbits) {
    if(scsi_id_mask == 0) ledbits = 0b0000000000101010;
  }

  switch(m_phase) {
    default:
    case PHASE_BUSFREE:
      BusFreePhaseHandler();
      break;
    case PHASE_SELECTION:
      //SelectionPhaseHandler();

      // Do other work while we wait for SEL to be asserted
      cmdPoll();
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

void SelectionPhaseISR() {
  m_msg = 0;

  // Wait until RST = H, BSY = H, SEL = L
  while(GET_BSY() || GET_RST()) {
    if(!GET_SEL()) return;
  }

  // BSY+ SEL-
  // If the ID to respond is not driven, wait for the next
  uint8_t scsiid = readIO() & scsi_id_mask;
  if(scsiid == 0) {
    // We were not selected, treat this as an abort and cancel any transactions we can.
    m_isBusReset = true;
    SCSI_TARGET_INACTIVE();
    m_phase = PHASE_BUSFREE;
    return;
  }
  LOG("Selection ");
  m_isBusReset = false;
  // Set BSY to-when selected
  SET_BSY_ACTIVE();     // Turn only BSY output ON, ACTIVE

  // Ask for a TARGET-ID to respond
#if USE_DB2ID_TABLE
  m_id = db2scsiid[scsiid];
#else
  for(m_id=7;m_id>=0;m_id--)
    if(scsiid & (1<<m_id)) break;
#endif
  m_lun = 0xff;

  //LOGN("Wait !SEL");
  // Wait until SEL becomes inactive
  while(GET_SEL() && !GET_BSY()) {
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
  if(GET_ATN()) {
    //LOGN("ATN");
    bool syncenable = false;
    int syncperiod = 50;
    int syncoffset = 0;
    int loopWait = 0;
    m_msc = 0;
    memset(m_msb, 0x00, sizeof(m_msb));
    while(GET_ATN() && loopWait < 255) {
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
  SET_CD_ACTIVE();
  SET_IO_INACTIVE();
  
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
  m_sel = (VirtualDevice_t *)0; // None
  if( (m_lun < NUM_SCSILUN) )
  {
    uint8_t v = m_vdevmap[m_id][m_lun];
    if(v < NUM_VDEV) {
      m_sel = &(m_vdev[v]); // There is an image
      if(!m_sel->m_enabled)
        m_sel = (VirtualDevice_t *)0;       // Image absent
    }
  }
  
  LOG(":ID ");
  LOG(m_id);
  LOG(":LUN ");
  LOG(m_lun);

  LOG(" ");

  if(!m_sel) {
    m_badlunhandler[m_cmd[0]]();
  } else {
    m_sel->m_handler[m_cmd[0]]();
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
  SET_CD_ACTIVE();
  SET_IO_ACTIVE();
  writeHandshake(m_sts);
  m_phase = PHASE_MESSAGEIN;
}

void MessageInPhaseHandler() {
  //LOGN("MsgIn");
  SET_MSG_ACTIVE();
  SET_CD_ACTIVE();
  SET_IO_ACTIVE();
  writeHandshake(m_msg);
  m_phase = PHASE_BUSFREE;
}

void BusFreePhaseHandler() {
  //LOGN("BusFree");
  // Release control of the bus.
  SCSI_TARGET_INACTIVE();
  // Clear the reset pending flag.
  m_isBusReset = false;
  // Reset back to waiting for selection phase.
  m_phase = PHASE_SELECTION;
}

typedef struct SelfTestPins_s {
  int A;
  int B;
  int pA;
  int pB;
  char nA[4];
  char nB[4];
} SelfTestPins_t;

SelfTestPins_t SelfTestPins[] = {
  { IO, DB0, 50, 2, "I/O", "DB0" },
  { IO, DB0, 48, 4, "REQ", "DB1" },
  { IO, DB0, 46, 6, "C/D", "DB2" },
  { IO, DB0, 44, 8, "SEL", "DB3" },
  { IO, DB0, 42, 10, "MSG", "DB4" },
  { IO, DB0, 50, 12, "RST", "DB5" },
  { IO, DB0, 38, 14, "ACK", "DB6" },
  { IO, DB0, 36, 16, "BSY", "DB7" },
  { IO, DB0, 32, 18, "ATN", "DBP" },
};

void SelfTest(int argc, char **argv) {
  int i, x;
  char c;

  Serial.printf("Are you sure you wish to run the self test? ");
  for(;;) {
    if (Serial.available()) {
      c = Serial.read();
      switch(c) {
        default:
          return;
        case 'y': case 'Y':
          goto ConnectHarness;
      }
    }
  }

ConnectHarness:
  // Clear any extra characters
  while (Serial.available()) {
    c = Serial.read();
  }

  // Disable normal operation and prepare the self test.
  detachInterrupt(RST);
  detachInterrupt(SEL);

  Serial.printf("Self Test starting...\r\n");

  // Delay for 3 seconds
  delay(3000);

  while (Serial.available()) {
    c = Serial.read();
  }

  Serial.printf("Connect the Loopback test adapter and press Enter.");
  for(;;) {
    if (Serial.available()) {
      c = Serial.read();
      switch(c) {
        case 0xA: case 0xD:
          goto ExecuteSelfTest;
      }
    }
  }

ExecuteSelfTest:
  // Clear any extra characters
  while (Serial.available()) {
    c = Serial.read();
  }

  // All pins input
  for(i = 0; i < 9; i++) {
    pinMode(SelfTestPins[i].A, INPUT_PULLUP);
    pinMode(SelfTestPins[i].B, INPUT_PULLUP);
  }

  for(i = 0; i < 9; i++) {
    // Test A -> B
    pinMode(SelfTestPins[i].A, OUTPUT_OPENDRAIN);
    digitalWrite(SelfTestPins[i].A, LOW);

    delay(10);

    if(digitalRead(SelfTestPins[i].B) != LOW) {
      Serial.printf("Self Test Failed. Pin %d (%s) was unable to pull Pin %d (%s) LOW.\r\n", SelfTestPins[i].pA, SelfTestPins[i].nA, SelfTestPins[i].pB, SelfTestPins[i].nB);
      pinMode(SelfTestPins[i].A, INPUT_PULLUP);
      return;
    }

    for(x = 0; x < 9; x++) {
      if(x != i) {
        if(digitalRead(SelfTestPins[x].A) == LOW) {
          Serial.printf("Self Test Failed. Pin %d (%s) is shorted to Pin %d (%s).\r\n", SelfTestPins[i].pA, SelfTestPins[i].nA, SelfTestPins[x].pA, SelfTestPins[x].nA);
          pinMode(SelfTestPins[i].A, INPUT_PULLUP);
          return;
        }
        if(digitalRead(SelfTestPins[x].B) == LOW) {
          Serial.printf("Self Test Failed. Pin %d (%s) is shorted to Pin %d (%s).\r\n", SelfTestPins[i].pA, SelfTestPins[i].nA, SelfTestPins[x].pB, SelfTestPins[x].nB);
          pinMode(SelfTestPins[i].A, INPUT_PULLUP);
          return;
        }
      }
    }

    pinMode(SelfTestPins[i].A, INPUT_PULLUP);

    delay(10);

    // Test B -> A
    pinMode(SelfTestPins[i].B, OUTPUT_OPENDRAIN);
    digitalWrite(SelfTestPins[i].B, LOW);

    delay(10);

    if(digitalRead(SelfTestPins[i].A) != LOW) {
      Serial.printf("Self Test Failed. Pin %d (%s) was unable to pull Pin %d (%s) LOW.\r\n", SelfTestPins[i].pB, SelfTestPins[i].nB, SelfTestPins[i].pA, SelfTestPins[i].nA);
      pinMode(SelfTestPins[i].B, INPUT_PULLUP);
      return;
    }

    for(x = 0; x < 9; x++) {
      if(x != i) {
        if(digitalRead(SelfTestPins[x].A) == LOW) {
          Serial.printf("Self Test Failed. Pin %d (%s) is shorted to Pin %d (%s).\r\n", SelfTestPins[i].pB, SelfTestPins[i].nB, SelfTestPins[x].pA, SelfTestPins[x].nA);
          pinMode(SelfTestPins[i].B, INPUT_PULLUP);
          return;
        }
        if(digitalRead(SelfTestPins[x].B) == LOW) {
          Serial.printf("Self Test Failed. Pin %d (%s) is shorted to Pin %d (%s).\r\n", SelfTestPins[i].pB, SelfTestPins[i].nB, SelfTestPins[x].pB, SelfTestPins[x].nB);
          pinMode(SelfTestPins[i].B, INPUT_PULLUP);
          return;
        }
      }
    }

    pinMode(SelfTestPins[i].B, INPUT_PULLUP);

    delay(10);
  }

  while (Serial.available()) {
    c = Serial.read();
  }

  Serial.printf("Disconnect the Loopback test adapter and press Enter.");
  for(;;) {
    if (Serial.available()) {
      c = Serial.read();
      switch(c) {
        case 0xA: case 0xD:
          goto DisconnectHarness;
      }
    }
  }

DisconnectHarness:
  // Clear any extra characters
  while (Serial.available()) {
    c = Serial.read();
  }

  for(i = 0; i < 9; i++) {
    // Test A -> B
    pinMode(SelfTestPins[i].A, OUTPUT_OPENDRAIN);
    digitalWrite(SelfTestPins[i].A, LOW);

    delay(10);

    if(digitalRead(SelfTestPins[i].B) == LOW) {
      Serial.printf("Self Test Failed. Pin %d (%s) is shorted to Pin %d (%s).\r\n", SelfTestPins[i].pA, SelfTestPins[i].nA, SelfTestPins[i].pB, SelfTestPins[i].nB);
      pinMode(SelfTestPins[i].A, INPUT_PULLUP);
      return;
    }

    // Test B -> A
    pinMode(SelfTestPins[i].B, OUTPUT_OPENDRAIN);
    digitalWrite(SelfTestPins[i].B, LOW);

    delay(10);

    if(digitalRead(SelfTestPins[i].A) == LOW) {
      Serial.printf("Self Test Failed. Pin %d (%s) is shorted to Pin %d (%s).\r\n", SelfTestPins[i].pB, SelfTestPins[i].nB, SelfTestPins[i].pA, SelfTestPins[i].nA);
      pinMode(SelfTestPins[i].B, INPUT_PULLUP);
      return;
    }
  }

//SelfTestComplete:
  // Clear any extra characters
  while (Serial.available()) {
    c = Serial.read();
  }

  Serial.printf("Self Test Passed.\r\n");

// On success, restore normal operation
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
  
  attachInterrupt(RST, onBusReset, FALLING);
  attachInterrupt(SEL, SelectionPhaseISR, FALLING);

  LED_OFF();
}
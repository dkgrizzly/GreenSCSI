#include "config.h"

// SCSI Drive Vendor information
#if SUPPORT_DISK
static uint8_t SCSI_DISK_INQUIRY_RESPONSE[96] = {
  0x00, //device type
  0x00, //RMB = 0
  0x05, //ISO, ECMA, ANSI version
  0x02, //Response data format
  35 - 4, //Additional data length
  0, 0, //Reserve
  0x00, //Support function
  'Q', 'U', 'A', 'N', 'T', 'U', 'M', ' ', // vendor 8
  'F', 'I', 'R', 'E', 'B', 'A', 'L', 'L', '1', ' ', ' ',' ', ' ', ' ', ' ', ' ', // product 16
  '1', '.', '0', ' ', // version 4
  // Release Number (1 Byte)
  0x20,
  // Revision Date (10 Bytes)
  '2','0','2','1','/','1','1','/','2','2',
  0
};
#endif /* SUPPORT_DISK */

#if SUPPORT_SASI
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
#endif /* SUPPORT_SASI */

#if SUPPORT_OPTICAL
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
  '2','0','2','1','/','1','1','/','2','2',
  0
};
#endif /* SUPPORT_OPTICAL */

#if SUPPORT_TAPE
static uint8_t SCSI_TAPE_INQUIRY_RESPONSE[96] = {
  0x01, //device type
  0x80, //RMB = 0
  0x03, //ISO, ECMA, ANSI version
  0x02, //Response data format
  37 - 4, //Additional data length
  0, //Reserve
  0x00, // Support function
  0x00, // Support function
  'I', 'B', 'M', ' ', ' ', ' ', ' ', ' ', // vendor 8
  'U', 'L', 'T', '3', '5', '8', '0', '-', 'T', 'D', '1',' ', ' ', ' ', ' ', ' ', // product 16
  'M', 'B', 'N', '0', // version 4
  0
};
#endif /* SUPPORT_TAPE */

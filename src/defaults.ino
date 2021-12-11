#include "config.h"

// SCSI Drive Vendor information

static uint8_t SCSI_INQUIRY_RESPONSE[][SCSI_INQUIRY_RESPONSE_SIZE] = {
#if SUPPORT_SASI
{
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
},
#endif /* SUPPORT_SASI */

#if SUPPORT_DISK
{
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
},

{
  0x00, //device type
  0x00, //RMB = 0
  0x05, //ISO, ECMA, ANSI version
  0x02, //Response data format
  35 - 4, //Additional data length
  0, 0, //Reserve
  0x00, //Support function
  'S', 'E', 'A', 'G', 'A', 'T', 'E', ' ', // vendor 8
  'S', 'T', '2', '2', '5', 'N', ' ', ' ', ' ', ' ', ' ',' ', ' ', ' ', ' ', ' ', // product 16
  '1', '.', '0', ' ', // version 4
  // Release Number (1 Byte)
  0x20,
  // Revision Date (10 Bytes)
  '2','0','2','1','/','1','1','/','2','2',
  0
},
#endif /* SUPPORT_DISK */

#if SUPPORT_OPTICAL
{
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
},
{
  0x05, //device type
  0x80, //RMB = 0
  0x05, //ISO, ECMA, ANSI version
  0x02, //Response data format
  46 - 4, //Additional data length
  0, 0, //Reserve
  0x00, //Support function
  'S', 'O', 'N', 'Y', ' ', ' ', ' ', ' ', // vendor 8
  'C', 'D', 'U', '-', '7', '6', 'S', ' ', ' ', ' ', ' ',' ', ' ', ' ', ' ', ' ', // product 16
  '1', '.', '0', ' ', // version 4
  // Release Number (1 Byte)
  0x20,
  // Revision Date (10 Bytes)
  '2','0','2','1','/','1','1','/','2','2',
  0
},
{
  0x05, //device type
  0x80, //RMB = 0
  0x05, //ISO, ECMA, ANSI version
  0x02, //Response data format
  35 - 4, //Additional data length
  0, 0, //Reserve
  0x00, //Support function
  'M', 'A', 'T', 'S', 'H', 'I', 'T', 'A', // vendor 8
  'C', 'D', '-', 'R', 'O', 'M', ' ', 'C', 'R', '8', '0','0', '5', ' ', ' ', ' ', // product 16
  '1', '.', '0', 'k', // version 4
  0
},
{
  0x05, //device type
  0x80, //RMB = 0
  0x05, //ISO, ECMA, ANSI version
  0x02, //Response data format
  35 - 4, //Additional data length
  0, 0, //Reserve
  0x00, //Support function
  'D', 'E', 'C', ' ', ' ', ' ', ' ', ' ', // vendor 8
  'R', 'R', 'D', '4', '5', ' ', ' ', ' ', '(', 'C', ')',' ', 'D', 'E', 'C', ' ', // product 16
  '0', '4', '3', '6', // version 4
  0
},
{
  0x05, //device type
  0x80, //RMB = 0
  0x05, //ISO, ECMA, ANSI version
  0x02, //Response data format
  35 - 4, //Additional data length
  0, 0, //Reserve
  0x98, //Support function
  'T', 'O', 'S', 'H', 'I', 'B', 'A', ' ', // vendor 8
  'C', 'D', '-', 'R', 'O', 'M', ' ', 'X', 'M', '-', '3','3', '0', '1', 'T', 'A', // product 16
  '0', '2', '7', '2', // version 4
  0
},
{
  0x05, //device type
  0x80, //RMB = 0
  0x05, //ISO, ECMA, ANSI version
  0x02, //Response data format
  35 - 4, //Additional data length
  0, 0, //Reserve
  0x98, //Support function
  'T', 'O', 'S', 'H', 'I', 'B', 'A', ' ', // vendor 8
  'C', 'D', '-', 'R', 'O', 'M', ' ', 'X', 'M', '-', '5','7', '0', '1', 'T', 'A', // product 16
  '3', '1', '3', '6', // version 4
  0
},
#endif /* SUPPORT_OPTICAL */

#if SUPPORT_TAPE
{
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
},
#endif /* SUPPORT_TAPE */

// Invalid entry to mark end of data
{
  0xff, 0xff, 0xff, 0xff, 0x00, 0x00, 0x00,
  ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ',
  ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ',
  ' ', ' ', ' ', ' ',
  0
}

};

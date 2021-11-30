#ifndef __SCSI_DEFS_H
#define __SCSI_DEFS_H

/* Mode pages */
#define MODEPAGE_VENDOR_SPECIFIC    0x00
#define MODEPAGE_RW_ERROR_RECOVERY  0x01
#define MODEPAGE_DCRC_PARAMETERS    0x02
#define MODEPAGE_FORMAT_PARAMETERS  0x03
#define MODEPAGE_RIGID_GEOMETRY     0x04
#define MODEPAGE_APPLE              0x30
#define MODEPAGE_ALL_PAGES          0x3F

/* 6 Byte command opcodes */
#define CMD_TEST_UNIT_READY       0x00
#define CMD_REZERO_UNIT           0x01
#define CMD_REQUEST_SENSE         0x03
#define CMD_FORMAT_UNIT           0x04
#define CMD_READ_BLOCK_LIMITS     0x05
#define CMD_FORMAT_UNIT_ALT       0x06
#define CMD_REASSIGN_BLOCKS       0x07
#define CMD_READ6                 0x08
#define CMD_WRITE6                0x0A  /* Optional */
#define CMD_SEEK6                 0x0B  /* Optional */

#define CMD_WRITE_FILEMARKS       0x10
#define CMD_SPACE                 0x11
#define CMD_INQUIRY               0x12
#define CMD_MODE_SELECT6          0x15  /* Optional */
#define CMD_RESERVE6              0x16  /* Optional */
#define CMD_RELEASE6              0x17  /* Optional */
#define CMD_ERASE                 0x19
#define CMD_MODE_SENSE6           0x1A  /* Optional */
#define CMD_START_STOP_UNIT       0x1B  /* Optional */
#define CMD_RECV_DIAGNOSTIC       0x1C
#define CMD_SEND_DIAGNOSTIC       0x1D
#define CMD_PREVENT_REMOVAL       0x1E
/* 10 Byte command opcodes */
#define CMD_READ_CAPACITY10       0x25
#define CMD_READ10                0x28
#define CMD_WRITE10               0x2A  /* Optional */
#define CMD_SEEK10                0x2B  /* Optional */
#define CMD_READUPDATEDBLOCK10    0x2D
#define CMD_WRITEANDVERIFY10      0x2E  /* Optional */
#define CMD_VERIFY10              0x2F
#define CMD_PREFETCH_CACHE10      0x34  /* Optional */
#define CMD_READPOSITION10        0x34  /* Optional */
#define CMD_SYNCHRONIZE_CACHE10   0x35  /* Optional */
#define CMD_LOCKUNLOCK_CACHE10    0x36  /* Optional */
#define CMD_READ_DEFECT_DATA      0x37  /* Optional */
#define CMD_WRITEBUFFER           0x3B  /* Optional */
#define CMD_READBUFFER            0x3C  /* Optional */
#define CMD_READLONG10            0x3E  /* Optional */
#define CMD_WRITELONG10           0x3F  /* Optional */
#define CMD_READ_TOC              0x43
#define CMD_READ_HEADER           0x44
#define CMD_GET_CONFIGURATION     0x46
#define CMD_GET_EVENT_STATUS_NOTIFICATION 0x4a
#define CMD_READ_DISC_INFORMATION 0x51
#define CMD_MODE_SELECT10         0x55
#define CMD_RESERVE10             0x56
#define CMD_RELEASE10             0x57
#define CMD_MODE_SENSE10          0x5A  /* Optional */
/* 12 Byte command opcodes */
#define CMD_REPORT_LUNS           0xA0  /* Optional */
#define CMD_SET_DRIVE_PARAMETER   0xC2
#define CMD_MAC_UNKNOWN           0xEE  /* Unknown */

/* Dayna SCSI/Link Ethernet */
#define CMD_SCSILINK_STATS        0x09
#define CMD_SCSILINK_ENABLE       0x0E
#define CMD_SCSILINK_SET          0x0C
#define CMD_SCSILINK_SETMODE      0x80
#define CMD_SCSILINK_SETMAC       0x40

/* Cabletron Ethernet */
#define CMD_CABLETRON_SEND         0x0c01
#define CMD_CABLETRON_RECV         0xe1
#define CMD_CABLETRON_GET_ADDR     0x0c04
#define CMD_CABLETRON_ADD_PROTO    0x0d01
#define CMD_CABLETRON_REM_PROTO    0x0d02
#define CMD_CABLETRON_SET_MODE     0x0d03
#define CMD_CABLETRON_SET_MULTI    0x0d04
#define CMD_CABLETRON_REMOVE_MULTI 0x0d05
#define CMD_CABLETRON_GET_STATS    0x0d06
#define CMD_CABLETRON_SET_MEDIA    0x0d07
#define CMD_CABLETRON_GET_MEDIA    0x0d08
#define CMD_CABLETRON_LOAD_IMAGE   0x0d09
#define CMD_CABLETRON_SET_ADDR     0x0d0a

/*
 *  SCSI DEVICE CODES
 */
#define DEV_DISK            0x00
#define DEV_TAPE            0x01
#define DEV_OPTICAL         0x05

/*
 *  SCSI MESSAGE CODES
 */
#define COMMAND_COMPLETE    0x00
#define EXTENDED_MESSAGE    0x01
#define     EXTENDED_MODIFY_DATA_POINTER    0x00
#define     EXTENDED_SDTR                   0x01
#define     EXTENDED_EXTENDED_IDENTIFY      0x02    /* SCSI-I only */
#define     EXTENDED_WDTR                   0x03
#define     EXTENDED_PPR                    0x04
#define     EXTENDED_MODIFY_BIDI_DATA_PTR   0x05
#define SAVE_POINTERS       0x02
#define RESTORE_POINTERS    0x03
#define DISCONNECT          0x04
#define INITIATOR_ERROR     0x05
#define ABORT_TASK_SET      0x06
#define MESSAGE_REJECT      0x07
#define NOP                 0x08
#define MSG_PARITY_ERROR    0x09
#define LINKED_CMD_COMPLETE 0x0a
#define LINKED_FLG_CMD_COMPLETE 0x0b
#define TARGET_RESET        0x0c
#define ABORT_TASK          0x0d
#define CLEAR_TASK_SET      0x0e
#define INITIATE_RECOVERY   0x0f            /* SCSI-II only */
#define RELEASE_RECOVERY    0x10            /* SCSI-II only */
#define CLEAR_ACA           0x16
#define LOGICAL_UNIT_RESET  0x17
#define SIMPLE_QUEUE_TAG    0x20
#define HEAD_OF_QUEUE_TAG   0x21
#define ORDERED_QUEUE_TAG   0x22
#define IGNORE_WIDE_RESIDUE 0x23
#define ACA                 0x24
#define QAS_REQUEST         0x55
#define IDENTIFY            0x80


/* Task states */
#define ENDED            0x00
#define CURRENT          0x01

/* Status */
#define STATUS_GOOD           0x00
#define STATUS_CHECK          0x02
#define STATUS_BUSY           0x08
#define STATUS_INTERMEDIATE   0x10
#define STATUS_CONFLICT       0x18

/* Sense keys */
#define NO_SENSE         0x00
#define RECOVERED_ERROR  0x01
#define NOT_READY        0x02
#define MEDIUM_ERROR     0x03
#define HARDWARE_ERROR   0x04
#define ILLEGAL_REQUEST  0x05
#define UNIT_ATTENTION   0x06
#define DATA_PROTECT     0x07

/* Additional Sense Information */
#define NO_ADDITIONAL_SENSE_INFORMATION 0x00
#define INVALID_LBA 0x21
#define INVALID_FIELD_IN_CDB 0x24
#define NOTREADY_TO_READY_CHANGE 0x28
#define UNIT_POWERON_RESET   0x29
#define NO_MEDIA             0x3A
#define ERROR_IN_OPCODE      0xC0

#if USE_DB2ID_TABLE
extern const uint8_t db2scsiid[256];
#endif

#endif

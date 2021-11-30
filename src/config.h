#ifndef __CONFIG_H
#define __CONFIG_H

#define DEBUG                  0          // 0:No debug information output (Faster by 2-3x+)
                                          // 1: Debug information output to USB Serial
                                          // 2: Debug information output to LOG.txt (slow)

#define SYNCHRO                false      // Support Synchronous mode.
#define ACK_INTERRUPTS         false
#define READ_SPEED_OPTIMIZE    true       // 
#define WRITE_SPEED_OPTIMIZE   true       // 
#define USE_DB2ID_TABLE        true       // Use table to get ID from SEL-DB

// SCSI config
#define NUM_SCSIID             8          // Maximum number of supported SCSI-IDs (The minimum is 1)
#define NUM_SCSILUN            8          // Maximum number of LUNs supported     (The minimum is 1)
#define NUM_VDEV               8          // Maximum number of VDEVs supported    (The minimum is 1)

#define READ_PARITY_CHECK      0          // Perform read parity check (unverified)

// HDD format
#define MAX_BLOCKSIZE          (1 << 15)  // Maximum BLOCK size (2048 to 8192 tested, 16384 had issues)


// Supported Device Types
#define SUPPORT_DISK           true
#define SUPPORT_OPTICAL        true
#define SUPPORT_TAPE           false


// Compatibility Settings
#define SUPPORT_SASI           false      // Enable SASI compatiblity for Sharp X68000
#define SUPPORT_SASI_DEFAULT   false      // Turn it on by default


#endif /* __CONFIG_H */

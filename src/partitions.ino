#if 0
#include "config.h"
#include "cmd.h"

typedef struct PartType_s {
  unsigned char Type;
  const char *Name;
} PartType_t;
PartType_t PartType[] = {
  { 0x00, "Empty" },
  { 0x01, "FAT12" },
  { 0x04, "FAT16 <32MB" },
  { 0x05, "Extended" },
  { 0x06, "FAT16 >32MB" },
  { 0x07, "NTFS" },
  { 0x0b, "FAT32" },
  { 0x0c, "FAT32 LBA" },
  { 0x0e, "FAT16 LBA" },
  { 0x0f, "Extended LBA" },
  { 0xf8, "TinySCSI Disk LUN" },
  { 0xf9, "TinySCSI Optical LUN" },
  { 0xff, NULL }
};

#endif

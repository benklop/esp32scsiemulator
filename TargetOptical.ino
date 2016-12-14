
#if 0
// Read Updated Block
int target_CommandReadUpdatedBlock() {
  uint32_t sectorOffset;
  uint32_t sectorCount;

  DEBUGPRINT(DBG_GENERAL, "\n\r\t>Read Updated Block(10)");
#if 0
  sectorOffset = (target_cmdbuf[2] << 24) | (target_cmdbuf[3] << 16) | (target_cmdbuf[4] << 8) | target_cmdbuf[5];
  sectorCount = 1;

  // Read Sectors
  return ncrReadSectors(sectorOffset, sectorCount);
#endif
  return 0;
}
#endif


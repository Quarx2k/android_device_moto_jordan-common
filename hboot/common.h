#ifndef __HW_COMMON_H__
#define __HW_COMMON_H__
#include "types.h"
#include "stdio.h"

extern uint32_t cfg_powerup_reason;
extern uint32_t cfg_emu_uart;
extern uint16_t cfg_batt_status;
extern uint8_t cfg_cid_recovery;

static inline uint8_t read8(addr_t addr) 
{
  return *((volatile uint8_t*)addr);
}

static inline void write8(uint8_t v, addr_t addr) 
{
  *((volatile uint8_t*)addr) = v;
}

static inline uint16_t read16(addr_t addr) 
{
  return *((volatile uint16_t*)addr);
}

static inline void write16(uint32_t v, addr_t addr) 
{
  *((volatile uint16_t*)addr) = v;
}

static inline uint32_t read32(addr_t addr) 
{
  return *((volatile uint32_t*)addr);
}

static inline void write32(uint32_t v, addr_t addr) 
{
  *((volatile uint32_t*)addr) = v;
}

static inline void modify_register16(addr_t addr, uint16_t mask, uint16_t val) 
{
  write16((read16(addr) & (~mask)) | val, addr);
}

static inline void modify_register32(addr_t addr, uint32_t mask, uint32_t val) 
{
  write32((read32(addr) & (~mask)) | val, addr);
}

void u_to_hex(int x, int digits, char *s);
int puts(const char *s);

#endif // __HW_COMMON_H__

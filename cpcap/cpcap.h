#ifndef CPCAP_H
#define CPCAP_H

#define SUCCEED(...) \
  if (! (__VA_ARGS__)) { \
    fprintf(stderr, "%s:%d expression '%s' failed: %s", __func__, __LINE__, #__VA_ARGS__, strerror(errno)); \
    if (cpcap_fd > 0) { close(cpcap_fd); } \
    return 1; \
  }

struct cpcap_batt_data_mb525 {
    int status;
    int health;
    int present;
    int capacity;
    int batt_volt;
    int batt_temp;
    int batt_full_capacity;
    int batt_capacity_one;
};


/*
Tegra2 (atrix) use two banks of macros

enum cpcap_macro {       // bank 1                              bank 2
    CPCAP_MACRO_ROMR,
    CPCAP_MACRO_RAMW,
    CPCAP_MACRO_RAMR,
    CPCAP_MACRO_USEROFF,
    CPCAP_MACRO_4,       // uc init like 12 (after fw upload)   standby
    CPCAP_MACRO_5,       // 3mm5, like 13                       standby
    CPCAP_MACRO_6,       // rgb led blinking                    kick the watchdog
    CPCAP_MACRO_7,
    CPCAP_MACRO_8,
    CPCAP_MACRO_9,
    CPCAP_MACRO_10,
    CPCAP_MACRO_11,
    CPCAP_MACRO_12,      // uc   ST only REG_MI2
    CPCAP_MACRO_13,      // 3mm5 ST only
    CPCAP_MACRO_14,      // pm debug TI REG_MIM1 / ST REG_MI2   REG_MI2
    CPCAP_MACRO_15,

    CPCAP_MACRO__END,
};
*/

#endif

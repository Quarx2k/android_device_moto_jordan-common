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

#endif

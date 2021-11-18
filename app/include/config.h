#ifndef __CONFIG_H_
#define __CONFIG_H_

#define DATABASE_FILE_NAME  "/.conf"
#define VALUES_COUNT        1000
#define VALUE_SIZE          12
#define CRC32_STR_SIZE      8


struct database_entry {
    uint16_t type;
    uint16_t id;
    uint8_t value[VALUE_SIZE];
};

enum database_entry_types {
    DB_TYPE_PHONE_NUMBER  = 0,
    DB_TYPE_CRC32   = 1000,
};


struct config {
    uint8_t number[VALUES_COUNT][VALUE_SIZE];
    uint32_t entries_count;
    uint32_t crc32;
    uint32_t is_valid;
};

extern struct config master;
extern void config_init(void);
extern void config_save();
extern int is_number_exists(uint8_t *number);

#endif /* __CONFIG_H_ */

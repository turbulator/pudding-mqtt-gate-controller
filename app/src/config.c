#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "api_fs.h"
#include <api_debug.h>


#include "config.h"
#include "crc32.h"

struct config master;

int config_load()
{
    int32_t fd, ret;
    uint32_t computed_crc32 = 0;
    uint8_t *path = DATABASE_FILE_NAME;
    struct database_entry entry;
    uint64_t filesize;

    fd = API_FS_Open(path, FS_O_RDONLY, 0);

    if(fd < 0) {
        Trace(2,"%s: can't open %s", __FUNCTION__, path);
        return -1;
    }

    filesize = API_FS_GetFileSize(fd);
    Trace(2,"%s: filesize = %d", __FUNCTION__, (int)filesize);

    do {
        ret = API_FS_Read(fd, (uint8_t*)&entry, sizeof(struct database_entry));
        // Trace(2,"%s: ret = %d", __FUNCTION__, ret);
        if(ret == sizeof(struct database_entry)) {
            switch (entry.type) {
                case DB_TYPE_PHONE_NUMBER:
                    if(entry.id >= 0 && entry.id < VALUES_COUNT) {
                        Trace(2,"%s: phone number entry[%d] = %s", __FUNCTION__, entry.id, entry.value);
                        memcpy(master.number[entry.id], entry.value, VALUE_SIZE);
                        master.entries_count++;
                    }
                    break;

                case DB_TYPE_CRC32:
                    Trace(2,"%s: crc32 entry = %s", __FUNCTION__, entry.value);
                    master.crc32 = atox(entry.value, CRC32_STR_SIZE);
                    break;

                default:
                    Trace(2,"%s: unknown entry type:%d id:%d value:%s", __FUNCTION__, entry.type, entry.id, entry.value);
                    break;
            }
        }
    } while (ret);

    API_FS_Close(fd);

    crc32((uint8_t*)master.number, sizeof(master.number), &computed_crc32);

    Trace(2,"%s: database contains %d entries", __FUNCTION__, master.entries_count);

    if(computed_crc32 == master.crc32) {
        Trace(2,"%s: database is fine", __FUNCTION__);
        master.is_valid = 1;
    } else {
        Trace(2,"%s: database is corrupted", __FUNCTION__);
        master.is_valid = 0;
    }


    return 0;
}

void config_save()
{
    int32_t fd;
    uint16_t type, i;
    uint8_t *path = DATABASE_FILE_NAME;
    uint8_t buffer[VALUE_SIZE];

    fd = API_FS_Open(path, FS_O_WRONLY | FS_O_CREAT | FS_O_TRUNC, 0);

    for(i = 0; i < VALUES_COUNT; i++) {
        if(strlen(master.number[i]) > 0) {
            // Trace(2,"%s: entry[%d] = %s", __FUNCTION__, i, master.number[i]);
            type = DB_TYPE_PHONE_NUMBER;
            API_FS_Write(fd, (uint8_t*)(&type), sizeof(uint16_t));        
            API_FS_Write(fd, (uint8_t*)(&i), sizeof(uint16_t));        
            API_FS_Write(fd, (uint8_t*)(master.number[i]), VALUE_SIZE);        
        }
    }

    Trace(2,"%s: crc32:%08X", __FUNCTION__, master.crc32);
    type = DB_TYPE_CRC32;
    snprintf(buffer, VALUE_SIZE, "%08X", master.crc32);
    API_FS_Write(fd, (uint8_t*)(&type), sizeof(uint16_t));        
    API_FS_Write(fd, (uint8_t*)(&type), sizeof(uint16_t));   // any two-byte-lenght value
    API_FS_Write(fd, (uint8_t*)(buffer), VALUE_SIZE);        


    API_FS_Flush(fd);

    API_FS_Close(fd);
}

int is_number_exists(uint8_t *number)
{
    int i;

    for(i = 0; i < VALUES_COUNT; i++) {
        if(strcmp(number, master.number[i]) == 0) {
            return i;
        }
    }

    return -1;
}

void config_init(void)
{
    memset(&master, 0x00, sizeof(struct config));
    config_load();
}

#include <stdio.h>
#include "error.h"
#include "sector.h"
#include "unixv6fs.h"
#include <errno.h>

#define SECTORS_TO_READ (1)
#define SECTORS_TO_WRITE (1)

/**
 * @brief read one 512-byte sector from the virtual disk
 * @param f open file of the virtual disk
 * @param sector the location (in sector units, not bytes) within the virtual disk
 * @param data a pointer to 512-bytes of memory (OUT)
 * @return 0 on success; <0 on error
 */
int sector_read(FILE *f, uint32_t sector, void *data)
{
    M_REQUIRE_NON_NULL(f);
    M_REQUIRE_NON_NULL(data);

    if ((ferror(f) == 0) && (fseek(f, SECTOR_SIZE * sector, SEEK_SET) == 0)
            && fread(data, SECTOR_SIZE , SECTORS_TO_READ, f) == SECTORS_TO_READ) {
        return 0;
    }

    return ERR_IO;
}

/**
 * @brief write one 512-byte sector to the virtual disk
 * @param f open file of the virtual disk
 * @param sector the location (in sector units, not bytes) within the virtual disk
 * @param data a pointer to 512-bytes of memory (IN)
 * @return 0 on success; <0 on error
 */
int sector_write(FILE *f, uint32_t sector, const void *data)
{
    M_REQUIRE_NON_NULL(f);
    M_REQUIRE_NON_NULL(data);

    if ((ferror(f) == 0) && (fseek(f, SECTOR_SIZE * sector, SEEK_SET) == 0)
            && fwrite(data, SECTOR_SIZE , SECTORS_TO_WRITE, f) == SECTORS_TO_WRITE) {
        return 0;
    }

    return ERR_IO;
}

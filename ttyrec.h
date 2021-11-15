#include <stdint.h>

#ifdef __APPLE__
# include <machine/endian.h>
#elif defined __FreeBSD__
# include <sys/endian.h>
#else
# include <endian.h>
#endif

#if BYTE_ORDER == BIG_ENDIAN
# define to_little_endian(x) ((uint32_t) ( \
    ((uint32_t)(x) &(uint32_t)0x000000ffU) << 24 | \
    ((uint32_t)(x) &(uint32_t)0x0000ff00U) <<  8 | \
    ((uint32_t)(x) &(uint32_t)0x00ff0000U) >>  8 | \
    ((uint32_t)(x) &(uint32_t)0xff000000U) >> 24))
#else
# define to_little_endian(x) ((uint32_t)(x))
#endif
#define from_little_endian(x) to_little_endian(x)

struct ttyrec_header
{
    uint32_t sec;
    uint32_t usec;
    uint32_t len;
};

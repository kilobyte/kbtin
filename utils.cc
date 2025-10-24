/*********************************************************************/
/* file: utils.c - some utility-functions                            */
/*                             TINTIN III                            */
/*          (T)he K(I)cki(N) (T)ickin D(I)kumud Clie(N)t             */
/*                     coded by peter unold 1992                     */
/*********************************************************************/
#include "tintin.h"
#include <time.h>
#include "protos/user.h"

void die(const char *msg, ...);

/*********************************************/
/* return: true if s1 is an abrevation of s2 */
/*********************************************/
bool is_abrev(const char *s1, const char *s2)
{
    return !strncmp(s2, s1, strlen(s1));
}

/********************************/
/* strdup - duplicates a string */
/* return: address of duplicate */
/********************************/
char* mystrdup(const char *s)
{
    char *dup;

    if (!s)
        return 0;
    int len = strlen(s) + 1;
    if (!(dup = static_cast<char*>(malloc(len))))
        die("Not enough memory for strdup.");
    memcpy(dup, s, len);
    return dup;
}

char* mystrdup(const std::string &s)
{
    char *dup;

    int len = s.size() + 1;
    if (!(dup = static_cast<char*>(malloc(len))))
        die("Not enough memory for strdup.");
    memcpy(dup, s.c_str(), len);
    return dup;
}

/***********************************************/
/* print non-errno error message and terminate */
/***********************************************/
void die(const char *msg, ...)
{
    va_list ap;

    if (ui_own_output)
        user_done();

    va_start(ap, msg);
    vfprintf(stderr, msg, ap);
    va_end(ap);
    fprintf(stderr, "\n");
    exit(1);
}

/*************************************************/
/* print system call error message and terminate */
/*************************************************/
void syserr(const char *msg, ...)
{
    va_list ap;

    if (ui_own_output)
        user_done();

    if (errno)
        fprintf(stderr, "ERROR (%s):  ", strerror(errno));
    else
        fprintf(stderr, "ERROR:  ");
    va_start(ap, msg);
    vfprintf(stderr, msg, ap);
    va_end(ap);
    fprintf(stderr, "\n");
    exit(1);
}

#ifndef HAVE_STRLCPY
/* not for protos.h */ size_t strlcpy(char *dst, const char *src, size_t n)
{
    if (!n)
        return strlen(src);

    const char *s = src;

    while (--n > 0)
        if (!(*dst++ = *s++))
            break;

    if (!n)
    {
        *dst++ = 0;
        while (*s++)
            ;
    }

    return s - src - 1;
}
#endif

void write_stdout(const char *restrict x, int len)
{
again:;
    int r = write(1, x, len);
    if (r == len)
        return;
    if (r > 0)
    {
        x+=r;
        len-=r;
        goto again;
    }
    errno = r;
    syserr("write to stdout failed");
}

timens_t current_time(void)
{
    struct timespec cts;
    clock_gettime(CLOCK_REALTIME, &cts);
    return cts.tv_sec*NANO + cts.tv_nsec;
}

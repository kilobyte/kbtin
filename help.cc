/*****************************************************************/
/* functions for the #help command                               */
/*****************************************************************/
#include "tintin.h"
#include <fcntl.h>
#include "protos/globals.h"
#include "protos/print.h"
#include "protos/run.h"
#include "protos/utils.h"


#ifndef O_BINARY
# define O_BINARY 0
#endif

static FILE* check_file(const char *filestring)
{
#if COMPRESSED_HELP
    char sysfile[BUFFER_SIZE+8];
    int f;

    sprintf(sysfile, "%s%s", filestring, COMPRESSION_EXT);
    if ((f=open(sysfile, O_RDONLY|O_BINARY))==-1)
        return 0;
    return mypopen(UNCOMPRESS_CMD, false, f);
#else
    return (FILE *) fopen(filestring, "r");
#endif
}

void help_command(const char *arg, session *ses)
{
    FILE *myfile=NULL;
    char text[BUFFER_SIZE], line[BUFFER_SIZE], filestring[BUFFER_SIZE];

    if (strcmp(DEFAULT_FILE_DIR, "HOME"))
    {
        sprintf(filestring, "%s/KBtin_help", DEFAULT_FILE_DIR);
        myfile = check_file(filestring);
    }
#ifdef DATA_PATH
    if (!myfile)
    {
        sprintf(filestring, "%s/KBtin_help", DATA_PATH);
        myfile = check_file(filestring);
    }
#endif
    if (!myfile)
    {
        snprintf(filestring, sizeof filestring, "%s_help", tintin_exec);
        myfile = check_file(filestring);
    }
    if (!myfile)
    {
        snprintf(filestring, sizeof filestring, "%s/KBtin_help", getenv("HOME"));
        myfile = check_file(filestring);
    }
    if (!myfile)
    {
        tintin_eprintf(0, "#Help file not found - no help available.");
        tintin_eprintf(0, "#Locations checked:");
        if (strcmp(DEFAULT_FILE_DIR, "HOME"))
            tintin_eprintf(0, "#      %s/KBtin_help%s", DEFAULT_FILE_DIR,
                COMPRESSION_EXT);
#ifdef DATA_PATH
        tintin_eprintf(0, "#      %s/KBtin_help%s", DATA_PATH,
            COMPRESSION_EXT);
#endif
        tintin_eprintf(0, "#      %s_help%s", tintin_exec,
            COMPRESSION_EXT);
        tintin_eprintf(0, "#      %s/KBtin_help%s", getenv("HOME"),
            COMPRESSION_EXT);
        return;
    }

    if (!*arg)
    {
        while (fgets(line, sizeof(line), myfile))
        {
            if (line[0]=='~' && line[1]=='~')
                break;
            char *eol=strchr(line, '\n');
            if (eol)
                *eol=0;
            tintin_printf(0, "%s", line);
        }
        fclose(myfile);
        return;
    }

    if (*arg==tintin_char)
        arg++;
    sprintf(text, "~%s", arg);
    while (fgets(line, sizeof(line), myfile))
    {
        if (*line != '~' || !is_abrev(text, line))
            continue;
        while (fgets(line, sizeof(line), myfile))
        {
            if (line[0]=='~' && line[1]=='~')
                break;
            char *eol=strchr(line, '\n');
            if (eol)
                *eol=0;
            tintin_printf(0, "%s", line);
        }
        fclose(myfile);
        return;
    }
    tintin_printf(0, "#Sorry, no help on that word.");
    fclose(myfile);
}

#include "../tintin.h"
#include <stdio.h>
#include "../protos/colors.h"

bool bold=1;

int main()
{
    char in[BUFFER_SIZE], out[BUFFER_SIZE];
    int c=7, oldc=7;

    while (fgets(in, sizeof in, stdin))
    {
        char *b=out;
        for (const char *a=in; *a; a++)
        {
            if (*a == '~')
            {
                if (!getcolor(&a, &c, 1))
                    *b++='~';
                else
                {
                    if (c == -1)
                        c = oldc;
                    b=ansicolor(b, c);
                }
            }
            else
                *b++=*a;
        }
        *b=0;
        printf("%s", out);
        oldc=c;
    }
    return 0;
}

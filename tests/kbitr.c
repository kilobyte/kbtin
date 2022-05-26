#include <stdio.h>
#include "../kbtree.h"

typedef int num;

static num numcmp(num a, num b)
{
    return a-b;
}

KBTREE_HEADER(num, num, numcmp)
KBTREE_CODE(num, num, numcmp)

int main()
{
    for (num n=0;n<=1000;n++)
    {
        kbtree_t(num) *elm = kb_init(num, KB_DEFAULT_SIZE);
        elm = kb_init(num, KB_DEFAULT_SIZE);
        for (int i=0; i<n; i++)
          kb_put(num, elm, i*2+1);

        for (num k=0; k<=n*2; k++)
        {
            kbitr_t itr;
            kb_itr_after(num, elm, &itr, k);
            if (kb_itr_valid(&itr))
            {
                num j = kb_itr_key(num, &itr);
                num l = (k + 1) | 1;
                if (j != l)
                {
                    printf("got %d wanted %d\n", j, l);
                    return 1;
                }
            }
            else if (k+1<n*2)
            {
                printf("falsely out of range\n");
                return 1;
            }
        }

        kb_destroy(num, elm);
    }

    return 0;
}

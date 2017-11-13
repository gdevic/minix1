    /* library routine for copying structs with unpleasant alignment */

    __stb(n, f, t)
	    register char *f, *t; register int n;
    {
	    if (n > 0)
		    do
			    *t++ = *f++;
		    while (--n);
    }

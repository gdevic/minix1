#include "stdio.h"
#include "errno.h"
extern int errno;

#ifdef LC

int _stack = 4000;
#define SWITCHCHAR '\\'    /* character that separates path elements */
#define PATHCHAR ';'       /* character that separates elements of the PATH */
char linecont = '+';       /* default line continuation character */
char switchc =  '\\';      /* default directory separation char */

#else

#define SHELL "/bin/sh"   /* the shell to execute */
#define SWITCHCHAR '/'    /* character that separates path elements */
#define PATHCHAR ':'      /* character that separates elements of the PATH */
char linecont = '\\';     /* default line continuation character */
char switchc =  '/';      /* default directory separation char */

#endif


/* the argument to malloc */
#define UI unsigned int

#define TIME long 

/* generic linked list */
struct llist {
    char *name;
    struct llist *next;
};

struct defnrec {
    char *name;
    int uptodate;
    TIME modified;
    struct llist *dependson;
    struct llist *howto;
    struct defnrec *nextdefn;
};

struct macrec {
    char *name,*mexpand;
    struct macrec *nextmac;
};

struct rulerec {
    char *dep,*targ;    /* order is important because there are defaults */
    struct llist *rule;
    int def_flag;
    struct rulerec *nextrule;
};

/* default rules for make */
/*
   1. Modifiy NDRULES to reflect the total number of rules...
   2. Enter rules in the following format
	"source-extension","object-extension","rule",NULL,
      and update NDRULES.
   3. Note that the default rules are searched from top to bottom in this
      list, and that within default rules, macros are ignored if not defined
      (You may specify FFLAGS in the rule without requiring the user to 
       define FFLAGS in his/her makefile)
   4. The rule applied will be the first one matching. If I ever get a
      public domain YACC working, the rule for it will be first, and look
      like ".y",".o","bison $(YFLAGS) $*.y","cc $* $(CFLAGS)",NULL,
      There are at present no depends in rules, so you need a .y.o: and
      a .y.c: rule
   5. The src and dest extensions are expected to be in lower case
   6. SUFFIX sets the name and precedence for the prerequisite macros,
      $< and $?, and for looking up rules. Basically, all <obj> type
      extensions must be to the left of <src> type extensions. All
      rules should have their suffix mentioned. See UNIX documentation or
      make.doc
*/

#define NDRULES 3
char *def_list[] =
{
    ".c",".s","$(CC) -S $(CFLAGS) $*.c",NULL,
    ".c",".o","$(CC) -c $(CFLAGS) $*.c",NULL,
    ".s",".o","$(AS) $(AFLAGS) -o $*.o $*.s",NULL,
};
#define SUFFIXES ".o .s .c"


#ifndef LC
TIME time();
#define now() (TIME) time( (char *) 0)  /* the current time, in seconds */
#else
#include "time.h"
TIME now()
{
struct tm timer,*p;
long t,ftpack();
char tim[6];

	p = &timer;
	time(&t);
	p = localtime(&t);
	tim[0] = p->tm_year - 80;
	tim[1] = p->tm_mon + 1;
	tim[2] = p->tm_mday;
	tim[3] = p->tm_hour;
	tim[4] = p->tm_min;
	tim[5] = p->tm_sec;
	return(ftpack(tim));
}
#endif


char **ext_env;
char *whoami = "Make";  


#ifndef LC
#define max(a,b) ((a)>(b)?(a):(b))
#define min(a,b) ((a)<=(b)?(a):(b))
#endif
#define BKSLSH '\134'
#undef ERROR
#define ERROR -1
#define TRUE 1
#define FALSE 0
#define EQ(X,Y) (strcmp(X,Y) == 0)
#define NUL '\0'
#define isnull(X) (X == '\0' ? TRUE : FALSE)
#define notnull(X) (X == '\0' ? FALSE : TRUE )
#define isspace(X) ( (X == ' ') || (X == '\011') )


/*
flags for expand. Report and ignore errors, and no_target means error if
$@ or $* is attempted ( as in $* : a.obj ) 
*/
#define NO_TARG  2
#define REPT_ERR 1
#define IGN_ERR  0

#define INMAX   500     /* maximum input line length (allow for expansion)*/
#define INMAXSH 80      /* length for short strings */


#ifdef BSD4.2
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#define WAIT union wait

#else
/* sysV and MONIX  and LATTICE */
struct stat {
    short int st_dev;
    unsigned short st_ino;
    unsigned short st_mode;
    short int st_nlink;
    short int st_uid;
    short int st_gid;
    short int st_rdev;
    long st_size;
    TIME st_atime;
    TIME st_mtime;
    TIME st_ctime;
} ; 
/*
#include <sys/types.h>
#include <sys/stat.h>
*/
#define WAIT int
#endif


struct macrec   *maclist = NULL;
struct defnrec  *defnlist = NULL;
struct llist    *dolist = NULL;
struct llist    *path_head = NULL;
struct llist    *suff_head = NULL;
struct rulerec  *rulelist = NULL;
struct llist    *makef = NULL;



#ifdef LC
int linkerf = TRUE;
char tfilename[50] = "makefile.mac";
#endif

int execute = TRUE;
int stopOnErr = TRUE;
int forgeahead = FALSE;
int madesomething = FALSE;
int knowhow = FALSE;
int no_file = TRUE;
int silentf = FALSE;
#ifdef DEBUG
int tree_and_quit = FALSE;
int post_tree = FALSE;
#endif DEBUG
int path_set = FALSE;

/* return declarations for functions not returning an int */
TIME make(),getmodified(),findexec();
char *mov_in(),*get_mem();
struct llist *MkListMem(), *mkllist(), *mkexphow(), *mkexpdep(),*add_llist();

/* temp storage variables for the prerequisite lists */
struct m_preq {
    char m_dep[6];
    char m_targ[6];
    char m_name[INMAXSH];
};
char dothis[INMAX];    /* generic temporary storage */
FILE *fopen();

main(argc,argv,envp)
int argc;
char *argv[],*envp[];
{

    ext_env = envp;
    init(argc,argv);
#ifdef DEBUG
    if (tree_and_quit){
	prtree();
fprintf(stderr, "tree and quit\n");
	done(0);
    }
#endif DEBUG
	

    /* now fall down the dolist and do them all */

    while (dolist != NULL) {
	madesomething = FALSE;
	make(dolist->name);
	if (!madesomething) {
	    if (knowhow) {
		if ( !silentf )
		    fprintf(stdout,"%s: '%s' is up to date.\n",whoami,dolist->name);
	    }
	    else {
		error2("Don't know how to make %s",dolist->name);
	    }
	}
	dolist = dolist->next;
    }
#ifdef DEBUG
    if (post_tree) prtree();
#endif DEBUG

    done( 0 );
}


init(argc,argv)
int argc;
char *argv[];
{
int i,k;
int pdep,ptarg;
struct llist *ptr;
int usedefault = TRUE;


    for (k=i=0; i < NDRULES; i++,k++) {
	/* couldn't init the linked list  with the compiler */
	pdep = k++;
	ptarg = k++;
	ptr = NULL;
	while ( def_list[k] != NULL ) {
	    ptr = add_llist(ptr,def_list[k]);
	    k++;
	}
	add_rule2(def_list[pdep],def_list[ptarg],ptr,TRUE);
    }

    /* initialize the suffix list */
    add_suff(SUFFIXES);

    /* load the macro MFLAGS */
    for  ( i = 1,dothis[0] = NUL;  i < argc ; i++ ) {
	strcat(dothis,argv[i]);
	strcat(dothis," ");
    }
    add_macro("MFLAGS",dothis);

    /* add any default macros here, such as CC */
       add_macro("CC","cc");
       add_macro("AS","as");

    /* step thru the args and read the flags, patterns and objects */
    for (i=1; i < argc; i++) {
	if (argv[i][0] == '-') {    /* option */
	    switch (argv[i][1]) {
		case 'f': case 'F': /* arg following is a makefile */
		    if (++i < argc) {
			makef = add_llist(makef,argv[i]);
			usedefault = FALSE;
		    }
		    else 
			panic("'-f' requires filename");
		    break;

		case 'c': case 'C':  /* line continuation char */
		   if ( argv[i][2] != NUL ) 
		      linecont = argv[i][2];
		   else {
		       if ( ++i < argc || notnull(argv[i][1]) )
			   linecont = argv[i][0];
		       else
			   error("'-c' requires single character");
		   }
		   break;

		case 'k':case 'K':  /* abandon work on this branch on error */
		    forgeahead = TRUE;
		    break;

		case 'b': case 'B':  /* switchchar change */
		   if ( argv[i][2] != NUL ) 
		      switchc = argv[i][2];
		   else {
		       if ( ++i < argc || notnull(argv[i][1]) )
			   switchc = argv[i][0];
		       else
			   error("'-b' requires single character");
		   }
		   break;

		case 'i': case 'I': /* ignore errors on execution */
		    stopOnErr = FALSE;
		    break;

		case 'n': case 'N': /* don't execute commands - just print */
		    execute = FALSE;
		    break;

		case 's': case 'S': /* execute, but don't print */
		    silentf = TRUE;
		    break;
		    
#ifdef DEBUG
		case 'p':case 'P':  /* print a tree and quit */
		    tree_and_quit = TRUE;
		    break;

		case 'v': case 'V': /* print the tree after all processing */
		    post_tree = TRUE;
		    break;
#endif DEBUG

		default:
		    error2("unknown option '%s'",argv[i]);
	    }

	}
	else {
	    if ( !maccheck(argv[i]) ) {
		/* if argv[i] is not of the pattern xxx=yyy, then
		   it must be something to make. maccheck will add the
		   macro if it is...
		*/
		dolist = add_llist(dolist,argv[i]);
		no_file = FALSE;
	    }
	}
    }
    /* 
       collect the flags, then read the makefile, otherwise a construct like
       make -f make2 aardvark
       will make both aardvark and the first target in make2
    */   
    if (usedefault) {
	if ( getmodified("Makefile") != (TIME) 0 ) readmakefile("Makefile");
	else readmakefile("makefile");
    }
    else {
	for ( ptr = makef; ptr != NULL; ptr = ptr->next )
	   readmakefile(ptr->name);
    }
    
    /* if not done by a PATH directive, set the basic PATH */
    if ( !path_set ) {mkpathlist();path_set = TRUE;}

    
}/* init */

TIME make(s)    /* returns the modified date/time */
char *s;
{
struct defnrec  *defnp,*tryrules(),*dummy;
struct llist    *depp,*depp2;
struct llist    *howp;
char            *m_comp,*m_ood,*m_obj;
char            *stradd(),*add_prereq();
TIME            latest,timeof;
struct m_preq   ma;


    /* look for the definition */
    for ( defnp = defnlist; defnp != NULL; defnp = defnp->nextdefn ) 
	if ( EQ(defnp->name,s) )  break;
    
    /* 
       there might be some adjusting of the lists for implied compilations.
       these additions are not done in readmakefile;
       they might not be required for these targets ( ie not requested )
    */

    if (defnp == NULL ){
	defnp = tryrules(s);
	if (defnp == NULL){   /* tryrules returns a pointer to a defnrec */
	    /* tryrules fails, and there is no definition */        
	    knowhow = FALSE;
	    latest = getmodified(s);
	    if (latest==0) {
		/* doesn't exist but don't know how to make */
		panic2("Can't make %s",s);
		/* NOTREACHED */
	    }
	    else {
		/*
		   exists - assume it's up to date since we don't know
		   how to make it. Make a defnrec with uptodate == TRUE.
		   This is not strictly necessary, and this line may be
		   deleted. It is a speed win, at the expense of core, when
		   there are a lot of header files.
		*/
		
		/*add_defn(s,TRUE,latest,
			 (struct llist *)NULL,(struct llist*)NULL); */
		return(latest);
	    }
	}
    }
    
    else{    /* there is a definition line */
	if (defnp->uptodate) {
	    return(defnp->modified);
	}
	dummy = tryrules(s); 
	if (dummy != NULL && defnp->howto == NULL){
	    /* any explicit howto overrides an implicit one */
	    /*add depend*/
	    if (defnp->dependson == NULL)
		defnp->dependson = dummy->dependson;
	    else{
		for (depp2 = defnp->dependson;
		     depp2->next != NULL;
		     depp2 = depp2->next); /* advance to end of list */
		depp2->next = dummy->dependson;
	    }
	    /* add howto line */
	    defnp->howto = dummy->howto;
	}
    }
    /* finished adjusting the lists */

    /* check that everything that the current target depends on is uptodate */
    m_comp = m_ood = m_obj = NULL;
    latest = 0;
    depp = defnp->dependson;
    while (depp != NULL) {

	m_comp = add_prereq(m_comp,depp->name,&ma);/* add to string for $< */
	timeof = make(depp->name);
	latest = max(timeof,latest);/*written this way to avoid side effects*/

	if ( defnp->modified < timeof ) { 
	    /* these are out of date with respect to the current target */
	    m_ood = stradd(m_ood,ma.m_name,ma.m_dep); /* add source */
	    m_obj = stradd(m_obj,ma.m_name,ma.m_targ);/* add object */
	}
	depp = depp->next;
    }

    knowhow = TRUE; /* has dependencies therefore we know how */

    /* if necessary, execute all of the commands to make it */
    /* if (out of date) || (depends on nothing)             */

    if (latest > defnp->modified || defnp->dependson==NULL) {
	/* make those suckers */
	howp = defnp->howto;
	if ((howp == NULL) && !in_dolist(s)) {
      fprintf(stderr,"%s: %s is out of date, but there is no command line\n",
	          whoami,s);
	   if ( stopOnErr ) mystop_err();
	}
	while (howp != NULL) {

	    /* the execute flag controls whether execution takes place */
	    p_expand(howp->name,dothis,m_comp,m_ood,m_obj);
	    if ( (exec_how(dothis) != 0) )
	       if (forgeahead) break;
	       else if (stopOnErr) panicstop();
	    howp = howp->next;
	}
	/* we just made something. Set the time of modification to now. */
	defnp->modified =  now();
	defnp->uptodate = TRUE;
	if (defnp->howto != NULL)   /* we had instructions */
	    madesomething = TRUE;
    }
    if ( m_comp != NULL ) free( m_comp );
    if ( m_ood  != NULL ) free( m_ood );
    if ( m_obj  != NULL ) free( m_obj );
    return(defnp->modified);

}


struct llist *add_llist(head,s)  /*this adds something to a linked list */
char *s;
struct llist *head;
{
struct llist *ptr1;

    /* find a slot in the list */
    if (head == NULL) {
	ptr1 = MkListMem();
	ptr1->name = mov_in(s);
	ptr1->next = NULL;
	return ( ptr1 );
    }
    else {
	for (ptr1 = head; ptr1->next != NULL ; ptr1 = ptr1->next ) ;
	ptr1->next = MkListMem();
	ptr1->next->name = mov_in(s);
	ptr1->next->next = NULL;
	return( head );
    }
}

expand(src,dest,target,flag)    /* expand any macros found*/
char *src,*dest,*target;
int  flag;
{
char  thismac[INMAX],*ismac(),*ismac_c();
char  thismac2[INMAX],*macptr;
int   i,pos,back, j;


   back = pos = 0;
   if ( src == NULL ) {   /* NULL pointer, shouldn't happen */
       dest[0] = NUL;
       return;
   }
   while( notnull(src[pos]) ) {

       if (src[pos] != '$') dest[back++] = src[pos++];

       else {
	   pos++;
	   /*found '$'. What kind of macro is this? */
	   switch(src[pos]){
		case '(':   /*regular macro*/
		case '{':   /*regular macro*/
		   /* do macro stuff */
		   pos = x_scan(src,pos,thismac); /* get macro */
		   if ( maclist == NULL && (flag & REPT_ERR) )
			error2("No macros defined -- %s",thismac);
		   else if ( (macptr=ismac(thismac)) == NULL) {
			expand(thismac,thismac2,target,flag);
			if ( (macptr = ismac(thismac2)) != NULL)
			    /* the recursive expand found it */
			    back = mv_install(macptr,dest,back);
			else {
			    if ( flag & REPT_ERR )
			        error2("Can't expand macro -- %s",thismac2);
			}
		   }
		   else {
		        /* macptr points to appropriate macro */
		        back = mv_install(macptr,dest,back);
		   }
		   break;

		case '*':    /*target without extension*/
		case '@':    /*whole target*/
		   if ((flag & NO_TARG) && (flag & REPT_ERR) ){
			 fprintf(stderr,"%s: '$%c' not in a command or dependency line\n",
				      whoami,src[pos]);
			  if ( stopOnErr ) mystop_err();
			  else return;

		   }
		   else {
			for (i=0; notnull(target[i]) ; i++) {
				if (target[i] == '.' && src[pos] == '*') {
					j = i;
					while (notnull(target[j]) && target[j] != switchc)
						j++;
					if (notnull(target[j]) == FALSE)
						break;
				}
			    dest[back++] = target[i];
			}
		   }
		   break;


		default:

		   if ( (macptr = ismac_c(src[pos])) != NULL )
			back = mv_install(macptr,dest,back);
		   else {
			/*not an approved macro, ignore*/
			dest[back++] = '$';
			dest[back++] = src[pos];
		   }
		   break;
	    }/* else */
	    pos++;
       }
   }
   dest[back] = NUL;
}

p_expand(src,dest,compl_preq,ood_preq,ood_obj)
char *src,*dest;
char *compl_preq,*ood_preq,*ood_obj;
{
    /*
       expand the special macros $< $? $> just before execution
    */
    int back,pos,i;
    
    if ( src == NULL ) {
	    dest[0] = NUL;
	    return;
    }

    back = pos = 0;
    while( notnull(src[pos]) ) {

    if (src[pos] != '$' ) dest[back++] = src[pos++];

    else {
	pos++;
	switch( src[pos] ) {

	case '<':     /* put in the complete list of prerequisites */
	    for ( i = 0; notnull(compl_preq[i]); i++ )
		dest[back++] = compl_preq[i];
	    break;

	case '?':    /* the prerequisites that are out of date */
	    for ( i = 0; notnull(ood_obj[i]); i++ )
		dest[back++] = ood_obj[i];
	    break;

	case '>':    /* the source files that are out of date (NOT STANDARD)*/
	    for ( i = 0; notnull(ood_preq[i]); i++ )
		dest[back++] = ood_preq[i];
	    break;

	case '$':     /* get rid of the doubled $$ */
	    dest[back++] = '$';
	    break;

	default:
	    dest[back++] = '$';
	    dest[back++] = src[pos];
	    break;
	}
	pos++;
    } /* else */

    } /* switch */
    dest[back] = NUL;
}

/* is this a character macro? */
char *ismac_c(cc)
char cc;
{
char *ismac();
char *str = " "; /* space for a one char string */
    str[0] = cc;
    return(ismac(str));
}

/* is this string a macro? */
char *ismac(test)
char *test;
{
struct macrec *ptr;

    ptr = maclist;
    if ( ptr == NULL ) return( NULL );
    while( TRUE ) {
       if      ( EQ(ptr->name,test) ) return( ptr->mexpand );
       else if ( ptr->nextmac == NULL )  return( NULL );
       else                              ptr = ptr->nextmac;
    }
}

maccheck(sptr) /* if this string has a '=', then add it as a macro */
char *sptr;
{
int k;

    for ( k=0; notnull(sptr[k]) && (sptr[k] != '='); k++ );
    if ( isnull(sptr[k]) ) return( FALSE );
    else {
	/* found a macro */
	sptr[k] = NUL;
	add_macro(sptr,sptr + k + 1);
	return( TRUE );
    }
}
	

x_scan(src,pos,dest)
char *src,*dest;
int pos;
{
char bterm,eterm;
int cnt;

    /* load dest with macro, allowing for nesting */
    if ( src[pos] == '('  ) eterm = ')';
    else if ( src[pos] == '{'  ) eterm = '}';
    else panic("very bad things happening in x_scan");

    bterm = src[pos++];
    cnt = 1;

    while ( notnull(src[pos]) ) {
       if ( src[pos] == bterm ) cnt++;
       else if ( src[pos] == eterm ) {
	   cnt--;
	   if ( cnt == 0 ) {
	      *dest = NUL;
	      return( pos );
	   }
       }
       *dest++ = src[pos++];
    }
    panic2("No closing brace/paren for %s",src);
    /* NOTREACHED */
}
    
/* expand and move to dest */
mv_install(from,to,back)
char *from,*to;
int back;
{
int i;

    for ( i=0; notnull(from[i]) ; i++)
	to[back++] = from[i];
    return( back );
}


/*
  attempts to apply a rule.  If an applicable rule is found, returns a 
  pointer to a (new) struct which can be added to the defnlist
  An applicable rule is one in which target ext matches, and a source file
  exists.
*/

struct defnrec *tryrules(string)
char *string;
{
struct rulerec *rptr,*isrule();
struct llist   *sptr;
struct defnrec *retval;
char           s[INMAXSH],buf[INMAXSH],sext[10];

	my_strcpy(s,string);
	get_ext(s,sext); /* s is now truncated */

	if ( sext[0] == NUL ) {
	    /*target has no extension*/
	    return(NULL);
	}

	/* look for this extension in the suffix list */
	for ( sptr = suff_head; sptr != NULL; sptr = sptr->next )
	    if ( EQ(sptr->name,sext) ) break;

	if ( sptr == NULL ) {
	    /* not a valid extension */
	    return( NULL );
	}

	/* continue looking, now for existence of a source file **/
	for ( sptr = sptr->next; sptr != NULL; sptr = sptr->next ) 
	    if ( exists(s,sptr->name) &&
		 ((rptr = isrule(rulelist,sptr->name,sext)) != NULL)
	       ) break;


	if ( sptr == NULL ) {
	    /* no rule applies */
	    return( NULL );
	}

	retval = (struct defnrec *)get_mem((UI) sizeof(struct defnrec));
	my_strcpy(buf,s);     /* s --  is the stem of the object*/
	strcat(buf,rptr->targ);
	retval->name = mov_in(buf);
	my_strcpy(buf,s);
	strcat(buf,rptr->dep);
	retval->dependson = mkllist(buf);
	retval->uptodate = FALSE;
	retval->modified = getmodified(retval->name);
	retval->nextdefn = NULL;
	retval->howto = mkexphow(rptr->rule,retval->name,
				 (rptr->def_flag)? IGN_ERR : REPT_ERR);
	return(retval);                 /*whew*/

}


/* does the file exist? */
exists(name,suffix)
char *name,*suffix;
{
char t[INMAXSH];

    my_strcpy(t,name);
    strcat(t,suffix);
    return (getmodified(t) != (TIME) 0 ? TRUE : FALSE );
}

struct rulerec *isrule(head,src,dest)
struct rulerec *head;
char *src,*dest;
{
    /* return ptr to rule if this is a legit rule */
    struct rulerec *ptr;

    if ( head == NULL ) return( NULL );
    else {
	for ( ptr = head; ptr != NULL; ptr = ptr->nextrule )
	    if ( EQ(ptr->dep,src) && EQ(ptr->targ,dest) )
		return( ptr );
	return( NULL );
    }
}

#ifdef DEBUG
/*debugging*/
/* print the dependencies and command lines... */
prtree()
{
struct defnrec *dummy;
struct macrec  *mdum;
struct llist   *dum2,*dum3,*rdum2;
struct rulerec *rdum;
int    cnt;

    dummy = defnlist;
    while (dummy != NULL){
	fprintf(stdout,"name '%s'  exists: %s\n",
		    dummy->name,(dummy->modified)?"no":"yes");
		    
	dum2 = dummy->dependson;
	fprintf(stdout,"   depends-on:");
	cnt =0;
	while (dum2 != NULL){
	    fprintf(stdout," %13s ",dum2->name);
	    cnt++;
	    if (cnt == 4){
		cnt = 0;
		fprintf(stdout,"\n              ");
	    }
	    dum2 = dum2->next;
	}
	fprintf(stdout,"\n");

	dum3 = dummy->howto;
	while (dum3 != NULL){
	    fprintf(stdout,"      command: %s\n",dum3->name);
	    dum3 = dum3->next;
	}
	dummy = dummy->nextdefn;
	fprintf(stdout,"\n");
    }

    fprintf(stdout,"\n       *RULES*\n\n");
    fprintf(stdout,"src=     dest=     rule=\n");
    rdum = rulelist;
    while ( rdum != NULL ) {
	if ( rdum->rule == NULL )
	    fprintf(stdout,"%4s     %4s      %s\n",
		    rdum->dep,rdum->targ,"** Empty Rule **");
	else {
	    fprintf(stdout,"%4s     %4s      %s\n",
	            rdum->dep,rdum->targ,rdum->rule->name);
	    rdum2 = rdum->rule->next;
	    while ( rdum2 != NULL ) {
		fprintf(stdout,"                   %s\n",rdum2->name);
		rdum2 = rdum2->next;
	    }
	}
	rdum = rdum->nextrule;
    }

    mdum = maclist;
    if ( mdum == NULL ) 
        fprintf(stdout,"\n        *NO MACROS*\n");
    else {
	fprintf(stdout,"\n        *MACROS*\n\n");
	fprintf(stdout," macro          expansion\n");
	while ( mdum != NULL ) {
           fprintf(stdout," %8s       %s\n",mdum->name,mdum->mexpand);
           mdum = mdum->nextmac;
        }
    }
    fprintf(stdout,"\n\nsuffix list is");
    if ( suff_head == NULL ) fprintf(stdout," empty.");
    else for ( dum2 = suff_head; dum2 != NULL; dum2 = dum2->next){
	fprintf(stdout," %s",dum2->name);
    }
    fprintf(stdout,"\npath is ");
    if (path_head == NULL) fprintf(stdout," empty.");
    else 
		for ( dum2 = path_head; dum2 != NULL; dum2 = dum2->next) 
	         fprintf(stdout," %s:",dum2->name);

    fprintf(stdout,"\nswitch character  '%c'\n",switchc);
    fprintf(stdout,"line continuation '%c'\n",linecont);
} /*debug*/
#endif DEBUG


error(s1)
char *s1;
{
    fprintf(stderr,"%s: ",whoami);
    fprintf(stderr,s1);
    fprintf(stderr,"\n");
    if (stopOnErr) mystop_err();
    else return;
}

error2(s1,s2)
char *s1,*s2;
{
    fprintf(stderr,"%s: ",whoami);
    fprintf(stderr,s1,s2);
    fprintf(stderr,"\n");	
    if (stopOnErr) mystop_err();
    else return;
}

panic(s1)
char *s1;
{
    fprintf(stderr,"%s: ",whoami);
    fprintf(stderr,s1);
    fprintf(stderr,"\n");	
    mystop_err();
    /* NOTREACHED */
}

panic2(s1,s2)
char *s1,*s2;
{
    fprintf(stderr,"%s: ",whoami);
    fprintf(stderr,s1,s2);
    fprintf(stderr,"\n");	
    mystop_err();
    /* NOTREACHED */
}

panicstop()
{
    fprintf(stderr,"\n\n  ***Stop.\n");
    mystop_err();
    /* NOTREACHED */
}

mystop_err()
{
    done( -1 );
    /* NOTREACHED */
}


in_dolist(s)  /* true if this is something we are to make */
char *s;
{
struct llist *ptr;

    for ( ptr = dolist; ptr != NULL ; ptr = ptr->next )
	if ( EQ(ptr->name,s) ) return( TRUE );

    return( FALSE );
}

char *add_prereq(head,nam,f)
char *head,*nam;
struct m_preq *f;
{
char *stradd();
struct llist *ptr;
    /*
       move the stem, src and dest extensions and the current time
       of modification to where make() can get at them. returns the
       updated list of prerequisites
    */

    /* get the prerequisite */

    /* if ext does not exist, return */
    my_strcpy(f->m_name,nam);
    get_ext(f->m_name,f->m_targ);
    if ( f->m_targ[0] == NUL ) return( head );

    /* if ext not on suffix list, return */
    for ( ptr = suff_head; ptr != NULL; ptr = ptr->next )
	if ( EQ(ptr->name,f->m_targ) ) break;
    if ( ptr == NULL ) return( head );

    /* if there does not exist a corresponding file, return */
    for ( ; ptr != NULL; ptr = ptr->next )
	if ( exists(f->m_name,ptr->name) ) break;
    if ( ptr == NULL ) return( head );

    /* add the source file to the string m_comp with a strcat */
    my_strcpy(f->m_dep,ptr->name);
    return( stradd(head,f->m_name,f->m_dep) );

}

char *stradd(f1,f2,f3)
char *f1,*f2,*f3;
{
char *ptr;
    /* return a new pointer to a string containing the three strings */
    ptr = get_mem((UI)(my_strlen(f1) + strlen(f2) + strlen(f3) + 2) );
    my_strcpy(ptr,f1);
    strcat(ptr," ");
    strcat(ptr,f2);
    strcat(ptr,f3);
    if ( f1 != NULL ) free( f1 );
    return( ptr );
}


get_ext(n,ex)
char *ex,*n;
{
int start,end,x;
    /* 
       put the extension of n in ex ( with the period )
       and truncate name ( at period )
    */
    start = my_strlen(n);
    end = ( start > 6 ) ? start - 6 : 0;

    for ( x = start; x > end; x-- ) if ( n[x] == '.' ) break;

    if ( x == end ) {
	ex[0] = NUL;
	return;
    }
    else {
	my_strcpy(ex,n + x);
	n[x] = NUL;
    }
}
/* read the makefile, and generate the linked lists */

#define DONE 1
#define ADEFN 2
#define ARULE 3
#define AMACRO 4
#define DIRECTIVE 5

/* local variables */
FILE *fil;
char *fword,*restline,line[INMAX],backup[INMAX];
char *frule;
struct llist *fhowto,*howp3;
struct llist *fdeps,*deprec3;
struct defnrec *defnp;
int sending,def_ready,gdone,rule_send,rule_ready;
struct llist  *targ,*q_how,*q_dep,*targ2;

readmakefile(s)
char *s;
{
int i,k;
char temp[50],tempdep[50];
  FILE *fopen();

    /* open the file */
    if ( EQ(s,"-") ) fil = stdin;
    else if ( (fil = fopen(s,"r")) == NULL) {
	error2("couldn't open %s",s);
	return;
    }

    /* initialise getnxt() */
    sending = def_ready = gdone = rule_send = rule_ready = FALSE;
    targ = q_how = q_dep = NULL;
    if (getline(fil, backup) == FALSE)
       panic("Empty Makefile");


    /* getnxt() parses the next line, and returns the parts */
    while (TRUE) switch( getnxt() ){

    case DONE: fclose(fil);return;

    case AMACRO:      /* add a macro to the list */
	 add_macro(fword,restline);
	 break;

    case DIRECTIVE: /* perform the actions specified for a directive */
	 squeezesp(temp,fword);
	 if ( EQ(temp,"PATH") ) {
	     if ( my_strlen(restline)  == 0 )
		 {free_list(path_head);path_head = NULL;path_set = TRUE;}
	     else  {
		 /* this fooling around is because the switchchar may
		    not be set yet, and the PATH variable will use it
		 */
		 if ( !path_set ) {mkpathlist();path_set = TRUE;}
		 add_path(restline);
	     }
	 }
	 else if ( EQ(temp,"SUFFIXES") ) {
	     /* this is easier, suffix syntax characters don't change */
	     if ( my_strlen(restline) == 0 )
		 {free_list(suff_head);suff_head = NULL;}
	     else  add_suff(restline);
	 }
	 else if ( EQ(temp,"IGNORE") ) stopOnErr = FALSE;
	 else if ( EQ(temp,"SWITCH") )
	    switchc = (isnull(restline[0])) ? switchc : restline[0];
	 else if ( EQ(temp,"LINECONT") )
	    linecont = ( isnull( restline[0] ) ) ? linecont : restline[0];
	 else if ( EQ(temp,"SILENT") ) silentf = TRUE;
#ifdef LC	 
	 else if ( EQ(temp,"LINKER") ) {
		 if ( my_strlen(restline) == 0 ) linkerf = TRUE;
		 else switch (restline[0]) {
			 case 'f': case 'F':
				 linkerf = FALSE;
			 case 't': case 'T':
				 linkerf = TRUE;
			 default:
				 panic("Bad argument to LINKER (TRUE/FALSE)");
		 }
	 }
	 else if ( EQ(temp,"MACFILE") ) {
		 if ( my_strlen(restline) == 0 ) {
			 warn2("no MACFILE name, defaulting to %s",tfilename);
		 }
		 else my_strcpy(tfilename,restline);
	 }
#endif
     else {
	 error2("unknown directive \(rule?\) '%s'",temp);
     }
     break;


    case ARULE:      /* add a rule to the list */

	/*fword[0] is defined to be a period*/
	for (i=1; notnull(fword[i]) ;i++) if ( fword[i] == '.' ) break;

	if ( i == my_strlen(fword) ) {
	    panic2("Bad rule '%s'",fword);
	    /* NOTREACHED */
	}

	fword[i] = NUL; /* dep */
	my_strcpy(tempdep,fword);

	/* be sure object extension has no spaces */
	for ( k = i+1 ; notnull(fword[k]) && !isspace(fword[k]) ; k++ );

	if ( isspace(fword[k]) ) fword[k] = NUL;
	my_strcpy(temp,".");
	strcat(temp,fword + i + 1); /* targ */
	add_rule2(tempdep, temp, fhowto, FALSE);
	/*
	  update the suffix list if required. To get fancier than this,
	  the use has to do it himself. Start at the beginning, and
	  if not present, add to the end.
	*/
	add_s_suff(temp);    /* order is important -- add targ first **/
	add_s_suff(tempdep); /* then dep */
	break;

    case ADEFN:     /* add a definition */

	if (no_file) {      /*if no target specified on command line... */
	    dolist = add_llist(dolist,fword);  /*push target on to-do list */
	    no_file = FALSE;
	}
	/* getnxt() returns target ( fword ) , pointer to expanded howto
	   list ( fhowto ) and pointer to expanded depends ( fdeps )
	*/
	if ( defnlist == NULL ) {
	    /* add the current definition to the end */
	    add_defn(fword,FALSE,getmodified(fword),fdeps,fhowto);
	}
	else {
	    defnp = defnlist;
	    while ( defnp != NULL ) {
		if ( EQ(defnp->name,fword) ) break;
		else defnp = defnp->nextdefn;
	    }
	    if ( defnp == NULL ) {
		/* target not currently in list */
		add_defn(fword,FALSE,getmodified(fword),fdeps,fhowto);
	    }
	    else {
		/* target is on list, add new depends and howtos */
		if (defnp->dependson == NULL)
		    defnp->dependson = fdeps;
		else {
		    deprec3 = defnp->dependson;
		    while (deprec3->next != NULL)
			 deprec3 = deprec3->next;
		    deprec3->next = fdeps;
		}
		/* add new howtos */
		if (defnp->howto == NULL)
		    defnp->howto = fhowto;
		else {
		    howp3 = defnp->howto;
		    while ( howp3->next != NULL)
			 howp3 = howp3->next;
		    howp3->next = fhowto;
		}
	    }
	}
	break;
    }
}

add_defn(n,u,m,d,h)     /* add a new definition */
char *n;   /* name */
int   u;   /* uptodate */
TIME  m;   /* time of modification */
struct llist *d,*h;  /* pointers to depends, howto */
{
struct defnrec *ptr,*ptr2;
    ptr = (struct defnrec *)get_mem(sizeof(struct defnrec));
    ptr->name = mov_in(n);
    ptr->uptodate = u;
    ptr->modified = m;
    ptr->dependson = d;
    ptr->howto = h;
    ptr->nextdefn = NULL;
    if ( defnlist == NULL ) {
	defnlist = ptr;
	return;
    }
    else {
	ptr2 = defnlist;
	while ( ptr2->nextdefn != NULL )
	    ptr2 = ptr2->nextdefn;
	ptr2->nextdefn = ptr;
    }
}

getnxt()
{
int pos,mark,parsed,x;
char exp_line[INMAX];
struct llist *q_how2,*q_how3;

  while ( TRUE ) {

   /* if we are currently sending targets */
   if ( sending ) {
	if ( targ2->next == NULL) {
	    sending = def_ready = FALSE;
	}
	fword = mov_in(targ2->name);
	fhowto = mkexphow(q_how,targ2->name,REPT_ERR);
	fdeps = mkexpdep(q_dep,targ2->name);
	targ2 = targ2->next;
	return ( ADEFN );
    }
    
    /* are we sending a rule? */
    if ( rule_send ) {
	fword = frule;
	fhowto = mkexphow(q_how,(char *)NULL,IGN_ERR); /* target == NULL -> don't expand */
	rule_send = rule_ready = FALSE;
	return( ARULE );
    }
    
    if ( gdone ) return ( DONE );
    /* if we are not currently sending... */

    /* load the next line into 'line' ( which may be backed-up ) */
    if ( backup[0] != NUL ) {
	my_strcpy(line,backup);
	backup[0] = NUL;
    }
    else {
	if ( getline(fil,line) == FALSE ) {
	    if ( def_ready ) sending = TRUE;
	    if ( rule_ready ) rule_send = TRUE;
	    gdone = TRUE;
	    continue;  /* break the loop, and flush any definitions */
	}
    }
    parsed = FALSE;

    /* check for rule or directive, and begin loading it if there */
    if (line[0] == '.'){
	for (pos = 0 ; line[pos] != ':' && notnull(line[pos]) ; pos++) ;

	if (isnull(line[pos]))
	   error2("bad rule or directive, needs colon separator:\n%s",line);
	mark = pos;
	for ( x = 1 ; x < mark ; x++ ) if ( line[x] == '.' ) break;
	if ( x == mark ) {
	    /* found DIRECTIVE -- .XXXXX: */
	    line[mark] = NUL;
	    fword = line + 1;
	    for ( x++ ; isspace(line[x]) ; x++ ) ;
	    restline = line + x ;
	    return( DIRECTIVE );
	}
	else {
	    /* found RULE -- .XX.XXX: */
	    parsed = TRUE;
	    if ( rule_ready || def_ready ) {
		if ( def_ready ) sending = TRUE;
		else rule_send = TRUE;
		/* push this line, and send what we already have */
		my_strcpy(backup,line);
	    }
	    else {
		/* begin a new rule */
		rule_ready = TRUE;
		line[mark] = NUL;
		frule = mov_in(line);
		free_list(q_how);
		/*
		  one last decision to make. If next non-space char is
		  ';', then the first howto of the rule follows, unless there
		  is a '#', in which case we ignore the comment
		*/
		for ( pos++ ; line[pos] != ';' && notnull(line[pos]) ; pos++)
		    if ( !isspace(line[pos]) ) break;
		if ( notnull(line[pos]) ) {
		    if ( line[pos] == '#' ) ; /* do nothing, fall thru */
		    else if ( line[pos] != ';' )
			/* found non-<ws>, non-';' after rule declaration */
			error("rule needs ';<rule>' or <newline> after ':'");
		    else {
			/* found <rule>:; <rule_body> */
			q_how = MkListMem();
			q_how->name = mov_in(line + pos + 1 );
			q_how->next = NULL;
		    }
		}
	    }
	}
    }
    
    /* check for macro, and return it if there */
    if ( !parsed ) {
	pos = 0;
	while ( line[pos] != '=' &&
		line[pos] != ':' &&
		!isspace(line[pos]) &&
		notnull(line[pos])) pos++;
	mark = pos;
	if (notnull(line[pos]) && line[pos] != ':'){
	    /* found a macro */
	    if (isspace(line[pos]))
	       while (isspace(line[pos]) && notnull(line[pos])) pos++;
	    if (isnull(line[pos]))
	       panic2("bad macro or definition '%s'",line);
	    if ( line[pos] == '=' ) {
		/* found a macro */
		line[mark] = NUL;
		fword = line;
		mark = pos + 1;
		while ( isspace(line[mark]) ) mark++; /* skip spaces before macro starts */
		restline = line + mark;
		return ( AMACRO );
	    }
	}
    }



    /* check for and add howto line */
    if ( isspace(line[0]) ) {
	if (!def_ready && !rule_ready)
	 error2("how-to line without preceeding definition or rule\n%s",line);
	q_how2 = MkListMem();
	if ( q_how == NULL ) {
	    q_how = q_how2;
	}
	else {
	   for (q_how3 = q_how; q_how3->next != NULL; q_how3 = q_how3->next); /* go to end of list */
	   q_how3->next = q_how2;
	}
	q_how2->name = mov_in(line);
	q_how2->next = NULL;
	parsed = TRUE;
    }

    
    /* check for definition */
    if (!parsed) {
	pos = 0;
	while ( notnull(line[pos]) && line[pos] != ':') pos++;
	if (line[pos] == ':') {
	    /* found a definition */
	    parsed = TRUE;
	    if (def_ready || rule_ready) {
		if ( def_ready ) sending = TRUE;
		else rule_send = TRUE;
		my_strcpy(backup,line);
	    }
	    else {
	       /* free up the space used by the previous lists */
	       free_list(targ);targ = NULL;
	       free_list(q_how);q_how = NULL;
	       free_list(q_dep);q_dep = NULL;
	       line[pos] = NUL;
	       expand(line,exp_line,"",NO_TARG);
	       targ2 = targ = mkllist(exp_line);
	       q_dep = mkllist(line + pos + 1);
	       def_ready = TRUE;
	    }
	}
    }
    if (!parsed)
       panic2("unable to parse line '%s'",line);
  }
}

    
/*
   load the next line from 'stream' to 'where' allowing for comment char
   and line continuations 
*/
getline(stream,where)
char *where;
FILE *stream;
{
int i;

    if (get_stripped_line(where,INMAX,stream) == TRUE) {
	i = my_strlen(where);
	where[--i] = NUL;
	while ( where[i-1] == linecont ) {
	    if ( get_stripped_line(where + i -1,INMAX,stream) == FALSE )
		panic("end of file before end of line");
	    i = my_strlen(where);
	    where[--i] = NUL;
	}
	if ( i >= INMAX ) {
	    where[INMAX] = NUL;
	    panic2("line too long\n'%s'",where);
	}
	return ( TRUE );
    }
    else  return ( FALSE );
}

get_stripped_line(where,len,stream)
char *where;
int len;
FILE *stream;
{
int x;
    /* return a meaningful line */
    while ( TRUE ) {
       if ( fgets(where,len,stream) == NULL ) return( FALSE );
       
       if ( where[my_strlen(where)-1] != '\n' ) {
	       x = my_strlen(where);
	       where[x] = '\n';
	       where[x+1] = NUL;
       }

       /* terminate standard input with a period alone */
       if ( (stream == stdin) && EQ(where,".\n") ) return( FALSE );
    
       /* if the line is only <ws> or <ws>#, skip it */
       for ( x = 0; isspace(where[x]) && (where[x] != '\n') ; x++) ;
       if ( (where[x] == '\n') || (where[x] == '#') ) continue;

       /* no reason to throw it out... */
       return( TRUE );
    }
    /* NOTREACHED */
}

struct llist *mkllist( s )  /* make a  linked list */
char *s;
{
int pos;
char lname[INMAX];
struct llist *ptr,*ptr2,*retval;

     pos = 0;
     retval = NULL;
     while ( TRUE ) {
	 /* get the next element, may have quotes */
	 pos = get_element(s,pos,lname);
	 if ( isnull(lname[0]) ) return( retval );

	 /* found something to add */
	 ptr = MkListMem();
	 if ( retval == NULL ) retval = ptr;
	 else {
	     for (ptr2 = retval; ptr2->next != NULL ; ptr2 = ptr2->next ) ;
	     ptr2->next = ptr;
	 }
	 ptr->name = mov_in(lname);
	 ptr->next = NULL;
     }
}

get_element(src,p,dest)
char *src,*dest;
int p;
{
int i,quotestop;

    i = 0;
    dest[0] = NUL;
    while ( notnull(src[p]) && isspace(src[p]) ) p++;
    if ( isnull(src[p]) ) return( p );

    if ( src[p] == '"' ) {
	quotestop = TRUE;
	p++;
    }
    else quotestop = FALSE;

    while ( TRUE ) {
	if ( isnull(src[p]) ) break;
	else if (src[p] == BKSLSH) {
	    if (src[p+1] == '"') p++;
	    dest[i++] = src[p++];
	}
	else if ( !quotestop && isspace(src[p]) ) break;
	else if (  quotestop && (src[p] == '"') ) break;
	else dest[i++] = src[p++];
    }
    dest[i] = NUL;
    return( p );
}


struct llist *mkexphow(head,target,eflag) /* make an expanded linked list for how */
struct llist *head;
char *target;
int eflag;
{
struct llist *p,*p2,*retval;
int x;
char temp[INMAX];

   if ( head == NULL ) {
       return ( NULL );
   }

   retval = NULL;
   while ( head != NULL ) {
      
      if ( target != NULL ) expand(head->name,temp,target,eflag);
      else my_strcpy(temp,head->name);
      p = MkListMem();
      for ( x = 0 ; notnull(temp[x]) ; x++ ) if ( !isspace(temp[x]) ) break;
      p->name = mov_in(temp + x);
      p->next = NULL;

      if ( retval == NULL ) retval = p;
      else {
	  p2 = retval;
	  while ( p2->next != NULL )
	      p2 = p2->next;
	  p2->next = p;
      }
      head = head->next;
   }
   return( retval );
}

struct llist *mkexpdep(head,target)  /* make an expanded linked list for dep*/
struct llist *head;
char *target;
{
struct llist *p,*p2,*p3,*retval;
char temp[INMAX];

 if ( head == NULL ) {
      return ( NULL );
 }

 retval = NULL;
 while ( head != NULL ) {
      
   expand(head->name,temp,target, REPT_ERR);
   p3 = mkllist(temp);
   while ( p3 != NULL ) {
      p = MkListMem();      
      p->name = mov_in(p3->name);
      p->next = NULL;

      if ( retval == NULL ) retval = p;
      else {
	  p2 = retval;
	  while ( p2->next != NULL ) p2 = p2->next;
	  p2->next = p;
      }
      p3 = p3->next;
   }
   free_list(p3);
   head = head->next;
 }
 return( retval );
}


add_suff(lin)
char *lin;
{
struct llist *ptr;

    /* add *lin to the list suff_head */

    if ( lin == NULL ) return;

    if ( suff_head == NULL ) suff_head = mkllist(lin);
    else {
	/* go to the tail of suff_head */
	for ( ptr = suff_head; ptr->next != NULL; ptr = ptr->next );
	ptr->next = mkllist(lin);
    }

    /* do a little error checking */
    for ( ptr = suff_head; ptr != NULL; ptr = ptr->next )
       if ( ptr->name[0] != '.' )
	   error2("add_suffix: bad syntax '%s'",ptr->name);
}

add_s_suff(lext)
char *lext;
{
struct llist *sptr;

    /* add this extension to suff_list, if its not already there */

    for ( sptr = suff_head; sptr != NULL; sptr = sptr->next )
	if ( EQ(sptr->name,lext) ) return;
    /* must not be there... */
    suff_head = add_llist(suff_head,lext);
}


add_macro(mname,expan)
char *mname,*expan;
{
struct macrec *macp,*macp2;
    
    if (maclist == NULL)
	maclist = macp = (struct macrec *)get_mem((UI) sizeof(struct macrec));
    else {
	macp2 = maclist;
	while (macp2->nextmac != NULL) {
	    if ( EQ(macp2->name,mname) ) {
		macp2->mexpand = mov_in(expan);
		/* previous contents not freed cause mostly they were not
		   malloc()-ed
		*/
		return;
	    }
	    macp2 = macp2->nextmac;
	}
	if ( EQ(macp2->name,mname) ) {
	    /* if the last on the list matches,
	       replace it
	    */
	    macp2->mexpand = mov_in(expan);
	    return;
	}
	macp2->nextmac = macp =
			 (struct macrec *)get_mem((UI) sizeof(struct macrec));
    }
    macp->name = mov_in(mname);
    macp->mexpand = mov_in(expan);
    macp->nextmac = NULL;
}

add_rule2(adep,atarg,arule,aflag)
char *adep,*atarg;
struct llist *arule;
int aflag;
{
struct rulerec *rulep,*rulep2;
    
    if (rulelist == NULL)
	rulelist = rulep = 
	    (struct rulerec *)get_mem((UI) sizeof(struct rulerec));
    else {
	rulep2 = rulelist;
	while (rulep2->nextrule != NULL) {
	    if ( EQ(rulep2->dep,adep) && EQ(rulep2->targ,atarg) ) {
		free_list(rulep2->rule);
		rulep2->rule = arule;
		return;
	    }
	    rulep2 = rulep2->nextrule;
	}
	if ( EQ(rulep2->dep,adep) && EQ(rulep2->targ,atarg) ) {
	    free_list(rulep2->rule);
	    rulep2->rule = arule;
	    return;
	}
	rulep2->nextrule = rulep =
		 (struct rulerec *)get_mem((UI) sizeof(struct rulerec));
    }
    rulep->dep = mov_in(adep);
    rulep->targ = mov_in(atarg);
    rulep->rule = arule;
    rulep->def_flag = aflag;
    rulep->nextrule = NULL;
}

free_list(head)   /* kill a linked list */
struct llist *head;
{
struct llist *ptr;

    if ( head == NULL ) return;
    else if ( head->next == NULL ) {
	free( head->name );
	free( (char *)head );
	return;
    }
    else {
	while ( TRUE ) {
	    for ( ptr = head; ptr->next->next != NULL; ptr = ptr->next ) ;
	    free(ptr->next->name);
	    free((char *)ptr->next);
	    ptr->next = NULL;
	    if ( ptr == head ) {
		free( ptr->name );
		free( (char *)ptr);
		return;
	    }
	}
    }
}

exec_how(cmd)
char *cmd;
{
int pos,this_echo,this_ign,x,i,no_more_flags;
int err_ret;
char cmdname[INMAXSH];

     i = pos = 0;
     this_echo = !silentf;
     this_ign = FALSE;
     no_more_flags = FALSE;
     while ( TRUE ) {
	 while ( isspace( cmd[pos] ) ) pos++;
	 switch ( cmd[pos] ) {
	     case '@':this_echo = FALSE;break;

	     case '-':this_ign = TRUE;break;
	 
	     default: no_more_flags = TRUE;break;
	 }
	 if (no_more_flags) break;
	 else pos++;
     }

     /* get the name of the command */
     for (x=pos; !isspace(cmd[x]) && notnull(cmd[x]); x++) 
	 cmdname[i++] = cmd[x];  
     cmdname[i] = NUL;  

     /* echo if appropriate */
     if ( this_echo )  {
	fprintf(stdout,"        %s\n",cmd+pos);
     }

     else if ( !execute  && !this_echo) {
	 fprintf(stdout,"        %s\n",cmd+pos);
	 return(0);
     }

     /* if we are not to actually do it... */
     if ( !execute ) return(0);
 
#ifdef LC
     else if( EQ(cmdname,"write-macro") || EQ(cmdname,"WRITE-MACRO") ) {
         err_ret = w_macros(cmd+x);
         return( (this_ign) ? 0 : err_ret);
     }
#endif     
     
     else {
	 err_ret = perform( cmdname, cmd+pos);
	 return(  (this_ign) ? 0 : err_ret );
     }
}

perform(cname, syscmd)
char *cname;     /* the name of the command */
char *syscmd;   /* the string to send to 'system' */
{
int x,ccode;
#ifndef LC
int pid;
WAIT status;
#endif
struct llist *largs;
char  **vargs,**mkargs();
char wholenam[INMAXSH];


    /* if there are metacharacters, use 'system' */
    for ( x= 0; notnull(syscmd[x]); x++ ) 
	if ( (syscmd[x] == '>') ||
	     (syscmd[x] == '<') ||
	     (syscmd[x] == '|') ||
	     (syscmd[x] == '*') ||
	     (syscmd[x] == '?') ||
	     (syscmd[x] == '&')
	   ) {
		return( mysystem(syscmd) );
    }

    /* is this a builtin command? */
    if ( findexec(cname,wholenam) == (TIME) 0 ) {
	/* file doesn't exist -- yes */
	return(mysystem(syscmd));
    }

    /* directly exec a file with args */
    largs = mkllist(syscmd);
    vargs = mkargs(largs);
    
#ifndef LC

    if ( (pid = fork()) == 0 ) {
	execv(wholenam,vargs);
	done( -1 );
    }
    free( (char *)vargs );
    free_list( largs );

    if ( pid < 0 ) {
	perror(whoami);
	return( -1 );
    }
    else {
	while( ((ccode = wait(&status)) != pid) && (ccode != -1) );
	if ( pid < 0 ) {
	    perror(whoami);
	    return( -1 );
	}
	return( pr_warning(&status) );
    }
#else
    if ( forkv(wholenam,vargs) != 0 ) {
	    perror(whoami);
	    panicstop();
    }
    free( (char *)vargs );
    free_list( largs );
    ccode = wait();
    return( pr_warning(&ccode) );
#endif    
	    
}

#ifndef LC
mysystem(cmd)   /* use the SHELL to execute a command line, print warnings */
char *cmd;
{
    int ccode,pid;
    WAIT status;

    if ( (pid = fork()) == 0 ) {
	execl(SHELL,"sh","-c",cmd,0);
	done( -1 );
    }
    if ( pid < 0 ) {  
	perror(whoami);     /* say why we couldn't fork */
	return( -1 );
    }
    else {
	while ( ((ccode = wait(&status)) != pid) && (ccode != -1));
	if ( pid < 0 ) {     /* no children? */
	    perror(whoami);
	    return( -1 );
	}
	return( pr_warning(&status) );
    }
}
#else
mysystem(cmd)
char *cmd;
{
extern int _oserr;

    if ( system(cmd) != 0 ) {
	    if ( _oserr == 0 ) panic("Can't process system() args");
	    else panic("error calling system()");
    }
    return( pr_warning(wait()) );
}
#endif

pr_warning(s)    /* print a warning, if any */
WAIT *s;
{

#ifdef BSD4.2
    if ( (s->w_T.w_Termsig == 0) && (s->w_T.w_Retcode == 0) ) return( 0 );
    else {

	   if ( s->w_T.w_Termsig )
	       fprintf(stderr,"%s: received signal %x\n",whoami,s->w_T.w_Termsig);
	   else {
		   fprintf(stderr,"%s: Error code %x",whoami,s->w_T.w_Retcode);
		   fprintf(stderr,"%s\n",( stopOnErr ) ? "" : " (ignored)");
	   }
	   return( -1 );
    }
}
#else
    if ( *s == 0 ) return( 0 );
    else {


	fprintf(stderr,"%s:",whoami);
	if ( *s & 0xFF) {
	    fprintf(stderr," received signal %x\n",*s & 0xFF);
	}
	else {
	    fprintf(stderr," Error code %x",(*s & ~0xFF) >> 8);
		fprintf(stderr,"%s\n",( stopOnErr ) ? "" : " (ignored)");
	}
	return( -1 );
    }
}
#endif


char **mkargs(arglist)
struct llist *arglist;
{
    struct llist *ptr;
    char **retval;
    int i;

    /* count up the number of args */
    for ( i=0,ptr = arglist; ptr != NULL; ptr = ptr->next,i++);

    retval = (char **)get_mem(((UI)(i+1)*sizeof(char *)));
    for ( i=0,ptr=arglist; ptr != NULL; ptr = ptr->next,i++)
	retval[i] = ptr->name;
    retval[i] = NULL;
    return( retval );
}

char *get_mem(size)
UI size;
{
char *p,*malloc();

    if ((p = malloc(size)) == 0)
	panic("Ran out of memory");
    return(p);
}

struct llist *MkListMem()
{
struct llist *p;
char *malloc();

    if ((p = (struct llist *)malloc(sizeof(struct llist))) == 0 )
	panic("Ran out of memory");
    return(p);
}

char *mov_in(string) /* return pointer to fresh memory with copy of string*/
char *string;
{
char   *ptr;
    
       ptr = get_mem((UI)(my_strlen(string) + 1));
       my_strcpy(ptr,string);
       return(ptr);
}

mkpathlist()
{
char *getenv();

    /* go get the PATH variable, and turn it into a linked list */
    char *path;

    path_head = NULL;

    path = getenv("PATH");
    add_path(path);

}

squeezesp(to,from)
char *to,*from;
{
    /* copy from from to to, squeezing out any spaces */
    if ( from == NULL ) return;
    while( *from ) {
       if ( isspace(*from) ) from++;
       else *to++ = *from++;
    }
    *to = NUL;
}

TIME findexec(s,where)
char *s,*where;
{
int i;
TIME retval;
struct llist *ptr;

    my_strcpy(where,s);

    /* search for switch character, if present, then this is full name
       and we won't search the path
    */
    for ( i = 0; notnull(s[i]); i++)
	if ( s[i] == switchc ) return(getmodified(where));

    if ( (retval=getmodified(where)) != (TIME) 0 ) return( retval );

    /* if there is no prefix to this file name */
    for ( ptr = path_head; ptr != NULL; ptr = ptr->next) {
	my_strcpy( where, ptr->name);
	strcat( where, s);
	if ( (retval=getmodified(where)) != (TIME) 0 ) return( retval );
    }

    return( (TIME)0 );  /* didn't find it */
}

TIME getmodified(s)
char *s;
{
struct stat statb;

    if ( stat(s,&statb) != 0 ) {
	if ( errno == ENOENT ) return( (TIME) 0 );
	else {
	    perror(whoami);
	    if (stopOnErr) panicstop();
	    else return( (TIME) 0 );
	}
    }
    else {
        return( statb.st_mtime );
    }
    /* NOTREACHED */
}

add_path(p)
char *p;
{
char temp[50];
int i,k;
       
    /* add to the linked list path_head */
    k = i = 0;
    squeezesp(p,p);
    if ( p == NULL ) return;
    while ( TRUE ) {

	while ( notnull(p[k]) && (p[k] != PATHCHAR ) )
	    temp[i++] = p[k++];
	if ( temp[i-1] != switchc ) temp[i++] = switchc;
	temp[i] = NUL;
	if ( i == 0 ) return;

	path_head = add_llist(path_head,temp);
	if ( isnull(p[k]) ) {
	    return;
	}
	k++;
	i = 0;
    }
}

/* emulation of getenv() */
char *getenv(s)
char *s;
{
char **p,*tp,*ematch();

    p = ext_env;

    while ( *p != NULL ) {
	if ( (tp = ematch(s,*p)) != NULL ) return(mov_in(tp));
	p++;
    }
    return( NULL );
}

char *ematch(s,p)
char *s,*p;
{
    /* if match up to the '=', return second half */
    while( *s != NULL )
	if ( *s++ != *p++ ) return( NULL );
    if ( *p++ != '=' ) return( NULL );
    return( p );
}

#ifdef LC

#include <fcntl.h>

stat(st,ptr)
char *st;
struct stat *ptr;
{
int fd;
TIME getft(),retval;

	/* return 0 for success, -1 on failure */
	fd = open(st,O_RDONLY);
	if ( fd < 0 ) return( -1 );
	retval = getft(fd);
	if ( retval == (TIME)(-1) ) return( -1 );
	else {
	    ptr->st_mtime = retval;
	    return( 0 );
	}
}


#define W_PERLINE 4
#define W_BUFLEN 80
char w_buf[W_BUFLEN];
int w_count;
int w_first;

w_macros(list)
char *list;
{

    FILE *tfp;
    struct llist *ptr,*ptr2,*mkllist();

    if ( (tfp = fopen(tfilename,"w")) == NULL ) {
        warn2("Can't write to '%s'",tfilename);
        return( 1 );  /* non-zero is error */
    }

    w_buf[0] = NUL;
    w_count = 0;
    w_first = TRUE;

    ptr = mkllist(list);
    for ( ptr2 = ptr ; ptr2 != NULL ; ptr2 = ptr2->next )
        if ( w_mac2(ptr2->name,tfp,ptr2->next) != 0 ) return( 1 ); /* non-zero is error */
    free_list(ptr);
    if ( w_buf[0] != NUL ) fprintf(tfp,"%s\n",w_buf);
    fclose(tfp);
    return( 0 );
}


w_mac2(w_word,stream,n)
char *w_word;
FILE *stream;
struct llist *n;
{

    /* write to stream */
    if ( !linkerf ) {
        fprintf(stream,"%s\n",w_word);
        return( 0 );
    }
    else {
        if ( w_first ) {
            my_strcpy(w_buf,w_word);
            w_first = FALSE;
            w_count++;
        }
        else {
            strcat(w_buf," + ");
            if ( (my_strlen(w_buf) + strlen(w_word)) > W_BUFLEN ) {
                fprintf(stream,"%s\n",w_buf);
                w_buf[0] = NUL;
                w_count = 1;
            }
            strcat(w_buf,w_word);
            w_count++;
            if ( w_count >= W_PERLINE ) {
                w_count = 0;
                w_first = TRUE;
                fprintf(stream,"%s %c\n",w_buf,( n == NULL )?' ':'+');
                w_buf[0] = NUL;
            }
            else w_count++;
        }
        return( 0 );
    }
    /* NOTREACHED */
}

warn2(s1,s2)
char *s1,*s2;
{
    fprintf(stderr,"%s: ",whoami);
    fprintf(stderr,s1,s2);
    fprintf(stderr,"\n");
}

#endif

done(n)
int n;
{
  _cleanup();
  exit(n);
}

my_strlen (src)
char *src;
{
	if (src == NULL)
		return 0;
	return strlen (src);
}

my_strcpy (dest, src)
char *dest, *src;
{
	if (src == NULL)
		*dest = '\0';
	else
		return strcpy (dest, src);
}

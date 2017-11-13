/* libpack - pack ASCII assembly code	Author: Andy Tanenbaum */


#define BUFSIZ 20000


#define FS 1
#define WRITE 4
#define NILP (char *)0

char *table[] = {
"push ax",
"ret",
"mov bp,sp",
"push bp",
"pop bp",
"mov sp,bp",
".text",
"xor ax,ax",
"push 4(bp)",
"pop bx",
"pop si",
"cbw",
"movb al,(bx)",
"pop ax",
"xorb ah,ah",
"mov ax,#1",
"call _callm1",
"add sp,#16",
"mov bx,4(bp)",
"push 6(bp)",
"mov -2(bp),ax",
"I0013:",
"call .cuu",
"mov ax,-2(bp)",
"add 4(bp),#1",
"or ax,ax",
"jmp I0011",
"mov bx,8(bp)",
"push dx",
"mov cx,#2",
"mov bx,#2",
"I0011:",
"I0012:",
"push -2(bp)",
"mov ax,4(bp)",
"mov ax,-4(bp)",
"add sp,#6",
"and ax,#255",
"push bx",
"mov bx,-2(bp)",
"loop 2b",
"jcxz 1f",
".word 4112",
"mov ax,(bx)",
"mov -4(bp),ax",
"jmp I0013",
".data",
"mov bx,6(bp)",
"mov (bx),ax",
"je I0012",
".word 8224",
".bss",
"mov ax,#2",
"call _len",
"call _callx",
".word 28494",
".word 0",
"push -4(bp)",
"movb (bx),al",
"mov bx,ax",
"mov -2(bp),#0",
"I0016:",
".word 514",
".word 257",
"mov ",
"push ",
".word ",
"pop ",
"add ",
"4(bp)",
"-2(bp)",
"(bx)",
".define ",
".globl ",
"movb ",
"xor ",
"jmp ",
"cmp ",
"6(bp)",
"-4(bp)",
"-6(bp)",
"#16",
"_callm1",
"call ",
"8(bp)",
"xorb ",
"and ",
"sub ",
"-8(bp)",
"jne ",
".cuu",
"lea ",
"inc ",
"_M+10",
"#255",
"loop",
"jcxz",
"ax,#",
"bx,#",
"cx,#",
"ax,",
"bx,",
"cx,",
"dx,",
"si,",
"di,",
"bp,",
"ax",
"bx",
"cx",
"dx",
"si",
"di",
"bp",
"sp",
"dec ",
"neg ",
"_execve",
",#0",
0};

int bol = 1;
int white = 0;


#define MAX 128
/* This table is used to look up strings.  */
struct node {
  char *string;
  struct node *next;
} node[MAX];

struct node *hash[MAX];		/* hash table */


char input[BUFSIZ+2];
char xbuf[BUFSIZ+2];
main()
{
  int n, count, outbytes;
  char *p;

  hainit();
  while (1) {
	for (p = &input[0]; p < &input[BUFSIZ+1]; p++) *p = 0;
	n = read(0, input, BUFSIZ);
	if (n == BUFSIZ) {
		std_err("Input file too long\n");
		exit(1);
	}
	if (n <= 0) exit(0);
	outbytes = pack88(input, xbuf, n);
	write(1, xbuf, outbytes);
  }
}



int pack88(inp, outp, count)
register char *inp;
char *outp;
int count;
{
  /* Take a string and pack it to compact assembly code. */

  int k, hit, whchar, n;
  char *orig = outp;
  char *inbeg = inp;
  int ct;
  char *p, **ppt;
  struct node *nood;

  /* Convert all tabs to spaces. */
  p = inp;
  while (p - inbeg < count)  {
	if (*p == '\t') *p = ' ';
	p++;
  }

  /* Loop on every char in the buffer. */
  while (1) {
	/* Is the current string in the table? */
  	if (inp - inbeg > count) write(2, "Bug in packing algorithm\n", 25);
	if (inp - inbeg == count) return(outp - orig);

	/* Delete leading white space */
	whchar = (*inp == ' ' ? 1 : 0);
	if (bol && whchar) {
		inp++;
		continue;
	} else {
		bol = 0;
	}
	if (*inp == '\n') bol = 1;

	/* Delete comments */
	if (*inp == '|') {
		while (*inp++ != '\n') ;
		inp--;
		bol = 1;
	}

	/* Compact white space */
	if (white && whchar) {
		inp++;
		continue;
	}
	white = whchar;

	ppt = table;
	hit = 0;

	/* Compute hash value for inp. */
	n = (*inp + *(inp+1)) & 0177;
	nood = hash[n];

	while (nood != 0) {
		if (match(inp, nood->string)) {
			*outp++ = (char) (128 + (nood - node));
			inp += strlen(nood->string);
			hit++;
			break;
		}
		nood = nood->next;
	}
	if (hit == 0) *outp++ = *inp++;
  }
}


int match(s1, s2)
register char *s1, *s2;
{

  while (*s2 != 0) {
	if (*s1++ != *s2++) return(0);
  }
  return(1);
} 


hainit()
{
/* Initialize the hash tables. */

  int n, i, free;
  char *p;
  struct node *nood;

  free = 0;			/* next free slot in node table */
  for (i = 0; i < MAX; i++) {
  	p = table[i];
  	if (p == (char *) 0) return;
  	n = *p + *(p+1);
  	n = n & 0177;

  	/* Enter string i on hash slot n. */
  	if (hash[n] == (struct node *) 0) {
		hash[n] = &node[free];
	} else {
		/* Find the end of the chain. */
		nood = hash[n];
		while (nood->next != (struct node *) 0) nood = nood->next;
		nood->next = &node[free];
	}
  	node[free].string = p;
  	node[free].next = (struct node *) 0;
  	free++;
  }
}



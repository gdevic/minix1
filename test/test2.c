
extern errno;
int kk=0;

char buf[2048];
main()
{
  int i;

  printf("Test  2 ");
  for (i = 0; i<19; i++) {
	test20();
  }
  printf("ok\n");
  exit(0);
}


test20()
{
/* Test pipes */

int fd[2];
  int n, i, j, ij, q=0, nn, m=0, k;

  if (pipe(fd) <0) {printf("pipe error.  errno= %d\n", errno); exit(0);}

  i = fork();
  if (i <0) { printf("fork failed\n"); exit(0);}
  if (i != 0) {
	/* Parent code */
	close(fd[0]);
	for (i=0; i<2048; i++) buf[i] = i & 0377;
	for (q=0; q<8; q++) {
		if(write(fd[1], buf, 2048)<0){
			printf("write pipe err.  errno=%d\n",errno); 
			exit(0);
		}
	}
	close(fd[1]);
	wait(&q);
	if (q != 256*58) {printf("wrong exit code %d\n",q);  exit(0);}
 } else {
	/* Child code */
	close(fd[1]);
	for (q=0; q<32; q++) {
		n = read(fd[0], buf, 512);
		if (n!=512){printf("read yielded %d bytes, not 512\n",n); exit(0);}
		for (j=0; j<n; j++)
		  if (  (buf[j]&0377) != (kk& 0377)  ){
			printf("wrong data: %d %d %d \n ",j,buf[j]&0377,kk&0377);
		  } else {
			kk++;
		}
	}
	exit(58);
  }
}

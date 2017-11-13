char buf[30000];
main()
{
  int i, n;

  while(1) {
	n = read(0, buf,30000);
	for (i=0; i<n; i++)  if(buf[i] == 015){printf("DOS\n"); exit();}
	if (n < 30000) exit(0);
  }
}

/* clr - clear the screen		Author: Andy Tanenbaum */

main()
{
/* Clear the screen. */

  prints("\033 8\033~0");
  exit(0);
}

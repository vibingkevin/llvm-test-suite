/* Verify that calls to fprintf don't get eliminated even if their
   result on success can be computed at compile time (they can fail).
   The calls can still be transformed into those of other functions.
   { dg-skip-if "requires io" { freestanding } } */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef __OHOS__
#include <unistd.h>
#endif

int main (void)
{
#ifdef __OHOS__
  /* The device has no writable /tmp; use the test working directory. */
  char tmpfname[] = "fprintf-2.XXXXXX";
  int fd = mkstemp (tmpfname);
  if (fd < 0)
    abort ();
  close (fd);
#else
  char *tmpfname = tmpnam (0);
#endif
  FILE *f = fopen (tmpfname, "w");
  if (!f)
    {
      perror ("fopen for writing");
      return 1;
    }

  fprintf (f, "1");
  fprintf (f, "%c", '2');
  fprintf (f, "%c%c", '3', '4');
  fprintf (f, "%s", "5");
  fprintf (f, "%s%s", "6", "7");
  fprintf (f, "%i", 8);
  fprintf (f, "%.1s\n", "9x");
  fclose (f);

  f = fopen (tmpfname, "r");
  if (!f)
    {
      perror ("fopen for reading");
      remove (tmpfname);
      return 1;
    }

  char buf[12] = "";
  if (1 != fscanf (f, "%s", buf))
    {
      perror ("fscanf");
      fclose (f);
      remove (tmpfname);
      return 1;
    }

  fclose (f);
  remove (tmpfname);

  if (strcmp (buf, "123456789"))
    abort ();

  return 0;
}

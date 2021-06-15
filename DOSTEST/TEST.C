/*
 *
 * test.c
 * Thursday, 8/22/1996.
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <GnuType.h>
#include <GnuArg.h>
#include "wineblib.h"



int CCONV main (int argc, CHAR *argv[])
   {
   UINT   uRet, uRet2, uFlags;
   PSZ    pszLib, pszFileSpec;
   CHAR   szError [256];

   ArgBuildBlk ("? *^help *^FullHelp ^Examples ^a- ^d- ^l- ^v- ^t- ^x-"
                " ^y- ^n- ^i? ^z- ^s- ^h- ^c% ^e- ^m- ^r- ^p-");

   if (ArgFillBlk (argv))
       printf ("%s\n", ArgGetErr ());

   EblInit ();

   pszLib      = ArgGet (NULL, 0);
   pszFileSpec = ArgGet (NULL, 1);

   uFlags  = (ArgIs ("n") ? EBL_NOOVERWRITE : 0);
   uFlags |= (ArgIs ("s") ? EBL_SYSTEMFILES : 0);
   uFlags |= (ArgIs ("h") ? EBL_HIDDENFILES : 0);
   uFlags |= (ArgIs ("r") ? EBL_RECURSE     : 0);
   uFlags |= (ArgIs ("p") ? EBL_STRIPPATH   : 0);

   if (ArgIs ("l"))
      uRet = EblList (pszLib, pszFileSpec, "out.txt");
   else if (ArgIs ("v"))
      uRet = EblList (pszLib, pszFileSpec, "out.txt");
   else if (ArgIs ("t"))
      uRet = EblTest (pszLib);
   else if (ArgIs ("a"))
      uRet = EblAdd (pszLib, pszFileSpec, uFlags);
   else if (ArgIs ("m"))
      uRet = EblMove (pszLib, pszFileSpec, uFlags);
   else if (ArgIs ("d"))
      uRet = EblDelete (pszLib, pszFileSpec, uFlags);
   else if (ArgIs ("x") || ArgIs ("e"))
      uRet = EblExtract (pszLib, pszFileSpec, uFlags);
   else if (ArgIs ("i"))
      uRet = EblDescribe (pszLib, "in.txt");
   else
      uRet = EblList (pszLib, pszFileSpec, "out.txt");

   uRet2 = EblGetError (szError);
   if (uRet)
      printf ("Error is:[%d][%d]  %s\n", uRet, uRet2, szError);

   return 0;
   }


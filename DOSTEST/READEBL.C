/*
 * -----------------------------------------------------------------------
 *          Copyright (c) 1996 by AASHTO.  All Rights Reserved.
 *  
 *  This program module is part of EBS, the Electronic Bidding System, a 
 *  proprietary product of AASHTO, no part of which may be reproduced or 
 *  transmitted in any form or by any means, electronic, mechanical, or 
 *  otherwise, including photocopying and recording or in connection with 
 *  any information storage or retrieval system, without permission in 
 *  writing from AASHTO. 
 * -----------------------------------------------------------------------
 *
 * ReadEBL.c
 *
 *
 * Craig Fitzgerald
 *
 * This file provides the EbOpen function which is a superset of fopen
 * which additionally allows access to files in an EBLIB library.
 *
 */


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "GnuType.h"
#include "GnuMem.h"
#include "GnuFile.h"
#include "GnuMisc.h"
#include "ReadEBL.h"

#define LIBDESCRSIZE   2048

UINT uLIBERR  = 0;

CHAR szEBLPATH  [256];
CHAR szEBLLIB   [256];
CHAR szEBLFILE  [256];

extern PFDESC PFD = NULL;
extern PLDESC PLD = NULL;




/*************************************************************************/
/*                                                                       */
/*                                                                       */
/*                                                                       */
/*************************************************************************/


PSZ ERRSTR[] = { /* 0 */ "No Error",
                 /* 1 */ "Unexpected end of Input file",
                 /* 2 */ "Unexpected end of Output file",
                 /* 3 */ "File is corrupt",
                 /* 4 */ "Unknown Error",
                 /* 5 */ "File size error",
                 /* 6 */ "Unable to open file",
                 /* 7 */ "File not in library",
                 /* 8 */ "End of Library found",
                 /* 9 */ "File Not a EBL lib file",
                 /* 10*/ "Insufficient Memory",
                 /* 11*/ "",
                 /* 12*/ NULL};



PVOID SetLibErr (UINT i)
   {
   uLIBERR  = i;
   return NULL;
   }


UINT GetLibErr (void)
   {
   return uLIBERR;
   }




/*************************************************************************/
/*                                                                       */
/*                                                                       */
/*                                                                       */
/*************************************************************************/



BOOL ReadMark (FILE *fp)
   {
   ULONG ulTmp;

   ulTmp = FilReadULong (fp);

   if (feof (fp))
      SetLibErr (8);
   else if (ulTmp != EBLMARK)
      SetLibErr (3);
   return (ulTmp == EBLMARK);
   }

//PSZ FilReadStr (FILE *fp, PSZ psz)
//   {
//   PSZ p;
//   int c;
//
//   p = (psz ? psz: pszBuff);
//
//   while ((c = (CHAR)fgetc (fp)) && c != EOF)
//      *p++ = (CHAR) c;
//   *p = '\0';
//
//   if (!psz)
//      return strdup (pszBuff);
//   return psz;
//   }




CHAR szDate [32], szTime[32], szAtt[8];

PSZ DateStr (FDATE fDate)
   {
   sprintf (szDate, "%2.2d-%2.2d-%2.2d", fDate.month, fDate.day, 
                     (fDate.year + 80) % 100);
   return szDate;
   }


PSZ TimeStr (FTIME fTime)
   {
   sprintf (szTime, "%2d:%2.2d%cm",
                     (fTime.hours % 12 ? fTime.hours % 12 : 12),
                     fTime.minutes, (fTime.hours > 11 ? 'p' : 'a'));
   return szTime;
   }

PSZ AttStr (UINT uAtt)
   {
   sprintf (szAtt, "%c%c%c%c",(uAtt & 1  ? 'R' : '-'),
                              (uAtt & 2  ? 'H' : '-'),
                              (uAtt & 4  ? 'S' : '-'),
                              (uAtt & 32 ? 'A' : '-'));
   return szAtt;
   }


ULONG Ratio (ULONG ulSmall, ULONG ulBig)
   {
   if (!ulBig || !ulSmall)
      return 0L;

   return 100 - (ulSmall * 100 + ulSmall / 2) / ulBig;
   }






void SplitFile (PSZ pszIn, PSZ pszPath, PSZ pszLib, PSZ pszFile)
   {
   PSZ p;

   /*--- Extract Path info ---*/
   strcpy (pszPath, pszIn);
   if (p=strrchr (pszPath, '\\'))                /*-- full path --*/
      {
      p[1] = '\0';
      strcpy (pszLib, strrchr (pszIn, '\\') + 1);
      }
   else if (pszPath[1] == ':')                   /*-- drive only --*/
      {
      pszPath[2] = '\0';
      strcpy (pszLib, pszIn + 2);
      }
   else                                          /*-- no path --*/
      {
      *pszPath = '\0';
      strcpy (pszLib, pszIn);
      }

   /*--- Extract File Info ---*/
   if (p=strchr (pszLib, ':'))                   /*-- lib & file --*/
      {
      *p = '\0';
      strcpy (pszFile, p+1);

      if (!strchr (pszLib, '.'))
         strcat (pszLib, EXT);
      }
   else                                          /*-- file only --*/
      {
      strcpy (pszFile, pszLib);
      *pszLib = '\0';
      }
   }


/*************************************************************************/
/*                                                                       */
/*                                                                       */
/*                                                                       */
/*************************************************************************/

PVOID FreePLD (PLDESC pld)
   {
   if (pld->pszDesc) free (pld->pszDesc);
   free (pld);
   return NULL;
   }

PVOID FreePFD (PFDESC pfd)
   {
   free (pfd);
   return NULL;
   }


/*
 * this fn opens a lib file
 * fp is left at 1st file in lib
 */
PLDESC OpenLib (PSZ pszLib)
   {
   PLDESC pld = NULL;
   FILE   *fp;
   CHAR   szBuff [HEADERSIZE+1];

   if (!(fp = fopen (pszLib, "rb")))
      return SetLibErr (6);

   if (fread (szBuff, 1, HEADERSIZE, fp) != HEADERSIZE)
      {
      fclose (fp);
      return SetLibErr (1);
      }
   if (strncmp (szBuff, LIBHEADER, HEADERSIZE))
      {
      fclose (fp);
      return SetLibErr (3);
      }
   pld = malloc (sizeof (LDESC));
   pld->pszDesc = malloc (LIBDESCRSIZE);

   /*--- Read Library Volume Information ---*/
   pld->fp       = fp;
   pld->ulOffset = FilReadULong  (fp);
   pld->ulSize   = FilReadULong  (fp);
   pld->uCount   = FilReadUShort (fp);
   pld->uLibVer  = FilReadUShort (fp);
   pld->pszDesc  = FilReadStr   (fp, pld->pszDesc);

   /*--- set to position of first file in library ---*/
   fseek (fp, pld->ulOffset, SEEK_SET);

   return pld;
   }



/*
 * this fn reads info about the next file
 * the lib handle must be pointing to the file header
 * On return the file ptr points to the next file if bSkip=TRUE
 * or to the file data if bSkip=FALSE
 */
PFDESC ReadFileInfo (PLDESC pld, BOOL bSkipData)
   {
   USHORT  u;
   PUSHORT p = &u;
   PFDESC  pfd;

   if (!ReadMark (pld->fp))
      return NULL;

   pfd = malloc (sizeof (FDESC));
   pfd->pld = pld;

   pfd->ulOffset = FilReadULong  (pld->fp);
   pfd->ulLen    = FilReadULong  (pld->fp);
   pfd->ulSize   = FilReadULong  (pld->fp);
   pfd->ulCRC    = FilReadULong  (pld->fp);
   pfd->uMethod  = FilReadUShort (pld->fp);

   *p = FilReadUShort (pld->fp);
   pfd->fDate    = *((PFDATE)p);

   *p = FilReadUShort (pld->fp);
   pfd->fTime    = *((PFTIME)p);

   pfd->uAtt     = FilReadUShort (pld->fp);

   FilReadStr (pld->fp, pfd->szName);
   FilReadStr (pld->fp, pfd->szDesc);

   fseek (pld->fp, pfd->ulOffset + (bSkipData ? pfd->ulSize + EBLMARKSIZE : 0), SEEK_SET);

//   if (bDEBUGMODE)
//      {
//      printf ("\n            File Header\n");
//      printf ("=======================================\n");
//      printf ("   File Name: %s\n"  ,fdesc.szName);
//      printf (" Data Offset: %lu\n" ,fdesc.ulOffset);
//      printf ("   File Size: %lu\n" ,fdesc.ulSize);
//      printf ("    File Len: %lu\n" ,fdesc.ulLen);
//      printf ("    File CRC: %lu\n" ,fdesc.ulCRC);
//      printf (" Compression: %u\n"  ,fdesc.uMethod);
//      printf ("   File Date: %s\n"  , DateStr (fdesc.fDate));
//      printf ("   File Time: %s\n"  , TimeStr (fdesc.fTime));
//      printf ("   File Atts: %s\n"  , AttStr  (fdesc.uAtt));
//
//      printf (" Description: %s\n\n",fdesc.szDesc);

   return pfd;
   }



void SkipFileData (PFDESC pfd)
   {
   fseek (pfd->pld->fp, pfd->ulOffset + pfd->ulSize + EBLMARKSIZE, SEEK_SET);
   }



/*************************************************************************/
/*                                                                       */
/*                                                                       */
/*                                                                       */
/*************************************************************************/





PFDESC GetNextFile (PLDESC pld, PFDESC pfdPrev, BOOL bFreeOld)
   {
   PFDESC pfd = NULL;

   if (pfdPrev)
      {
      fseek (pld->fp, pfdPrev->ulOffset, SEEK_SET);
      SkipFileData (pfdPrev);
      if (bFreeOld)
         FreePFD (pfdPrev);
      }
   else
      fseek (pld->fp, pld->ulOffset, SEEK_SET);

   if (!(pfd = ReadFileInfo (pld, FALSE)))
         return NULL;

   if (!ReadMark (pld->fp))
      return NULL;

   return pfd;
   }




/*
 * [path][lib][:][file]
 * path: c: c:\ c:\dir\dir\ \dir\ dir\ <none>
 * lib   lib.exe lib
 *
 *
 * c:\libs\liba.ebl:file7.txt
 * c:liba.ebl:file7.txt
 *
 */
FILE *EbOpen (PSZ pszFile, PSZ pszMode)
   {
   UINT j;
   CHAR   szTmp[256];
   PLDESC pld;
   PFDESC pfd = NULL;

   /*--- Init Global Info ---*/
   PFD      = NULL;
   PLD      = NULL;
   uLIBERR  = 0;

   SplitFile (pszFile, szEBLPATH, szEBLLIB, szEBLFILE);

   if (!*szEBLLIB)
      return fopen (pszFile, pszMode);

   sprintf (szTmp, "%s%s", szEBLPATH, szEBLLIB);
   if (!(pld = OpenLib (szTmp)))
      return NULL;

   for (j=0; j<pld->uCount; j++)
      {
      if (!(pfd = GetNextFile (pld, pfd, TRUE)))
         {
         fclose (pld->fp);
         return NULL;
         }

      if (stricmp (szEBLFILE, pfd->szName))
         continue;

      /*--- set global ptrs ---*/
      PFD = pfd;
      PLD = pld;

      return pld->fp;
      }
   fclose (pld->fp);
   FreePLD (pld);
   return SetLibErr (7);
   }



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
 * EBLIB.c
 *
 *
 * Craig Fitzgerald
 *
 * This is the main file for the EBLib program.  This file provides the
 * functionality for handling library files.
 *
 *
 * At this point I must apologize for the sorry state of this file.
 * I'm Sorry!
 * This code has been modified and hacked so many times that it resembles
 * the innards of a 1960's tv set.
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <io.h>
#include <conio.h>
#include "GnuType.h"
#include "GnuMem.h"
#include "GnuStr.h"
#include "GnuFile.h"
#include "GnuZip.h"
#include "GnuMisc.h"
#include "GnuDir.h"
#include "ReadEBL.h"
#include "WinEbLib.h"

#define TMPLIB       "TMP--LIB.TMP"

#define BUFFERSIZE    35000U
#define MAXDESCLEN    2000U

#define LIB           1
#define CMDLINE       2
#define UPDATE        3
#define DELET         4
#define EXTRACT       5

#define INITCRC       12345L

PFDESC fList = NULL;
FDESC  fdesc;

int  iOVERWRITE = 0;

CHAR szLIB [256];
PSZ  pszWORKBUFF = NULL;

BOOL bSTOREONLY;
BOOL bINCLSYSTEM;
BOOL bINCLHIDDEN;
BOOL bLIBDESC;
BOOL bFILEDESC;
BOOL bRECURSE;
BOOL bSTRIPPATH;

FTIME ftimeZero = {0, 0, 0};
FDATE fdateZero = {0, 0, 0};

int GLOBALERROR = 0;


PSZ ppszEBL_ERRORS[] =
{ /* 0 */ "No Error",
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
  /* 11*/ "Cant Read",
  /* 12*/ "Cant Create File",
  /* 13*/ "File Exists",
  /* 14*/ "Cant make path",
  /* 15*/ NULL};





/*******************************************************************/
/*                                                                 */
/*                                                                 */
/*                                                                 */
/*******************************************************************/

static UINT SetErr (UINT uErr)
   {
   if (!uErr)
      GLOBALERROR = 0;
   else if (!GLOBALERROR)
      GLOBALERROR = uErr;
   return GLOBALERROR;
   }

static UINT GetErr (void)
   {
   return GLOBALERROR;
   }



static void WriteMark (FILE *fp)
   {
   FilWriteULong (fp, EBLMARK);
   }


static int NewStrLen (PSZ p)
   {
   if (!p)
      return 0;
   return strlen (p);
   }


/*
 * Strip off drive info, and all path or prefix path onfo 
 */
static PSZ SkipLeadingCrap (PSZ pszSpec, BOOL bSkipAllPathInfo)
   {
   PSZ psz, psz2;

   psz = ((psz2 = strrchr (pszSpec, ':')) ? psz2+1 : pszSpec);
   if (bSkipAllPathInfo)
      psz = ((psz2 = strrchr (psz, '\\')) ? psz2+1 : psz);
   else if (*psz == '\\')
      psz++;
   return psz;
   }


/*
 * Splits filespec into path and file
 *
 */
static PSZ SplitUp (PSZ pszPath, PSZ pszMatch, PSZ pszFullMatchSpec)
   {
   PSZ psz;

   strcpy (pszPath, pszFullMatchSpec);
   if ((psz = strrchr (pszPath, '\\')) || (psz = strchr (pszPath, ':')))
      {
      if (pszMatch)
         strcpy (pszMatch, psz+1);
      psz[1] = '\0';
      }
   else
      {
      *pszPath = '\0';
      if (pszMatch)
         strcpy (pszMatch, pszFullMatchSpec);
      }
   return pszPath;
   }


BOOL FileMatches (PSZ pszFile, PSZ pszMatchSpec)
   {
   if (!pszMatchSpec)
      return TRUE;  // assume *.* if not specified

   return StrMatches (pszFile, pszMatchSpec, FALSE);
   }


/*******************************************************************/
/*                                                                 */
/* Compression / Uncompression routines                            */
/*                                                                 */
/*******************************************************************/

//static ULONG ulSRCLEN, ulREADLEN;
//
//static void CCONV UpdateFn (UINT uIncSize, UINT uSoFar, UINT uTotalSize)
//   {
//   ULONG ulPct;
//
//   ulREADLEN += uIncSize;
//   ulPct = ulREADLEN * 100 / (ulSRCLEN + 1);
//   printf ("\b\b\b%2.2ld%%", ulPct);
//   }


/*
 * Here is what is going on:
 *   This fn accomplishes 2 tasks:
 *      1> copy data from 1 file to another (or NULL)
 *      2> generate a CRC value from the data
 *   Generating a CRC value of uncompressed data is simple, just
 *   get a cumulitave CRC value of each byte in the stream.
 *   Generating a CRC value of compressed data is not so simple
 *   beacuse is format is:
 *      CompressedSegmentLength  (2 bytes, call it n)
 *      CompressedSegment        (n-2 bytes)
 *      CompressedSegmentLength  (2 bytes, call it m)
 *      CompressedSegment        (m-2 bytes)
 *      .                        .
 *      .                        .
 *      .                        .
 *   We need to get a cumulative CRC of the CompressedSegment's only.
 *   Added to this, this fn buffers the input/output. This buffer
 *   area (of size BUFFERSIZE), is not necessarily as large as a
 *   CompressedSegment, so each segment may require more than 1 read
 *
 *   The reason for the complicated CRC on the compressed data is one
 *   of speed. The compression generates a CRC as it writes, but doesn't
 *   write the CompressedSegmentLength until the segment is written.
 *
 *   Why not generate a CRC of the data in uncompressed form you say?
 *   Again for Speed.  Using a crc for the compressed form greatly speeds
 *   up file validation, and allows verification of all files in a lib
 *   when the lib is being updated
 *
 * returns:
 *
 */
static UINT CopyFile (FILE *fpIn, FILE *fpOut, ULONG ulSize, UINT uCompression)
   {
   UINT  uPiece, uIOBytes;
   ULONG ulSegment;

   /*--- set up the update status display ---*/
//   ulSRCLEN  = ulSize;
//   ulREADLEN = 0;

   while (ulSize)
      {
      if (uCompression)
         {
         if ((ulSegment = (ULONG)FilReadUShort (fpIn)) > ulSize)
            return SetErr (EBLERR_BADSIZE);

         ulSize -= ulSegment;
         if (fpOut)
            FilWriteUShort (fpOut, (UINT)ulSegment);
         ulSegment -= 2;
         }
      else
         {
         ulSegment = ulSize;
         ulSize = 0;
         }
      while (ulSegment)
         {
         uPiece = (UINT) min ((ULONG)BUFFERSIZE, ulSegment);
         uIOBytes = fread (pszWORKBUFF, 1, uPiece, fpIn);
         if (uPiece != uIOBytes)
            return SetErr (EBLERR_INPUTEOF);

         if (fpOut)
            {
            uIOBytes = fwrite (pszWORKBUFF, 1, uPiece, fpOut);
            if (uPiece != uIOBytes)
               return SetErr (EBLERR_OUTPUTEOF);
            }
         ulSegment -= uPiece;

         if (Cmp2CRCEnabled (0))
            Cmp2SetCRC (CRC_BUFF (Cmp2GetCRC (0), pszWORKBUFF, uPiece), 0);
         if (Cmp2CRCEnabled (1))
            Cmp2SetCRC (CRC_BUFF (Cmp2GetCRC (1), pszWORKBUFF, uPiece), 1);
         }
      }
   return 0;
   }


/*
 * returns:
 */
static UINT UncompressFile (FILE *fpIn, FILE *fpOut, ULONG ulInSize, ULONG ulOutSize)
   {
   UINT uInSize, uOutSize;
   ULONG  ulSrcRead, ulTotalOut;

   ulSrcRead = ulTotalOut = 0;

   /*--- set up the update status display ---*/
//   ulSRCLEN  = ulInSize;
//   ulREADLEN = 0;

   while (ulSrcRead < ulInSize)
      {
      Cmp2fpEfp (fpOut, &uOutSize, fpIn, &uInSize);
      ulSrcRead  += uInSize;
      ulTotalOut += uOutSize;
      }

   if (ulTotalOut != ulOutSize)
      return SetErr (EBLERR_BADSIZE);
   return 0;
   }


/*
 * 
 */
static UINT CompressFile (FILE   *fpIn,           //
                          FILE   *fpOut,          //
                          ULONG  ulInSize,        //
                          PULONG pulOutSize,      //
                          BOOL   bExitIfBloating) //
   {
   UINT  uInSize, uOutSize, uChunkSize;
   ULONG ulTotalIn, ulTotalOut;
   BOOL  bOK;

   ulTotalIn = ulTotalOut = 0;

   /*--- set up the update status display ---*/
//   ulSRCLEN  = ulInSize;
//   ulREADLEN = 0;

   for (bOK=TRUE; ulTotalIn < ulInSize && bOK; )
      {
      uChunkSize = 0;
      if (ulTotalIn + MAXSAFEBLOCKSIZE > ulInSize)
         uChunkSize = (UINT)(ulInSize - ulTotalIn);

      Cmp2fpIfp (fpOut, &uOutSize, fpIn, uChunkSize, &uInSize);
//check err

      ulTotalIn  += uInSize;
      ulTotalOut += uOutSize;

      if (bExitIfBloating && (ulTotalOut > ulTotalIn))
         bOK = FALSE;
      }
   *pulOutSize = ulTotalOut;
   return (bOK ? GetErr () : 999);
   }



/*******************************************************************/
/*                                                                 */
/*                                                                 */
/*                                                                 */
/*******************************************************************/


/*
 * fp should point to the file data area
 * 0 - ok
 * # - error
 */
static UINT TestFile (PFDESC pfd)
   {
   UINT uRet;

   ReadMark (pfd->pld->fp);

   /*--- compression module vars ---*/
   Cmp2EnableCRC (TRUE,  TRUE);  // enable READ crc
   Cmp2EnableCRC (FALSE, FALSE); // disable Write crc
   Cmp2SetCRC (INITCRC, TRUE);   // init Read crc

   if (uRet = CopyFile (pfd->pld->fp, NULL, pfd->ulSize, pfd->uMethod))
      return uRet;
      
   if (Cmp2GetCRC (1) != pfd->ulCRC)
      return SetErr (EBLERR_CRCERROR);
   return 0;
   }



/*
 * This fn writes a file's data from a lib to a file
 * This fn assumes the file pointer is pointing
 * to the start of the file data area unless bSetFilePos 
 */
static UINT WriteToFile (PFDESC pfd, BOOL bSetFilePos)
   {
   FILE *fpOut;
   PSZ  pszName;
   CHAR szPath[255];
   UINT uRet;

   pszName = SkipLeadingCrap (pfd->szName, bSTRIPPATH);

   if (!access (pszName, 0) && (iOVERWRITE < 0))
      return SetErr (EBLERR_FILEEXISTS);

   SplitUp (szPath, NULL, pszName);
   if (*szPath && szPath[strlen(szPath)-1] == '\\')
      szPath[strlen(szPath)-1] = '\0';
      
   if (*szPath && access (szPath, 0))
      if (!DirMakePath (szPath))
         return SetErr (EBLERR_CANNOTMAKEPATH);

   if (!(fpOut = fopen (pszName, "wb")))
      return SetErr (EBLERR_CANTOPEN);

   if (bSetFilePos)
      fseek (pfd->pld->fp, pfd->ulOffset, SEEK_SET);

   ReadMark (pfd->pld->fp);

   /*--- compression module vars ---*/
   Cmp2EnableCRC (TRUE,  TRUE);  // enable READ crc
   Cmp2EnableCRC (FALSE, FALSE); // disable Write crc
   Cmp2SetCRC (INITCRC, TRUE);   // init Read crc

   if (pfd->uMethod)
      uRet = UncompressFile (pfd->pld->fp, fpOut, pfd->ulSize, pfd->ulLen);
   else
      uRet = CopyFile (pfd->pld->fp, fpOut, pfd->ulSize, pfd->uMethod);

   fclose (fpOut);

   if (!uRet && (Cmp2GetCRC (1) != pfd->ulCRC))
      uRet = SetErr (EBLERR_CRCERROR);

   /*--- set date / time ---*/
   DirTouch (pszName, pfd->fDate, pfd->fTime);

   /*--- set file mode ---*/
   DirSetFileAtt (pszName, pfd->uAtt);

   /*-- write 4dos descriptions --*/
   FilPut4DosDesc (pszName, pfd->szDesc);

   return uRet;
   }



/*
 * compares files
 * files w/o paths come first
 *
 * psz2 - if bSKIPPATH, ignore psz2's path
 */
static int CompareFileNames (PSZ psz1, PSZ psz2)
   {
   PSZ pszF1, pszF2;

   if (bSTRIPPATH)
      psz2 = SkipLeadingCrap (psz2, TRUE);

   psz1  = SkipLeadingCrap (psz1, FALSE);  // ignore leading drive info 
   psz2  = SkipLeadingCrap (psz2, FALSE);  // and possible '\'

   pszF1 = SkipLeadingCrap (psz1, TRUE);   // this skips all path info
   pszF2 = SkipLeadingCrap (psz2, TRUE);   //

   if ((pszF1 == psz1) && (pszF2 == psz2)) // Neither file has path info
      return stricmp (pszF1, pszF2);
   if (pszF1 == psz1)                      // 1st file no path. ie: it first
      return -1;
   if (pszF2 == psz2)                      // 2nd file no path. ie: it first
      return 1;

   return stricmp (psz1, psz2);            // Compare including path info
   }



/*
 * add files from old lib first
 * add cmd line files next
 *
 * MODES:
 *   LIB
 *   CMDLINE
 *   UPDATE
 */
static PVOID AddToFileList (PFDESC pfd)
   {
   PFDESC fTmp;
   int    i;

   pfd->Next = NULL;

   /*--- 1st node ? ---*/
   if (!fList)
      return (fList = pfd);

   /*--- before 1st node ? ---*/
   if ((i=CompareFileNames (fList->szName, pfd->szName))>0)
      {
      pfd->Next = fList;
      return (fList = pfd);
      }

   /*--- does it match the first node ? ---*/
   else if (!i)
      {
      if (fList->uMode == LIB)
         pfd->uMode = UPDATE;
      fTmp = fList;
      pfd->Next = fList->Next;
      fList = pfd;
      return MemFreeData (fTmp);
      }       

   for (fTmp = fList; fTmp; fTmp = fTmp->Next)
      {
      if (!fTmp->Next || (i = CompareFileNames (fTmp->Next->szName, pfd->szName))>0)
         {
         pfd->Next = fTmp->Next;
         fTmp->Next = pfd;
         break;
         }
      else if (!i)
         {
         if (fTmp->Next->uMode == LIB)
            pfd->uMode = UPDATE;
         pfd->Next = fTmp->Next->Next;
         free (fTmp->Next);
         fTmp->Next = pfd;
         break;
         }
      }
   return NULL;
   }



/*
 *
 *
 *
 */
static void WriteFileHeader (FILE *fpOut, PFDESC pfd)
   {
   PUSHORT p;
   PSZ     psz;

   WriteMark  (fpOut);
   FilPushPos (fpOut);
   FilWriteULong  (fpOut, 0);               // ulOffset
   FilWriteULong  (fpOut, pfd->ulLen);
   FilWriteULong  (fpOut, pfd->ulSize);
   FilWriteULong  (fpOut, pfd->ulCRC);
   FilWriteUShort (fpOut, pfd->uMethod);
   p = (PUSHORT)(PVOID)&(pfd->fDate);
   FilWriteUShort (fpOut, *p);
   p = (PUSHORT)(PVOID)&(pfd->fTime);
   FilWriteUShort (fpOut, *p);
   FilWriteUShort (fpOut, pfd->uAtt);

   /*--- Strip off drive info, and all path or prefix path onfo ---*/
   psz = SkipLeadingCrap (pfd->szName, bSTRIPPATH);

   FilWriteStr  (fpOut, psz);
   FilWriteStr  (fpOut, pfd->szDesc);
   FilSwapPos   (fpOut, TRUE);
   FilWriteULong (fpOut, FilPeekPos (fpOut));
   FilPopPos    (fpOut, TRUE);
   }



static void UpdateFileHeader (FILE *fp, ULONG ulFilePos, ULONG ulOutSize, ULONG ulLen, ULONG ulWRITECRC)
   {
   FilPushPos (fp);
   fseek (fp, ulFilePos + SIZEOFFSET, SEEK_SET);
   FilWriteULong (fp, ulOutSize);
   FilWriteULong (fp, ulWRITECRC);
   FilPopPos (fp, TRUE);
   }



/*
 * adds a file to the library
 *
 * returns:
 *    0   - ok
 *    999 - file got bigger
 *    #   - error
 */
static UINT WriteTheDamnFile (PLDESC pldOut, PFDESC pfd, ULONG ulFilePos)
   {
   FILE   *fpOut, *fpIn;
   ULONG  ulOutSize;
   UINT   uRet;

   /*--- compression module vars ---*/
   Cmp2EnableCRC (TRUE,  FALSE);  // disable READ crc
   Cmp2EnableCRC (FALSE, TRUE);   // enable Write crc
   Cmp2SetCRC (INITCRC, FALSE);   // init Write crc

   fpOut = pldOut->fp;

   WriteFileHeader (fpOut, pfd);
   WriteMark  (fpOut);

   if (pfd->uMode == LIB)
      {
      fseek (pfd->pld->fp, pfd->ulOffset, SEEK_SET);
      ReadMark (pfd->pld->fp);
      return CopyFile (pfd->pld->fp, fpOut, pfd->ulSize, pfd->uMethod);
      }

   if (pfd->uMode != UPDATE && pfd->uMode != CMDLINE)
      return 0;

   if (!(fpIn = fopen (pfd->szName, "rb")))
      return SetErr (EBLERR_CANTOPEN);

   if (!pfd->uMethod)
      {
      if (uRet = CopyFile (fpIn, fpOut, pfd->ulSize, pfd->uMethod))
         UpdateFileHeader (fpOut, ulFilePos, pfd->ulSize, pfd->ulSize, Cmp2GetCRC(0));
      fclose (fpIn);
      return uRet;
      }
   uRet = CompressFile (fpIn, fpOut, pfd->ulSize, &ulOutSize, TRUE);
   fclose (fpIn);

   UpdateFileHeader (fpOut, ulFilePos, ulOutSize, pfd->ulSize, Cmp2GetCRC(0));
   return uRet;
   }



static UINT WriteLibFromList (PLDESC pldOut)
   {
   PFDESC pfd;
   ULONG  ulFilePos, ulCurrPos;
   UINT i, uRet, iFiles = 0;

   for (i=0, pfd = fList; pfd; pfd = pfd->Next, i++)
      {
      if (pfd->uMode == DELET)
         continue;

      ulFilePos = ftell (pldOut->fp);

      uRet = WriteTheDamnFile (pldOut, pfd, ulFilePos);
      if (!uRet) // ok
         {
         iFiles++;
         }
      else if (uRet == 999)  // bloated file
         {
         fseek (pldOut->fp, ulFilePos, SEEK_SET); // rewind over file
         pfd->uMethod = STORE;
         if (!WriteTheDamnFile (pldOut, pfd, ulFilePos))
            iFiles++;
         else
            fseek (pldOut->fp, ulFilePos, SEEK_SET);
         }
      else // (uRet == 2)  error
         {
         fseek (pldOut->fp, ulFilePos, SEEK_SET); // rewind over error'd file
         }
      }
   WriteMark  (pldOut->fp);
   ulCurrPos = ftell (pldOut->fp);
   fseek (pldOut->fp, FILELENOFFSET, SEEK_SET);
   FilWriteULong  (pldOut->fp, ulCurrPos);
   FilWriteUShort (pldOut->fp, iFiles);
   fclose (pldOut->fp);
   return 0;
   }



static void DeleteFilesInList (void)
   {
   PFDESC pfd;

   for (pfd = fList; pfd; pfd = pfd->Next) 
      if (pfd->uMode == CMDLINE || pfd->uMode == UPDATE)
         unlink (pfd->szName);
   }



/*******************************************************************/
/*                                                                 */
/*                                                                 */
/*                                                                 */
/*******************************************************************/
                 
static PLDESC MakePLDESC (PSZ pszDesc) 
   {
   PLDESC pld;

   pld = calloc (1, sizeof (LDESC));
   pld->ulOffset = HEADERSIZE + 13 + NewStrLen (pszDesc);
   pld->ulSize   = 0;
   pld->uCount   = 0;
   pld->uLibVer  = LIBVER;
   pld->pszDesc  = (pszDesc ? strdup (pszDesc) : NULL);
   return pld;
   }

static PLDESC FreePLDESC (PLDESC pld) 
   {
   MemFreeData (pld->pszDesc);
   MemFreeData (pld);
   return NULL;
   }


/*
 * fp is left at end, where 1st MARK will be placed
 */
static PLDESC MakeLibFile (PSZ pszLib, PSZ pszDesc)
   {
   PLDESC pld;

   pld = MakePLDESC (pszDesc);

   if (!(pld->fp = fopen (pszLib, "wb")))
      {
      SetErr (EBLERR_CANTOPEN);
      return FreePLDESC (pld);
      }
   fwrite (LIBHEADER, 1, HEADERSIZE, pld->fp);
   FilWriteULong  (pld->fp, pld->ulOffset);
   FilWriteULong  (pld->fp, pld->ulSize  );
   FilWriteUShort (pld->fp, pld->uCount  );
   FilWriteUShort (pld->fp, pld->uLibVer );
   FilWriteStr   (pld->fp, pld->pszDesc );

   return pld;
   }


/*******************************************************************/
/*                                                                 */
/*                                                                 */
/*                                                                 */
/*******************************************************************/

static int ListLib (PSZ pszLib, PSZ pszFileSpec, PSZ pszOutFile)
   {
   PLDESC pld;
   PFDESC pfd;
   UINT   i;
   FILE   *fp;

   if (!(pld = OpenLib (pszLib)))
      return SetErr (GetLibErr ());

   if (!(fp = fopen (pszOutFile, "wt")))
      return SetErr (EBLERR_CANTOPEN);

   /*--- lib description on 1st line ---*/   
   fprintf (fp, "%s\n", (pld->pszDesc ? pld->pszDesc : ""));

   for (i=0; i<pld->uCount; i++)
      {
      if (!(pfd = ReadFileInfo (pld, TRUE)))
         return SetErr (EBLERR_CANTREAD);

      if (!FileMatches (pfd->szName, pszFileSpec))
         continue;

      fprintf (fp, "%s\t%lu\t%lu\t%s\t%s\t%s\n", 
                      pfd->szName, pfd->ulLen, 
                      pfd->ulSize, DateStr (pfd->fDate), 
                      TimeStr (pfd->fTime), pfd->szDesc);
      }
   fclose (fp);
   fclose (pld->fp);
   return 0;
   }



static int TestLib (PSZ pszLib)
   {
   PLDESC pld;
   PFDESC pfd;
   UINT i;

   if (!(pld = OpenLib (pszLib)))
      return SetErr (GetLibErr ());

   if (!pld->uCount)
      return SetErr(EBLERR_NOMATCH);

   for (i=0; i<pld->uCount; i++)
      {
      if (!(pfd = ReadFileInfo (pld, FALSE)))
         return SetErr (EBLERR_CANTREAD);

      if (TestFile (pfd))
         ; // error set in test file
      }       
   fclose (pld->fp);
   return 0;
   }



static UINT AddMatchingFiles (PSZ pszPath, PSZ pszMatch)
   {
   UINT uMatchCt, uAtts;
   PFINFO pfo = NULL;
   PFDESC pfd;
   CHAR   szSearchSpec[256];
   PSZ    psz;

   sprintf (szSearchSpec, "%s%s", pszPath, pszMatch);

   uAtts = FILE_NORMAL | FILE_ARCHIVED |
           (bINCLSYSTEM ? FILE_SYSTEM : 0) | (bINCLHIDDEN ? FILE_HIDDEN : 0);

   for (uMatchCt=0; pfo = DirFindFile (szSearchSpec, uAtts, pfo); uMatchCt++)
      {
      if (!stricmp (pfo->szName, szLIB) || !stricmp (pfo->szName, TMPLIB))
         continue;

      pfd = malloc (sizeof (FDESC));
      pfd->uMethod = (bSTOREONLY ? STORE : HOSE);

      /*--- don't even try to compress certain file types ---*/
      if ((psz = strrchr (pfo->szName, '.')) &&
         (!strnicmp (psz, ".0", 2)  || !strnicmp (psz, ".EBS", 4) ||
         !strnicmp (psz, ".EBL", 4) || !strnicmp (psz, ".ZIP", 4)))
         pfd->uMethod  = STORE;

      sprintf (pfd->szName, "%s%s", pszPath, pfo->szName);
      *pfd->szDesc = '\0';

      pfd->ulSize = pfo->ulSize;
      pfd->ulLen  = pfo->ulSize;
      pfd->fDate  = pfo->fDate;
      pfd->fTime  = pfo->fTime;
      pfd->uAtt   = (UINT)pfo->cAttr;
      pfd->uMode  = CMDLINE;

      AddToFileList (pfd);
      }
   return uMatchCt;
   }



static int Dogbert (PSZ pszMatchSpec)
   {
   UINT uFiles;
   PFINFO pfoTmp, pfo = NULL;
   CHAR   szPath[256], szNewPath[256], szMatch[128];

   SplitUp (szPath, szMatch, pszMatchSpec);

   /*--- look for files in current directory ---*/
   uFiles = AddMatchingFiles (szPath, szMatch);

   if (!bRECURSE)
      return uFiles;

   /*--- look for files in other directories ---*/
   sprintf (szNewPath, "%s*.*", szPath);
   pfo = DirFindAll (szNewPath, FILE_DIRECTORY |(bINCLHIDDEN ? FILE_HIDDEN : 0));

   for (pfoTmp = pfo; pfoTmp; pfoTmp = pfoTmp->next)
      {
      sprintf (szNewPath, "%s%s\\", szPath, pfoTmp->szName);
      uFiles += AddMatchingFiles (szNewPath, szMatch);
      }
   DirFindAllCleanup (pfo);
   return uFiles;
   }



static int RecurseAddMatchingFiles (PSZ pszMatchSpec)
   {
   CHAR szSingleSpec [80];
   UINT uCt = 0;

   pszMatchSpec = (pszMatchSpec && *pszMatchSpec ? pszMatchSpec : "*.*");
   while (StrGetWord (&pszMatchSpec, szSingleSpec, NULL, "; ", FALSE, TRUE) != -1)
      uCt += Dogbert (szSingleSpec);
   return uCt;
   }



static UINT AddLib (PSZ pszLib, PSZ pszFileSpec, BOOL bMove)
   {
   PLDESC pldOut, pldIn;
   PFDESC pfd;
   PSZ    pszOutFile;
   UINT   i;

   pszOutFile = TMPLIB;
   if (!(pldIn = OpenLib (pszLib)))        // could not open lib or bad lib
      {
      if (GetLibErr () != 6) // error=6 is no lib file
         return SetErr (GetLibErr ());
      pszOutFile = pszLib;
      }
   else  // opened lib ok
      {
      for (i=0; i<pldIn->uCount; i++)
         {
         if (!(pfd = ReadFileInfo (pldIn, TRUE)))
            return SetErr (EBLERR_CANTREAD);
         pfd->uMode = LIB;
         AddToFileList (pfd);
         }
      }

   if (!(pldOut = MakeLibFile (pszOutFile, (pldIn ? pldIn->pszDesc : NULL))))
      return GetErr ();
      
   if (!RecurseAddMatchingFiles (pszFileSpec))
      return SetErr (EBLERR_NOMATCH);

   WriteLibFromList (pldOut);
   fclose (pldOut->fp);
   if (pldIn)
      {
      fclose (pldIn->fp);
      unlink (pszLib);
      }

   if (pszOutFile != pszLib)
      rename (pszOutFile, pszLib);

   if (bMove)
      DeleteFilesInList ();
   }



static UINT DelLib (PSZ pszLib, PSZ pszFileSpec)
   {
   PFDESC fTmp, pfd;
   PLDESC pldOut, pldIn;
   UINT   i, uDeleteCount = 0;

   if (!(pldIn = OpenLib (pszLib)))
      return SetErr (GetLibErr ());

   if (!pszFileSpec)
      return 0;

   /*--- read in file descriptors ---*/
   for (i=0; i<pldIn->uCount; i++)
      {
      if (!(pfd = ReadFileInfo (pldIn, TRUE)))
         return SetErr (EBLERR_CANTREAD);

      pfd->uMode = LIB;
      AddToFileList (pfd);
      }

   /*--- match file names ---*/
   for (fTmp = fList; fTmp; fTmp = fTmp->Next)
      if (FileMatches (pfd->szName, pszFileSpec) && fTmp->uMode != DELET)
         {
         uDeleteCount++;
         fTmp->uMode = DELET;
         }

   if (!uDeleteCount)
      return SetErr (EBLERR_NOMATCH);

   if (!(pldOut = MakeLibFile (TMPLIB, pldIn->pszDesc)))
      return GetErr ();

   WriteLibFromList (pldOut);
   fclose (pldOut->fp);
   fclose (pldIn->fp);
   unlink (pszLib);
   rename (TMPLIB, pszLib);
   }



static UINT XLib (PSZ pszLib, PSZ pszFileSpec)
   {
   PFDESC pfd;
   PLDESC pldIn;
   UINT   uErr, j, uExtractCount = 0;

   if (!(pldIn = OpenLib (pszLib)))
      return SetErr (GetLibErr ());

   /*--- read in file descriptors ---*/
   for (j=0; j<pldIn->uCount; j++)
      {
      if (!(pfd = ReadFileInfo (pldIn, FALSE)))
         return SetErr (EBLERR_CANTREAD);

      if (FileMatches (pfd->szName, pszFileSpec))
         uErr = WriteToFile (pfd, FALSE);
      else
         uErr = 1;

      if (!uErr)
         uExtractCount++;
      else
         SkipFileData (pfd);
      }
   fclose (pldIn->fp);
   }



static UINT DescLib (PSZ pszLib, PSZ pszDesc)
   {
   UINT   i;
   PLDESC pldOut, pldIn;
   PFDESC pfd;
   ULONG  ulFilePos;
  
   /*--- open input lib ---*/
   if (!(pldIn = OpenLib (pszLib)))
      return SetErr (GetLibErr ());

   /*--- open output lib ---*/
   if (!(pldOut = MakeLibFile (TMPLIB, pszDesc)))
      return GetErr ();

   for (i=0; i<pldIn->uCount; i++)
      {
      if (!(pfd = ReadFileInfo (pldIn, TRUE)))
         return SetErr (EBLERR_CANTREAD);
      pfd->uMode = LIB;
      ulFilePos  = ftell (pldOut->fp);
      WriteTheDamnFile (pldOut, pfd, ulFilePos);
      }
   WriteMark  (pldOut->fp);
   ulFilePos = ftell (pldOut->fp);
   fseek (pldOut->fp, FILELENOFFSET, SEEK_SET);
   FilWriteULong  (pldOut->fp, ulFilePos);
   FilWriteUShort (pldOut->fp, i);
   fclose (pldOut->fp);
   fclose (pldIn->fp);
   unlink (pszLib);
   rename (TMPLIB, pszLib);
   }


   
/*******************************************************************/
/*                                                                 */
/*                                                                 */
/*                                                                 */
/*******************************************************************/


static void MakeLibName (PSZ pszDest, PSZ pszSrc)
   {
   PSZ p1, p2;

   strcpy (pszDest, pszSrc);
   p2 = strrchr (pszDest, '\\');
   p1 = (p2 ? p2+1 : pszDest);
   if (!strchr (p1, '.'))
      strcat (p1, ".EBL");
   }


static void SetFlags (UINT uFlags)
   {
   bINCLSYSTEM = uFlags & EBL_SYSTEMFILES;
   bINCLHIDDEN = uFlags & EBL_HIDDENFILES;
   bRECURSE    = uFlags & EBL_RECURSE    ;
   bSTRIPPATH  = uFlags & EBL_STRIPPATH  ;
   bFILEDESC   = FALSE;
   bSTOREONLY  = FALSE;
   bLIBDESC    = FALSE;
   iOVERWRITE = (uFlags & EBL_NOOVERWRITE ? -1 : 1);
   }


/*******************************************************************/
/*                                                                 */
/*                                                                 */
/*                                                                 */
/*******************************************************************/


UINT EblInit (void)
   {
   if (!(pszWORKBUFF = malloc (35256U)))
      return SetErr (EBLERR_NOMEM);
   Cmp2Init (pszWORKBUFF, 3, 1);
   SetErr (0);
   return 0;
   }

UINT EblTerm (void)
   {
   free (pszWORKBUFF);
   return 0;
   }

UINT EblGetError (PSZ pszErrorStr)
   {
   strcpy (pszErrorStr, ppszEBL_ERRORS [GLOBALERROR]);
   return GLOBALERROR;
   }

UINT EblAdd (PSZ pszLIB, PSZ pszFileSpec, UINT uFlags)
   {
   MakeLibName (szLIB, pszLIB);
   SetFlags (uFlags);
   AddLib (szLIB, pszFileSpec, FALSE);
   return GLOBALERROR;
   }

UINT EblMove (PSZ pszLIB, PSZ pszFileSpec, UINT uFlags)
   {
   MakeLibName (szLIB, pszLIB);
   SetFlags (uFlags);
   AddLib (szLIB, pszFileSpec, TRUE);
   return GLOBALERROR;
   }

UINT EblDelete (PSZ pszLIB, PSZ pszFileSpec, UINT uFlags)
   {
   MakeLibName (szLIB, pszLIB);
   SetFlags (uFlags);
   DelLib (szLIB, pszFileSpec);
   return GLOBALERROR;
   }

UINT EblExtract (PSZ pszLIB, PSZ pszFileSpec, UINT uFlags)
   {
   MakeLibName (szLIB, pszLIB);
   SetFlags (uFlags);
   XLib (szLIB, pszFileSpec);
   return GLOBALERROR;
   }

UINT EblList (PSZ pszLIB, PSZ pszFileSpec, PSZ pszOutFile)
   {
   MakeLibName (szLIB, pszLIB);
   ListLib (szLIB, pszFileSpec, pszOutFile);
   return GLOBALERROR;
   }

UINT EblTest (PSZ pszLIB)
   {
   MakeLibName (szLIB, pszLIB);
   TestLib (szLIB);
   return GLOBALERROR;
   }

UINT EblDescribe (PSZ pszLIB, PSZ pszDesc)
   {
   bLIBDESC = TRUE;
   MakeLibName (szLIB, pszLIB);
   DescLib (szLIB, pszDesc);
   return GLOBALERROR;
   }


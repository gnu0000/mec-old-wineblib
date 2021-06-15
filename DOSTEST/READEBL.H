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
 * ReadEBL.h
 *
 *
 * Craig Fitzgerald
 *
 */


#define EXT           ".EBL"
#define LIBVER        100
#define LIBHEADER     "This is an EBS library file.\n\x1A"
#define HEADERSIZE    30

#define EBLMARK       0x21554E47UL
#define EBLMARKSIZE   4

#define FILELENOFFSET HEADERSIZE + 4
#define SIZEOFFSET    12

#define STORE         0
#define HOSE          1
#define UNHOSE        -1


/*
 * Library Volume Information
 */
typedef struct
   {
   FILE   *fp;        // Lib File Handle
   ULONG  ulOffset;   // offset into Header of first file
   ULONG  ulSize;     // Size of Lib File
   UINT uCount;     // count of proposal files in lib file
   UINT uLibVer;    // Version of lib file
   PSZ    pszDesc;    // Description of file
   } LDESC;
typedef LDESC *PLDESC;


/*
 * File Entry Header Information
 */
typedef struct _fd
   {
   ULONG  ulOffset;     // Starting Position in File data relative to file
   ULONG  ulLen;        // Size of uncompressed file data
   ULONG  ulSize;       // Size of compressed file data
   ULONG  ulCRC;        // CRC check of file data
   UINT uMethod;      // Compression method
   FDATE  fDate;        // Date of file
   FTIME  fTime;        // Time of file
   UINT uAtt;
   CHAR   szName [256]; // Name of file
   CHAR   szDesc [256]; // Description of proposal file

   /*--- The following are not kept in the file ---*/
   PLDESC pld;          // ptr to owning lib Volume info
   UINT uMode;        // processing mode
   struct _fd *Next;    // used when building processing chains
   } FDESC;
typedef FDESC *PFDESC;


extern UINT uLIBERR;

extern CHAR szEBLPATH  [];   // used by EbOpen
extern CHAR szEBLLIB   [];   // used by EbOpen
extern CHAR szEBLFILE  [];   // used by EbOpen

extern PFDESC PFD;      // used by EbOpen
extern PLDESC PLD;      // used by EbOpen


/*
 * [path][lib][:][file]
 * path: c: c:\ c:\dir\dir\ \dir\ dir\ <none>
 * lib   lib.exe lib
 *
 * c:\libs\liba.ebl:file7.txt
 * c:liba.ebl:file7.txt
 */
FILE *EbOpen (PSZ pszFile, PSZ pszMode);

PFDESC GetNextFile (PLDESC pld, PFDESC pfdPrev, BOOL bFreeOld);

PFDESC ReadFileInfo (PLDESC pld, BOOL bSkipData);

PLDESC OpenLib (PSZ pszLib);

PVOID FreePFD (PFDESC pfd);

PVOID FreePLD (PLDESC pld);

PSZ DateStr (FDATE fDate);

PSZ TimeStr (FTIME fTime);

PSZ AttStr (UINT uAtt);

ULONG Ratio (ULONG ulSmall, ULONG ulBig);

BOOL ReadMark (FILE *fp);
PVOID SetLibErr (UINT i);
UINT GetLibErr (void);
void SkipFileData (PFDESC pfd);



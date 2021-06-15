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
 * WinEBLIB.h
 *
 *
 * Craig Fitzgerald
 *
 * This file is part of the EBS module
 *
 */


#define EBLERR_OK             0
#define EBLERR_INPUTEOF       1
#define EBLERR_OUTPUTEOF      2
#define EBLERR_CRCERROR       3
#define EBLERR_UNKNOWN        4
#define EBLERR_BADSIZE        5
#define EBLERR_CANTOPEN       6
#define EBLERR_NOMATCH        7
#define EBLERR_EOL            8
#define EBLERR_NOTEBL         9
#define EBLERR_NOMEM          10
#define EBLERR_CANTREAD       11
#define EBLERR_CANTCREATE     12
#define EBLERR_FILEEXISTS     13
#define EBLERR_CANNOTMAKEPATH 14


#define EBL_NOOVERWRITE   0x01   //  1 default is to overwrite
#define EBL_SYSTEMFILES   0x02   //  2 default is no sys files
#define EBL_HIDDENFILES   0x04   //  4 default is no hidden files
#define EBL_RECURSE       0x08   //  8 default is to not recurse dirs
#define EBL_STRIPPATH     0x10   // 16 default is to keep path


UINT EblInit (void);
UINT EblTerm (void);

UINT EblGetError (PSZ pszErrorStr);

UINT EblAdd (PSZ pszLIB, PSZ pszFileSpec, UINT uFlags);
UINT EblMove (PSZ pszLIB, PSZ pszFileSpec, UINT uFlags);
UINT EblDelete (PSZ pszLIB, PSZ pszFileSpec, UINT uFlags);
UINT EblExtract (PSZ pszLIB, PSZ pszFileSpec, UINT uFlags);
UINT EblList (PSZ pszLIB, PSZ pszFileSpec, PSZ pszOutFile);
UINT EblTest (PSZ pszLIB);
UINT EblDescribe (PSZ pszLIB, PSZ pszDescr);


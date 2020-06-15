/***************************************************************************\
     MAXDEL.C

   Read Maxlog.Dat and delete all references to a specified file name.
 
   Written by: Pete Norloff
               10/2/91

\***************************************************************************/
/* Include Files:                                                          */
#define INCL_DOSFILEMGR
#include <os2.h>

#include <stdio.h>
#include <string.h>
#include <process.h>
#include <stdlib.h>

/***************************************************************************/
/* Function Prototypes:                                                    */
VOID  main(SHORT argc, CHAR *argv[]);
VOID  purge_data_file(VOID);

/***************************************************************************/
#define LOGOFF           1
#define DOWNLOAD         2
#define FILE_REQ         3
#define MAIL_SESSION_END 4

/***************************************************************************/
/* Data  Structures:                                                       */
typedef struct xfr
{
   LONG  date;
   LONG  time;
   SHORT node;
   LONG  filesize;
   SHORT name_length;
};
struct xfr xfer;

typedef struct lgoff
{
   LONG  date;
   LONG  time;
   SHORT node;
   LONG  duration;
};
struct lgoff logoff;

/***************************************************************************/
/* External Variables:                                                     */
CHAR delfile[10][40];
SHORT nfiles;

/***************************************************************************/
VOID main(SHORT argc, CHAR *argv[])
{
   SHORT i;

   printf("MaxDel v1.0 - delete references to a specified file name.\n");
   printf("written by Pete Norloff, 1:109/347@fidonet\n\n");

   if ((argc > 1) && (argc <= 11))
   {
      nfiles = argc-1;
/*    Purge filename from Maxlog.dat.                                      */
      printf("Purging %i filenames:\n",nfiles);
      for (i=0; i< nfiles; i++)
      {
         strcpy(delfile[i],argv[i+1]);
         printf("%s\n",delfile[i]);
      }
      purge_data_file();
   }
   else
   {
      printf("Usage is MaxDel filename [[filename] filename...].\n");
   }

   printf("Done.\n");
}
/***************************************************************************/
VOID purge_data_file()
{
   FILE  *old_file,*new_file;
   SHORT downloads_purged = 0;
   ULONG bytes_dl_purged = 0;
   SHORT event_number;
   CHAR  filename[255];
   SHORT i;
   SHORT skip;

/* Copy the .Dat file to .Bak.                                             */
   DosCopy("Maxlog.Dat","Maxlog.Bak",DCPY_EXISTING,0);

   if ((old_file=fopen("Maxlog.Bak","rb")) == NULL)
   {
      printf("Error opening Maxlog.Bak (purge)\n");
      exit(1);
   }
   if ((new_file=fopen("Maxlog.Dat","wb")) == NULL)
   {
      printf("Error opening Maxlog.Dat (purge)\n");
      exit(1);
   }

   while (feof(old_file) == 0)
   {
      fread(&event_number,sizeof(SHORT),1,old_file);
      switch(event_number)
      {
         case DOWNLOAD:
         case FILE_REQ:
            if (fread(&xfer,sizeof(xfer),1,old_file) == 0) break;
            fread(filename,xfer.name_length,1,old_file);

            skip = FALSE;
            for (i=0; i<nfiles; i++)
            {
               if (stricmp(filename,delfile[i]) == 0)
               {
                  skip = TRUE;
                  break;
               }
            }

            if (skip == TRUE)
            {
               printf("Purging record of %s\n",delfile[i]);
               downloads_purged++;
               bytes_dl_purged += xfer.filesize;
            }
            else
            {
               fwrite(&event_number,sizeof(SHORT),1,new_file);
               fwrite(&xfer,sizeof(xfer),1,new_file);
               fwrite(filename,xfer.name_length,1,new_file);
            }
            break;

         case LOGOFF:
         case MAIL_SESSION_END:
            if (fread(&logoff,sizeof(logoff),1,old_file) == 0) break;
            fwrite(&event_number,sizeof(SHORT),1,new_file);
            fwrite(&logoff,sizeof(logoff),1,new_file);
            break;

         default:
            printf("UNKNOWN EVENT!!! (=%i) (bug)(purge)\n",event_number);
            break;
      }
   }

   fclose(new_file);
   fclose(old_file);

   printf("\nDownloads Purged: %i\n",downloads_purged);
   printf("\n    Bytes Purged: %li\n\n",bytes_dl_purged);
}
/***************************************************************************/

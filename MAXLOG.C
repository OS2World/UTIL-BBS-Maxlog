/***************************************************************************\
     MAXLOG.C

   Read Maximus callers log and store entries of interest in a data file.
 
   Purge the data fileback to a specified number of days.
 
   Written by: Pete Norloff
               1/10/91

   Modified 3/2/91 to include Binkley log files.

\***************************************************************************/
/* Include Files:                                                          */
#define INCL_DOSFILEMGR
#define INCL_DOSDATETIME
#include <os2.h>

#include <stdio.h>
#include <string.h>
#include <process.h>
#include <stdlib.h>

/***************************************************************************/
/* Function Prototypes:                                                    */
VOID  main(SHORT argc, CHAR *argv[]);
VOID  get_date_time(CHAR *,CHAR *, LONG *, LONG *);
VOID  read_configuration(VOID);
SHORT extract_file_name(CHAR *,CHAR *);
VOID  capture_new_data(VOID);
VOID  produce_bulletins(VOID);
VOID  purge_data_file(VOID);
VOID  sort_dl_list(VOID);
VOID  add_dl_to_list(CHAR *, LONG);
VOID  write_top_dl_bulletin(VOID);
VOID  write_system_usage_bulletin(VOID);
VOID  count_usage_days(SHORT, SHORT *);
VOID  add_call_to_graph(struct lgoff *);
VOID  purge_logfile(CHAR *);

/***************************************************************************/
#define MAX_DAYS       365
#define MAX_LOGFILES     4
#define LOGOFF           1
#define DOWNLOAD         2
#define FILE_REQ         3
#define MAIL_SESSION_END 4
#define MAX_USAGELINES 100
#define MAX_DL_FILES  6000

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

typedef struct dloads
{
   char  *filename;
   SHORT dl_count;
   LONG  dl_bytes;
};
struct dloads downloads[MAX_DL_FILES];

typedef struct usage
{
   USHORT n_calls;
   float  hours;
};
static struct usage system_used[MAX_LOGFILES][366];

/***************************************************************************/
/* External Variables:                                                     */
SHORT n_logfiles = 0;
CHAR  logfile_name[MAX_LOGFILES][256];
SHORT n_binkfiles = 0;
CHAR  binkfile_name[MAX_LOGFILES][256];
CHAR  outbound_dir[256];
SHORT dl_struct_index = 0;
CHAR  bbs_name[81];
SHORT summary_days;
SHORT archive = FALSE;
SHORT save_data = FALSE;
SHORT log = FALSE;
SHORT UsageLines = 10;
static LONG usage_by_hour[MAX_LOGFILES][24];
DATETIME datetime;
ULONG  total_bytes_dl = 0;

/***************************************************************************/
VOID main(SHORT argc, CHAR *argv[])
{

   printf("Maxlog v1.32 - create Maximus system usage bulletins.\n");
   printf("written by Pete Norloff, 1:109/347@fidonet\n\n");

/* Read in the configuration file.                                         */
   read_configuration();

   if ((argc <= 1) || ((argc > 1) && (strcmpi(argv[1],"skip") != 0)))
   {
/*    Capture new data from the log files.                                 */
      printf("Capturing new data from log file.\n");
      capture_new_data();

/*    Purge data file, saving the number of days listed in the .Cfg file.  */
      printf("Purging old data from file.\n");
      purge_data_file();
   }
   else
   {
      printf("Not capturing new data or purging.\n");
   }

/* Create the bulletins from the data.                                     */
   printf("Processing Maxlog.Dat.\n");
   produce_bulletins();

   printf("Done.\n");
}
/***************************************************************************/
VOID read_configuration()
{
   FILE  *config;
   SHORT i;
   CHAR  dummy[81];
   CHAR  keyword[40];

/* Get the current dte and time.  Several functions use this info.         */
   DosGetDateTime(&datetime);

   if ((config=fopen("Maxlog.Cfg","r")) == NULL)
   {
      printf("Error opening Maxlog.Cfg\n");
      exit(1);
   }

   while (feof(config) == 0)
   {
      fscanf(config,"%s",keyword);

      if (strcmpi(keyword,"Name") == 0)
      {
         fgets(bbs_name,80,config);
/*       Trim off the new line character from the bbs_name.                */
         bbs_name[strlen(bbs_name)-1] = '\0';
      }
      else if (strcmpi(keyword,"NFiles") == 0)
      {
         fscanf(config,"%i",&n_logfiles);
         if ((n_logfiles < 0) || (n_logfiles > MAX_LOGFILES))
         {
            printf("Invalid number of logfiles defined\n");
            exit(1);
         }
      }
      else if (strcmpi(keyword,"FileNames") == 0)
      {
/*       Read in the log file names.                                        */
         for (i=0; i<n_logfiles; i++)
         {
            fscanf(config,"%s",logfile_name[i]);
         }
      }
      else if (strcmpi(keyword,"NBinkFiles") == 0)
      {
         fscanf(config,"%i",&n_binkfiles);
         if ((n_binkfiles < 0) || (n_binkfiles > MAX_LOGFILES))
         {
            printf("Invalid number of binkfiles defined\n");
            exit(1);
         }
      }
      else if (strcmpi(keyword,"Outbound") == 0)
      {
         fscanf(config,"%s",outbound_dir);
      }
      else if (strcmpi(keyword,"BinkFileNames") == 0)
      {
/*       Read in the log file names.                                        */
         for (i=0; i<n_binkfiles; i++)
         {
            fscanf(config,"%s",binkfile_name[i]);
         }
      }
      else if (keyword[0] == ';')
      {
/*       Comment line, read and discard line.                               */
         fgets(dummy,80,config);
      }
      else if (strcmpi(keyword,"Days") == 0)
      {
         fscanf(config,"%i",&summary_days);
         if ((summary_days <= 0) || (summary_days > MAX_DAYS))
         {
            printf("Invalid number of days defined\n");
            exit(1);
         }
      }
      else if (strcmpi(keyword,"Archive") == 0)
      {
/*       The presence of the keyword "archive" in the configuration file    */
/*       enables the archive option which appends the log file to a file    */
/*       named <filename>.sav.                                              */
         archive = TRUE;
      }
      else if (strcmpi(keyword,"SaveData") == 0)
      {
/*       The presence of the keyword "SaveData" in the configuration file   */
/*       enables the save data option which appends the purged entries      */
/*       from the Maxlog.Dat to a file called Maxlog.Rmv.                   */
         save_data = TRUE;
      }
      else if (strcmpi(keyword,"Log") == 0)
      {
/*       The presence of the keyword "Log" in the configuration file        */
/*       enables the logging of activity.  This writes two lines per day to */
/*       a file called Maxlog.Log, containing the number of events added    */
/*       and purged each day.                                               */
         log = TRUE;
      }
      else if (strcmpi(keyword,"UsageLines") == 0)
      {
         fscanf(config,"%i",&UsageLines);
         if ((UsageLines < 10) || (UsageLines > MAX_USAGELINES))
         {
            printf("Invalid number of days UsageLines\n");
            exit(1);
         }
      }
   }
   fclose(config);
}
/***************************************************************************/
VOID capture_new_data()
{
   FILE   *logfile,*event;
   CHAR   text[81];
   LONG   date,start_time,end_time,elapsed_time,dl_time;
   static CHAR  date_string[7];
   SHORT  dummy;
   static CHAR bytes[15];
   SHORT  i;
   SHORT  event_number;
   CHAR   filename[255];
   CHAR   dummy_string[80],file_dir[255];
   SHORT  online = FALSE;
   SHORT  logoffs_added = 0;
   SHORT  downloads_added = 0;
   ULONG  bytes_dl_added = 0;
   ULONG  seconds_added = 0;

/* Open the data file for append to save the records of interest.         */
   if ((event=fopen("Maxlog.Dat","ab")) == NULL)
   {
      printf("Error opening Maxlog.Dat\n");
      exit(1);
   }

/* Loop over the number of log files specified in the configuration file. */
   for (i=0; i<n_logfiles; i++)
   {
/*    Open the log file named in the configuration file.                  */
      if ((logfile=fopen(logfile_name[i],"r+b")) == NULL)
      {
         printf("Error opening %s\n",logfile_name[i]);
         continue;
      }

/*    Read a line from the file.                                          */
      while(fgets(text,80,logfile) != NULL)
      {
/*       Ignore strings less than 23 characters in length.                */
         if (strlen(text) < 23) continue;

/*       For line entry "CPS", extract the number of bytes in the file.   */
         if (strncmp(&text[23],"CPS,",3) == 0)
         {
            sscanf(&text[28],"%i %s",&dummy,bytes);
         }
/*       For line entry "DL", extract the file name.                        */
         if (strncmp(&text[23],"DL,",2) == 0)
         {
/*          Check to see if the file sent was a mail packet.  If so, ignore */
/*          so, ignore.                                                     */
            sscanf(&text[23],"%s %s",dummy_string,file_dir);
            if (strnicmp(".qw",&file_dir[strlen(file_dir)-4],3) != 0)
            {
               event_number = DOWNLOAD;
               xfer.node = i+1;
               get_date_time(text,date_string,&date,&dl_time);
               xfer.date = date;
               xfer.time = dl_time;
               xfer.filesize = atol(&bytes[1]);
               extract_file_name(filename,&text[28]);
               xfer.name_length = strlen(filename) - 1;
               filename[xfer.name_length-1] = '\0';
               fwrite(&event_number,sizeof(SHORT),1,event);
               fwrite(&xfer,sizeof(xfer),1,event);
               fwrite(filename,xfer.name_length,1,event);
               downloads_added++;
               bytes_dl_added += xfer.filesize;
            }
         }
         else if (strncmp(&text[23],"Begin,",6) == 0)
         {
/*          Pass string to get_date_time() to calculate int date and times*/
            get_date_time(text,date_string,&date,&start_time);
            online = TRUE;
         }
         else if (strncmp(&text[23],"End,",4) == 0)
         {
            if (online == TRUE)
            {
/*             Pass string to get_date_time() to calculate int date and times*/
               get_date_time(text,date_string,&date,&end_time);
               event_number = LOGOFF;
               logoff.node = i+1;
               logoff.date = date;
               logoff.time = end_time;
               elapsed_time = end_time - start_time;
               if (elapsed_time < 0)
               {
                  elapsed_time = elapsed_time + 86400L;
               }
               logoff.duration = elapsed_time;
               fwrite(&event_number,sizeof(SHORT),1,event);
               fwrite(&logoff,sizeof(logoff),1,event);
               online = FALSE;
               logoffs_added++;
               seconds_added += logoff.duration;
            }
         }
      }
      fclose(logfile);
      purge_logfile(logfile_name[i]);
   }

/* Loop over the Binkley log files specified in the configuration file.   */
   for (i=0; i<n_binkfiles; i++)
   {
/*    Open the log file named in the configuration file.                  */
      if ((logfile=fopen(binkfile_name[i],"r+b")) == NULL)
      {
         printf("Error opening %s\n",binkfile_name[i]);
         continue;
      }

/*    Read a line from the file.                                          */
      while(fgets(text,80,logfile) != NULL)
      {
/*       Ignore strings less than 23 characters in length.                */
         if (strlen(text) < 23) continue;

/*       For line entry "CPS", extract the number of bytes in the file.   */
         if (strncmp(&text[23],"CPS,",3) == 0)
         {
            sscanf(&text[28],"%i %s",&dummy,bytes);
         }
/*       For line entry "Sent", extract the file name.                     */
         if (strncmp(&text[23],"Sent,",4) == 0)
         {
/*          Check to see if the file sent was from the outbound area.  If  */
/*          so, ignore.                                                    */
/*          Also ignore *.rsp files .tic files, and QWK packets (*.qw?).   */
            sscanf(&text[23],"%s %s",dummy_string,file_dir);
            if ((strnicmp(outbound_dir,file_dir,strlen(outbound_dir)) != 0) &&
                (stricmp(".rsp",&file_dir[strlen(file_dir)-4]) != 0) &&
                (stricmp(".tic",&file_dir[strlen(file_dir)-4]) != 0))
            {
               event_number = FILE_REQ;
               xfer.node = i+1;
               get_date_time(text,date_string,&date,&dl_time);
               xfer.date = date;
               xfer.time = dl_time;
               xfer.filesize = atol(&bytes[1]);
               extract_file_name(filename,&text[33]);
               xfer.name_length = strlen(filename) - 1;
               filename[xfer.name_length-1] = '\0';
               fwrite(&event_number,sizeof(SHORT),1,event);
               fwrite(&xfer,sizeof(xfer),1,event);
               fwrite(filename,xfer.name_length,1,event);
               downloads_added++;
               bytes_dl_added += xfer.filesize;
            }
         }
         else if (strncmp(&text[23],"Remote,",6) == 0)
         {
/*          Pass string to get_date_time() to calculate int date and times*/
            get_date_time(text,date_string,&date,&start_time);
            online = TRUE;
         }
         else if (strncmp(&text[23],"End ",4) == 0)
         {
            if (online == TRUE)
            {
/*             Pass string to get_date_time() to calculate int date and times*/
               get_date_time(text,date_string,&date,&end_time);
               event_number = MAIL_SESSION_END;
               logoff.node = i+1;
               logoff.date = date;
               logoff.time = end_time;
               elapsed_time = end_time - start_time;
               if (elapsed_time < 0)
               {
                  elapsed_time = elapsed_time + 86400L;
               }
               logoff.duration = elapsed_time;
               fwrite(&event_number,sizeof(SHORT),1,event);
               fwrite(&logoff,sizeof(logoff),1,event);
               online = FALSE;
               logoffs_added++;
               seconds_added += logoff.duration;
            }
         }
      }
      fclose(logfile);
      purge_logfile(binkfile_name[i]);
   }
   fclose(event);

   if (log == TRUE)
   {
/*    Log the number of additions to the log file.                         */
      if ((logfile=fopen("Maxlog.Log","a")) == NULL)
      {
         printf("Error opening Maxlog.Log\n");
         return;
      }
      fprintf(logfile,"%02i/%02i/%02i:  ",datetime.month,datetime.day,datetime.year);
      fprintf(logfile,"calls add : %3i  Min: %5i  Xfers add : %4i  bytes: %7li\n",
                       logoffs_added, (SHORT)(seconds_added/60),downloads_added, bytes_dl_added);
      fclose(logfile);
   }
}
/***************************************************************************/
VOID purge_logfile(CHAR *filename)
{
   SHORT i;
   CHAR  save_file[255];

/* If the flag variable "archive" is TRUE, append log file to save file.   */
   if (archive == TRUE)
   {
/*    Create a save_file name which has the extention ".Sav".           */
      strcpy(save_file,filename);
      for (i=strlen(save_file); i>0; i--)
      {
         if (save_file[i] == '.')
         {
            strcpy(&save_file[i],".Sav");
            break;
         }
      }

/*    Make sure that the save_file name was created.                    */
      if (i == 0)
      {
         strcat(save_file,".Sav");
      }

      if (DosCopy(filename,save_file,DCPY_APPEND,0) == 0)
      {
         DosDelete(filename,0);
      }
      printf("%s archived and deleted.\n",filename);
   }
   else
/* Just delete the old log file.                                           */
   {
      DosDelete(filename,0);
      printf("%s deleted.\n",filename);
   }
}
/***************************************************************************/
VOID purge_data_file()
{
   FILE  *old_file,*new_file,*save_file,*logfile;
   SHORT i;
   SHORT downloads_purged = 0;
   SHORT logoffs_purged = 0;
   ULONG bytes_dl_purged = 0;
   ULONG seconds_purged = 0;
   LONG  test_date,todays_date;
   LONG  days[12] = {31,28,31,30,31,30,31,31,30,31,30,31};
   SHORT event_number;
   CHAR  filename[255];

/* Calculate today's date.                                                 */
   todays_date = 0;
   for (i=0; i< datetime.month-1; i++)
   {
      todays_date += days[i];
   }
   todays_date += datetime.day;

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
   if (save_data == TRUE)
   {
      if ((save_file=fopen("Maxlog.Rmv","ab")) == NULL)
      {
         printf("Error opening Maxlog.Rmv (purge)\n");
         exit(1);
      }
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
            test_date = (365-todays_date+xfer.date-1) % 365;
            if (test_date >= 365 - summary_days - 1)
            {
               fwrite(&event_number,sizeof(SHORT),1,new_file);
               fwrite(&xfer,sizeof(xfer),1,new_file);
               fwrite(filename,xfer.name_length,1,new_file);
            }
            else
            {
               if (save_data == TRUE)
               {
                  fwrite(&event_number,sizeof(SHORT),1,save_file);
                  fwrite(&xfer,sizeof(xfer),1,save_file);
                  fwrite(filename,xfer.name_length,1,save_file);
               }
               downloads_purged++;
               bytes_dl_purged += xfer.filesize;
            }
            break;

         case LOGOFF:
         case MAIL_SESSION_END:
            if (fread(&logoff,sizeof(logoff),1,old_file) == 0) break;
            test_date = (365-todays_date+logoff.date-1) % 365;
            if (test_date >= 365 - summary_days - 1)
            {
               fwrite(&event_number,sizeof(SHORT),1,new_file);
               fwrite(&logoff,sizeof(logoff),1,new_file);
            }
            else
            {
               if (save_data == TRUE)
               {
                  fwrite(&event_number,sizeof(SHORT),1,save_file);
                  fwrite(&logoff,sizeof(logoff),1,save_file);
               }
               logoffs_purged++;
               seconds_purged += logoff.duration;
            }
            break;

         default:
            printf("UNKNOWN EVENT!!! (=%i) (bug)(purge)\n",event_number);
            break;
      }
   }

   fclose(new_file);
   fclose(old_file);
   if (save_data == TRUE)
   {
      fclose(save_file);
   }

   printf("\nDownloads Purged: %i\n",downloads_purged);
   printf("  Logoffs Purged: %i\n\n",logoffs_purged);

   if (log == TRUE)
   {
/*    Log the number of purges to the log file.                            */
      if ((logfile=fopen("Maxlog.Log","a")) == NULL)
      {
         printf("Error opening Maxlog.Log\n");
         return;
      }
      fprintf(logfile,"%02i/%02i/%02i:  ",datetime.month,datetime.day,datetime.year);
      fprintf(logfile,"calls prgd: %3i  Min: %5i  Xfers prgd: %4i  bytes: %7li\n\n",
                       logoffs_purged, (SHORT)(seconds_purged/60),downloads_purged, bytes_dl_purged);
      fclose(logfile);
   }
}
/***************************************************************************/
VOID produce_bulletins()
{
   FILE  *event;
   CHAR  filename[255];
   SHORT event_number;
   static LONG old_date;

/* Open the data file for append to save the records of interest.          */
   if ((event=fopen("Maxlog.Dat","rb")) == NULL)
   {
      printf("Error opening Maxlog.Dat\n");
      exit(1);
   }
   while (feof(event) == 0)
   {
      fread(&event_number,sizeof(SHORT),1,event);
      switch(event_number)
      {
         case DOWNLOAD:
         case FILE_REQ:
            fread(&xfer,sizeof(xfer),1,event);
            fread(filename,xfer.name_length,1,event);
            add_dl_to_list(filename,xfer.filesize);
            break;

         case LOGOFF:
         case MAIL_SESSION_END:
            fread(&logoff,sizeof(logoff),1,event);

/*          Collect the events in system_used[][].                          */
/*            if (logoff.duration > 60)*/
            {
               system_used[logoff.node-1][logoff.date].n_calls++;
               system_used[logoff.node-1][logoff.date].hours +=
                                      ((float)logoff.duration / (float)3600);
/*             Collect the events in the usage_by_hour array.               */
               add_call_to_graph(&logoff);
            }

/*          Print a few dots just to show the program is still working.     */
            if (logoff.date != old_date)
            {
               printf(".");
               old_date = logoff.date;
            }
            break;

         default:
            printf("UNKNOWN EVENT!!! (=%i) (bug)\n",event_number);
            break;
      }
   }
   fclose(event);
   printf("\n");

/* Now sort the download list.                                             */
   printf("Sorting download list.\n");
   sort_dl_list();

/* Write out the top downloads bulletin.                                   */
   printf("Writing Top Download bulletin.\n");
   write_top_dl_bulletin();

/* Write out the system usage bulletin.                                    */
   printf("Writing System Usage bulletin.\n");
   write_system_usage_bulletin();
}
/***************************************************************************/
VOID sort_dl_list()
{
   SHORT i,j;
   SHORT tmp_count;
   CHAR  *tmp_name;
   LONG  tmp_bytes;
   SHORT sorting = TRUE;
   SHORT pass_number = 0;

   while (sorting == TRUE)
   {
      sorting = FALSE;
      for (i=0; i<dl_struct_index-1; i++)
      {
         if (downloads[i+1].dl_count > downloads[i].dl_count)
         {
            sorting = TRUE;

            tmp_name = downloads[i].filename;
            tmp_count = downloads[i].dl_count;
            tmp_bytes = downloads[i].dl_bytes;

            downloads[i].filename = downloads[i+1].filename;
            downloads[i].dl_count = downloads[i+1].dl_count;
            downloads[i].dl_bytes = downloads[i+1].dl_bytes;

            downloads[i+1].filename = tmp_name;
            downloads[i+1].dl_count = tmp_count;
            downloads[i+1].dl_bytes = tmp_bytes;
         }
      }
      if (j++%10 == 0) printf(".");
   }
   printf("\n");
}
/***************************************************************************/
VOID write_top_dl_bulletin()
{
   SHORT i;
   FILE *top_dl, *all_dl;
   SHORT days_used;
   SHORT days_in_bulletin;
   SHORT total_files_dl = 0;

/* Find out how many days this .dat file covers.                           */
   count_usage_days(0, &days_used);

   if ((days_used > 0) && (days_used < summary_days))
   {
      days_in_bulletin = days_used;
   }
   else
   {
      days_in_bulletin = summary_days;
   }

/* Open the text file to write the "All Download" bulletin.                */
   if ((all_dl=fopen("All_Dl.Txt","w")) == NULL)
   {
      printf("Error opening All_Dl.Txt\n");
      exit(1);
   }

   for (i=0; i<dl_struct_index; i++)
   {
      fprintf(all_dl,"%3i  %s\n",downloads[i].dl_count,downloads[i].filename);
      total_files_dl += downloads[i].dl_count;
/* this line is removed because total_bytes_dl is now calculated in the */
/* add_dl_to_list function. 1/16/92                                     */
/*      total_bytes_dl += downloads[i].dl_bytes * downloads[i].dl_count;*/
   }
   fclose(all_dl);

/* Open the text file to write the "Top Download" bulletin.                */
   if ((top_dl=fopen("Top_Dl.Txt","w")) == NULL)
   {
      printf("Error opening Top_Dl.Txt\n");
      exit(1);
   }
   fprintf(top_dl,"Top Files downloaded from%s: \n",bbs_name);
   fprintf(top_dl,"Generated: %02i-%02i-%02i at %02i:%02i\n",
            datetime.month,datetime.day,datetime.year,
            datetime.hours,datetime.minutes);

   for (i=0; i<15; i++)
   {
      fprintf(top_dl,"     %12s %3i     %12s %3i     %12s %3i\n",
               downloads[i].filename,   downloads[i].dl_count,
               downloads[i+15].filename,downloads[i+15].dl_count,
               downloads[i+30].filename,downloads[i+30].dl_count);
   }

   fprintf(top_dl,"\nFor a period covering the last %i days:\n",days_in_bulletin);
   fprintf(top_dl,"Total number of files downloaded: %i\n",total_files_dl);
   fprintf(top_dl,"Total number of K bytes downloaded: %lu\n",total_bytes_dl/1000);
   fprintf(top_dl,"A total of %i different files have been downloaded\n",dl_struct_index);

   fclose(top_dl);
}
/***************************************************************************/
VOID write_system_usage_bulletin()
{
   SHORT i,j,k;
   FILE  *usage, *hours;
   SHORT total_calls;
   float total_hours;
   float total_for_node;
   SHORT days_used;
   CHAR  filename[15];
   CHAR  output_string[81];
   CHAR  graph_string[4];
   SHORT total_nodes;

/* Loop for the maximum of n_logfiles and n_binkfiles.                     */
   total_nodes = max(n_logfiles,n_binkfiles);

/* Open the data file for append to save the records of interest.          */
   if ((usage=fopen("Usage.Txt","w")) == NULL)
   {
      printf("Error opening Usage.Txt\n");
      exit(1);
   }

/* Print the whole usage structure out.                                    */
   fprintf(usage,"System usage by day:\n");
   fprintf(usage,"Day  ");
   for (i=0; i<total_nodes; i++)
   {
      fprintf(usage,"Calls  Hours     ");
   }
   fprintf(usage,"Totals\n");
   for (j=1; j<=365; j++)
   {
      total_calls = 0;
      total_hours = 0;
      fprintf(usage,"%3i  ",j);
      for (i=0; i<total_nodes; i++)
      {
         fprintf(usage,"%5i  %5.2f     ",system_used[i][j].n_calls,
                                         system_used[i][j].hours);
         total_calls+= system_used[i][j].n_calls;
         total_hours+= system_used[i][j].hours;
      }
      fprintf(usage,"%5i  %5.2f\n",total_calls,total_hours);
   }


/* Print the usage by hour table.                                          */
   for (j=0; j<total_nodes; j++)
   {
/*    Find out how many days this .dat file covers.                        */
      count_usage_days(j, &days_used);

      total_for_node = (float)0;
      fprintf(usage,"\n\n");
      for (i=0; i<24; i++)
      {
         fprintf(usage,"hour %2i: %4li  %% used: %5.2f\n",
                 i,usage_by_hour[j][i],
                 (float)usage_by_hour[j][i]/(float)60/(float)(days_used-1));

         total_for_node+=(float)usage_by_hour[j][i]/60;
      }
      fprintf(usage,"Total for this node: %5.2f\n",total_for_node);
   }
   fclose(usage);

   for (k=0; k<total_nodes; k++)
   {
/*    Find out how many days this .dat file covers.                        */
      count_usage_days(k, &days_used);

      sprintf(filename,"Hours%i.Txt",k);

/*    Open the bulletin file for write.                                    */
      if ((hours=fopen(filename,"w")) == NULL)
      {
         printf("Error opening %s\n",filename);
         exit(1);
      }

      fprintf(hours,"Percentage of system usage by hour, generated: %02i-%02i-%02i at %02i:%02i\n",
               datetime.month,datetime.day,datetime.year,
               datetime.hours,datetime.minutes);
      fprintf(hours,"Summarized over the last %i days\n",days_used);

      for (i=UsageLines-1; i>=0; i--)
      {
         sprintf(output_string,"%3i%%  ",(i+1) * 100/UsageLines);
         for (j=0; j<24; j++)
         {
            if (usage_by_hour[k][j]/(float)60/(float)(days_used-1) >
                          (float)i / (float)UsageLines)
            {
               strcpy(graph_string,"   ");
               graph_string[0] = 0xdb;
               strcat(output_string,graph_string);
            }
            else
            {
               strcat(output_string,"   ");
            }
         }
/*       Strip off all trailing spaces from the graph.                      */
         for (j=strlen(output_string); j>0; j--)
         {
            if (output_string[j-1] == ' ')
            {
               output_string[j-1] = '\0';
            }
            else
            {
               break;
            }
         }
         fprintf(hours,"%s\n",output_string);
      }
      fprintf(hours,"      0  1  2  3  4  5  6  7  8  9 10 11 12 13 14 15 16 17 18 19 20 21 22 23\n");

      fclose(hours);
   }
}
/***************************************************************************/
VOID add_call_to_graph(struct lgoff *logoff)
{
   SHORT i;
   SHORT start_time,end_time;

/* Calculate the end time in minutes.                                      */
   end_time = (short)(logoff->time/60);

   start_time = end_time - (short)(logoff->duration/60) + 1;

/* Special case for when the logon was before midnight and logoff was      */
/* after midnight.                                                         */
   if (start_time < 0)
   {
      start_time += 1440;
      for (i=start_time; i<=1439; i++)
      {
         usage_by_hour[logoff->node-1][i/60]++;
      }
      start_time = 0;
   }

   for (i=start_time; i<=end_time; i++)
   {
      usage_by_hour[logoff->node-1][i/60]++;
   }
}
/***************************************************************************/
VOID count_usage_days(SHORT node, SHORT *days_used)
{
/* This function just counts the number of entries in the system_used      */
/* structure to obtain the number of days covered in this .dat file        */

   SHORT i;

   *days_used = 0;
   for (i=1; i<=365; i++)
   {
      if (system_used[node][i].n_calls != 0) *days_used = (*days_used)+1;
   }
}
/***************************************************************************/
VOID add_dl_to_list(CHAR *filename, LONG bytes)
{
   SHORT i;

/* Loop through the downloads structure to see if this file is already in  */
/* the list.                                                               */
   for (i=0; i<dl_struct_index; i++)
   {
      if (stricmp(downloads[i].filename, filename) == 0)
      {
         downloads[i].dl_count++;
         total_bytes_dl += bytes;
         return;
      }
   }

/* If you get to here, the filename isn't already in the list.              */
   downloads[dl_struct_index].filename = calloc(strlen(filename)+1, sizeof(CHAR));
   strcpy(downloads[dl_struct_index].filename, filename);
   downloads[dl_struct_index].dl_count = 1;
   downloads[dl_struct_index].dl_bytes = bytes;
   total_bytes_dl += bytes;
   dl_struct_index++;

/* Test dl_struct_index for overflow.                                       */
   if (dl_struct_index >= MAX_DL_FILES)
   {
      printf("ERROR: MaxLog has reached its capacity for total number of files.\n");
      printf("The limit is currently set to %i.  Please contact the author.\n",MAX_DL_FILES);
      exit(99);
   }
}
/****************************************************************************/
VOID get_date_time(CHAR *text, CHAR *date_string, LONG *date, LONG *time)
{
   LONG hour, minute, second;
   LONG day;
   static LONG month;
   CHAR temp[10];
   LONG days[12] = {31,28,31,30,31,30,31,31,30,31,30,31};
   LONG i;

/* Save the date text.                                                    */
   strncpy(date_string,&text[2],6);
   date_string[6] = '\0';

   strncpy(temp,&text[2],2);
   temp[2] = '\0';
   day = atoi(temp);

   strncpy(temp,&text[9],2);
   temp[2] = '\0';
   hour = atoi(temp);

   strncpy(temp,&text[12],2);
   temp[2] = '\0';
   minute = atoi(temp);

   strncpy(temp,&text[15],2);
   temp[2] = '\0';
   second = atoi(temp);

   strncpy(temp,&text[5],3);
   temp[3] = '\0';
   if (strcmp(temp,"Jan")  == 0) month=1;
   else if (strcmp(temp,"Feb")  == 0) month=2;
   else if (strcmp(temp,"Mar")  == 0) month=3;
   else if (strcmp(temp,"Apr")  == 0) month=4;
   else if (strcmp(temp,"May")  == 0) month=5;
   else if (strcmp(temp,"Jun")  == 0) month=6;
   else if (strcmp(temp,"Jul")  == 0) month=7;
   else if (strcmp(temp,"Aug")  == 0) month=8;
   else if (strcmp(temp,"Sep")  == 0) month=9;
   else if (strcmp(temp,"Oct")  == 0) month=10;
   else if (strcmp(temp,"Nov")  == 0) month=11;
   else if (strcmp(temp,"Dec")  == 0) month=12;

   *time = hour*3600L + minute*60L + second;

   *date = 0;
   for (i=0; i< month-1; i++)
   {
      *date += days[i];
   }
   *date += day;
}
/***************************************************************************/
SHORT extract_file_name(char target[],char source[])
{
   SHORT i;

/* Loop through the source string looking for a '\' character.             */
   for (i=strlen(source); i>=0; i--)
   {
/*    Once found, copy the file name portion to target and return TRUE.    */
      if (source[i] == '\\')
      {
         strcpy(target, &source[i+1]);
         return(TRUE);
      }
   }
/* If there is no "\", return the whole string.                            */
   strcpy(target, source);
   return(TRUE);
}
/***************************************************************************/

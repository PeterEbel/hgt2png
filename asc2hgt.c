#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <assert.h>
#include <sys/stat.h>

typedef struct tag_Metadata {
  int iCols;
  int iRows;
  long lXLLCenter;
  long lYLLCenter;
  int iCellSize;
  int iNoDataValue;
} Metadata, *pMetadata;

int main(int argc, const char *argv[])
{
  FILE *InFile;
  FILE *OutFile;
  char ReadBuffer[300000];

  if (argc != 2)
  {
    fprintf(stderr, "Usage: asc2hgt inputfile\n");
    return 1;
  }

  if ((InFile = fopen(argv[1], "rt")) == NULL)
  {
    fprintf(stderr, "Error: Cannot open input file %s\n", argv[1]);
    return 1;
  }

  Metadata md;

  char TempBuffer[80];

  for (int i = 0; i < 6; i++)
  {

    fgets(ReadBuffer, sizeof(ReadBuffer), InFile);

    switch(i) {
      case 0:
        strcpy(TempBuffer, &ReadBuffer[6]);
        md.iCols = atoi(TempBuffer);
        break;
      case 1:
        strcpy(TempBuffer, &ReadBuffer[6]);
        md.iRows = atoi(TempBuffer);
        break;
      case 2:
        strcpy(TempBuffer, &ReadBuffer[10]);
        md.lXLLCenter = atoi(TempBuffer);
        break;
      case 3:
        strcpy(TempBuffer, &ReadBuffer[10]);
        md.lYLLCenter = atoi(TempBuffer);
        break;
      case 4:
        strcpy(TempBuffer, &ReadBuffer[9]);
        md.iCellSize = atoi(TempBuffer);
        break;
      case 5:
        strcpy(TempBuffer, &ReadBuffer[13]);
        md.iNoDataValue = atoi(TempBuffer);
        break;
      default:
        break;
    }
  }

  char szTmpFilename[255];
  char szTmp[255];

  strcpy(szTmpFilename, argv[1]);
  szTmpFilename[strlen(szTmpFilename) - 4] = '\0';
  strcat(szTmpFilename, "_"); 
  sprintf(szTmp, "%d", md.iCols); 
  strcat(szTmpFilename, szTmp); 
  strcat(szTmpFilename, "x"); 
  sprintf(szTmp, "%d", md.iRows); 
  strcat(szTmpFilename, szTmp); 
  strcat(szTmpFilename, ".HGT"); 

  if ((OutFile = fopen(szTmpFilename, "wb")) == NULL)
  {
    fprintf(stderr, "Error: Cannot open output file %s\n", szTmpFilename);
    return 1;
  }

  short int iCurrentElevation;
  int iCurrentLine = 0;
  int iCurrentCol = 0;
  
  fprintf(stderr, "Info: writing file %s...\n", szTmpFilename);

  while (!feof(InFile))
  {
    memset(ReadBuffer, 0, sizeof(ReadBuffer));
    fgets(ReadBuffer, sizeof(ReadBuffer), InFile);
    iCurrentLine++;

  //Establish string and get the first token  
    char *token = strtok(ReadBuffer, " ");
    if (token != NULL)
    {
      iCurrentCol = 1;
      iCurrentElevation = (int) atof(token);
      fwrite(&iCurrentElevation, sizeof(short int), 1, OutFile);
      if (iCurrentCol == 5761)
        fprintf(stderr, "%4d %4d: %d\n", iCurrentLine, iCurrentCol, iCurrentElevation);
    }

  //Find next token  
    while (token != NULL) {
      if (*token != '\n')
      {
        iCurrentElevation = (int) atof(token = strtok(NULL, " "));
        if (iCurrentElevation != 0)
        {
          iCurrentCol++;
          fwrite(&iCurrentElevation, sizeof(short int), 1, OutFile);
          if (iCurrentCol == 5761)
          {
            fprintf(stderr, "%4d %4d: %d\n", iCurrentLine, iCurrentCol, iCurrentElevation);
            //getchar();
          }
        }
      }
      else
      {
        token = NULL;
      }
    }
    iCurrentCol = 1;
  }
                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                            
  fflush(OutFile);

  fclose(InFile);
  fclose(OutFile);

  fprintf(stderr, "Info: Ready.\n");

}


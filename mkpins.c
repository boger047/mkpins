//************************************************************************
//************************************************************************
//
//  PROGRAM:    MKPINS
//
//  FUNCTION:   Reads a comma-delimited file (prepared by a spreadsheet)
//              containing the pinout and signal names for a given
//              embedded project (currently it works on the Cortex M3
//              LPC17xx processor by NXP, but could easily be expanded
//              to other micros).
//
//
//************************************************************************
// First few rows from a sample file:
// "ITEM","P176x","PORT","BIT","FUNC1","FUNC2","FUNC3","SIGNAME","FUNC","IN/OUT","MODE","OD","DEF","ACT"
// 1,46,0,0,"RD1","TXD3","SDA1","GSM_TX",2,0,,,,
// 2,47,0,1,"TD1","RXD3","SCL1","GSM_RX",2,1,,,,
// 
// First row is ignored and assumed header row
// 
// Fields are as follows:
//
// 1.  ITEM   just sequential ID for each pin/signal
// 2.  PIN#   physical pin number for the package
// 3.  PORT   Port number associated with this signal
// 4.  BIT    Bit number associated with this signal
// 5.  FUNC1  Name of alternate function 1
// 6.  FUNC2  Name of alternate function 2
// 7.  FUNC3  Name of alternate function 3
// 8.  SIG    Name of the signal in this project
// 9.  FUNC   Which function to use (0,1,2,3)
// 10. IN/OUT Signal is input (1) or output (0)
// 11. MODE   Port Mode (0,1,2,3)
// 12. OD     Port Open Drain Mode (1=on)
// 13. DEF    Desired Default (Initialized) State
// 14. ACT    Signal is active high (1) or active low (0)
//************************************************************************


#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <fcntl.h>
#include <time.h>

#ifdef _WIN32
// stdint.h for Microsoft Visual C, obtained from:
// http://code.google.com/p/msinttypes/
#include "stdint_win.h"
// stdint.h for Microsoft Visual C, obtained from:
// http://waterjuice.org/2013/06/stdbool-h/
#include "stdbool_win.h"
#include "windows.h"
#include "conio.h"
#else
#include <stdint.h>
#include <stdbool.h>
#endif

#define MAXCHARS (256)
typedef struct tagPINDEF {
  int seq;
  int pinnum;
  int port;
  int bit;
  char altfunc1[MAXCHARS];
  char altfunc2[MAXCHARS];
  char altfunc3[MAXCHARS];
  char signame[MAXCHARS];
  int func;
  int inout;
  int mode;
  int odrain;
  int def;
  int active;
} PINDEF;

extern void print_file( FILE *fp, FILE *file2print );
extern void print_headers_note( FILE *fp );
extern void print_headers_c( FILE *fp );
extern void print_headers_h( FILE *fp );
extern void print_pindef_h( FILE *fp, PINDEF *pd );
extern void print_pindef_c( FILE *fp, PINDEF *pd );
extern void print_pinarray_h( FILE *fp );
extern void print_pinarray_c( FILE *fp );

extern void calc_PINSEL( void );
extern void print_PINSEL( FILE *fp );
extern void calc_PINMODE( void );
extern void print_PINMODE( FILE *fp );
extern void calc_FIODIR( void );
extern void print_FIODIR( FILE *fp );
extern void calc_FIOPIN( void );
extern void print_FIOPIN( FILE *fp );
extern void calc_FIOMASK( void );
extern void print_FIOMASK( FILE *fp );

extern void print_bit_macros( FILE *fp );
extern void print_bit_defines( FILE *fp );

extern char* trim_lead( char *cp );
extern char* trim_bom( char *cp );
extern void trim_trail( char *cp );
extern void trim_eoline( char *cp );
extern char* trim_quotes( char *cp );

#define MAXPINS (256)
#define NA (0xff)
char line[MAXCHARS]; // saves all the input CSV records
PINDEF pins[MAXPINS];
int nseqs;
unsigned long PINSEL[11];
unsigned long PINMODE[11];
unsigned long PINMODE_OD[5];
unsigned long FIODIR[5];
unsigned long FIOPIN[5];
unsigned long FIOMASK[5];

// Project name prefix (keep it short)
char prefix[MAXCHARS]; 
char PREFIX[MAXCHARS];

char fname_in[MAXCHARS];
char fname_out_c[MAXCHARS];
char fname_out_h[MAXCHARS];
char mkpins_date_time[MAXCHARS];

int main( int argc, char *argv[] ) {
  int i,j,k, cnt,br;
  unsigned long lineno;
  int seqno;
  char *lp;
  int beg,len;

  PINDEF *pd, pindef;

#define MAXFIELDS (16)
  char *field_ptr[MAXFIELDS];
  int field_beg[MAXFIELDS];
  int field_len[MAXFIELDS];
  char field[MAXCHARS];

  int itemp;
  int okay;

  int item;
  int seq;
  int pinnum;
  int port;
  int bit;
  char altfunc1[MAXCHARS];
  char altfunc2[MAXCHARS];
  char altfunc3[MAXCHARS];
  char signame[MAXCHARS];
  int func;
  int inout;
  int mode;
  int odrain;
  int def;
  int active;
  FILE *fin, *foutc, *fouth;
  time_t tbeg;
  int exit_code=0;

  pd=&pindef;
  tbeg=time(NULL);
  strftime( mkpins_date_time, MAXCHARS, "%a %d-%b-%Y %H:%M:%S", localtime(&tbeg));

  if(argc<3) {
    fprintf(stderr,"Usage:   mkpins filename project-name\n");
    fprintf(stderr,"e.g.,    mkpins pinout.csv zebra\n");
    exit(99);
  }

  strncpy( fname_in, argv[1], MAXCHARS );
  fin = fopen( fname_in, "r" );
  if(!fin) {
    fprintf(stderr,"Error opening input file: %s\n", fname_in );
    exit(99);
  } else {
    fprintf(stderr,"Opened input CSV file: %s\n", fname_in );
  }

  strncpy( prefix, argv[2], MAXCHARS );
  len = strlen(prefix);
  for(i=0;i<len;i++) { // check and clean prefix
    if(isprint(prefix[i])) { // simple check, should really be more thorough
      prefix[i] = tolower(prefix[i]);
      PREFIX[i] = toupper(prefix[i]);
    } else {
      break;
    }
  }
  if(i<len) {
    fprintf(stderr,"Error with project prefix: %s\n", argv[2] );
    exit(99);
  }
  sprintf( fname_out_c, "%s_gpio.c", prefix );
  sprintf( fname_out_h, "%s_gpio.h", prefix );
  fprintf(stderr,"prefix: %s\n", prefix );
  fprintf(stderr,"PREFIX: %s\n", PREFIX );

  foutc=fopen( fname_out_c, "w" );
  if(!foutc) {
    fprintf(stderr,"Error opening C output file: %s\n", fname_out_c );
    exit(99);
  } else {
    fprintf(stderr,"Opened for output C-File: %s\n", fname_out_c );
  }

  fouth=fopen( fname_out_h, "w" );
  if(!fouth) {
    fprintf(stderr,"Error opening H output file: %s\n", fname_out_h );
    exit(99);
  } else {
    fprintf(stderr,"Opened for output H-File: %s\n", fname_out_h );
  }
  

  for(i=0;i<5;i++) {
    PINMODE_OD[i]=0;
    FIODIR[i]=0;
    FIOPIN[i]=0;
    FIOMASK[i]=0;
  }

  for(i=0;i<11;i++) {
    PINSEL[i]=0;
    PINMODE[i]=0;
  }


  seqno=0; // keeps track of entries actually saved and stored
  lineno=0; // keeps track of line number on input file
  fgets( line, MAXCHARS, fin );  // read and ignore (header)
  lp=trim_bom( line ); // we don't need header, but just in case, don't forget BOM
  lineno++;

  print_headers_note( foutc );
  print_headers_c( foutc );

  print_headers_note( fouth );
  print_headers_h( fouth );

  while( fgets( line, MAXCHARS, fin ) ) {
    lp=line;
    if(0==strncmp(line,"END", 3)) break;
    lineno++;

    item=0;
    seq=0;
    pinnum=0;
    port=0;
    bit=0;
    for(i=0;i<MAXCHARS;i++) {
      altfunc1[i]=0;
      altfunc2[i]=0;
      altfunc3[i]=0;
      signame[i]=0;
    }
    func=NA;
    inout=NA;
    mode=0;
    odrain=0;
    def=0;
    active=1;

    for(i=0;i<16;i++) {
      field_ptr[i]=(char *)(0);
      field_beg[i]=0;
      field_len[i]=0;
    }
    
    beg=0;
    for(i=0;i<14;i++) {

      len = strcspn(lp+beg,",");

      field_ptr[i]=lp+beg;
      field_beg[i]=beg;
      field_len[i]=len;

      // copy field to temporary string
      // eliminate any double quotes
      for(j=0;j<MAXCHARS;j++) field[j]=0;
      k=0;
      for(j=0;j<len;j++) {
        if((j==0) && (lp[j+beg]=='\"')) continue;
        else if((j==(len-1)) && (lp[j+beg]=='\"')) continue;
        else field[k++]=lp[j+beg];
      }
      field_len[i]=strlen(field);

      if(len!=0) { // parse each field

        if(i==0) {
          if(1==sscanf(field,"%d",&itemp)) seq = itemp;
          else goto MYERROR;
        }
        if(i==1) {
          itemp=strncmp(field,"N/A", 3);
          if(itemp==0) break;
          if(1==sscanf(field,"%d",&itemp)) pinnum = itemp;
          else goto MYERROR;
        }
        if(i==2) {
          if(1==sscanf(field,"%d",&itemp)) port = itemp;
          else goto MYERROR;
        }
        if(i==3) {
          if(1==sscanf(field,"%d",&itemp)) bit = itemp;
          else goto MYERROR;
        }
        // if((i==4) && (strlen(field)>1)) strncpy(altfunc1,trim_quotes(field),MAXCHARS);
        // if((i==5) && (strlen(field)>1)) strncpy(altfunc2,trim_quotes(field),MAXCHARS);
        // if((i==6) && (strlen(field)>1)) strncpy(altfunc3,trim_quotes(field),MAXCHARS);
        // if((i==7) && (strlen(field)>1)) strncpy(signame,trim_quotes(field),MAXCHARS);

        if((i==4) && (strlen(field)>1)) strncpy(altfunc1,field,MAXCHARS);
        if((i==5) && (strlen(field)>1)) strncpy(altfunc2,field,MAXCHARS);
        if((i==6) && (strlen(field)>1)) strncpy(altfunc3,field,MAXCHARS);
        if((i==7) && (strlen(field)>1)) strncpy(signame,field,MAXCHARS);
        if(i==8) {
          if(1==sscanf(field,"%d",&itemp)) func = itemp;
        }
        if(i==9) {
          if(1==sscanf(field,"%d",&itemp)) inout = itemp;
        }
        if(i==10) {
          if(1==sscanf(field,"%d",&itemp)) mode = itemp;
        }
        if(i==11) {
          if(1==sscanf(field,"%d",&itemp)) odrain = itemp;
        }
        if(i==12) {
          if(1==sscanf(field,"%d",&itemp)) def = itemp;
        }
        if(i==13) {
          if(1==sscanf(field,"%d",&itemp)) active = itemp;
        }
      }
      if(lp[beg+len] != '\0') beg += len + 1;
      else                    beg += len;
    }

    if(pinnum==0) continue;  // if this port bit doesn't exist on the package
    if(strlen(signame)==0) continue; // if we don't use this pin this design

    // print this entry
    pd->seq=seqno;
    pd->pinnum=pinnum;
    pd->port=port;
    pd->bit=bit;
    strncpy(pd->altfunc1,altfunc1,MAXCHARS);
    strncpy(pd->altfunc2,altfunc2,MAXCHARS);
    strncpy(pd->altfunc3,altfunc3,MAXCHARS);
    strncpy(pd->signame,signame,MAXCHARS);
    pd->func=func;
    pd->inout=inout;
    pd->mode=mode;
    pd->odrain=odrain;
    pd->def=def;
    pd->active=active;

    print_pindef_h( fouth, pd );
    print_pindef_c( foutc, pd );

    pins[seqno]=pindef; // save to array of pin defs
    seqno++;
    if(seqno >= MAXPINS) break;
    
  }
  nseqs = seqno;
  fprintf(stderr, "Processed %d entries in %ld lines\n", nseqs, lineno);

  print_pinarray_h( fouth );
  print_pinarray_c( foutc );

  // the rest are just #defines, all go in the header
  calc_PINSEL();
  print_PINSEL( fouth );

  calc_PINMODE();
  print_PINMODE( fouth );

  calc_FIODIR();
  print_FIODIR( fouth );

  calc_FIOPIN();
  print_FIOPIN( fouth );

  calc_FIOMASK();
  print_FIOMASK( fouth );

  print_bit_defines( fouth );
  print_bit_macros( fouth );

  print_file( fouth, fin );

  exit_code=0;
  goto MYEXIT;

MYERROR:
  fprintf(stderr,"Error: line %ld, Field %d, String %s\n", lineno, i, field );
  exit_code=99;


MYEXIT:
  fclose(fin);
  fclose(foutc);
  fclose(fouth);
  exit(exit_code);
}

void print_file( FILE *fp, FILE *file2print ) {
  unsigned long lineno;
  char line[MAXCHARS];
  char *lp;
  fprintf( fp, "//************************************************************************\n"); 
  fprintf( fp, "//***  Input Pin Info CSV file %s, printed below for reference:\n", fname_in );
  fprintf( fp, "//************************************************************************\n"); 
  rewind(file2print);
  lineno=0;
  while( fgets( line, MAXCHARS, file2print ) ) {
    if(lineno == 0) lp = trim_bom(line);
    lineno++;
    trim_eoline( lp );
    fprintf( fp, "//%04lu: %s\n", lineno, lp );
  }
  fprintf( fp, "//************************************************************************\n"); 
  fprintf( fp, "//***  END OF FILE %s\n", fname_in );
  fprintf( fp, "//************************************************************************\n"); 
}

void print_headers_note( FILE *fp ) {
  fprintf( fp, "//************************************************************************\n"); 
  fprintf( fp, "//************************************************************************\n"); 
  fprintf( fp, "//***\n");
  fprintf( fp, "//***  NOTE:  This file was automatically generated by MKPINS\n");
  fprintf( fp, "//***  Processing Date/Time:     %s\n", mkpins_date_time );
  fprintf( fp, "//***  Input Pin Info CSV file:  %s\n", fname_in );
  fprintf( fp, "//***  Project Name Prefix:      %s\n", PREFIX );
  fprintf( fp, "//***  Output C-File:            %s\n", fname_out_c );
  fprintf( fp, "//***  Output H-File:            %s\n", fname_out_h );
  fprintf( fp, "//***\n");
  fprintf( fp, "//************************************************************************\n"); 
  fprintf( fp, "//************************************************************************\n"); 
  fprintf( fp, "\n");
}

void print_headers_c( FILE *fp ) {
  fprintf( fp, "#include \"%s\"\n", fname_out_h );
  fprintf( fp, "\n");
}

void print_headers_h( FILE *fp ) {
  fprintf( fp, "typedef struct tag%s_PINDEF {\n", PREFIX );
  fprintf( fp, "  int seq;\n");
  fprintf( fp, "  int pinnum;\n");
  fprintf( fp, "  int port;\n");
  fprintf( fp, "  int bit;\n");
  fprintf( fp, "  char *altfunc1;\n");
  fprintf( fp, "  char *altfunc2;\n");
  fprintf( fp, "  char *altfunc3;\n");
  fprintf( fp, "  char *signame;\n");
  fprintf( fp, "  int func;\n");
  fprintf( fp, "  int inout;\n");
  fprintf( fp, "  int mode;\n");
  fprintf( fp, "  int odrain;\n");
  fprintf( fp, "  int def;\n");
  fprintf( fp, "  int active;\n");
  fprintf( fp, "} %s_PINDEF;\n", PREFIX );
  fprintf( fp, "\n");
}

void print_pindef_h( FILE *fp, PINDEF *pd ) {
    fprintf( fp, "extern const %s_PINDEF %s_%s;\n", PREFIX, PREFIX, pd->signame );
}

void print_pindef_c( FILE *fp, PINDEF *pd ) {
    fprintf( fp, "const %s_PINDEF %s_%s = { %d, %d, %d, %d, \"%s\", \"%s\", \"%s\", \"%s\", %d, %d, %d, %d, %d, %d };\n", 
        PREFIX, PREFIX, pd->signame, 
        pd->seq, pd->pinnum, pd->port, pd->bit, 
        pd->altfunc1, pd->altfunc2, pd->altfunc3, pd->signame, 
        pd->func, pd->inout, pd->mode, pd->odrain,
        pd->def, pd->active );
}

void print_pinarray_h( FILE *fp ) {
  int i, len;
  int ncol;
  fprintf( fp, "#define NUM_PINDEFS (%d)\n", nseqs );
  fprintf( fp, "extern const %s_PINDEF* %s_PINS[NUM_PINDEFS];\n", PREFIX, PREFIX );
  fprintf( fp, "\n");
}

void print_pinarray_c( FILE *fp ) {
  int i, len;
  int ncol;
  fprintf( fp, "const %s_PINDEF* %s_PINS[NUM_PINDEFS] = {\n", PREFIX, PREFIX );
  fprintf( fp, "    ");
  ncol=4;
  for(i=0;i<nseqs;i++) {
    len=strlen(pins[i].signame)+2; // length of name plus comma and space
    len+=strlen(PREFIX)+1; // account for project prefix and underbar
    fprintf( fp, "&%s_%s, ", PREFIX, pins[i].signame );
    ncol += len;
    if(ncol > 80) {
      fprintf( fp, "\n    ");
      ncol=4;
    }
  }
  fprintf( fp, "\n};\n");
}

// Database definition:
#define IN (1)
#define OUT (0)
// note: FIODIR bit is 0 for input, 1 for output
void calc_FIODIR( void ) {
  int i;
  int bit, port, inout;
  for(i=0;i<nseqs;i++) {
    bit = pins[i].bit;
    port = pins[i].port;
    inout = pins[i].inout;
    if(inout==IN)  FIODIR[port] &= ~(1<<bit);
    if(inout==OUT) FIODIR[port] |=  (1<<bit);
  }
}

void print_FIODIR( FILE *fp ) {
  int i;
  for(i=0;i<5;i++) {
    fprintf( fp, "#define %s_FIODIR%d_INIT (0x%08lx)\n", PREFIX, i, FIODIR[i] );
  }
  fprintf( fp, "\n");
}

void calc_PINSEL( void ) {
  int i;
  int bit, port, func;
  int reg, bit2;
  for(i=0;i<nseqs;i++) {
    bit = pins[i].bit;
    port = pins[i].port;
    func = pins[i].func;
    if(bit<16) {
      reg = port*2;
      bit2 = 2*bit;
    } else {
      reg = 1 + port*2;
      bit2= 2*(bit-16);
    }
    PINSEL[reg] &= ~(0x03 << bit2); // zero the pair of bits
    PINSEL[reg] |= (func << bit2); // or-in the desired bits field
  }
}

void print_PINSEL( FILE *fp ) {
  int i;
  for(i=0;i<11;i++) {
    fprintf( fp, "#define %s_PINSEL%d_INIT (0x%08lx)\n", PREFIX, i, PINSEL[i] );
  }
  fprintf( fp, "\n");
}


void calc_PINMODE( void ) {
  int i;
  int bit, port, mode, odrain;
  int reg, bit2;
  for(i=0;i<nseqs;i++) {
    bit = pins[i].bit;
    port = pins[i].port;
    mode = pins[i].mode;
    odrain = pins[i].odrain;
    if(bit<16) {
      reg = port*2;
      bit2 = 2*bit;
    } else {
      reg = 1 + port*2;
      bit2= 2*(bit-16);
    }
    PINMODE[reg] &= ~(0x03 << bit2); // zero the pair of bits
    PINMODE[reg] |= (mode << bit2); // or-in the desired bits field
    if(odrain==1) PINMODE_OD[port] |=  (1<<bit);
    if(odrain==0) PINMODE_OD[port] &= ~(1<<bit);
  }
}

void print_PINMODE( FILE *fp ) {
  int i;
  for(i=0;i<10;i++) {
    fprintf( fp, "#define %s_PINMODE%d_INIT (0x%08lx)\n", PREFIX, i, PINMODE[i] );
  }
  fprintf( fp, "\n");
  for(i=0;i<5;i++) {
    fprintf( fp, "#define %s_PINMODE_OD%d_INIT (0x%08lx)\n", PREFIX, i, PINMODE_OD[i] );
  }
  fprintf( fp, "\n");
}

// these defines are used to initialize pins to the desired default state
void calc_FIOPIN( void ) {
  int i;
  int bit, port, def;
  for(i=0;i<nseqs;i++) {
    bit = pins[i].bit;
    port = pins[i].port;
    def  = pins[i].def;
    if(def==0)  FIOPIN[port] &= ~(1<<bit);
    if(def==1)  FIOPIN[port] |=  (1<<bit);
  }
}

void print_FIOPIN( FILE *fp ) {
  int i;
  for(i=0;i<5;i++) {
    fprintf( fp, "#define %s_FIOPIN%d_INIT (0x%08lx)\n", PREFIX, i, FIOPIN[i] );
  }
  fprintf( fp, "\n");
}



// these defines are used to initialize default mask values
// we assume all defined GPIO pins should be unmasked, all
void calc_FIOMASK( void ) {
  int i;
  int bit, port, func;
  for(i=0;i<5;i++) FIOMASK[i]=0xffffffffL;
  for(i=0;i<nseqs;i++) {
    bit = pins[i].bit;
    port = pins[i].port;
    func  = pins[i].func;
    if(func==0)  FIOMASK[port] &= ~(1<<bit);
  }
}

void print_FIOMASK( FILE *fp ) {
  int i;
  for(i=0;i<5;i++) {
    fprintf( fp, "#define %s_FIOMASK%d_INIT (0x%08lx)\n", PREFIX, i, FIOMASK[i] );
  }
  fprintf( fp, "\n");
}


// consider output, open-drain


void print_bit_macros( FILE *fp ) {
  int i;
  for(i=0;i<nseqs;i++) {

    fprintf( fp, "#define %s_GET_%-25s   ((LPC_GPIO%d->FIOPIN & (1<<%d)) >> %d)\n", 
                        PREFIX, pins[i].signame, pins[i].port, pins[i].bit, pins[i].bit );

    if(pins[i].odrain==1) {  // open drain
      fprintf( fp, "#define %s_OPEN_%-25s    (LPC_GPIO%d->FIOSET = (1<<%d))\n", 
                          PREFIX, pins[i].signame, pins[i].port, pins[i].bit );
      fprintf( fp, "#define %s_SINK_%-25s    (LPC_GPIO%d->FIOCLR = (1<<%d))\n", 
                          PREFIX, pins[i].signame, pins[i].port, pins[i].bit );
    } else { // driven output
      fprintf( fp, "#define %s_SET_%-25s    (LPC_GPIO%d->FIOSET = (1<<%d))\n", 
                          PREFIX, pins[i].signame, pins[i].port, pins[i].bit );
      fprintf( fp, "#define %s_CLR_%-25s    (LPC_GPIO%d->FIOCLR = (1<<%d))\n", 
                          PREFIX, pins[i].signame, pins[i].port, pins[i].bit );
      if(pins[i].active==1) { // active high
        fprintf( fp, "#define %s_ON_%-25s    (LPC_GPIO%d->FIOSET = (1<<%d))\n", 
                            PREFIX, pins[i].signame, pins[i].port, pins[i].bit );
        fprintf( fp, "#define %s_OFF_%-25s    (LPC_GPIO%d->FIOCLR = (1<<%d))\n", 
                            PREFIX, pins[i].signame, pins[i].port, pins[i].bit );
        fprintf( fp, "#define %s_QON_%-25s   ((LPC_GPIO%d->FIOPIN & (1<<%d)) >> %d)\n", 
                            PREFIX, pins[i].signame, pins[i].port, pins[i].bit, pins[i].bit );
      } else if(pins[i].active==0) { // active low
        fprintf( fp, "#define %s_ON_%-25s     (LPC_GPIO%d->FIOCLR = (1<<%d))\n", 
                            PREFIX, pins[i].signame, pins[i].port, pins[i].bit );
        fprintf( fp, "#define %s_OFF_%-25s    (LPC_GPIO%d->FIOSET = (1<<%d))\n", 
                            PREFIX, pins[i].signame, pins[i].port, pins[i].bit );
        fprintf( fp, "#define %s_QON_%-25s  (((LPC_GPIO%d->FIOPIN & (1<<%d)) >> %d)^1)\n", 
                            PREFIX, pins[i].signame, pins[i].port, pins[i].bit, pins[i].bit );
      }
    }
  }
  fprintf( fp, "\n");
}

void print_bit_defines( FILE *fp ) {
  int i;
  char temp[MAXCHARS];

  for(i=0;i<nseqs;i++) {
    sprintf( temp, "%s_%s_PORT", PREFIX, pins[i].signame );
    fprintf( fp, "#define %-32s    (%d)\n", temp, pins[i].port );
    sprintf( temp, "%s_%s_BIT", PREFIX, pins[i].signame );
    fprintf( fp, "#define %-32s    (%d)\n", temp, pins[i].bit );
  }
  fprintf( fp, "\n");
}


//************************************************************************
// General Purpose String trimming functions
//************************************************************************

char* trim_lead( char *cp ) {
  int i,len;
  char *rp;
  len=strlen(cp);
  if(len==0) return cp;
  rp=cp;
  for(i=0;i<len;i++) {
    if(isspace(cp[i])) rp++;
    else break;
  }
  return rp;
}

char* trim_bom( char *cp ) {
  int i,len;
  char *rp;
  len=strlen(cp);
  if(len==0) return cp;
  rp=cp;
  for(i=0;i<len;i++) {
    if(!isascii(cp[i])) rp++;
    else break;
  }
  return rp;
}

void trim_eoline( char *cp ) {
  int i,len;
  len=strlen(cp);
  if(len==0) return;
  for(i=len-1;i>=0;i--) {
    if((cp[i])=='\r') cp[i]=0;
    else if((cp[i])=='\n') cp[i]=0;
    else break;
  }
  return;
}

void trim_trail( char *cp ) {
  int i,len;
  len=strlen(cp);
  if(len==0) return;
  for(i=len-1;i>=0;i--) {
    if(isspace(cp[i])) cp[i]=0;
    else break;
  }
  return;
}

char* trim_both( char *cp ) {
  int i,len;
  char *rp;
  len=strlen(cp);
  if(len==0) return cp;
  rp=cp;
/* trim trailing first */
  for(i=len-1;i>=0;i--) {
    if(isspace(cp[i])) cp[i]=0;
    else break;
  }
/* now trim any leading */
  for(i=0;i<len;i++) {
    if(isspace(cp[i])) rp++;
    else break;
  }
  return rp;
}

char* trim_quotes( char *cp ) {
  int i,len;
  char *rp;
  len=strlen(cp);
  if(len==0) return cp;
  rp=cp;
/* trim trailing first */
  i=len-1;
  if(cp[i]=='\"') cp[i]=0;
/* now trim any leading */
  i=0;
  if(cp[i]=='\"') rp++;
  return rp;
}

/*                                                           10.Oct.2009
  ============================================================================

  v3w.c 
  ~~~~~~~~~~

  Description: 
  ~~~~~~~~~~~~
  
  Demonstration program for v3w module using the ITU standard G726 and G711.
  This version accepts either linear or G.711 A/u-law
  input. Since this implementation of the G.726 requires G.711 compressed
  samples, linear samples are converted to G.711 format before being
  processed. Therefore, the same ammount of quantization distortion should
  be expect either way.
  
  Input data is supposed to be aligned at word boundaries, i.e.,
  organized in 16-bit words.
  
  Output data will be generated in the same format as decribed above for
  the input data.
  
  Usage:
  ~~~~~~
  $ v3w [-options] InpFile OutFile 
             [FrameSize [1stBlock [NoOfBlocks [Reset]]]]
  where:
  InpFile     is the name of the file to be processed;
  OutFile     is the name with the processed data;
  FrameSize   is the frame size, in number of samples;
              [default: 32 samples]
  1stBlock    is the number of the first block of the input file
              to be processed [default: 1st block]
  NoOfBlocks  is the number of blocks to be processed, starting on
    	      block "1stBlock" [default: all blocks]

  Options:
  -law #      the letters A or a for G.711 A-law, letter u for 
              G.711 u-law, or letter l for linear. If linear is
	      chosen, A-law is used to compress/expand samples to/from
	      the G.726 routines. Default is A-law.
  -rate #     is the bit-rate (in kbit/s): 40, 32, 24 or 16 (in kbit/s); 
              or a combination of them using dashes (e.g. 32-24 or
	      16-24-32). Default is 32 kbit/s.
  -enc        run only the G.726 encoder on the samples 
              [default: run encoder and decoder]
  -dec        run only the G.726 decoder on the samples 
              [default: run encoder and decoder]
  -noreset    don't apply reset to the encoder/decoder
  -?/-help    print help message

  Example:
  $ v3w -law a -enc -rate 24 original_sample.wav output.wav 256 1 256

  The command above takes the samples in file "original_sample.wav", already
  in a law format, processes the data through the G726 encoder
  only at a rate of 24 kbit/s, saving them into the file
  "output.wav". The processing starts at block 1 for 256 blocks,
  each block being 256 samples wide.

  Variables:
  ~~~~~~~~~~
  law  	  law to use (either A or u)
  conv    desired processing
  rate    desired rate
  inpfil  input file name;
  outfil  output file name;
  block_size  	  block size;
  first_block  	  first block to process;
  num_blocks  	  no. of blocks to process;

  Exit values:
  ~~~~~~~~~~~~

  ============================================================================
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <ctype.h>

#include <sys/stat.h>

/* ..... G.726 module as include functions ..... */
#include "g726.h"
#include "g711.h"

#define P(x) printf x
void display_usage()
{
  P(("  \n"));
  P(("  Usage:\n"));
  P(("  v3w [-options] InpFile OutFile \n"));
  P(("             [FrameSize [1stBlock [NoOfBlocks [Reset]]]]\n"));
  P(("  where:\n"));
  P(("  InpFile     is the name of the file to be processed;\n"));
  P(("  OutFile     is the name with the processed data;\n"));
  P(("  FrameSize   is the frame size, in number of samples; \n"));
  P(("              [default: 32 samples]\n"));
  P(("  1stBlock    is the number of the first block of the input file\n"));
  P(("              to be processed [default: 1st block]\n"));
  P(("  NoOfBlocks  is the number of blocks to be processed, starting on\n"));
  P(("              block \"1stBlock\" [default: all blocks]\n"));
  P(("\n"));
  P(("  Options:\n"));
  P(("  -law #      the letters A or a for G.711 A-law, letter u for \n"));
  P(("              G.711 u-law, or letter l for linear. If linear is\n"));
  P(("              chosen, A-law is used to compress/expand samples to/from\n"));
  P(("              the G.726 routines. Default is A-law.\n"));
  P(("  -rate #     is the bit-rate (in kbit/s): 40, 32, 24 or 16 (in kbit/s); \n"));
  P(("              Default is 32 kbit/s.\n"));
  P(("  -enc        run only the G.726 encoder on the samples \n"));
  P(("              [default: run encoder and decoder]\n"));
  P(("  -dec        run only the G.726 decoder on the samples \n"));
  P(("              [default: run encoder and decoder]\n"));
  P(("  -?/-help    print help message\n"));
  P(("\n"));

  /* Quit program */
  exit(-1);
}
#undef P
/* .................... End of display_usage() ........................... */


int main(argc, argv)
  int             argc;
  char           *argv[];
{
  G726_state      encoder_state, decoder_state;
  long            block_size = 32, first_block = 1, num_blocks = 0, cur_blk, smpno;
  short           *tmp_buf, *inp_buf, *out_buf, reset=1;
  short           inp_type, out_type;
  char            encode = 1, decode = 1, law[4] = "A";
  /* ITU-T Classification of rate based on 8000 sample/sec
   40 Kbits/sec -> 5 bits
   32 Kbits/sec -> 4 bits
   24 Kbits/sec -> 3 bits
   16 Kbits/sec -> 2 bits
  */
  int             rate = 4;

/* File variables */
  char            FileIn[80], FileOut[80];
  FILE           *Fi, *Fo;
  int             inp, out;
  long            start_byte;

/*
 * ......... PARAMETERS FOR PROCESSING .........
 */

  /* GETTING OPTIONS */

  if (argc < 2)
    display_usage();
  else
  {
    while (argc > 1 && argv[1][0] == '-')
      if (strcmp(argv[1], "-noreset") == 0)
      {
	/* No reset */
	reset = 0;

	/* Update argc/argv to next valid option/argument */
	argv++;
	argc--;
      }
      else if (strcmp(argv[1], "-enc") == 0)
      {
	/* Encoder-only operation */
	encode = 1;
	decode = 0;

	/* Move argv over the option to the next argument */
	argv++;
	argc--;
      }
      else if (strcmp(argv[1], "-dec") == 0)
      {
	/*Decoder-only operation */
	encode = 0;
	decode = 1;

	/* Move argv over the option to the next argument */
	argv++;
	argc--;
      }
      else if (strcmp(argv[1], "-law") == 0)
      {
	/* Define law for operation: A, u, or linear */
	switch (toupper(argv[2][0]))
	{
	case 'A':
	  law[0] = '1';
	  break;
	case 'U':
	  law[0] = '0';
	  break;
	case 'L':
	  law[0] = '2';
	  break;
	default:
	  printf(" Invalid law (A or u)! Aborted...\n");
          return -1;
	}

	/* Move argv over the option to the next argument */
	argv+=2;
	argc-=2;
      }
      else if (strcmp(argv[1], "-rate") == 0)
      {
	/*Define rate(s) for operation */
        rate = atoi(argv[2]);
        switch(rate)
        {
         case 40:
                    rate = 5;
                    break;
         case 32:
                    rate = 4;
                    break;
         case 24:
                    rate = 3;
                    break;
         case 16:
                    rate = 2;
                    break;
         default:
                   printf("\n Unsupported rate. Using Default\n");
                   rate = 4;
                   break;
        }
	/* Move argv over the option to the next argument */
	argv+=2;
	argc-=2;
      }
      else if (strcmp(argv[1], "-?") == 0 || strcmp(argv[1], "-help") == 0)
      {
	/* Print help */
	display_usage();
      }
      else
      {
	fprintf(stderr, "ERROR! Invalid option \"%s\" in command line\n\n",
		argv[1]);
	display_usage();
      }
  }

  /* Now get regular parameters */
  strcpy(FileIn,argv[1]);
  strcpy(FileOut,argv[2]);
  if (argc > 3)
       block_size = atol(argv[3]);
  if (argc > 4)
       first_block = atol(argv[4]);
  if (argc > 5)
       num_blocks = atol(argv[5]);
  /*Display inputs being used*/
  fprintf(stderr,"encode: %d\n",encode);
  fprintf(stderr,"decode: %d\n",decode);
  fprintf(stderr,"rate: %d\n",rate*8);
  fprintf(stderr,"Infile: %s\n",FileIn);
  fprintf(stderr,"Outfile: %s\n",FileOut);
  fprintf(stderr,"block_size: %ld\n",block_size);
  fprintf(stderr,"first block: %ld\n",first_block);
  fprintf(stderr,"num blocks: %ld\n",num_blocks);

  /* Inform user of the compading law used: 0->u, 1->A, 2->linear */
  fprintf(stderr, "Using %s\n",
	  law[0] == '1'? "A-law" : ( law[0] == '0' ? "u-law" : "linear PCM"));

  /* Find starting byte in file */
  start_byte = sizeof(short) * (long) (--first_block) * (long) block_size;

  /* Check if is to process the whole file */
  if (num_blocks == 0)
  {
    struct stat     st;

    /* ... find the input file size ... */
    stat(FileIn, &st);
    num_blocks = ceil((st.st_size - start_byte) / (double)(block_size * sizeof(short)));
  }

  /* Define correct data I/O types */
  if (encode && decode) 
  {  
    inp_type = out_type = (law[0] == '2'? IS_LIN : IS_LOG);
  }
  else if (encode)     
  {
    inp_type = law[0] == '2'? IS_LIN : IS_LOG;
    out_type = IS_ADPCM;
  }
  else     
  {
    inp_type = IS_ADPCM;
    out_type = law[0] == '2'? IS_LIN : IS_LOG;
  }

  /* Force law to be used *by the ADPCM* to A-law, if input is linear */
  if (law[0]=='2')
    law[0]='1';
  if ((inp_buf = (short *) calloc(block_size, sizeof(short))) == NULL) 
     printf("Error in memory allocation!\n");
  if ((out_buf = (short *) calloc(block_size, sizeof(short))) == NULL) 
     printf("Error in memory allocation!\n");
  if ((tmp_buf = (short *) calloc(block_size, sizeof(short))) == NULL) 
     printf("Error in memory allocation!\n");

  /* Opening input file; abort if there's any problem */
  if ((Fi = fopen(FileIn, "rb")) == NULL)
       {
         fprintf(stderr, "Opening In file failed. Exiting\n");
         exit(0);
       }
  inp = fileno(Fi);

  /* Creates output file */
  if ((Fo = fopen(FileOut, "w")) == NULL)
       {
         fprintf(stderr, "Opening out file failed. Exiting\n");
         exit(0);
       }
  out = fileno(Fo);

  /* Move pointer to 1st block of interest */
  if (fseek(Fi, start_byte, 0) < 0l)
       {
         fprintf(stderr, "Moving pointer in Infile to start byte %ld failed. Exiting\n", start_byte);
         exit(0);
       }
  
    /*Read header from the input file and set it in the output
      Header is of 22 words i.e. 44 bytes
      https://ccrma.stanford.edu/courses/422/projects/WaveFormat/
    */
     
   /* Read a block of samples */
    if ((smpno = fread(inp_buf, sizeof(short), 22, Fi)) < 0)
       {
         fprintf(stderr, "Reading header from Infile failed. Exiting\n");
         exit(0);
       }
    /* Write ADPCM output word */
    if ((smpno = fwrite(inp_buf, sizeof(short), smpno, Fo)) < 0)
       {
         fprintf(stderr, "Writing header to Outfile failed. Exiting\n");
         exit(0);
       }

/*
 * ......... PROCESSING ACCORDING TO ITU-T G.726 .........
 */
  /* Reset v3w counters */

  for (cur_blk = 0; cur_blk < num_blocks; cur_blk++)
  {

    /* Read a block of samples */
    if ((smpno = fread(inp_buf, sizeof(short), block_size, Fi)) < 0)
       {
         fprintf(stderr, "Reading from Infile failed. Exiting\n");
         exit(0);
       }

    /* Compress linear input samples */
    if (inp_type == IS_LIN)
    {
      /* Compress using A-law */
      alaw_compress(smpno, inp_buf, tmp_buf);

      /* copy temporary buffer over input buffer */
      memcpy(inp_buf, tmp_buf, sizeof(short) * smpno);
    }

    /* Check if reset is needed */
    reset = (reset == 1 && cur_blk == 0) ? 1 : 0;

    /* Carry out the desired operation */
    if (encode && ! decode)
      G726_encode(inp_buf, out_buf, smpno, law, 
		  rate, reset, &encoder_state);
    else if (decode && !encode)
      G726_decode(inp_buf, out_buf, smpno, law, 
		  rate, reset, &decoder_state);
    else if (encode && decode)
    {
      G726_encode(inp_buf, tmp_buf, smpno, law, 
		  rate, reset, &encoder_state);
      G726_decode(tmp_buf, out_buf, smpno, law, 
		  rate, reset, &decoder_state);
    }

    /* Expand linear input samples */
    if (out_type == IS_LIN)
    {
      /* Compress using A-law */
      alaw_expand(smpno, out_buf, tmp_buf);

      /* copy temporary buffer over input buffer */
      memcpy(out_buf, tmp_buf, sizeof(short) * smpno);
    }

    /* Write ADPCM output word */
    if ((smpno = fwrite(out_buf, sizeof(short), smpno, Fo)) < 0)
       {
         fprintf(stderr, "writing to Outfile failed. Exiting\n");
         exit(0);
       }
  }

  /* Free allocated memory */
  free(tmp_buf);
  free(out_buf);
  free(inp_buf);

  /* Close input and output files */
  fclose(Fi);
  fclose(Fo);

  return (0);
}
/* ............................. end of main() ............................. */

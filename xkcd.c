/* skeinsum.c -- command-line driver for skein.
   Copyright (C) 2011 Casey Marshall. */
   

#include <errno.h>
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <skeinApi.h>
#include <time.h>
#include <sys/time.h>
#include <assert.h>

#define BUFFER_SIZE 4 * 1024
#define BITS_TO_BYTES(nbits) (((nbits) + 7) / 8)

char *to_hex(uint8_t *digest, size_t digestlen);
int compare(uint8_t *digest, size_t digestlen);

static const char *progname = "skeinsum";

static const uint8_t reference[] = {0x5b,0x4d,0xa9,0x5f,0x5f,0xa0,0x82,0x80,0xfc,0x98,0x79,0xdf,0x44,0xf4,0x18,0xc8,0xf9,0xf1,0x2b,0xa4,0x24,0xb7,0x75,0x7d,0xe0,0x2b,0xbd,0xfb,0xae,0x0d,0x4c,0x4f,0xdf,0x93,0x17,0xc8,0x0c,0xc5,0xfe,0x04,0xc6,0x42,0x90,0x73,0x46,0x6c,0xf2,0x97,0x06,0xb8,0xc2,0x59,0x99,0xdd,0xd2,0xf6,0x54,0x0d,0x44,0x75,0xcc,0x97,0x7b,0x87,0xf4,0x75,0x7b,0xe0,0x23,0xf1,0x9b,0x8f,0x40,0x35,0xd7,0x72,0x28,0x86,0xb7,0x88,0x69,0x82,0x6d,0xe9,0x16,0xa7,0x9c,0xf9,0xc9,0x4c,0xc7,0x9c,0xd4,0x34,0x7d,0x24,0xb5,0x67,0xaa,0x3e,0x23,0x90,0xa5,0x73,0xa3,0x73,0xa4,0x8a,0x5e,0x67,0x66,0x40,0xc7,0x9c,0xc7,0x01,0x97,0xe1,0xc5,0xe7,0xf9,0x02,0xfb,0x53,0xca,0x18,0x58,0xb6};

/*static const char *printable = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ";*/
static const char *spacings[] = {" ","+","%20",""};
static const int spacelen[] = {1, 1, 2, 0};
static const int nspacings = 4;

int main(int argc, char * const * argv)
{
    int c;
    const char *mode = "r";
    int check = 0;
    int quiet = 0;
    int status = 0;
    int warn_lines = 0;
    SkeinSize_t size = Skein1024;
    size_t bitlen = 1024;
    SkeinCtx_t skein;
    unsigned nwords = 0;
    char **words = NULL;
    int counter = 0;
    time_t oldtime = time(NULL);
    int threshold;

    if ( argc != 2 ) {
      printf("Usage: %s <threshold>\n", argv[0]);
      exit(1);
    }
    threshold = atoi(argv[1]);
    assert(threshold >= 0 && threshold <= 1024);

    /* Seed rand */
    {
      struct timeval tv;
      gettimeofday(&tv,NULL);
      srand((tv.tv_sec * 1000) + (tv.tv_usec / 1000));
    }

    /* Load words */
    {
      FILE *fh = fopen("/usr/share/dict/words", "r");
      unsigned wordsize = 1000;
      words = malloc(wordsize*sizeof(words[0]));
      while ( 1 ) { 
        char *word = malloc(256);
        assert(word);
        if ( !fgets(word, 256, fh) || strlen(word) <= 0) break;
        word[strlen(word)-1] = 0; /* Remove newline */
        if ( nwords >= wordsize ) {
          wordsize *= 2;
          printf("  reallocing to %p to %d\n", words, wordsize);
          words = realloc(words, wordsize*sizeof(words[0]));
          assert(words);
        }
        words[nwords] = word;
        nwords++;
      }
      printf("Read %u (< %u) words: %s-%s\n", nwords, wordsize, words[0], words[nwords-1]); 
    }
    assert(nwords < RAND_MAX);

    /* Initialize skein */
    if (skeinCtxPrepare(&skein, size) != SKEIN_SUCCESS)
    {
        fprintf(stderr, "%s: error initializing Skein context.\n", progname);
        exit(EXIT_FAILURE);
    }
    if (skeinInit(&skein, bitlen) != SKEIN_SUCCESS)
    {
        fprintf(stderr, "%s: failed to initialize skein context for parameters %d, %d\n", progname, size, bitlen);
        exit(EXIT_FAILURE);
    }
    
    while ( 1 ) {
      int errcode = 0;
      uint8_t digest[BITS_TO_BYTES(1024)];
      char buffer[4096] = "", buflen = 0;
      time_t now = time(NULL);
      int len = rand() % 4 + 1;
      int wordchoices[1024];
      int i = 0, space = 0;
      /* Choose words */
      for ( i = 0; i < len; i++ )
        wordchoices[i] = rand() % nwords;

      for ( space=0;space<(len > 1 ? nspacings : 2);space++ ) {
        /* Generate buffer */
        buffer[0] = 0;
        for ( i = 0; i < len; i++ ) {
          if ( i > 0 ) strcat(buffer, spacings[space]);
          strcat(buffer, words[wordchoices[i]]);
        }
        skeinUpdate(&skein, buffer, strlen(buffer));
        if ((errcode = skeinFinal(&skein, digest)) != SKEIN_SUCCESS)
        {
          fprintf(stderr, "%s: skeinFinal error %d\n", progname, errcode);
          return 1;
        }
        else
        {
            int score = compare(digest, bitlen);
            if ( score < threshold ) {
              printf("Got %d:\necho -n hashable='%s' | POST http://almamater.xkcd.com/?edu=fixme\n%s\n", score, buffer, to_hex(digest, bitlen));
              exit(0);
            }
        }
        skeinReset(&skein);

        if ( now != oldtime ) {
          printf("H/s: %d '%s'\n", counter, buffer);
          oldtime = now;
          counter = 0;
        }
        counter++;
      }      
    }

    exit(0);
}

static char hex_buffer[BITS_TO_BYTES(1024) * 2 + 1];

char *to_hex(uint8_t *digest, size_t digestlen)
{
    int i;
    char *b = hex_buffer;
    b[0] = '\0';
    for (i = 0; i < BITS_TO_BYTES(digestlen) && i < BITS_TO_BYTES(1024); i++)
    {
        sprintf(b, "%02x", digest[i]);
        b += 2;
    }
    return hex_buffer;
}

int compare(uint8_t *digest, size_t digestlen)
{
    int i;
    int score = 0;
    for (i = 0; i < BITS_TO_BYTES(digestlen) && i < BITS_TO_BYTES(1024); i++) {
      uint8_t j = digest[i] ^ reference[i];
      while ( j ) {
        score += j&1;
        j >>= 1;
      }
    }
    return score;
}

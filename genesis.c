// Copyright "Remember remember the 5th of November" 2013

#include <assert.h>
#include <ctype.h>
#include <inttypes.h>
#include <openssl/sha.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

// Copied from Bitcoin source
static const uint64_t COIN = 100000000;

static const uint32_t OP_CHECKSIG = 172; // This is expressed as 0xAC

typedef struct {
  /* Hash of Tx */
  uint8_t merkleHash[32];

  /* Tx serialization before hashing */
  uint8_t *serializedData;

  /* Tx version */
  uint32_t version;

  /* Input */
  uint8_t numInputs; // Program assumes one input
  uint8_t prevOutput[32];
  uint32_t prevoutIndex;
  uint8_t *scriptSig;
  uint32_t sequence;

  /* Output */
  uint8_t numOutputs; // Program assumes one output
  uint64_t outValue;
  uint8_t *pubkeyScript;

  /* Final */
  uint32_t locktime;
} Transaction;

// Got this off the internet. Am not sure if it can fail in some circumstances
void byteswap(uint8_t *buf, int length) {
  int i;
  uint8_t temp;

  for (i = 0; i < length / 2; i++) {
    temp = buf[i];
    buf[i] = buf[length - i - 1];
    buf[length - i - 1] = temp;
  }
}

// Following two functions are borrowed from cgminer.
char *bin2hex(const unsigned char *p, size_t len) {
  char *s = malloc((len * 2) + 1);
  unsigned int i;

  if (!s)
    return NULL;

  for (i = 0; i < len; i++)
    sprintf(s + (i * 2), "%02x", (unsigned int)p[i]);

  return s;
}

size_t hex2bin(unsigned char *p, const char *hexstr, size_t len) {
  int ret = 0;
  size_t retlen = len;

  while (*hexstr && len) {
    char hex_byte[4];
    unsigned int v;

    if (!hexstr[1]) {
      return ret;
    }

    memset(hex_byte, 0, 4);
    hex_byte[0] = hexstr[0];
    hex_byte[1] = hexstr[1];

    if (sscanf(hex_byte, "%x", &v) != 1) {
      return ret;
    }

    *p = (unsigned char)v;

    p++;
    hexstr += 2;
    len--;
  }

  if (len == 0 && *hexstr == 0)
    ret = retlen;

  return ret;
}

Transaction *InitTransaction() {
  Transaction *transaction;

  transaction = calloc(1, sizeof(*transaction));
  if (!transaction) {
    return NULL;
  }

  // Set some initial data that will remain constant throughout the program
  transaction->version = 1;
  transaction->numInputs = 1;
  transaction->numOutputs = 1;
  transaction->locktime = 0;
  transaction->prevoutIndex = 0xFFFFFFFF;
  transaction->sequence = 0xFFFFFFFF;
  transaction->outValue = 50 * COIN;

  // We initialize the previous output to 0 as there is none
  memset(transaction->prevOutput, 0, 32);

  return transaction;
}

int main(int argc, char *argv[]) {
  Transaction *transaction;
  char timestamp[255];
  uint32_t timestamp_len = 0;
  uint32_t nBits = 0;
  uint32_t startNonce = 0;
  uint32_t unixtime = 0;
  uint32_t numKeystones = 0;

  if ((argc - 1) < 3) {
    fprintf(stderr, "Usage: %s <nBits> <num_keystones> <startNonce>\n",
            argv[0]);
    return 0;
  }

  timestamp_len = strlen(argv[2]);

  if (timestamp_len <= 0) {
    fprintf(stderr, "Size of timestamp is 0!\n");
    return 1;
  }
  if (timestamp_len > 91) {
    fprintf(stderr,
            "Size of timestamp exceeds maximum length of 91 characters!\n");
    return 1;
  }

  transaction = InitTransaction();
  if (!transaction) {
    fprintf(stderr, "Could not allocate memory! Exiting...\n");
    return 2;
  }

  strncpy(timestamp, argv[2], sizeof(timestamp));
  sscanf(argv[1], "%lu", (long unsigned int *)&nBits);
  sscanf(argv[2], "%lu", (long unsigned int *)&numKeystones);
  char *endptr = NULL;
  if (argc > 5) {
    startNonce = strtoul(argv[3], &endptr, 0);
    if (!endptr) {
      fprintf(stderr, "Invalid start nonce: %s\n", argv[3]);
      return 1;
    }
  }
  unixtime = 1337;
  printf("nBits: 0x%x\nnum_keystones %u\nstartNonce: %u\nunixtime: %u\n", nBits,
         numKeystones, startNonce, unixtime);

  {
    printf("Generating block...\n");

    unsigned char block_header[80], block_hash1[32], block_hash2[32];
    memcpy(block_header, "\x02\x00\x00\x00", 4);
    memset(block_header + 4, 0, 32);
    memcpy(block_header + 36,
           "6cd52f4d96544efa90b1053156a5d3035d30c9d57b3af15d6e4d0886e5f9e4fc",
           32);
    memcpy(block_header + 68, &unixtime, 4);
    memcpy(block_header + 72, &nBits, 4);
    memcpy(block_header + 76, &startNonce, 4);

    uint32_t *pNonce = (uint32_t *)(block_header + 76);
    uint32_t *pUnixtime = (uint32_t *)(block_header + 68);
    unsigned int counter, start = time(NULL);
    while (1) {
      SHA256(block_header, 80, block_hash1);
      SHA256(block_hash1, 32, block_hash2);

      unsigned int check =
          *((uint32_t *)(block_hash2 + 28)); // The hash is in little-endian, so
                                             // we check the last 4 bytes.
      if (check == 0)                        // \x00\x00\x00\x00
      {
        byteswap(block_hash2, 32);
        char *blockHash = bin2hex(block_hash2, 32);
        printf("\nBlock found!\nHash: %s\nNonce: %u\nUnix time: %u", blockHash,
               startNonce, unixtime);
        free(blockHash);
        break;
      }

      startNonce++;
      counter += 1;
      if (time(NULL) - start >= 1) {
        printf("\r%d Hashes/s, Nonce %u\r", counter, startNonce);
        counter = 0;
        start = time(NULL);
      }
      *pNonce = startNonce;
      if (startNonce > 4294967294LL) {
        unixtime++;
        *pUnixtime = unixtime;
        startNonce = 0;
      }
    }
  }

  free(transaction->serializedData);
  free(transaction->scriptSig);
  free(transaction->pubkeyScript);
  free(transaction);

  return 0;
}

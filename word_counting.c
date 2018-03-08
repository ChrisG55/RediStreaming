/*
 * Copyright (C) 2018 Christian Göttel
 *
 * This file is part of RedisStreaming.
 *
 * RedisStreaming is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * RedisStreaming is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with RedisStreaming.  If not, see <https://www.gnu.org/licenses/>.
 */

#include <math.h>
#include <string.h>

#define NDEBUG
#include <assert.h>

#include "word_counting.h"

typedef struct wc {
  char *word;
  unsigned long count;
  size_t len;
  struct wc *next;
} wc_t;

/**
 * Bernstein hash djb2
 */
unsigned int bernstein(char *str) {
  char *c;
  unsigned int hash = 5381;
  for (c = str; *c != '\0'; c++)
    hash = hash * 33 + *c; // allow integer overflows, avoids modulo a large
			   // prime number
  
  return hash;
}

/**
 * 
 *
 * Pub/sub output:
 *   <word>,<count>,...
 */
int WordCount(RedisModuleCtx *ctx, RedisModuleString **argv, int argc) {
  int i, ret = REDISMODULE_OK;
  size_t msglen;
  wc_t *head = NULL;
  wc_t *tail = NULL;

  // Allocate the key's string and open it
  // TODO: a static Redis string for the key
  RedisModuleString *keystr = RedisModule_CreateString(ctx, "_wordcounting", 13);
  RedisModuleKey *key = RedisModule_OpenKey(ctx, keystr, REDISMODULE_READ | REDISMODULE_WRITE);

  // Account for additional separators due to multiple strings and the
  // terminating null byte.
  msglen = argc - 1 + 1;
  for (i = 0; i < argc; i++) {
    size_t len, words = 0;
    // Retrieve the C string from the argument vector
    const char *str = RedisModule_StringPtrLen(argv[i], &len);
    // Allocate a copy of the C string
    char *strc = RedisModule_Strdup(str);
    if (strc == NULL)
      return REDISMODULE_ERR;
    // Split the C string into words using SPACE " " as delimiter
    char *word = strsep(&strc, " ");
    for ( ; word != NULL; word = strsep(&strc, " ")) {
      int flags = 0;
      double clen, score;
      // Allocate and initialize a word count list element
      // TODO: check for allocation error
      if (head == NULL) {
	head = (wc_t *)RedisModule_Calloc(1, sizeof(wc_t));
	if (head == NULL)
	  return REDISMODULE_ERR;
	tail = head;
      } else {
	tail->next = (wc_t *)RedisModule_Calloc(1, sizeof(wc_t));
	if (tail->next == NULL)
	  return REDISMODULE_ERR;
	tail = tail->next;
      }
      tail->word = word;
      // Compute the length of the word
      len = strlen(word);
      // Create a Redis string for the word
      RedisModuleString *rstr = RedisModule_CreateString(ctx, word, len);
      if (rstr == NULL)
	return REDISMODULE_ERR;
      // Add the word to a sorted set and increment the count (aka score)
      ret = RedisModule_ZsetIncrby(key, 1, rstr, &flags, &score);
      tail->count = (unsigned long)score;
      // Free the Redis string
      RedisModule_FreeString(ctx, rstr);
      // Determine the length of the integer value
      clen = log10(score);
      // Add the lengths (word, separator and count) to the pub/sub's message
      // length
      tail->len = len + (size_t)clen + 1;
      msglen += tail->len + 1;
      words++;
    }
    assert(strc == NULL);
    // Add separators between words
    msglen += words - 1;
  }

  // Close the key and free the its string
  RedisModule_CloseKey(key);
  RedisModule_FreeString(ctx, keystr);

  // Allocate memory for the pub/sub message
  char *msg = (char *)RedisModule_Calloc(msglen, sizeof(char));
  if (msg == NULL)
    return REDISMODULE_ERR;

  // For each word count list element write the data to the message and free the
  // allocated space.
  wc_t *ptr = head;
  char *mptr = msg;
  while (ptr != NULL) {
    snprintf(mptr, ptr->len + 2,"%s,%lu", ptr->word, ptr->count);
    if (ptr->next != NULL) {
      mptr[ptr->len + 1] = ',';
      mptr += ptr->len + 2;
    }
    head = ptr->next;
    /* RedisModule_Free(ptr->word); // DO NOT FREE! Redis keeps a reference to this */
    RedisModule_Free(ptr);
    ptr = head;
  }

  assert(head == NULL && ptr == NULL);
  // NOTE: head & ptr == NULL, but tail != NULL

  // Stream the results via pub/sub
  RedisModuleCallReply *r = RedisModule_Call(ctx, "PUBLISH", "cc", "streaming", msg);
  RedisModule_FreeCallReply(r);
  RedisModule_Free(msg);

  return ret;
}

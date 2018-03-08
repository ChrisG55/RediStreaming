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

#include <stdlib.h>
#include <string.h>

#include "version.h"
#include "streaming.h"
// TODO: implement extensions that can dynamically added or removed from a module
#include "word_counting.h"

// Filters classed by Redis module key type
filter_t *filters[7];

// TODO: implement a pattern matching algorithm
int match(const char *pattern, const char *token) {
  if ((strlen(pattern) == 1) && (pattern[0] == '*'))
     return 1; // match
  return 0; // mismatch
}

filter_t *getFilter(int type, const char *key) {
  filter_t *f;

  for (f = filters[type]; f->next != NULL; f = f->next);
  if (match(f->kpattern, key))
    return f;
 
  return NULL;
}

/**
 * Adds a new filter to the list of filters classed by Redis key type.
 */
int Add(RedisModuleCtx *ctx, RedisModuleString **argv, int argc) {
  const char *kpattern, *fpattern;
  int rmkt = REDISMODULE_KEYTYPE_EMPTY; // Redis module key type
  size_t len;
  filter_t *filter;

  const char *type = RedisModule_StringPtrLen(argv[1], &len);

  filter = (filter_t *)RedisModule_Calloc(1, sizeof(filter_t));

  if (strncmp(type, "HASH", 4) == 0) {
    /*
     * argv: STREAM ADD HASH <function> <key?> <field>...
     * argc:   -1    0   1       2        3       4
     */
    kpattern = (const char *)RedisModule_StringPtrLen(argv[3], &len);
    fpattern = (const char *)RedisModule_StringPtrLen(argv[4], &len);
    rmkt = REDISMODULE_KEYTYPE_HASH;
    filter->func = &WordCount;
    filter->kpattern = (const char *)RedisModule_Strdup(kpattern);
    filter->fpattern = (const char *)RedisModule_Strdup(fpattern);
  } else if (strncmp(type, "STRING", 6) == 0) {
    /*
     * argv: STREAM ADD STRING <function> <key?>
     * argc:   -1    0    1        2        3
     */
    kpattern = (const char *)RedisModule_StringPtrLen(argv[3], &len);
    rmkt = REDISMODULE_KEYTYPE_STRING;
    filter->func = &WordCount;
    filter->kpattern = (const char *)RedisModule_Strdup(kpattern);
  } else {
    // TODO: return a type error
    RedisModule_Free(filter);
    return REDISMODULE_ERR;
  }

  // Check for duplicate filters and insert the new filter
  if (filters[rmkt] == NULL)
    filters[rmkt] = filter;
  else {
    filter_t *f;
    for (f = filters[rmkt]; f->next != NULL; f = f->next) {
      if (f->func == filter->func) {
	if (strncmp(f->kpattern, filter->kpattern, strlen(f->kpattern)) == 0) {
	  if (strncmp(f->fpattern, filter->fpattern, strlen(f->fpattern)) == 0) {
	    // Duplicate filter found - nothing to do
	    RedisModule_Free(filter);
	    return REDISMODULE_OK;
	  }
	}
      }
    }

    f->next = filter;
  }

  RedisModule_ReplyWithSimpleString(ctx, "OK");

  return REDISMODULE_OK;
}

int StreamCall(RedisModuleCtx *ctx, RedisModuleString **argv, int argc) {
  size_t len;
  const char *cmd = RedisModule_StringPtrLen(argv[0], &len);
  const char *token;
  int i, idx = 0, ret = REDISMODULE_OK;
  filter_t *f;

  // Call the command before streaming
  RedisModuleCallReply *r = RedisModule_Call(ctx, cmd, "v", &argv[1], argc - 1);
  RedisModule_ReplyWithCallReply(ctx, r);
  RedisModule_FreeCallReply(r);

  // Stream
  if ((strncmp(cmd, "SET", 3) == 0) &&
      filters[REDISMODULE_KEYTYPE_STRING] != NULL) {
    token = RedisModule_StringPtrLen(argv[1], &len);
    f = getFilter(REDISMODULE_KEYTYPE_STRING, token);
    if (f != NULL) {
      // Key matches
      ret = f->func(ctx, &argv[2], 1);
    }
  } else if ((strncmp(cmd, "HSET", 4) == 0) &&
	     filters[REDISMODULE_KEYTYPE_HASH] != NULL) {
    token = RedisModule_StringPtrLen(argv[1], &len);
    f = getFilter(REDISMODULE_KEYTYPE_HASH, token);
    if (f != NULL) {
      // Key matches, now match the field
      token = RedisModule_StringPtrLen(argv[2], &len);
      if (match(f->fpattern, token)) {
	ret = f->func(ctx, &argv[3], 1);
      }
    }
  } else if ((strncmp(cmd, "HMSET", 5) == 0) &&
	     filters[REDISMODULE_KEYTYPE_HASH] != NULL) {
    token = RedisModule_StringPtrLen(argv[1], &len);
    f = getFilter(REDISMODULE_KEYTYPE_HASH, token);
    if (f != NULL) {
      RedisModuleString **args;
      // Key matches, now match the fields
      args = (RedisModuleString **)RedisModule_Calloc((argc - 2) / 2, sizeof(RedisModuleString *));
      if (args == NULL)
	return REDISMODULE_ERR;
      for (i = 2; i < argc; i+=2) {
	token = RedisModule_StringPtrLen(argv[i], &len);
	if (match(f->fpattern, token)) {
	  args[idx++] = argv[i + 1];
	}
      }
      if (idx)
	ret = f->func(ctx, args, idx);
      RedisModule_Free(args);
    }
  }

  return ret;
}

int Stream(RedisModuleCtx *ctx, RedisModuleString **argv, int argc) {
  const char *cmd;
  int ret;
  size_t len;

  cmd = RedisModule_StringPtrLen(argv[1], &len);
  if(strncmp(cmd, "ADD", 3) == 0) {
    /*
     * Redis streaming command - adding a filter
     * STREAM ADD {HASH|STRING} <function> <key> <field>...
     */
    ret = Add(ctx, &argv[1], argc - 1);
  } else if (strncmp(cmd, "FUNCTIONS", 9) == 0) {
    /*
     * Redis streaming command - list available functions
     * STREAM FUNCTIONS
     */
    // TODO: currently hardcoded
    ret = RedisModule_ReplyWithArray(ctx, 1);
    ret |= RedisModule_ReplyWithStringBuffer(ctx, "Foo bar baz", 11);
  } else {
    /*
     * Redis streaming command - stream and execute
     * STREAM <CMD> <ARGS>
     * Example: STREAM HSET <hash> <field> <value>
     */
    ret = StreamCall(ctx, &argv[1], argc - 1);
  }
  
  return ret;
}

int RedisModule_OnLoad(RedisModuleCtx *ctx, RedisModuleString **argv, int argc) {
    if (RedisModule_Init(ctx,"streaming",STREAMING_MODULE_VERSION,REDISMODULE_APIVER_1)
        == REDISMODULE_ERR) return REDISMODULE_ERR;

    if (RedisModule_CreateCommand(ctx, "STREAM", Stream, "write deny-oom pubsub", 1, 1, 1) == REDISMODULE_ERR)
        return REDISMODULE_ERR;

    return REDISMODULE_OK;
}

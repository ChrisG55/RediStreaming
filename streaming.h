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

#ifndef STREAMING_H
#define STREAMING_H

#include "redismodule.h"

typedef int (*RedisModuleStreamingFunc) (RedisModuleCtx *ctx, RedisModuleString **argv, int argc);

typedef struct filter {
  RedisModuleStreamingFunc func; // function to execute for streaming
  const char *kpattern; // key pattern to apply the function to
  const char *fpattern; // HASH only: field pattern
  struct filter *next; // Points to the next filter in the single-linked list
} filter_t;

#endif

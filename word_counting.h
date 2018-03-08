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

#ifndef WORD_COUNTING_H
#define WORD_COUNTING_H

#include "streaming.h"

int WordCount(RedisModuleCtx *, RedisModuleString **, int);

#endif

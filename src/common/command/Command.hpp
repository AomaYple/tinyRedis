#pragma once

enum class Command : unsigned char {
    select,
    del,
    exists,
    move,
    rename,
    renamenx,
    type,
    set,
    get,
    getRange,
    mget,
    setnx,
    setRange,
    strlen,
    mset,
    msetnx,
    incr,
    incrby,
    decr,
    decrBy,
    append,
    hdel,
    hexists,
    hget
};

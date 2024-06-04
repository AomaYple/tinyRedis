#pragma once

enum class Command : unsigned char {
    select,
    del,
    dump,
    exists,
    move,
    rename,
    renamenx,
    type,
    set,
    get,
    getRange,
    getSet,
    mget,
    setnx,
    setRange,
    strlen
};

#pragma once

enum class Command : unsigned char {
    select,
    del,
    dump,
    exists,
    move,
    rename,
    renamenx,
    get,
};

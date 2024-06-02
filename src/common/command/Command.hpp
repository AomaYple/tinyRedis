#pragma once

enum class Command : unsigned char {
    select,
    del,
    dump,
    move,
    exists,
    get,
};

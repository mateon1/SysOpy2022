// Mateusz Naściszewski, 2022
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <assert.h>

typedef struct hashcell {
    char *key;
    void *val;
} _hashcell;

typedef struct hashmap {
    hashcell *table;
    unsigned size; // size always a power of two
    unsigned elems;
    unsigned tombs;
    void (*elem_free)(void*);
} hashmap;

static uint64_t hashfn(char *str) {
    uint64_t ret = 0xB5F3EC974D101806;
    while (*str) {
        uint64_t part = 0x5555555555555555;
        for (int i = 0; i < 8 && *str; str++)
            part ^= ((unsigned)*str << (8 * i));
        ret ^= part;
        ret ^= (ret << 13);
        ret ^= (ret >> 7);
        ret ^= (ret << 17);
    }
    return ret;
}

static hashcell *_hash_get_cell(hashmap m, char *key) {
    assert(m.size > 0);
    assert(m.elems < m.size);
    assert(m.size & (m.size-1) == 0); // power of two
    uint64_t hash = hashfn(key);
    uint64_t mask = m.size - 1;
    uint64_t inc = ((hash >> 5) - hash) | 1;
    uint64_t idx = hash;
    _hashcell *res;
    while ((res = &m.table[idx & mask]) && res.key != NULL) {
        if (strcmp(key, res.key) == 0) {
            return res;
        }
        idx += inc; // bad for cache locality, but meh
    }
    return res; // empty slot
}

static void hash_add(hashmap m, char *key, void *val) {
    _hashcell *res = _hash_get_cell(m, key);
    if (res.key != NULL) {
        // old value, just rewrite the value
        if (m.val_free && res.val) m.val_free(res.val);
        res.val = val;
    } else {
        // TODO: Resizing, size accounting, etc.
    }
}

int main(int argc, char** argv) {
    if (argc != 2) {
        printf("Usage: %s <input file>\n", argc > 0 ? argv[0] : "main");
        exit(2);
    }

    return 0;
}

#define _GNU_SOURCE
#include <elf.h>
#include <link.h>
#include <stdio.h>
#include <string.h>

extern const void __executable_start; // provided by the ld's default linker-script

static const char* library_name = "./libtest.so";
static const char* function_name = "test_function";

// function the linker uses for the hashes in the GNU_HASH section
static uint32_t gnu_hash(const char* s)
{
    uint32_t h = 5381;
    for (unsigned char c = *s; c != '\0'; c = *++s)
    {
        h = h * 33 + c;
    }

    return h & 0xffffffff;
}

static const struct link_map* get_link_map()
{
    // The symbol itself marks the the start of the executable, it's not a pointer
    const Elf32_Ehdr* ehdr = &__executable_start;
    const Elf32_Phdr* phdr = &__executable_start + ehdr->e_phoff;

    for (; phdr->p_type != PT_DYNAMIC; phdr++);

    const Elf32_Dyn* dyn = (const Elf32_Dyn*)phdr->p_vaddr;
    for (; dyn->d_tag != DT_PLTGOT; dyn++);

    // Yay! We got the global offset table!
    void** got = (void*)dyn->d_un.d_ptr;

    // get the second GOT-entry, it's a pointer to the link map
    return got[1];
}

static const struct link_map* link_map_entry_for_library(const char* library)
{
    for (const struct link_map* map = get_link_map(); map != NULL; map = map->l_next)
    {
        if (map->l_name && strcmp(basename(map->l_name), basename(library)) == 0)
        {
            return map;
        }
    }

    fprintf(stderr, "failed to locate link-map-entry for library %s\n", library);
    return NULL;
}

static const void* get_table(const struct link_map* map, int type)
{
    for (const Elf32_Dyn* dyn = (const Elf32_Dyn*)map->l_ld; dyn->d_tag != DT_NULL; dyn++)
    {
        if (dyn->d_tag == type)
        {
            return (void*)dyn->d_un.d_ptr;
        }
    }

    fprintf(stderr, "failed to locate dyn-section of type %i for library %s\n", type, map->l_name);
    return NULL;
}

static void* resolve_symbol(const char* library, const char* symbol)
{
    const struct link_map* map = link_map_entry_for_library(library);

    if (!map)
    {
        return NULL;
    }

    const uint32_t* hashtab = get_table(map, DT_GNU_HASH);
    const Elf32_Sym* symtab = get_table(map, DT_SYMTAB);
    const char* strtab = get_table(map, DT_STRTAB);

    uint32_t nbuckets = hashtab[0];
    uint32_t symndx = hashtab[1];
    uint32_t maskwords = hashtab[2];
    // index 3 is used for a 'fast-reject-filter' we just ignore
    const uint32_t* buckets = &hashtab[4 + maskwords];
    const uint32_t* values = &hashtab[4 + maskwords + nbuckets];

    uint32_t hash = gnu_hash(symbol);

    uint32_t chain = buckets[hash % nbuckets];
    if (chain == 0)
    {
        fprintf(stderr, "hash of symbol %s in %s refers to empty chain\n", symbol, library);
        return NULL;
    }

    const Elf32_Sym* sym = &symtab[chain];
    const uint32_t* chain_ptr = &values[chain - symndx];
    do
    {
        if ((hash & ~1) == (*chain_ptr & ~1) && strcmp(symbol, strtab + sym->st_name) == 0)
        {
            return (void*)(map->l_addr + sym->st_value);
        }

        sym += 1;
        chain_ptr += 1;
    } while (*chain_ptr & 1); // last bit is set for all but the last entry of the chain

    fprintf(stderr, "failed to resolve symbol %s in %s\n", symbol, library);
    return NULL;
}

int main(int argc, char** argv)
{
    void* dlopen_handle = dlopen(library_name, RTLD_NOW);
    if (!dlopen_handle)
    {
        fprintf(stderr, "failed to dlopen() %s(): %s\n", library_name, dlerror());
        return 1;
    }

    void (*dlsymloaded_func)() = dlsym(dlopen_handle, function_name);
    if (!dlsymloaded_func)
    {
        fprintf(stderr, "failed to dlsym() for %s(): %s\n", function_name, dlerror());
        return 1;
    }

    void (*selfloaded_func)() = resolve_symbol(library_name, function_name);
    if (!selfloaded_func)
    {
        fprintf(stderr, "failed to locate %s()\n", function_name);
        return 1;
    }

    fprintf(stderr, "calling %s() by pointer from dlsym():\n", function_name);
    dlsymloaded_func();

    fprintf(stderr, "calling %s() by programically located pointer):\n", function_name);
    selfloaded_func();
}


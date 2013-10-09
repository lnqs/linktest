#include <elf.h>
#include <link.h>
#include <stdio.h>
#include <string.h>
#include <libgen.h>

extern const void __executable_start; // provided by the ld's default linker-script

// defines the libraries and symbols to find as test
struct symbol_def
{
    const char* library;
    const char* symbol;
};

static const struct symbol_def symbol_defs[] = {
    { "libSDL-1.2.so.0", "SDL_Init" }
};

// Searches and returns the link_map of the dynamic linker
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

    printf("found GOT at %p\n", got);

    // get the second GOT-entry, it's a pointer to the link map
    return got[1];
}

static void get_tables(const struct link_map* map,
        const Elf32_Sym** sym, const char** str, unsigned long* nchains)
{
    *sym = NULL;
    *str = NULL;
    *nchains = 0;

    for (const Elf32_Dyn* dyn = (const Elf32_Dyn*)map->l_ld; dyn->d_tag != DT_NULL; dyn++)
    {
        switch (dyn->d_tag)
        {
            case DT_HASH:
            case DT_GNU_HASH:
                printf("found hashtab at 0x%x\n", dyn->d_un.d_ptr + 4);
                *nchains = *(unsigned long*)(dyn->d_un.d_ptr + 4);
                break;

            case DT_SYMTAB:
                printf("found symtab at 0x%x\n", dyn->d_un.d_ptr);
                *sym = (const Elf32_Sym*)dyn->d_un.d_ptr;
                break;

            case DT_STRTAB:
                printf("found strtab at 0x%x\n", dyn->d_un.d_ptr);
                *str = (const char*)dyn->d_un.d_ptr;
                break;
        }
    }
}

static void* resolve_symbol(const char* library, const char* symbol)
{
    for (const struct link_map* map = get_link_map(); map != NULL; map = map->l_next)
    {
        printf("found link-map entry for '%s'\n", map->l_name);

        if (map->l_name && strcmp(basename(map->l_name), library) == 0)
        {
            const Elf32_Sym* sym;
            const char* str;
            unsigned long nchains;

            get_tables(map, &sym, &str, &nchains);

            for (size_t i = 0; i < nchains; i++)
            {
                printf("found symbol '%s' at 0x%x\n", &str[sym[i].st_name], sym[i].st_value);
                if (strcmp(&str[sym[i].st_name], symbol) == 0)
                {
                    return (void*)sym[i].st_value;
                }
            }

            return NULL;
        }
    }

    return NULL;
}

int main(int argc, char** argv)
{
    for (size_t i = 0; i < sizeof(symbol_defs) / sizeof(struct symbol_def); i++)
    {
        void* dlopen_handle = dlopen("libSDL-1.2.so.0", RTLD_NOW);
        void* dlsym_address = dlsym(dlopen_handle, symbol_defs[i].symbol);

        void* address = resolve_symbol(symbol_defs[i].library, symbol_defs[i].symbol);

        printf("programically located address %p of %s (in %s) %s address %p from dlsym() ",
                address, symbol_defs[i].symbol, symbol_defs[i].library,
                address == dlsym_address ? "matches" : "doesn't match",
                dlsym_address);
    }
}


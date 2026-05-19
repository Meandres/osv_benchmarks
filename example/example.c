/*
 * Example application built directly into the OSv kernel image.
 *
 * Compile with: make APP_OBJECTS="benchmarks/example/example.o"
 *
 * For C++ apps, rename to example.cc and use:
 *   extern "C" int app_main(int argc, char** argv) { ... }
 */

#include <stdio.h>
#include <osv/application.hh>

int app_main(int argc, char** argv)
{
    printf("Hello from app_main!\n");
    printf("argc = %d\n", argc);
    for (int i = 0; i < argc; i++) {
        printf("  argv[%d] = %s\n", i, argv[i]);
    }
    return 0;
}

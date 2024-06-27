#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include "debug.h"
#include "hex_dump.h"
#include "limits.h"
#include "round.h"

void create(char* command);
void dumpdata(char* command);
void delete(char* command);

void list_func(char arg[][30], int option);
void bitmap_func(char arg[][30], int option);
void hash_func(char arg[][30], int option);

#include "bitmap.h"
#include <assert.h>	
#include "limits.h"	
#include "round.h"	
#include <stdio.h>
#include <stdlib.h>	
#include <string.h>

#define ASSERT(CONDITION) assert(CONDITION)	

/* Element type.

   This must be an unsigned integer type at least as wide as int.

   Each bit represents one bit in the bitmap.
   If bit 0 in an element represents bit K in the bitmap,
   then bit 1 in the element represents bit K+1 in the bitmap,
   and so on. */
typedef unsigned long elem_type;

const char* bitmap_command_name[]  = {
    "bitmap_create", "bitmap_create_in_buf", "bitmap_buf_size", "bitmap_destroy", //create & destruct
    "bitmap_size", //size
    "bitmap_set", "bitmap_mark", "bitmap_reset", "bitmap_flip", "bitmap_test", //setting & testing single bits
    "bitmap_set_all", "bitmap_set_multiple", "bitmap_count", "bitmap_contains", "bitmap_any", "bitmap_none", "bitmap_all", //set & test multi bits
    "bitmap_scan", "bitmap_scan_and_flip", //Finding set or unset bits
    "bitmap_file_size", //file input & output
    "bitmap_dump", "bitmap_expand"
};

/* Number of bits in an element. */
#define ELEM_BITS (sizeof (elem_type) * CHAR_BIT)

/* From the outside, a bitmap is an array of bits.  From the
   inside, it's an array of elem_type (defined above) that
   simulates an array of bits. */
struct bitmap
  {
    size_t bit_cnt;     /* Number of bits. */
    elem_type *bits;    /* Elements that represent bits. */
  };

/* Returns the index of the element that contains the bit
   numbered BIT_IDX. */
static inline size_t
elem_idx (size_t bit_idx) 
{
  return bit_idx / ELEM_BITS;
}

/* Returns an elem_type where only the bit corresponding to
   BIT_IDX is turned on. */
static inline elem_type
bit_mask (size_t bit_idx) 
{
  return (elem_type) 1 << (bit_idx % ELEM_BITS);
}

/* Returns the number of elements required for BIT_CNT bits. */
static inline size_t
elem_cnt (size_t bit_cnt)
{
  return DIV_ROUND_UP (bit_cnt, ELEM_BITS);
}

/* Returns the number of bytes required for BIT_CNT bits. */
static inline size_t
byte_cnt (size_t bit_cnt)
{
  return sizeof (elem_type) * elem_cnt (bit_cnt);
}

/* Returns a bit mask in which the bits actually used in the last
   element of B's bits are set to 1 and the rest are set to 0. */
static inline elem_type
last_mask (const struct bitmap *b) 
{
  int last_bits = b->bit_cnt % ELEM_BITS;
  return last_bits ? ((elem_type) 1 << last_bits) - 1 : (elem_type) -1;
}

/* Creation and destruction. */

/* Initializes B to be a bitmap of BIT_CNT bits
   and sets all of its bits to false.
   Returns true if success, false if memory allocation
   failed. */
struct bitmap *
bitmap_create (size_t bit_cnt) 
{
  struct bitmap *b = malloc (sizeof *b);
  if (b != NULL)
    {
      b->bit_cnt = bit_cnt;
      b->bits = malloc (byte_cnt (bit_cnt));
      if (b->bits != NULL || bit_cnt == 0)
        {
          bitmap_set_all (b, false);
          return b;
        }
      free (b);
    }
  return NULL;
}

/* Creates and returns a bitmap with BIT_CNT bits in the
   BLOCK_SIZE bytes of storage preallocated at BLOCK.
   BLOCK_SIZE must be at least bitmap_needed_bytes(BIT_CNT). */
struct bitmap *
bitmap_create_in_buf (size_t bit_cnt, void *block, size_t block_size )
	// Remove KERNEL MACRO 'UNUSED')
{
  struct bitmap *b = block;
  
  ASSERT (block_size >= bitmap_buf_size (bit_cnt));

  b->bit_cnt = bit_cnt;
  b->bits = (elem_type *) (b + 1);
  bitmap_set_all (b, false);
  return b;
}

/* Returns the number of bytes required to accomodate a bitmap
   with BIT_CNT bits (for use with bitmap_create_in_buf()). */
size_t
bitmap_buf_size (size_t bit_cnt) 
{
  return sizeof (struct bitmap) + byte_cnt (bit_cnt);
}

/* Destroys bitmap B, freeing its storage.
   Not for use on bitmaps created by
   bitmap_create_preallocated(). */
void
bitmap_destroy (struct bitmap *b) 
{
  if (b != NULL) 
    {
      free (b->bits);
      free (b);
    }
}

/* Bitmap size. */

/* Returns the number of bits in B. */
size_t
bitmap_size (const struct bitmap *b)
{
  return b->bit_cnt;
}

/* Setting and testing single bits. */

/* Atomically sets the bit numbered IDX in B to VALUE. */
void
bitmap_set (struct bitmap *b, size_t idx, bool value) 
{
  ASSERT (b != NULL);
  ASSERT (idx < b->bit_cnt);
  if (value)
    bitmap_mark (b, idx);
  else
    bitmap_reset (b, idx);
}

/* Atomically sets the bit numbered BIT_IDX in B to true. */
void
bitmap_mark (struct bitmap *b, size_t bit_idx) 
{
  size_t idx = elem_idx (bit_idx);
  elem_type mask = bit_mask (bit_idx);

  /* This is equivalent to `b->bits[idx] |= mask' except that it
     is guaranteed to be atomic on a uniprocessor machine.  See
     the description of the OR instruction in [IA32-v2b]. */
  asm ("orl %k1, %k0" : "=m" (b->bits[idx]) : "r" (mask) : "cc");
}

/* Atomically sets the bit numbered BIT_IDX in B to false. */
void
bitmap_reset (struct bitmap *b, size_t bit_idx) 
{
  size_t idx = elem_idx (bit_idx);
  elem_type mask = bit_mask (bit_idx);

  /* This is equivalent to `b->bits[idx] &= ~mask' except that it
     is guaranteed to be atomic on a uniprocessor machine.  See
     the description of the AND instruction in [IA32-v2a]. */
  asm ("andl %k1, %k0" : "=m" (b->bits[idx]) : "r" (~mask) : "cc");
}

/* Atomically toggles the bit numbered IDX in B;
   that is, if it is true, makes it false,
   and if it is false, makes it true. */
void
bitmap_flip (struct bitmap *b, size_t bit_idx) 
{
  size_t idx = elem_idx (bit_idx);
  elem_type mask = bit_mask (bit_idx);

  /* This is equivalent to `b->bits[idx] ^= mask' except that it
     is guaranteed to be atomic on a uniprocessor machine.  See
     the description of the XOR instruction in [IA32-v2b]. */
  asm ("xorl %k1, %k0" : "=m" (b->bits[idx]) : "r" (mask) : "cc");
}

/* Returns the value of the bit numbered IDX in B. */
bool
bitmap_test (const struct bitmap *b, size_t idx) 
{
  ASSERT (b != NULL);
  ASSERT (idx < b->bit_cnt);
  return (b->bits[elem_idx (idx)] & bit_mask (idx)) != 0;
}

/* Setting and testing multiple bits. */

/* Sets all bits in B to VALUE. */
void
bitmap_set_all (struct bitmap *b, bool value) 
{
  ASSERT (b != NULL);

  bitmap_set_multiple (b, 0, bitmap_size (b), value);
}

/* Sets the CNT bits starting at START in B to VALUE. */
void
bitmap_set_multiple (struct bitmap *b, size_t start, size_t cnt, bool value) 
{
  size_t i;
  
  ASSERT (b != NULL);
  ASSERT (start <= b->bit_cnt);
  ASSERT (start + cnt <= b->bit_cnt);

  for (i = 0; i < cnt; i++)
    bitmap_set (b, start + i, value);
}

/* Returns the number of bits in B between START and START + CNT,
   exclusive, that are set to VALUE. */
size_t
bitmap_count (const struct bitmap *b, size_t start, size_t cnt, bool value) 
{
  size_t i, value_cnt;

  ASSERT (b != NULL);
  ASSERT (start <= b->bit_cnt);
  ASSERT (start + cnt <= b->bit_cnt);

  value_cnt = 0;
  for (i = 0; i < cnt; i++)
    if (bitmap_test (b, start + i) == value)
      value_cnt++;
  return value_cnt;
}

/* Returns true if any bits in B between START and START + CNT,
   exclusive, are set to VALUE, and false otherwise. */
bool
bitmap_contains (const struct bitmap *b, size_t start, size_t cnt, bool value) 
{
  size_t i;
  
  ASSERT (b != NULL);
  ASSERT (start <= b->bit_cnt);
  ASSERT (start + cnt <= b->bit_cnt);

  for (i = 0; i < cnt; i++)
    if (bitmap_test (b, start + i) == value)
      return true;
  return false;
}

/* Returns true if any bits in B between START and START + CNT,
   exclusive, are set to true, and false otherwise.*/
bool
bitmap_any (const struct bitmap *b, size_t start, size_t cnt) 
{
  return bitmap_contains (b, start, cnt, true);
}

/* Returns true if no bits in B between START and START + CNT,
   exclusive, are set to true, and false otherwise.*/
bool
bitmap_none (const struct bitmap *b, size_t start, size_t cnt) 
{
  return !bitmap_contains (b, start, cnt, true);
}

/* Returns true if every bit in B between START and START + CNT,
   exclusive, is set to true, and false otherwise. */
bool
bitmap_all (const struct bitmap *b, size_t start, size_t cnt) 
{
  return !bitmap_contains (b, start, cnt, false);
}

/* Finding set or unset bits. */

/* Finds and returns the starting index of the first group of CNT
   consecutive bits in B at or after START that are all set to
   VALUE.
   If there is no such group, returns BITMAP_ERROR. */
size_t
bitmap_scan (const struct bitmap *b, size_t start, size_t cnt, bool value) 
{
  ASSERT (b != NULL);
  ASSERT (start <= b->bit_cnt);

  if (cnt <= b->bit_cnt) 
    {
      size_t last = b->bit_cnt - cnt;
      size_t i;
      for (i = start; i <= last; i++)
        if (!bitmap_contains (b, i, cnt, !value))
          return i; 
    }
  return BITMAP_ERROR;
}

/* Finds the first group of CNT consecutive bits in B at or after
   START that are all set to VALUE, flips them all to !VALUE,
   and returns the index of the first bit in the group.
   If there is no such group, returns BITMAP_ERROR.
   If CNT is zero, returns 0.
   Bits are set atomically, but testing bits is not atomic with
   setting them. */
size_t
bitmap_scan_and_flip (struct bitmap *b, size_t start, size_t cnt, bool value)
{
  size_t idx = bitmap_scan (b, start, cnt, value);
  if (idx != BITMAP_ERROR) 
    bitmap_set_multiple (b, idx, cnt, !value);
  return idx;
}

/* Returns the number of bytes needed to store B in a file. */
size_t
bitmap_file_size (const struct bitmap *b) 
{
  return byte_cnt (b->bit_cnt);
}

/* Debugging. */

/* Dumps the contents of B to the console as hexadecimal. */
void bitmap_dump (const struct bitmap *b) 
{
  hex_dump (0, b->bits, byte_cnt (b->bit_cnt)/2, false);
}

/*expand the bitmap size*/
struct bitmap *bitmap_expand(struct bitmap *bitmap, int size){
  size_t before = bitmap_size(bitmap);
  bitmap->bit_cnt = before + size;
  bitmap->bits = realloc(bitmap->bits, byte_cnt(bitmap->bit_cnt));

  if(!bitmap->bits) return NULL;
  
  bitmap_set_multiple(bitmap, before, size, false);
  
  return bitmap;
}
//find the bitmap which name is same with parameter 'name'
struct bitmap_table *bitmap_find(char* name){
  struct bitmap_table *ptr;

  for(ptr = bitmap_head; ptr != NULL; ptr = ptr->link){
    if(!strcmp(ptr->name, name)) return ptr;
  }
  return NULL;

}

//dumpdata bitmap
void bitmap_dumpdata(struct bitmap* bitmap){
	for(int i = 0; i<bitmap_size(bitmap); i++){
		if(bitmap_test(bitmap, i)) printf("1");
		else printf("0");
	}

	printf("\n");
	return;
}
//link the command to correct function
void bitmap_command(char* command){

  struct bitmap_table* bitmap_ptr;
  char arg[8][30] = {'\0',};
  char* token = strtok(command, " ");
  int i = 1;
  /*save commands separated by spaces(' ')*/
	while(token != NULL){
		strcpy(arg[i], token);
		token = strtok(NULL, " ");
		i++;
	}   
  /*match the entered command with function*/
  for(i = 21; i > 0; --i){
    if(!strcmp(bitmap_command_name[i],arg[1])) break;
  }

  switch (i)
  {
    //size_t bit_cnt
    case 0: case 2: //create, buf_size
      break;
    //struct bitmap *
    case 3: //bitmap_destroy
    case 4: //bitmap_size
    case 20: //bitmap_dump
    
      bitmap_ptr = bitmap_find(arg[2]);
      if (i == 4){
        printf("%zu\n", bitmap_size(bitmap_ptr->bitmap));
      }
      else if (i == 20){
        bitmap_dump(bitmap_ptr->bitmap);
      }
      else{

      }
      break;
    //struct bitmap *, size_t idx
    case 6: case 7: case 8: case 9: //mark, reset, flip, test (single)

      bitmap_ptr = bitmap_find(arg[2]);
      if(i == 6) bitmap_mark(bitmap_ptr->bitmap, atoi(arg[3]));
      else if(i == 7) bitmap_reset(bitmap_ptr->bitmap, atoi(arg[3]));
      else if(i == 8) bitmap_flip(bitmap_ptr->bitmap, atoi(arg[3]));
      else if(i == 9){
        bool result = bitmap_test(bitmap_ptr->bitmap, atoi(arg[3]));
        if (result) printf("true\n");
        else printf("false\n");
      } 
      break;
    //struct bitmap* size_t idx, bool
    case 5: //set

      bitmap_ptr = bitmap_find(arg[2]);
      if(!strcmp(arg[4], "true"))
        bitmap_set(bitmap_ptr->bitmap, atoi(arg[3]), true);
      else 
        bitmap_set(bitmap_ptr->bitmap, atoi(arg[3]), false);
      break;
    //size_t bit_cnt, void *, size_t
    case 1: //create_in_buf
      break;
    //struct bitmap *, bool

    case 10: //set_all
      bitmap_ptr = bitmap_find(arg[2]);

      if(!strcmp(arg[3],"true"))
        bitmap_set_all(bitmap_ptr->bitmap, true);
      else 
        bitmap_set_all(bitmap_ptr->bitmap, false);
      break;
    //struct bitmap *, size_t, size_t
    case 14: case 15: case 16: //any, none, all
      bitmap_ptr = bitmap_find(arg[2]);
      bool result;

      if (i == 14) result = bitmap_any(bitmap_ptr->bitmap, atoi(arg[3]), atoi(arg[4]));
      else if(i == 15) result = bitmap_none(bitmap_ptr->bitmap, atoi(arg[3]), atoi(arg[4]));
      else if(i == 16) result = bitmap_all(bitmap_ptr->bitmap, atoi(arg[3]), atoi(arg[4]));

      if(result) printf("true\n");
      else printf("false\n");
      break;

    //struct bitmap *, size_t, size_t, bool
    case 11: case 12: case 13: case 17: case 18: //set_multiple, count, countains, scan, scan_and_flip
      bitmap_ptr = bitmap_find(arg[2]);
      bool boolean;
      
      if(!strcmp(arg[5], "true")) boolean = true;
      else boolean = false;
      if (i == 11) bitmap_set_multiple(bitmap_ptr->bitmap, atoi(arg[3]), atoi(arg[4]), boolean);
      else if(i == 12) printf("%zu\n", bitmap_count(bitmap_ptr->bitmap, atoi(arg[3]), atoi(arg[4]), boolean));
      else if(i == 13){
        if(bitmap_contains(bitmap_ptr->bitmap, atoi(arg[3]), atoi(arg[4]), boolean)) printf("true\n");
        else printf("false\n");
      } 
      else if(i == 17) printf("%zu\n", bitmap_scan(bitmap_ptr->bitmap, atoi(arg[3]), atoi(arg[4]), boolean));
      else printf("%zu\n", bitmap_scan_and_flip(bitmap_ptr->bitmap, atoi(arg[3]), atoi(arg[4]), boolean));
      
      break;
    //struct bitmap *, int
    case 21: //bitmap_expand
      bitmap_ptr = bitmap_find(arg[2]);
      bitmap_ptr->bitmap=bitmap_expand(bitmap_ptr->bitmap, atoi(arg[3]));
      break;
  
  default:
    break;
  }
}
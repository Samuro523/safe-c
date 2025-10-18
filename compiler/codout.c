
// codout.c : output pcode

from std use memory;
use pcodes, error, goptions;


#define debug  1


#if debug
  from std use console, thread;
  const int MAX_STACK_SIZE = 1000;
  char int_stack[MAX_STACK_SIZE], float_stack[MAX_STACK_SIZE], addr_stack[MAX_STACK_SIZE];
  int  int_stack_count, float_stack_count, addr_stack_count;
#endif


/************************************************************************************************************/
#begin unsafe
/************************************************************************************************************/

byte* mem;
int   mem_pos, mem_size;
int   enter_pos;     // position of P_ENTER

/************************************************************************************************************/

const int BLOCK = 4096;

/************************************************************************************************************/

void probe (int size)
{
  byte* mem2;

  if (mem_pos + size <= mem_size)
    return;

  mem2 = malloc ((uint)(mem_size + BLOCK));
  if (mem2 == null)
    fatal_out_of_memory_error ("codout.probe");

  mem2[0:mem_size] = mem[0:mem_size];
  clear mem2[mem_size : BLOCK];

  freem (mem);
  mem = mem2;
  mem_size += BLOCK;
}

/************************************************************************************************************/

void display_pcode_stacks ()
{
#if debug
  {
    int i;

    printf ("int: ");
    for (i=0; i<int_stack_count; i++)
      printf ("%c", int_stack[i]);
    printf ("\n");

    printf ("flt: ");
    for (i=0; i<float_stack_count; i++)
      printf ("%c", float_stack[i]);
    printf ("\n");

    printf ("adr: ");
    for (i=0; i<addr_stack_count; i++)
      printf ("%c", addr_stack[i]);
    printf ("\n");
  }
#endif
}

/************************************************************************************************************/

public
void put_code (PCODE code)
{
  if (code == P_ENTER)
    enter_pos = mem_pos;     // position of P_ENTER

  probe (2);

  mem[mem_pos:2] = code'byte;
  mem_pos += 2;  

//  trace ("\n%s", pcode_table[code].name);

#if debug
  {
    {
      ref string input = pcode_table[(int)code].input;
      int        index;

      index = input'length;
      index--;

      while (index >= 0)
      {
        switch (input[index])
        {
          case 'b':
          case 'i':
          case 'l':
            if (int_stack_count == 0 || int_stack[int_stack_count-1] != input[index])
            {
              printf ("\nargument mismatch on int_stack (pcode=%s) !\n", pcode_table[(int)code].name);
              display_pcode_stacks ();
              fatal_compiler_error0 ("int_stack mismatch");
            }
            int_stack_count--;
            break;

          case 'f':
          case 'd':
            if (float_stack_count == 0 || float_stack[float_stack_count-1] != input[index])
            {
              printf ("\nargument mismatch on float_stack (pcode=%s) !\n", pcode_table[(int)code].name);
              display_pcode_stacks ();
              fatal_compiler_error0 ("float_stack mismatch");
            }
            float_stack_count--;
            break;

          case 'a':
            if (addr_stack_count == 0 || addr_stack[addr_stack_count-1] != input[index])
            {
              printf ("\nargument mismatch on addr_stack (pcode=%s) !\n", pcode_table[(int)code].name);
              display_pcode_stacks ();
              fatal_compiler_error0 ("addr_stack mismatch");
            }
            addr_stack_count--;
            break;

          default:
            {
              printf ("\nargument error in table pcode_table[]\n");
              exit (-1);
            }
            break;
        }
        index--;
      }
    }
    
    {
      ref string output = pcode_table[(int)code].output;
      int        index;

      index = 0;

      while (index < output'length)
      {
        switch (output[index])
        {
          case 'b':
          case 'i':
          case 'l':
            if (int_stack_count == MAX_STACK_SIZE)
            {
              printf ("\nint_stack is full !\n");
              display_pcode_stacks ();
              fatal_compiler_error0 ("int_stack full");
            }
            int_stack[int_stack_count++] = output[index];
            break;

          case 'f':
          case 'd':
            if (float_stack_count == MAX_STACK_SIZE)
            {
              printf ("\nfloat_stack is full !\n");
              display_pcode_stacks ();
              fatal_compiler_error0 ("float_stack full");
            }
            float_stack[float_stack_count++] = output[index];
            break;

          case 'a':
            if (addr_stack_count == MAX_STACK_SIZE)
            {
              printf ("\naddr_stack is full !\n");
              display_pcode_stacks ();
              fatal_compiler_error0 ("addr_stack full");
            }
            addr_stack[addr_stack_count++] = output[index];
            break;

          default:
            {
              printf ("\nargument error in table pcode_table[]\n");
              exit (-1);
            }
            break;
        }
        
        index++;
      }
    }
  }
#endif

}

/************************************************************************************************************/

public
void sync_stacks (int i, int f, int a)
{
  int_stack_count   -= i;
  float_stack_count -= f;
  addr_stack_count  -= a;
}

/************************************************************************************************************/

public
void check_all_xx_stacks_empty ()
{
#if debug
  if (int_stack_count != 0 || float_stack_count != 0 || addr_stack_count != 0)
  {
    printf ("\nerror: xx_stacks should be empty after instruction\n");
    display_pcode_stacks ();
    exit (-1);
  }
#endif
}

/************************************************************************************************************/

public
void put_byte (byte b)
{
  probe (1);
  mem[mem_pos++] = b;

//  trace (" %d", b);
}

/************************************************************************************************************/

public
void put_int4 (int4 i)
{
  probe (4);

  *((int4 *)&mem[mem_pos]) = i;
  mem_pos += 4;

//  trace (" %d", i);
}

/************************************************************************************************************/

public
void put_int8 (int8 i)
{
  probe (8);

  *((int8 *)&mem[mem_pos]) = i;
  mem_pos += 8;

//  trace (" %I64d", i);
}

/************************************************************************************************************/

public
void put_float  (float f)
{
  probe (4);

  *((float *)&mem[mem_pos]) = f;
  mem_pos += 4;

//  trace (" %f", f);
}

/************************************************************************************************************/

public
void put_double (double d)
{
  probe (8);

  *((double *)&mem[mem_pos]) = d;
  mem_pos += 8;

//  trace (" %f", d);
}

/************************************************************************************************************/

public
void put_address (int8 ad)
{
  probe (address_size);

  if (address_size == 4)
    *((int4 *)&mem[mem_pos]) = (int4)ad;
  else
    *((int8 *)&mem[mem_pos]) = ad;

  mem_pos += address_size;

//  trace (" %I64d", ad);
}

/************************************************************************************************************/

public
void codout_reset_pos ()
{
  mem_pos = 0;
  clear mem[0 : mem_size];      // cleanup
}

/************************************************************************************************************/

public
void codout_fix_enter (uint4 size, uint4 freespace, uint4 block, int callee_registers_addr)
{
  *((uint4 *)&mem[enter_pos+(int)PCODE'size]) = size;
  *((uint4 *)&mem[enter_pos+(int)PCODE'size+4]) = freespace;
  *((uint4 *)&mem[enter_pos+(int)PCODE'size+8]) = block;
  *((int4 *)&mem[enter_pos+(int)PCODE'size+12]) = callee_registers_addr;
}

/************************************************************************************************************/

#if 0

// the following functions are used for xx_stacks validity check
// for ?: operator and switch statement.

typedef struct {
  int int_stack_count, float_stack_count, addr_stack_count;
  char last_int;
} XX_STACK_COUNTERS;

  void get_xx_stack_counters (XX_STACK_COUNTERS *p)
  {
    p->int_stack_count   = int_stack_count;
    p->float_stack_count = float_stack_count;
    p->addr_stack_count  = addr_stack_count;
    if (int_stack_count)
      p->last_int = int_stack[int_stack_count-1];
  }

  void put_xx_stack_counters (XX_STACK_COUNTERS *p)
  {
    int_stack_count   = p->int_stack_count;
    float_stack_count = p->float_stack_count;
    addr_stack_count  = p->addr_stack_count;
    if (int_stack_count)
      int_stack[int_stack_count-1] = p->last_int;
  }

  bool same_xx_stack_counters (XX_STACK_COUNTERS *p, XX_STACK_COUNTERS *q)
  {
    return p->int_stack_count   == q->int_stack_count &&
           p->float_stack_count == q->float_stack_count &&
           p->addr_stack_count  == q->addr_stack_count;
  }
#endif

/************************************************************************************************************/

public
void out_obtain_pcode_mem (out byte* pcode_mem, out int pcode_mem_size)
{
  pcode_mem      = mem;
  pcode_mem_size = mem_pos;
}

/************************************************************************************************************/
#end unsafe
/************************************************************************************************************/

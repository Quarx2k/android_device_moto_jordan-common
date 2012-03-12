//Created by Skrilax_CZ (skrilax@gmail.com),
//based on work done by Pradeep Padala (p_padala@yahoo.com)

#include <sys/ptrace.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <unistd.h>
#include <linux/user.h>
#include <stdio.h>
#include <string.h>

#ifndef SECOND_INIT_INJECTION_FUDGE
#define SECOND_INIT_INJECTION_FUDGE 2048
#endif

union u
{
  long val;
  char chars[sizeof(long)];
};

void getdata(pid_t child, long addr, char *str, int len)
{
  char *laddr;
  int i, j;
  union u data;

  i = 0;
  j = len / sizeof(long);
  laddr = str;

  while(i < j)
  {
    data.val = ptrace(PTRACE_PEEKDATA, child, (void*)(addr + i * 4), NULL);
    memcpy(laddr, data.chars, sizeof(long));
    ++i;
    laddr += sizeof(long);
  }

  j = len % sizeof(long);

  if(j != 0)
  {
    data.val = ptrace(PTRACE_PEEKDATA, child, (void*)(addr + i * 4), NULL);
    memcpy(laddr, data.chars, j);
  }

  str[len] = '\0';
}

void putdata(pid_t child, long addr, char *str, int len)
{
  char *laddr;
  int i, j;
  union u data;

  i = 0;
  j = len / sizeof(long);
  laddr = str;
  while(i < j)
  {
    memcpy(data.chars, laddr, sizeof(long));
    ptrace(PTRACE_POKEDATA, child, (void*)(addr + i * 4), (void*)(data.val));
    ++i;
    laddr += sizeof(long);
  }

  j = len % sizeof(long);
  if(j != 0)
  {
    memcpy(data.chars, laddr, j);
    ptrace(PTRACE_POKEDATA, child, (void*)(addr + i * 4), (void*)(data.val));
  }
}

long get_free_address(pid_t pid)
{
  FILE *fp;
  char filename[30];
  char line[85];
  long addr, heap_addr, code_addr;
  char exec;
  char str[20];

  addr = heap_addr = code_addr = 0;

  sprintf(filename, "/proc/%d/maps", pid);
  fp = fopen(filename, "r");

  if(fp == NULL)
    exit(1);

  while(fgets(line, 85, fp) != NULL)
  {
    // save the end of the code address in case heap space isn't executable
    if(code_addr == 0)
      sscanf(line, "%lx-%lx %*c%*c%c%*c %*s %s", &heap_addr, &code_addr, &exec, str);
    else
      sscanf(line, "%lx-%*x %*c%*c%c%*c %*s %s", &heap_addr, &exec, str);

    // we found our heap space
    if(strcmp(str, "00:00") == 0)
    {
      // if executable, use it, or else use the code space
      if(exec == 'x')
        addr = heap_addr;
      else
        addr = code_addr - SECOND_INIT_INJECTION_FUDGE;

      break;
    }
  }

  fclose(fp);
  return addr;
}

void get_base_image_address(pid_t pid, long* address, long* size)
{
  FILE *fp;
  char filename[30];
  char line[85];

  *address = 0;
  *size = 0;

  long start_address = 0;
  long end_address = 0;

  sprintf(filename, "/proc/%d/maps", pid);
  fp = fopen(filename, "r");

  if(fp == NULL)
    exit(1);

  if(fgets(line, 85, fp) != NULL)
  {
    sscanf(line, "%lx-%lx %*s %*s %*s", &start_address, &end_address);

    *address = start_address;
    *size = end_address - start_address;
  }

  fclose(fp);
}

int main(int argc, char** argv)
{
  struct pt_regs regs;
  char buff[512];
  char init_env[0x1C0];

  //read the enviromental variables of the init
  FILE* f = fopen("/proc/1/environ", "r");

  if (f == 0)
  {
    printf("Couldn't read /init enviromental variables.\n");
    return 2;
  }

  size_t sz = fread(init_env, 1, 0x1C0-1, f);
  init_env[sz] = 0;
  fclose(f);

  //init has pid always 1
  memset(&regs, 0, sizeof(regs));
  if (ptrace(PTRACE_ATTACH, 1, NULL, NULL))
  {
    printf("ERROR: Couldn't attach to /init.\n");
    return 1;
  }

  //wait for interrupt
  wait(NULL);

  ptrace(PTRACE_GETREGS, 1, NULL, &regs);

  //check if PC is valid
  if (regs.ARM_pc == 0)
  {
    printf("ERROR: Could get PC register value.\n");
    return 1;
  }

  printf("/init PC is on: 0x%08lX.\n", regs.ARM_pc);

  //structure of init is (static executable!)
  //0x8000 image base (usually)
  //0xA0 ELF header size
  //=>start is on 0x80A0
  //ARM mode

  long injected_code_address = get_free_address(1);
  printf("Address for the injection: 0x%08lX.\n", injected_code_address);

  //nah the space on heap will be bigger
  char injected_code[0x400];
  memset(injected_code, 0, sizeof(injected_code));

  //supposed to call
  //execve("/init", { "/init", NULL }, envp);

  //find execve inside init
  //===============================================================================
  //
  // find it based on these four instructions
  //
  // STMFD   SP!, {R4,R7}
  // MOV     R7, #0xB
  // SVC     0
  // LDMFD   SP!, {R4,R7}
  //
  // HEX: 90002DE9 0B70A0E3 000000EF 9000BDE8

  long image_base;
  long image_size;
  get_base_image_address(1, &image_base, &image_size);

  if (image_base == 0 || image_size == 0)
  {
    printf("ERROR: Couldn't get the image base of /init.\n");
    printf("Detaching...\n");
    ptrace(PTRACE_DETACH, 1, NULL, NULL);
    return 1;
  }

  printf("image_base: 0x%08lX.\n", image_base);
  printf("image_size: 0x%08lX.\n", image_size);

  char* init_image = malloc(image_size);
  getdata(1, image_base, init_image, image_size);

  //now look for the bytes
  long c,d;
  char execve_code[] = { 0x90, 0x00, 0x2D, 0xE9,
                         0x0B, 0x70, 0xA0, 0xE3,
                         0x00, 0x00, 0x00, 0xEF,
                         0x90, 0x00, 0xBD, 0xE8 };

  long execve_address = 0;
  c = 0;

  while (c < image_size - sizeof(execve_code))
  {
    int found = 1;

    for(d = 0; d < sizeof(execve_code); d++)
    {
      if (init_image[c+d] != execve_code[d])
      {
        found = 0;
        break;
      }
    }

    if (found)
    {
      execve_address = image_base + c;
      break;
    }

    c+=4; //ARM mode
  }

  if (!execve_address)
  {
    printf("ERROR: Failed locating execve.\n");
    printf("Detaching...\n");
    ptrace(PTRACE_DETACH, 1, NULL, NULL);
    return 5;
  }

  printf("execve located on: 0x%08lX.\n", execve_address);

  //fill in the instructions
  //===============================================================================

  /* LDR R0="/init"
   * LDR R1, &args #(args = { "/init", NULL })
   * LDR R2, &env #(read em first using /proc)
   * BL execve #if there is just branch and it fails, then I dunno what happens next, so branch with link
   * some illegal instruction here (let's say zeroes :D)
   */

  //this could be set directly to the current registers
  //but I find cleaner just using the code

  //LDR R0=PC-8+0x100 (HEX=0xE59F00F8); (pointer to "/init")
  //LDR R1=PC-8+0x108 (HEX=0xE59F10FC); (pointer to { "/init", NULL })
  //LDR R2=PC-8+0x120 (HEX=0xE59F2110); (pointer to env variables (char**) )
  //BL execve (HEX=0xEB000000 + ((#execve-PC)/4 & 0x00FFFFFF) )

  //on offset 0x100 create the pointers
  long instructions[4];
  instructions[0] = 0xE59F00F8;
  instructions[1] = 0xE59F10FC;
  instructions[2] = 0xE59F2110;
  instructions[3] = 0xEB000000 +
      ( ((execve_address - (injected_code_address + 0x0C + 8) )/4) & 0x00FFFFFF );

  //copy them
  memcpy((void*)injected_code, &instructions[0], sizeof(long) * 4);

  //fill in the pointers
  //===============================================================================

  //map
  //0x100 - char* - argument filename and argp[0] - pointer to "/init" on 0x200
  //0x104 - null pointer - argp[1] (set by memset)
  //0x108 - char** - argument argp - pointer to the pointers on 0x100
  //0x120 - char** - argument envp - pointer to the pointers on 0x130
  //0x130 - envp[0] -
  //0x134 - envp[1]
  //0x138 - envp[2]
  //etc.

  //write the arguments
  long execve_arg_filename_target = injected_code_address + 0x200;
  long execve_arg_argp_target = injected_code_address + 0x100;
  long execve_arg_envp_target = injected_code_address + 0x130;

  memcpy(&(injected_code[0x100]), &execve_arg_filename_target, sizeof(long));
  memcpy(&(injected_code[0x108]), &execve_arg_argp_target, sizeof(long));
  memcpy(&(injected_code[0x120]), &execve_arg_envp_target, sizeof(long));

  //fill in the strings and envp
  //===============================================================================

  //"/init" goes to 0x200
  strcpy(&(injected_code[0x200]), "/init");

  //enviroment variables
  long current_envp_address_incr = 0x130;
  char* iter = init_env;
  int w = 0x220;

  while (*iter)
  {
    //an env. var is found, write its address and copy it
    long current_envp_string_address = injected_code_address + w;

    memcpy(&(injected_code[current_envp_address_incr]), &current_envp_string_address, sizeof(long));
    current_envp_address_incr += sizeof(long);
    strcpy(&(injected_code[w]), iter);
    int len = strlen(iter) + 1;
    iter += len;
    w += len;

    while (w%(sizeof(long)))
      w++;
  }

  //terminating null pointer is preset by memset

  //put the data
  putdata(1, injected_code_address, injected_code, 1024);

  //set the PC
  regs.ARM_pc = injected_code_address;
  printf("Setting /init PC to: 0x%08lX.\n", injected_code_address);
  ptrace(PTRACE_SETREGS, 1, NULL, &regs);

  //fire it
  printf("Detaching...\n");
  ptrace(PTRACE_DETACH, 1, NULL, NULL);
  return 0;
}

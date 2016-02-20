/*
 * Joe Baker
 * Charles Smith
 * CS 1541
 * Project 1
 *
 * pipeline.c
 */

#include <stdio.h>
#include <inttypes.h>
#include <arpa/inet.h>
#include "trace_item.h" 

#define TRACE_BUFSIZE 1024*1024

//set to 0 to disable debug prints
#define DEBUG 1

static FILE *trace_fd;
static int trace_buf_ptr;
static int trace_buf_end;
static struct trace_item *trace_buf;

// Represents a buffer between two pipelined stages. Holds control info and data.
typedef struct {
    // put stuffs in here
} pipeline_buffer;


/* Helpers to read from trace into trace buffer */

int is_big_endian(void)
{
    union {
        uint32_t i;
        char c[4];
    } bint = {0x01020304};

    return bint.c[0] == 1; 
}

uint32_t my_ntohl(uint32_t x)
{
  u_char *s = (u_char *)&x;
  return (uint32_t)(s[3] << 24 | s[2] << 16 | s[1] << 8 | s[0]);
}

void trace_init()
{
  trace_buf = malloc(sizeof(struct trace_item) * TRACE_BUFSIZE);

  if (!trace_buf) {
    fprintf(stdout, "** trace_buf not allocated\n");
    exit(-1);
  }

  trace_buf_ptr = 0;
  trace_buf_end = 0;
}

void trace_uninit()
{
  free(trace_buf);
  fclose(trace_fd);
}

int trace_get_item(struct trace_item **item)
{
  int n_items;

  if (trace_buf_ptr == trace_buf_end) {	/* if no more unprocessed items in the trace buffer, get new data  */
    n_items = fread(trace_buf, sizeof(struct trace_item), TRACE_BUFSIZE, trace_fd);
    if (!n_items) return 0;				/* if no more items in the file, we are done */

    trace_buf_ptr = 0;
    trace_buf_end = n_items;			/* n_items were read and placed in trace buffer */
  }

  *item = &trace_buf[trace_buf_ptr];	/* read a new trace item for processing */
  trace_buf_ptr++;
  
  if (is_big_endian()) {
    (*item)->PC = my_ntohl((*item)->PC);
    (*item)->Addr = my_ntohl((*item)->Addr);
  }

  return 1;
}



void debug_print(char * str){
  if(DEBUG>0){
    printf("%s\n",str);
  }
}



/* Pipeline Stage Implementations
 *
 * Each stage of the pipeline is simulated by calling a function. The function
 * is given the data from the previous stage's buffer, does what it is supposed to,
 * and updates the buffer ahead of it.
 */

void sim_IF_stage(unsigned int pc, struct trace_item* inst, pipeline_buffer* if_id)
{
    // steps:
    // 1. read pc
    // 2. load instruction
    // 3. increment pc
    // 4. write pc, instruction data to buffer
}

void sim_ID_stage(pipeline_buffer* if_id, pipeline_buffer* id_ex)
{
    // steps:
    // 1. read from buffer
    // 2. read register values
    // 3. decode instruction
    // 4. write control, registers, immediates to buffer
}

void sim_EX_stage(pipeline_buffer* id_ex, pipeline_buffer* ex_mem)
{
    // steps:
    // 1. read from buffer
    // 2. alu
    // 3. branch condition
    // 4. write buffer
}

void sim_MEM_stage(pipeline_buffer* ex_mem, pipeline_buffer* mem_wb)
{
    // steps:
    // 1. read from buffer
    // 2. read/write to mem
    // 3. write buffer
}

void sim_WB_stage(pipeline_buffer* mem_wb)
{
    // steps:
    // 1. read from buffer
    // 2. write to register
}

//other helper methods
void zero_buff(pipeline_buffer* buff)
{

}


int main(int argc, char **argv)
{
  struct trace_item *tr_entry;
  size_t size;
  char *trace_file_name;
  int trace_view_on = 0;
  int branch_prediction_method = 0;

  unsigned int cycle_number = 0;

  // Parse Inputs
  if (argc != 4) {
    fprintf(stdout, "\nUSAGE: tv <trace_file> <switch - any character>\n");
    fprintf(stdout, "\n(switch) to turn on or off individual item view.\n\n");
    exit(0);
  } else {
    trace_file_name = argv[1];
    trace_view_on = atoi(argv[2]);
    branch_prediction_method = (atoi(argv[3] == 1)) : 1 ? 0;
  }

  // Open the trace file.
  fprintf(stdout, "\n ** opening file %s\n", trace_file_name);
  trace_fd = fopen(trace_file_name, "rb");
  if (!trace_fd) {
    fprintf(stdout, "\ntrace file %s not opened.\n\n", trace_file_name);
    exit(0);
  }

  trace_init();

  /* 'Hardware' */
    // Pipeline Buffers. These are 'hardware' so we only need 1 instance of each
    pipeline_buffer if_id__buffer;
    pipeline_buffer id_ex__buffer;
    pipeline_buffer ex_mem__buffer;
    pipeline_buffer mem_wb__buffer;

    zero_buff(&if_id__buffer);
    zero_buff(&id_ex__buffer);
    zero_buff(&ex_mem__buffer);
    zero_buff(&mem_wb__buffer);

    // Program counter...
    unsigned int pc = 0;
  
  while(1) {
    size = trace_get_item(&tr_entry);
    if (!size) {       /* no more instructions (trace_items) to simulate */
      printf("+ Simulation terminates at cycle : %u\n", cycle_number);
      break;
    }
    else{              /* parse the next instruction to simulate */
      cycle_number++;
    }

    //if NOT STALL
    //  read trace
    //else
    //  squash

   /* Stalling Mechanism */

   //if instruction in IF is a branch and the next instruciton isn't PC+4
   //we're squashing things
   //this is also going to be looking at the branch prediction here.
   //
   //if PC != PC+4
   //  look at branch prediction
   //  squash or predict

   /* Happy-path Pipeline */

  
    // so we went about this wrong at first
    // each stage is not actually calculating anything, just tracing
    // so have a debug print of what's in them, but don't worry about the alu


    // IF Stage
    sim_IF_stage(&pc, tr_entry, &if_id__buffer);

    // ID Stage
    sim_ID_stage(&if_id__buffer, &id_ex__buffer);
    
    // EX Stage
    sim_EX_stage(&id_ex__buffer, &ex_mem__buffer);

    // MEM Stage
    sim_MEM_stage(&ex_mem__buffer, &mem_wb__buffer);

    //WB Stage
    sim_WB_stage(&mem_wb__buffer);


    
    /* Not sure if we need this, or if we'll need to modify it. Just leaving here for now. */
    if (trace_view_on) {/* print the executed instruction if trace_view_on=1 */
      switch(tr_entry->type) {
        case ti_NOP:
          printf("[cycle %d] NOP:",cycle_number) ;
          break;
        case ti_RTYPE:
          printf("[cycle %d] RTYPE:",cycle_number) ;
          printf(" (PC: %x)(sReg_a: %d)(sReg_b: %d)(dReg: %d) \n", tr_entry->PC, tr_entry->sReg_a, tr_entry->sReg_b, tr_entry->dReg);
          break;
        case ti_ITYPE:
          printf("[cycle %d] ITYPE:",cycle_number) ;
          printf(" (PC: %x)(sReg_a: %d)(dReg: %d)(addr: %x)\n", tr_entry->PC, tr_entry->sReg_a, tr_entry->dReg, tr_entry->Addr);
          break;
        case ti_LOAD:
          printf("[cycle %d] LOAD:",cycle_number) ;      
          printf(" (PC: %x)(sReg_a: %d)(dReg: %d)(addr: %x)\n", tr_entry->PC, tr_entry->sReg_a, tr_entry->dReg, tr_entry->Addr);
          break;
        case ti_STORE:
          printf("[cycle %d] STORE:",cycle_number) ;      
          printf(" (PC: %x)(sReg_a: %d)(sReg_b: %d)(addr: %x)\n", tr_entry->PC, tr_entry->sReg_a, tr_entry->sReg_b, tr_entry->Addr);
          break;
        case ti_BRANCH:
          printf("[cycle %d] BRANCH:",cycle_number) ;
          printf(" (PC: %x)(sReg_a: %d)(sReg_b: %d)(addr: %x)\n", tr_entry->PC, tr_entry->sReg_a, tr_entry->sReg_b, tr_entry->Addr);
          break;
        case ti_JTYPE:
          printf("[cycle %d] JTYPE:",cycle_number) ;
          printf(" (PC: %x)(addr: %x)\n", tr_entry->PC,tr_entry->Addr);
          break;
        case ti_SPECIAL:
          printf("[cycle %d] SPECIAL:",cycle_number) ;      	
          break;
        case ti_JRTYPE:
          printf("[cycle %d] JRTYPE:",cycle_number) ;
          printf(" (PC: %x) (sReg_a: %d)(addr: %x)\n", tr_entry->PC, tr_entry->dReg, tr_entry->Addr);
          break;
      }
    }
  }

  trace_uninit();

  exit(0);
}



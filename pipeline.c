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

#define BTB_ENTRIES 128

static FILE *trace_fd;
static int trace_buf_ptr;
static int trace_buf_end;
static struct trace_item *trace_buf;

short btb_table[BTB_ENTRIES];

short get_btb_value(unsigned int address) {
    // translate address
    unsigned int mask = 127;
    unsigned int index = (address >> 4) & mask;
    
    // lookup and return
    return btb_table[index];
}

void set_btb_value(unsigned int address, int taken) {
    // translate
    unsigned int mask = 127;
    unsigned int index = (address >> 4) & mask;

    // lookup and set
    btb_table[index] = (taken == 0) ? 0 : 1;
}

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

int trace_get_item(struct trace_item *item)
{
  int n_items;

  if (trace_buf_ptr == trace_buf_end) {	/* if no more unprocessed items in the trace buffer, get new data  */
    n_items = fread(trace_buf, sizeof(struct trace_item), TRACE_BUFSIZE, trace_fd);
    if (!n_items) return 0;				/* if no more items in the file, we are done */

    trace_buf_ptr = 0;
    trace_buf_end = n_items;			/* n_items were read and placed in trace buffer */
  }
  struct trace_item* temp = &trace_buf[trace_buf_ptr];	/* read a new trace item for processing */
  *item = *temp;
  trace_buf_ptr++;
  
  if (is_big_endian()) {
    item->PC = my_ntohl(temp->PC);
    item->Addr = my_ntohl(temp->Addr);
  }

  return 1;
}

void debug_print(char * str){
  if(DEBUG>0){
    printf("%s\n", str);
  }
}

typedef struct queue_entry {
    struct trace_item entry;
    struct queue_entry* next;
    struct queue_entry* prev;
} queue_entry;
queue_entry* queue_start = 0;
queue_entry* queue_end   = 0;
int inst_queue_size = 0;

//other helper methods
void zero_buf(struct trace_item* buf) {
    memset(buf, 0, sizeof(struct trace_item));
}

/* returns 0 if nothing left, 1 if there is something left */
int read_instruction(struct trace_item* instruction) {
    // the next instruction will either be what we read from the trace,
    // or it will be one of the instructions following a branch.
    // if it's from the branch, we will get the instruction from our
    // queue of instructions that were backed up from a branch
    if (inst_queue_size == 0) {
        if (!trace_get_item(instruction)) {       /* no more instructions (trace_items) to simulate */
            return 0;
        }
    }
    else {
        if(!get_queued_instruction(instruction)) {
            return 0;
        }
    }
    return 1;
 
}

print_finished_instruction(struct trace_item* inst, int cycle_number) {
      switch(inst->type) {
        case ti_NOP:
          printf("[cycle %d] NOP:\n",cycle_number) ;
          break;
        case ti_RTYPE:
          printf("[cycle %d] RTYPE:",cycle_number) ;
          printf(" (PC: %x)(sReg_a: %d)(sReg_b: %d)(dReg: %d) \n", inst->PC, inst->sReg_a, inst->sReg_b, inst->dReg);
          break;
        case ti_ITYPE:
          printf("[cycle %d] ITYPE:",cycle_number) ;
          printf(" (PC: %x)(sReg_a: %d)(dReg: %d)(addr: %x)\n", inst->PC, inst->sReg_a, inst->dReg, inst->Addr);
          break;
        case ti_LOAD:
          printf("[cycle %d] LOAD:",cycle_number) ;      
          printf(" (PC: %x)(sReg_a: %d)(dReg: %d)(addr: %x)\n", inst->PC, inst->sReg_a, inst->dReg, inst->Addr);
          break;
        case ti_STORE:
          printf("[cycle %d] STORE:",cycle_number) ;      
          printf(" (PC: %x)(sReg_a: %d)(sReg_b: %d)(addr: %x)\n", inst->PC, inst->sReg_a, inst->sReg_b, inst->Addr);
          break;
        case ti_BRANCH:
          printf("[cycle %d] BRANCH:",cycle_number) ;
          printf(" (PC: %x)(sReg_a: %d)(sReg_b: %d)(addr: %x)\n", inst->PC, inst->sReg_a, inst->sReg_b, inst->Addr);
          break;
        case ti_JTYPE:
          printf("[cycle %d] JTYPE:",cycle_number) ;
          printf(" (PC: %x)(addr: %x)\n", inst->PC,inst->Addr);
          break;
        case ti_SPECIAL:
          printf("[cycle %d] SPECIAL:\n",cycle_number) ;      	
          break;
        case ti_JRTYPE:
          printf("[cycle %d] JRTYPE:",cycle_number) ;
          printf(" (PC: %x) (sReg_a: %d)(addr: %x)\n", inst->PC, inst->dReg, inst->Addr);
          break;
      }
}

void add_queued_instruction(struct trace_item* inst) {
    queue_entry* new_entry = (queue_entry*) malloc(sizeof(queue_entry));
    new_entry->entry = *inst;
    new_entry->next = queue_start;
    new_entry->prev = 0;
    queue_entry* old_start = queue_start;
    queue_start = new_entry;
    if (inst_queue_size == 0) {
        queue_end = queue_start;
    }
    else {
        old_start->prev = queue_start;
    }
    inst_queue_size++;
}

int get_queued_instruction(struct trace_item* inst) {
    if(queue_end == NULL) {
        return 0;
    }
    *inst = queue_end->entry;
    queue_entry* new_end = queue_end->prev;
    free(queue_end);
    queue_end = new_end;
    inst_queue_size--;
    return 1;
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
    fprintf(stdout, "\nUSAGE: tv <trace_file> <switch - any character> <branch_prediction - 0|1>\n");
    fprintf(stdout, "\n(switch) to turn on or off individual item view.\n");
    fprintf(stdout, "(branch_prediction) sets the branch prediction method as \'assume not taken\' (0), or a 1-bit branch predictor (1)\n\n");
    exit(0);
  } else {
    trace_file_name = argv[1];
    trace_view_on = atoi(argv[2]);
    branch_prediction_method = (atoi(argv[3]) == 1) ? 1 : 0;

    // debug
    char dbg_msg[200];
    sprintf(dbg_msg,
            "\n-debug- parsed inputs. file=%s, view_trace=%d, branch_pred=%d\n",
            trace_file_name,
            (trace_view_on == 0) ? 0 : 1,
            (branch_prediction_method == 0) ? 0 : 1
           );
    debug_print(dbg_msg);
  }

  // Open the trace file.
  fprintf(stdout, "\n ** opening file %s\n", trace_file_name);
  trace_fd = fopen(trace_file_name, "rb");
  if (!trace_fd) {
    fprintf(stdout, "\ntrace file %s not opened.\n\n", trace_file_name);
    exit(0);
  }

  trace_init();

  // store what instruction is in each stage of the pipeline (can be no-ops)
    struct trace_item new_instruction;
    struct trace_item if_stage;
    struct trace_item id_stage;
    struct trace_item ex_stage;
    struct trace_item mem_stage;
    struct trace_item wb_stage;
    zero_buf(&new_instruction);
    zero_buf(&if_stage);
    zero_buf(&id_stage);
    zero_buf(&ex_stage);
    zero_buf(&mem_stage);
    zero_buf(&wb_stage);

    memset(&btb_table, 0, sizeof(short) * BTB_ENTRIES);

  int instructions_left = 5;  
  while(instructions_left) {

  /*
   * We have 5 different things that can happen in the pipeline
   * 1. Happy Path
   * 2. Jump || JumpReg
   * 3. Forwarding (specifically when we have an instruction that depends on a lw directly in front)
   * 4. Branching with 'assume not taken'
   * 5. Branching with 1-bit predictor
   *
   * Heres some pseudocode for what should happen in each case:
   * 1. Happy Path
   *    * print what finished
   *    * shift instructions to the next stage
   *    * load next instruction either from trace, or queue if it has stuff
   * 2. Jump || JumpReg
   *    * push id,if -> queue in respective order
   *    * squash id,if (set each of those stages to 0);
   *    * shift instructions to the next stage
   *    * load next instruction either from trace or queue if non-empty
   * 3. Forwarding
   *    * if ( id stage depends on result of lw in ex )
   *        * shift instructions ex..wb
   *        * squash ex
   *        * keep id,if the same, dont read new instruction from queue or trace, do not pass go, do not collect $200
   * 4. Branching with assume NT
   *    * if (id's PC != ex's PC + 4)
   *        * push id,if -> queue in respective order
   *        * squash id,if
   *        * shift all instructions, and load new instruction from queue if non-empty, or trace
   * 5. Branching with 1-bit
   *    1 (needs to be done before step 2 here:
   *        * if(ex is a branch instruction)
   *            * if (local BTB value != result of ex)  --
   *                * push id,if -> queue in respective order
   *                * update BTB with result
   *                * shift instructions and load new instruction from either queue or trace
   *    2 (after 1...):
   *        * if (IF is a branch instruction)
   *            * if( branch dest address not in BTB)
   *                * add branch dest address to BTB, and set its branch value to not taken
   *                * set a local variable with as branch not taken
   *            * else
   *                * set a local variable with the branch taken value from BTB for this dest address
   */


      /*
       * Steps in simulation:
       * 1. Detect which case we're in
       * 2. Run that case
       */
     

    cycle_number++;
 
    print_finished_instruction(&wb_stage, cycle_number);

    int hazard = 0;
    
    //detection of non-happy
    if(branch_prediction_method == 0 && ex_stage.type == ti_BRANCH){
        if(ex_stage.PC + 4 != id_stage.PC){
            hazard = 2; //incorrect no prediction
            debug_print("[HAZARD] Incorrect default (not taken) prediciton\n");
        }
    }
    else if(ex_stage.type == ti_JTYPE|| ex_stage.type == ti_JRTYPE){
        hazard = 2;  //jump
        debug_print("[HAZARD] jump\n");
    }
    else if(branch_prediction_method == 1 && ex_stage.type == ti_BRANCH){
        if(ex_stage.PC +4 == id_stage.PC){ // not taken
            if(get_btb_value(ex_stage.PC) == 1){ //predict taken
                hazard = 2; //branch
                debug_print("[HAZARD] predicted taken when not taken\n");
                set_btb_value(ex_stage.PC, 0);
            }
        }
        else{ //taken
            if(get_btb_value(ex_stage.PC) == 0){ //predict not taken
                hazard = 2; //branch
                set_btb_value(ex_stage.PC, 1);
                debug_print("[HAZARD] predicted not taken when taken\n");
            }
        }
    }
    else if(ex_stage.type == ti_LOAD){
        if(ex_stage.dReg == id_stage.sReg_a || ex_stage.dReg == id_stage.sReg_b){
            hazard = 1;  //forward
            debug_print("[HAZARD] forward\n");
        }
    }


    wb_stage  = mem_stage;
    mem_stage = ex_stage; 

    switch(hazard) {
        case 0: //happy path
           ex_stage = id_stage;
           id_stage = if_stage;
           if(!read_instruction(&if_stage)) {
               instructions_left--;
               zero_buf(&if_stage);
           }
           break;
           
        case 1: //forward
            zero_buf(&ex_stage); 
            break;
            
        case 2: //branch resolve/jump resolve
            add_queued_instruction(&id_stage);
            add_queued_instruction(&if_stage);
            zero_buf(&id_stage);
            zero_buf(&if_stage);  
            ex_stage = id_stage;
            id_stage = if_stage;
            if(!read_instruction(&if_stage)) {
                instructions_left--;
                zero_buf(&if_stage);
            }
            break;

        default:
           printf("fuck.\n");
           exit(1);
    }
    


  


  //{
  //  // the next instruction will either be what we read from the trace,
  //  // or it will be one of the instructions following a branch.
  //  // if it's from the branch, we will get the instruction from our
  //  // queue of instructions that were backed up from a branch
  //  if (inst_queue_size == 0) {
  //      size = trace_get_item(&tr_entry);
  //      if (!size) {       /* no more instructions (trace_items) to simulate */
  //          printf("+ Simulation terminates at cycle : %u\n", cycle_number);
  //          break;
  //      }
  //      new_instruction = *tr_entry;
  //  }
  //  else {
  //      new_instruction = get_queued_instruction();
  //  }
  //  cycle_number++;
  //  
  // /* Happy-path case */
  //  // in the happy case, everything moves down the pipeline
  //  if(trace_view_on) {
  //      print_finished_instruction(&wb_stage, cycle_number);
  //  }
  //  wb_stage = mem_stage;
  //  mem_stage = ex_stage;
  //  ex_stage = id_stage;
  //  id_stage = if_stage;
  //  if_stage = new_instruction;
  //}

  //  /* Jump cases */
  //{
  //  if (inst_queue_size == 0) {
  //      size = trace_get_item(&tr_entry);
  //      if (!size) {       /* no more instructions (trace_items) to simulate */
  //          printf("+ Simulation terminates at cycle : %u\n", cycle_number);
  //          break;
  //      }
  //      new_instruction = *tr_entry;
  //  }
  //  else {
  //      new_instruction = get_queued_instruction();
  //  }
  //  cycle_number++;
  //  
  //  add_queued_instruction(&id_stage);
  //  add_queued_instruction(&if_stage);
  //  zero_buf(&id_stage);
  //  zero_buf(&if_stage);
  //  print_finished_instruction(&wb_stage, cycle_number);
  //  wb_stage = mem_stage;
  //  mem_stage = ex_stage;
  //  ex_stage = id_stage;
  //  id_stage = if_stage;
  //  if_stage = new_instruction;
  //}
  //
  //  /* Forwarding case */
  //{
  //  if(1) {
  //      print_finished_instruction(&wb_stage, cycle_number);
  //      wb_stage = mem_stage;
  //      mem_stage = ex_stage;
  //      zero_buf(&ex_stage);
  //  }
  //  cycle_number++;
  //}
  //
  //  /* Branching with assume not taken */
  //{
  //  if (1) {
  //      add_queued_instruction(&id_stage);
  //      add_queued_instruction(&if_stage);
  //      zero_buf(&id_stage);
  //      zero_buf(&if_stage);
  //      print_finished_instruction(&wb_stage, cycle_number);
  //      wb_stage = mem_stage;
  //      mem_stage = ex_stage;
  //      ex_stage = id_stage;
  //      id_stage = if_stage;
  //      if_stage = new_instruction;
  //  }
  //  cycle_number++;
  //}
  //
  //  /* Branching with 1-bit predictor */
  //{
  //  if(1) {
  //      if(1) {
  //          add_queued_instruction(&id_stage);
  //          add_queued_instruction(&if_stage);
  //          // update BTB
  //          zero_buf(&id_stage);
  //          zero_buf(&if_stage);
  //          print_finished_instruction(&wb_stage, cycle_number);
  //          wb_stage = mem_stage;
  //          mem_stage = ex_stage;
  //          ex_stage = id_stage;
  //          id_stage = if_stage;
  //          if_stage = new_instruction;
  //      }
  //  }
  //  if(1) {
  //      if(1) {

  //      }
  //      else {

  //      }
  //  }
  //  cycle_number++;
  //}
    // so we went about this wrong at first
    // each stage is not actually calculating anything, just tracing
    // so have a debug print of what's in them, but don't worry about the alu
    
    /* Not sure if we need this, or if we'll need to modify it. Just leaving here for now. */
     }

  trace_uninit();

  exit(0);
}



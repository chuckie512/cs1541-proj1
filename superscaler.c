/*
 * Joe Baker
 * Charles Smith
 * CS 1541
 * Project 1
 *
 * superscaler.c
 */

#include <stdio.h>
#include <inttypes.h>
#include <arpa/inet.h>
#include "trace_item.h" 

#define TRACE_BUFSIZE 1024*1024

//set to 0 to disable debug prints
#ifndef DEBUG
#define DEBUG 1
#endif

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

int if_id_buf_size = 0;

typedef struct inst_buffer {
    struct trace_item older;
    struct trace_item newer;
} inst_buffer;

typedef struct queue_entry {
    struct trace_item entry;
    struct queue_entry* next;
    struct queue_entry* prev;
} queue_entry;
queue_entry* queue_start = 0;
queue_entry* queue_end   = 0;
int inst_queue_size = 0;
int trace_nonempty = 1;

//other helper methods
void zero_buf(void* buf, int size) {
    memset(buf, 0, size);
}

/* returns 0 if nothing left, 1 if there is something left */
int read_instruction(struct trace_item* instruction) {
    // the next instruction will either be what we read from the trace,
    // or it will be one of the instructions following a branch.
    // if it's from the branch, we will get the instruction from our
    // queue of instructions that were backed up from a branch
    if (inst_queue_size == 0) {
        if (!trace_get_item(instruction)) {       /* no more instructions (trace_items) to simulate */
            trace_nonempty = 0;
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
    inst_buffer if_id_stage;
    struct trace_item reg1_stage;
    struct trace_item reg2_stage;
    struct trace_item ex1_stage;
    struct trace_item ex2_stage;
    struct trace_item mem1_stage;
    struct trace_item mem2_stage;
    struct trace_item wb1_stage;
    struct trace_item wb2_stage;
    zero_buf(&if_id_stage, sizeof(inst_buffer));
    zero_buf(&reg1_stage, sizeof(struct trace_item));
    zero_buf(&reg2_stage, sizeof(struct trace_item));
    zero_buf(&ex1_stage, sizeof(struct trace_item));
    zero_buf(&ex2_stage, sizeof(struct trace_item));
    zero_buf(&mem1_stage, sizeof(struct trace_item));
    zero_buf(&mem2_stage, sizeof(struct trace_item));
    zero_buf(&wb1_stage, sizeof(struct trace_item));
    zero_buf(&wb2_stage, sizeof(struct trace_item));

    memset(&btb_table, 0, sizeof(short) * BTB_ENTRIES);

    while(trace_nonempty || inst_queue_size || if_id_buf_size) {

        cycle_number++;

        if(trace_view_on) { 
            print_finished_instruction(&wb1_stage, cycle_number);
            print_finished_instruction(&wb2_stage, cycle_number);
        }

        wb1_stage = mem1_stage;
        wb2_stage = mem2_stage;
        mem1_stage = ex1_stage;
        mem2_stage = ex1_stage;




        //detect
        short squash = 0;
        if(ex1_stage.type == ti_JTYPE ||
           ex1_stage.type == ti_JRTYPE){
            squash = 1;
        }
        if(ex1_stage.type == ti_BRANCH){  //ex1 is holding a branch
            if(branch_prediction_method == 1){
                if(ex1_stage.PC + 4 == reg1_stage.PC || 
                   ex1_stage.PC + 4 == reg2_stage.PC ||
                   ex1_stage.PC + 4 == if_id_stage.newer.PC){  //not taken
                    if(get_btb_value(ex1_stage.PC) == 1){ //predict taken
                        debug_print("[HAZARD] predicted taken when not taken");
                        squash = 1;
                        set_btb_value(ex1_stage.PC, 0);
                    }
                }
                else{  //taken
                    if(get_btb_value(ex1_stage.PC) == 0){ //predict not taken
                        debug_print("[HAZARD] predict not taken when taken");
                        squash = 1;
                        set_btb_value(ex1_stage.PC, 1);
                    } 
                }
            }
            else{
                if(!(ex1_stage.PC + 4 == reg1_stage.PC ||
                     ex1_stage.PC + 4 == reg2_stage.PC ||
                     ex1_stage.PC + 4 == if_id_stage.older.PC)){  //taken
                    debug_print("[HAZARD] incorrect default (not taken) prediction");
                    squash = 1;
                }
            }

        }
        if(squash == 1){
            if(reg1_stage.type != ti_NOP){
                if(reg2_stage.type != ti_NOP){
                    if(reg1_stage.PC<reg2_stage.PC){
                        add_queued_instruction(&reg1_stage);
                        add_queued_instruction(&reg2_stage);
                        zero_buf(&reg1_stage, sizeof(reg1_stage));
                        zero_buf(&reg2_stage, sizeof(reg2_stage));
                    }
                    else{
                        add_queued_instruction(&reg2_stage);
                        add_queued_instruction(&reg1_stage);
                        zero_buf(&reg2_stage, sizeof(reg2_stage));
                        zero_buf(&reg1_stage, sizeof(reg1_stage));
                    }
                }
                else{
                    add_queued_instruction(&reg1_stage);
                    zero_buf(&reg1_stage, sizeof(reg1_stage));
                }
            }
            else if(reg2_stage.type != ti_NOP){
                add_queued_instruction(&reg2_stage);
                zero_buf(&reg2_stage, sizeof(reg2_stage));
            }
            if(if_id_stage.older.type != ti_NOP){
                add_queued_instruction(&if_id_stage.older);
                zero_buf(&if_id_stage.older, sizeof(if_id_stage.older));
            }
            if(if_id_stage.newer.type != ti_NOP){
                add_queued_instruction(&if_id_stage.newer);
                zero_buf(&if_id_stage.newer, sizeof(if_id_stage.newer));
            }
	    if_id_buf_size = 0;
        }


        // step 5.5: reg -> ex
        ex1_stage = reg1_stage;
        ex2_stage = reg2_stage;
        
        /* ISSUE */
        
	if(!squash){
		int issuetwo = 0;
		int issueone = 0;
	    




            //logic
//            if( ((if_id_stage.newer.type == ti_LOAD || if_id_stage.newer.type == ti_STORE) && 
//                 (if_id_stage.older.type != ti_LOAD && if_id_stage.older.type != ti_STORE)  ) || 
//                ((if_id_stage.older.type == ti_LOAD || if_id_stage.older.type == ti_STORE) && 
//                 (if_id_stage.newer.type != ti_LOAD && if_id_stage.newer.type != ti_STORE) )) {
//		if(if_id_stage.older.type != ti_BRANCH) {
//			if(!((if_id_stage.newer.dReg == if_id_stage.older.sReg_a || if_id_stage.newer.dReg == if_id_stage.older.sReg_b)||
//			      if_id_stage.older.dReg == if_id_stage.newer.sReg_a || if_id_stage.older.dReg == if_id_stage.newer.sReg_b  )){
//				if(reg2_stage.type == ti_LOAD &&
//				   (reg2_stage.dReg != if_id_stage.newer.sReg_a && reg2_stage.dReg != if_id_stage.newer.sReg_b &&
//				    reg2_stage.dReg != if_id_stage.older.sReg_a && reg2_stage.dReg != if_id_stage.older.sReg_b )){
//					issuetwo = 1;
//				}
//			}
//		}	
//	    }
//            else if( reg2_stage.type != ti_LOAD ||
//		    (if_id_stage.older.sReg_a != reg2_stage.dReg &&
//		     if_id_stage.older.sReg_b != reg2_stage.dReg )){
//                issueone = 1;
//	    }
            


	    int no_old_lw_depend = 0;
            int is_depend        = 0;
            int no_lw_depend     = 0;
            int older_not_branch = 0;
            int one_of_each_inst = 0;
            
            //older_not_branch
            if(if_id_stage.older.type != ti_BRANCH){
                older_not_branch = 1;
                //debug_print("older_not_branch");
            }

            //no_old_lw_depend
            if(reg2_stage.dReg != if_id_stage.newer.sReg_a &&
               reg2_stage.dReg != if_id_stage.newer.sReg_b){
                no_old_lw_depend = 1;
                //debug_print("no_old_lw_depend");
            }
                
            //no_lw_depend
            if(no_old_lw_depend &&
               reg2_stage.dReg != if_id_stage.newer.sReg_a &&
               reg2_stage.dReg != if_id_stage.newer.sReg_b){
                no_lw_depend = 1;
                //debug_print("no_lw_depend");
            }

            //is_depend
            if(if_id_stage.newer.dReg == if_id_stage.older.sReg_a ||
               if_id_stage.newer.dReg == if_id_stage.older.sReg_b ||
               if_id_stage.older.dReg == if_id_stage.newer.sReg_a ||
               if_id_stage.older.dReg == if_id_stage.newer.sReg_b){
                is_depend = 1;
                //debug_print("is_depend");
            }

            //one_of_each_inst
            if( ((if_id_stage.newer.type == ti_LOAD || if_id_stage.newer.type == ti_STORE) &&
                 (if_id_stage.older.type != ti_LOAD && if_id_stage.older.type != ti_STORE)) ||
                ((if_id_stage.older.type == ti_LOAD || if_id_stage.older.type == ti_STORE) &&
                 (if_id_stage.newer.type != ti_LOAD && if_id_stage.newer.type != ti_STORE)) ){
                one_of_each_inst = 1;
                //debug_print("one_of_each_inst"); 
            }

            
            //issue two
            if(!is_depend && no_lw_depend && older_not_branch && one_of_each_inst){
                issuetwo = 1;
            }
            //issue one
            if(no_old_lw_depend){
                issueone = 1;
            }
              
            //issues
	    if(issuetwo){
                debug_print("issue two");
                if(if_id_stage.newer.type == ti_LOAD ||
		   if_id_stage.newer.type == ti_STORE){
                    reg2_stage = if_id_stage.newer;
		    reg1_stage = if_id_stage.older;
                    zero_buf(&if_id_stage.newer, sizeof(if_id_stage.newer));
                    zero_buf(&if_id_stage.older, sizeof(if_id_stage.older));
		}
		else{
                    reg2_stage = if_id_stage.older;
		    reg1_stage = if_id_stage.newer;
                    if(if_id_stage.older.type != ti_LOAD ||
                       if_id_stage.older.type != ti_STORE){
                        debug_print("this shouldn't happen, look at one_of_each");
                    }
                    zero_buf(&if_id_stage.older, sizeof(if_id_stage.older));
                    zero_buf(&if_id_stage.newer, sizeof(if_id_stage.newer));
		}
		if_id_buf_size -= 2;
  	    } else if(issueone){
                debug_print("issue one");
		if(if_id_stage.older.type == ti_LOAD ||
	           if_id_stage.older.type == ti_STORE){
                    reg2_stage = if_id_stage.older;
		    zero_buf(&reg1_stage, sizeof(reg1_stage));
		}
		else{
                    reg1_stage = if_id_stage.older;
		    zero_buf(&reg2_stage, sizeof(reg2_stage));
		}
	        if_id_stage.older = if_id_stage.newer;
		if_id_buf_size -= 1;
		
	    } else{
                debug_print("issue zero");
                zero_buf(&reg1_stage, sizeof(reg1_stage));
		zero_buf(&reg2_stage, sizeof(reg2_stage));
	    }
	


	}
        
        /* Read new instructions */
        if(if_id_stage.newer.type == ti_NOP) {
            struct trace_item temp = {};
            if(read_instruction(&temp)) {
                if_id_stage.newer = temp;
            }
        }
        if(if_id_stage.older.type == ti_NOP) {
            if_id_stage.older = if_id_stage.newer;
            struct trace_item temp = {};
            if(read_instruction(&temp)) {
                if_id_stage.newer = temp;
            }
        } 
    }

    printf("+ Simulation terminates at cycle : %u\n", cycle_number); 

    trace_uninit();

    exit(0);
}



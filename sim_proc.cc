#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include "sim_proc.h"
#include <iostream>
#include <vector>
#include <algorithm>
/*  argc holds the number of command line arguments
    argv[] holds the commands themselves

    Example:-
    sim 256 32 4 gcc_trace.txt
    argc = 5
    argv[0] = "sim"
    argv[1] = "256"
    argv[2] = "32"
    ... and so on
*/
FILE *FP;               // File handler
std::vector<rob> rob_array;
std::vector<iq> iq_array;
std::vector<inst> insts (10000);
std::vector<rmt> rmt_array (67,{0,0});
std::vector<pipe_reg>  DE, RN, RR, DI;
std::vector<execute_list> execute_array;
std::vector<execute_list> wb; // Temporary list to hold instructions to move to WB
int global_seq_num = 0;
int total_cycles = 0;
int rob_head = 0; 
int rob_tail = 0;
int rob_count = 0; 
 
void initialize_arrays(int rob_size, int iq_size, int width) {
   iq_array.resize(iq_size, {0,0,0,0,0,0});
   rob_array.resize(rob_size, {0,0,0});
   DE.resize(width);  // Resize the outer dimension to 'width'
   RN.resize(width);  // Resize the outer dimension to 'width'
   RR.resize(width);  // Resize the outer dimension to 'width'
   DI.resize(width);  // Resize the outer dimension to 'width'
}
void print_instruction(const inst& instruction) {
    printf("%d fu{%d} src{%d,%d} dst{%d} FE{%d,%d} DE{%d,%d} RN{%d,%d} RR{%d,%d} DI{%d,%d} IS{%d,%d} EX{%d,%d} WB{%d,%d} RT{%d,%d}\n",
           instruction.seq_num,instruction.op_type,instruction.src1,instruction.src2,instruction.dest,instruction.fe_cycle_1,instruction.fe_cycle_2,instruction.de_cycle_1,instruction.de_cycle_2,instruction.rn_cycle_1,instruction.rn_cycle_2,instruction.rr_cycle_1,instruction.rr_cycle_2,instruction.di_cycle_1,instruction.di_cycle_2,instruction.is_cycle_1,instruction.is_cycle_2,instruction.ex_cycle_1,instruction.ex_cycle_2,instruction.wb_cycle_1,instruction.wb_cycle_2,instruction.rt_cycle_1,instruction.rt_cycle_2);
}

void retire(int width) {
    for (int i = 0; i < width && rob_count > 0; i++) { // Process up to 'width' instructions
        if (rob_array[rob_head].rdy == 1) { 
            int seq_num = rob_array[rob_head].seq_num;
	   if (insts[seq_num].rt_cycle_1 ==0){ 
            insts[seq_num].rt_cycle_1 = insts[seq_num].wb_cycle_1 +1; 
	   }
            int old_dest = insts[seq_num].dest;    
            
	    if (rmt_array[old_dest].rob_tag == rob_array[rob_head].dest)
		        rmt_array[old_dest].valid = 0;  			//clear valid if it was the latest version in the ROB	    
	    rob_array[rob_head].valid = 0; 			
	    insts[seq_num].rt_cycle_2 = total_cycles - insts[seq_num].rt_cycle_1 +1;
            rob_head = (rob_head + 1) % rob_array.size(); // Move ROB head forward
            rob_count--; 
        } else {
            break; // Stop if the current instruction is not ready
        }
    }

  }
void writeback(int width) {
    for (auto it = wb.begin(); it != wb.end();) {
        if (it->seq_num != -1) {
            int seq_num = it->seq_num;
		insts[seq_num].wb_cycle_1 = insts[seq_num].ex_cycle_1 + insts[seq_num].ex_cycle_2  ;
            for (auto& rob_entry : rob_array) {
                if (rob_entry.seq_num == seq_num) {
                    rob_entry.rdy = 1; // Mark as ready
                    break;
                }
            }

        it++;
	}
    }
    for (auto it = wb.begin(); it != wb.end();) 
            it = wb.erase(it);
}


void execute(int width) {
    for (auto it = execute_array.begin(); it != execute_array.end();) {
        int seq_num = it->seq_num;
        if (insts[seq_num].ex_cycle_1 == 0) {
            insts[seq_num].ex_cycle_1 = total_cycles; // Update execution start cycle
            insts[seq_num].is_cycle_2 = insts[seq_num].ex_cycle_1 - insts[seq_num].is_cycle_1 ; // Update execution start cycle
        }	
	it++;
    }
    for (auto it = execute_array.begin(); it != execute_array.end();) {
        if (it->latency == 1) {
            for (auto& iq_entry : iq_array) {
                if (iq_entry.seq_num != -1) { // Valid entry
                    if (iq_entry.src1 == it->dest) {
                        iq_entry.s1_rdy = 1;
                    }
                    if (iq_entry.src2 == it->dest) {
                        iq_entry.s2_rdy = 1;
                    }
                }
            }
	    for (int i = 0; i < width; i++) {
                if (DI[i].src1 == it->dest) {
                    DI[i].s1_rdy = 1;
                }
                if (DI[i].src2 == it->dest) {
                    DI[i].s2_rdy = 1;
                }
            }
	    for (int i = 0; i < width; i++) {
                if (RR[i].src1 == it->dest) {
                    RR[i].s1_rdy = 1;
                }
                if (RR[i].src2 == it->dest) {
                    RR[i].s2_rdy = 1;
                }
            }

            wb.push_back(*it);

            it = execute_array.erase(it);
        } else {
            it->latency--;
            it++;
        }
    }
}

void issue(int width) {
    int issued_count = 0;

    std::sort(iq_array.begin(), iq_array.end(), [](const iq& a, const iq& b) {
        return a.seq_num < b.seq_num;
    });
	
    for (size_t i = 0; i < iq_array.size(); i++) {
        if (iq_array[i].seq_num != -1) {
        		insts[iq_array[i].seq_num].is_cycle_1 = insts[iq_array[i].seq_num].di_cycle_1 + insts[iq_array[i].seq_num].di_cycle_2;
	}
    }
	
    for (size_t i = 0; i < iq_array.size() && issued_count < width; i++) {
        if (iq_array[i].seq_num == -1)
		 continue; 
        if (iq_array[i].s1_rdy && iq_array[i].s2_rdy) {			//if both sources are ready go ahead
            // Add the instruction to the execute list
            int seq_num = iq_array[i].seq_num;
	    int dest = iq_array[i].dest; 
            int latency = (iq_array[i].op_type == 0) ? 1 : (iq_array[i].op_type == 1) ? 2 : 5;
            execute_list inst; 
	    inst = {seq_num, dest, latency};
            execute_array.push_back(inst);
            insts[seq_num].ex_cycle_2 = latency;
            iq_array[i] = {-1, 0, 0, 0, 0, 0}; // Clear IQ entry
            issued_count++;
        }  
    } 	

}
void dispatch(int width) {
    int free_iq_entries = 0;

    for (const auto& iq_entry : iq_array) {
        if (iq_entry.seq_num == -1) { // An IQ entry is free if seq_num == -1
            free_iq_entries++;
        }
    }

		
    for (int i = 0; i < width; i++) {
   	 if (DI[i].seq_num != -1) {
		if (insts[DI[i].seq_num].di_cycle_1==0) {	
                        insts[DI[i].seq_num].di_cycle_1 = total_cycles;
                        insts[DI[i].seq_num].rr_cycle_2 = total_cycles - insts[DI[i].seq_num].rr_cycle_1;
		}
	}
    }
    if (free_iq_entries >= width) {
        for (int i = 0; i < width; i++) {
            if (DI[i].seq_num != -1) { 
                for (auto& iq_entry : iq_array) {
                    if (iq_entry.seq_num == -1) { // Free IQ entry found
                        // Move the instruction from DI to the IQ
                        iq_entry.op_type = DI[i].op_type;
                        iq_entry.dest = DI[i].dest;
                        iq_entry.src1 = DI[i].src1;
                        iq_entry.src2 = DI[i].src2;
                        iq_entry.s1_rdy = DI[i].s1_rdy;
                        iq_entry.s2_rdy = DI[i].s2_rdy;
                        iq_entry.seq_num = DI[i].seq_num;
                        //insts[DI[i].seq_num].di_cycle_1 = total_cycles;

                        DI[i].seq_num = -1;
                        break; // Move to the next instruction in DI
                    }
                }
            }
        }
    } else {
        for (int i = 0; i < width; i++) {
		if (DI[i].seq_num != -1)	
		insts[DI[i].seq_num].di_cycle_2++;	
	}
    }

}


void reg_read(int width) {
    int free_entries = 0;
    for (int i = 0; i < width; i++) {
	if (DI[i].seq_num == -1){
		free_entries++;
	}
    }
    for (int i = 0; i < width; i++) {
        if (RR[i].seq_num != -1 && RR[i].in_rob == 1 ) {
        if (free_entries ==  width ) {
            DI[i] = RR[i]; // Move instruction to regread
            if (DI[i].seq_num >= 0)  {
		if (insts[DI[i].seq_num].rr_cycle_1==0){
                insts[DI[i].seq_num].rr_cycle_1 = total_cycles;
                insts[DI[i].seq_num].rn_cycle_2 = total_cycles - insts[DI[i].seq_num].rn_cycle_1 ;
		}
            }
		if (DI[i].src1 >= 0){
			if (DI[i].renamed_s1 ==0)		//we have not renamed the source, so it must be ready as there is only one version in the ARF
                        	DI[i].s1_rdy = 1;
			if (rob_array[DI[i].src1].rdy ==1)
                        	DI[i].s1_rdy = 1;
		} else 
                        	DI[i].s1_rdy = 1;
		if (DI[i].src2 >= 0){
			if (DI[i].renamed_s2 ==0)		//we have not renamed the source, so it must be ready as there is only one version in the ARF
                        	DI[i].s2_rdy = 1;
			if (rob_array[DI[i].src2].rdy ==1)
                        	DI[i].s2_rdy = 1;
		} else 
                        	DI[i].s2_rdy = 1;
            RR[i].seq_num = -1; // Clearing the RR reg
        }else if (free_entries < width ){
		if (insts[RR[i].seq_num].rr_cycle_1==0){
                	insts[RR[i].seq_num].rr_cycle_1 = total_cycles;
                	insts[RR[i].seq_num].rn_cycle_2 = total_cycles - insts[RR[i].seq_num].rn_cycle_1 ;
		}	
	}
    }
}
}

void rename(int width, int rob_size) {
    int free_entries = 0;
    for (int i = 0; i < width; i++) {
	if (RR[i].seq_num == -1){
		free_entries++;
	}
    }
    for (int i = 0; i < width; i++) {
        if (RN[i].seq_num != -1 ) {
            int seq_num = RN[i].seq_num;
		if (insts[seq_num].rn_cycle_1 ==0){
                	insts[seq_num].rn_cycle_1 = total_cycles ;
		}
    	}
    }

    if (free_entries==width && (width < (rob_size - rob_count +1))){
    	for (int i = 0; i < width; i++) {
       		if (RN[i].seq_num != -1 ) {
    	           	RR[i] = RN[i];
                	RN[i].seq_num = -1; // Clear DE stage
                	rob_array[rob_tail].seq_num = RR[i].seq_num;
                	rob_array[rob_tail].valid = 1;
                	rob_array[rob_tail].rdy = 0;
			if (RR[i].src1 >= 0){
                    		if (rmt_array[RR[i].src1].valid) {
                        		RR[i].src1 = rmt_array[RR[i].src1].rob_tag;		//If we are renaming, means, source is not ready, we will stall in IQ
					//if (rob_array[RR[i].src1].rdy ==0)
					RR[i].renamed_s1 = 1;
                    		}
			}
			if (RR[i].src2 >= 0){
                    		if (rmt_array[RR[i].src2].valid) {
                        		RR[i].src2 = rmt_array[RR[i].src2].rob_tag;		//If we are renaming, means, source is not ready, we will stall in IQ
					//if (rob_array[RR[i].src2].rdy ==0)
					RR[i].renamed_s2 = 1;
                    		}
			} 
                	if (RR[i].dest >= 0) {
                    		rmt_array[RR[i].dest].rob_tag = rob_tail;
                    		rmt_array[RR[i].dest].valid = 1;
		    		RR[i].dest =  rob_tail;
                	}
		    	RR[i].in_rob = 1;
                	rob_array[rob_tail].dest = RR[i].dest;
                	rob_tail = (rob_tail + 1) % rob_array.size();  		//instead of pointers we will use indexes as we have a ROB array head and tail indices will be used as it seems easy
                	rob_count++;
		}	
	}
   }
}

void decode(int width) {
    int free_entries = 0;
    for (int i = 0; i < width; i++) {
	if (RN[i].seq_num == -1){
		free_entries++;
	}
    }
    	for (int i = 0; i < width; i++) {
            if (DE[i].seq_num != -1) {
                int seq_num = DE[i].seq_num;
		if (insts[seq_num].de_cycle_1 == 0){
                	insts[seq_num].de_cycle_1 = total_cycles; // Mark the decode cycle
                	insts[seq_num].fe_cycle_1 = total_cycles -1; // Mark the fe cycle
		}
		if (RN[i].seq_num != -1 || free_entries < width){
                	insts[DE[i].seq_num].de_cycle_2++; // Mark the decode cycle
		} else if (RN[i].seq_num == -1){	
            			RN[i] = DE[i]; // Move instruction to decode
	    			DE[i].seq_num = -1; // Clear reg stage
        	}
    	    }
	}
}

void fetch(int width) {
    int fetched_instructions = 0;
    int op_type, dest, src1, src2;
    int pc = 0;
    int free_entries = 0;
    for (int i = 0; i < width; ++i) {
	if (DE[i].seq_num == -1){
		free_entries++;
	}
    }
    if (feof(FP)|| (free_entries < width)) {
               return;  
    } else if ( free_entries == width ) {
    	for (int i = 0; i < width; ++i) {
        if(fscanf(FP, "%x %d %d %d %d", &pc, &op_type, &dest, &src1, &src2)==5){
            DE[i] = {pc, op_type, dest, src1, src2, global_seq_num};
            insts[global_seq_num] = {pc, op_type, dest, src1, src2, global_seq_num, total_cycles};
            fetched_instructions++;
            global_seq_num++;
	}
    }
    }
    return ;
}
void print_all_instructions(const std::vector<inst>& insts) {
    for (const auto& instruction : insts) {
        if (instruction.seq_num >-1) {
            print_instruction(instruction);
        }
    }
}
int advance_cycles(){
	total_cycles++;
	if (!feof(FP)|| rob_count >0){
		return 1;
	}
	else 
	return 0;	
}
int main (int argc, char* argv[])
{
    char *trace_file;       // Variable that holds trace file name;
    proc_params params;       // look at sim_bp.h header file for the the definition of struct proc_params
    //int op_type, dest, src1, src2;  // Variables are read from trace file
 //   uint64_t pc; // Variable holds the pc read from input file
    int dynamic_instr_count;
    float ipc;
    if (argc != 5)
    {
        printf("Error: Wrong number of inputs:%d\n", argc-1);
        exit(EXIT_FAILURE);
    }
    params.rob_size     = strtoul(argv[1], NULL, 10);
    params.iq_size      = strtoul(argv[2], NULL, 10);
    params.width        = strtoul(argv[3], NULL, 10);
    trace_file          = argv[4];
//    printf("rob_size:%lu "
//            "iq_size:%lu "
//            "width:%lu "
//            "tracefile:%s\n", params.rob_size, params.iq_size, params.width, trace_file);
    // Open trace_file in read mode
    initialize_arrays(params.rob_size, params.iq_size, params.width);
    FP = fopen(trace_file, "r");
    if(FP == NULL)
    {
        // Throw error and exit if fopen() failed
        printf("Error: Unable to open file %s\n", trace_file);
        exit(EXIT_FAILURE);
    }
    ///////////////////////////////////////////////////////////////////////////////////////////////////////
    //
    // The following loop just tests reading the trace and echoing it back to the screen.
    //
    // Replace this loop with the "do { } while (Advance_Cycle());" loop indicated in the Project 3 spec.
    // Note: fscanf() calls -- to obtain a fetch bundle worth of instructions from the trace -- should be
    // inside the Fetch() function.
    //
    ///////////////////////////////////////////////////////////////////////////////////////////////////////
do {
    retire(params.width);
    writeback(params.width);
    execute(params.width);
    issue(params.width);
    dispatch(params.width);
    reg_read(params.width);
    rename(params.width, params.rob_size);
    decode(params.width);
    fetch(params.width);
} while (advance_cycles());

  print_all_instructions(insts);
    dynamic_instr_count = global_seq_num;
    ipc = (float)  dynamic_instr_count/total_cycles;
    printf("# === Simulator Command =========\n");
    printf("# ./sim %d %d %d %s\n", params.rob_size, params.iq_size, params.width, trace_file);
    printf("# === Processor Configuration ===\n");
    printf("# ROB_SIZE = %d\n", params.rob_size);
    printf("# IQ_SIZE  = %d\n", params.iq_size);
    printf("# WIDTH    = %d\n", params.width);
    printf("# === Simulation Results ========\n");
    printf("# Dynamic Instruction Count    = %d\n", dynamic_instr_count);
    printf("# Cycles                       = %d\n", total_cycles);
    printf("# Instructions Per Cycle (IPC) = %.2f\n", ipc);
    return 0;
}

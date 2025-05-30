#ifndef SIM_PROC_H
#define SIM_PROC_H
typedef struct proc_params{
     int rob_size;
     int iq_size;
     int width;
}proc_params;
typedef struct rob{
     int dest;
     int rdy;
     int seq_num = -1;
     int valid;
}rob;
typedef struct iq{
     int valid;
    int op_type;
     int dest;
     int s1_rdy;
     int src1;
     int s2_rdy;
    int src2;
    int seq_num = -1;
}iq;
typedef struct rmt{
    int rob_tag;
     int valid;
}rmt;
typedef struct arf{
    long int value;
}arf;
typedef struct Instruction {
    int  pc;
    int op_type;
    int dest;
    int src1;
    int src2;
    int seq_num;
    int fe_cycle_1;
    int fe_cycle_2 = 1;
    int de_cycle_1;
    int de_cycle_2 = 1;
    int rn_cycle_1;
    int rn_cycle_2 = 1;
    int rr_cycle_1;
    int rr_cycle_2 = 1;
    int di_cycle_1;
    int di_cycle_2 = 1;
    int is_cycle_1;
    int is_cycle_2 = 1;
    int ex_cycle_1;
    int ex_cycle_2;
    int wb_cycle_1;
    int wb_cycle_2 = 1;
    int rt_cycle_1;
    int rt_cycle_2 = 1;

}inst;
// Put additional data structures here as per your requirement
typedef struct pipe_reg {
    int pc;
    int op_type;
    int dest;
    int src1;
    int src2;
    int seq_num = -1;
    int s1_rdy; 	//assume sources are always ready, unless we see a conflict in the RMT
    int s2_rdy;		//We will set the sources, rdy to be 0 if we find a conflict in the RMT
    int renamed_s1;
    int renamed_s2;
    int in_rob;
}pipe_reg;
typedef struct execute_list {
    int seq_num = -1;
    int dest;
    int latency;
}execute_list;

#endif

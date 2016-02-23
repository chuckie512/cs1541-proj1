*********************************************
Joe Baker
Charles Smith
CS 1541
Project 1

readme.txt
*********************************************

USAGE
*********************************************
Compile with gcc version 3.4.2 on unixs.cis.pitt.edu.
To compile part 1, run 'gcc pipeline.c -o pipeline'
To compile part 2, run 'gcc superscaler.c -o superscaler'
To run part 1, run './pipeline <trace_file_name> [[view_trace] [branch_prediction_method]]'
    where <trace_file_name> is the path to the trace file, [view_trace] is 1 or 0 indicating
    whether or not you wish to view the trace as it is processed, and [branch_prediction_method]
    is 1 or 0, indicating whether you want to use 'assume not taken' (0), or '1-bit predictor' (1).
    If not specified, view_trace=0, branch_prediction_method=0. Either specify both, or neither.
To run part 2, run './superscaler <trace_file_name> [[view_trace] [branch_precition_method]]'
    The options for this are the same as part 1.

NOTES
*********************************************
We assume that JTYPE and JRTYPE instructions are evaluated in the EX stage of the pipeline for
both parts 1 and 2. Thus when these instructions are encountered, there will be two cycles
wasted (IF and ID) stages.

We print finished instructions as they pass through the WB stages for both parts 1 and 2. Thus,
during the beginning of execution, a few NOPs will be printed as the pipeline fills. The number
of NOPs is consistent and has a negligible effect on CPI.

As the last instruction is issued in part 2, we let the CPU run the max number of remaining cycles.
Thus, there may be a few trailing NOPs printed at the end of execution as the pipeline drains. This
is because some traces may not need all of these remaining cycles to finish.

KNOWN BUGS
*********************************************
No known bugs.

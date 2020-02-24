/* External definitions for time-shared computer model. */

#include "simlib.h"               /* Required for use of simlib.c. */

#define EVENT_ARRIVAL          1  /* Event type for arrival of job to CPU. */
#define EVENT_END_CPU_1_RUN    2  /* Event type for end of a CPU1 run. */
#define EVENT_END_CPU_2_RUN    3  /* Event type for end of a CPU1 run. */
#define EVENT_END_SIMULATION   4  /* Event type for end of the simulation. */
#define LIST_CPU_1             1  /* CPU1 */
#define LIST_CPU_2             2  /* CPU2 */
#define LIST_QUEUE_1           3  /* Queue CPU1. */
#define LIST_QUEUE_2           4  /* Queue CPU2. */
#define SAMPST_RESPONSE_TIMES  1  /* sampst variable for response times. */
#define STREAM_THINK           1  /* Random-number stream for think times. */
#define STREAM_SERVICE         2  /* Random-number stream for service times. */
#define STREAM_ASSIGN          3  /* Random-number for assigning cou to job */

/* Declare non-simlib global variables. */

int   min_terms, max_terms, incr_terms, num_terms, num_responses,
      num_responses_required, term;
float mean_think, mean_service, quantum, swap;
FILE  *infile, *outfile;

/* Declare non-simlib functions. */

void arrive(void);
int assignJob(void);
void start_CPU_run(int cpu_num);
void end_CPU_run(int cpu_num);
void report(void);


int main()  /* Main function. */
{
    /* Open input and output files. */

    infile  = fopen("../tscomp.in",  "r");
    outfile = fopen("../tscomp.out", "w");

    while(!feof(infile)) {

        /* Read input parameters. */

        fscanf(infile, "%d %d %d %d %f %f %f %f",
            &min_terms, &max_terms, &incr_terms, &num_responses_required,
            &mean_think, &mean_service, &quantum, &swap);

        /* Write report heading and input parameters. */

        fprintf(outfile, "\n\nTime-shared computer model\n\n");
        fprintf(outfile, "Number of terminals%9d to%4d by %4d\n\n",
                min_terms, max_terms, incr_terms);
        fprintf(outfile, "Mean think time  %11.3f seconds\n\n", mean_think);
        fprintf(outfile, "Mean service time%11.3f seconds\n\n", mean_service);
        fprintf(outfile, "Quantum          %11.3f seconds\n\n", quantum);
        fprintf(outfile, "Swap time        %11.3f seconds\n\n", swap);
        fprintf(outfile, "Number of jobs processed%12d\n\n\n",
                num_responses_required);
        fprintf(outfile, "Number of      Average         Average         Average");
        fprintf(outfile, "       Utilization       Utilization\n");
        fprintf(outfile, "terminals   response time  number in queue1  number in queue2   of CPU1       of CPU2");

        /* Run the simulation varying the number of terminals. */

        for (num_terms = min_terms; num_terms <= max_terms;
            num_terms += incr_terms) {

            /* Initialize simlib */

            init_simlib();

            /* Set maxatr = max(maximum number of attributes per record, 4) */

            maxatr = 4;  /* NEVER SET maxatr TO BE SMALLER THAN 4. */

            /* Initialize the non-simlib statistical counter. */

            num_responses = 0;

            /* Schedule the first arrival to the CPU from each terminal. */

            for (term = 1; term <= num_terms; ++term)
                event_schedule(expon(mean_think, STREAM_THINK), EVENT_ARRIVAL);

            /* Run the simulation until it terminates after an end-simulation event
            (type EVENT_END_SIMULATION) occurs. */

            do {

                /* Determine the next event. */

                timing();

                /* Invoke the appropriate event function. */

                switch (next_event_type) {
                    case EVENT_ARRIVAL:
                        printf("ARRIVE\n");
                        arrive();
                        break;
                    case EVENT_END_CPU_1_RUN:
                        printf("END EVENT CPU1\n");
                        end_CPU_run(LIST_CPU_1);
                        break;
                    case EVENT_END_CPU_2_RUN:
                        printf("END EVENT CPU2\n");
                        end_CPU_run(LIST_CPU_2);
                        break;
                    case EVENT_END_SIMULATION:
                        printf("END SIM\n");
                        report();
                        break;
                }

            /* If the event just executed was not the end-simulation event (type
            EVENT_END_SIMULATION), continue simulating.  Otherwise, end the
            simulation. */

            } while (next_event_type != EVENT_END_SIMULATION);
        }
    }

    fclose(infile);
    fclose(outfile);

    return 0;
}


void arrive(void)  /* Event function for arrival of job at CPU after think
                      time. */
{
    int queue_num;

    /* Place the arriving job at the end of the CPU queue.
       Note that the following attributes are stored for each job record:
            1. Time of arrival to the computer.
            2. The (remaining) CPU service time required (here equal to the
               total service time since the job is just arriving). */

    transfer[1] = sim_time;
    transfer[2] = expon(mean_service, STREAM_SERVICE);

    queue_num = assignJob();
    list_file(LAST, queue_num);

    /* If the CPU is idle, start a CPU run. */
    /* Note: LIST_CPU_N is (LIST_QUEUE for CPU N - 2) */

    if (list_size[queue_num - 2] == 0)
        start_CPU_run(queue_num - 2);
}

int assignJob(void) {
    double result = uniform(0, 1, STREAM_ASSIGN);
    if (result < 0.5) {
        return LIST_QUEUE_1;
    }
    else {
        return LIST_QUEUE_2;
    }
}


void start_CPU_run(int cpu_num)  /* Non-event function to start a CPU run of a job. */
{
    const int queue_num = cpu_num + 2;
    int cpu_end_event;
    float run_time;

    /* Remove the first job from the queue. */

    list_remove(FIRST, queue_num);

    /* Determine the CPU time for this pass, including the swap time. */

    if (quantum < transfer[2])
        run_time = quantum + swap;
    else
        run_time = transfer[2] + swap;

    /* Decrement remaining CPU time by a full quantum.  (If less than a full
       quantum is needed, this attribute becomes negative.  This indicates that
       the job, after exiting the CPU for the current pass, will be done and is
       to be sent back to its terminal.) */

    transfer[2] -= quantum;

    /* Place the job into the CPU. */

    list_file(FIRST, cpu_num);

    /* Schedule the end of the CPU run. */

    if (cpu_num == LIST_CPU_1) {
        cpu_end_event = EVENT_END_CPU_1_RUN;
    }
    else if (cpu_num == LIST_CPU_2) {
        cpu_end_event = EVENT_END_CPU_2_RUN;
    }

    event_schedule(sim_time + run_time, cpu_end_event);
}


void end_CPU_run(int cpu_num)  /* Event function to end a CPU run of a job. */
{
    const int queue_num = cpu_num + 2;

    /* Remove the job from the CPU. */

    list_remove(FIRST, cpu_num);

    /* Check to see whether this job requires more CPU time. */

    if (transfer[2] > 0.0) {

        /* This job requires more CPU time, so place it at the end of the queue
           and start the first job in the queue. */

        list_file(LAST, queue_num);
        start_CPU_run(cpu_num);
    }

    else {

        /* This job is finished, so collect response-time statistics and send it
           back to its terminal, i.e., schedule another arrival from the same
           terminal. */

        sampst(sim_time - transfer[1], SAMPST_RESPONSE_TIMES);

        event_schedule(sim_time + expon(mean_think, STREAM_THINK),
                       EVENT_ARRIVAL);

        /* Increment the number of completed jobs. */

        ++num_responses;

        /* Check to see whether enough jobs are done. */

        if (num_responses >= num_responses_required)

            /* Enough jobs are done, so schedule the end of the simulation
               immediately (forcing it to the head of the event list). */

            event_schedule(sim_time, EVENT_END_SIMULATION);

        else

            /* Not enough jobs are done; if the queue is not empty, start
               another job. */

            if (list_size[queue_num] > 0)
                start_CPU_run(cpu_num);
    }
}


void report(void)  /* Report generator function. */
{
    /* Get and write out estimates of desired measures of performance. */

    fprintf(outfile, "\n\n%5d%16.3f%16.3f%16.3f%16.3f%16.3f", num_terms,
            sampst(0.0, -SAMPST_RESPONSE_TIMES), filest(LIST_QUEUE_1), filest(LIST_QUEUE_2),
            filest(LIST_CPU_1), filest(LIST_CPU_2));
}


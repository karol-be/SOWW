#include <stdio.h>
#include <mpi.h>
#include <math.h>
#include <stdlib.h>
#include <stdbool.h>

#define RANGESIZE 1
#define DATA 0
#define RESULT 1
#define FINISH 2

#define START -20
#define STOP 20
#define PRECISION 0.0000001

//#define DEBUG

double f(double x)
{
    return sin(x);
}

double SimpleIntegration(double a, double b)
{
    double i;
    double sum = 0;
    for (i = a; i < b; i += PRECISION)
        sum += f(i) * PRECISION;
    return sum;
}

struct
{
    int procCount;
    int myRank;
    MPI_Request *requests;
    double* ranges;
    double* resulttemp;

} typedef MPI_Proc_Context;

bool EnsureSufficientProcCount(int count)
{
    if (count < 2)
    {
        printf("Run with at least 2 processes");
        return false;
    }
    return true;
}

bool Startup(MPI_Proc_Context *context)
{
       
    MPI_Comm_rank(MPI_COMM_WORLD, &(context->myRank));
    MPI_Comm_size(MPI_COMM_WORLD, &(context->procCount));
    if (!EnsureSufficientProcCount(context->procCount))
    {
        return false;
    }
    return true;
}



bool InitializeMasterBuffers(MPI_Proc_Context* context)
{
    context->requests = (MPI_Request *)malloc(3 * (context->procCount - 1) * sizeof(MPI_Request));
    context->ranges = (double *)malloc(4 * (context->procCount - 1) * sizeof(double));
    context->resulttemp = (double *)malloc((context->procCount - 1) * sizeof(double));
    
    if (!context->requests || !context->ranges || !context->resulttemp)
    {
        printf("\nNot enough memory");
        return false;
    }
    return true;
}

bool HandleMaster(MPI_Proc_Context *context)
{
    int requestcount = 0;
    int requestcompleted;
    double a = START * M_PI, b = STOP * M_PI;
    double range[2];
    double result = 0;
    int sentcount = 0;
    int recvcount = 0;
    int i;
    MPI_Status status;

 if (((b - a) / RANGESIZE) < 2 * (context->procCount - 1))
    {
        printf("More subranges needed");
        return false;
    }

    if(!InitializeMasterBuffers(context)) return false;

    range[0] = a;

    // first distribute some ranges to all slaves
    for (int procNum = 1; procNum < context->procCount; procNum++)
    {
        range[1] = range[0] + RANGESIZE;
        MPI_Send(range, 2, MPI_DOUBLE, procNum, DATA, MPI_COMM_WORLD);
        sentcount++;
        range[0] = range[1];
    }

    // the first proccount requests will be for receiving, the latter ones for sending
    for (i = 0; i < 2 * (context->procCount - 1); i++)
        context->requests[i] = MPI_REQUEST_NULL; // none active at this point

    // start receiving for results from the slaves
    for (i = 1; i < context->procCount; i++)
        MPI_Irecv(&(context->resulttemp[i - 1]), 1, MPI_DOUBLE, i, RESULT, MPI_COMM_WORLD, &(context->requests[i - 1]));

    // start sending new data parts to the slaves
    for (i = 1; i < context->procCount; i++)
    {
        range[1] = range[0] + RANGESIZE;

        context->ranges[2 * i - 2] = range[0];
        context->ranges[2 * i - 1] = range[1];

        // send it to process i
        MPI_Isend(&(context->ranges[2 * i - 2]), 2, MPI_DOUBLE, i, DATA, MPI_COMM_WORLD, &(context->requests[context->procCount - 2 + i]));

        sentcount++;
        range[0] = range[1];
    }
    while (range[1] < b)
    {
        // wait for completion of any of the requests
        MPI_Waitany(2 * context->procCount - 2, context->requests, &requestcompleted, MPI_STATUS_IGNORE);

        // if it is a result then send new data to the process
        // and add the result
        if (requestcompleted < (context->procCount - 1))
        {
            result += context->resulttemp[requestcompleted];
            recvcount++;

            // first check if the send has terminated
            MPI_Wait(&(context->requests[context->procCount - 1 + requestcompleted]), MPI_STATUS_IGNORE);

            // now send some new data portion to this process
            range[1] = range[0] + RANGESIZE;

            if (range[1] > b)
                range[1] = b;

            context->ranges[2 * requestcompleted] = range[0];
            context->ranges[2 * requestcompleted + 1] = range[1];
            MPI_Isend(&(context->ranges[2 * requestcompleted]), 2, MPI_DOUBLE, requestcompleted + 1, DATA, MPI_COMM_WORLD,
                      &(context->requests[context->procCount - 1 + requestcompleted]));
            sentcount++;
            range[0] = range[1];

            // now issue a corresponding recv
            MPI_Irecv(&(context->resulttemp[requestcompleted]), 1,
                      MPI_DOUBLE, requestcompleted + 1, RESULT,
                      MPI_COMM_WORLD,
                      &(context->requests[requestcompleted]));
        }
    }
    // now send the FINISHING ranges to the slaves
    // shut down the slaves
    range[0] = range[1];
    for (i = 1; i < context->procCount; i++)
    {
        context->ranges[2 * i - 4 + 2 * context->procCount] = range[0];
        context->ranges[2 * i - 3 + 2 * context->procCount] = range[1];
        MPI_Isend(range, 2, MPI_DOUBLE, i, DATA, MPI_COMM_WORLD, &(context->requests[2 * context->procCount - 3 + i]));
    }

    // now receive results from the processes - that is finalize the pending requests
    MPI_Waitall(3 * context->procCount - 3, context->requests, MPI_STATUSES_IGNORE);

    // now simply add the results
    for (i = 0; i < (context->procCount - 1); i++)
    {
        result += context->resulttemp[i];
    }
    // now receive results for the initial sends
    for (i = 0; i < (context->procCount - 1); i++)
    {

        MPI_Recv(&(context->resulttemp[i]), 1, MPI_DOUBLE, i + 1, RESULT, MPI_COMM_WORLD, &status);
        result += context->resulttemp[i];
        recvcount++;

    }
    // now display the result
    // printf("\nHi, I am process 0, the result is %f\n", result);
    printf("Proc count: %i", context->procCount);
    printf("\n Squares sin result: %f \n", result);
    return true;
}

bool InitializeSlaveBuffers(MPI_Proc_Context* context)
{
    context->requests = (MPI_Request *)malloc(2 * sizeof(MPI_Request));
    context->requests[0] = context->requests[1] = MPI_REQUEST_NULL;
    context->ranges = (double *)malloc(2 * sizeof(double));
    context->resulttemp = (double *)malloc(2 * sizeof(double));

    if (!context->requests || !context->ranges || !context->resulttemp)
    {
        printf("\nNot enough memory");
        MPI_Finalize();
        return false;
    }
    return true;
}

void HandleSlave(MPI_Proc_Context* context)
{
    int requestcount = 0;
    double range[2];
    double result = 0;
    MPI_Status status;

    InitializeSlaveBuffers(context);

    // first receive the initial data
    MPI_Recv(range, 2, MPI_DOUBLE, 0, DATA, MPI_COMM_WORLD, &status);
    while (range[0] < range[1])
    {
        // if there is some data to process
        // before computing the next part start receiving a new data part
        MPI_Irecv(context->ranges, 2, MPI_DOUBLE, 0, DATA, MPI_COMM_WORLD, &(context->requests[0]));

        // compute my part
        context->resulttemp[1] = SimpleIntegration(range[0], range[1]);
        // now finish receiving the new part
        // and finish sending the previous results back to the master
        MPI_Waitall(2, context->requests, MPI_STATUSES_IGNORE);
        
        range[0] = context->ranges[0];
        range[1] = context->ranges[1];
        context->resulttemp[0] = context->resulttemp[1];

        // and start sending the results back
        MPI_Isend(&(context->resulttemp[0]), 1, MPI_DOUBLE, 0, RESULT, MPI_COMM_WORLD, &(context->requests[1]));    

    }
    // now finish sending the last results to the master
    MPI_Wait(&(context->requests[1]), MPI_STATUS_IGNORE);
}

int main(int argc, char **argv)
{
    MPI_Init(&argc, &argv);
    MPI_Proc_Context context;

    if (!Startup(&context))
    {
        MPI_Finalize();
        return -1;
    }


    // now the master will distribute the data and slave processes will perform computations
    if (context.myRank == 0)
    {
        HandleMaster(&context);
    }
    else //slave
    {
        HandleSlave(&context);
    }

    // Shut down MPI
    MPI_Finalize();

    return 0;
}
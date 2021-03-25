#include <stdio.h>
#include <mpi.h>
#include <math.h>
#include <stdbool.h>
#define PRECISION 0.001
#define RANGESIZE 1
#define DATA 0
#define RESULT 1
#define FINISH 2

#define DEBUG

double f(double x)
{
    return sin(x) * sin(x) / x;
}

double SimpleIntegration(double a, double b)
{
    double i;
    double sum = 0;
    for (i = a; i < b; i += PRECISION)
        sum += f(i) * PRECISION;
    return sum;
}

bool hasSufficientProcessCount(int procCount)
{
    if (procCount < 2)
    {
        printf("Run with at least 2 processes");
        MPI_Finalize();
        return false;
    }
    return true;
}

bool hasSufficientRangeCount(int rangeStart, int rangeStop, int procCount)
{
    if (((rangeStop - rangeStart) / RANGESIZE) < 2 * (procCount - 1))
    {
        printf("More subranges needed");
        MPI_Finalize();
        return false;
    }
    return true;
}

#define START -100
#define STOP 100

bool SendNextChunkToTarget(int target)
{
    static const double step = 1;
    static double currentRangeStart = START;

    double range[2];
    range[0] = currentRangeStart;
    range[1] = currentRangeStart + step;

#ifdef DEBUG
    printf("\nMaster sending range %f,%f to process %d", range[0], range[1], target);
    fflush(stdout);
#endif
    // send it to process i
    MPI_Send(range, 2, MPI_DOUBLE, target, DATA, MPI_COMM_WORLD);
    currentRangeStart += step;
    return currentRangeStart < STOP;
}

void ReceiveFromAnyProcess(double *resultStore, MPI_Status *status)
{
    MPI_Recv(resultStore, 1, MPI_DOUBLE, MPI_ANY_SOURCE, RESULT, MPI_COMM_WORLD, status);
#ifdef DEBUG
    printf("\nMaster received result %f from process %d", *resultStore, status->MPI_SOURCE);
    fflush(stdout);
#endif
}

void KillAllSlaves(int procCount)
{
    for (int i = 1; i < procCount; i++)
    {
        MPI_Send(NULL, 0, MPI_DOUBLE, i, FINISH, MPI_COMM_WORLD);
    }
}

void HandleMaster(int procCount)
{
    MPI_Status status;
    double resultTemp, result = 0;
    int procNum;

    for (procNum = 1; procNum < procCount; procNum++)
    {
        SendNextChunkToTarget(procNum);
    }

    bool isAnythingLeftToSend = true;
    do
    {
        MPI_Recv(&resultTemp, 1, MPI_DOUBLE, MPI_ANY_SOURCE, RESULT, MPI_COMM_WORLD, &status);
        ReceiveFromAnyProcess(&resultTemp, &status);
        result += resultTemp;

        isAnythingLeftToSend = SendNextChunkToTarget(status.MPI_SOURCE);
    } while (isAnythingLeftToSend);

    for (procNum = 1; procNum < procCount; procNum++)
    {
        ReceiveFromAnyProcess(&resultTemp, &status);
        result += resultTemp;
    }

    printf("\nHi, I am process 0, the result is %f\n", result);
}

void HandleSlave(int rank)
{
    MPI_Status status;
    double range[2];
    double resultTemp = 0;

    do
    {
        MPI_Probe(0, MPI_ANY_TAG, MPI_COMM_WORLD, &status);

        #ifdef DEBUG
        printf("Slave no: %d probes for data", rank);
        #endif

        if (status.MPI_TAG == DATA)
        {
            MPI_Recv(range, 2, MPI_DOUBLE, 0, DATA, MPI_COMM_WORLD, &status);
            resultTemp = SimpleIntegration(range[0], range[1]);
            MPI_Send(&resultTemp, 1, MPI_DOUBLE, 0, RESULT, MPI_COMM_WORLD);
        }
    }
    while (status.MPI_TAG != FINISH) ;
}

int main(int argc, char **argv)
{
    MPI_Init(&argc, &argv);

    int myRank, procCount;
    MPI_Comm_rank(MPI_COMM_WORLD, &myRank);
    MPI_Comm_size(MPI_COMM_WORLD, &procCount);
    bool isMaster = myRank == 0;

    if (!hasSufficientProcessCount(procCount) || !hasSufficientRangeCount(START, STOP, procCount))
        return -1;

    if (isMaster)
        HandleMaster(procCount);
    else
        HandleSlave(myRank);

    MPI_Finalize();

    return 0;
}
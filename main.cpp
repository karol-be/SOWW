#include <stdio.h>
#include <mpi.h>
#include <cmath>

double rect_area_under_sin(double rangeStart, double rangeStop, int sampleCount)
{
  auto increment = (rangeStop - rangeStart) / sampleCount;

  double areaSum = 0;
  auto x = rangeStart + 0.5 * increment;
  while (x < rangeStop)
  {
    auto sinAtX = sin(x);
    areaSum += sinAtX * increment;
    x += increment;
  }

  return areaSum;
}

double trapesoid_area_under_sin(double rangeStart, double rangeStop, double sampleCount)
{
  auto increment = (rangeStop - rangeStart) / sampleCount;

  double areaSum = 0;
  auto x = rangeStart;
  while (x + increment < rangeStop)
  {
    auto sinAtX = sin(x);
    auto sinAtXp1 = sin(x + increment);
    areaSum += (2 * sinAtX + sinAtXp1) * increment / 2;
    x += increment;
  }

  return areaSum;
}

int main(int argc, char **argv)
{
  double precision = 1000 * 1000 * 50;
  int myRank, procCount;
  double rectArea, trapArea, sinAreaFinal, trapAreaFinal;
  double globalRangeStart = -2 * M_PI;
  double globalRangeStop = 2 * M_PI;

  MPI_Init(&argc, &argv);

  MPI_Comm_rank(MPI_COMM_WORLD, &myRank);
  MPI_Comm_size(MPI_COMM_WORLD, &procCount);

  double processRangeSize = (globalRangeStop - globalRangeStart) / procCount;
  printf("Proc count: %i", procCount);
  if (precision < procCount)
  {
    printf("Precision smaller than the number of processes - try again.");
    MPI_Finalize();
    return -1;
  }

  double processRangeStart = globalRangeStart + myRank * processRangeSize;
  double processRangeStop = processRangeStart + processRangeSize;

  rectArea = rect_area_under_sin(processRangeStart, processRangeStop, precision / procCount);
  trapArea = trapesoid_area_under_sin(processRangeStart, processRangeStop, precision / procCount);

  MPI_Reduce(&rectArea, &sinAreaFinal, 1, MPI_DOUBLE, MPI_SUM, 0, MPI_COMM_WORLD);
  MPI_Reduce(&trapArea, &trapAreaFinal, 1, MPI_DOUBLE, MPI_SUM, 0, MPI_COMM_WORLD);

  if (!myRank)
  {
    printf("\n SinArea is: %f \n", sinAreaFinal);
    printf("\n TrapArea is: %f \n", trapAreaFinal);
  }

  MPI_Finalize();

  return 0;
}

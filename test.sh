#!
echo "Compiling both labs"
mpiCC ./lab1/lab1.cpp -o ./lab1/lab1.out
mpiCC ./lab2/lab2.c -o ./lab2/lab2.out
echo "Compilation done"

echo -e '\n'
echo "Running blocking version"
time mpirun ./lab1/lab1.out

echo -e '\n'
echo "Running nonblocking version"
time mpirun ./lab2/lab2.out

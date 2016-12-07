#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <mpi.h>

int func(int* matrix,int row, int n) {
	int result = 0, j;
	for (j = 0; j < n; j++)
		result += *(matrix + row * n + j);
	return result;
}

int main(int argc, char** argv) {
	int procNum, procRank;
	FILE *infile;
	int* matrix;
	int* parall_result;
	int* norm_result;
	int* local_result;
	int i, j, m, n;
	double start_time = 0.0, end_time = 0.0, parall_time = 0.0, norm_time = 0.0;

	MPI_Init(&argc, &argv);

	MPI_Comm_rank(MPI_COMM_WORLD, &procRank);
	MPI_Comm_size(MPI_COMM_WORLD, &procNum);

	if (procRank == 0) {
		infile = fopen("input.txt", "r");
		if (infile == NULL) {
			printf("Can not open the file\n");
			exit(1);
		}
		fscanf(infile, "%d %d", &m, &n);
	}

	MPI_Bcast(&m, 1, MPI_INT, 0, MPI_COMM_WORLD);
	MPI_Bcast(&n, 1, MPI_INT, 0, MPI_COMM_WORLD);
	matrix = (int*)malloc(m * n * sizeof(int));

	if (procRank == 0) {
		for (i = 0; i < m; i++)
			for (j = 0; j < n; j++)
				fscanf(infile, "%d", &matrix[n*i + j]);
		fclose(infile);
	}

	parall_result = (int*)malloc(m * sizeof(int));
	for (i = 0; i < m; i++)
		parall_result[i] = 0;
	local_result = (int*)malloc(m * sizeof(int));
	for (i = 0; i < m; i++)
		local_result[i] = 0;

	MPI_Bcast(matrix, (m*n), MPI_INT, 0, MPI_COMM_WORLD);

	if(procRank == 0)
		start_time = MPI_Wtime();

	for (i = procRank; i <= m; i += procNum) {
		local_result[i] = func(matrix, i, n);
	}

	MPI_Reduce(local_result, parall_result, m, MPI_INT, MPI_SUM, 0, MPI_COMM_WORLD);
	
	if (procRank == 0) {
		end_time = MPI_Wtime();
		parall_time = end_time - start_time;
		printf("parallel time = %f\n", parall_time);
		//for (i = 0; i < m; i++)
			//printf("%d ", parall_result[i]);
		printf("\n");
		
		norm_result = (int*)malloc(m * sizeof(int));
		for (i = 0; i < m; i++)
			norm_result[i] = 0;

		start_time = MPI_Wtime();
		for (i = 0; i < m; i++)
			for (j = 0; j < n; j++)
				norm_result[i] += matrix[i*n + j];
		end_time = MPI_Wtime();
		norm_time = end_time - start_time;
		printf("norm time = %f\n", norm_time);
		//for (i = 0; i < m; i++)
			//printf("%d ", norm_result[i]);
				
	}

	MPI_Finalize();

	return 0;
}
#include "mpi.h"
#include <iostream>
#include <ctime>
#include <string>

#define MainProccess 0 // Ранг главного процесса

template <typename T1>
inline void PrintArray(const T1* Array, const int SizeArray, const std::string nameArr = "")
{
	std::cout << nameArr;
	for (int i = 0; i < SizeArray; ++i)
		std::cout << " " << Array[i];
}

inline void Random_massive(int* A, int SizeMassive, int min = 0, int max = 32767)
{
	srand(unsigned(time(0)));
	for (int i = 0; i < SizeMassive; ++i)
		A[i] = rand() % max + min / 2;
}

inline void Initialize_Massive_size(const int argc, char* argv[], int &SizeMassive)
{
	// после имени программы можно задать количество элементов массива
	int n1 = 0;

	if (argc > 1)
		n1 = atoi(argv[1]);

	if (n1 <= 0 || n1 > 1000000000000)
		n1 = 10000;

	SizeMassive = n1;
}

template <typename T1>
inline bool Test(const T1* A, const int n)
{
	bool OK = true;
	std::cout << "\n Check the results... ";
	for (int i = 1; i < n; ++i)
		if (A[i - 1] > A[i])
		{
			OK = false;
			break;
		}
	if (OK != true)
		std::cout << "Something goes wrong." << std::endl;
	else
		std::cout << "Successfully" << std::endl;

	return OK;
}

inline void CheckResults(const double sequentTimeWork, const double parallTimeWork)
{
	if (parallTimeWork < sequentTimeWork)
		std::cout << "Parallel algorithm is faster" << std::endl;
	else
		std::cout << "Sequential algorithm is faster" << std::endl;

	std::cout.precision(3);
	std::cout.setf(std::ios::fixed);
	std::cout << " Speedup: " << sequentTimeWork / parallTimeWork << std::endl;
}

template <typename T1>
void MSD_RadixSort(T1 A[], const int n, int byte)
{
	int count[256], offset[256];

	for (int i = 0; i < 256; ++i)
		count[i] = 0;

	for (int i = 0; i < n; ++i)
	{
		if (byte == 3)
			count[((A[i] >> (byte * 8)) + 128) & 0xff]++;
		else
			count[(A[i] >> (byte * 8)) & 0xff]++;
	}

	offset[0] = 0;
	for (int i = 1; i < 256; ++i)
		offset[i] = offset[i - 1] + count[i - 1];

	int *Result = new int[n];
	for (int i = 0; i < n; ++i)
	{
		if (byte == 3)
			Result[offset[((A[i] >> (byte * 8)) + 128) & 0xff]++] = A[i];
		else
			Result[offset[(A[i] >> (byte * 8)) & 0xff]++] = A[i];
	}

	for (int i = 0; i < n; ++i)
		A[i] = Result[i];

	delete[] Result;

	for (int i = 0; i < 256; ++i)
	{
		if (count[i] > 1 && byte > 0)
			MSD_RadixSort(A + offset[i] - count[i], count[i], byte - 1);
		if (offset[i] == n) break;
	}
}

template <typename T1>
inline double MyMSD_RadixSort(T1 A[], const int n)
{
	double time_start, time_end;
	time_start = MPI_Wtime();

	MSD_RadixSort(A, n, 3);

	time_end = MPI_Wtime();
	return (time_end - time_start) * 1000;
}

// Parallel algorithm 

inline void Put_together(int buf_Proc[], const int workload, const int id_proc_send, const int workload1, int* merge)
{
	MPI_Status Status;
	int* buf = new int[workload];
	MPI_Recv(buf, workload1, MPI_INT, id_proc_send, MPI_ANY_TAG, MPI_COMM_WORLD, &Status);

	int ind_buf_Proc = 0;
	int ind_buf = 0;
	int ind_buf_merge = 0;

	// Слияние, merge всегда упорядочен
	while (ind_buf_Proc < workload && ind_buf < workload1)
		if (buf_Proc[ind_buf_Proc] < buf[ind_buf])
			merge[ind_buf_merge++] = buf_Proc[ind_buf_Proc++];
		else
			merge[ind_buf_merge++] = buf[ind_buf++];

	while (ind_buf < workload1)
		merge[ind_buf_merge++] = buf[ind_buf++];
	delete[] buf;

	while (ind_buf_Proc < workload)
		merge[ind_buf_merge++] = buf_Proc[ind_buf_Proc++];

}

double Parallel_MSD_RadixSort(int A[], const int n, const int ProcRank, const int ProcNum)
{
	double time_start, time_end;

	int* sendcounts = new int[ProcNum];
	int* displs = new int[ProcNum];

	int mid_workload = n / ProcNum;
	int r = n % ProcNum;

	for (int i = 0; i < r; ++i)
	{
		displs[i] = i * (mid_workload + 1);
		sendcounts[i] = mid_workload + 1;
	}

	for (int i = r; i < ProcNum; ++i)
	{
		displs[i] = mid_workload * i + r;
		sendcounts[i] = mid_workload;
	}

	int workload = sendcounts[ProcRank]; // загрузка процесса
	int *buf_Proc = new int[workload]; // буфер процесса

	MPI_Barrier(MPI_COMM_WORLD);
	time_start = MPI_Wtime();

	MPI_Scatterv(A, sendcounts, displs, MPI_INT, buf_Proc, workload, MPI_INT, MainProccess, MPI_COMM_WORLD);

	MSD_RadixSort(buf_Proc, workload, 3);


	int lastIteration = int(ceil(log2(ProcNum)));
	for (int i = 0, delta = 1; i < lastIteration; ++i, delta *= 2)
	{
		if ((ProcRank % (delta * 2)) == 0 && ProcNum > ProcRank + delta)
		{
			MPI_Status Status;
			int workload1;
			MPI_Recv(&workload1, 1, MPI_INT, ProcRank + delta, MPI_ANY_TAG, MPI_COMM_WORLD, &Status);


			int* merge = new int[workload1 + workload];
			Put_together(buf_Proc, workload, ProcRank + delta, workload1, merge);

			delete[] buf_Proc;
			buf_Proc = new int[workload + workload1];


			for (int i = 0; i < workload + workload1; ++i)
				buf_Proc[i] = merge[i];
			delete[] merge;

			workload += workload1;
		}
		else
			if ((ProcRank - delta) % (delta * 2) == 0)
			{
				MPI_Send(&workload, 1, MPI_INT, ProcRank - delta, 0, MPI_COMM_WORLD);
				MPI_Send(buf_Proc, workload, MPI_INT, ProcRank - delta, 0, MPI_COMM_WORLD);
			}
	}

	if (ProcRank == MainProccess)
	{
		for (int i = 0; i < workload; ++i)
			A[i] = buf_Proc[i];
	}

	time_end = MPI_Wtime();
	delete[] sendcounts;
	delete[] displs;
	delete[] buf_Proc;

	return (time_end - time_start) * 1000;
}

int main(int argc, char* argv[])
{
	int ProcNum;
	int	ProcRank;
	int SizeMassive = 0; // Количество элементов массива
	int *Massive = NULL; // Массив для сортировки

	double parallTimeWork = 0;
	double sequentTimeWork = 0;

	MPI_Init(&argc, &argv);
	MPI_Comm_size(MPI_COMM_WORLD, &ProcNum);
	MPI_Comm_rank(MPI_COMM_WORLD, &ProcRank);

	Initialize_Massive_size(argc, argv, SizeMassive);

	if (ProcRank == MainProccess)
	{
		Massive = new int[SizeMassive];
		Random_massive(Massive, SizeMassive, -10000, 10000);

		// Массив для последовательного алгоритма
		int *Clone_Massive = new int[SizeMassive];
		for (int i = 0; i < SizeMassive; ++i)
			Clone_Massive[i] = Massive[i];

		// PrintArray(Clone_Massive, SizeMassive, "\n seq_Massive = ");

		sequentTimeWork = MyMSD_RadixSort(Clone_Massive, SizeMassive);

		//PrintArray(Clone_Massive, SizeMassive, "\n seq_Massive = ");
		std::cout << " Massive size = " << SizeMassive << " \n";
		std::cout << "\n MSD_RadixSort ";
		if (Test(Clone_Massive, SizeMassive))
			std::cout << " time = " << sequentTimeWork << "ms\n \n\n";

		delete[] Clone_Massive;
	}

	parallTimeWork = Parallel_MSD_RadixSort(Massive, SizeMassive, ProcRank, ProcNum);

	if (ProcRank == MainProccess)
	{
		//PrintArray(Massive, SizeMassive, "\n parl1_Massive = ");
		std::cout << " Parallel_MSD_RadixSort ";
		if (Test(Massive, SizeMassive))
			std::cout << " time = " << parallTimeWork << "ms\n \n";

		delete[] Massive;

		CheckResults(sequentTimeWork, parallTimeWork);
	}

	MPI_Finalize();
	return 0;
}
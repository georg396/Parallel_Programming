#include "mpi.h"
#include <stdlib.h>
#include <iostream>

#define CLIENT_APPEARED 101
#define BARBER_WALK 103
#define NO_PLACE 104
#define HAIRCUT_DONE 105
#define STOP 106

using namespace std;

int main(int argc, char* argv[]) {
	int rank, size;
	const int clientMax = 5;
	int clientNum = 0;
	int leaved = 0;
	int i = 0;
	int doneClient = 0;
	bool client = false;
	bool stopFlag = false;
	bool barberIsBusy = false;

	int queue[clientMax];
	for (int j = 0; j < clientMax; j++)
		queue[j] = 0;

	MPI_Init(&argc, &argv);

	MPI_Comm_rank(MPI_COMM_WORLD, &rank);
	MPI_Comm_size(MPI_COMM_WORLD, &size);

	if (rank == 0) { // monitor
		while (true) {
			MPI_Status status;
			MPI_Recv(&i, 1, MPI_INT, MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &status);
			if (status.MPI_TAG == CLIENT_APPEARED) {
				if (clientNum < clientMax) {
					clientNum++;
					for (int j = 0; j < clientMax; j++)
						if (queue[j] == 0) {
							queue[j] = status.MPI_SOURCE;
							break;
						}
					MPI_Send(&i, 1, MPI_INT, 1, CLIENT_APPEARED, MPI_COMM_WORLD);
				}
				else {
					leaved++;
					MPI_Send(&i, 1, MPI_INT, status.MPI_SOURCE, NO_PLACE, MPI_COMM_WORLD);
				}
			}
			else if (status.MPI_TAG == BARBER_WALK) {
				clientNum--;
				barberIsBusy = true;
				doneClient = queue[0];
				i = doneClient;
				cout << "[WWW] Client from proc" << doneClient << " is at the barber" << endl; //
				MPI_Send(&i, 1, MPI_INT, doneClient, BARBER_WALK, MPI_COMM_WORLD);
				MPI_Send(&i, 1, MPI_INT, 1, BARBER_WALK, MPI_COMM_WORLD);
				for (int j = 0; j < clientMax - 1; j++)
					queue[j] = queue[j + 1];
				queue[clientMax - 1] = 0;
			}
			else if (status.MPI_TAG == HAIRCUT_DONE) {
				barberIsBusy = false;
				cout << "[DDD] Client from proc" << i << " done haircut" << endl; //
				MPI_Send(&i, 1, MPI_INT, i, HAIRCUT_DONE, MPI_COMM_WORLD);
			}
			else if (status.MPI_TAG == STOP) {
				stopFlag = true;
			}
			if (clientNum == 0 && queue[0] == 0 && stopFlag == true && barberIsBusy == false) {
				MPI_Send(&i, 1, MPI_INT, 1, STOP, MPI_COMM_WORLD);
				cout << "STOP" << endl;
				break;
			}
		}
	}

	else if (rank == 1) { // barber
		while (true) {
			MPI_Status status;
			MPI_Recv(&i, 1, MPI_INT, 0, MPI_ANY_TAG, MPI_COMM_WORLD, &status);
			if (status.MPI_TAG == CLIENT_APPEARED) {
				MPI_Send(&i, 1, MPI_INT, 0, BARBER_WALK, MPI_COMM_WORLD);
				MPI_Recv(&i, 1, MPI_INT, 0, BARBER_WALK, MPI_COMM_WORLD, &status);
				_sleep(rand() % 250 + 100);
				MPI_Send(&i, 1, MPI_INT, 0, HAIRCUT_DONE, MPI_COMM_WORLD);
			}
			else if (status.MPI_TAG == STOP)
				break;
		}
	}
	else { // client
		int counter = 0;
		while (true) {
			int r = 75;
			if (r > 25) {
				client = true;
				cout << "[GGG] Client generated from proc" << rank << endl; //
			}
			if (client == true) {
				MPI_Status status;
				MPI_Send(&i, 1, MPI_INT, 0, CLIENT_APPEARED, MPI_COMM_WORLD);
				MPI_Recv(&i, 1, MPI_INT, 0, MPI_ANY_TAG, MPI_COMM_WORLD, &status);
				if (status.MPI_TAG == NO_PLACE)
					client = false;
				else if (status.MPI_TAG == BARBER_WALK) {
					MPI_Recv(&i, 1, MPI_INT, 0, HAIRCUT_DONE, MPI_COMM_WORLD, &status);
					client = false;
				}
				else if (status.MPI_TAG == STOP)
					break;
			}
			counter++;
			if (counter == 5) {
				MPI_Send(&i, 1, MPI_INT, 0, STOP, MPI_COMM_WORLD);
				break;
			}
		}
	}

	//MPI_Barrier(MPI_COMM_WORLD);
	MPI_Finalize();

	if (rank == 0)
		cout << "Clients leaved: " << leaved << endl;

	return 0;
}
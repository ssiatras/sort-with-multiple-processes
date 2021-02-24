#include <sys/types.h>
#include <sys/times.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>

#include "../records/record.h"
#include "../defines.h"

void quicksort(recordPtr *, long, long, int);  // Quick Sort Algorithm
long partition(recordPtr *, long, long, int); // Partition Algorithm for Quick Sort

int main(int argc, char *argv[]) {
	double t1, t2, dt;
	struct tms tb1, tb2;

	double ticspersec = (double) sysconf(_SC_CLK_TCK);

	t1 = (double) times(&tb1); // Start timer

	if (argc < 5) {
		fprintf(stderr, "Too few arguments.\nUsage: './quicksorter inputfile low high columnid'\n");

		return 1;
	}

	else if (argc > 5) {
		fprintf(stderr, "Too many arguments.\nUsage: './quicksorter inputfile low high columnid'\n");

		return 1;
	}

	long low = atol(argv[2]);
	if (low < 0) {
		fprintf(stderr, "Invalid 'low' parameter.\n");

		return 4;
	}

	long high = atol(argv[3]);
	if (high < 0) {
		fprintf(stderr, "Invalid 'high' parameter.\n");

		return 4;
	}

	long numOfRecords = high - low + 1; // Number of records
	if (numOfRecords < 0) { // If low > high
		fprintf(stderr, "Invalid 'low' and 'high' parameters.\n");

		return 4;
	}

	int columnid = atoi(argv[4]);
	if ((columnid < 1) || (columnid > 8)) {
		fprintf(stderr, "Invalid 'columnid' parameter.\n");

		return 4;
	}

	char *input = NULL;
	FILE *finput = NULL;

	input = argv[1];
	finput = fopen(input, "r");
	if (finput == NULL) {
		fprintf(stderr, "Invalid input file: File does not exist.\n");

		return 2;
	}

	size_t recordSize = sizeof(record); // Get size of struct record

	fseek(finput, low * recordSize, SEEK_SET); // Start reading from low element

	recordPtr *recordList = malloc(numOfRecords * sizeof(recordPtr));  // Malloc array of pointers to records

	for (long i = 0; i < numOfRecords; i++) { // Malloc records and read their data from binary input file
		recordList[i] = recordInit();

		fread(recordList[i], recordSize, 1, finput);
	}

	if (fclose(finput) == EOF) { // Check fclose failure
		fprintf(stderr, "Error closing input file.\n");

		return 3;
	}
	finput = NULL;
	input = NULL;

	quicksort(recordList, 0, numOfRecords - 1, columnid);

	t2 = (double) times(&tb2); // Stop timer

	size_t recordPipeCap = PIPECAPACITY / recordSize; // The pipe capacity in number of records
	long recordsTransferred = 0; // Number of records that have been transferred

	while (numOfRecords > recordsTransferred) { // While there are still records to be transferred
		if (numOfRecords - recordsTransferred >= recordPipeCap) { // If there are records left more than (or equal) to the record pipe capacity
			void *pipeBuff = malloc(recordPipeCap * recordSize); // Malloc buffer to get records in one buffer (we only had array of pointers)

			for (long i = 0; i < recordPipeCap; i++) {  // Memcpy records to one buffer
				
				memcpy(pipeBuff + i * recordSize, recordList[recordsTransferred + i], recordSize);
			}

			if (write(STDOUT_FILENO, pipeBuff, recordSize * recordPipeCap) == -1) { // Check write failure

				perror("Write to coach-sorter pipe");
			}

			free(pipeBuff);
			recordsTransferred += recordPipeCap; // Update the number of records that have been transferred
		}

		else { // records left < recordPipeCap
			void *pipeBuff = malloc((numOfRecords - recordsTransferred) * recordSize);

			for (long i = 0; i < numOfRecords - recordsTransferred; i++) {  // Memcpy records to one buffer
				
				memcpy(pipeBuff + i * recordSize, recordList[recordsTransferred + i], recordSize);
			}

			if (write(STDOUT_FILENO, pipeBuff, (numOfRecords - recordsTransferred) * recordSize) == -1) { // Check write failure

				perror("Write to coach-sorter pipe");
			}

			free(pipeBuff);
			recordsTransferred += (numOfRecords - recordsTransferred); // Update the number of records that have been transferred
		}
	}

	for (long i = 0; i < numOfRecords; i++) {  // Free records

		recordFree(recordList[i]);
	}

	free(recordList);
	recordList = NULL;

	dt = (t2 - t1) / ticspersec;
	if (write(STDOUT_FILENO, &dt, sizeof(double)) == -1) { // Check write failure

		perror("Write to coach-sorter pipe");
	}

	kill(getppid(),SIGUSR2); // Send SIGUSR2 Signal to parent

	return 0;
}

void quicksort(recordPtr *a, long l, long r, int col) { // Quick Sort Algorithm
	if (l < r) {
		long p;

		p = partition(a, l, r, col);
		quicksort(a, l, p - 1, col);
		quicksort(a, p + 1, r, col);
	}
}

long partition(recordPtr *a, long l, long r, int col) {
	long i, j;
	recordPtr pivot = a[r];

	i = l - 1;
	j = l;

	while (j <= r - 1) {
		if (recordCmp(a[j], pivot, col) < 0) { // If a[j] < pivot
			i++;

			recordSwap(&a[i], &a[j]);
		}

		j++;
	}
	recordSwap(&a[i + 1], &a[r]);

	return i + 1;
}

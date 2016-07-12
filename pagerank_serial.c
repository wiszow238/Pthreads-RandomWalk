#include <stdio.h> 
#include <string.h>
#include <stdlib.h> 
#include <pthread.h>
#include <ctype.h>
#include <math.h>
#include <stdbool.h>
#include <sys/time.h>

typedef struct { 
   double* values;   //Array of node values in one row
   long int* col;    //Array of col_inds in one row
   long int vcount;  //Number of nodes in one row
   int rowId;
} rowData;

struct row {
   rowData* rowary;  //Stores data for each row
   int rcount;       //Number of rows
};

struct pNode {
   int val;
   struct pNode *next;
   struct pNode *last;
};

struct cNode {
   int val;
   int numofReq;
   struct pNode *reqLink;
   struct cNode *next;
};

struct finalData {
   int* stepCount;
   int* nodeId;
};

struct comData{
   int stepCount;
   int nodeId;
};

int compareSort(const void * a, const void * b) {
   struct comData *orderA = (struct comData *)a;
   struct comData *orderB = (struct comData *)b;
   return ( orderB->stepCount - orderA->stepCount );
}

int max_threads, numberofRows;
void *compute (void *);
pthread_mutex_t sumLock;

pthread_barrier_t barr;

struct finalData *partitionedResultArray;

struct row matrixdata;
int* partitionedStartingNodes;
int walkLength;

int main(int argc, char **argv) { 
   int partitionCount = 1;

   max_threads = partitionCount;
   pthread_t p_threads[max_threads];
   pthread_attr_t attr;
   pthread_attr_init(&attr);
   pthread_mutex_init(&sumLock, NULL);
   pthread_barrier_init(&barr, NULL, max_threads);

   struct timeval start, end;
   FILE *pFile = fopen(argv[1], "r");
   if (pFile==NULL) {
      printf("File error");
      fputs ("File error", stderr); 
      exit(1);
   }

   int size;
   fseek (pFile, 0, SEEK_END);
   size = ftell(pFile);
   rewind (pFile);

   int i = 0;
   char st[20];
   int readRowsPtrs = 0;
   long int* rownum = (long int *) malloc(size * sizeof(long int));

   printf("READING LAST LINE\n");
   while (fscanf(pFile, "%20s", st) != EOF) {
      if (readRowsPtrs == 1) {
         long int v;
         v = strtol(st, NULL, 0);
         rownum[i] = v;
         i++;
      }

      if (strcmp(st, "row_ptrs:") == 0) {
         readRowsPtrs = 1;
      }
   }
   rewind(pFile);
   printf("LAST LINE READ\n");

   // struct row matrixdata;
   matrixdata.rowary = malloc(i * sizeof(rowData));
   matrixdata.rcount = i-1;

   int c;
   //Initialize the matrix and the multiply array
   for (c=0; c < matrixdata.rcount; c++) { //i is the total amount of rows in the matrix
      long int rowSize;
      rowSize = rownum[c+1] - rownum[c];

      matrixdata.rowary[c].values = malloc((rowSize+1) * sizeof(double));
      matrixdata.rowary[c].col = malloc((rowSize+1) * sizeof(long int));
      matrixdata.rowary[c].vcount = rowSize;
   }
   free(rownum);

   i = 0;
   c = 0;      //Matrix row incrementor
   int newline = 0;
   int addSelfPointer = 0;
   char ch;

   //Read matrix file and populate matrix data structure
   printf("POPULATING MATRIX\n");
   while (fscanf(pFile, "%s%c", st, &ch) != EOF && newline < 3) {
      if(isdigit(st[0])) {
         if(i == matrixdata.rowary[c].vcount) {
            if (newline == 2 && addSelfPointer == 0) {
               matrixdata.rowary[c].col[i] = (long int)c;
               matrixdata.rowary[c].values[i] = 1.0;//matrixdata.rowary[c].values[i];
               matrixdata.rowary[c].vcount++;
            }
            i = 0;
            addSelfPointer = 0;
            c++;
         }
         if(c > matrixdata.rcount) {
            printf("Number of rows is more than allocated rows\n");
            exit(1);
         }

         //Determine what the current values represent
         if(newline == 1) {
            //Convert String values to double strtod 
            double v;
            v = strtod(st, NULL);
            matrixdata.rowary[c].values[i] = v;
         } else if (newline == 2) { //if(strcmp(header, "col_inds:") == 0) {
            //Convert String values to int strtol
            long int v;
            v = strtol(st, NULL, 0);
            if ((long int)c == v)
               addSelfPointer = 1;
            else if ((long int)c < v && addSelfPointer == 0) {
               matrixdata.rowary[c].col[i] = (long int)c;
               matrixdata.rowary[c].values[i] = matrixdata.rowary[c].values[i];
               matrixdata.rowary[c].vcount++;
               addSelfPointer = 1;
               i++;
            } 
            matrixdata.rowary[c].col[i] = v;
            matrixdata.rowary[c].values[i] = 1;//matrixdata.rowary[c].values[i];
         } 
         i++; //Counts the total number of nodes
      } else {
         c=0;
         i=0;
         addSelfPointer = 0;
         newline++;
      }
      
      if (ch == '\n') { //END OF LINE
         if (newline == 2 && addSelfPointer == 0) {
            matrixdata.rowary[c].col[i] = (long int)c;
            matrixdata.rowary[c].values[i] = 1.0;//matrixdata.rowary[c].values[i];
            matrixdata.rowary[c].vcount++;
         }
         
         c = 0;
         i = 0;
         addSelfPointer = 0;
      }
   }

   // printf("---\n");
   // //%ld for long int 
   // for (c=0; c < matrixdata.rcount; c++) {
   //    printf("rowId: %d  ", c);
   //    for (i=0; i < matrixdata.rowary[c].vcount; i++) {
   //       printf("(col: %ld, val: %.1f) ", matrixdata.rowary[c].col[i], matrixdata.rowary[c].values[i]);
   //    }
   //    printf("vcount: %ld\n", matrixdata.rowary[c].vcount);
   // }

   printf("END OF FILE\n");
   fclose(pFile);
   printf("FILE CLOSED\n");

   partitionedStartingNodes = malloc(partitionCount * sizeof(int));
   printf("PARTITION MATRIX\n");
   partitionedStartingNodes[0] = 0;
   for (c=1; c<partitionCount; c++) {
      partitionedStartingNodes[c] = partitionedStartingNodes[c-1] + (int)ceil((double)matrixdata.rcount/(double)partitionCount);
   }
   printf("MATRIX PARTITIONED\n");

   numberofRows = matrixdata.rcount;
   partitionedResultArray = malloc(partitionCount * sizeof(struct finalData));
   for(c=0; c<partitionCount; c++) {
      partitionedResultArray[c].stepCount = malloc(numberofRows * sizeof(int));
      partitionedResultArray[c].nodeId    = malloc(numberofRows * sizeof(int));
      for(i=0; i<numberofRows; i++) {
         partitionedResultArray[c].stepCount[i] = 0;
         partitionedResultArray[c].nodeId[i]    = i;
      }
   }
   // srand(time(NULL)); 

   gettimeofday(&start, NULL);
   walkLength = atoi(argv[2]);
   printf("START WALK\n");
   for(i=0; i<max_threads; i++) {
      int *arg = malloc(sizeof(*arg));
      *arg = i;
      pthread_create(&p_threads[i], &attr, compute, arg);
   }

   for (i=0; i<max_threads; i++) {
      pthread_join(p_threads[i], NULL);
   }
   printf("WALK DONE\n");
   gettimeofday(&end, NULL);

   struct comData *combinedResultArray = malloc(numberofRows * sizeof(struct comData));
   for(c=0; c<numberofRows; c++) {
      combinedResultArray[c].stepCount = 0;
      combinedResultArray[c].nodeId    = c;
   }

   for(c=0; c<partitionCount; c++) {
      for(i=0; i<numberofRows; i++) {
         combinedResultArray[i].stepCount = combinedResultArray[i].stepCount + partitionedResultArray[c].stepCount[i];
      }
   }

   int highestVisited = atoi(argv[3]);
   qsort(combinedResultArray, numberofRows, sizeof(struct comData), compareSort);

   long runTime = (end.tv_sec * 1000000 + end.tv_usec) - (start.tv_sec * 1000000 + start.tv_usec);
   printf("TIME: %ld\n", runTime);
   FILE *fp;

   fp = fopen("pagerank.result", "w+");
   fprintf(fp,"time: %ld us\n", runTime);
   fprintf(fp,"node_Id walkCountq\n");
   for (c=0; c < numberofRows; c++) {
      if (highestVisited > c) {
         fprintf(fp, "%d %d\n", combinedResultArray[c].nodeId, combinedResultArray[c].stepCount);
      }
   }
   fclose(fp);

   for(c=0; c<partitionCount; c++) {
      free(partitionedResultArray[c].stepCount);
      free(partitionedResultArray[c].nodeId);
   }
   free(partitionedResultArray);
   free(combinedResultArray);

   for (c=0; c < matrixdata.rcount; c++) {
      free(matrixdata.rowary[c].values);
      free(matrixdata.rowary[c].col);
   }   
   free(matrixdata.rowary);
   free(partitionedStartingNodes);
}

void *compute(void *s){
   // struct timeval ts, te;
   int pnum = *((int *) s);
   //srand(time(NULL));

   int c, i;
   int wl = walkLength;
   int start = partitionedStartingNodes[pnum];
   int end = matrixdata.rcount;
   struct finalData *resultArray = &partitionedResultArray[pnum];

   if (pnum+1 < max_threads) 
      end = partitionedStartingNodes[pnum+1];

   // i=0;
   // printf("(%d) start: %d, end: %d\n", pnum, start, end);
   // gettimeofday(&ts, NULL);
   for (c=start; c < end; c++) {
      // i++;
      int currentNode = c;
      int walkcount;
      for (walkcount=0; walkcount<wl; walkcount++) {
         // i++; nrand48
         int toNode = rand_r((int *) s) % matrixdata.rowary[currentNode].vcount;
         int colNode = matrixdata.rowary[currentNode].col[toNode];
         resultArray->stepCount[colNode]++;
         currentNode = colNode;
      }
   }
   // gettimeofday(&te, NULL);
   // long ttime = (te.tv_sec * 1000000 + te.tv_usec) - (ts.tv_sec * 1000000 + ts.tv_usec);
   // printf("(%d) TIME2: %ld\n", pnum, ttime);

   // printf("(%d) loop count: %d\n", pnum, i);
   free(s);
   pthread_exit(0);
}
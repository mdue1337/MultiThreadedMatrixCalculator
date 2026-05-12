#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

/*
 * Using indexing [i * cols + j];
 * Result will be in m3;
 */
typedef struct 
{
    int rows;
    int cols;
    int *data;
    void *map_start;  // Original 'mapped' pointer. Null if not using mmap.
    size_t map_size;  // Size of the mapped region. Null if not using mmap.
} Matrix;

/*
 * Struct for thread work
 */
typedef struct {
    int     id;
    int     tnum;
    Matrix *m1;
    Matrix *m2;
    Matrix *m3;
} ThreadArgs;

/*
* Helper function to get thread arguments and check for null pointer. Exits if null pointer is found.
*/
ThreadArgs *getThreadArgs(void *arg)
{
    ThreadArgs *t = (ThreadArgs *)arg;
    if (t == NULL)
    {
        printf("Error! Null pointer!\n");
        exit(EXIT_FAILURE);
    }
    return t;
}

/*
 *  Logic for minus is m3[i][j] = m1[i][j] - m2[i][j]
 *  Each thread will work on rows id, id + tnum, id + 2*tnum, ...
 */
void *minus(void *arg)
{
    ThreadArgs *t = getThreadArgs(arg);

    for (int i = t->id; i < t->m3->rows; i += t->tnum) {
        for (int j = 0; j < t->m3->cols; j++) {
            t->m3->data[i * t->m3->cols + j] = t->m1->data[i * t->m1->cols + j] - t->m2->data[i * t->m2->cols + j];
        }
    }
    return NULL;
}

/*
 *  Logic for plus is m3[i][j] = m1[i][j] + m2[i][j]
 *  Each thread will work on rows id, id + tnum, id + 2*tnum, ...
 */
void *plus(void *arg)
{
    ThreadArgs *t = getThreadArgs(arg);

    for (int i = t->id; i < t->m3->rows; i += t->tnum) {
        for (int j = 0; j < t->m3->cols; j++) {
            t->m3->data[i * t->m3->cols + j] = t->m1->data[i * t->m1->cols + j] + t->m2->data[i * t->m2->cols + j];
        }
    }
    return NULL;
}

/*
 *  Logic for mult is m3[i][j] = sum(m1[i][k] * m2[k][j])
 *  Each thread will work on rows id, id + tnum, id + 2*tnum, ...
 */
void *mult(void *arg)
{
    ThreadArgs *t = getThreadArgs(arg);

    for (int i = t->id; i < t->m3->rows; i += t->tnum) {
        for (int j = 0; j < t->m3->cols; j++) {
            int sum = 0;
            for (int k = 0; k < t->m1->cols; k++) {
                sum += t->m1->data[i * t->m1->cols + k] * t->m2->data[k * t->m2->cols + j];
            }
            t->m3->data[i * t->m3->cols + j] = sum;
        }
    }

    return NULL;
}

/*
 * Checks matrix dimensions are valid for the given operation:
 * minus: (m x n) and (m x n)
 * add:   (m x n) and (m x n)
 * mult:  (m x n) and (n x p)
 */
void checkMatrixSize(char *method, Matrix *m1, Matrix *m2)
{
    if (strcmp(method, "plus") == 0 || strcmp(method, "minus") == 0)
    {
        if (m1->rows != m2->rows || m1->cols != m2->cols)
        {
            printf("Size mismatch for %s: (%dx%d) vs (%dx%d)\n",
                   method, m1->rows, m1->cols, m2->rows, m2->cols);
            exit(EXIT_FAILURE);
        }
    }
    else if (strcmp(method, "mult") == 0)
    {
        if (m1->cols != m2->rows)
        {
            printf("Size mismatch for mult: (%dx%d) * (%dx%d)\n",
                   m1->rows, m1->cols, m2->rows, m2->cols);
            exit(EXIT_FAILURE);
        }
    }
}

/*
 * Reads from input.txt, and calls checkMatrixSize()
 * Finally fills arrays. Max matrix size is 100x100, change buffer to handle bigger matrices.
 */
void readTextInput(char *method, Matrix **m1, Matrix **m2, Matrix **m3)
{
    FILE *fptr = fopen("input.txt", "r");
    if (fptr == NULL)
    {
        printf("Failed to open input.txt\n");
        exit(EXIT_FAILURE);
    }

    char line[8192]; // Buffer for reading lines
    int matricesRead = 0;
    bool inside = false;

    // Temporary storage while reading - max matrix size is 100x100
    int (*tmp)[100][100] = malloc(2 * sizeof(*tmp));
    if (tmp == NULL)
    {
        printf("malloc failed for 'tmp'\n");
        exit(EXIT_FAILURE);
    }
    int rows[2] = {0, 0};
    int cols[2] = {0, 0};

    while (fgets(line, sizeof(line), fptr))
    {
        char *s = line;
        while (*s == ' ' || *s == '\t')
            s++;

        if (*s == '{')
        {
            inside = true;
            rows[matricesRead] = 0;
        }
        else if (*s == '}')
        {
            inside = false;
            matricesRead++;
            if (matricesRead == 2)
                break;
        }
        else if (inside && *s == '[')
        {
            int r = rows[matricesRead];
            if(r >= 100)
            {
                printf("Matrix row count exceeds 100. Please use benchmark mode for larger matrices.\n");
                exit(EXIT_FAILURE);
            }
            int c = 0;
            s++; // skip '['
            while (*s && *s != ']')
            {
                if (c >= 100)
                {
                    printf("Matrix column count exceeds 100. Please use benchmark mode for larger matrices.\n");
                    exit(EXIT_FAILURE);
                }
                if ((*s >= '0' && *s <= '9') || *s == '-')
                    tmp[matricesRead][r][c++] = (int)strtol(s, &s, 10);
                else
                    s++;
            }
            if (cols[matricesRead] == 0)
                cols[matricesRead] = c;
            rows[matricesRead]++;
        }
    }
    fclose(fptr);

    (*m1) = malloc(sizeof(Matrix));
    (*m2) = malloc(sizeof(Matrix));
    (*m3) = malloc(sizeof(Matrix));
    if ((*m1) == NULL || (*m2) == NULL || (*m3) == NULL)
    {
        printf("malloc failed for matrices\n");
        exit(EXIT_FAILURE);
    }

    // Set dimensions for m1 and m2
    (*m1)->rows = rows[0];
    (*m1)->cols = cols[0];
    (*m2)->rows = rows[1];
    (*m2)->cols = cols[1];

    // Since not using mmap, set these to NULL/0 for freeMatrix
    (*m1)->map_start = NULL;
    (*m1)->map_size = 0;
    (*m2)->map_start = NULL;
    (*m2)->map_size = 0;

    // Make sure sizes are correct
    checkMatrixSize(method, *m1, *m2);

    // Allocate and fill m1
    (*m1)->data = malloc((*m1)->rows * (*m1)->cols * sizeof(int));
    if ((*m1)->data == NULL)
    {
        printf("malloc failed for 'm1'\n");
        exit(EXIT_FAILURE);
    }
    for (int i = 0; i < (*m1)->rows; i++)
        for (int j = 0; j < (*m1)->cols; j++)
            (*m1)->data[i * (*m1)->cols + j] = tmp[0][i][j];

    // Allocate and fill m2
    (*m2)->data = malloc((*m2)->rows * (*m2)->cols * sizeof(int));
    if ((*m2)->data == NULL)
    {
        printf("malloc failed for 'm2'\n");
        exit(EXIT_FAILURE);
    }
    for (int i = 0; i < (*m2)->rows; i++)
        for (int j = 0; j < (*m2)->cols; j++)
            (*m2)->data[i * (*m2)->cols + j] = tmp[1][i][j];

    // Allocate m3 (result matrix)
    (*m3)->rows = (*m1)->rows;
    (*m3)->cols = (strcmp(method, "mult") == 0) ? (*m2)->cols : (*m1)->cols;
    (*m3)->map_start = NULL; 
    (*m3)->map_size = 0;

    (*m3)->data = malloc((*m3)->rows * (*m3)->cols * sizeof(int));
    if ((*m3)->data == NULL)
    {
        printf("malloc failed for 'm3'\n");
        exit(EXIT_FAILURE);
    }
    free(tmp);
}

/*
* Reads matrix from binary file. The binary file format is:
- 4 bytes: int for number of rows (m)
- 4 bytes: int for number of cols (n)
- m*n*4 bytes: int data in row-major order
*/
Matrix *readMatrixFromBinary(const char *filename) {
    // Open file for reading
    int fd = open(filename, O_RDONLY);
    if (fd == -1){
        printf("Failed to open %s\n", filename);
        exit(EXIT_FAILURE);
    }

    // Get file size
    struct stat sb;
    fstat(fd, &sb);

    // Map the entire file into memory
    // PROT_READ = read-only, MAP_PRIVATE = don't share changes
    void *mapped = mmap(NULL, sb.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
    close(fd);
    if (mapped == MAP_FAILED){
        printf("Failed to map %s\n", filename);
        exit(EXIT_FAILURE);
    }

    int *ptr = (int *)mapped;
    Matrix *m = malloc(sizeof(Matrix));
    
    // Extract header info (first two ints)
    m->rows = ptr[0];
    m->cols = ptr[1];
    
    // Point data to the rest of the file (offset by 2 ints)
    m->data = &ptr[2]; 

    // Save these for freeMatrix
    m->map_start = mapped; 
    m->map_size = sb.st_size;

    return m;
}

/*
* Uses normal I/O to write the result matrix to output.txt in the same format as input.txt.
*/
void writeToFile(Matrix *m3)
{
    FILE *fptr = fopen("output.txt", "w");
    if (fptr == NULL)
    {
        printf("Failed to open output.txt\n");
        exit(EXIT_FAILURE);
    }

    fprintf(fptr, "{\n");
    for (int i = 0; i < m3->rows; i++)
    {
        fprintf(fptr, "  [");
        for (int j = 0; j < m3->cols; j++)
        {
            fprintf(fptr, "%d", m3->data[i * m3->cols + j]);
            if (j < m3->cols - 1)
                fprintf(fptr, ", ");
        }
        fprintf(fptr, "]");
        if (i < m3->rows - 1)
            fprintf(fptr, ",");
        fprintf(fptr, "\n");
    }
    fprintf(fptr, "}\n");
    fclose(fptr);
}

/*
* Uses binary I/O to write the result matrix to output.bin.
*/
void writeBinaryToFile(Matrix *m3){
    FILE *fptr = fopen("output.bin", "wb");
    if (fptr == NULL) {
        perror("Failed to open binary output file");
        exit(EXIT_FAILURE);
    }

    // 1. Write the header (rows and cols)
    int header[2] = {m3->rows, m3->cols};
    fwrite(header, sizeof(int), 2, fptr);

    // 2. Write the entire data block in one go
    size_t total_elements = (size_t)m3->rows * m3->cols;
    size_t written = fwrite(m3->data, sizeof(int), total_elements, fptr);

    if (written != total_elements) {
        fprintf(stderr, "Error writing matrix data to binary file\n");
    }

    fclose(fptr);
}
/*
* Sets up matrices for benchmark mode. Reads from binary files and checks dimensions. Allocates m3.
*/
void setupBenchmark(char *method, Matrix **m1, Matrix **m2, Matrix **m3) 
{
    printf("Benchmark mode: Using binary I/O\n");

    *m1 = readMatrixFromBinary("m1.bin");
    *m2 = readMatrixFromBinary("m2.bin");

    // Check dimensions before allocating m3
    checkMatrixSize(method, *m1, *m2);

    *m3 = malloc(sizeof(Matrix));
    if (*m3 == NULL) {
        perror("malloc failed for 'm3'");
        exit(EXIT_FAILURE);
    }

    (*m3)->rows = (*m1)->rows;
    (*m3)->cols = (strcmp(method, "mult") == 0) ? (*m2)->cols : (*m1)->cols;
    
    // m3 is ALWAYS malloc'd because we need to write to it
    (*m3)->map_start = NULL; 
    (*m3)->map_size = 0;
    (*m3)->data = malloc((*m3)->rows * (*m3)->cols * sizeof(int));

    if ((*m3)->data == NULL) {
        perror("malloc failed for 'm3->data'");
        exit(EXIT_FAILURE);
    }
}

/*
* Frees a matrix. If the matrix was created using mmap, it will unmap the memory. 
Otherwise, it will free the malloc'd data.
*/
void freeMatrix(Matrix *m)
{
    if (m != NULL)
    {
        if (m->map_start != NULL) 
        {
            // This was a memory-mapped file
            munmap(m->map_start, m->map_size);
        } 
        else if (m->data != NULL) 
        {
            // This was standard malloc'd memory
            free(m->data);
        }
        
        free(m);
    }
}

int main(int argc, char **argv)
{
    if (argc != 3 && argc != 4)
    {
        printf("Usage: <Number of threads> <Operation (supports +, -, *)> <OPTIONAL: -benchmark>\n");
        return EXIT_FAILURE;
    }

    int tnum = atoi(argv[1]);
    if(tnum <= 0)
    {
        printf("Thread count must be a positive integer\n");
        return EXIT_FAILURE;
    }
    char operand = argv[2][0];
    char *method;
    void *(*fn)(void *);

    switch (operand)
    {
    case '+': method = "plus";  fn = plus;  break;
    case '-': method = "minus"; fn = minus; break;
    case '*': method = "mult";  fn = mult;  break;
    default:
        printf("Unknown operand '%c'\n", operand);
        return EXIT_FAILURE;
    }

    /*
    * if -benchmark tag is supplied we will use binary I/O
    * else we will read from input.txt and write to output.txt using normal I/O.
    */
    Matrix *m1, *m2, *m3;
    if (argc == 4 && strcmp(argv[3], "-benchmark") == 0){
        setupBenchmark(method, &m1, &m2, &m3);
    }
    else
    {
        readTextInput(method, &m1, &m2, &m3);
    }

    // Make array of threads
    pthread_t *threads = malloc(tnum * sizeof(pthread_t));
    if (threads == NULL)
    {
        printf("malloc failed for 'threads'\n");
        return EXIT_FAILURE;
    }

    // Make array of work structs
    ThreadArgs *threadWork = malloc(tnum * sizeof(ThreadArgs));
    if (threadWork == NULL)
    {
        printf("malloc failed for 'threadWork'\n");
        free(threads);
        return EXIT_FAILURE;
    } 

    /*
    * Spawn threads but first check if the users supplied thread count 
    * is not more than the number of rows in the result matrix. 
    * If it is, we can just use the number of rows as thread count to avoid
    */    
    if (tnum > m3->rows)
    {
        printf("Thread count %d is more than number of rows %d. Using %d threads instead.\n", tnum, m3->rows, m3->rows);
        tnum = m3->rows;
    }
    for (int i = 0; i < tnum; i++)
    {
        threadWork[i].id = i;
        threadWork[i].tnum = tnum;
        threadWork[i].m1 = m1;
        threadWork[i].m2 = m2;
        threadWork[i].m3 = m3;

        int code = pthread_create(&threads[i], NULL, fn, (void *)&threadWork[i]);
        if (code != 0) // Thread creation failed
        {
            // Join the threads that were successfully started
            for (int j = 0; j < i; j++) pthread_join(threads[j], NULL);
            
            // Cleanup everything
            freeMatrix(m1); freeMatrix(m2); freeMatrix(m3);
            free(threads); free(threadWork);
            return EXIT_FAILURE;
        }
    }

    // Join threads on return NULL since we don't care about return value
    for (int i = 0; i < tnum; i++){
        pthread_join(threads[i], NULL);
    }
    
    // Write result to file. If -benchmark tag is supplied, write in binary format. 
    // Else write in text format.
    if(argc == 4 && strcmp(argv[3], "-benchmark") == 0){
        writeBinaryToFile(m3);
    }
    else{
        writeToFile(m3);
    }

    printf("Done. Result written to output.txt\n");

    // Cleanup
    freeMatrix(m1); freeMatrix(m2); freeMatrix(m3);
    free(threads); free(threadWork);

    return EXIT_SUCCESS;
}
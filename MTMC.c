#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

/*
 * Global variables
 *
 * Using indexing [i * cols + j];
 * Result will be in m3;
 */
int *m1;
int *m2;
int *m3;
int m1_rows, m1_cols;
int m2_rows, m2_cols;
int m3_rows, m3_cols;

/*
 * Struct for work
 */
typedef struct work work;
struct work
{
    int id;
    int tnum;
};

void *getWork(void *arg, int *id, int *tnum)
{
    work *w = (work *)arg;
    if (w == NULL)
    {
        printf("Error! Null pointer!\n");
        exit(EXIT_FAILURE);
    }
    *id = w->id;
    *tnum = w->tnum;
}

/*
 *  Logic for minus is m3[i][j] = m1[i][j] - m2[i][j]
 *  Each thread will work on rows id, id + tnum, id + 2*tnum, ...
 */
void *minus(void *arg)
{
    int id, tnum;
    getWork(arg, &id, &tnum);

    for (int i = id; i < m3_rows; i += tnum) {
        for (int j = 0; j < m3_cols; j++) {
            m3[i * m3_cols + j] = m1[i * m3_cols + j] - m2[i * m3_cols + j];
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
    int id, tnum;
    getWork(arg, &id, &tnum);

    for (int i = id; i < m3_rows; i += tnum) {
        for (int j = 0; j < m3_cols; j++) {
            m3[i * m3_cols + j] = m1[i * m3_cols + j] + m2[i * m3_cols + j];
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
    int id, tnum;
    getWork(arg, &id, &tnum);

    for (int i = id; i < m3_rows; i += tnum) {
        for (int j = 0; j < m3_cols; j++) {
            int sum = 0;
            for (int k = 0; k < m1_cols; k++) {
                sum += m1[i * m1_cols + k] * m2[k * m2_cols + j];
            }
            m3[i * m3_cols + j] = sum;
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
void checkMatrixSize(char *method)
{
    if (strcmp(method, "plus") == 0 || strcmp(method, "minus") == 0)
    {
        if (m1_rows != m2_rows || m1_cols != m2_cols)
        {
            printf("Size mismatch for %s: (%dx%d) vs (%dx%d)\n",
                   method, m1_rows, m1_cols, m2_rows, m2_cols);
            exit(EXIT_FAILURE);
        }
    }
    else if (strcmp(method, "mult") == 0)
    {
        if (m1_cols != m2_rows)
        {
            printf("Size mismatch for mult: (%dx%d) * (%dx%d)\n",
                   m1_rows, m1_cols, m2_rows, m2_cols);
            exit(EXIT_FAILURE);
        }
    }
}

/*
 * Reads from input.txt, and calls checkMatrixSize()
 * Finally fills arrays. Max matrix size is 100x100, change buffer to handle bigger matrices.
 */
void readInput(char *method)
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

    // Temporary storage while reading - max matrix size is 1000x1000
    int (*tmp)[1000][1000] = malloc(2 * sizeof(*tmp));
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
            int c = 0;
            s++; // skip '['
            while (*s && *s != ']')
            {
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

    m1_rows = rows[0];
    m1_cols = cols[0];
    m2_rows = rows[1];
    m2_cols = cols[1];

    // Make sure sizes are correct
    checkMatrixSize(method);

    // Allocate and fill m1
    m1 = malloc(m1_rows * m1_cols * sizeof(int));
    if (m1 == NULL)
    {
        printf("malloc failed for 'm1'\n");
        exit(EXIT_FAILURE);
    }
    for (int i = 0; i < m1_rows; i++)
        for (int j = 0; j < m1_cols; j++)
            m1[i * m1_cols + j] = tmp[0][i][j];

    // Allocate and fill m2
    m2 = malloc(m2_rows * m2_cols * sizeof(int));
    if (m2 == NULL)
    {
        printf("malloc failed for 'm2'\n");
        exit(EXIT_FAILURE);
    }
    for (int i = 0; i < m2_rows; i++)
        for (int j = 0; j < m2_cols; j++)
            m2[i * m2_cols + j] = tmp[1][i][j];

    // Allocate m3 (result matrix)
    m3_rows = m1_rows;
    m3_cols = (strcmp(method, "mult") == 0) ? m2_cols : m1_cols;

    m3 = malloc(m3_rows * m3_cols * sizeof(int));
    if (m3 == NULL)
    {
        printf("malloc failed for 'm3'\n");
        exit(EXIT_FAILURE);
    }
    free(tmp);
}

void writeToFile()
{
    FILE *fptr = fopen("output.txt", "w");
    if (fptr == NULL)
    {
        printf("Failed to open output.txt\n");
        exit(EXIT_FAILURE);
    }

    fprintf(fptr, "{\n");
    for (int i = 0; i < m3_rows; i++)
    {
        fprintf(fptr, "  [");
        for (int j = 0; j < m3_cols; j++)
        {
            fprintf(fptr, "%d", m3[i * m3_cols + j]);
            if (j < m3_cols - 1)
                fprintf(fptr, ", ");
        }
        fprintf(fptr, "]");
        if (i < m3_rows - 1)
            fprintf(fptr, ",");
        fprintf(fptr, "\n");
    }
    fprintf(fptr, "}\n");
    fclose(fptr);
}

int main(int argc, char **argv)
{
    if (argc != 3)
    {
        printf("Usage: <Number of threads> <Operation (supports +, -, *)>\n");
        return EXIT_FAILURE;
    }

    int tnum = atoi(argv[1]);
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

    // Read from input.txt and fill m1, m2, m3
    readInput(method);

    // Make array of threads
    pthread_t *threads = malloc(tnum * sizeof(pthread_t));
    if (threads == NULL)
    {
        printf("malloc failed for 'threads'\n");
        return EXIT_FAILURE;
    }

    // Make array of work structs
    work *workItems = malloc(tnum * sizeof(work));
    if (workItems == NULL)
    {
        printf("malloc failed for 'workItems'\n");
        free(threads);
        return EXIT_FAILURE;
    } 

    // Spawn threads
    for (int i = 0; i < tnum; i++)
    {
        workItems[i].id = i;
        workItems[i].tnum = tnum;

        int code = pthread_create(&threads[i], NULL, fn, (void *)&workItems[i]);
        if (code != 0)
        {
            printf("pthread_create failed\n");
            return EXIT_FAILURE;
        }
    }

    // Join threads
    for (int i = 0; i < tnum; i++)
        pthread_join(threads[i], NULL);

    writeToFile();

    printf("Done. Results written to output.txt\n");

    // Cleanup
    free(threads);
    free(workItems);
    free(m1);
    free(m2);
    free(m3);

    return EXIT_SUCCESS;
}

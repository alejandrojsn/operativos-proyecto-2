#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdbool.h>
#include <pthread.h>
#include <sys/mman.h>

char *filename;
int n_procesos, n_generaciones, n_visualizacion, nfilas, ncol, *limite_inferior, *limite_superior, region_tabla, **tablero;

FILE *input_file;

pid_t *pids;

int **master_pipes, **down_pipes, **up_pipes;

pthread_mutex_t *mutex;
pthread_mutexattr_t mutexattr;

void master();
void process_worker(int i);
void load_region();
void create_pipes();
void close_unnecessary_pipes();

int main(int argc, char *argv[])
{
    if (argc < 5) {
        printf("Insufficient arguments");
        exit(1);
    }

    n_procesos = atoi(argv[1]);
    n_generaciones = atoi(argv[2]);
    n_visualizacion = atoi(argv[3]);
    
    filename = malloc(sizeof(argv[4]));

    strcpy(filename, argv[4]);

    master();

    return 0;
}

void master()
{
    input_file = fopen(filename, "r");

    if (input_file == NULL)
    {
        printf("Error: Could not open file");
        exit(1);
    }

    fscanf(input_file, "%d %d", &nfilas, &ncol);

    n_procesos = n_procesos < nfilas ? n_procesos : nfilas;

    limite_inferior = malloc(sizeof(int) * n_procesos);
    limite_superior = malloc(sizeof(int) * n_procesos);

    int filas_por_proceso = nfilas / n_procesos;
    int resto = nfilas % n_procesos;
    int contador = 0;

    for (int i = 0; i < n_procesos; i++) {
        limite_inferior[i] = contador;

        limite_superior[i] = contador += filas_por_proceso + (resto > 0);

        resto -= resto > 0;
    }

    create_pipes();

    pthread_mutexattr_init(&mutexattr);
        
    /* modify attribute */
    pthread_mutexattr_setpshared(&mutexattr, PTHREAD_PROCESS_SHARED);

    mutex = (pthread_mutex_t*) mmap(NULL, sizeof(pthread_mutex_t), PROT_READ|PROT_WRITE, MAP_SHARED|MAP_ANON, -1, 0);

    /* mutex Initialization */
    pthread_mutex_init(mutex, &mutexattr);

    pids = malloc(sizeof(pid_t) * n_procesos);

    for (int i = 0; i < n_procesos; i++) {
        pid_t pid = fork();

        if (pid == 0) {
            process_worker(i);
            return;
        }

        pids[i] = pid;
    }

    for (int i = 0; i < n_procesos; i++) {
        waitpid(pids[i], NULL, 0);
    }

    pthread_mutexattr_destroy(&mutexattr);
    pthread_mutex_destroy(mutex);

    munmap(mutex, sizeof(pthread_mutex_t));
}

void process_worker(int i)
{
    region_tabla = i;

    close_unnecessary_pipes();

    load_region();
}

void load_region()
{
    // move to this process' corresponding region
    for(int i = 0; i < limite_inferior[region_tabla] * ncol; i++) {
        int helper;
        fscanf(input_file, "%d", &helper);
    }

    int num_filas = limite_superior[region_tabla] - limite_inferior[region_tabla];

    // create matrix
    tablero = malloc(sizeof(int *) * num_filas);
    tablero[0] = malloc(sizeof(int) * num_filas * ncol);
    for (int i = 1; i < num_filas; i++) {
        tablero[i] = tablero[0] + i * ncol;
    }

    // load region
    for (int i = 0; i < num_filas; i++) {
        for (int j = 0; j < ncol; j++) {
            fscanf(input_file, "%d", &tablero[i][j]);
        }
    }

    fclose(input_file);
}

void create_pipes()
{
    down_pipes = malloc(sizeof(int *) * n_procesos);
    down_pipes[0] = malloc(sizeof(int) * 2);
    for (int i = 1; i < n_procesos; i++) {
       down_pipes[i] = down_pipes[0] + i * 2;
    }

    up_pipes = malloc(sizeof(int *) * n_procesos);
    up_pipes[0] = malloc(sizeof(int) * 2);
    for (int i = 1; i < n_procesos; i++) {
       up_pipes[i] = up_pipes[0] + i * 2;
    }

    master_pipes = malloc(sizeof(int *) * n_procesos);
    master_pipes[0] = malloc(sizeof(int) * 2);
    for (int i = 1; i < n_procesos; i++) {
       master_pipes[i] = master_pipes[0] + i * 2;
    }

    for(int i = 0; i < n_procesos; i++) {
        pipe(down_pipes[i]);
        pipe(up_pipes[i]);
        pipe(master_pipes[i]);
    }
}

void close_unnecessary_pipes()
{
    for (int i = 0; i < n_procesos; i++) {

        // close all read pipes, except the ones this process will use
        if (i != region_tabla) {
            close(down_pipes[i][0]);
            close(up_pipes[i][0]);
            close(master_pipes[i][0]);
            colse(master_pipes[i][1]);
        }

        if (i != region_tabla + 1) {
            close(down_pipes[i][0]);
        }

        if (i != region_tabla - 1) {
            close(up_pipes[i][0]);
        }
    }
}

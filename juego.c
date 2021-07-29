#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdbool.h>

char *filename;
int n_procesos, n_generaciones, n_visualizacion, nrows, ncols, *lower_bounds, *upper_bounds, id, **grid, N;

FILE *input_file;

int **master_pipes, **down_pipes, **up_pipes;

void master();
void process_worker(int i);
void load_region();
void create_pipes();
void close_unnecessary_pipes();
void write_to_neighbors();
void read_from_neighbors();
void simulate_game();
int count_neighbours(int i, int j);
void write_to_master();
void read_from_childs_and_print();

int main(int argc, char *argv[])
{
    // obtain arguments
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

    // read nrows and ncols (first two numbers on input file)
    fscanf(input_file, "%d %d", &nrows, &ncols);

    // we'll use min(n_procesos, nrows) processes. Since each process needs to manage at least 1 row it doesn't make sense to use more than nrows processes
    n_procesos = n_procesos < nrows ? n_procesos : nrows;

    // create arrays to store bounds for each process
    lower_bounds = malloc(sizeof(int) * n_procesos);
    upper_bounds = malloc(sizeof(int) * n_procesos);

    // calculate bounds for each process
    int rows_per_process = nrows / n_procesos;
    int mod = nrows % n_procesos;
    int c = 0;

    for (int i = 0; i < n_procesos; i++) {
        lower_bounds[i] = c;

        upper_bounds[i] = c += rows_per_process + (mod > 0);

        mod -= mod > 0;
    }

    // create pipes
    create_pipes();

    // create processes
    for (int i = 0; i < n_procesos; i++) {
        pid_t pid = fork();

        if (pid == 0) {
            process_worker(i);
            return;
        } else if(pid == -1) {
            printf("There was an error creating a child process");
            exit(1);
        }
    }

    // read first generation from childs
    printf("Generación 0\n");
    read_from_childs_and_print();

    // read all the other generations
    for(int i = 1; i <= n_generaciones / n_visualizacion; i++) {
        printf("Generación %d\n", i*n_visualizacion);
        read_from_childs_and_print();
    }

    // if n_generaciones was not exactly divisible by n_visualizaciones, this is to print the last generation
    if (n_generaciones % n_visualizacion != 0) {
        printf("Generación %d\n", n_generaciones);
        read_from_childs_and_print();
    }
}

void process_worker(int i)
{
    id = i;

    N = upper_bounds[id] - lower_bounds[id] + 2;

    // close the pipes that won't be used by this process
    close_unnecessary_pipes();

    // load the initial generation
    load_region();

    // write the initial generation to the parent process
    write_to_master();

    // main loop
    for(int i = 1; i <= n_generaciones; i++) {

        // send frontier rows to neighbors
        write_to_neighbors();

        // read frontier rows from neighbors
        read_from_neighbors();

        // apply the rules from the games of life
        simulate_game();

        // send the current generation to the parent process if it's time to do so
        if (i % n_visualizacion == 0) {
            write_to_master();
        }
    }

    // if n_generaciones was not exactly divisible by n_visualizaciones, this is to show the last generation
    if (n_generaciones % n_visualizacion != 0) {
        write_to_master();
    }

    // close pipes
    if (id > 0) {
        close(down_pipes[id][0]);
        close(down_pipes[id+1][1]);
    }

    if (id < n_procesos - 1) {
        close(up_pipes[id][0]);
        close(up_pipes[id-1][1]);
    }
}

void load_region()
{
    // move to this process' corresponding region
    for(int i = 0; i < lower_bounds[id] * ncols; i++) {
        int helper;
        fscanf(input_file, "%d", &helper);
    }

    // create matrix
    grid = malloc(sizeof(int *) * N);
    grid[0] = malloc(sizeof(int) * N * ncols);
    for (int i = 1; i < N; i++) {
        grid[i] = grid[0] + i * ncols;
    }

    // just in case. fill the matrix with 0s
    memset(grid[0], 0, sizeof(int) * N * ncols);

    // load region
    for (int i = 1; i < N - 1; i++) {
        for (int j = 0; j < ncols; j++) {
            fscanf(input_file, "%d", &grid[i][j]);
        }
    }

    fclose(input_file);
}

void create_pipes()
{
    // create array for down pipes
    down_pipes = malloc(sizeof(int *) * n_procesos);
    down_pipes[0] = malloc(sizeof(int) * 2 * n_procesos);
    for (int i = 1; i < n_procesos; i++) {
       down_pipes[i] = down_pipes[0] + i * 2;
    }

    // create array for up pipes
    up_pipes = malloc(sizeof(int *) * n_procesos);
    up_pipes[0] = malloc(sizeof(int) * 2 * n_procesos);
    for (int i = 1; i < n_procesos; i++) {
       up_pipes[i] = up_pipes[0] + i * 2;
    }

    // create array for master pipes
    master_pipes = malloc(sizeof(int *) * n_procesos);
    master_pipes[0] = malloc(sizeof(int) * 2 * n_procesos);
    for (int i = 1; i < n_procesos; i++) {
       master_pipes[i] = master_pipes[0] + i * 2;
    }

    // initialize pipes
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
        // also close all write master_pipes except the one this process will use
        if (i != id) {
            close(down_pipes[i][0]);
            close(up_pipes[i][0]);
            close(master_pipes[i][1]);
        }

        // close all read down_pipes, except the ones this process will use
        if (i != id + 1) {
            close(down_pipes[i][1]);
        }

        // close all read up_pipes, except the ones this process will use
        if (i != id - 1) {
            close(up_pipes[i][1]);
        }

        // close all read master_pipes
        close(master_pipes[i][0]);
    }

    // if id is 0 this process doesnt have any neighbor above
    if (id == 0) {
        close(down_pipes[id][0]);
    }

    // if id is n_procesos - 1 this process doesnt have any neighbor below
    if (id == n_procesos - 1) {
        close(up_pipes[id][0]);
    }
}

void write_to_neighbors()
{
    // write to the neighbor below, except if it's the last process
    if (id < n_procesos - 1) {
        write(down_pipes[id + 1][1], grid[N - 2], sizeof(int) * ncols);
    }

    // write to the neighbor above, except if it's the first process
    if (id > 0) {
        write(up_pipes[id - 1][1], grid[1], sizeof(int) * ncols);
    }
}

void read_from_neighbors()
{
    // read from the neighbor above, except if it's the first process
    if (id > 0) {
        read(down_pipes[id][0], grid[0], sizeof(int) * ncols);
    }

    // read from the neighbor below, except if it's the last process
    if (id < n_procesos - 1) {
        read(up_pipes[id][0], grid[N-1], sizeof(int) * ncols);
    }
}

void simulate_game()
{
    // create matrix for storing neighbours count
    int **neighbours = malloc(sizeof(int *) * N);
    neighbours[0] = malloc(sizeof(int) * N * ncols);
    for (int i = 1; i < N; i++) {
        neighbours[i] = neighbours[0] + i * ncols;
    }

    // count the neighbors of every cell
    for (int i = 1; i < N-1; i++) {
        for (int j = 0; j < ncols; j++) {
            neighbours[i][j] = count_neighbours(i, j);
        }
    }

    // change the cell alive/dead status based on neighbours count following the rules of the game of life
    for (int i = 1; i < N-1; i++) {
        for(int j = 0; j < ncols; j++) {
            if (!grid[i][j] && neighbours[i][j] == 3) {
                grid[i][j] = 1;
            } else if (grid[i][j] && (neighbours[i][j] != 2 && neighbours[i][j] != 3)) {
                grid[i][j] = 0;
            }
        }
    }

    // free memory from temporary matrix
    free(neighbours[0]);
    free(neighbours);
}

int count_neighbours(int i, int j)
{
    // since this process manages only the middle N - 2 rows, every cell has neighbors directly above or below
    int neighbours_count = grid[i-1][j] + grid[i+1][j];

    // count neighbors to the left if there are
    if (j > 0) {
        neighbours_count += grid[i-1][j-1] + grid[i][j-1] + grid[i + 1][j-1];
    }

    // count neighbors to the right if there are
    if (j < ncols - 1) {
        neighbours_count += grid[i-1][j+1] + grid[i][j+1] + grid[i + 1][j+1];
    }

    return neighbours_count;
}

void write_to_master()
{
    // write to parent only this process' managed rows
    write(master_pipes[id][1], grid[1], sizeof(int) * (N - 2) * ncols);
}

void read_from_childs_and_print()
{
    for(int i = 0; i < n_procesos; i++) {
        // size in rows of the current child process region
        int N = upper_bounds[i] - lower_bounds[i];
        
        // create temp matrix to store info from child
        int **temp = malloc(sizeof(int *) * N);
        temp[0] = malloc(sizeof(int) * N * ncols);
        for (int j = 1; j < N; j++) {
            temp[j] = temp[0] + j * ncols;
        }

        // read from child process i
        read(master_pipes[i][0], temp[0], sizeof(int) * N * ncols);

        // print region
        for(int j = 0; j < N; j++) {
            for (int k = 0; k < ncols; k++) {
                printf("%d ", temp[j][k]);
            }

            printf("\n");
        }
        
        // free memory from temp variable
        free(temp[0]);
        free(temp);
    }
}

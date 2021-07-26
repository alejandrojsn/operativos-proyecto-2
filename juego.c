#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdbool.h>

char *filename;
int n_procesos, n_generaciones, n_visualizacion, nfilas, ncol;

void master();

int main(int argc, char *argv[])
{
    if(argc < 5) {
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
    FILE *input_file = fopen(filename, "r");

    if (input_file == NULL)
    {
        printf("Error: Could not open file");
        exit(1);
    }

    fscanf(input_file, "%d %d", &nfilas, &ncol);

    fclose(input_file);
}
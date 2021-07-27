#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdbool.h>

char *filename;
int n_procesos, n_generaciones, n_visualizacion, nfilas, ncol, *limite_inferior, *limite_superior;

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

    n_procesos = n_procesos < nfilas ? n_procesos : nfilas;

    limite_inferior = malloc(sizeof(int) * n_procesos);
    limite_superior = malloc(sizeof(int) * n_procesos);

    int filas_por_proceso = nfilas / n_procesos;
    int resto = nfilas % n_procesos;
    int contador = 0;

    for(int i = 0; i < n_procesos; i++) {
        limite_inferior[i] = contador;

        limite_superior[i] = contador += filas_por_proceso + (resto > 0);

        resto -= resto > 0;
    }

    for(int i = 0; i < n_procesos; i++) {
        printf("[%d,", limite_inferior[i]);
        printf("%d)\n", limite_superior[i]);
    }

    fclose(input_file);
}
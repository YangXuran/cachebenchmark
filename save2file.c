#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static FILE *file;

void create_file(char *filename, char *title, char *xlabels, char *ylabels)
{
	file = fopen(filename, "w");
	if (file == NULL) {
		printf("Error: cannot create file %s\n", filename);
		exit(1);
	}
	fprintf(file, "%s,%s,%s\n", title, xlabels, ylabels);
}

void save_label(int xlabel_s, const char xlabel[][xlabel_s], int xc, const char *line_titles[],
		int xl)
{
	for (int i = 0; i < xl; i++) {
		fprintf(file, "%s", line_titles[i]);
                if (i < xl-1)
                        fprintf(file, ",");
        }
	fprintf(file, "\n");
	for (int i = 0; i < xc; i++) {
		fprintf(file, "%s", xlabel[i]);
                if (i < xc-1)
                        fprintf(file, ",");
        }
	fprintf(file, "\n");
}

void save_data(double x[], int c)
{
	for (int i = 0; i < c; i++) {
		fprintf(file, "%f", x[i]);
                if (i < c-1)
                        fprintf(file, ",");
        }
	fprintf(file, "\n");
}

void close_file()
{
	fclose(file);
}

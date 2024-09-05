/*
 * Copyright (C) 2024 Xuran Yang
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>

static FILE *gnuplotPipe;

void create_plot(const char *filename, const char *title, const char *xlabel, const char *ylabel)
{
	gnuplotPipe = popen("gnuplot -persistent", "w");
	if (gnuplotPipe == NULL) {
		fprintf(stderr, "Error: Could not open pipe to gnuplot.\n");
		return;
	}

	fprintf(gnuplotPipe, "set terminal svg enhanced font 'Arial,10' size 1000,700\n");
	fprintf(gnuplotPipe, "set output '%s'\n", filename);
	fprintf(gnuplotPipe, "set title '%s'\n", title);
	fprintf(gnuplotPipe, "set xlabel '%s'\n", xlabel);
	fprintf(gnuplotPipe, "set ylabel '%s'\n", ylabel);
	fprintf(gnuplotPipe, "set autoscale xy\n");
	fprintf(gnuplotPipe, "set xtics rotate by -45\n");
	fprintf(gnuplotPipe, "set ytics auto 5000\n");
}

void set_label(int xlabel_s, const char xlabel[][xlabel_s], int xc, const char *line_titles[],
	       int xl)
{
	fprintf(gnuplotPipe, "set xtics (");
	for (int i = 0; i < xc; i++) {
		fprintf(gnuplotPipe, "'%s' %d", xlabel[i], i + 1);
		if (i < xc - 1)
			fprintf(gnuplotPipe, ", ");
	}
	fprintf(gnuplotPipe, ")\n");

	fprintf(gnuplotPipe, "plot ");
	for (int i = 0; i < xl; i++) {
		fprintf(gnuplotPipe, "'-' using 1:2 with linespoints title '%s'", line_titles[i]);
		if (i < xl - 1)
			fprintf(gnuplotPipe, ", ");
	}
	fprintf(gnuplotPipe, "\n");
}

void write_data(double x[], int c)
{
	for (int i = 0; i < c; i++)
		fprintf(gnuplotPipe, "%d %lf\n", i + 1, x[i]);

	fprintf(gnuplotPipe, "e\n");
}

void draw_plot()
{
	fflush(gnuplotPipe);
	pclose(gnuplotPipe);
}

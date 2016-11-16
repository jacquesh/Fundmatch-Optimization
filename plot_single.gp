set terminal pngcairo size 1024,1024 enhanced font "Verdana, 20"
set output "fitness_single.png"

set macros
load "gnuplot-palettes/set1.pal"

#set yrange [0:*]
set xrange [0:*]
set xlabel "Iteration"
set ylabel "Cost"

set macro
heuristic_fitness = "`cat heuristic_fitness.dat`"+0
manual_fitness = "`cat manual_fitness.dat`"+0

set key at graph 1,0.99
plot 'pso_fitness.dat' with lines ls 2 lw 4 title "Particle Swarm", \
     'ga_fitness.dat' with lines ls 3 lw 4 title "Genetic Algorithm", \
     heuristic_fitness with lines ls 9 lw 4 title "Heuristic"

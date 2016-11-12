set terminal pngcairo size 1024,1024 enhanced font "Verdana, 20"
set output "fitness_single_dataset.png"

#set yrange [0:*]
set xrange [0:*]
set xlabel "Iteration"
set ylabel "Fitness"

set style line 1 lc rgb '#0060ad' lt 1 lw 4 pt 7 ps 0.5
set style line 2 lc rgb '#60ad00' lt 1 lw 4 pt 7 ps 0.5
set style line 3 lc rgb '#ad0060' lt 1 lw 4 pt 7 ps 0.5

set macro
heuristic_fitness = "`cat heuristic_fitness.dat`"+0

plot 'ga_fitness.dat' with lines ls 1 title "Genetic Algorithm", \
     'pso_fitness.dat' with lines ls 2 title "Particle Swarm", \
      heuristic_fitness with lines ls 3 title "Heuristic"

pause 5

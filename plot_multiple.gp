set terminal pngcairo size 1024,1024 enhanced font "Verdana,20"

set macros
load "gnuplot-palettes/set1.pal"

set yrange [0:*]
set style data histogram
set style histogram cluster gap 1
set style fill solid border rgb "black"
set grid ytics
set key left top

set output "fitness_multiple_dataset.png"
set title "Average solution fitness per method"
plot "fitness_comparison.dat" using 2:xtic(1) title col ls 1, \
                           '' using 3:xtic(1) title col ls 2, \
                           '' using 4:xtic(1) title col ls 3

set output "runtime_multiple_dataset.png"
set title "Average runtime per method"
plot "runtime_comparison.dat" using 2:xtic(1) title col ls 1, \
                           '' using 3:xtic(1) title col ls 2, \
                           '' using 4:xtic(1) title col ls 3

set output "alloc_count_multiple_dataset.png"
set title "Average # of allocations per method"
plot "alloc_count_comparison.dat" using 2:xtic(1) title col ls 1, \
                               '' using 3:xtic(1) title col ls 2, \
                               '' using 4:xtic(1) title col ls 3

pause 0

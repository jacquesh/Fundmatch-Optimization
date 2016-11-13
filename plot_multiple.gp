set terminal pngcairo size 1024,1024 enhanced font "Verdana,20"

set macros
load "gnuplot-palettes/set1.pal"

set yrange [0:*]
set style data histogram
set style histogram errorbars gap 1 lw 2
set style fill solid border rgb "black"
set grid ytics
set key left top
set errorbars 1.0

set output "fitness_multiple_dataset.png"
set title "Average solution fitness per method"
plot "fitness_comparison.dat" using 2:3:4:xtic(1) title col ls 1, \
                           '' using 5:6:7:xtic(1) title col ls 2, \
                           '' using 8:9:10:xtic(1) title col ls 3

set output "runtime_multiple_dataset.png"
set title "Average runtime per method"
plot "runtime_comparison.dat" using 2:3:4:xtic(1) title col ls 1, \
                           '' using 5:6:7:xtic(1) title col ls 2, \
                           '' using 8:9:10:xtic(1) title col ls 3

set output "alloc_count_multiple_dataset.png"
set title "Average # of allocations per method"
plot "alloc_count_comparison.dat" using 2:3:4:xtic(1) title col ls 1, \
                               '' using 5:6:7:xtic(1) title col ls 2, \
                               '' using 8:9:10:xtic(1) title col ls 3

pause 0

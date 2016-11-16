set terminal pngcairo size 1024,1024 enhanced font "Verdana,20"

set macros
load "gnuplot-palettes/set1.pal"

set yrange [0:*]
set style data histogram
set style histogram errorbars gap 1 lw 2
set style fill solid border rgb "black"
set grid ytics
set key box opaque
set notitle
set errorbars 1.0

set output "fitness_multiple_dataset.png"
set xlabel "Dataset"
set ylabel "Normalized Solution Cost"
set key at graph 0.99,0.115
plot "fitness_compa_full.dat" using 2:3:4:xtic(1) title col ls 2, \
                           '' using 5:6:7:xtic(1) title col ls 3, \
                           '' using 8:9:10:xtic(1) title col ls 9

set output "runtime_multiple_dataset.png"
set ylabel "Computation run time (seconds)"
set key at graph 0.33,0.99
plot "runtime_compa_full.dat" using 2:3:4:xtic(1) title col ls 2, \
                           '' using 5:6:7:xtic(1) title col ls 3, \
                           '' using 8:9:10:xtic(1) title col ls 9

set output "alloc_count_multiple_dataset.png"
set ylabel "Allocations per requirement"
set key at graph 0.99,0.99
plot "alloc_count_compa_full.dat" using 2:3:4:xtic(1) title col ls 2, \
                               '' using 5:6:7:xtic(1) title col ls 3, \
                               '' using 8:9:10:xtic(1) title col ls 9

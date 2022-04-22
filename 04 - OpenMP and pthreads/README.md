# OpenMP and Pthreads

## Before

After compiling with `make` and running with `./main` without any changes (4 threads):

```.
Manual              Time:   3.339781e+00s
OpenMP:   CORRECT   Time:   3.326527e+00s   Speedup: 1.00x
Pthreads: INCORRECT Time:   0.000000e+00s   Speedup: infx
BLAS:     CORRECT   Time:   1.848006e-02s   Speedup: 180.72x
```

## After

After the changes i get the following results;

```.
Manual              Time:   3.171166e+00s
OpenMP:   CORRECT   Time:   6.127269e-01s   Speedup: 5.18x
Pthreads: CORRECT   Time:   6.077590e-01s   Speedup: 5.22x
BLAS:     CORRECT   Time:   1.778388e-02s   Speedup: 178.32x
```

## Testing

Running with different amounts of threads:

```-
Number of Threads:  1
Manual              Time:   3.168655e+00s
OpenMP:   CORRECT   Time:   3.173314e+00s   Speedup: 1.00x
Pthreads: CORRECT   Time:   3.088024e+00s   Speedup: 1.03x
BLAS:     CORRECT   Time:   1.856589e-02s   Speedup: 170.67x
```

```-
Number of Threads:  2
Manual              Time:   2.366420e+00s
OpenMP:   CORRECT   Time:   1.036895e+00s   Speedup: 2.28x
Pthreads: CORRECT   Time:   1.056572e+00s   Speedup: 2.24x
BLAS:     CORRECT   Time:   2.352786e-02s   Speedup: 100.58x
```

```-
Number of Threads:  7
Manual              Time:   3.163345e+00s
OpenMP:   CORRECT   Time:   5.973401e-01s   Speedup: 5.30x
Pthreads: CORRECT   Time:   5.918820e-01s   Speedup: 5.34x
BLAS:     CORRECT   Time:   1.214910e-02s   Speedup: 260.38x
```

```-
Number of Threads:  15
Manual              Time:   3.100396e+00s
OpenMP:   CORRECT   Time:   5.870221e-01s   Speedup: 5.28x
Pthreads: CORRECT   Time:   5.812960e-01s   Speedup: 5.33x
BLAS:     CORRECT   Time:   1.038218e-02s   Speedup: 298.63x
```
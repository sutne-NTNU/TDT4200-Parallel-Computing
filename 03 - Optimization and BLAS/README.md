# Optimization and BLAS

## Commands

Compile the code with:
```.
make
```

Time the genann implementation for example 4:
```.
time ./example4
```

Create callgrind report:
```.
valgrind --tool=callgrind --callgrind-out-file=reports/example4-prof-X ./example4
```

Open callgrind report in qcachegrind
```.
qcachegrind reports/example4-prof-X
```

## Part 1

### Initial Results

#### Time

```.
GENANN example 4.
Train an ANN on the IRIS dataset using backpropagation.
Loading 150 data points from example/iris.data
Training for 1000 loops over data.
147/150 correct (98.0%).

real    0m1.392s
user    0m1.390s
sys     0m0.000s
```

#### Callgrind report

Saved in **reports/example4-prof-1**

We see that the two functions with the highest execution time is:

- 65.34% `genann_train()`
- 28.79% `genann_run()`

> These two functions are responsible for **94.22%**

In the code the two lines with the highest execution time are marked with

```c
// HOTSPOT Part 1 Before
```

### Results after using BLAS

#### Time after

```.
GENANN example 4.
Train an ANN on the IRIS dataset using backpropagation.
Loading 150 data points from example/iris.data
Training for 1000 loops over data.
147/150 correct (98.0%).

real    0m0.975s
user    0m1.053s
sys     0m0.305s
```

This is a relative speedup of:

- **0.417** seconds - **30,0%**

#### Callgrind report after

Saved in **reports/example4-prof-2**

The two functions with the highest execution times are now:

- 80.99% `genann_train()`
- 6.57% `genann_run()`

> These two functions are responsible for **87.56%**

In the code the two lines with the highest execution time are marked with

```c
// HOTSPOT Part 1 After
```

## Part 2

### Time after changes
```.
GENANN example 4.
Train an ANN on the IRIS dataset using backpropagation.
Loading 150 data points from example/iris.data
Training for 1000 loops over data.
147/150 correct (98.0%).

real    0m0.608s
user    0m0.663s
sys     0m0.327s
```

This is a relative speedup (compared to the original) of:

- **0.784** seconds - **56,3%**

### callgrind report after changes

Saved in **reports/example4-prof-3**

The two functions with the highest execution times are now:

- 56.82% `genann_train()`
- 10.54% `genann_run()`

> These two functions are responsible for **67.36%**

In the code the two lines with the highest execution time are marked with

```c
// HOTSPOT Part 2 After
```

#### [Leaderboard](http://selbu.idi.ntnu.no:8080/)
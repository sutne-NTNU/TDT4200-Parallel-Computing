# CUDA Intro - Image Scaling

## Serial Version (`main_serial.cu`)

Compile using:

```-
make main-serial
```

To perform scaling by a factor of `2 * width` and `5 * height` use:

```-
./main-serial input.jpg 2 5
```

## CUDA Version (`main_solution.cu`)

Compile using:

```-
make main
```

To perform scaling by a factor of `2 * width` and `5 * height` use:

```-
./main input.jpg 2 5
```
## Execution Time

### Serial Implementation

```-
Image dimensions: 3840x2160
Time spent 4.030 seconds
```

### CUDA Implementation

```-
Image dimensions: 3840x2160
Time spent 0.074 seconds
Time spent including transfer: 0.275 seconds
```

## Input

<p align="center">
    <img src=input.jpg width=512>
</p>

## Output
<p align="center">
    <img src=output.png width=1024>
</p>
#include "genann.h"

#include <assert.h>
#include <errno.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <cblas.h>

#ifndef genann_act
#define genann_act_hidden genann_act_hidden_indirect
#define genann_act_output genann_act_output_indirect
#else
#define genann_act_hidden genann_act
#define genann_act_output genann_act
#endif

#define LOOKUP_SIZE 4096

double genann_act_hidden_indirect(const struct genann *ann, double a)
{
    return ann->activation_hidden(ann, a);
}

double genann_act_output_indirect(const struct genann *ann, double a)
{
    return ann->activation_output(ann, a);
}

const double sigmoid_dom_min = -15.0;
const double sigmoid_dom_max = 15.0;
double interval;
double lookup[LOOKUP_SIZE];

#ifdef __GNUC__
#define likely(x) __builtin_expect(!!(x), 1)
#define unlikely(x) __builtin_expect(!!(x), 0)
#define unused __attribute__((unused))
#else
#define likely(x) x
#define unlikely(x) x
#define unused
#pragma warning(disable : 4996) /* For fscanf */
#endif

double genann_act_sigmoid(const genann *ann unused, double a)
{
    if (a < -45.0)
        return 0;
    if (a > 45.0)
        return 1;
    return 1.0 / (1 + exp(-a));
}

void genann_init_sigmoid_lookup(const genann *ann)
{
    const double f = (sigmoid_dom_max - sigmoid_dom_min) / LOOKUP_SIZE;
    int i;

    interval = LOOKUP_SIZE / (sigmoid_dom_max - sigmoid_dom_min);
    for (i = 0; i < LOOKUP_SIZE; ++i)
    {
        lookup[i] = genann_act_sigmoid(ann, sigmoid_dom_min + f * i);
    }
}

double genann_act_sigmoid_cached(const genann *ann unused, double a)
{
    assert(!isnan(a));

    if (a < sigmoid_dom_min)
        return lookup[0];
    if (a >= sigmoid_dom_max)
        return lookup[LOOKUP_SIZE - 1];

    size_t j = (size_t)((a - sigmoid_dom_min) * interval + 0.5);

    /* Because doubleing point... */
    if (unlikely(j >= LOOKUP_SIZE))
        return lookup[LOOKUP_SIZE - 1];

    return lookup[j];
}

double genann_act_linear(const struct genann *ann unused, double a)
{
    return a;
}

double genann_act_threshold(const struct genann *ann unused, double a)
{
    return a > 0;
}

genann *genann_init(int inputs, int hidden_layers, int hidden, int outputs)
{
    if (hidden_layers < 0)
        return 0;
    if (inputs < 1)
        return 0;
    if (outputs < 1)
        return 0;
    if (hidden_layers > 0 && hidden < 1)
        return 0;

    const int hidden_weights = hidden_layers ? (inputs + 1) * hidden + (hidden_layers - 1) * (hidden + 1) * hidden : 0;
    const int output_weights = (hidden_layers ? (hidden + 1) : (inputs + 1)) * outputs;
    const int total_weights = (hidden_weights + output_weights);

    const int total_neurons = (inputs + hidden * hidden_layers + outputs);

    /* Allocate extra size for weights, outputs, and deltas. */
    const int size = sizeof(genann) + sizeof(double) * (total_weights + total_neurons + (total_neurons - inputs));
    genann *ret = malloc(size);
    if (!ret)
        return 0;

    ret->inputs = inputs;
    ret->hidden_layers = hidden_layers;
    ret->hidden = hidden;
    ret->outputs = outputs;

    ret->total_weights = total_weights;
    ret->total_neurons = total_neurons;

    /* Set pointers. */
    ret->weight = (double *)((char *)ret + sizeof(genann));
    ret->output = ret->weight + ret->total_weights;
    ret->delta = ret->output + ret->total_neurons;

    genann_randomize(ret);

    ret->activation_hidden = genann_act_sigmoid_cached;
    ret->activation_output = genann_act_sigmoid_cached;

    genann_init_sigmoid_lookup(ret);

    return ret;
}

genann *genann_read(FILE *in)
{
    int inputs, hidden_layers, hidden, outputs;
    int rc;

    errno = 0;
    rc = fscanf(in, "%d %d %d %d", &inputs, &hidden_layers, &hidden, &outputs);
    if (rc < 4 || errno != 0)
    {
        perror("fscanf");
        return NULL;
    }

    genann *ann = genann_init(inputs, hidden_layers, hidden, outputs);

    int i;
    for (i = 0; i < ann->total_weights; ++i)
    {
        errno = 0;
        rc = fscanf(in, " %le", ann->weight + i);
        if (rc < 1 || errno != 0)
        {
            perror("fscanf");
            genann_free(ann);

            return NULL;
        }
    }

    return ann;
}

genann *genann_copy(genann const *ann)
{
    const int size = sizeof(genann) + sizeof(double) * (ann->total_weights + ann->total_neurons + (ann->total_neurons - ann->inputs));
    genann *ret = malloc(size);
    if (!ret)
        return 0;

    memcpy(ret, ann, size);

    /* Set pointers. */
    ret->weight = (double *)((char *)ret + sizeof(genann));
    ret->output = ret->weight + ret->total_weights;
    ret->delta = ret->output + ret->total_neurons;

    return ret;
}

void genann_randomize(genann *ann)
{
    int i;
    for (i = 0; i < ann->total_weights; ++i)
    {
        double r = GENANN_RANDOM();
        /* Sets weights from -0.5 to 0.5. */
        ann->weight[i] = r - 0.5;
    }
}

void genann_free(genann *ann)
{
    /* The weight, output, and delta pointers go to the same buffer. */
    free(ann);
}

/**
 * Double GEneral Matrix Vector multiplication. 
 * 
 * Performs Y = (alpha * A * X) + (beta * Y)
 * 
 * @param order Specifies row-major (C) or column-major (Fortran) data ordering.
 * @param trans Specifies whether to transpose matrix A.
 * @param m     Number of rows in matrix A.
 * @param n     Number of columns in matrix A.
 * @param alpha Scaling factor for the product of matrix A and vector X.
 * @param A     Matrix A.
 * @param lda   The size of the first dimention of matrix A;
 * @param X     Vector X.
 * @param incx  Stride within X. For example, if incX is 7, every 7th element is used.
 * @param beta  Scaling factor for vector Y.
 * @param Y     Vector Y
 * @param incy  Stride within Y. For example, if incY is 7, every 7th element is used.
 * 
 * @return The output is saved in vector Y
 */
void GEMV(CBLAS_ORDER order, CBLAS_TRANSPOSE trans, int m, int n, double alpha, const double *A, int lda, const double *X, int incx, double beta, double *Y, int incy)
{
    cblas_dgemv(order, trans, m, n, alpha, A, lda, X, incx, beta, Y, incy);
}

double const *genann_run(genann const *ann, double const *inputs)
{
    //Copy the weights into a more convenient variable
    double const *w = ann->weight;
    //Initialize the output vector to point at the second layer of neurons
    double *o = ann->output + ann->inputs;
    //Initialize the input vector to point at the first layer of neurons
    double const *i = ann->output;

    /* Copy the inputs to the scratch area, where we also store each neuron's
     * output, for consistency. This way the first layer isn't a special case. */
    memcpy(ann->output, inputs, sizeof(double) * ann->inputs);

    int h, j, k;

    if (!ann->hidden_layers)
    {
        double *ret = o;
        for (j = 0; j < ann->outputs; ++j)
        {
            double sum = *w++ * -1.0;
            for (k = 0; k < ann->inputs; ++k)
            {
                sum += *w++ * i[k];
            }
            *o++ = genann_act_output(ann, sum);
        }

        return ret;
    }

    /* Figure input layer */
    for (j = 0; j < ann->hidden; ++j)
    {
        double sum = *w++ * -1.0;
        for (k = 0; k < ann->inputs; ++k)
        {
            sum += *w++ * i[k];
        }
        *o++ = genann_act_hidden(ann, sum);
    }

    i += ann->inputs;

    //These are the dimensions of the square weight matrix
    int m = ann->hidden;
    int n = ann->hidden;

    //In addition to n edge weights, each neuron has one value (bias) associated with it.
    //This value is *also* saved in w, meaning the complete matrix has (n+1)*m elements.
    //This value is always multiplied by -1, which we make room for in a copy of the input vector.
    double *temp_i = malloc((ann->hidden + 1) * sizeof(double));
    double *sums = calloc((ann->hidden + 1), sizeof(double));

    /////////////////////////////////
    // TODO: 1 GEMV using BLAS     //
    /////////////////////////////////

    for (h = 1; h < ann->hidden_layers; ++h)
    {
        //Copyyng the input vector and setting the first value to -1 as described above.
        temp_i[0] = -1.0;
        memcpy(temp_i + 1, i, n * sizeof(double));

        /*/////////////////////////////////////////////////////////////
        // Decompose and replace this double for loop with GEMV call //
        ///////////////////////////////////////////////////////////////
        for (j = 0; j < ann->hidden; ++j) {
            for (k = 0; k < ann->hidden+1; ++k) {
                sums[j] += w[k + j*(ann->hidden+1)] * temp_i[k]; // HOTSPOT Part 1 Before - 18,36%
            }
            o[j] = genann_act_hidden(ann, sums[j]);
        }
        ////////////////////////////////////////////////////////////*/

        // After Part 1 : 4,27%
        GEMV(CblasRowMajor, CblasNoTrans, ann->hidden, ann->hidden + 1, 1.0, w, n + 1, temp_i, 1, 0.0, sums, 1);

        for (j = 0; j < ann->hidden; j++)
        {
            o[j] = genann_act_hidden(ann, sums[j]);
        }

        w += (n + 1) * m;
        o += m;
        i += m;
    }
    free(temp_i);
    free(sums);

    /////////////////////////////////
    // TODO 1 END                  //
    /////////////////////////////////

    double const *ret = o;
    /* Figure output layer. */
    for (j = 0; j < ann->outputs; ++j)
    {
        double sum = *w++ * -1.0;
        for (k = 0; k < ann->hidden; ++k)
        {
            sum += *w++ * i[k];
        }
        *o++ = genann_act_output(ann, sum);
    }

    /* Sanity check that we used all weights and wrote all outputs. */
    assert(w - ann->weight == ann->total_weights);
    assert(o - ann->output == ann->total_neurons);

    return ret;
}

void genann_train(genann const *ann, double const *inputs, double const *desired_outputs, double learning_rate)
{
    /* To begin with, we must run the network forward. */
    genann_run(ann, inputs);

    int h, j, k;

    {                                                                                   /* First set the output layer deltas. */
        double const *o = ann->output + ann->inputs + ann->hidden * ann->hidden_layers; /* First output. */
        double *d = ann->delta + ann->hidden * ann->hidden_layers;                      /* First delta. */
        double const *t = desired_outputs;                                              /* First desired output. */

        /* Set output layer deltas. */
        if (genann_act_output == genann_act_linear ||
            ann->activation_output == genann_act_linear)
        {
            for (j = 0; j < ann->outputs; ++j)
            {
                *d++ = *t++ - *o++;
            }
        }
        else
        {
            for (j = 0; j < ann->outputs; ++j)
            {
                *d++ = (*t - *o) * *o * (1.0 - *o);
                ++o;
                ++t;
            }
        }
    }

    /* Set hidden layer deltas, start on last layer and work backwards. */
    /* Note that loop is skipped in the case of hidden_layers == 0. */
    for (h = ann->hidden_layers - 1; h >= 0; --h)
    {

        /* Find first output and delta in this layer. */
        double const *o = ann->output + ann->inputs + (h * ann->hidden);
        double *d = ann->delta + (h * ann->hidden);

        /* Find first delta in following layer (which may be hidden or output). */
        double const *const dd = ann->delta + ((h + 1) * ann->hidden);

        /* Find first weight in following layer (which may be hidden or output). */
        double const *const ww = ann->weight + ((ann->inputs + 1) * ann->hidden) + ((ann->hidden + 1) * ann->hidden * (h));

        /////////////////////////////////
        // TODO: 2 GEMV using BLAS   //
        /////////////////////////////////
        /*
        int m = 0;
        int n = 0;
        */
        //////////////////////////////////////////////////////////////////
        // TODO 2.a: Define the m and n dimension of the delta matrix   //
        // Hint: Look at the double for loop                            //
        //////////////////////////////////////////////////////////////////
        int m = (h == ann->hidden_layers - 1 ? ann->outputs : ann->hidden);
        int n = ann->hidden;

        //A temporary vector to store the propagated delta from the previuos layer.
        double *delta = calloc(ann->hidden, sizeof(double));
        ///////////////////////////////////////////////////////////////////
        // TODO 2.b: Decompose and implement GEMV BLAS call for the code //
        // Hint: Think about how ww is offset from its original address. //
        // You will need pointer arithmetic for the BLAS call            //
        ///////////////////////////////////////////////////////////////////
        GEMV(CblasRowMajor, CblasTrans, m, n, 1.0, ww + 1, n + 1, dd, 1, 0.0, delta, 1);

        for (j = 0; j < ann->hidden; ++j)
        {
            /*
            for (k = 0; k < (h == ann->hidden_layers - 1 ? ann->outputs : ann->hidden); ++k)
            {
                const int windex = k * (ann->hidden + 1) + (j + 1);
                delta[j] += dd[k] * ww[windex]; // HOTSPOT Part 1 Before - 24,97%, Part 1 After - 31,06%
            }
            */

            //Calculate the actual new deltas for this layer
            d[j] = o[j] * (1.0 - o[j]) * delta[j];
        }
        free(delta);

        /////////////////////////////////
        // TODO 2 END                  //
        /////////////////////////////////
    }

    /* Train the outputs. */
    {
        /* Find first output delta. */
        double const *d = ann->delta + ann->hidden * ann->hidden_layers; /* First output delta. */

        /* Find first weight to first output delta. */
        double *w = ann->weight + (ann->hidden_layers
                                       ? ((ann->inputs + 1) * ann->hidden + (ann->hidden + 1) * ann->hidden * (ann->hidden_layers - 1))
                                       : (0));

        /* Find first output in previous layer. */
        double const *const i = ann->output + (ann->hidden_layers
                                                   ? (ann->inputs + (ann->hidden) * (ann->hidden_layers - 1))
                                                   : 0);

        /* Set output layer weights. */
        for (j = 0; j < ann->outputs; ++j)
        {
            *w++ += *d * learning_rate * -1.0;
            for (k = 1; k < (ann->hidden_layers ? ann->hidden : ann->inputs) + 1; ++k)
            {
                *w++ += *d * learning_rate * i[k - 1];
            }

            ++d;
        }

        assert(w - ann->weight == ann->total_weights);
    }

    /* Train the hidden layers. */
    for (h = ann->hidden_layers - 1; h >= 0; --h)
    {

        /* Find first delta in this layer. */
        double const *d = ann->delta + (h * ann->hidden);

        /* Find first input to this layer. */
        double const *i = ann->output + (h
                                             ? (ann->inputs + ann->hidden * (h - 1))
                                             : 0);

        /* Find first weight to this layer. */
        double *w = ann->weight + (h
                                       ? ((ann->inputs + 1) * ann->hidden + (ann->hidden + 1) * (ann->hidden) * (h - 1))
                                       : 0);

        /////////////////////////////////
        // TODO: 3 (Optional) Optimize //
        /////////////////////////////////
        int iii = (h == 0 ? ann->inputs : ann->hidden) + 1;
        for (j = 0; j < ann->hidden; ++j)
        {
            *w++ += *d * learning_rate * -1.0; // HOTSPOT Part 2 After - 22.60%
            for (k = 1; k < iii; ++k)
            {
                *w++ += *d * learning_rate * i[k - 1]; // HOTSPOT Part 1 After - 16.53%, Part 2 After - 26.58%
            }
            ++d;
        }
        /////////////////////////////////
        // TODO 3 END                  //
        /////////////////////////////////
    }
}

void genann_write(genann const *ann, FILE *out)
{
    fprintf(out, "%d %d %d %d", ann->inputs, ann->hidden_layers, ann->hidden, ann->outputs);

    int i;
    for (i = 0; i < ann->total_weights; ++i)
    {
        fprintf(out, " %.20e", ann->weight[i]);
    }
}

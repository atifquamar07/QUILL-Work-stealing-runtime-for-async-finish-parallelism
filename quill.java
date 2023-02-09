/**********************************************************************************************/
/*  This program is part of the Barcelona OpenMP Tasks Suite                                  */
/*  Copyright (C) 2009 Barcelona Supercomputing Center - Centro Nacional de Supercomputacion  */
/*  Copyright (C) 2009 Universitat Politecnica de Catalunya                                   */
/*                                                                                            */
/*  This program is free software; you can redistribute it and/or modify                      */
/*  it under the terms of the GNU General Public License as published by                      */
/*  the Free Software Foundation; either version 2 of the License, or                         */
/*  (at your option) any later version.                                                       */
/*                                                                                            */
/*  This program is distributed in the hope that it will be useful,                           */
/*  but WITHOUT ANY WARRANTY; without even the implied warranty of                            */
/*  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the                             */
/*  GNU General Public License for more details.                                              */
/*                                                                                            */
/*  You should have received a copy of the GNU General Public License                         */
/*  along with this program; if not, write to the Free Software                               */
/*  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA            */
/**********************************************************************************************/

/*
 * Original code from the Cilk project (by Keith Randall)
 *
 * Copyright (c) 2000 Massachusetts Institute of Technology
 * Copyright (c) 2000 Matteo Frigo
 */
import edu.rice.hj.runtime.accumulator.FinishAccumulator;
import edu.rice.hj.runtime.accumulator.Operator;

import static edu.rice.hj.Module1.*;

/**
 * Nqueens was ported from the BOTS nqueens.c benchmark.  See below for provenance.
 *
 * N-Queens problem is how many possible placing are there for n queens on a n x n board
 *
 * This program computes all solutions to the n-queens problem where n is specified in args[0] (default = 13),
 * and repeats the computation "repeat" times where "repeat" is specifies in args[1] (default = 6).
 * There is a cutoff value specified as an optional third parameter in args[1] (default = 4)
 * that is used in the async seq clause to specify when a new async should be created.
 *
 * The program uses the count of the total number of solutions as a correctness check and
 * also prints the execution time for each repetition.
 * The FinishAccumulator class is used to accumulate the total count as an illustration
 * of how non-blocking operations can be used in conjunction with HJ.
 *
 * Note the use of single "finish" statement in findQueensPar() that awaits termination of all
 * async's created by the recursive calls to nqueensKernelPar.
 *
 * To study scalability on a multi-core processor, you can execute "Nqueens 13 4 6" by
 * varying the number of worker threads.
 *
 * @author Jun Shirako, Rice University
 * @author Vivek Sarkar, Rice University
 */
public class Nqueens {

    // STATIC FIELDS, the soluction for various values of n
    private static final int[] solutions = {
            1,
            0,
            0,
            2,
            10, /* 5 */
            4,
            40,
            92,
            352,
            724, /* 10 */
            2680,
            14200,
            73712,
            365596,
    };
    private static final int MAX_SOLUTIONS = 14;

    private static int size; // Problem size
    private static int cutoff_value; // Used to specify cutoff threshold in async seq clause

    /**
     * Main function
     * @param args Takes a number of queens (default 13), then a number
     *             of times to repeat the computation (default 6) and
     *             a cutoff value (default 4)
     */
    public static void main(final String[] args) {

        final int repeat = initialize(args);

        // global-finish
        finish(() -> {
            for (int i = 0; i < repeat; i++) {
                System.out.println("\nIteration-" + i);
                runNqueens(false);
                runNqueens(true);
            }
        });
    }

    /**
     * Grab info from the input parameters, then set and output them
     * @param args input arguments, same as main
     * @return the value of repeat
     */
    private static int initialize(final String[] args) {
        size = (args.length > 0) ? Integer.parseInt(args[0]) : 13;
        cutoff_value = (args.length > 2) ? Integer.parseInt(args[2]) : 4;
        final int repeat = (args.length > 1) ? Integer.parseInt(args[1]) : 6;

        if (size < 1) {
            size = 1;
            System.out.println("Size was modified to 1");
        } else if (size > MAX_SOLUTIONS) {
            size = MAX_SOLUTIONS;
            System.out.println("Size was modified to " + MAX_SOLUTIONS);
        }

        System.out.println("Configuration:");
        System.out.printf("%14s = %2d \n", "Input size", size);
        System.out.printf("%14s = %2d \n", "Cutoff value", cutoff_value);
        System.out.printf("%14s = %2d \n", "Repeats", repeat);

        return repeat;
    }

    /**
     * Runs NQueens program, outputs time analysis to stdout
     * @param parallel whether to operate in parallel or sequential
     */
    private static void runNqueens(final boolean parallel) {
        final Nqueens q = new Nqueens();

        // Timing for parallel run
        final long startTimeNanos = System.nanoTime();
        if (parallel) {
            q.findQueensPar();
        } else {
            q.findQueensSeq();
        }
        final long endTimeNanos = System.nanoTime();
        final double execTimeMillis = (endTimeNanos - startTimeNanos) / 1e6;

        final String modeStr = parallel ? "Parallel" : "Sequential";
        System.out.printf("  %10s Time: %9.2f ms. ", modeStr, execTimeMillis);

        q.verifyQueens();
    }

    private int total_count;

    /**
     * Parallel method to solving the n-queens problem
     */
    void findQueensPar() {

        final FinishAccumulator ac = newFinishAccumulator(Operator.SUM, int.class);

        finish(ac, () -> {
            final int[] a = new int[0];
            nqueensKernelPar(a, 0, ac);
        });

        total_count = ac.get().intValue();
    }

    /**
     * Does the bulk of the work finding the number of possible solutions there are to
     * n queens. Should be used recursively
     * @param a - mantains position of each queen
     * @param depth - Recursion depth, also which queen to place
     * @param ac - finish accumulator to count any solutions we found
     */
    void nqueensKernelPar(final int[] a, final int depth, final FinishAccumulator ac) {

        if (size == depth) {
            ac.put(1);
            return;
        }

        /* try each possible position for queen <depth> */
        for (int i = 0; i < size; i++) {
            final int ii = i;
            asyncSeq(depth >= cutoff_value, () -> {
                /* allocate a temporary array and copy <a> into it */
                final int[] b = new int[depth + 1];
                System.arraycopy(a, 0, b, 0, depth);
                b[depth] = ii;
                if (boardValid((depth + 1), b)) {
                    nqueensKernelPar(b, depth + 1, ac);
                }
            });

        }
    }

    /**
     * Sequentioal Method for finding the number of queens
     */
    void findQueensSeq() {

        final int[] a = new int[0];
        final int[] ac = {0}; // accumulates the result

        nqueensKernelSeq(a, 0, ac);

        total_count = ac[0];
    }

    /**
     * Does the bulk of the work finding the number of possible solutions there are to
     * n queens. Should be used recursively
     * @param a - mantains position of each queen
     * @param depth - Recursion depth, also which queen to place
     * @param ac - finish accumulator to count any solutions we found
     */
    void nqueensKernelSeq(final int[] a, final int depth, final int[] ac) {

        if (size == depth) {
            ac[0] += 1;
            return;
        }

        /* try each possible position for queen <depth> */
        for (int i = 0; i < size; i++) {
            /* allocate a temporary array and copy <a> into it */
            final int[] b = new int[depth + 1];
            System.arraycopy(a, 0, b, 0, depth);
            b[depth] = i;
            if (boardValid((depth + 1), b)) {
                nqueensKernelSeq(b, depth + 1, ac);
            }
        }
    }

    /*
     * <a> contains array of <n> queen positions.  Returns 1
     * if none of the queens conflict, and returns 0 otherwise.
     * @param n -  number of queens
     * @param a - positions of each queen
     * @return - whether none of the queens create conflict
     */
    boolean boardValid(final int n, final int[] a) {
        int i, j;
        int p, q;

        for (i = 0; i < n; i++) {
            p = a[i];

            for (j = (i + 1); j < n; j++) {
                q = a[j];
                if (q == p || q == p - (j - i) || q == p + (j - i)) {
                    return false;
                }
            }
        }

        return true;
    }

    /**
     * Check the answer obtained with the previously calculated solutions
     */
    void verifyQueens() {
        if (total_count == solutions[size - 1]) {
            System.out.println("  Answer OK");
        } else {
            System.err.println("  Incorrect answer");
        }
    }

}
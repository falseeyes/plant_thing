
/*
 * The following routines have been built from knowledge gathered
 * around the Web. I am not aware of any copyright problem with
 * them, so use it as you want.
 * N. Devillard - 1998
 */

/*----------------------------------------------------------------------------
   Function :   opt_med3()
   In       :   pointer to array of 3 pixel values
   Out      :   a int
   Job      :   optimized search of the median of 3 pixel values
   Notice   :   found on sci.image.processing
                cannot go faster unless assumptions are made
                on the nature of the input signal.
 ---------------------------------------------------------------------------*/

int opt_med3(int * p);

/*----------------------------------------------------------------------------
   Function :   opt_med5()
   In       :   pointer to array of 5 pixel values
   Out      :   a int
   Job      :   optimized search of the median of 5 pixel values
   Notice   :   found on sci.image.processing
                cannot go faster unless assumptions are made
                on the nature of the input signal.
 ---------------------------------------------------------------------------*/

int opt_med5(int * p);

/*----------------------------------------------------------------------------
   Function :   opt_med6()
   In       :   pointer to array of 6 pixel values
   Out      :   a int
   Job      :   optimized search of the median of 6 pixel values
   Notice   :   from Christoph_John@gmx.de
                based on a selection network which was proposed in
                "FAST, EFFICIENT MEDIAN FILTERS WITH EVEN LENGTH WINDOWS"
                J.P. HAVLICEK, K.A. SAKADY, G.R.KATZ
                If you need larger even length kernels check the paper
 ---------------------------------------------------------------------------*/

int opt_med6(int * p);


/*----------------------------------------------------------------------------
   Function :   opt_med7()
   In       :   pointer to array of 7 pixel values
   Out      :   a int
   Job      :   optimized search of the median of 7 pixel values
   Notice   :   found on sci.image.processing
                cannot go faster unless assumptions are made
                on the nature of the input signal.
 ---------------------------------------------------------------------------*/

int opt_med7(int * p);

/*----------------------------------------------------------------------------
   Function :   opt_med9()
   In       :   pointer to an array of 9 ints
   Out      :   a int
   Job      :   optimized search of the median of 9 ints
   Notice   :   in theory, cannot go faster without assumptions on the
                signal.
                Formula from:
                XILINX XCELL magazine, vol. 23 by John L. Smith
  
                The input array is modified in the process
                The result array is guaranteed to contain the median
                value
                in middle position, but other elements are NOT sorted.
 ---------------------------------------------------------------------------*/

int opt_med9(int * p);


/*----------------------------------------------------------------------------
   Function :   opt_med25()
   In       :   pointer to an array of 25 ints
   Out      :   a int
   Job      :   optimized search of the median of 25 ints
   Notice   :   in theory, cannot go faster without assumptions on the
                signal.
  				Code taken from Graphic Gems.
 ---------------------------------------------------------------------------*/

int opt_med25(int * p);

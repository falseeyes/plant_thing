
/*
 * The following routines have been built from knowledge gathered
 * around the Web. I am not aware of any copyright problem with
 * them, so use it as you want.
 * N. Devillard - 1998
 */


#define INT_SORT(a,b) { if ((a)>(b)) INT_SWAP((a),(b)); }
#define INT_SWAP(a,b) { int temp=(a);(a)=(b);(b)=temp; }

/*----------------------------------------------------------------------------
   Function :   opt_med3()
   In       :   pointer to array of 3 pixel values
   Out      :   a int
   Job      :   optimized search of the median of 3 pixel values
   Notice   :   found on sci.image.processing
                cannot go faster unless assumptions are made
                on the nature of the input signal.
 ---------------------------------------------------------------------------*/

int opt_med3(int * p)
{
    INT_SORT(p[0],p[1]) ; INT_SORT(p[1],p[2]) ; INT_SORT(p[0],p[1]) ;
    return(p[1]) ;
}

/*----------------------------------------------------------------------------
   Function :   opt_med5()
   In       :   pointer to array of 5 pixel values
   Out      :   a int
   Job      :   optimized search of the median of 5 pixel values
   Notice   :   found on sci.image.processing
                cannot go faster unless assumptions are made
                on the nature of the input signal.
 ---------------------------------------------------------------------------*/

int opt_med5(int * p)
{
    INT_SORT(p[0],p[1]) ; INT_SORT(p[3],p[4]) ; INT_SORT(p[0],p[3]) ;
    INT_SORT(p[1],p[4]) ; INT_SORT(p[1],p[2]) ; INT_SORT(p[2],p[3]) ;
    INT_SORT(p[1],p[2]) ; return(p[2]) ;
}

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

int opt_med6(int * p)
{
    INT_SORT(p[1], p[2]); INT_SORT(p[3],p[4]);
    INT_SORT(p[0], p[1]); INT_SORT(p[2],p[3]); INT_SORT(p[4],p[5]);
    INT_SORT(p[1], p[2]); INT_SORT(p[3],p[4]);
    INT_SORT(p[0], p[1]); INT_SORT(p[2],p[3]); INT_SORT(p[4],p[5]);
    INT_SORT(p[1], p[2]); INT_SORT(p[3],p[4]);
    return ( p[2] + p[3] ) * 0.5;
    /* INT_SORT(p[2], p[3]) results in lower median in p[2] and upper median in p[3] */
}


/*----------------------------------------------------------------------------
   Function :   opt_med7()
   In       :   pointer to array of 7 pixel values
   Out      :   a int
   Job      :   optimized search of the median of 7 pixel values
   Notice   :   found on sci.image.processing
                cannot go faster unless assumptions are made
                on the nature of the input signal.
 ---------------------------------------------------------------------------*/

int opt_med7(int * p)
{
    INT_SORT(p[0], p[5]) ; INT_SORT(p[0], p[3]) ; INT_SORT(p[1], p[6]) ;
    INT_SORT(p[2], p[4]) ; INT_SORT(p[0], p[1]) ; INT_SORT(p[3], p[5]) ;
    INT_SORT(p[2], p[6]) ; INT_SORT(p[2], p[3]) ; INT_SORT(p[3], p[6]) ;
    INT_SORT(p[4], p[5]) ; INT_SORT(p[1], p[4]) ; INT_SORT(p[1], p[3]) ;
    INT_SORT(p[3], p[4]) ; return (p[3]) ;
}

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

int opt_med9(int * p)
{
    INT_SORT(p[1], p[2]) ; INT_SORT(p[4], p[5]) ; INT_SORT(p[7], p[8]) ;
    INT_SORT(p[0], p[1]) ; INT_SORT(p[3], p[4]) ; INT_SORT(p[6], p[7]) ;
    INT_SORT(p[1], p[2]) ; INT_SORT(p[4], p[5]) ; INT_SORT(p[7], p[8]) ;
    INT_SORT(p[0], p[3]) ; INT_SORT(p[5], p[8]) ; INT_SORT(p[4], p[7]) ;
    INT_SORT(p[3], p[6]) ; INT_SORT(p[1], p[4]) ; INT_SORT(p[2], p[5]) ;
    INT_SORT(p[4], p[7]) ; INT_SORT(p[4], p[2]) ; INT_SORT(p[6], p[4]) ;
    INT_SORT(p[4], p[2]) ; return(p[4]) ;
}


/*----------------------------------------------------------------------------
   Function :   opt_med25()
   In       :   pointer to an array of 25 ints
   Out      :   a int
   Job      :   optimized search of the median of 25 ints
   Notice   :   in theory, cannot go faster without assumptions on the
                signal.
  				Code taken from Graphic Gems.
 ---------------------------------------------------------------------------*/

int opt_med25(int * p)
{


    INT_SORT(p[0], p[1]) ;   INT_SORT(p[3], p[4]) ;   INT_SORT(p[2], p[4]) ;
    INT_SORT(p[2], p[3]) ;   INT_SORT(p[6], p[7]) ;   INT_SORT(p[5], p[7]) ;
    INT_SORT(p[5], p[6]) ;   INT_SORT(p[9], p[10]) ;  INT_SORT(p[8], p[10]) ;
    INT_SORT(p[8], p[9]) ;   INT_SORT(p[12], p[13]) ; INT_SORT(p[11], p[13]) ;
    INT_SORT(p[11], p[12]) ; INT_SORT(p[15], p[16]) ; INT_SORT(p[14], p[16]) ;
    INT_SORT(p[14], p[15]) ; INT_SORT(p[18], p[19]) ; INT_SORT(p[17], p[19]) ;
    INT_SORT(p[17], p[18]) ; INT_SORT(p[21], p[22]) ; INT_SORT(p[20], p[22]) ;
    INT_SORT(p[20], p[21]) ; INT_SORT(p[23], p[24]) ; INT_SORT(p[2], p[5]) ;
    INT_SORT(p[3], p[6]) ;   INT_SORT(p[0], p[6]) ;   INT_SORT(p[0], p[3]) ;
    INT_SORT(p[4], p[7]) ;   INT_SORT(p[1], p[7]) ;   INT_SORT(p[1], p[4]) ;
    INT_SORT(p[11], p[14]) ; INT_SORT(p[8], p[14]) ;  INT_SORT(p[8], p[11]) ;
    INT_SORT(p[12], p[15]) ; INT_SORT(p[9], p[15]) ;  INT_SORT(p[9], p[12]) ;
    INT_SORT(p[13], p[16]) ; INT_SORT(p[10], p[16]) ; INT_SORT(p[10], p[13]) ;
    INT_SORT(p[20], p[23]) ; INT_SORT(p[17], p[23]) ; INT_SORT(p[17], p[20]) ;
    INT_SORT(p[21], p[24]) ; INT_SORT(p[18], p[24]) ; INT_SORT(p[18], p[21]) ;
    INT_SORT(p[19], p[22]) ; INT_SORT(p[8], p[17]) ;  INT_SORT(p[9], p[18]) ;
    INT_SORT(p[0], p[18]) ;  INT_SORT(p[0], p[9]) ;   INT_SORT(p[10], p[19]) ;
    INT_SORT(p[1], p[19]) ;  INT_SORT(p[1], p[10]) ;  INT_SORT(p[11], p[20]) ;
    INT_SORT(p[2], p[20]) ;  INT_SORT(p[2], p[11]) ;  INT_SORT(p[12], p[21]) ;
    INT_SORT(p[3], p[21]) ;  INT_SORT(p[3], p[12]) ;  INT_SORT(p[13], p[22]) ;
    INT_SORT(p[4], p[22]) ;  INT_SORT(p[4], p[13]) ;  INT_SORT(p[14], p[23]) ;
    INT_SORT(p[5], p[23]) ;  INT_SORT(p[5], p[14]) ;  INT_SORT(p[15], p[24]) ;
    INT_SORT(p[6], p[24]) ;  INT_SORT(p[6], p[15]) ;  INT_SORT(p[7], p[16]) ;
    INT_SORT(p[7], p[19]) ;  INT_SORT(p[13], p[21]) ; INT_SORT(p[15], p[23]) ;
    INT_SORT(p[7], p[13]) ;  INT_SORT(p[7], p[15]) ;  INT_SORT(p[1], p[9]) ;
    INT_SORT(p[3], p[11]) ;  INT_SORT(p[5], p[17]) ;  INT_SORT(p[11], p[17]) ;
    INT_SORT(p[9], p[17]) ;  INT_SORT(p[4], p[10]) ;  INT_SORT(p[6], p[12]) ;
    INT_SORT(p[7], p[14]) ;  INT_SORT(p[4], p[6]) ;   INT_SORT(p[4], p[7]) ;
    INT_SORT(p[12], p[14]) ; INT_SORT(p[10], p[14]) ; INT_SORT(p[6], p[7]) ;
    INT_SORT(p[10], p[12]) ; INT_SORT(p[6], p[10]) ;  INT_SORT(p[6], p[17]) ;
    INT_SORT(p[12], p[17]) ; INT_SORT(p[7], p[17]) ;  INT_SORT(p[7], p[10]) ;
    INT_SORT(p[12], p[18]) ; INT_SORT(p[7], p[12]) ;  INT_SORT(p[10], p[18]) ;
    INT_SORT(p[12], p[20]) ; INT_SORT(p[10], p[20]) ; INT_SORT(p[10], p[12]) ;

    return (p[12]);
}



#undef INT_SORT
#undef INT_SWAP


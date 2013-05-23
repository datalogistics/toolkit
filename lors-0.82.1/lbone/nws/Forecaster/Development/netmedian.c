/* $Id: netmedian.c,v 1.1.1.1 1998/01/16 23:59:45 jhayes Exp $ */

#include <sys/types.h>
#include <sys/times.h>
#include <math.h>
#include <stdio.h>
#include "netpred.h"

#define MAX_MED_N HISTORYSIZE

double M_array[MAX_MED_N];

Sort(M,size)
double *M;
int size;
{
	int i;
	int j;
	double temp;

	for(i = 0; i < size; i++)
	{
		for(j = 0; j < (size - (i + 1)); j++)
		{
			if(M[j] > M[j+1])
			{
				temp = M[j+1];
				M[j+1] = M[j];
				M[j] = temp;
			}
		}
	}
}

double
Median(tp,med_n)
struct thruput_pred *tp;
int med_n;
{
	struct value_el *data = tp->data;

	int i;
	int curr;
	int curr_size;
	double val;
	int index;

	curr_size = MODMINUS(tp->head,tp->tail,HISTORYSIZE);
	if(curr_size > med_n)
		curr_size = med_n;

	i = 0;
	curr = tp->head;
	while(i < curr_size)
	{
		M_array[i] = data[curr].value;
		curr = MODMINUS(curr,1,HISTORYSIZE);
		i++;
	}

	Sort(M_array,curr_size);

	index = curr_size/2;

	/*
	 * if even
	 */
	if(curr_size == (2 * index))
	{
		val = (M_array[index-1] + M_array[index]) / 2.0;
	}
	else
	{
		val = M_array[index];
	}

	return(val);
}

/*
 * page 264 in DSP book
 */
double
TrimmedMedian(tp,med_n,alpha)
struct thruput_pred *tp;
int med_n;
double alpha;
{
	struct value_el *data = tp->data;

	int i;
	int curr;
	int curr_size;
	double val;
	int index;

	int T;


	curr_size = MODMINUS(tp->head,tp->tail,HISTORYSIZE);
	if(curr_size > med_n)
		curr_size = med_n;

	T = alpha * curr_size;;
/*
 * Done by Median which must be called first
 */
#ifdef MEDIAN_NOT_FIRST
	i = 0;
	curr = tp->head;
	while(i < curr_size)
	{
		M_array[i] = data[curr].value;
		curr = MODMINUS(curr,1,HISTORYSIZE);
		i++;
	}

	Sort(M_array,curr_size);
#endif

	val = 0.0;
	for(index = T; index < (curr_size - T); index++)
	{
		val += M_array[index];
	}

	val = val / (double)(curr_size - 2*T);


	return(val);
}

#ifdef NOTRIGHTNOW
/*
 * pg. 260 -> 0.5*(L -2) passes to smooth to a root.  alpha
 * dictates L
 */
double
SmoothedMedian(tp,med_n,passes)
struct thruput_pred *tp;
int med_n;
int passes;
{
	struct value_el *data = tp->data;

	int i;
	int curr;
	int curr_size;
	double val;
	int index;
	int j;
	double replace_value;


	curr_size = MODMINUS(tp->head,tp->tail,HISTORYSIZE);
	if(curr_size > med_n)
		curr_size = med_n;

	replace_value = data[tp->head].value;

	for(j=0; j < passes; j++)
	{
		i = 0;
		curr = MODMINUS(tp->head,1,HISTORYSIZE);
		while(i < (curr_size - 1))
		{
			M_array[i] = data[curr].value;
			curr = MODMINUS(curr,1,HISTORYSIZE);
			i++;
		}

		M_array[curr_size - 1] = replace_value;
		Sort(M_array,curr_size);

		index = curr_size / 2;

        	/*
         	 * if even
         	 */
        	if(curr_size == (2 * index))
        	{
                	val = (M_array[index-1] + M_array[index]) / 2.0;
        	}
        	else
        	{
                	val = M_array[index];
        	}

		replace_value = val;

	}


	return(val);
}

double
StenciledMedian(tp,med_n)
struct thruput_pred *tp;
int med_n;
{
	struct value_el *data = tp->data;

	int i;
	int curr;
	int curr_size;
	double val;
	double val2;
	double val3;
	int index;

	curr_size = MODMINUS(tp->head,tp->tail,HISTORYSIZE);
	if(curr_size > med_n)
		curr_size = med_n;

	i = 0;
	curr = tp->head;
	while(i < curr_size)
	{
		M_array[i] = data[curr].value;
		curr = MODMINUS(curr,1,HISTORYSIZE);
		i++;
	}

	Sort(M_array,curr_size);

	index = curr_size/2;

	/*
	 * if even
	 */
	if(curr_size < 3)
	{
		if(curr_size == (2 * index))
		{
			val = (M_array[index-1] + M_array[index]) / 2.0;
		}
		else
		{
			val = M_array[index];
		}

		return(val);
	}
	else
	{
		if(curr_size == (2 * index))
		{
			val = (M_array[index-1] + M_array[index]) / 2.0;
			val2 = (M_array[index-2] + M_array[index-1]) / 2.0;
			val3 = (M_array[index] + M_array[index+1]) / 2.0;
		}
		else
		{
			val = M_array[index];
			val2 = M_array[index-1];
			val3 = M_array[index+1];
		}
		val = 0.5*val + 0.25*val2 + 0.25*val3;
		return(val);
	}


	
}

AdjustedMedian(tp,pred,min,max)
struct thruput_pred *tp;
int pred;
int min;
int max;
{
	double err1;
	double err2;
	double err3;
	double val1;
	double val2;
	double val3;
	int win;
	struct value_el *data = tp->data;
	int prev = MODMINUS(tp->head,1,HISTORYSIZE);
	int head = tp->head;
	double value = tp->data[head].value;
	double predictor = PRED(pred,data[prev]);
	int out_win;
	double out_val;

	win = WIN(pred,data[prev]);
	if(MODMINUS(head,tp->tail,HISTORYSIZE) < 3)
	{
		PRED(pred,data[head]) = value;
		WIN(pred,data[head]) = win;
		return(0.0);
	}
	if(win < 2)
		win = 2;
	val1 = Median(tp,win-1);
	val2 = Median(tp,win+1);
	val3 = Median(tp,win);

	err1 = (value - val1) * (value - val1);
	err2 = (value - val2) * (value - val2);
	err3 = (value - val3) * (value - val3);

	if(err1 < err2)
	{
		if(err1 < err3)
		{
			out_win = win - 1;
			out_val = val1;
		}
		else
		{
			out_win = win;
			out_val = val3;
		}
	}
	else if(err2 < err1)
	{
		if(err2 < err3)
		{
			out_win = win + 1;
			out_val = val2;
		}
		else
		{
			out_win = win;
			out_val = val3;
		}
	}
	else
	{
		out_win = win;
		out_val = val3;
	}

	if(out_win < min)
	{
		out_win = min;
		out_val = Median(tp,min);
	}

	if(out_win > max)
	{
		out_win = max;
		out_val = Median(tp,max);
	}

	WIN(pred,data[head]) = out_win;
	PRED(pred,data[head]) = out_val;
}
#endif

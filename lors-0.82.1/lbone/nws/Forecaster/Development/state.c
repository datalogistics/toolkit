/* $Id: state.c,v 1.3 1999/01/22 23:21:34 jhayes Exp $ */

#include <sys/types.h>
#include <sys/times.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "statepred.h"
#include "state.h"
#include "strutil.h"

extern double pow();
extern Sort();


#define SIGNIFICANT 50

static
SortIt2D(M,row,rowsize)
double *M;
int row;
int rowsize;
{
        int i;
        int j;
        double temp;
 
        for(i = 0; i < rowsize; i++)
        {
                for(j = 0; j < (rowsize - (i + 1)); j++)
                {
                        if(EL(M,row,j,MED_N) > EL(M,row,j+1,MED_N)) 
                        {
                                temp = EL(M,row,j+1,MED_N);
				EL(M,row,j+1,MED_N) = EL(M,row,j,MED_N);
				EL(M,row,j,MED_N) = temp;
                        }
                }
        }
}

static
SortRow2D(value,M,row,rowsize)
double value;
double *M;
int row;
int rowsize;
{
	int i;
	if(rowsize >= MED_N)
	{
		for(i=0; i < rowsize; i++)
		{
			if(value < EL(M,row,i,MED_N))
			{
				EL(M,row,i,MED_N) = value;
				return(0);
			}
		}
		EL(M,row,i-1,MED_N) = value;
	}
	else
	{
		EL(M,row,rowsize,MED_N) = value;
		SortIt2D(M,row,rowsize+1);
	}
	return(0);
}
		


double
RowCount(h,row)
struct histo *h;
int row;
{
	int i;
	double temp = 0.0;

	for(i=0; i < h->states; i++)
	{
		temp += EL(h->histogram,row,i,h->states);
	}

	return(temp);
}
		
double
ColCount(h,col)
struct histo *h;
int col;
{
	int i;
	double temp = 0.0;
	int rows;

	rows = (int)(pow((double)h->states,(double)h->dim));

	for(i=0; i < rows; i++)
	{
		temp += EL(h->histogram,i,col,h->states);
	}

	return(temp);
}

struct histo *
DefineHistogram(states,dim,min,max)
int states; /* number of different possible states */
int dim; /* the number of states in a history */
double min;
double max;
{
	int total_size;
	struct histo *h;

	h = (struct histo *)malloc(sizeof(struct histo));

	if(h == NULL)
	{
		return(NULL);
	}

	memset(h,0,sizeof(struct histo));
	h->states = states;
	h->dim = dim;
	h->min = min;
	h->max = max;

	
	total_size = (int)(pow((double)states,(double)dim) * 
			states * sizeof(double));

	h->histogram = (double *)malloc(total_size * sizeof(double));  

	if(h->histogram == NULL)
	{
		free(h);
		return(NULL);
	}
	memset(h->histogram,0,total_size * sizeof(double));  

	h->state_history = (int *)malloc(dim*sizeof(int));

	if(h->state_history == NULL)
	{
		free(h->histogram);
		free(h);
		return(NULL);
	}
	memset(h->state_history,0,dim*sizeof(int));

	return(h);
}

struct histo_pred *
DefineHistogramPred(h)
struct histo *h;
{
	struct histo_pred *hp;

	hp = (struct histo_pred *)malloc(sizeof(struct histo_pred));

	if(hp == NULL)
	{
		return(NULL);
	}

	hp->preds = (double *)malloc(MED_N*h->states*sizeof(double));
	if(hp->preds == NULL)
	{
		free(hp);
		return(NULL);
	}
	memset(hp->preds,0,MED_N*h->states*sizeof(double));

	hp->correct_mode = (double *)malloc(h->states*sizeof(double));
	if(hp->correct_mode == NULL)
	{
		free(hp->preds);
		free(hp);
		return(NULL);
	}
	memset(hp->correct_mode,0,h->states*sizeof(double));

	hp->count = (double *)malloc(h->states*sizeof(double));
	if(hp->count == NULL)
	{
		free(hp->preds);
		free(hp->correct_mode);
		free(hp);
		return(NULL);
	}
	memset(hp->count,0,h->states*sizeof(double));

	hp->errs = (double *)malloc(h->states*sizeof(double));
	if(hp->errs == NULL)
	{
		free(hp->preds);
		free(hp->correct_mode);
		free(hp->count);
		free(hp);
		return(NULL);
	}
	memset(hp->errs,0,h->states*sizeof(double));

	return(hp);
}

int
Hindex(history,states,dim)
int *history;
int states;
int dim;
{
	int index = 0;
	int i;
	double temp_d;

	for(i=0; i < dim; i++)
	{
		/*
		 * slot * states ^ i
		 */
		temp_d = (double)(history[i]) * pow((double)states,
			     (double)i);
		index += ((int)temp_d);
	}

	return(index);
}

Hstate(value,states,min,max)
double value;
int states;
double min;
double max;
{
	
	double state_range;  /* range of a single state */
	int ret_state; /* the actual state */

	/*
	 * if outside the range, return the boundaries
	 */
	if(value < min)
	{
		return(0);
	}
	if(value > max)
	{
		return(states-1);
	}

	state_range = (max - min) / (double)states;

	ret_state = (int)(value / state_range);

	return(ret_state);
}

double
MiddleOfState(state,total_states,min,max)
int state;
int total_states;
double min;
double max;
{
	
	double state_range;  /* range of a single state */
	double ret_val; /* the actual state */

	state_range = (max - min) / (double)total_states;
	/*
	 * if outside the range, return the boundaries
	 */
	if(state < 0)
	{
		ret_val = min + state_range / 2.0;
		return(ret_val);
	}
	if(state >= total_states)
	{
		ret_val = max - state_range / 2.0;
		return(ret_val);
	}


	ret_val = ((double)state * state_range) + (state_range / 2.0);

	return(ret_val);
}

int
MostLikelyStateIndex(h,index)
struct histo *h;
int index;
{
	double temp;
	double count;
	double max = 0.0;
	int i;
	int state = 0;

	count = RowCount(h,index);

	if(count == 0.0)
	{
		return(0);
	}

	for(i=0; i < h->states; i++)
	{
		temp = EL(h->histogram,index,i,h->states) / count;
		if(temp > max)
		{
			max = temp;
			state = i;
		}
	}

	return(state);
}

PredictedState(h,history)
struct histo *h;
int *history;
{
	int index;
	double temp;
	double count;
	double max = 0.0;
	int i;
	int state = 0;

	index = Hindex(history,h->states,h->dim);
	count = RowCount(h,index);

	if(count == 0.0)
	{
		return(0);
	}

	for(i=0; i < h->states; i++)
	{
		temp = EL(h->histogram,index,i,h->states) / count;
		if(temp > max)
		{
			max = temp;
			state = i;
		}
	}

	return(state);
}
int
MostLikelyState(h)
struct histo *h;
{
	int index;
	double temp;
	double count;
	double max = 0.0;
	int i;
	int state = 0;

	index = Hindex(h->state_history,h->states,h->dim);
	count = RowCount(h,index);

	if(count == 0.0)
	{
		return(0);
	}

	for(i=0; i < h->states; i++)
	{
		temp = EL(h->histogram,index,i,h->states) / count;
		if(temp > max)
		{
			max = temp;
			state = i;
		}
	}

	return(state);
}
		

UpdateHistogram(h,value)
struct histo *h;
double value;
{
	int row;  /* the previous states */
	int col;  /* the current state */
	int i;

	row = Hindex(h->state_history,h->states,h->dim);
	col = Hstate(value,h->states,h->min,h->max);
	
	/*
	 * update the count 
	 */
	EL(h->histogram,row,col,h->states) += 1.0;

	/*
	 * update the state history
	 */
	if(h->dim > 1)
	{
		for(i=h->dim - 2; i >= 0; i--)
		{
			h->state_history[i+1] = h->state_history[i];
		}
	}
	h->state_history[0] = col;
}



PrintIt(h,curr,array)
struct histo *h;
int curr;
int *array;
{
	double row_count;	
	int i;
	int j;
	int index;
	double prob;

	if(curr < h->dim)
	{
		for(i=0; i < h->states; i++)
		{
			array[h->dim - curr - 1] = i;
			PrintIt(h,curr+1,array);
		}
	}
	else
	{
		for(j=h->dim - 1; j >= 0; j--)
		{
			fprintf(stdout,"%d",array[j]);
		}
		fprintf(stdout,"  :   ");
		index = Hindex(array,h->states,h->dim);
		row_count = RowCount(h,index);
		for(j=0; j < h->states; j++)
		{
			if(row_count > 0.0)
			{
				prob = 
				EL(h->histogram,index,j,h->states) / row_count;
			}
			else
			{
				prob = 0.0;
			}
			fprintf(stdout,"%3.2f ",prob);
			fflush(stdout);
		}
		fprintf(stdout," (%d)\n",(int)row_count);

	}
}

PrintHistogram(h)
struct histo *h;
{
	int *array;
	int i;
	double total_correct = 0.0;
	double total_count = 0.0;

	array = (int *)malloc(h->dim * sizeof(int));

	if(array == NULL)
	{
		return(0);
	}

	PrintIt(h,0,array);



	free(array);
}

CheckIt(h,curr,array)
struct histo *h;
int curr;
int *array;
{
	double row_count;	
	int i;
	int j;
	int index;
	double prob;
	int next_state;

	if(curr < h->dim)
	{
		for(i=0; i < h->states; i++)
		{
			array[h->dim - curr - 1] = i;
			CheckIt(h,curr+1,array);
		}
	}
	else
	{
		index = Hindex(array,h->states,h->dim);
		next_state = MostLikelyStateIndex(h,index);
		if((array[0] != next_state) &&
			RowCount(h,index) > SIGNIFICANT)
		{
			for(j=h->dim - 1; j >= 0; j--)
                	{
                        	fprintf(stdout,"%d",array[j]);
                	}
                	fprintf(stdout,"  :   ");
			row_count = RowCount(h,index);
			for(j=0; j < h->states; j++)
			{
				if(row_count > 0.0)
				{
					prob = 
					EL(h->histogram,index,j,h->states) 
						/ row_count;
				}
				else
				{
					prob = 0.0;
				}
				fprintf(stdout,"%3.2f ",prob);
				fflush(stdout);
			}
			fprintf(stdout," (%d)\n",(int)row_count);

		}
	}
}

CheckHistogram(h)
struct histo *h;
{
	int *array;
	int i;
	double total_correct = 0.0;
	double total_count = 0.0;

	array = (int *)malloc(h->dim * sizeof(int));

	if(array == NULL)
	{
		return(0);
	}

	CheckIt(h,0,array);



	free(array);
}

double
MeanStatePrediction(h,hp)
struct histo *h;
struct histo_pred *hp;
{
	double pred;
	int pred_state;
	double col_count;

        /*
         * find the most likley next mode
         */
        pred_state = MostLikelyState(h);
 
        hp->next_state = pred_state;
 
        col_count = ColCount(h,pred_state);
 
        /*
         * predict the average
         */
        if(col_count > 0.0)
                pred = EL(hp->preds,pred_state,0,h->states);
        else
	{
                pred = 0.0;
	}

	return(pred);
}

UpdateMeanStatePrediction(h,hp,value)
struct histo *h;
struct histo_pred *hp;
double value;
{

	int pred_state;
	double pred;
	int current_state;
	double col_count;
	double temp_d;

	/*
	 * get state in which value falls
	 */
	current_state = Hstate(value,h->states,h->min,h->max);
	/*
	 * mark if we predicted correctly
	 */
	if(current_state == hp->next_state)
	{
		hp->correct_mode[current_state] += 1.0;
	}
	/*
	 * update the prediction state -- predictions are stored row
	 * major by state.  Zeroth element contains running mean
	 */
	temp_d = EL(hp->preds,current_state,0,MED_N);
	col_count = ColCount(h,current_state); 
	if(col_count > 0.0)
	{
		/*
		 * -1.0 since we have already added value to histogram
		 */
		temp_d = temp_d * (col_count - 1.0);
		temp_d += value;
		temp_d = temp_d / col_count;
		EL(hp->preds,current_state,0,MED_N) = temp_d;
	}
	else
	{
		EL(hp->preds,current_state,0,MED_N) = value;
	}

	return(0);
}

static double TempMed[MED_N];

double
MedianP(h,hp,pred_state)
struct histo *h;
struct histo_pred *hp;
int pred_state;
{
	double pred;
	double pred_count;
	int history_size;
	int i;
 
        pred_count = hp->count[pred_state];

        if(pred_count == 0.0)
        {
                return(0.0);
        }

	if(pred_count > MED_N)
	{
		pred_count = MED_N;
	}

	for(i=0; i < (int)pred_count; i++)
	{
		TempMed[i] = EL(hp->preds,pred_state,i,MED_N);
	}
	
	Sort(TempMed,(int)pred_count);
 
 
        if((int)pred_count < MED_N)
        {
                history_size = (int)pred_count;
        }
        else
        {
                history_size = MED_N;
        }
 
        /*
         * predict the median of the most likely mode
         */
        if(((history_size / 2) * 2) == history_size)
        {
                pred = (TempMed[history_size/2] +
                       TempMed[(history_size/2)-1]) / 2.0;
        }
        else
	{
                pred = TempMed[history_size/2];
        }

 
        return(pred);
}

double
MedianStatePrediction(h,hp)
struct histo *h;
struct histo_pred *hp;
{
	int pred_state;
	double pred;

	/*
         * find the most likley next mode
         */
        pred_state = MostLikelyState(h);
 
        hp->next_state = pred_state;
	pred = MedianP(h,hp,pred_state);

	return(pred);
}


UpdateMedianStatePrediction(h,hp,value)
struct histo *h;
struct histo_pred *hp;
double value;
{

	int pred_state;
	double modal_pred;
	int current_state;
	double pred_count;
	double current_count;
	int history_size;
	int i;
	double err;


	current_state = Hstate(value,h->states,h->min,h->max);
	if(current_state == hp->next_state)
	{
		hp->correct_mode[current_state] += 1.0;
	}

	current_count = hp->count[current_state];
	if(current_count == 0.0)
	{
		EL(hp->preds,current_state,0,MED_N) = value;
	}
	else
	{
		if((int)current_count < MED_N)
		{
			EL(hp->preds,current_state,(int)current_count,MED_N) =
				value;
		}
		else
		{
			for(i=MED_N-2; i >= 0; i--)
			{
				EL(hp->preds,current_state,i+1,MED_N) =
					EL(hp->preds,current_state,i,MED_N);
			}
			EL(hp->preds,current_state,0,MED_N) = value;
		}
	}
	hp->count[current_state] += 1.0;

	/*
	 * update deviations
	 */
	pred_count = hp->count[current_state];
 
        if((int)pred_count < MED_N)
        {
                history_size = (int)pred_count;
        }
        else
        {
                history_size = MED_N;
        }
 
        /*
         * get the median of the current mode
         */
	modal_pred = MedianP(h,hp,current_state);
	
	/*
	 * update the per mode errs
	 */
	err = value - modal_pred;
	if(err < 0.0)
	{
		err = err * -1.0;
	}
	hp->errs[current_state] = hp->errs[current_state]*(pred_count - 1.0);
	hp->errs[current_state] = (hp->errs[current_state] + err) / pred_count;
	
	

	return(0);
}

ModalDev(h,hp,factor,hi,lo)
struct histo *h;
struct histo_pred *hp;
double factor;
double *hi;
double *lo;
{ 

	double median;

	median = MedianP(h,hp,0);
	*lo = median - (factor*(hp->errs[0]));
	median = MedianP(h,hp,h->states-1);
	*hi = median + (factor*(hp->errs[h->states-1]));
}

double
MiddleStatePrediction(h,hp)
struct histo *h;
struct histo_pred *hp;
{
	int pred_state;
	double pred;
	double col_count;

        /*
         * find the most likley next mode
         */
        pred_state = MostLikelyState(h);
 
        pred = MiddleOfState(pred_state,h->states,h->min,h->max);
 
        hp->next_state = pred_state;
 
        col_count = ColCount(h,pred_state);
 
        if(col_count == 0.0)
        {
                return(0.0);
        }
 
 
        /*
         * predict the middle value
         */
        return(pred);
}


UpdateMiddleStatePrediction(h,hp,value)
struct histo *h;
struct histo_pred *hp;
double value;
{

	int pred_state;
	double pred;
	int current_state;
	double col_count;

	current_state = Hstate(value,h->states,h->min,h->max);
	if(current_state == hp->next_state)
	{
		hp->correct_mode[current_state] += 1.0;
	}


	return(0);
}


#if defined(STANDALONESTATES)

#define PRED_ARGS "f:d:s:l:h:"

#include "format.h"

CalculateHistogram(fname,h)
char *fname;
struct histo *h;
{
	FILE *fd;
	char line_buf[255];
	char *cp;
	time_t ltime;
	float value;
	int i;


	fd = fopen(fname,"r");

	if(fd == NULL)
	{
		fprintf(stderr,
			"CalculateThruputPredictors: unable to open file %s\n",
				fname);
		fflush(stderr);
		exit(1);
	}

	while(fgets(line_buf,255,fd))
	{
		cp = line_buf;

		while(isspace(*cp))
			cp++;

		for(i=1; i < TSFIELD; i++)
		{
			while(!isspace(*cp))
				cp++;

			while(isspace(*cp))
				cp++;
		}

		ltime = atoi(cp);

		cp = line_buf;

		for(i=1; i < VALFIELD; i++)
		{
			while(!isspace(*cp))
				cp++;

			while(isspace(*cp))
				cp++;
		}

		value = atof(cp);

		UpdateHistogram(h,log(value));

	}

	fclose(fd);
}



 
struct histo *Histogram;
int Histodim;
int Histostates;
double Histomax;
double Histomin;

main(argc,argv)
int argc;
char *argv[];
{

	FILE *fd;
	int i;
	int j;
	double temp_mse;
	double temp_mpe;
	char fname[255];
	char c;


	if(argc < 2)
	{
		fprintf(stderr,"usage: testpred filename\n");
		fflush(stderr);
		exit(1);
	}

	fname[0] = 0;
	Histodim = DIM;
	Histostates = STATES;
	Histomax = HMAX;
	Histomin = HMIN;

	while((c = getopt(argc,argv,PRED_ARGS)) != EOF)
	{
		switch(c)
		{
			case 'f':
				SAFESTRCPY(fname,optarg);
				break;
			case 'd':
				Histodim = atoi(optarg);
				break;
			case 's':
				Histostates = atoi(optarg);
				break;
			case 'l':
				Histomin = atof(optarg);
				break;
			case 'h':
				Histomax = atof(optarg);
				break;
		}
	}

	if(fname[0] == 0)
	{
		fprintf(stderr,"usage: states -f fname [-d,s,l,h]\n");
		fflush(stderr);
		exit(1);
	}

	Histogram = DefineHistogram(Histostates,
                        Histodim,
                        log(Histomin),
                        log(Histomax));
				
	fprintf(stdout,"Calculating histogram...");
	fflush(stdout);

	CalculateHistogram(fname,Histogram);

	fprintf(stdout,"done.\n");
	fflush(stdout);

	PrintHistogram(Histogram);
	fflush(stdout);

	exit(0);

	
}
#endif

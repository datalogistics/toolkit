/* $Id: netpred.c,v 1.2 1999/01/22 23:21:33 jhayes Exp $ */

/****************************************************************/
/*                                                              */
/*      Global include files                                    */
/*                                                              */
/****************************************************************/

#include <sys/types.h>
#include <sys/times.h>
 /* seems that POSIX specifies that fcntl.h is simply <fctnl.h> while */
 /* for some reason, I was including <sys/fcntl.h>. This breaks */
 /* Ultrix, and was discovered by Johnathan Stone at Stanford. */
#include <stdio.h>
#include <sys/time.h>
#include <errno.h>
#include <math.h>
#include <string.h>


#include <sys/file.h>


#include "netlib.h"
#include "netpred.h"

extern double Median();
extern double TrimmedMedian();


ThruputPrediction(tp)
struct thruput_pred *tp;
{
	 int tail = tp->tail;
	 int head = tp->head;
	 int i;
	 int prev;
	 double error;
	 double sq_error;
	 struct value_el *data = tp->data;
	 double count = 0.0;
	 double pred;
	 double value;
	 double run_mean = tp->run_mean;
	 double total = tp->total;
	 double tot_sum;
	 double min_run_p_error = 999999999.99;
	 int min_run_p_i;
	 double min_run_sq_error = 99999999.99;
	 int min_run_sq_i;
	 double prev_pred;
	 double percent_error;
	 int last_error;
	 double min_win_sq_error = 99999999.99;
	 double min_win_p_error = 999999999.99;
	 int min_win_sq_i;
	 int min_win_p_i;
	 double min_tot_sq_error = 99999999.99;
	 double min_tot_p_error = 999999999.99;
	 int min_tot_sq_i;
	 int min_tot_p_i;
	 double win_error;
	 int error_window;
	 int datasize;

	 datasize = MODMINUS(head,tail,HISTORYSIZE) - 1;
	 if(ERR_WIN > datasize)
	 	error_window = datasize;
	 else
		error_window = ERR_WIN;

	 last_error = MODMINUS(head,error_window,HISTORYSIZE);


	/*
	 * first, calculate the error our last prediction made
	 * now that we have the actual value
	 */
	prev = MODMINUS(head,1,HISTORYSIZE);

	for(i=0; i < PREDS; i++)
	{

		if(PRED(i,data[prev]) == 0.0)
			continue;

        	error = data[head].value - PRED(i,data[prev]);
        	sq_error = error * error;

        	SQERR(i,data[head]) = SQERR(i,data[prev])+sq_error;
		TPMSE(tp,i) += sq_error;
		if(error < 0.0)
			error = -1.0 * error;
#ifdef MPE_ON
		if(data[head].value != 0.0)
			percent_error = error / data[head].value;
		else
			percent_error = 0.0;
        	ERR(i,data[head]) = ERR(i,data[prev])+percent_error;
		TPMPE(tp,i) += percent_error;
#else
        	ERR(i,data[head]) = ERR(i,data[prev])+error;
		TPMPE(tp,i) += error;
#endif
		TPCOUNT(tp,i) += 1.0;

	}


	value = data[head].value;


	/*
	 * use the current running mean
	 */
	PRED(0,data[head]) = value;


	/*
	 * use the current value as a prediction of the next value
	 */
	tot_sum = run_mean * total;
	tot_sum += data[head].value;
	total++;
	run_mean = tot_sum/total;
	tp->run_mean = run_mean;
	tp->total = total;
	PRED(1,data[head]) = run_mean;

	/*
	 * use the Van Jacobson method
	 */
	prev_pred = PRED(2,data[prev]);
	PRED(2,data[head]) = prev_pred + GAIN*(value - prev_pred);

	PRED(3,data[head]) = Median(tp,MED_N);
	PRED(4,data[head]) = TrimmedMedian(tp,MED_N,MED_ALPHA);

	/*
	 * Now use TrimmedMean to give a sliding window mean
	 */
	PRED(5,data[head]) = TrimmedMedian(tp,MED_N,0.0);

	for(i=0; i < STATIC_PREDS; i++)
	{
		if((TPMSE(tp,i)/TPCOUNT(tp,i)) < min_run_sq_error)
		{
			min_run_sq_error = TPMSE(tp,i)/TPCOUNT(tp,i);
			min_run_sq_i = i;
		}

		if((TPMPE(tp,i)/TPCOUNT(tp,i)) < min_run_p_error)
		{
			min_run_p_error = TPMPE(tp,i)/TPCOUNT(tp,i);
			min_run_p_i = i;
		}

		win_error = 
			(SQERR(i,data[head]) - SQERR(i,data[last_error])) /
				(double)error_window;
		if(win_error < min_win_sq_error)
		{
			min_win_sq_error = win_error;
			min_win_sq_i = i;
		}

		win_error = 
			(ERR(i,data[head]) - ERR(i,data[last_error])) /
				(double)error_window;
		if(win_error < min_win_p_error)
		{
			min_win_p_error = win_error;
			min_win_p_i = i;
		}

	}


	PRED(MSE_PRED,data[head]) = PRED(min_run_sq_i,data[head]);
	TPPREDNUM(tp,MSE_PRED) = min_run_sq_i;
	PRED(MPE_PRED,data[head]) = PRED(min_run_p_i,data[head]);
	TPPREDNUM(tp,MPE_PRED) = min_run_p_i;
	PRED(WIN_MSE_PRED,data[head]) = PRED(min_win_sq_i,data[head]);
	TPPREDNUM(tp,WIN_MSE_PRED) = min_win_sq_i;
	PRED(WIN_MPE_PRED,data[head]) = PRED(min_win_p_i,data[head]);
	TPPREDNUM(tp,WIN_MPE_PRED) = min_win_p_i;

	for(i=STATIC_PREDS; i < SECOND_ORDER_PREDS; i++)
	{
		if((TPMSE(tp,i)/TPCOUNT(tp,i)) < min_tot_sq_error)
		{
			min_tot_sq_error = TPMSE(tp,i)/TPCOUNT(tp,i);
			min_tot_sq_i = TPPREDNUM(tp,i);
		}
		if((TPMPE(tp,i)/TPCOUNT(tp,i)) < min_tot_p_error)
		{
			min_tot_p_error = TPMPE(tp,i)/TPCOUNT(tp,i);
			min_tot_p_i = TPPREDNUM(tp,i);
		}
	}
	PRED(TOT_MSE_PRED,data[head]) = PRED(min_tot_sq_i,data[head]);
	TPPREDNUM(tp,TOT_MSE_PRED) = min_tot_sq_i;
	PRED(TOT_MPE_PRED,data[head]) = PRED(min_tot_p_i,data[head]);
	TPPREDNUM(tp,TOT_MPE_PRED) = min_tot_p_i;

#ifdef STANDALONE
	fprintf(stdout,"%d %d %d\n",data[head].time,min_win_sq_i,min_run_sq_i);
	fflush(stdout);
#endif


}

InitThruputPrediction(tp)
struct thruput_pred *tp;
{
	int i;

	tp->head = 0;
	tp->tail = 0;


	memset(tp->data,0,sizeof(tp->data));

	for(i=0; i < PREDS; i++)
	{
		TPMSE(tp,i) = 0.0;
		TPCOUNT(tp,i) = 0.0;
	}
}

UpdateThruputPrediction(tp,value,time)
struct thruput_pred *tp;
float value;
time_t time;
{

	int head = tp->head;
	int prev;
	int tail = tp->tail;
	struct value_el *data = tp->data;
	int i;
        int data_size = MODMINUS(head,tail,HISTORYSIZE);

	tp->data[tp->head].time = time;
	tp->data[tp->head].value = value;

	/*
	 * a negative tail value says that we haven't filled the
	 * pipe yet.  See if this does it.
	 */
	if(data_size > MIN_DATA_HISTORY)
	{
		ThruputPrediction(tp);

	}
	else
	{
		for(i=0; i < PREDS; i++)
		{
			PRED(i,data[head]) = tp->data[head].value;
			ERR(i,data[head]) = 0.0;
			SQERR(i,data[head]) = 0.0;
		}
	}



	tp->head = MODPLUS(head,1,HISTORYSIZE);

	/*
	 * if we have incremented head forward to that it is sitting on
	 * top of tail, then we need to bump tail.  The entire buffer is
	 * being used to fill the window.
	 */
	if(tp->head == tp->tail)
		tp->tail = MODPLUS(tp->tail,1,HISTORYSIZE);

	return(0);
}

#ifdef STANDALONE

#include "format.h"

CalculateThruputPredictors(fname,tp)
char *fname;
struct thruput_pred *tp;
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

		if(value != 0.0)
			UpdateThruputPrediction(tp,value,ltime);

	}

	fclose(fd);
}


struct thruput_pred Tp;

main(argc,argv)
int argc;
char *argv[];
{

	FILE *fd;
	int i;

	InitThruputPrediction(&Tp);

	if(argc < 2)
	{
		fprintf(stderr,"usage: testpred filename\n");
		fflush(stderr);
		exit(1);
	}

	fprintf(stdout,"Calculating throughput predictors...");
	fflush(stdout);

	CalculateThruputPredictors(argv[1],&Tp);

	fprintf(stdout,"done.\n");
	fflush(stdout);

	for(i=0; i < PREDS; i++)
	{

		fprintf(stdout,"Predictor %d, MSE: %3.4f MPE: %3.4f\n",
			i,TPMSE(&Tp,i)/TPCOUNT(&Tp,i),
				TPMPE(&Tp,i)/TPCOUNT(&Tp,i));
	}

	fflush(stdout);

	exit(0);

	
}
#endif

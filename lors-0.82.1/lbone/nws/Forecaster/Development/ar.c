/* $Id: ar.c,v 1.1.1.1 1998/01/16 23:59:45 jhayes Exp $ */

#include <sys/types.h>
#include <sys/times.h>
#include <math.h>
#include <stdio.h>

#include "predictor.h"

static double F[AR_P+1][AR_N+1];
static double B[AR_P+1][AR_N+1];
static double A[AR_P+1][AR_N+1];
static double K[AR_N+1];
static double NEW_K[AR_N+1];
static double E_P[AR_P+1][AR_N+1];
static double E_M[AR_P+1][AR_N+1];

double Ppred;

double
CalcP(f,b,n,N)
double *f;
double *b;
int n;
int N;
{
	int j;
	double tot = 0.0;

	for(j=n; j <= N-1; j++)
	{
		tot += (EL(f,n-1,j,N) * EL(b,n-1,j,N));
	}
	return(tot);
}

double
CalcQ(f,b,n,N)
double *f;
double *b;
int n;
int N;
{
	int j;
	double tot = 0.0;

	for(j=n; j <= N-1; j++)
	{
		tot += (EL(f,n-1,j,N)*EL(f,n-1,j,N) +
			  EL(b,n-1,j,N)*EL(b,n-1,j,N));
	}
	return(tot);
}

/*
 * pth-order AR forecast using N data points
 *
 * follows pg 336 and appendix E of Haddad/Parsons DSP -- Theory and Practice
 */
double AR(tp,N,p)
struct thruput_pred *tp;
int N;
int p;
{
	
	double *f;
	double *b;
	double *a;
	double *k;
	double *e;
	int j;
	int n;
	struct value_el *data = tp->data;
	int curr_j;
	int curr_jm1;
	int i;
	int curr_i;
	double err = 0.0;
	double pred = 0.0;
	double P;
	double Q;
	int curr_size;

	curr_size = MODMINUS(tp->head,tp->tail,HISTORYSIZE) - 1;

	if(curr_size <= p)
	{
		return(0.0);
	}

	if(curr_size <= N) 
		N = curr_size;

/*
	f = (double*)malloc((p+1)*(N+1)*sizeof(double));
	b = (double*)malloc((p+1)*(N+1)*sizeof(double));
	a = (double*)malloc((p+1)*(N+1)*sizeof(double));
	k = (double*)malloc((N+1)*sizeof(double));


	if((f == NULL) || (b == NULL) || (a == NULL) || (k == NULL))
	{
		fprintf(stderr,"no space for AR arrays\n");
		exit(1);
	}
*/

	f = (double *)F;
	b = (double *)B;
	a = (double *)A;
	k = K;
	



	/*
	 * initialize iteration 0
	 */
	curr_j = tp->head;
	curr_jm1 = MODMINUS(tp->head,1,HISTORYSIZE);
	for(j = 1; j <= N; j++)
	{
		EL(f,0,j,N) = data[curr_j].value;
		EL(b,0,j,N) = data[curr_jm1].value;
		curr_j = MODMINUS(curr_j,1,HISTORYSIZE);
		curr_jm1 = MODMINUS(curr_jm1,1,HISTORYSIZE);
	}

	EL(a,0,0,N) = 1.0;

	for(n = 1; n <= p; n++)
	{
		P = CalcP(f,b,n,N);
		Q = CalcQ(f,b,n,N);
		k[n] = -2.0*P/Q;
		EL(a,n,n,N) = k[n];
		EL(a,n,0,N) = 1.0;
		for(i=1; i <= n-1; i++)
		{
			EL(a,n,i,N) = EL(a,n-1,i,N) +
				k[n] * EL(a,n-1,n-i,N);
		}
		for(j=n; j <= N-1; j++)
		{
			EL(f,n,j,N) = EL(f,n-1,j,N) +
				k[n] * EL(b,n-1,j,N);
			EL(b,n,j,N) = EL(b,n-1,j-1,N) +
				k[n] * EL(f,n-1,j-1,N);
		}

	}

/*
	curr_i = MODMINUS(tp->head,1,HISTORYSIZE);
	for(i=1; i <= N; i++)
	{
		pred += (data[curr_i].value * EL(a,p,i,N));
		curr_i = MODMINUS(curr_i,1,HISTORYSIZE);
	}
*/

	curr_i = MODMINUS(tp->head,1,HISTORYSIZE);
	for(i=1; i <= p; i++)
	{
		pred += (data[curr_i].value * EL(a,p,i,N));
		curr_i = MODMINUS(curr_i,1,HISTORYSIZE);
	}
	pred = -1.0 * pred;

	Ppred = pred;

/*
	free(a);
	free(f);
	free(b);
	free(k);
*/
	return(pred);
}

/*
 * uses K computed in AR
 */
double
LAT(tp,N,p)
struct thruput_pred *tp;
int N;
int p;
{
	int i;
	int j;
	double *k = K;
	double *new_k = NEW_K;
	double *em = (double *)E_M;
	double *ep = (double *)E_P;
	double pred1;
	double pred2;
	int curr_j;
	int curr_size;
	double value = tp->data[tp->head].value;
	double err;
	double sigma_sq=0.0;
	double mu;

	curr_size = MODMINUS(tp->head,tp->tail,HISTORYSIZE)-1;

	if(curr_size <= p)
		return(0.0);
	if(curr_size < N)
		N = curr_size;

	curr_j = MODMINUS(tp->head,N,HISTORYSIZE);
	for(j=0; j<= N; j++)
	{
		E_P[0][j]  = tp->data[curr_j].value;
		E_M[0][j]  = tp->data[curr_j].value;
		curr_j = MODPLUS(curr_j,1,HISTORYSIZE);
	}

	for(i=1; i <= p; i++)
	{
		for(j = 1; j <= N; j++)
		{
			E_M[i][j]  = E_M[i-1][j-1] +
				k[i] * E_P[i-1][j-1];
			E_P[i][j] = E_P[i-1][j] +
				k[i] * E_M[i-1][j];
		}
	}

	curr_j = tp->head;
	for(j=1; j <= N-1; j++)
	{
		sigma_sq += (tp->data[curr_j].value * tp->data[curr_j].value);
		curr_j = MODMINUS(curr_j,1,HISTORYSIZE);
	}
	sigma_sq = sigma_sq / (double)(N-1);
	mu = 1.0/(2.0 * N * sigma_sq);

	for(i=1; i<=p; i++)
	{
		new_k[i] = k[i] - (2.0 * mu * E_P[p][N] * E_M[p][N]);
	}

	err = E_P[p][N];
	pred1 = value - err;

	for(i=1; i <= p; i++)
	{
		for(j = 1; j <= N; j++)
		{
			E_M[i][j]  = E_M[i-1][j-1] +
				new_k[i] * E_P[i-1][j-1];
			E_P[i][j] = E_P[i-1][j] +
				new_k[i] * E_M[i-1][j];
		}
	}

	err = E_P[p][N];
	pred2 = value - err;
	return(pred2);
}



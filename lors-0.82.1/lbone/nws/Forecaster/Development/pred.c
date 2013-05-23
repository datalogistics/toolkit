/* $Id: pred.c,v 1.1.1.1 1998/01/16 23:59:45 jhayes Exp $ */

#include <stdio.h>

extern double atof();
extern double strtod();
extern double fabs();


#define MAXBUFFSIZE 5000
#define MAXINTERVAL 3600
#define STEP 120

#define MODMINUS(dst,src,val) \
	if((src) == 0) \
		(dst) = MAXBUFFSIZE - (val); \
	else \
		(dst) = (src) - (val);\

struct buff_strc
{
	double ts;
	double value;
};

struct buff_strc Buffer[MAXBUFFSIZE];

int B_end;
int B_start;
int B_curr;


FILE *Fd;

ReadData(ts,value)
double *ts;
double *value;
{

        char *err;
        char line_buf[255];
        char *ctmp;
        unsigned int utmp;

again:
	*ts = 0.0;
        err = fgets(line_buf,255,Fd);

        if(!err)
        {
                *ts = -1.0;
                return(0);
        }

        ctmp = line_buf;

        *ts = strtod(ctmp,&ctmp);

	if(*ts == 0.0)
		goto again;


        while(!isdigit(*ctmp))
                ctmp++;

        *value = atof(ctmp);


        return(1);
}


GetBuffer(interval)
double interval;
{
	int i;
	char line_buff[255];
	char *err;

	B_curr = -1;

	for(i=B_start; i <= B_end; i++)
	{
		if(!ReadData(&(Buffer[i].ts),&(Buffer[i].value)))
		{
			B_end = i;
			if(B_curr == -1)
				B_curr = B_end;
			break;
		}

		if((B_curr == -1) &&
			((Buffer[i].ts - interval) >= Buffer[B_start].ts))
			B_curr = i;
	}	


	return(1);

}

InitBuffer(interval)
double interval;
{
	rewind(Fd);

	B_start = 0;
	B_end = MAXBUFFSIZE - 1;
	B_curr = 0;

	if(!GetBuffer(interval))
		return(0);

	return(1);
}

GetNextEntry(ts,value)
double *ts;
double *value;
{

	int end;

	end = (B_end + 1) % MAXBUFFSIZE;

	if(B_curr == end)
	{
		if(!ReadData(&(Buffer[B_curr].ts),&(Buffer[B_curr].value)))
		{
			*ts = -1.0;
			return(0);
		}

		B_end = (B_end + 1) % MAXBUFFSIZE;
		B_start = (B_start + 1) % MAXBUFFSIZE;
	}

	*ts = Buffer[B_curr].ts;
	*value = Buffer[B_curr].value;

	return(1);
}

double
IntervalMean(interval)
double interval;
{
	double wredge;
	int curr;
	double sum = 0.0;
	double count = 0.0;

	/*
	 * walk backwards from B_curr (which will not be considered)
	 */

	MODMINUS(curr,B_curr,1);

	while(Buffer[curr].ts >= (Buffer[B_curr].ts - interval))
	{
		sum += Buffer[curr].value;
		count += 1.0;

		MODMINUS(curr,curr,1);

		if(curr == B_curr)
		{
			break;
		}
	}

	if(count == 0.0)
		return(-1.0);
	else
		return(sum/count);
}

main(argc,argv)
int argc;
char *argv[];
{
	char err_str[255];
	double interval;
	double mean;
	double ts;
	double value;
	unsigned int utmp;
	double count = 0.0;
	double mse = 0.0;

	int i;
	int mini = 0;
	double minmse = 999999999.99;
	int cinterval = 0;

	if(argc < 2)
        {
                fprintf(stderr,"usage: pred interval filename\n");
                fflush(stderr);
                exit(1);
        }

	if(!(Fd = fopen(argv[2],"r")))
        {
                sprintf(err_str,"pred: could not open file %s",argv[2]);
                fprintf(stderr,"%s\n",err_str); 
		fclose(Fd); 
		exit(1);
        }

	printf("value\tprediction\tsq err\n");
	printf("===========================\n");


	interval = atof(argv[1]);
	InitBuffer(interval);


	mean = IntervalMean(cinterval);
	cinterval++;
	value = Buffer[B_curr].value;

	if(mean != -1.0)
	{
	    printf("%3.3f %3.3f %3.3f\n",value,mean,(mean-value)*(mean-value));
	    fflush(stdout);
	}

	B_curr = (B_curr + 1) % MAXBUFFSIZE;

	while(GetNextEntry(&ts,&value))
	{
		mean = IntervalMean(cinterval);
		cinterval++;
		if(mean != -1.0)
		{
	            printf("%3.3f %3.3f %3.3f\n",value,
				mean,(mean-value)*(mean-value));
		    fflush(stdout);
		}

		B_curr = (B_curr + 1) % MAXBUFFSIZE;
	}


	fclose(Fd);

}	
	

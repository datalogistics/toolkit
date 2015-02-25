/* $Id: state.h,v 1.1.1.1 1998/01/16 23:59:45 jhayes Exp $ */


struct histo
{
        double *histogram;
        int *state_history;
        int states;
        int dim;
	double min;
	double max;
};

struct histo_pred
{
	double *count;
	double *preds;
	double *errs;
	double *correct_mode;
	int next_state;
};

extern struct histo* DefineHistogram();
extern struct histo_pred* DefineHistogramPred();
extern UpdateHistogram();
extern PrintHistogram();

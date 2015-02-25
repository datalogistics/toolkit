/* $Id: netpred.h,v 1.1.1.1 1998/01/16 23:59:45 jhayes Exp $ */

#define HISTORYSIZE 12
#define MIN_DATA_HISTORY 2

#define PREDS 12
#define STATIC_PREDS (PREDS - 6)
#define SECOND_ORDER_PREDS (PREDS - 2)
#define MSE_PRED (STATIC_PREDS)
#define MPE_PRED (STATIC_PREDS+1)
#define WIN_MSE_PRED (STATIC_PREDS+2)
#define WIN_MPE_PRED (STATIC_PREDS+3)
#define TOT_MSE_PRED (SECOND_ORDER_PREDS)
#define TOT_MPE_PRED (SECOND_ORDER_PREDS+1)

#define DYNPRED TOT_MSE_PRED

#define MED_N 10
#define MED_ALPHA (0.1)

#define ERR_WIN 10

struct pred_el
{
	float pred;
	double error;
	double sq_error;
};

struct value_el
{
	time_t time;
	float value;
	struct pred_el preds[PREDS];
};

struct thruput_pred
{
	int head;
	int tail;
	struct value_el data[HISTORYSIZE];

	double mean;
	double run_mean;
	double total;

	double mses[PREDS];
	double mpes[PREDS];
	double counts[PREDS];
	int pred_nums[PREDS-STATIC_PREDS];
};

#define MODPLUS(a,b,m) (((a) + (b)) % (m))
#define MODMINUS(a,b,m) (((a) - (b) + (m)) % (m))

#define PRED(n,v) ((v).preds[(n)].pred)
#define ERR(n,v) ((v).preds[(n)].error)
#define SQERR(n,v) ((v).preds[(n)].sq_error)

#define TPPREDNUM(tp,n) ((tp)->pred_nums[(n)-STATIC_PREDS])
#define TPMSE(tp,n) ((tp)->mses[(n)])
#define TPMPE(tp,n) ((tp)->mpes[(n)])
#define TPCOUNT(tp,n) ((tp)->counts[(n)])

#define CURRVAL(tp) ((tp).data[MODMINUS((tp).head,1,HISTORYSIZE)].value)
#define CURRTIME(tp) ((tp).data[MODMINUS((tp).head,1,HISTORYSIZE)].time)
#define CURRPRED(tp) \
	PRED(TOT_MSE_PRED,(tp).data[MODMINUS((tp).head,1,HISTORYSIZE)])
#define CURRMSE(tp) \
        (TPCOUNT(&(tp),TOT_MSE_PRED) != 0.0 ?\
	(TPMSE(&(tp),TOT_MSE_PRED)/TPCOUNT(&(tp),TOT_MSE_PRED)) : 0.0)
#define CURRPREDNUM(tp) TPPREDNUM(&(tp),TOT_MSE_PRED)

extern UpdateThruputPrediction();
extern InitThruputPrediction();

#define GAIN (0.05)

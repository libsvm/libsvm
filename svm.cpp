#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <float.h>
#include <string.h>
#include "svm.h"
template <class T> inline T min(T x,T y) { return (x<y)?x:y; }
template <class T> inline T max(T x,T y) { return (x>y)?x:y; }
#define EPS_A 1e-12
#define INF DBL_MAX

// Kernel Cache
class Cache
{
private:
	int l;
	int size;
	struct head_t
	{
		head_t *prev, *next;	// a cicular list
		int index;
		double *data;
	};

	head_t* head;
	head_t* lru_head;
	head_t** index_to_head;
	void move_to_last(head_t *h);
public:
	Cache(int _l,int _size);
	~Cache();

	// return 1 if returned data is valid (cache hit)
	// return 0 if returned data needs to be filled (cache miss)
	int get_data(const int index,double **data);
};

Cache::Cache(int _l,int _size):l(_l),size(_size)
{
	head = new head_t[size];
	int i;
	for(i=0;i<size;i++)
	{
		head[i].next = &head[i+1];
		head[i].prev = &head[i-1];
		head[i].index = -1;
		head[i].data = new double[l];
	}

	head[0].prev = &head[size-1];
	head[size-1].next = &head[0];
	lru_head = &head[0];

	index_to_head = new head_t *[l];
	for(i=0;i<l;i++)
		index_to_head[i] = 0;
}

Cache::~Cache()
{
	for(int i=0;i<size;i++)
		delete[] head[i].data;
	delete[] head;
	delete[] index_to_head;
}

void Cache::move_to_last(head_t *h)
{
	if(lru_head == h)
		lru_head = lru_head->next;
	else
	{
		// delete from current location
		h->prev->next = h->next;
		h->next->prev = h->prev;

		// insert to last position
		h->next = lru_head;
		h->prev = lru_head->prev;
		h->prev->next = h;
		h->next->prev = h;
	}
}

int Cache::get_data(const int index,double **data)
{
	head_t *h=index_to_head[index];
	if(h)
	{
		move_to_last(h);
		*data = h->data;
		return 1;
	}		
	else
	{
		// get one from lru_head
		h = lru_head;
		lru_head = lru_head->next;
		if(h->index!=-1)
			index_to_head[h->index] = 0;
		h->index = index;
		index_to_head[index] = h;
		*data = h->data;
		return 0;
	}
}

// dot

static double dot(const svm_node *px, const svm_node *py)
{
	double sum = 0;
	while(px->index != -1 && py->index != -1)
	{
		if(px->index == py->index)
		{
			sum += px->value * py->value;
			++px;
			++py;
		}
		else
		{
			if(px->index > py->index)
				++py;
			else
				++px;
		}			
	}
	return sum;
}

// The Q Matrix
class Kernel {
private:
	// svm_problem
	const int l;
	const svm_node * const * const x;
	const signed char *y;

	// svm_parameter
	const int kernel_type;
	const double degree;
	const double gamma;
	const double coef0;

	Cache *cache;
	double *x_square;
	double (Kernel::*kernel_function)(int i, int j) const;
	double kernel_linear(int i, int j) const
	{
		return dot(x[i],x[j]);
	}
	double kernel_poly(int i, int j) const
	{
		return pow(gamma*dot(x[i],x[j])+coef0,degree);
	}
	double kernel_rbf(int i, int j) const
	{
		return exp(-gamma*(x_square[i]+x_square[j]-2*dot(x[i],x[j])));
	}
	double kernel_sigmoid(int i, int j) const
	{
		return tanh(gamma*dot(x[i],x[j])+coef0);
	}
public:
	Kernel(const svm_problem& prob, const svm_parameter& param);
	~Kernel();
	double *get_Q(int i) const;
};

Kernel::Kernel(const svm_problem& prob, const svm_parameter& param)
:l(prob.l),x(prob.x),y(prob.y),
 kernel_type(param.kernel_type),degree(param.degree),gamma(param.gamma),
 coef0(param.coef0)
{
	cache = new Cache(l,(int)min((double)l,(param.cache_size*(1<<20))/(sizeof(double)*l)));

	switch(kernel_type)
	{
		case LINEAR:
			kernel_function = &Kernel::kernel_linear;
			break;
		case POLY:
			kernel_function = &Kernel::kernel_poly;
			break;
		case RBF:
			kernel_function = &Kernel::kernel_rbf;
			break;
		case SIGMOID:
			kernel_function = &Kernel::kernel_sigmoid;
			break;
		default:
			fprintf(stderr,"unknown kernel function.\n");
			exit(1);
	}

	if(kernel_type == RBF)
	{
		x_square = new double[l];
		for(int i=0;i<l;i++)
			x_square[i] = dot(x[i],x[i]);
	}
	else
		x_square = 0;
}

Kernel::~Kernel()
{
	delete x_square;
	delete cache;
}

double *Kernel::get_Q(int i) const
{
	double *data;
	if(cache->get_data(i,&data)==0)
	{
		for(int j=0;j<l;j++)
			data[j] = y[i]*y[j]*(this->*kernel_function)(i,j);
	}
	return data;
}

// XXX, because of x_square, I have to duplicate this...
static double k_function(const svm_node *x, const svm_node *y,
			 const svm_parameter& param)
{
	switch(param.kernel_type)
	{
		case LINEAR:
			return dot(x,y);
		case POLY:
			return pow(param.gamma*dot(x,y)+param.coef0,param.degree);
		case RBF:
		{
			double sum = 0;
			while(x->index != -1 && y->index !=-1)
			{
				if(x->index == y->index)
				{
					double d = x->value - y->value;
					sum += d*d;
					++x;
					++y;
				}
				else
				{
					if(x->index > y->index)
					{	
						sum += y->value * y->value;
						++y;
					}
					else
					{
						sum += x->value * x->value;
						++x;
					}
				}
			}

			while(x->index != -1)
			{
				sum += x->value * x->value;
				++x;
			}

			while(y->index != -1)
			{
				sum += y->value * y->value;
				++y;
			}
			
			return exp(-param.gamma*sum);
		}
		case SIGMOID:
			return tanh(param.gamma*dot(x,y)+param.coef0);
		default:
			fprintf(stderr,"unknown kernel function.\n");
			exit(1);
	}
}

// svm_model

struct svm_model
{
	int n;			// number of SVs
	double *alpha_y;	// coefficients of SV[i] * y[i]
	svm_node const ** SV;	// SVs
	double b;		// b in the decision function

	svm_parameter param;	// parameter

	int free_sv;		// XXX: 1 if svm_model is created by svm_load_model
				//      0 if svm_model is created by svm_train
};

// SMO+SVMlight algorithm
class Solver {
private:
	const int l;		// # of training vectors
	const svm_node * const * const x;	// training vectors
	const signed char *y;	// label of training data
	const double C;
	const double eps;	// stopping criterion
	double *alpha;
	enum { LOWER_BOUND, UPPER_BOUND, FREE };
	char *alpha_status;	// LOWER_BOUND, UPPER_BOUND, FREE
	double *O;		// gradient of objective function
	svm_model& out_model;
	const Kernel* kernel;

	bool is_upper_bound(int i) { return alpha_status[i] == UPPER_BOUND; }
	bool is_lower_bound(int i) { return alpha_status[i] == LOWER_BOUND; }
	int select_working_set(int &i, int &j);
public:
	Solver(const svm_problem& prob, const svm_parameter& param, svm_model& model);
	~Solver();
	void solve();
};

Solver::Solver(const svm_problem& prob, const svm_parameter& param, svm_model& model)
:l(prob.l),x(prob.x),y(prob.y),
 C(param.C),eps(param.eps),
 out_model(model),kernel(new Kernel(prob,param))
{
	alpha = new double[l];
	alpha_status = new char[l];
	O = new double[l];
	model.param = param;
}

Solver::~Solver()
{
	delete[] O;
	delete[] alpha_status;
	delete[] alpha;
	delete kernel;
}

// return 1 if already optimal, return 0 otherwise
int Solver::select_working_set(int &out_i, int &out_j)
{
	// return i,j which maximize -grad(f)^T d , under constraint
	// if alpha_i == C, d != +1
	// if alpha_i == 0, d != -1

	double Omax1 = -INF;		// max { -grad(f)_i * d | y_i*d = +1 }
	int Omax1_idx = -1;

	double Omax2 = -INF;		// max { -grad(f)_i * d | y_i*d = -1 }
	int Omax2_idx = -1;

	for(int i=0;i<l;i++)
	{
		if(y[i]==+1)	// y = +1
		{
			if(!is_upper_bound(i))	// d = +1
			{
				if(-O[i] > Omax1)
				{
					Omax1 = -O[i];
					Omax1_idx = i;
				}
			}
			if(!is_lower_bound(i))	// d = -1
			{
				if(O[i] > Omax2)
				{
					Omax2 = O[i];
					Omax2_idx = i;
				}
			}
		}
		else		// y = -1
		{
			if(!is_upper_bound(i))	// d = +1
			{
				if(-O[i] > Omax2)
				{
					Omax2 = -O[i];
					Omax2_idx = i;
				}
			}
			if(!is_lower_bound(i))	// d = -1
			{
				if(O[i] > Omax1)
				{
					Omax1 = O[i];
					Omax1_idx = i;
				}
			}
		}
	}

	if(Omax1+Omax2 < eps)
 		return 1;

	out_i = Omax1_idx;
	out_j = Omax2_idx;
	return 0;
}

void Solver::solve()
{
	// initialize alpha and O

	int i;
	for(i=0;i<l;i++)
	{
		alpha[i] = 0;
		alpha_status[i] = LOWER_BOUND;
		O[i] = -1;
	}

	// optimization step

	int iter = 0;
	int counter = 0;

	while(1)
	{
		int i,j;
		if(select_working_set(i,j)!=0)
			break;

		++iter;

		// progress bar

		if(++counter == 100)
		{	
			fprintf(stderr,".");
			counter = 0;
		}

		// update alpha[i] and alpha[j]

		double old_alpha_i = alpha[i];
		double old_alpha_j = alpha[j];

		double *Q_i = kernel->get_Q(i);
		double *Q_j = kernel->get_Q(j);
		
		if(y[i]!=y[j])
		{
			double L = max(0.0,alpha[j]-alpha[i]);
			double H = min(C,C+alpha[j]-alpha[i]);
			alpha[j] += (-O[i]-O[j])/(Q_i[i]+Q_j[j]+2*Q_i[j]);
			if(alpha[j] >= H) alpha[j] = H;
			else if(alpha[j] <= L) alpha[j] = L;
			alpha[i] += (alpha[j] - old_alpha_j);
		}
		else
		{
			double L = max(0.0,alpha[i]+alpha[j]-C);
			double H = min(C,alpha[i]+alpha[j]);
			alpha[j] += (O[i]-O[j])/(Q_i[i]+Q_j[j]-2*Q_i[j]);
			if(alpha[j] >= H) alpha[j] = H;
			else if(alpha[j] <= L) alpha[j] = L;			
			alpha[i] -= (alpha[j] - old_alpha_j);
		}

		// update alpha_status

		if(alpha[i] >= C-EPS_A)
			alpha_status[i] = UPPER_BOUND;
		else if(alpha[i] <= EPS_A)
			alpha_status[i] = LOWER_BOUND;
		else alpha_status[i] = FREE;

		if(alpha[j] >= C-EPS_A)
			alpha_status[j] = UPPER_BOUND;
		else if(alpha[j] <= EPS_A)
			alpha_status[j] = LOWER_BOUND;
		else alpha_status[j] = FREE;

		// update O

		double delta_alpha_i = alpha[i] - old_alpha_i;
		double delta_alpha_j = alpha[j] - old_alpha_j;
		
		for(int k=0;k<l;k++)
		{
			O[k] += Q_i[k]*delta_alpha_i + Q_j[k]*delta_alpha_j;
		}
	}

	// calculate and output b

	{
		double b;
		int nr_free = 0;
		double ub = INF, lb = -INF, sum_free = 0;
		for(int i=0;i<l;i++)
		{
			double yO = y[i]*O[i];

			if(is_lower_bound(i))
				ub = min(ub,yO);
			else if(is_upper_bound(i))
				lb = max(lb,yO);
			else
			{
				++nr_free;
				sum_free += yO;
			}
		}

		if(nr_free>0)
			b = sum_free/nr_free;
		else
			b = (ub+lb)/2;
		
		out_model.b = b;
	}

	// output SVs

	int nSV=0;
	for(i=0;i<l;i++)
	{
		if(!is_lower_bound(i))
			++nSV;
	}

	out_model.n = nSV;
	out_model.alpha_y = (double *)malloc(sizeof(double) * nSV);
	out_model.SV = (const svm_node **)malloc(sizeof(svm_node*) * nSV);

	{
		int j = 0;
		for(int i=0;i<l;i++)
		{
			if(!is_lower_bound(i))
			{
				out_model.alpha_y[j] = alpha[i] * y[i];
				out_model.SV[j] = x[i];
				++j;
			}
		}
	}

	printf("\noptimization finished, #iter = %d\n",iter);
}

// Interface

svm_model *svm_train(const svm_problem *prob, const svm_parameter *param)
{
	svm_model *model = (svm_model *)malloc(sizeof(svm_model));
	Solver smo(*prob,*param,*model);
	smo.solve();
	model->free_sv = 0;	// XXX
	return model;
}

double svm_classify(const svm_model *model, const svm_node *x)
{
	const int n = model->n;
	const double *alpha_y = model->alpha_y;

	double sum = 0;
	for(int i=0;i<n;i++)
		sum += alpha_y[i] * k_function(x,model->SV[i],model->param);

	return sum-(model->b);
}

const char *kernel_type_table[]=
{
	"linear","polynomial","rbf","sigmoid",NULL
};

int svm_save_model(const char *model_file_name, const svm_model *model)
{
	FILE *fp = fopen(model_file_name,"w");
	if(fp==NULL) return -1;

	const svm_parameter& param = model->param;

	fprintf(fp,"kernel_type %s\n", kernel_type_table[param.kernel_type]);

	if(param.kernel_type == POLY)
		fprintf(fp,"degree %g\n", param.degree);

	if(param.kernel_type == POLY || param.kernel_type == RBF || param.kernel_type == SIGMOID)
		fprintf(fp,"gamma %g\n", param.gamma);

	if(param.kernel_type == POLY || param.kernel_type == SIGMOID)
		fprintf(fp,"coef0 %g\n", param.coef0);

	fprintf(fp, "b %g\n", model->b);
	fprintf(fp, "SV %d\n", model->n);

	const int n = model->n;
	const double * const alpha_y = model->alpha_y;
	const svm_node **SV = model->SV;

	for(int i=0;i<n;i++)
	{
		fprintf(fp, "%.16g ",alpha_y[i]);
		const svm_node *p = SV[i];
		while(p->index != -1)
		{
			fprintf(fp,"%d:%.8g ",p->index,p->value);
			p++;
		}
		fprintf(fp, "\n");
	}

	fclose(fp);
	return 0;
}

svm_model *svm_load_model(const char *model_file_name)
{
	FILE *fp = fopen(model_file_name,"r");
	if(fp==NULL) return NULL;
	
	// read n,b,param

	svm_model *model = (svm_model *)malloc(sizeof(svm_model));
	svm_parameter& param = model->param;

	char cmd[81];
	while(1)
	{
		fscanf(fp,"%80s",cmd);
		if(strcmp(cmd,"kernel_type")==0)
		{		
			fscanf(fp,"%80s",cmd);
			int i;
			for(i=0;kernel_type_table[i];i++)
			{
				if(strcmp(kernel_type_table[i],cmd)==0)
				{
					param.kernel_type=i;
					break;
				}
			}
			if(kernel_type_table[i] == NULL)
			{
				fprintf(stderr,"unknown kernel function.\n");
				exit(1);
			}
		}
		else if(strcmp(cmd,"degree")==0)
			fscanf(fp,"%lf",&param.degree);
		else if(strcmp(cmd,"gamma")==0)
			fscanf(fp,"%lf",&param.gamma);
		else if(strcmp(cmd,"coef0")==0)
			fscanf(fp,"%lf",&param.coef0);
		else if(strcmp(cmd,"b")==0)
			fscanf(fp,"%lf",&model->b);
		else if(strcmp(cmd,"SV")==0)
		{
			fscanf(fp,"%d",&model->n);
			while(1)
			{
				int c = getc(fp);
				if(c==EOF || c=='\n') break;	
			}
			break;
		}
		else
		{
			fprintf(stderr,"unknown text in model file\n");
			exit(1);
		}
	}

	// read alpha_y and SV

	int elements = 0;
	long pos = ftell(fp);

	while(1)
	{
		int c = fgetc(fp);
		switch(c)
		{
			case '\n':
				// count the '-1' element
			case ':':
				++elements;
				break;
			case EOF:
				goto out;
			default:
				;
		}
	}
out:
	fseek(fp,pos,SEEK_SET);

	const int n = model->n;
	model->alpha_y = (double*)malloc(sizeof(double)*n);
	model->SV = (const svm_node**)malloc(sizeof(svm_node*)*n);
	svm_node *x_space = (svm_node*)malloc(sizeof(svm_node)*elements);

	int j=0;
	for(int i=0;i<n;i++)
	{
		model->SV[i] = &x_space[j];
		fscanf(fp,"%lf",&model->alpha_y[i]);
		while(1)
		{
			int c;
			do {
				c = getc(fp);
				if(c=='\n') goto out2;
			} while(isspace(c));
			ungetc(c,fp);
			fscanf(fp,"%d:%lf",&(x_space[j].index),&(x_space[j].value));
			++j;
		}	
out2:
		x_space[j++].index = -1;
	}

	fclose(fp);

	model->free_sv = 1;	// XXX
	return model;
}

void svm_destroy_model(svm_model* model)
{
	if(model->free_sv)
		free((void *)(model->SV[0]));
	free(model->alpha_y);
	free(model->SV);
	free(model);
}

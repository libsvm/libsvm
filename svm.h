#ifdef __cplusplus
extern "C" {
#endif

struct svm_node
{
	int index;
	double value;
};

struct svm_problem
{
	int l;
	signed char *y;
	struct svm_node **x;
};

enum { LINEAR, POLY, RBF, SIGMOID };	/* kernel_type */

struct svm_parameter
{
	int kernel_type;
	double degree;	// for poly
	double gamma;	// for poly/rbf/sigmoid
	double coef0;	// for poly/sigmoid

	// these are for training only
	double cache_size; // in MB
	double C;
	double eps;
};

struct svm_model;

struct svm_model *svm_train(const struct svm_problem *prob,
			    const struct svm_parameter *param);

int svm_save_model(const char *model_file_name, const struct svm_model *model);

struct svm_model *svm_load_model(const char *model_file_name);

double svm_classify(const struct svm_model *model, const struct svm_node *x);

void svm_destroy_model(struct svm_model *model);

#ifdef __cplusplus
}
#endif

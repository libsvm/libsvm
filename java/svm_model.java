//
// svm_model
//
public class svm_model
{
	int n;			// number of SVs
	double[] sv_coef;	// sv_coef[i] is the coefficient of SV[i]
	svm_node[][] SV;	// SVs
	double rho;		// the constant in the decision function

	svm_parameter param;	// parameter
};

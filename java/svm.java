
import java.io.*;
import java.util.*;

//
// Kernel Cache
//
// l is the number of total data items
// size is the cache size limit in bytes
//
class Cache {
	private final int l;
	private int size;
	private final class head_t
	{
		head_t prev, next;	// a cicular list
		double[] data;
		int len;		// data[0,len) is cached in this entry
	}
	private final head_t[] head;
	private head_t lru_head;

	Cache(int l_, int size_)
	{
		l = l_;
		size = size_;
		head = new head_t[l];
		for(int i=0;i<l;i++) head[i] = new head_t();
		size /= 8;
		size -= l * 2;	// sizeof(head_t) / sizeof(double)
		lru_head = new head_t();
		lru_head.next = lru_head.prev = lru_head;
	}

	private void lru_delete(head_t h)
	{
		// delete from current location
		h.prev.next = h.next;
		h.next.prev = h.prev;
	}

	private void lru_insert(head_t h)
	{
		// insert to last position
		h.next = lru_head;
		h.prev = lru_head.prev;
		h.prev.next = h;
		h.next.prev = h;
	}

	// request data [0,len)
	// return some position p where [p,len) need to be filled
	// (p >= len if nothing needs to be filled)
	// java: simulate pointer using single-element array
	int get_data(int index, double[][] data, int len)
	{
		head_t h = head[index];
		if(h.len > 0) lru_delete(h);
		int more = len - h.len;

		if(more > 0)
		{
			// free old space
			while(size < more)
			{
				head_t old = lru_head.next;
				lru_delete(old);
				size += old.len;
				old.data = null;
				old.len = 0;
			}

			// allocate new space
			double[] new_data = new double[len];
			if(h.data != null) System.arraycopy(h.data,0,new_data,0,h.len);
			h.data = new_data;
			size -= more;
			do {int _=h.len; h.len=len; len=_;} while(false);
		}

		lru_insert(h);
		data[0] = h.data;
		return len;
	}

	void swap_index(int i, int j)
	{
		if(i==j) return;
		
		if(head[i].len > 0) lru_delete(head[i]);
		if(head[j].len > 0) lru_delete(head[j]);
		do {double[] _=head[i].data; head[i].data=head[j].data; head[j].data=_;} while(false);
		do {int _=head[i].len; head[i].len=head[j].len; head[j].len=_;} while(false);
		if(head[i].len > 0) lru_insert(head[i]);
		if(head[j].len > 0) lru_insert(head[j]);

		if(i>j) do {int _=i; i=j; j=_;} while(false);
		for(head_t h = lru_head.next; h!=lru_head; h=h.next)
		{
			if(h.len > i)
			{
				if(h.len > j)
					do {double _=h.data[i]; h.data[i]=h.data[j]; h.data[j]=_;} while(false);
				else
				{
					// give up
					lru_delete(h);
					size += h.len;
					h.data = null;
					h.len = 0;
				}
			}
		}
	}
}

//
// Kernel evaluation
//
// the static method k_function is for doing single kernel evaluation
// the constructor of Kernel prepares to calculate the l*l kernel matrix
// the member function get_Q is for getting one column from the Q Matrix
//
abstract class Kernel {
	private svm_node[][] x;
	private final double[] x_square;

	// svm_parameter
	private final int kernel_type;
	private final double degree;
	private final double gamma;
	private final double coef0;

	abstract double[] get_Q(int column, int len);

	void swap_index(int i, int j)
	{
		do {svm_node[] _=x[i]; x[i]=x[j]; x[j]=_;} while(false);
		if(x_square != null) do {double _=x_square[i]; x_square[i]=x_square[j]; x_square[j]=_;} while(false);
	}

	private static double tanh(double x)
	{
		double e = Math.exp(x);
		return 1.0-2.0/(e*e+1);
	}

	double kernel_function(int i, int j)
	{
		switch(kernel_type)
		{
			case svm_parameter.LINEAR:
				return dot(x[i],x[j]);
			case svm_parameter.POLY:
				return Math.pow(gamma*dot(x[i],x[j])+coef0,degree);
			case svm_parameter.RBF:
				return Math.exp(-gamma*(x_square[i]+x_square[j]-2*dot(x[i],x[j])));
			case svm_parameter.SIGMOID:
				return tanh(gamma*dot(x[i],x[j])+coef0);
			default:
				System.err.print("unknown kernel function.\n");
				System.exit(1);
				return 0;	// java
		}
	}

	Kernel(int l, svm_node[][] x_, svm_parameter param)
	{
		this.kernel_type = param.kernel_type;
		this.degree = param.degree;
		this.gamma = param.gamma;
		this.coef0 = param.coef0;

		x = (svm_node[][])x_.clone();

		if(kernel_type == svm_parameter.RBF)
		{
			x_square = new double[l];
			for(int i=0;i<l;i++)
				x_square[i] = dot(x[i],x[i]);
		}
		else x_square = null;
	}

	static double dot(svm_node[] x, svm_node[] y)
	{
		double sum = 0;
		int xlen = x.length;
		int ylen = y.length;
		int i = 0;
		int j = 0;
		while(i < xlen && j < ylen)
		{
			if(x[i].index == y[j].index)
				sum += x[i++].value * y[j++].value;
			else
			{
				if(x[i].index > y[j].index)
					++j;
				else
					++i;
			}
		}
		return sum;
	}

	static double k_function(svm_node[] x, svm_node[] y,
					svm_parameter param)
	{
		switch(param.kernel_type)
		{
			case svm_parameter.LINEAR:
				return dot(x,y);
			case svm_parameter.POLY:
				return Math.pow(param.gamma*dot(x,y)+param.coef0,param.degree);
			case svm_parameter.RBF:
			{
				double sum = 0;
				int xlen = x.length;
				int ylen = y.length;
				int i = 0;
				int j = 0;
				while(i < xlen && j < ylen)
				{
					if(x[i].index == y[j].index)
					{
						double d = x[i++].value - y[j++].value;
						sum += d*d;
					}
					else if(x[i].index > y[j].index)
					{
						sum += y[j].value * y[j].value;
						++j;
					}
					else
					{
						sum += x[i].value * x[i].value;
						++i;
					}
				}

				while(i < xlen)
				{
					sum += x[i].value * x[i].value;
					++i;
				}

				while(j < ylen)
				{
					sum += y[j].value * y[j].value;
					++j;
				}

				return Math.exp(-param.gamma*sum);
			}
			case svm_parameter.SIGMOID:
				return tanh(param.gamma*dot(x,y)+param.coef0);
			default:
				System.err.print("unknown kernel function.\n");
				System.exit(1);
				return 0;	// java
		}
	}
}

// Generalized SMO+SVMlight algorithm
// Solves:
//
//	min 0.5(\alpha^T Q \alpha) + b^T \alpha
//
//		0 <= alpha_i <= C
//		y^T \alpha = \delta
//		y_i = +1 or -1
//
// Given:
//
//	Q, b, y, C, and an initial feasible point \alpha
//	l is the size of vectors and matrices
//	eps is the stopping criterion
//
// solution will be put in \alpha, objective value will be put in obj
//
class Solver {
	int active_size;
	byte[] y;
	double[] G;		// gradient of objective function
	static final byte LOWER_BOUND = 0;
	static final byte UPPER_BOUND = 1;
	static final byte FREE = 2;
	byte[] alpha_status;	// LOWER_BOUND, UPPER_BOUND, FREE
	double[] alpha;
	Kernel Q;
	double eps;
	double C;
	double[] b;
	int[] active_set;
	double[] G_bar;		// gradient, if we treat free variables as 0
	int l;
	boolean unshrinked;		// XXX
	
	static final double EPS_A = 1e-12;
	static final double INF = java.lang.Double.POSITIVE_INFINITY;

	void update_alpha_status(int i)
	{
		if(alpha[i] >= C-EPS_A)
			alpha_status[i] = UPPER_BOUND;
		else if(alpha[i] <= EPS_A)
			alpha_status[i] = LOWER_BOUND;
		else alpha_status[i] = FREE;
	}
	boolean is_upper_bound(int i) { return alpha_status[i] == UPPER_BOUND; }
	boolean is_lower_bound(int i) { return alpha_status[i] == LOWER_BOUND; }
	boolean is_free(int i) {  return alpha_status[i] == FREE; }

	// java: information about solution except alpha,
	// because we cannot return multiple values otherwise...
	static class SolutionInfo {
		double obj;
		double rho;
		double upper_bound;
		double r;	// for nu-svc
	}

	void swap_index(int i, int j)
	{
		Q.swap_index(i,j);
		do {byte _=y[i]; y[i]=y[j]; y[j]=_;} while(false);
		do {double _=G[i]; G[i]=G[j]; G[j]=_;} while(false);
		do {byte _=alpha_status[i]; alpha_status[i]=alpha_status[j]; alpha_status[j]=_;} while(false);
		do {double _=alpha[i]; alpha[i]=alpha[j]; alpha[j]=_;} while(false);
		do {double _=b[i]; b[i]=b[j]; b[j]=_;} while(false);
		do {int _=active_set[i]; active_set[i]=active_set[j]; active_set[j]=_;} while(false);
		do {double _=G_bar[i]; G_bar[i]=G_bar[j]; G_bar[j]=_;} while(false);
	}

	void reconstruct_gradient()
	{
		// reconstruct inactive elements of G from G_bar and free variables

		if(active_size == l) return;

		int i;
		for(i=active_size;i<l;i++)
			G[i] = C * G_bar[i] + b[i];
		
		for(i=0;i<active_size;i++)
			if(is_free(i))
			{
				double[] Q_i = Q.get_Q(i,l);
				double alpha_i = alpha[i];
				for(int j=active_size;j<l;j++)
					G[j] += alpha_i * Q_i[j];
			}
	}

	void Solve(int l, Kernel Q, double[] b_, byte[] y_,
		   double[] alpha_, double C, double eps, SolutionInfo si, int shrinking)
	{
		this.l = l;
		this.Q = Q;
		b = (double[])b_.clone();
		y = (byte[])y_.clone();
		alpha = (double[])alpha_.clone();
		this.C = C;
		this.eps = eps;
		this.unshrinked = false;

		// initialize alpha_status
		{
			alpha_status = new byte[l];
			for(int i=0;i<l;i++)
				update_alpha_status(i);
		}

		// initialize active set (for shrinking)
		{
			active_set = new int[l];
			for(int i=0;i<l;i++)
				active_set[i] = i;
			active_size = l;
		}

		// initialize gradient
		{
			G = new double[l];
			G_bar = new double[l];
			int i;
			for(i=0;i<l;i++)
			{
				G[i] = b[i];
				G_bar[i] = 0;
			}
			for(i=0;i<l;i++)
				if(!is_lower_bound(i))
				{
					double[] Q_i = Q.get_Q(i,l);
					double alpha_i = alpha[i];
					int j;
					for(j=0;j<l;j++)
						G[j] += alpha_i*Q_i[j];
					if(is_upper_bound(i))
						for(j=0;j<l;j++)
							G_bar[j] += Q_i[j];
				}
		}

		// optimization step

		int iter = 0;
		int counter = Math.min(l,1000)+1;
		int[] working_set = new int[2];

		while(true)
		{
			// show progress and do shrinking

			if(--counter == 0)
			{
				counter = Math.min(l,1000);
				if(shrinking!=0) do_shrinking();
				System.err.print(".");
			}

			if(select_working_set(working_set)!=0)
			{
				// reconstruct the whole gradient
				reconstruct_gradient();
				// reset active set size and check
				active_size = l;
				System.err.print("*");
				if(select_working_set(working_set)!=0)
					break;
				else
					counter = 1;	// do shrinking next iteration
			}
			
			int i = working_set[0];
			int j = working_set[1];

			++iter;

			// update alpha[i] and alpha[j]

			double[] Q_i = Q.get_Q(i,active_size);
			double[] Q_j = Q.get_Q(j,active_size);

			double old_alpha_i = alpha[i];
			double old_alpha_j = alpha[j];

			if(y[i]!=y[j])
			{
				double L = Math.max(0.0,alpha[j]-alpha[i]);
				double H = Math.min(C,C+alpha[j]-alpha[i]);
				alpha[j] += (-G[i]-G[j])/(Q_i[i]+Q_j[j]+2*Q_i[j]);
				if(alpha[j] >= H) alpha[j] = H;
				else if(alpha[j] <= L) alpha[j] = L;
				alpha[i] += (alpha[j] - old_alpha_j);
			}
			else
			{
				double L = Math.max(0.0,alpha[i]+alpha[j]-C);
				double H = Math.min(C,alpha[i]+alpha[j]);
				alpha[j] += (G[i]-G[j])/(Q_i[i]+Q_j[j]-2*Q_i[j]);
				if(alpha[j] >= H) alpha[j] = H;
				else if(alpha[j] <= L) alpha[j] = L;
				alpha[i] -= (alpha[j] - old_alpha_j);
			}

			// update alpha_status and G_bar

			{
				boolean ui = is_upper_bound(i);
				boolean uj = is_upper_bound(j);
				update_alpha_status(i);
				update_alpha_status(j);
				int k;
				if(ui != is_upper_bound(i))
				{
					Q_i = Q.get_Q(i,l);
					if(ui)
						for(k=0;k<l;k++)
							G_bar[k] -= Q_i[k];
					else
						for(k=0;k<l;k++)
							G_bar[k] += Q_i[k];
				}

				if(uj != is_upper_bound(j))
				{
					Q_j = Q.get_Q(j,l);
					if(uj)
						for(k=0;k<l;k++)
							G_bar[k] -= Q_j[k];
					else
						for(k=0;k<l;k++)
							G_bar[k] += Q_j[k];
				}
			}

			// update G

			double delta_alpha_i = alpha[i] - old_alpha_i;
			double delta_alpha_j = alpha[j] - old_alpha_j;

			for(int k=0;k<active_size;k++)
			{
				G[k] += Q_i[k]*delta_alpha_i + Q_j[k]*delta_alpha_j;
			}
		}

		// calculate rho

		si.rho = calculate_rho();

		// calculate objective value
		{
			double v = 0;
			int i;
			for(i=0;i<l;i++)
				v += alpha[i] * (G[i] + b[i]);

			si.obj = v/2;
		}

		// put back the solution
		{
			for(int i=0;i<l;i++)
				alpha_[active_set[i]] = alpha[i];
		}

		si.upper_bound = C;
		System.out.print("\noptimization finished, #iter = "+iter+"\n");
	}

	// return 1 if already optimal, return 0 otherwise
	int select_working_set(int[] working_set)
	{
		// return i,j which maximize -grad(f)^T d , under constraint
		// if alpha_i == C, d != +1
		// if alpha_i == 0, d != -1

		double Gmax1 = -INF;		// max { -grad(f)_i * d | y_i*d = +1 }
		int Gmax1_idx = -1;

		double Gmax2 = -INF;		// max { -grad(f)_i * d | y_i*d = -1 }
		int Gmax2_idx = -1;

		for(int i=0;i<active_size;i++)
		{
			if(y[i]==+1)	// y = +1
			{
				if(!is_upper_bound(i))	// d = +1
				{
					if(-G[i] > Gmax1)
					{
						Gmax1 = -G[i];
						Gmax1_idx = i;
					}
				}
				if(!is_lower_bound(i))	// d = -1
				{
					if(G[i] > Gmax2)
					{
						Gmax2 = G[i];
						Gmax2_idx = i;
					}
				}
			}
			else		// y = -1
			{
				if(!is_upper_bound(i))	// d = +1
				{
					if(-G[i] > Gmax2)
					{
						Gmax2 = -G[i];
						Gmax2_idx = i;
					}
				}
				if(!is_lower_bound(i))	// d = -1
				{
					if(G[i] > Gmax1)
					{
						Gmax1 = G[i];
						Gmax1_idx = i;
					}
				}
			}
		}

		if(Gmax1+Gmax2 < eps)
	 		return 1;

		working_set[0] = Gmax1_idx;
		working_set[1] = Gmax2_idx;
		return 0;
	}

	void do_shrinking()
	{
		int i,j,k;
		int[] working_set = new int[2];
		if(select_working_set(working_set)!=0) return;
		i = working_set[0];
		j = working_set[1];
		double Gm1 = -y[j]*G[j];
		double Gm2 = y[i]*G[i];

		// shrink
	
		for(k=0;k<active_size;k++)
		{
			if(is_lower_bound(k))
			{
				if(y[k]==+1)
				{
					if(-G[k] >= Gm1) continue;
				}
				else	if(-G[k] >= Gm2) continue;
			}
			else if(is_upper_bound(k))
			{
				if(y[k]==+1)
				{
					if(G[k] >= Gm2) continue;
				}
				else	if(G[k] >= Gm1) continue;
			}
			else continue;

			--active_size;
			swap_index(k,active_size);
			--k;	// look at the newcomer
		}

		// unshrink, check all variables again before final iterations

		if(unshrinked || -(Gm1 + Gm2) > eps*10) return;

		unshrinked = true;
		reconstruct_gradient();

		for(k=l-1;k>=active_size;k--)
		{
			if(is_lower_bound(k))
			{
				if(y[k]==+1)
				{
					if(-G[k] < Gm1) continue;
				}
				else	if(-G[k] < Gm2) continue;
			}
			else if(is_upper_bound(k))
			{
				if(y[k]==+1)
				{
					if(G[k] < Gm2) continue;
				}
				else	if(G[k] < Gm1) continue;
			}
			else continue;

			swap_index(k,active_size);
			active_size++;
			++k;	// look at the newcomer
		}
	}

	double calculate_rho()
	{
		double r;
		int nr_free = 0;
		double ub = INF, lb = -INF, sum_free = 0;
		for(int i=0;i<active_size;i++)
		{
			double yG = y[i]*G[i];

			if(is_lower_bound(i))
			{
				if(y[i] > 0)
					ub = Math.min(ub,yG);
				else
					lb = Math.max(lb,yG);
			}
			else if(is_upper_bound(i))
			{
				if(y[i] < 0)
					ub = Math.min(ub,yG);
				else
					lb = Math.max(lb,yG);
			}
			else
			{
				++nr_free;
				sum_free += yG;
			}
		}

		if(nr_free>0)
			r = sum_free/nr_free;
		else
			r = (ub+lb)/2;

		return r;
	}

}

//
// Solver for nu-svm classification
//

final class Solver_NU_SVC extends Solver
{
	private byte[] y1;
	private SolutionInfo si;

	void Solve(int l, Kernel Q, double[] b, byte[] y,
		   double[] alpha, double C, double eps,
		   SolutionInfo si, byte[] y1_, int shrinking)
	{
		y1 = (byte[])y1_.clone();
		this.si = si;
		super.Solve(l,Q,b,y,alpha,C,eps,si,shrinking);
	}

	int select_working_set(int[] working_set)
	{
		// return i,j which maximize -grad(f)^T d , under constraint
		// if alpha_i == C, d != +1
		// if alpha_i == 0, d != -1

		double Gmax1 = -INF;	// max { -grad(f)_i * d | y_i = +1, d = +1 }
		int Gmax1_idx = -1;

		double Gmax2 = -INF;	// max { -grad(f)_i * d | y_i = +1, d = -1 }
		int Gmax2_idx = -1;

		double Gmax3 = -INF;	// max { -grad(f)_i * d | y_i = -1, d = +1 }
		int Gmax3_idx = -1;

		double Gmax4 = -INF;	// max { -grad(f)_i * d | y_i = -1, d = -1 }
		int Gmax4_idx = -1;

		for(int i=0;i<active_size;i++)
		{
			if(y1[i]==+1)	// y == +1
			{
				if(!is_upper_bound(i))	// d = +1
				{
					if(-G[i] > Gmax1)
					{
						Gmax1 = -G[i];
						Gmax1_idx = i;
					}
				}
				if(!is_lower_bound(i))	// d = -1
				{
					if(G[i] > Gmax2)
					{
						Gmax2 = G[i];
						Gmax2_idx = i;
					}
				}
			}
			else		// y == -1
			{
				if(!is_upper_bound(i))	// d = +1
				{
					if(-G[i] > Gmax3)
					{
						Gmax3 = -G[i];
						Gmax3_idx = i;
					}
				}
				if(!is_lower_bound(i))	// d = -1
				{
					if(G[i] > Gmax4)
					{
						Gmax4 = G[i];
						Gmax4_idx = i;
					}
				}
			}
		}

		if(Math.max(Gmax1+Gmax2,Gmax3+Gmax4) < eps)
 			return 1;

		if(Gmax1+Gmax2 > Gmax3+Gmax4)
		{
			working_set[0] = Gmax1_idx;
			working_set[1] = Gmax2_idx;
		}
		else
		{
			working_set[0] = Gmax3_idx;
			working_set[1] = Gmax4_idx;
		}
		return 0;
	}

	void do_shrinking()
	{
		double Gmax1 = -INF;	// max { -grad(f)_i * d | y_i = +1, d = +1 }
		double Gmax2 = -INF;	// max { -grad(f)_i * d | y_i = +1, d = -1 }
		double Gmax3 = -INF;	// max { -grad(f)_i * d | y_i = -1, d = +1 }
		double Gmax4 = -INF;	// max { -grad(f)_i * d | y_i = -1, d = -1 }

		int k;
		for(k=0;k<active_size;k++)
		{
			if(!is_upper_bound(k))
			{
				if(y1[k]==+1)
				{
					if(-G[k] > Gmax1) Gmax1 = -G[k];
				}
				else	if(-G[k] > Gmax3) Gmax3 = -G[k];
			}
			if(!is_lower_bound(k))
			{
				if(y1[k]==+1)
				{	
					if(G[k] > Gmax2) Gmax2 = G[k];
				}
				else	if(G[k] > Gmax4) Gmax4 = G[k];
			}
		}

		double Gm1 = -Gmax2;
		double Gm2 = -Gmax1;
		double Gm3 = -Gmax4;
		double Gm4 = -Gmax3;

		for(k=0;k<active_size;k++)
		{
			if(is_lower_bound(k))
			{
				if(y1[k]==+1)
				{
					if(-G[k] >= Gm1) continue;
				}
				else	if(-G[k] >= Gm3) continue;
			}
			else if(is_upper_bound(k))
			{
				if(y1[k]==+1)
				{
					if(G[k] >= Gm2) continue;
				}
				else	if(G[k] >= Gm4) continue;
			}
			else continue;

			--active_size;
			do {byte _=y1[k]; y1[k]=y1[active_size]; y1[active_size]=_;} while(false);
			swap_index(k,active_size);
			--k;	// look at the newcomer
		}

		// unshrink, check all variables again before final iterations

		if(unshrinked || Math.max(-(Gm1+Gm2),-(Gm3+Gm4)) > eps*10) return;
	
		unshrinked = true;
		reconstruct_gradient();

		for(k=l-1;k>=active_size;k--)
		{
			if(is_lower_bound(k))
			{
				if(y[k]==+1)
				{
					if(-G[k] < Gm1) continue;
				}
				else	if(-G[k] < Gm3) continue;
			}
			else if(is_upper_bound(k))
			{
				if(y[k]==+1)
				{
					if(G[k] < Gm2) continue;
				}
				else	if(G[k] < Gm4) continue;
			}
			else continue;

			do {byte _=y1[k]; y1[k]=y1[active_size]; y1[active_size]=_;} while(false);
			swap_index(k,active_size);
			active_size++;
			++k;	// look at the newcomer
		}
	}
	
	double calculate_rho()
	{
		int nr_free1 = 0,nr_free2 = 0;
		double ub1 = INF, ub2 = INF;
		double lb1 = -INF, lb2 = -INF;
		double sum_free1 = 0, sum_free2 = 0;

		for(int i=0;i<active_size;i++)
		{
			if(y1[i]==+1)
			{
				if(is_lower_bound(i))
					ub1 = Math.min(ub1,G[i]);
				else if(is_upper_bound(i))
					lb1 = Math.max(lb1,G[i]);
				else
				{
					++nr_free1;
					sum_free1 += G[i];
				}
			}
			else
			{
				if(is_lower_bound(i))
					ub2 = Math.min(ub2,G[i]);
				else if(is_upper_bound(i))
					lb2 = Math.max(lb2,G[i]);
				else
				{
					++nr_free2;
					sum_free2 += G[i];
				}
			}
		}

		double r1,r2;
		if(nr_free1 > 0)
			r1 = sum_free1/nr_free1;
		else
			r1 = (ub1+lb1)/2;

		if(nr_free2 > 0)
			r2 = sum_free2/nr_free2;
		else
			r2 = (ub2+lb2)/2;

		si.r = (r1+r2)/2;
		return (r1-r2)/2;
	}
}

//
// Q matrices for different formulations
//
class C_SVC_Q extends Kernel
{
	private final byte[] y;
	private final Cache cache;

	C_SVC_Q(svm_problem prob, svm_parameter param, byte[] y_)
	{
		super(prob.l, prob.x, param);
		y = (byte[])y_.clone();
		cache = new Cache(prob.l,(int)(param.cache_size*(1<<20)));
	}

	double[] get_Q(int i, int len)
	{
		double[][] data = new double[1][];
		int start;
		if((start = cache.get_data(i,data,len)) < len)
		{
			for(int j=start;j<len;j++)
				data[0][j] = y[i]*y[j]*kernel_function(i,j);
		}
		return data[0];
	}

	void swap_index(int i, int j)
	{
		cache.swap_index(i,j);
		super.swap_index(i,j);
		do {byte _=y[i]; y[i]=y[j]; y[j]=_;} while(false);
	}
}

class ONE_CLASS_Q extends Kernel
{
	private final Cache cache;

	ONE_CLASS_Q(svm_problem prob, svm_parameter param)
	{
		super(prob.l, prob.x, param);
		cache = new Cache(prob.l,(int)(param.cache_size*(1<<20)));
	}

	double[] get_Q(int i, int len)
	{
		double[][] data = new double[1][];
		int start;
		if((start = cache.get_data(i,data,len)) < len)
		{
			for(int j=start;j<len;j++)
				data[0][j] = kernel_function(i,j);
		}
		return data[0];
	}
}

class C_SVR_Q extends Kernel
{
	private final Cache cache;
	private final byte[] sign;
	private final int[] index;

	C_SVR_Q(svm_problem prob, svm_parameter param)
	{
		super(prob.l, prob.x, param);
		int l = prob.l;
		cache = new Cache(2*l,(int)(param.cache_size*(1<<20)));
		sign = new byte[2*l];
		index = new int[2*l];
		for(int k=0;k<l;k++)
		{
			sign[k] = 1;
			sign[k+l] = -1;
			index[k] = k;
			index[k+l] = k;
		}
	}

	void swap_index(int i, int j)
	{
		cache.swap_index(i,j);
		do {byte _=sign[i]; sign[i]=sign[j]; sign[j]=_;} while(false);
		do {int _=index[i]; index[i]=index[j]; index[j]=_;} while(false);
	}

	double[] get_Q(int i, int len)
	{
		double[][] data = new double[1][];
		int start;
		if((start = cache.get_data(i,data,len)) < len)
		{
			byte si = sign[i];
			for(int j=start;j<len;j++)
			{
				double v = kernel_function(index[i],index[j]);
				if(si == sign[j]) data[0][j] = v;
				else data[0][j] = -v;
			}
		}
		return data[0];
	}
}

public class svm {
	private static void solve_c_svc(svm_problem prob, svm_parameter param,
					double[] alpha, Solver.SolutionInfo si)
	{
		int l = prob.l;
		double[] minus_ones = new double[l];
		byte[] y = new byte[l];

		int i;

		for(i=0;i<l;i++)
		{
			alpha[i] = 0;
			minus_ones[i] = -1;
			if(prob.y[i] > 0) y[i] = +1; else y[i]=-1;
		}

		Solver s = new Solver();
		s.Solve(l, new C_SVC_Q(prob,param,y), minus_ones, y,
			alpha, param.C, param.eps, si, param.shrinking);

		double sum_alpha=0;
		for(i=0;i<l;i++)
			sum_alpha += alpha[i];

		System.out.print("nu = "+sum_alpha/(param.C*prob.l)+"\n");

		for(i=0;i<l;i++)
			alpha[i] *= y[i];
	}

	private static void solve_nu_svc(svm_problem prob, svm_parameter param,
				 	double[] alpha, Solver.SolutionInfo si)
	{
		int i;
		int l = prob.l;
		double nu = param.nu;

		int y_pos = 0;
		int y_neg = 0;
		byte[] y = new byte[l];

		for(i=0;i<l;i++)
			if(prob.y[i]>0)
			{
				y[i] = +1;
				++y_pos;
			}
			else
			{
				y[i] = -1;
				++y_neg;
			}

		if(nu < 0 || nu*l/2 > Math.min(y_pos,y_neg))
		{
			System.err.print("specified nu is infeasible\n");
			System.exit(1);
		}

		double sum_pos = nu*l/2;
		double sum_neg = nu*l/2;

		for(i=0;i<l;i++)
			if(y[i] == +1)
			{
				alpha[i] = Math.min(1.0,sum_pos);
				sum_pos -= alpha[i];
			}
			else
			{
				alpha[i] = Math.min(1.0,sum_neg);
				sum_neg -= alpha[i];
			}

		double[] zeros = new double[l];
		byte[] ones = new byte[l];

		for(i=0;i<l;i++)
		{
			zeros[i] = 0;
			ones[i] = 1;
		}

		Solver_NU_SVC s = new Solver_NU_SVC();
		s.Solve(l, new C_SVC_Q(prob,param,y), zeros, ones,
			alpha, 1.0, param.eps, si, y, param.shrinking);
		double r = si.r;

		System.out.print("C = "+1/r+"\n");

		for(i=0;i<l;i++)
			alpha[i] *= y[i]/r;

		si.rho /= r;
		si.obj /= (r*r);
		si.upper_bound = 1/r;
	}

	private static void solve_one_class(svm_problem prob, svm_parameter param,
				    	double[] alpha, Solver.SolutionInfo si)
	{
		int l = prob.l;
		double[] zeros = new double[l];
		byte[] ones = new byte[l];
		int i;

		int n = (int)(param.nu*prob.l);	// # of alpha's at upper bound
		if(n>=prob.l)
		{
			System.err.print("nu must be in (0,1)\n");
			System.exit(1);
		}
		for(i=0;i<n;i++)
			alpha[i] = 1;
		alpha[n] = param.nu * prob.l - n;
		for(i=n+1;i<l;i++)
			alpha[i] = 0;

		for(i=0;i<l;i++)
		{
			zeros[i] = 0;
			ones[i] = 1;
		}

		Solver s = new Solver();
		s.Solve(l, new ONE_CLASS_Q(prob,param), zeros, ones,
			alpha, 1.0, param.eps, si, param.shrinking);
	}

	private static void solve_c_svr(svm_problem prob, svm_parameter param,
					double[] alpha, Solver.SolutionInfo si)
	{
		int l = prob.l;
		double[] alpha2 = new double[2*l];
		double[] linear_term = new double[2*l];
		byte[] y = new byte[2*l];
		int i;

		for(i=0;i<l;i++)
		{
			alpha2[i] = 0;
			linear_term[i] = param.p - prob.y[i];
			y[i] = 1;

			alpha2[i+l] = 0;
			linear_term[i+l] = param.p + prob.y[i];
			y[i+l] = -1;
		}

		Solver s = new Solver();
		s.Solve(2*l, new C_SVR_Q(prob,param), linear_term, y,
			alpha2, param.C, param.eps, si, param.shrinking);

		for(i=0;i<l;i++)
			alpha[i] = alpha2[i] - alpha2[i+l];
	}

	//
	// Interface functions
	//
	public static svm_model svm_train(svm_problem prob, svm_parameter param)
	{
		svm_model model = new svm_model();
		model.param = param;
		double[] alpha = new double[prob.l];
		Solver.SolutionInfo si = new Solver.SolutionInfo();
		switch(param.svm_type)
		{
			case svm_parameter.C_SVC:
				solve_c_svc(prob,param,alpha,si);
				break;
			case svm_parameter.NU_SVC:
				solve_nu_svc(prob,param,alpha,si);
				break;
			case svm_parameter.ONE_CLASS:
				solve_one_class(prob,param,alpha,si);
				break;
			case svm_parameter.C_SVR:
				solve_c_svr(prob,param,alpha,si);
				break;
		}

		model.rho = si.rho;
		System.out.print("obj = "+si.obj+", rho = "+si.rho+"\n");

		// output SVs

		int nSV = 0;
		int nBSV = 0;
		for(int i=0;i<prob.l;i++)
		{
			if(Math.abs(alpha[i]) >= Solver.EPS_A)
			{
				++nSV;
				if(Math.abs(alpha[i]) >= si.upper_bound - Solver.EPS_A)
					++nBSV;
			}
		}

		System.out.print("nSV = "+nSV+", nBSV = "+nBSV+"\n");

		model.n = nSV;
		model.sv_coef = new double[nSV];
		model.SV = new svm_node[nSV][];

		{
			int j = 0;
			for(int i=0;i<prob.l;i++)
			{
				if(Math.abs(alpha[i]) >= Solver.EPS_A)
				{
					model.sv_coef[j] = alpha[i];
					model.SV[j] = prob.x[i];
					++j;
				}
			}
		}
		return model;
	}

	public static double svm_classify(svm_model model, svm_node[] x)
	{
		int n = model.n;
		double[] sv_coef = model.sv_coef;

		double sum = 0;

		for(int i=0;i<n;i++)
			sum += sv_coef[i] * Kernel.k_function(x,model.SV[i],model.param);
		sum-=(model.rho);
		return sum;
	}

	static final String svm_type_table[] =
	{
		"c_svc","nu_svc","one_class","c_svr",
	};

	static final String kernel_type_table[]=
	{
		"linear","polynomial","rbf","sigmoid",
	};

	public static void svm_save_model(String model_file_name, svm_model model) throws IOException
	{
		DataOutputStream fp = new DataOutputStream(new FileOutputStream(model_file_name));

		svm_parameter param = model.param;

		fp.writeBytes("svm_type "+svm_type_table[param.svm_type]+"\n");
		fp.writeBytes("kernel_type "+kernel_type_table[param.kernel_type]+"\n");

		if(param.kernel_type == svm_parameter.POLY)
			fp.writeBytes("degree "+param.degree+"\n");

		if(param.kernel_type == svm_parameter.POLY ||
		   param.kernel_type == svm_parameter.RBF ||
		   param.kernel_type == svm_parameter.SIGMOID)
			fp.writeBytes("gamma "+param.gamma+"\n");

		if(param.kernel_type == svm_parameter.POLY ||
		   param.kernel_type == svm_parameter.SIGMOID)
			fp.writeBytes("coef0 "+param.coef0+"\n");

		fp.writeBytes("rho "+model.rho+"\n");
		fp.writeBytes("SV "+model.n+"\n");

		int n = model.n;
		double[] sv_coef = model.sv_coef;
		svm_node[][] SV = model.SV;

		for(int i=0;i<n;i++)
		{
			fp.writeBytes(sv_coef[i]+" ");
			svm_node[] p = SV[i];
			for(int j=0;j<p.length;j++)
				fp.writeBytes(p[j].index+":"+p[j].value+" ");
			fp.writeBytes("\n");
		}

		fp.close();
	}

	private static double atof(String s)
	{
		return Double.valueOf(s).doubleValue();
	}

	private static int atoi(String s)
	{
		return Integer.parseInt(s);
	}

	public static svm_model svm_load_model(String model_file_name) throws IOException
	{
		BufferedReader fp = new BufferedReader(new FileReader(model_file_name));

		// read n,b,param

		svm_model model = new svm_model();
		svm_parameter param = new svm_parameter();
		model.param = param;

		while(true)
		{
			String cmd = fp.readLine();
			String arg = cmd.substring(cmd.indexOf(' ')+1);

			if(cmd.startsWith("svm_type"))
			{
				int i;
				for(i=0;i<svm_type_table.length;i++)
				{
					if(arg.indexOf(svm_type_table[i])!=-1)
					{
						param.svm_type=i;
						break;
					}
				}
				if(i == svm_type_table.length)
				{
					System.err.print("unknown svm type.\n");
					System.exit(1);
				}
			}
			else if(cmd.startsWith("kernel_type"))
			{
				int i;
				for(i=0;i<kernel_type_table.length;i++)
				{
					if(arg.indexOf(kernel_type_table[i])!=-1)
					{
						param.kernel_type=i;
						break;
					}
				}
				if(i == kernel_type_table.length)
				{
					System.err.print("unknown kernel function.\n");
					System.exit(1);
				}
			}
			else if(cmd.startsWith("degree"))
				param.degree = atof(arg);
			else if(cmd.startsWith("gamma"))
				param.gamma = atof(arg);
			else if(cmd.startsWith("coef0"))
				param.coef0 = atof(arg);
			else if(cmd.startsWith("rho"))
				model.rho = atof(arg);
			else if(cmd.startsWith("SV"))
			{
				model.n = atoi(arg);
				break;
			}
			else
			{
				System.err.print("unknown text in model file\n");
				System.exit(1);
			}
		}

		// read sv_coef and SV

		int n = model.n;
		model.sv_coef = new double[n];
		model.SV = new svm_node[n][];

		for(int i=0;i<n;i++)
		{
			String line = fp.readLine();
			StringTokenizer st = new StringTokenizer(line," \t\n\r\f:");

			model.sv_coef[i] = atof(st.nextToken());
			int m = st.countTokens()/2;
			model.SV[i] = new svm_node[m];
			for(int j=0;j<m;j++)
			{
				model.SV[i][j] = new svm_node();
				model.SV[i][j].index = atoi(st.nextToken());
				model.SV[i][j].value = atof(st.nextToken());
			}
		}

		fp.close();
		return model;
	}
}

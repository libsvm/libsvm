import svmc

from svmc import C_SVC, NU_SVC, ONE_CLASS, EPSILON_SVR, NU_SVR
from svmc import LINEAR, POLY, RBF, SIGMOID

class svm_parameter:
	
	# default values
	default_parameters = {
	'svm_type' : C_SVC,
	'kernel_type' : RBF,
	'degree' : 3,
	'gamma' : 0,		# 1/k
	'coef0' : 0,
	'nu' : 0.5,
	'cache_size' : 40,
	'C' : 1,
	'eps' : 1e-3,
	'p' : 0.1,
	'shrinking' : 1,
	'nr_weight' : 0,
	#'param.weight_label' : 'NULL',
	#'param.weight' : 'NULL',
	}

	def __init__(self,**kw):
		self.__dict__['param'] = svmc.new_svm_parameter()
		for attr,val in self.default_parameters.items():
			setattr(self,attr,val)
		for attr,val in kw.items():
			setattr(self,attr,val)

	def __getattr__(self,attr):
		get_func = getattr(svmc,'svm_parameter_%s_get' % (attr))
		return get_func(self.param)

	def __setattr__(self,attr,val):
		set_func = getattr(svmc,'svm_parameter_%s_set' % (attr))
		set_func(self.param,val)

	def __repr__(self):
		ret = '<svm_parameter:'
		for name in dir(svmc):
			if name[:len('svm_parameter_')] == 'svm_parameter_' and name[-len('_set'):] == '_set':
				attr = name[len('svm_parameter_'):-len('_set')]
				ret = ret+' %s = %s,' % (attr,getattr(self,attr))
		return ret+'>'

	def __del__(self):
		svmc.delete_svm_parameter(self.param)

def _convert_to_svm_node_array(x):
	""" convert a sequence or mapping to an svm_node array """
	data = svmc.svm_node_array(len(x)+1)
	svmc.svm_node_array_set(data,len(x),-1,0)
	import operator
	if operator.isMappingType(x):
		keys = x.keys()
		keys.sort()
		j = 0
		for k in keys:
			svmc.svm_node_array_set(data,j,k,x[k])
			j = j + 1
	elif operator.isSequenceType(x):
		for j in range(len(x)):
			svmc.svm_node_array_set(data,j,j+1,x[j])
	else:
		raise TypeError,"data must be a mapping or a sequence"
	
	return data

class svm_problem:
	def __init__(self,y,x):
		assert len(y) == len(x)
		self.prob = prob = svmc.new_svm_problem()
		self.size = size = len(y)

		self.y_array = y_array = svmc.double_array(size)
		for i in range(size):
			svmc.double_set(y_array,i,y[i])

		self.x_matrix = x_matrix = svmc.svm_node_matrix(size)
		self.data = []
		self.maxlen = 0;
		for i in range(size):
			data = _convert_to_svm_node_array(x[i])
			self.data.append(data);
			svmc.svm_node_matrix_set(x_matrix,i,data)
			self.maxlen = max(self.maxlen,len(x[i]))

		svmc.svm_problem_l_set(prob,size)
		svmc.svm_problem_y_set(prob,y_array)
		svmc.svm_problem_x_set(prob,x_matrix)

	def __repr__(self):
		return "<svm_problem: size = %s>" % (self.size)

	def __del__(self):
		svmc.delete_svm_problem(self.prob)
		svmc.double_destroy(self.y_array)
		for i in range(self.size):
			svmc.svm_node_array_destroy(self.data[i])
		svmc.svm_node_matrix_destroy(self.x_matrix)

class svm_model:
	def __init__(self,arg1,arg2=None):
		if arg2 == None:
			# create model from file
			filename = arg1
			self.model = svmc.svm_load_model(filename)
		else:
			# create model from problem and parameter
			prob,param = arg1,arg2
			if param.gamma == 0:
				param.gamma = 1.0/prob.maxlen
			self.model = svmc.svm_train(prob.prob,param.param)

	def predict(self,x):
		data = _convert_to_svm_node_array(x)
		ret = svmc.svm_predict(self.model,data)
		svmc.svm_node_array_destroy(data)
		return ret

	def save(self,filename):
		svmc.svm_save_model(filename,self.model)

	def __del__(self):
		svmc.svm_destroy_model(self.model)

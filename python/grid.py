#!/usr/bin/python

import os, sys
import Queue
import getpass
from threading import Thread
from string import find, split, join, atof
is_win32 = (sys.platform == 'win32')

# svmtrain and gnuplot executable

svmtrain_exe = "../svm-train"
gnuplot_exe = "/usr/bin/gnuplot"

# example for windows
# svmtrain_exe = r"c:\tmp\libsvm-2.35\windows\svmtrain.exe"
# gnuplot_exe = r"c:\tmp\gp371w32\pgnuplot.exe"

assert os.path.exists(gnuplot_exe),"gnuplot executable not found"
assert os.path.exists(svmtrain_exe),"svm-train executable not found"

gnuplot = os.popen(gnuplot_exe,'w')

# global parameters and their default values

fold = 5
c_begin, c_end, c_step = -5,  15, 1
g_begin, g_end, g_step =  3, -15, -1
global dataset_pathname, dataset_title, pass_through_string

# experimental

remote_workers = []
nr_local_worker = 1

# process command line options, set global parameters
def process_options(argv=sys.argv):

	global fold
	global c_begin, c_end, c_step
	global g_begin, g_end, g_step
	global dataset_pathname, dataset_title, pass_through_string

	if len(argv) < 2:
		print "Usage: grid.py [-c begin,end,step] [-g begin,end,step] [-v fold] dataset"
		sys.exit()

	dataset_pathname = argv[-1]
	dataset_title = os.path.split(dataset_pathname)[1]
	pass_through_options = []

	i = 1
	while i < len(argv) - 1:
		if argv[i] == "-c":
			i = i + 1
			(c_begin,c_end,c_step) = map(atof,split(argv[i],","))
		elif argv[i] == "-g":
			i = i + 1
			(g_begin,g_end,g_step) = map(atof,split(argv[i],","))
		elif argv[i] == "-v":
			i = i + 1
			fold = argv[i]
		else:
			pass_through_options.append(argv[i])
		i = i + 1

	pass_through_string = join(pass_through_options," ")

def range_f(begin,end,step):
	# like range, but works on non-integer too
	seq = []
	while 1:
		if step > 0 and begin > end: break
		if step < 0 and begin < end: break
		seq.append(begin)
		begin = begin + step
	return seq

def permute_sequence(seq):
	n = len(seq)
	if n <= 1: return seq

	mid = int(n/2)
	left = permute_sequence(seq[:mid])
	right = permute_sequence(seq[mid+1:])

	ret = []
	while left or right:
		if left: ret.append(left.pop())
		if right: ret.append(right.pop())
	ret.append(seq[mid])

	return ret

def redraw (db,tofile=0):
	if len(db) == 0: return
	begin_level = round(max(map(lambda(x):x[2],db))) - 3
	step_size = 0.5
	if tofile:
		gnuplot.write("set term png small color\n")
		gnuplot.write("set output \"%s.png\"\n" % dataset_title)
		#gnuplot.write("set term postscript color solid\n")
		#gnuplot.write("set output \"%s.ps\"\n" % dataset_title)
	else:
		if is_win32:
			gnuplot.write("set term windows\n")
		else:
			gnuplot.write("set term x11\n")
	gnuplot.write("set xlabel \"lg(C)\"\n")
	gnuplot.write("set ylabel \"lg(gamma)\"\n")
	gnuplot.write("set xrange [%s:%s]\n" % (c_begin,c_end))
	gnuplot.write("set yrange [%s:%s]\n" % (g_begin,g_end))
	gnuplot.write("set contour\n")
	gnuplot.write("set cntrparam levels incremental %s,%s,100\n" % (begin_level,step_size))
	gnuplot.write("set nosurface\n")
	gnuplot.write("set view 0,0\n")
	gnuplot.write("set label \"%s\" at screen 0.4,0.9\n" % dataset_title)
	gnuplot.write("splot \"-\" with lines\n")
	def cmp (x,y):
		if x[0] < y[0]: return -1
		if x[0] > y[0]: return 1
		if x[1] > y[1]: return -1
		if x[1] < y[1]: return 1
		return 0
	db.sort(cmp)
	prevc = db[0][0]
	for line in db:
		if prevc != line[0]:
			gnuplot.write("\n")
			prevc = line[0]
		gnuplot.write("%s %s %s\n" % line)
	gnuplot.write("e\n")
	gnuplot.flush()


def calculate_jobs():
	c_seq = permute_sequence(range_f(c_begin,c_end,c_step))
	g_seq = permute_sequence(range_f(g_begin,g_end,g_step))
	c_seq.reverse()
	g_seq.reverse()
	nr_c = float(len(c_seq))
	nr_g = float(len(g_seq))
	i = 0
	j = 0
	jobs = []

	while i < nr_c or j < nr_g:
		if i/nr_c < j/nr_g:
			# increase C resolution
			line = []
			for k in range(0,j):
				line.append((c_seq[i],g_seq[k]))
			i = i + 1
			jobs.append(line)
		else:
			# increase g resolution
			line = []
			for k in range(0,i):
				line.append((c_seq[k],g_seq[j]))
			j = j + 1
			jobs.append(line)
	return jobs

class LocalWorker(Thread):
	def __init__(self,job_queue,result_queue):
		Thread.__init__(self)
		self.job_queue = job_queue
		self.result_queue = result_queue
	def run(self):
		try:
			while 1:
				(c,g) = self.job_queue.get_nowait()
				rate = self.run_one(c,g)
				self.result_queue.put((c,g,rate))
		except Queue.Empty:
			pass
	def run_one(self,c,g):
		cexp = 2.0**c
		gexp = 2.0**g
		cmdline = '%s -c %s -g %s -v %s %s %s' % \
		  (svmtrain_exe,cexp,gexp,fold,pass_through_string,dataset_pathname)
		result = os.popen(cmdline,'r')
		for line in result.readlines():
			if find(line,"Cross") != -1:
				rate = atof(split(line)[-1][0:-1])
				print "[local] %(c)s %(g)s %(rate)s" % locals()
		return rate


class RemoteWorker(Thread):
	def __init__(self,job_queue,result_queue,host,username,password):
		Thread.__init__(self)
		self.job_queue = job_queue
		self.result_queue = result_queue
		self.host = host
		self.username = username
		self.password = password		
	def run(self):
		import telnetlib
		self.tn = tn = telnetlib.Telnet(self.host)
		tn.read_until("login: ")
		tn.write(self.username + "\n")
		tn.read_until("Password: ")
		tn.write(self.password + "\n")

		# XXX: how to know whether login is successful?
		tn.read_until(self.username)
		# 
		print 'login ok', self.host
		tn.write("cd "+os.getcwd()+"\n")

		try:
			while 1:
				(c,g) = self.job_queue.get_nowait()
				rate = self.run_one(c,g)
				self.result_queue.put((c,g,rate))
		except Queue.Empty:
			pass
		tn.write("exit\n")			     

	def run_one(self,c,g):
		cexp = 2.0**c
		gexp = 2.0**g
		cmdline = '%s -c %s -g %s -v %s %s %s' % \
		  (svmtrain_exe,cexp,gexp,fold,pass_through_string,dataset_pathname)
		result = self.tn.write(cmdline+'\n')
		(idx,matchm,output) = self.tn.expect(['Cross.*\n'])
		for line in split(output,'\n'):
			if find(line,"Cross") != -1:
				rate = atof(split(line)[-1][0:-1])
				print '[%s]' % self.host,
				print "%(c)s %(g)s %(rate)s" % locals()
		return rate


def main():

	# set parameters

	process_options()

	# put jobs in queue

	jobs = calculate_jobs()
	job_queue = Queue.Queue(0)
	result_queue = Queue.Queue(0)

	for line in jobs:
		for (c,g) in line:
			job_queue.put((c,g))

	# fire remote workers

	if remote_workers:
		nr_remote_worker = len(remote_workers)
		username = getpass.getuser()
		password = getpass.getpass()
		for host in remote_workers:
			RemoteWorker(job_queue,result_queue,
				     host,username,password).start()

	# fire local workers

	for i in range(nr_local_worker):
		LocalWorker(job_queue,result_queue).start()

	# gather results

	done_jobs = {}
	result_file = open('%s.out' % dataset_title ,'w',0)
	db = []

	for line in jobs:
		for (c,g) in line:
			while not done_jobs.has_key((c,g)):
				(c1,g1,rate) = result_queue.get()
				done_jobs[(c1,g1)] = rate
				result_file.write('%s %s %s\n' %(c1,g1,rate))
			db.append((c,g,done_jobs[(c,g)]))
		redraw(db)
		redraw(db,1)
		print 

main()

#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include "svm.h"
#define MAX_LINE_LEN 100000
#define MAX_NR_ATTR 1000

char line[MAX_LINE_LEN];
struct svm_node x[MAX_NR_ATTR];

struct svm_model* model;

void classify(FILE *input, FILE *output)
{
	int correct = 0;
	int total = 0;
	
#define SKIP_CLASS\
	while(isspace(*p)) ++p;\
	while(!isspace(*p)) ++p;

#define SKIP_ELEMENT\
	while(*p!=':') ++p;\
	++p;\
	while(isspace(*p)) ++p;\
	while(*p && !isspace(*p)) ++p;

	while(fgets(line,sizeof(line),input)!=NULL)
	{
		int i = 0;
		double label,v;
		const char *p = line;

		if(sscanf(p,"%lf",&label)!=1) break;

		SKIP_CLASS

		while(sscanf(p,"%d:%lf",&x[i].index,&x[i].value)==2)
		{
			SKIP_ELEMENT;
			++i;
		}

		x[i].index = -1;
		if(svm_classify(model,x,label,&v))
			++correct;
		++total;

		fprintf(output,"%g\n",v);
	}

	printf("correct/total = %d/%d (%g%%)\n",correct,total,
					       (double)correct/total*100);
}

int main(int argc, char **argv)
{
	FILE *input, *output;
	
	if(argc!=4)
	{
		fprintf(stderr,"usage: svm-classify test_file model_file output_file\n");
		exit(1);
	}

	input = fopen(argv[1],"r");
	if(input == NULL)
	{
		fprintf(stderr,"can't open input file %s\n",argv[1]);
		exit(1);
	}

	output = fopen(argv[3],"w");
	if(output == NULL)
	{
		fprintf(stderr,"can't open output file %s\n",argv[3]);
		exit(1);
	}

	if((model=svm_load_model(argv[2]))==0)
	{
		fprintf(stderr,"can't open model file %s\n",argv[2]);
		exit(1);
	}

	classify(input,output);
	svm_destroy_model(model);
	fclose(input);
	fclose(output);
	return 0;
}
